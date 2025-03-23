#pragma once
#include<iostream>
#include<fstream>
#include<map>
#include<unordered_map>
#include<unordered_set>
#include<string>
#include <queue>
#include<algorithm>
#include "forest_struct.h"
#include "../fsa.h"
#include "../streaming_graph.h"

#define merge_long_long(s, d) (((unsigned long long)s<<32)|d)

using namespace std;

// code for the S-PATH algorithm

class RPQ_forest
{
public:

	streaming_graph* g;
    [[nodiscard]] streaming_graph *getSnapshotGraph() const {
        return g;
    }

    // pointer to the streaming graph .
	FiniteStateAutomaton* aut; // pointer to the DFA of the regular expression.
	unordered_map<unsigned long long, RPQ_tree*> forests; // map from product graph node to tree pointer
	map<long long, tree_info_index*> v2t_index; // reverse index that maps a graph vertex to the trees that contains it. The first layer maps state to tree_info_index, and the second layer maps vertex ID to list of trees contains this node
	unordered_map<unsigned long long, long long> result_pairs; // result set, maps from vertex paris to the largest timestamp of regular paths between them

	long long distinct_results = 0;
	long long expand_call = 0;
	double peek_memory = 0;
	double memory_current_avg; // La media corrente
	size_t memory_count;

	RPQ_forest(streaming_graph* g_, FiniteStateAutomaton* automaton)
	{
		g = g_;
		aut = automaton;
	}
	~RPQ_forest()
	{
		unordered_map<unsigned long long, RPQ_tree*>::iterator it;
		for (it = forests.begin(); it != forests.end(); it++)
			delete it->second;
		forests.clear();
		map<long long, tree_info_index*>::iterator it2;
		for (it2 = v2t_index.begin(); it2 != v2t_index.end(); it2++)
			delete it2->second;
		v2t_index.clear();
		result_pairs.clear();
	}

	// export the result set into a csv file with the format of "src, dst, timestamp"
	void export_result(const string& file_name)
    {
        ofstream fout(file_name);
        for (auto & iter : result_pairs)
        {
            long long src = iter.first >> 32;
            long long dst = iter.first & 0xFFFFFFFF;
            fout << src << "," << dst << "," << iter.second << endl;
        }
        fout.close();
    }

	void update_result(unordered_map<long long, long long>& updated_nodes, long long root_ID) // update the result set, first of a KV in the um is a vertex ID v, and second is a timestamp t, update timestamp of (root v) pair to t
	{
		for (auto & updated_node : updated_nodes)
		{
			long long dst = updated_node.first;
			long long time = updated_node.second;
			if (dst == root_ID) // self-join is omitted
				continue;
			unsigned long long result_pair = static_cast<unsigned long long>(root_ID) << 32 | dst;
			if (result_pairs.find(result_pair) != result_pairs.end())
				result_pairs[result_pair] = result_pairs[result_pair] > time ? result_pairs[result_pair] : time; // if the vertex pair exists, update its timestamp
			else {
				result_pairs[result_pair] = time;  // else add the vertex pair.
				distinct_results++;
			}
		}
	}

	void add_index(RPQ_tree* tree_pt, long long v, long long state, long long root_ID) // modify the reverse index to record the presence of a node in a tree.
	{
		auto iter = v2t_index.find(state);
		if (iter == v2t_index.end())
			v2t_index[state] = new tree_info_index;
		v2t_index[state]->add_node(tree_pt, v, root_ID);
	}

	// Addition of tree node for S-PATH
	tree_node* add_node(RPQ_tree* tree_pt, long long v, long long state, long long root_ID, tree_node* parent, long long timestamp, long long edge_time, long long edge_exp) // add  a node to a spanning tree, given all the necessary information.
	{
		add_index(tree_pt, v, state, root_ID);
		tree_node* tmp = tree_pt->add_node(v, state, parent, timestamp, edge_time, edge_exp);
		return tmp;
	}

	void delete_index(long long v, long long state, long long root) // modify the reverse index when a node is not in a spanning tree any more.
	{
		auto iter = v2t_index.find(state);
		if (iter != v2t_index.end())
		{
			iter->second->delete_node(v, root);
			if (iter->second->tree_index.empty())
				v2t_index.erase(iter);
		}
	}
	void expand(tree_node* expand_node, RPQ_tree* tree_pt) // function used to expand a spanning tree with a BFS manner when a new node is added into a spanning tree. expand node is the new node
	{
		expand_call++;
		long long root_ID = tree_pt->root->node_ID;
		unordered_map<long long, long long> updated_results;
		priority_queue<tree_node*, vector<tree_node*>, time_compare> q;
		q.push(expand_node);
		while (!q.empty())
		{
			tree_node* tmp = q.top();
			q.pop();
			unsigned long long tmp_info = merge_long_long(tmp->node_ID, tmp->state);
			if (aut->isFinalState(tmp->state)) { // if this is a final state, we need to update the result set, we record it first and at last carry out the update together
				if (updated_results.find(tmp->node_ID) != updated_results.end())
					updated_results[tmp->node_ID] = updated_results[tmp->node_ID] > tmp->edge_expiration ? updated_results[tmp->node_ID] : tmp->edge_expiration;
				else {
					updated_results[tmp->node_ID] = tmp->edge_expiration;
				}
			}

			vector<edge_info> vec = g->get_all_suc(tmp->node_ID); // get all the successor edges in the snapshot graph, and check each of them to find the successor nodes in the product graph.
			for (auto & i : vec)
			{
				long long successor = i.d;
				long long edge_label = i.label;
				long long dst_state = aut->getNextState(tmp->state, edge_label);
				if (dst_state == -1)
					continue;
				long long time = min(tmp->timestamp, i.timestamp);
				if (tree_pt->node_map.find(dst_state) == tree_pt->node_map.end() || tree_pt->node_map[dst_state]->index.find(successor) == tree_pt->node_map[dst_state]->index.end())// If this node does not exit before, we add this node.
					// todo added exp
					q.push(add_node(tree_pt, successor, dst_state, tree_pt->root->node_ID, tmp, time, i.timestamp, i.expiration_time));
				else
				{
					tree_node* dst_pt = tree_pt->node_map[dst_state]->index[successor];
					if (dst_pt->timestamp < time) { // else if its current timestamp is smaller than the new timestamp, we update the timestamp and link it to the new parent.
						if (dst_pt->parent != tmp)
							tree_pt->substitute_parent(tmp, dst_pt);
						dst_pt->timestamp = time;
						dst_pt->edge_timestamp = i.timestamp;
						q.push(dst_pt);
					}
				}
			}
		}
		update_result(updated_results, tree_pt->root->node_ID);
		updated_results.clear();
	}

	void insert_per_tree(long long s, long long d, long long label, long long timestamp, long long exp, long long src_state, long long dst_state,
		RPQ_tree* tree_pt) // processing a new product graph edge from (s, src_state) to (d, dst_state) in a spanning tree tree_pt; 
	{
		if (tree_pt->node_map.find(src_state) != tree_pt->node_map.end())
		{
			tree_node_index* tmp_index = tree_pt->node_map[src_state];
			if (tmp_index->index.find(s) != tmp_index->index.end()) // find the src node
			{
				tree_node* src_pt = tmp_index->index[s];
				long long time = min(src_pt->timestamp, timestamp);
				if (tree_pt->node_map.find(dst_state) == tree_pt->node_map.end() || tree_pt->node_map[dst_state]->index.find(d) == tree_pt->node_map[dst_state]->index.end()) { // if the dst node does not exit
					// todo added exp
					tree_node* dst_pt = add_node(tree_pt, d, dst_state, tree_pt->root->node_ID, src_pt, min(src_pt->timestamp, timestamp), timestamp, exp);
					expand(dst_pt, tree_pt); // add the dst node and futher expand,
				}
				else
				{
					tree_node* dst_pt = tree_pt->node_map[dst_state]->index[d];
					if (dst_pt->timestamp < time) // if the dst node exists but has a smaller timestamp, update its timestamp, and use expand to propagate the new timestamp down.
					{
						if (dst_pt->parent != src_pt)
							tree_pt->substitute_parent(src_pt, dst_pt);
						dst_pt->timestamp = time;
						dst_pt->edge_timestamp = timestamp;
						expand(dst_pt, tree_pt);
					}
				}
			}
		}
	}

	sg_edge * insert_edge_spath(long long id, long long s, long long d, long long label, long long timestamp, long long exp) //  a new snapshot graph edge (s, d) is inserted, update the spanning forest accordingly.
	{

		sg_edge* new_sg_edge = g->insert_edge(id, s, d, label, timestamp, exp); // update snapshot graph

		if (aut->getNextState(0, label) != -1) // if this edge can be accepted by the initial state, and this is no spanning tree with root (s, 0), we add this tree
		{
			if (forests.find(merge_long_long(s, 0)) == forests.end()) {
				auto* new_tree = new RPQ_tree();
				new_tree->root = add_node(new_tree, s, 0, s, nullptr, MAX_INT, MAX_INT, exp);
				forests[merge_long_long(s, 0)] = new_tree;
			}
		}
		vector<pair<long long, long long> >vec = aut->getStatePairsWithTransition(label);// find all the state pairs where the src state can translate to the dst state when accepting this label
		for (auto & i : vec) {
			long long src_state = i.first;
			long long dst_state = i.second;
			auto index_iter2 = v2t_index.find(src_state);
			if (index_iter2 != v2t_index.end())
			{
				auto tree_iter = index_iter2->second->tree_index.find(s);
				if (tree_iter != index_iter2->second->tree_index.end()) {
					tree_info* tmp = tree_iter->second;
					while (tmp)
					{
						// todo Propagating also here
						insert_per_tree(s, d, label, timestamp, exp, src_state, dst_state, tmp->tree); // for each state pair, find the trees containing (s, src_state), and update it with the new edge.
						tmp = tmp->next;
					}
				}
			}
		}

		return new_sg_edge;
	}

	void results_update(long long time) // given the threshold of expiration, delete all the expired result pairs.
	{
		if (time <= 0)
			return;
		for (auto iter = result_pairs.begin(); iter != result_pairs.end();)
		{
			if (iter->second < time) {
				iter = result_pairs.erase(iter);
			}
			else
				iter++;
		}
		shrink(result_pairs);
	}

	void erase_tree_node(RPQ_tree* tree_pt, tree_node* child) // given an expired node, delete the subtree rooted at it in tree_pt, all the nodes in its subtree also expire. 
	{
		queue<tree_node*> q;
		q.push(child);
		tree_pt->separate_node(child); // 'child' is disconnected with its parent,other nodes donot need to call this function, as there parents and brothers are all deleted;
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


	void expire_per_tree(long long v, long long state, RPQ_tree* tree_pt, long long current_time) // given a produce graph node (v, state) which can be possibly an expired node, can try to delete its subtree.
	{
		if (tree_pt->node_map.find(state) != tree_pt->node_map.end())
		{
			if (tree_pt->node_map[state]->index.find(v) != tree_pt->node_map[state]->index.end()) {
				tree_node* dst_pt = tree_pt->node_map[state]->index[v];
				if (dst_pt->edge_expiration < current_time) // if it is indeed an expired node, delete its subtree.
					erase_tree_node(tree_pt, dst_pt);
			}

		}
	}

	void expire(long long current_time, const vector<edge_info>& deleted_edges) // given current time, delete the expired tree nodes and results.
	{
		// long long expire_time = current_time - g->window_size; // compute the threshold of expiration.
		// results_update(frontier); // delete expired results
		// g->expire(current_time, deleted_edges); // delete expired graph edges.

		// for each expired edge, find the expired nodes in the spanning forest
		// significant edges will not be erased nor from the graph nor from the forest, this leads to having potentially
		unordered_set<unsigned long long> visited; 
		for (auto & deleted_edge : deleted_edges)
		{
			long long dst = deleted_edge.d;
			long long label = deleted_edge.label; // for each expired edge, find its dst node. All the expired nodes in the spanning forest must be in a subtree of such dst node.
			vector<pair<long long, long long> > vec = aut->getStatePairsWithTransition(label); // get possible states of the dst node.
			for (auto & j : vec) {
				long long dst_state = j.second;
				if (dst_state == -1)
					continue;
				if(visited.find(merge_long_long(dst, dst_state))!=visited.end())
					continue;
				visited.insert(merge_long_long(dst,dst_state)); // record the checked dst node, in case of repeated process.
				auto iter = v2t_index.find(dst_state);
				if (iter != v2t_index.end())
				{
					auto tree_iter = iter->second->tree_index.find(dst);
					if (tree_iter != iter->second->tree_index.end())
					{
						vector<RPQ_tree*> tree_to_delete;
						tree_info* tmp = tree_iter->second;
						while (tmp)	// first record the trees in the list with a vector, as when we delete expired nodes we will change the tree list, leading to error in the list scan.
						{
							tree_to_delete.push_back(tmp->tree);
							tmp = tmp->next;
						}
						for (auto & k : tree_to_delete)
						{
							expire_per_tree(dst, dst_state, k, current_time); // expire in each tree
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



	void output_match(ofstream& fout) // output the recorded result pairs, used to 
	{
		for (unordered_map<unsigned long long, long long>::iterator iter = result_pairs.begin(); iter != result_pairs.end(); iter++)
			fout << (iter->first >> 32) << " " << (iter->first & 0xFFFFFFFF) << " " << iter->second << endl;
	}

	std::pair<size_t, long long> count(ofstream& fout, long long expired_time = 0) // count the memory used in the algorithm, but exclude the memory of automaton and streaming graph, because they are essential for any algorithm.
	{
		long long um_size = sizeof(unordered_map<long long, long long>); // size of statistics in a map, which does not change with the key-value type, usually is 56, but may change with the system.
		long long m_size = sizeof(map<long long, long long>); // size of pointers and statistics in a map, which does not change with the key-value type, usually is 48, but may change with the system.
		// the memory of an unordered_map is computed as um_size + bucket_count()*8 + KV_number * (KV_size + 8). It is a hash table where each bucket is a pointer, pointing to a KV list. Each KV is associated with a pointer
		// pointing to next KV in the list.
		// the memory of a map is computed as m_size + KV_number * (KV_size + 24). It is a binary search tree where each KV is associated with 3 pointer, 2 for child and 1 for parent.
		double memory =  (double)(result_pairs.size() * 24 + result_pairs.bucket_count() * 8 + um_size) / (1024 * 1024);
		cout << "result pair size: " << result_pairs.size() << ", memory: " << memory << endl;  // number of result vertex pairs, and the memory used to store these results.
		fout << "result pair size: " << result_pairs.size() << ", memory: " << memory << endl;
		long long tree_size = m_size + 16; // For S-PATH, we only consider the memory of node_map, root pointer and integer node_num;
		double tree_memory = ((double)(um_size + forests.bucket_count() * 8 + forests.size() * (24+tree_size)) / (1024 * 1024)); //  forests has KV size 16, 
		double tree_node_memory = 0;
		double sum = 0;
		double sum_of_squares = 0;
		unsigned extent = 0;
		size_t num_trees = 0;

		for (auto & forest : forests)
		{
			RPQ_tree* tree_pt = forest.second;
			double node_memory = 0;
			node_memory += tree_pt->node_map.size() * 40; 
			for (map<long long, tree_node_index*>::iterator iter2 = tree_pt->node_map.begin(); iter2 != tree_pt->node_map.end(); iter2++)
				node_memory += um_size + iter2->second->index.bucket_count() * 8 + iter2->second->index.size() * (24 +40); // 24 is the KV size and 40 is size of each tree node
			tree_node_memory += node_memory;

			double node_count = tree_pt->node_cnt;
			unsigned min_ts = tree_pt->min_timestamp;
			unsigned max_ts = tree_pt->max_timestamp;

			// compute average
			sum += node_count;
			sum_of_squares += node_count * node_count;
			++num_trees;

			// compute average path time extent
			extent += (max_ts - min_ts);
		}

		if (num_trees > 0)
		{
			double average = sum / num_trees;
			double avg_extent = extent / num_trees;
			double variance = (sum_of_squares / num_trees) - (average * average);

			// Output results
			std::cout << "Average number of nodes in trees: " << average << "\n";
			std::cout << "Variance: " << variance << "\n";
			std::cout << "Average extent: " << avg_extent << "\n";
		}

		long long tree_node_num = 0;
		tree_node_memory += m_size  + v2t_index.size() * 40; // memory of the first layer of v2t_index, each KV has 16 byte (integer + pointer)
		for (auto & iter : v2t_index)
		{
			tree_node_memory += um_size + m_size; // each tree_info index has one unordered map and one map
			tree_node_memory += iter.second->tree_index.bucket_count() * 8 + iter.second->tree_index.size() * 24;
			for (auto & iter2 : iter.second->tree_index)
			{
				tree_info* tmp = iter2.second;
				while (tmp)
				{
					tree_node_num++;
					tmp = tmp->next;
				}
			}
			tree_node_memory += iter.second->info_map.size() * 40; // the KV pair of info_map has size 16
		}
		tree_node_memory += tree_node_num * 24;
		tree_node_memory = tree_node_memory / (1024 * 1024);
		memory_count++;
		memory_current_avg = ((memory_current_avg * (memory_count-1)) + tree_node_memory) / memory_count;
		peek_memory = peek_memory > tree_node_memory ? peek_memory : tree_node_memory;
		cout << "total node number in forest: " << tree_node_num << ", memory: " << tree_node_memory << endl; // the total number of nodes in the forest
		cout << "total memory besides result set: " << (tree_memory + tree_node_memory) << endl; // total memory usage
		fout << "total node number in forest: " << tree_node_num << ", memory: " << tree_node_memory << endl;
		fout << "total memory besides result set: " << (tree_memory + tree_node_memory) << endl;


		return std::make_pair(result_pairs.size(), tree_node_num);
	}

	long long count_nodes_forest() {
		long long tree_node_num = 0;
		for (auto & iter : v2t_index)
		{
			for (auto & iter2 : iter.second->tree_index)
			{
				tree_info* tmp = iter2.second;
				while (tmp)
				{
					tree_node_num++;
					tmp = tmp->next;
				}
			}
		}
		return tree_node_num;
	}

	void print_tree(long long ID, long long state)
	{

		unordered_map<unsigned long long, RPQ_tree*>::iterator iter = forests.find(merge_long_long(ID, state));
		if (iter != forests.end()) {
			tree_node* tmp = iter->second->root;
			queue<tree_node*> q;
			q.push(tmp);
			long long cnt = 0;
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

	void print_path(long long ID, long long root_state, long long dst, long long dst_state)
	{
		unordered_map<unsigned long long, RPQ_tree*>::iterator iter = forests.find(merge_long_long(ID, root_state));
		if (iter != forests.end()) {
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
