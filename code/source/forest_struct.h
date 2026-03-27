#pragma once
#include<map>
#include<unordered_map>
#include<unordered_set>
#include <queue>
#include<cassert>
#include <iostream>
#include <list>
#define um_shrink_threshold 2
#define merge_long_long(s, d) (((unsigned long long)s<<32)|d)
using namespace std;
#define min(x, y) (x<y?x:y)
#define max(x, y) (x>y?x:y)
#define MAX_INT 0x7FFFFFFF

// this file defines the basic structures and associated functions shared by both S-PATH and LM-SRPQ. Note that some variables are not needed by S-PATH, and they will not be included in memory computation for S-PATH.

template<typename K, typename V>
void shrink(unordered_map<K, V>& um)  // this function shrinks the bucket number of unordered map (um) if the number of bucket is 2 times larger than the number of KV pairs.
//Because the system will not automatically shrink the unordered_map or unordered_set, the bucket number may be much larger than the KV number if a lot of KV are inserted into the um the then deleted.
{
	if (um.empty()) return;
	if (static_cast<double>(um.bucket_count()) / um.size() > um_shrink_threshold) {
		um.reserve(um.size());
	}
}

template<typename K>
void shrink(unordered_set<K>& us) // this function shrinks the bucket number of unordered set
{
	if (us.empty()) return;
	if (static_cast<double>(us.bucket_count()) / us.size() > um_shrink_threshold)
		us.reserve(us.size());
}

struct tree_node // node in the spanning tree
{
	unsigned int node_ID;
	unsigned int edge_timestamp; // timestamp of the edge linked this node and its parent;
	unsigned int timestamp; // timestamp of this node
	unsigned int state;
	bool lm; // indicating if this is a landmark. This bool is not needed in S-PATH and will not be calculated in memory usage. 
	tree_node* parent;	// pointer to parent. As we may need to move a subtree from one parent to another, a parent pointer will accelerate this procedure, as suggested by the authors.
	tree_node* child;
	tree_node* brother;	// first child and list of brother, classic method for tree maintaining
	tree_node(unsigned int ID, unsigned int state_, unsigned int time, unsigned int edge_time)
	{
		node_ID = ID;
		state = state_;
		timestamp = time;
		edge_timestamp = edge_time;
		lm = false;
		parent = nullptr;
		child = nullptr;
		brother = nullptr;
	}
};

struct tree_node_index //  maps a vertex ID to the tree node in a spanning tree
{
	unordered_map<unsigned int, tree_node*> index;
	~tree_node_index()
	{
		index.clear();
	}
};
struct time_info_index // maps a vertex ID to the timestamp in TI map;
{
	unordered_map<unsigned int, unsigned int> index;
	~time_info_index()
	{
		index.clear();
	}
};

class RPQ_tree // class for the spanning trees in the spanning forest.
{
public:
	tree_node* root;
	map<unsigned int, tree_node_index*> node_map; // map from a state to the relevant node index. In the node index the reverse map from vertex ID to the tree node pointer is stored. The state in the first layer an the vertex ID in the second layer form a product graph node ID 
	map<unsigned int, time_info_index*> time_info; // TI map, used by LM-SRPQ, but not by S-PATH. Maps a state to the relevant time_info_index. In each the reverse index we map vertex ID to the timestamp in TI map.The state in the first layer an the vertex ID in the second layer form a product graph node ID 
	unordered_set<unsigned long long> landmarks; // set of landmarks contained in this tree. Merge the vertex ID and state with merge_long_long. Used by LM-SRPQ.
	unordered_map<unsigned long long, unsigned int> timed_landmarks; // this structure is used to directly get the landmarks and the timestamp of this landmark in the spanning tree. 
	// This structure is used when we need to traverse forward in the dependency graph, and thus is only needed in the dependency-forest version of LM-SRPQ
	int node_cnt;
	vector<unsigned int> tree_counter;

	RPQ_tree(int states_count) {
		root = nullptr;
		node_cnt = 0;
		tree_counter.resize(states_count, 0);
	}
	void clear()
	{
		if (root) {
			queue<tree_node*> q;
			q.push(root);
			while (!q.empty())
			{
				tree_node* tmp = q.front();
				q.pop();
				tree_node* cur = tmp->child;
				while (cur)
				{
					q.push(cur);
					cur = cur->brother;
				}
				delete tmp;
			}
			for (auto & iter : node_map)
				delete iter.second;
			for (auto & iter : time_info)
				delete iter.second;
			node_map.clear();
			landmarks.clear();
			time_info.clear();
			root = nullptr;
		}
	}
	void clear_time_info() // clear the TI map, used when a spanning tree is not an LM tree any more.
	{
		for (auto & iter : time_info)
			delete iter.second;
		time_info.clear();
	}
	~RPQ_tree()
	{
		clear();
	}
	void add_time_info(unsigned int v, unsigned int state, unsigned int time) // add a product graph node ID + timestamp pair to the TI map
	{
		if (time_info.find(state) == time_info.end())
			time_info[state] = new time_info_index;
		time_info[state]->index[v] = time;
	}
	unsigned int get_time_info(unsigned int v, unsigned int state) // get the timestamp of a product graph node in the TI map
	{
		if (time_info.find(state) == time_info.end())
			return 0;
		if (time_info[state]->index.find(v) != time_info[state]->index.end())
			return time_info[state]->index[v];
		else
			return 0;
	}
	tree_node* add_node(unsigned int v, unsigned int state, tree_node* parent, unsigned int time, unsigned int edge_time) // add a new tree node with given ID, state, node time ,edge time and parent
	{
		auto* tmp = new tree_node(v, state, time, edge_time);
		tmp->parent = parent;
		if (parent) {
			tmp->brother = parent->child; // add this node to the head of the child list of the parent
			parent->child = tmp;
		} else tmp->brother = nullptr;
		if (node_map.find(state) == node_map.end())
			node_map[state] = new tree_node_index;
		node_map[state]->index[v] = tmp; // add this node to the node map
		node_cnt++;
		tree_counter[state]++;
		return tmp;
	}
	void set_lm(unsigned int v, unsigned int state) // set the LM tag of a node to true;
	{
		if (node_map.find(state) == node_map.end()) {
			if (node_map[state]->index.find(v) != node_map[state]->index.end())
			{
				tree_node* tmp = node_map[state]->index[v];
				tmp->lm = true;
			}
		}
	}

	void add_lm(unsigned long long lm) // add a node into the LM set.
	{
		landmarks.insert(lm);
	}
	void add_timed_lm(unsigned long long lm, unsigned int timestamp)
	{
		timed_landmarks[lm] = timestamp;
	}
	void separate_node(tree_node* child) // separate a node from the spanning tree
	{
		if (child->parent == nullptr)
			return;
		if (child->parent->child == child) // find the node in the child list of the parent, and split it.
			child->parent->child = child->brother;
		else
		{
			tree_node* tmp = child->parent->child;
			while (tmp != nullptr && tmp->brother != child)
				tmp = tmp->brother;
			if (!tmp) {
				cerr << "Error: child node not found in the child list of its parent." << endl;
				exit(-1);
			}
			tmp->brother = child->brother;
		}
		child->parent = nullptr;
	}

	void remove_node(tree_node* node) // delete a node from the node map and the landmark set (if it is in the landmark set)
	{
		if (node_map.find(node->state) != node_map.end())
		{
			node_map[node->state]->index.erase(node->node_ID);
			shrink(node_map[node->state]->index);
			if (node_map[node->state]->index.empty())
				node_map.erase(node->state);
		}
		tree_counter[node->state]--;
		node_cnt--; // need to modify the node index in the upper layer.
		landmarks.erase(static_cast<unsigned long long>(node->node_ID) << 32 | node->state);
		shrink(landmarks);
	}
	void delete_node(tree_node* node) // delete a node, including separate it from and tree and delete it in the node map
	{
		separate_node(node);
		remove_node(node);
	}
	void remove_lm(tree_node* node) // delete a landmark.
	{
		landmarks.erase(node->node_ID);
		shrink(landmarks);
	}
	void remove_lm(unsigned long long ID)
	{
		landmarks.erase(ID);
		shrink(landmarks);
	}

	void substitute_parent(tree_node* parent, tree_node* child)// change the parent pointer of child to the given parent
	{
		if (child->parent->child == child) child->parent->child = child->brother;
		else {
			tree_node* tmp = child->parent->child;
			while (tmp != nullptr && tmp->brother != child) tmp = tmp->brother;
			if (!tmp) {
				cerr << "Error: child node not found in the child list of its parent." << endl;
				exit(-1);
			}
			tmp->brother = child->brother;
		}
		child->parent = parent;
		child->brother = parent->child;
		parent->child = child;
	}

	tree_node* find_node(unsigned int ID, unsigned int state) // given a product graph node, find its corresponding tree node
	{
		tree_node* result = nullptr;
		if (node_map.find(state) != node_map.end()) {
			if (node_map[state]->index.find(ID) != node_map[state]->index.end()) result = node_map[state]->index[ID];
		}
		return result;
	}


};

struct tree_info // structure for tree pointer list, used into the reverse map which maps product graph nodes to the spanning trees containing it.
{
	RPQ_tree* tree;
	tree_info* next;
	tree_info* prev;
	tree_info(RPQ_tree* t = nullptr)
	{
		tree = t;
		next = nullptr;
		prev = nullptr;
	}
};

struct v2t_unit // structure stores a vertex id and a tree_root ID, used to find the tree_info structure in a vertex_to_tree map, namely reverse map between product graph node and normal trees containing it.
{
	unsigned int vertex;
	unsigned int tree_root;
	v2t_unit(unsigned int v, unsigned int tr)
	{
		tree_root = tr;
		vertex = v;
	}
};

inline bool operator<(const v2t_unit& v1, const v2t_unit& v2)
{
	if (v1.vertex < v2.vertex)
		return true;
	else if (v1.vertex == v2.vertex)
	{
		if (v1.tree_root < v2.tree_root)
			return true;
	}
	return false;
}

inline bool operator==(const v2t_unit& v1, const v2t_unit& v2)
{
	if (v1.vertex == v2.vertex && v1.tree_root == v2.tree_root)
		return true;
	else
		return false;
}

inline bool operator>(const v2t_unit& v1, const v2t_unit& v2)
{
	if (v1.vertex > v2.vertex)
		return true;
	else if (v1.vertex == v2.vertex)
	{
		if (v1.tree_root > v2.tree_root)
			return true;
	}
	return false;
}

struct v2l_unit// structure stores a vertex id and a tree_root ID and a tree_root state, used to find the tree_info structure in a vertex_to_LM map, namely reverse map between product graph node and LM trees containing it.
	// different from v2t_unit, there is an additional root_state, as the state of root of normal trees is always the initial state 0, but LM trees have different root state;
{
	unsigned int vertex;
	unsigned int root_ID;
	unsigned int root_state;
	v2l_unit(unsigned int v, unsigned int tr, unsigned int state)
	{
		root_ID = tr;
		vertex = v;
		root_state = state;
	}
};

inline bool operator<(const v2l_unit& v1, const v2l_unit& v2)
{
	if (v1.vertex < v2.vertex) return true;
	if (v1.vertex == v2.vertex) {
		if (v1.root_ID < v2.root_ID) return true;
		if (v1.root_ID == v2.root_ID && v1.root_state < v2.root_state) return true;
	}
	return false;
}

inline bool operator==(const v2l_unit& v1, const v2l_unit& v2)
{
	return (v1.vertex == v2.vertex && v1.root_ID == v2.root_ID &&v1.root_state == v2.root_state);

}

inline bool operator>(const v2l_unit& v1, const v2l_unit& v2)
{
	if (v1.vertex > v2.vertex)
		return true;
	if (v1.vertex == v2.vertex) {
		if (v1.root_ID > v2.root_ID)
			return true;
		if (v1.root_ID == v2.root_ID && v1.root_state > v2.root_state)
			return true;
	}
	return false;
}

class tree_info_index // reverse index from vertex ID to normal trees containing the product graph node. State of the product graph node is given in the upper layer. All product graph nodes in this index have the same state.
{
public:
	unordered_map<unsigned int, tree_info*> tree_index; // map from vertex ID to normal trees
	map<v2t_unit, tree_info*> info_map; // map from combination of vertex ID and tree root to the tree_info unit, used in deletion to quickly delete a reverse index unit.
	tree_info_index() = default;
	~tree_info_index()
	{
		for (auto & iter : tree_index)
		{
			tree_info* cur = iter.second;
			while (cur)
			{
				tree_info* tmp = cur;
				cur = cur->next;
				delete tmp;
			}
		}
		tree_index.clear();
		info_map.clear();
	}
	void add_node(RPQ_tree* tree_pt, unsigned int v, unsigned int root_ID) // add a normal tree pointer + vertex ID to the reverse index
	{
		if (info_map.find(v2t_unit(v, root_ID)) != info_map.end())	// if the combination is already stored.
			return;
		auto iter = tree_index.find(v);
		if (iter == tree_index.end())
		{
			auto* cur = new tree_info(tree_pt); //if there is no tree list before, add a new one
			tree_index[v] = cur;
			info_map[v2t_unit(v, root_ID)] = cur; // add this unit to the info_map
		}
		else
		{
			auto* cur = new tree_info(tree_pt);// If there is a tree lits, add the new tree_info to the head
			cur->next = iter->second;
			iter->second->prev = cur;
			iter->second = cur;
			info_map[v2t_unit(v, root_ID)] = cur;
		}
	}

	void delete_node(unsigned int v, unsigned int root_ID) // delete a tree info unit given the vertex ID -tree root pair, used when a node is deleted from a normal tree.
	{
		if (info_map.find(v2t_unit(v, root_ID)) != info_map.end()) // use the info_map to find the unit without scanning the list. 
		{
			auto iter = info_map.find(v2t_unit(v, root_ID));
			tree_info* cur = iter->second;
			if (cur->prev) // if this unit is not the head of the tree list 
			{
				cur->prev->next = cur->next;
				if (cur->next)
					cur->next->prev = cur->prev;
			}
			else
			{
				if (auto iterator = tree_index.find(v); iterator != tree_index.end())
				{
					assert(iterator->second == cur);
					if (cur->next)
					{
						iterator->second = cur->next;
						cur->next->prev = nullptr;
					}
					else {
						tree_index.erase(iterator); // if the tree list is empty;
						shrink(tree_index);
					}
				}
			}
			info_map.erase(iter);
			delete cur;
		}
	}
};


class lm_info_index // similar to the tree_info_index, but is used for LM trees. All the functions are also similar.
{
public:
	unordered_map<unsigned int, tree_info*> tree_index;
	map<v2l_unit, tree_info*> info_map;
	lm_info_index() = default;
	~lm_info_index()
	{
		for (auto &[fst, snd] : tree_index)
		{
			tree_info* cur = snd;
			while (cur)
			{
				tree_info* tmp = cur;
				cur = cur->next;
				delete tmp;
			}
		}
		tree_index.clear();
		info_map.clear();
	}
	void add_node(RPQ_tree* tree_pt, unsigned int v, unsigned int root_ID, unsigned int root_state)
	{
		if (info_map.find(v2l_unit(v, root_ID, root_state)) != info_map.end())
			return;
		if (auto iter = tree_index.find(v); iter == tree_index.end())
		{
			auto* cur = new tree_info(tree_pt);
			tree_index[v] = cur;
			info_map[v2l_unit(v, root_ID, root_state)] = cur;
		}
		else
		{
			auto* cur = new tree_info(tree_pt);
			cur->next = iter->second;
			iter->second->prev = cur;
			iter->second = cur;
			info_map[v2l_unit(v, root_ID, root_state)] = cur;
		}
	}

	void delete_node(unsigned int v, unsigned int root_ID, unsigned int root_state)
	{
		if (info_map.find(v2l_unit(v, root_ID, root_state)) != info_map.end())
		{
			auto iter = info_map.find(v2l_unit(v, root_ID, root_state));
			tree_info* cur = iter->second;
			if (cur->prev)
			{
				cur->prev->next = cur->next;
				if (cur->next)
					cur->next->prev = cur->prev;
			}
			else
			{
				if (auto iterator = tree_index.find(v); iterator != tree_index.end())
				{
					assert(iterator->second == cur);
					if (cur->next)
					{
						iterator->second = cur->next;
						cur->next->prev = nullptr;
					}
					else {
						tree_index.erase(iterator);
						shrink(tree_index);
					}
				}
			}
			info_map.erase(iter);
			delete cur;
		}
	}
};

struct vertex_score // structe used to record score of a product graph node, used in landmark selection. The score is the approximated stpanning tree size.
{
	unsigned int ID;
	unsigned int state;
	double score;
	vertex_score(unsigned int v, unsigned int state_, double s)
	{
		ID = v;
		state = state_;
		score = s;
	}
};

inline bool my_compare(const vertex_score& v1, const vertex_score& v2)
{
	return v1.score < v2.score;
}

inline bool operator<(const vertex_score& v1, const vertex_score& v2)
{
	return v1.score < v2.score;
}


inline bool operator>(const vertex_score& v1, const vertex_score& v2)
{
	return v1.score > v2.score;
}


struct time_compare
{
	bool operator()(tree_node* &t1, tree_node* &t2)
	{
		return t1->timestamp<t2->timestamp;
	}
};

struct pair_compare
{
	bool operator()(pair<unsigned long long, unsigned int>& p1, pair<unsigned long long, unsigned int> &p2)
	{
		return p1.second<p2.second;
	}
};
