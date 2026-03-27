#pragma once
#include<iostream>
#include<fstream>
#include<map>
#include<unordered_map>
#include<unordered_set>
#include<string>
#include <queue>
#include "forest_struct.h"
#include "fsa.h"
#include "sink.h"
#include "streaming_graph.h"
#define merge_long_long(s, d) (((unsigned long long)s<<32)|d)
using namespace std;

// code for the S-PATH algorithm

class S_PATH {
public:
	FiniteStateAutomaton &aut;
	streaming_graph &g;
	Sink &sink;

	unordered_map<unsigned long long, RPQ_tree*> forests; // map from product graph node to tree pointer
	map<unsigned int, tree_info_index*> v2t_index; // reverse index that maps a graph vertex to the trees that contains it. The first layer maps state to tree_info_index, and the second layer maps vertex ID to list of trees contains this node

	S_PATH(FiniteStateAutomaton &aut, streaming_graph &g, Sink &sink)
		: aut(aut), g(g), sink(sink) {
	}

	~S_PATH() {
		unordered_map<unsigned long long, RPQ_tree*>::iterator it;
		for (it = forests.begin(); it != forests.end(); it++)
			delete it->second;
		forests.clear();
		map<unsigned int, tree_info_index*>::iterator it2;
		for (it2 = v2t_index.begin(); it2 != v2t_index.end(); it2++)
			delete it2->second;
		v2t_index.clear();
	}

	bool insert_edge(unsigned int s, unsigned int d, unsigned int label, long long timestamp) //  a new snapshot graph edge (s, d) is inserted, update the spanning forest accordingly.
	{
		bool result = false;
        if (aut.getNextState(0, label) != -1 && forests.find(merge_long_long(s, 0)) == forests.end())
		{
			auto* new_tree = new RPQ_tree();
			new_tree->root = add_node(new_tree, s, 0, s, nullptr, MAX_INT, MAX_INT);
			forests[merge_long_long(s, 0)] = new_tree;
		}
        vector<pair<long long, long long> > vec = aut.getStatePairsWithTransition(label); // find all the state paris that can accept this label
		for (auto &[fst, snd] : vec) {
			unsigned int src_state = fst;
			unsigned int dst_state = snd;
			if (auto index_iter2 = v2t_index.find(src_state); index_iter2 != v2t_index.end())
			{
				if (auto tree_iter = index_iter2->second->tree_index.find(s); tree_iter != index_iter2->second->tree_index.end()) {
					tree_info* tmp = tree_iter->second;
					while (tmp) {
						result = insert_per_tree(s, d, label, timestamp, src_state, dst_state, tmp->tree); // for each state pair, find the trees containing (s, src_state), and update it with the new edge.
						tmp = tmp->next;
					}
				}
			}
		}
		return result;
	}

	void expire_forest(long long eviction_time, const std::vector<streaming_graph::expired_edge_info>& deleted_edges) // given current time, delete the expired tree nodes and results.
	{
		unordered_set<unsigned long long> visited;
		for (const auto & deleted_edge : deleted_edges)
		{
			unsigned int dst = deleted_edge.dst;
			unsigned int label = deleted_edge.label; // for each expired edge, find its dst node. All the expired nodes in the spanning forest must be in a subtree of such dst node.
			std::vector<std::pair<long long, long long> > vec = aut.getStatePairsWithTransition(label);
			for (auto &[fst, snd] : vec) {
				long long dst_state = snd;
				if (dst_state == -1)
					continue;
				if(visited.find(merge_long_long(dst, dst_state))!=visited.end())
					continue;
				visited.insert(merge_long_long(dst,dst_state)); // record the checked dst node, in case of repeated process.
				if (auto iter = v2t_index.find(dst_state); iter != v2t_index.end())
				{
					if (auto tree_iter = iter->second->tree_index.find(dst); tree_iter != iter->second->tree_index.end()) {
						vector<RPQ_tree*> tree_to_delete;
						tree_info* tmp = tree_iter->second;
						while (tmp)	// first record the trees in the list with a vector, as when we delete expired nodes we will change the tree list, leading to error in the list scan.
						{
							tree_to_delete.push_back(tmp->tree);
							tmp = tmp->next;
						}
						for (auto & k : tree_to_delete) {
							expire_per_tree(dst, dst_state, k, eviction_time); // expire in each tree
							if (k->root->child == nullptr) // delete the tree if it is empty.
							{
								delete_index(k->root->node_ID, k->root->state, k->root->node_ID);
								forests.erase(merge_long_long(k->root->node_ID, k->root->state));
								delete k;
							}
						}
						shrink(forests);
						tree_to_delete.clear();
					}
				}
			}
		}
	}

	// Handle mid-window edge deletion (load shedding). For each deleted edge,
	// find tree nodes whose parent link used that edge. For each such node,
	// use the reverse adjacency list to find an alternative incoming edge
	// whose source exists in the same tree; reconnect or delete the subtree.
	//
	// Complexity per affected node: O(in_degree(dst) * |FSA_transitions|)
	void shed_edges(const std::vector<streaming_graph::expired_edge_info>& deleted_edges) {
		for (const auto& edge : deleted_edges) {
			unsigned int src = edge.src;
			unsigned int dst = edge.dst;
			unsigned int label = edge.label;

			std::vector<std::pair<long long, long long>> vec = aut.getStatePairsWithTransition(label);
			for (auto& [src_state, dst_state] : vec) {
				if (dst_state == -1) continue;

				auto v2t_iter = v2t_index.find(dst_state);
				if (v2t_iter == v2t_index.end()) continue;
				auto tree_iter = v2t_iter->second->tree_index.find(dst);
				if (tree_iter == v2t_iter->second->tree_index.end()) continue;

				vector<RPQ_tree*> affected_trees;
				for (tree_info* ti = tree_iter->second; ti; ti = ti->next)
					affected_trees.push_back(ti->tree);

				for (RPQ_tree* tree_pt : affected_trees) {
					tree_node* node = tree_pt->find_node(dst, dst_state);
					if (!node || !node->parent) continue;
					if (node->parent->node_ID != src || node->parent->state != static_cast<unsigned int>(src_state))
						continue;

					// Parent link is broken. Search for best alternative via
					// reverse adjacency list: all edges pointing to dst.
					tree_node* best_parent = nullptr;
					unsigned int best_timestamp = 0;
					unsigned int best_edge_time = 0;

					vector<sg_edge*> predecessors = g.get_all_pred_ptrs(dst);
					for (sg_edge* pe : predecessors) {
						unsigned int pred_v = pe->s;
						unsigned int pred_label = pe->label;
						std::vector<std::pair<long long, long long>> trans = aut.getStatePairsWithTransition(pred_label);
						for (auto& [q_src, q_dst] : trans) {
							if (q_dst != static_cast<long long>(dst_state)) continue;
							tree_node* candidate = tree_pt->find_node(pred_v, q_src);
							if (!candidate || candidate == node) continue;
							unsigned int candidate_ts = min(candidate->timestamp, static_cast<unsigned int>(pe->timestamp));
							if (candidate_ts > best_timestamp) {
								best_parent = candidate;
								best_timestamp = candidate_ts;
								best_edge_time = pe->timestamp;
							}
						}
					}

					if (best_parent) {
						tree_pt->substitute_parent(best_parent, node);
						node->edge_timestamp = best_edge_time;
						node->timestamp = best_timestamp;
						propagate_timestamps(node);
					} else {
						erase_tree_node(tree_pt, node);
						g.shed_count++;
					}

					if (tree_pt->root->child == nullptr) {
						delete_index(tree_pt->root->node_ID, tree_pt->root->state, tree_pt->root->node_ID);
						forests.erase(merge_long_long(tree_pt->root->node_ID, tree_pt->root->state));
						delete tree_pt;
						break;
					}
				}
			}
		}
		shrink(forests);
	}

private:

	// Propagate timestamps downward after a node's timestamp has decreased.
	// Each child's timestamp = min(parent->timestamp, child->edge_timestamp).
	// If a child's timestamp doesn't change, its subtree is unaffected and we stop.
	void propagate_timestamps(tree_node* node) {
		queue<tree_node*> q;
		for (tree_node* cur = node->child; cur; cur = cur->brother)
			q.push(cur);
		while (!q.empty()) {
			tree_node* tmp = q.front();
			q.pop();
			unsigned int new_ts = min(tmp->parent->timestamp, tmp->edge_timestamp);
			if (new_ts == tmp->timestamp)
				continue;
			tmp->timestamp = new_ts;
			for (tree_node* cur = tmp->child; cur; cur = cur->brother)
				q.push(cur);
		}
	}

	void update_result(unordered_map<unsigned int, unsigned int>& updated_nodes, unsigned int root_ID) // update the result set, first of a KV in the um is a vertex ID v, and second is a timestamp t, update timestamp of (root v) pair
	// to t
	{
		for (auto & updated_node : updated_nodes) {
			unsigned int dst = updated_node.first;
			unsigned int time = updated_node.second;
			// if (dst == root_ID) continue;
			sink.addEntry(root_ID, dst, time);
		}
	}

	void add_index(RPQ_tree* tree_pt, unsigned int v, unsigned int state, unsigned int root_ID) // modify the reverse index to record the presence of a node in a tree. 
	{
		auto iter = v2t_index.find(state);
		if (iter == v2t_index.end())
			v2t_index[state] = new tree_info_index;
		v2t_index[state]->add_node(tree_pt, v, root_ID);
	}


	tree_node* add_node(RPQ_tree* tree_pt, unsigned int v, unsigned int state, unsigned int root_ID, tree_node* parent, unsigned int timestamp, unsigned int edge_time) // add  a node to a spanning tree, given all the necessary information.
	{
		add_index(tree_pt, v, state, root_ID);
		tree_node* tmp = tree_pt->add_node(v, state, parent, timestamp, edge_time);
		return tmp;
	}



	void delete_index(unsigned int v, unsigned int state, unsigned int root) // modify the reverse index when a node is not in a spanning tree any more. 
	{
		auto iter = v2t_index.find(state);
		if (iter != v2t_index.end())
		{
			iter->second->delete_node(v, root);
			if (iter->second->tree_index.empty())
				v2t_index.erase(iter);
		}
	}
	bool expand(tree_node* expand_node, RPQ_tree* tree_pt) // function used to expand a spanning tree with a BFS manner when a new node is added into a spanning tree. expand node is the new node
	{
		unordered_map<unsigned int, unsigned int> updated_results;
		priority_queue<tree_node*, vector<tree_node*>, time_compare> q;
		q.push(expand_node);
		while (!q.empty())
		{
			tree_node* tmp = q.top();
			q.pop();
			if (aut.isFinalState(tmp->state)) {			// if this is a final state, we need to update the result set, we record it first and at last carry out the update together
				if (updated_results.find(tmp->node_ID) != updated_results.end())
					updated_results[tmp->node_ID] = max(updated_results[tmp->node_ID], tmp->timestamp);
				else
					updated_results[tmp->node_ID] = tmp->timestamp;
			}

			vector<sg_edge*> vec = g.get_all_suc_ptrs(tmp->node_ID); // get all the successor edges in the snapshot graph, and check each of them to find the successor nodes in the product graph.
			for (auto & i : vec) {
				unsigned int successor = i->d;
				unsigned int edge_label = i->label;
				long long dst_state = aut.getNextState(tmp->state, edge_label);
				if (dst_state == -1) continue;
				unsigned int time = min(tmp->timestamp, i->timestamp);
				if (tree_pt->node_map.find(dst_state) == tree_pt->node_map.end() || tree_pt->node_map[dst_state]->index.find(successor) == tree_pt->node_map[dst_state]->index.end()) // If this node does not exit before, we add this node.
					q.push(add_node(tree_pt, successor, dst_state, tree_pt->root->node_ID, tmp, time, i->timestamp));
				else {
					if (tree_node* dst_pt = tree_pt->node_map[dst_state]->index[successor]; dst_pt->timestamp < time) { // else if its current timestamp is smaller than the new timestamp, we update the timestamp and link it to the new parent.
						if (dst_pt->parent != tmp) {
							tree_pt->substitute_parent(tmp, dst_pt);
						}
						dst_pt->timestamp = time;
						dst_pt->edge_timestamp = i->timestamp;
						q.push(dst_pt);
					}
				}
			}
		}
		update_result(updated_results, tree_pt->root->node_ID);
		updated_results.clear();
		return updated_results.empty();
	}

	bool insert_per_tree(unsigned int s, unsigned int d, unsigned int label, int timestamp, unsigned int src_state, unsigned int dst_state, RPQ_tree* tree_pt) // processing a new product graph edge from (s, src_state) to (d, dst_state) in a spanning tree tree_pt;
	{
		bool result = false;
		if (tree_pt->node_map.find(src_state) != tree_pt->node_map.end())
		{
			if (tree_node_index* tmp_index = tree_pt->node_map[src_state]; tmp_index->index.find(s) != tmp_index->index.end()) // find the src node
			{
				tree_node* src_pt = tmp_index->index[s];
				unsigned int time = min(src_pt->timestamp, timestamp);
				if (tree_pt->node_map.find(dst_state) == tree_pt->node_map.end() || tree_pt->node_map[dst_state]->index.find(d) == tree_pt->node_map[dst_state]->index.end()) { // if the dst node does not exist
					tree_node* dst_pt = add_node(tree_pt, d, dst_state, tree_pt->root->node_ID, src_pt, min(src_pt->timestamp, timestamp), timestamp);
					result = expand(dst_pt, tree_pt); // add the dst node and further expand,
				} else {
					tree_node* dst_pt = tree_pt->node_map[dst_state]->index[d];
					if (dst_pt->timestamp < time) // if the dst node exists but has a smaller timestamp, update its timestamp, and use expand to propagate the new timestamp down.
					{
						if (dst_pt->parent != src_pt) {
							tree_pt->substitute_parent(src_pt, dst_pt);
						}
						dst_pt->timestamp = time;
						dst_pt->edge_timestamp = timestamp;
						result = expand(dst_pt, tree_pt);
					}
				}
			}
		}
		return result;
	}

	void erase_tree_node(RPQ_tree* tree_pt, tree_node* child) // given an expired node, delete the subtree rooted at it in tree_pt, all the nodes in its subtree also expire.
	{
		queue<tree_node*> q;
		q.push(child);
		tree_pt->separate_node(child); // 'child' is disconnected with its parent, other nodes do not need to call this function, as there parents and brothers are all deleted;
		while (!q.empty())
		{
			tree_node* tmp = q.front();
			q.pop();
			for (tree_node* cur = tmp->child; cur; cur = cur->brother)
				q.push(cur);
			tree_pt->remove_node(tmp);
			delete_index(tmp->node_ID, tmp->state, tree_pt->root->node_ID);
			delete tmp;
		}
	}

	void expire_per_tree(unsigned int v, unsigned int state, RPQ_tree* tree_pt, unsigned int expired_time) // given a product graph node (v, state) which can possibly be an expired node, try to delete its subtree.
	{
		if (tree_pt->node_map.find(state) != tree_pt->node_map.end()) {
			if (tree_pt->node_map[state]->index.find(v) != tree_pt->node_map[state]->index.end()) {
				if (tree_node* dst_pt = tree_pt->node_map[state]->index[v]; dst_pt->timestamp < expired_time) // if it is indeed an expired node, delete its subtree.
					erase_tree_node(tree_pt, dst_pt);
			}
		}
	}

	void print_tree(unsigned int ID, unsigned int state)
	{
		if (auto iter = forests.find(merge_long_long(ID, state)); iter != forests.end()) {
			tree_node* tmp = iter->second->root;
			queue<tree_node*> q;
			q.push(tmp);
			int cnt = 0;
			while (!q.empty())
			{
				tmp = q.front();
				q.pop();
				cnt++;
				if (tmp->lm)
					cout << "lm node " << tmp->node_ID << ' ' << tmp->state << ' ' << tmp->timestamp << ' ' << tmp->edge_timestamp << endl;
				else
					cout << "node " << tmp->node_ID << ' ' << tmp->state << ' ' << tmp->timestamp << ' ' << tmp->edge_timestamp << endl;
				tmp = tmp->child;
				cout << "child: " << endl;
				while (tmp)
				{
					cout << tmp->node_ID << " " << tmp->state << endl;
					q.push(tmp);
					tmp = tmp->brother;
				}
				cout << endl;
			}
			cout << endl << endl;

		}
	}

	void print_path(unsigned int ID, unsigned int root_state, unsigned int dst, unsigned int dst_state)
	{
		if (auto iter = forests.find(merge_long_long(ID, root_state)); iter != forests.end()) {
			if (iter->second->node_map.find(dst_state) != iter->second->node_map.end()) {
				if (iter->second->node_map[dst_state]->index.find(dst) != iter->second->node_map[dst_state]->index.end()) {
					tree_node* tmp = iter->second->node_map[dst_state]->index[dst];
					while (tmp)
					{
						cout << tmp->node_ID << ' ' << tmp->state << ' ' << tmp->edge_timestamp << ' ' << tmp->timestamp << ' ';
						if (tmp->parent)
							cout << tmp->parent->node_ID << ' ' << tmp->parent->state << endl;
						else
							cout << "NULL" << endl;
						tmp = tmp->parent;
					}
				}
			}
		}
	}

	

};
