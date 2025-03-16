#pragma once
#include<iostream>
#include<fstream>
#include<map>
#include<unordered_map>
#include<unordered_set>
#include <queue>
#include<cassert>
#define um_shrink_threshold 2
#define merge_long_long(s, d) (((unsigned long long)s<<32)|d)
using namespace std;

#define MAX_INT 0x7FFFFFFF

inline long long min_custom (long long x, long long y) {
	return x < y ? x : y;
}

// this file defines the basic structures and associated functions shared by both S-PATH and LM-SRPQ. Note that some variables are not needed by S-PATH, and they will not be included in memory computation for S-PATH.

template<typename K, typename V>
inline void shrink(unordered_map<K, V>& um)  // this function shrinks the bucket number of unordered map(um) if the number of bucket is 2 times larger than the number of KV pairs. 
//Because the system will not automatically shrink the unordered_map or unodered_set, the bucket number may be much larger than the KV number if a lot of KV are inerted into the um the then deleted. 
{
	if (((double)um.bucket_count()) / um.size() > um_shrink_threshold) {
		um.reserve(um.size());
	}
}

template<typename K>
inline void shrink(unordered_set<K>& us) // this function shrinks the bucket number of unordered set
{
	if (((double)us.bucket_count()) / us.size() > um_shrink_threshold)
		us.reserve(us.size());
}

struct tree_node // node in the spanning tree
{
	long long node_ID;
	long long edge_timestamp; // timestamp of the edge linked this node and its parent;
	long long timestamp; // timestamp of this node
	long long edge_expiration;
	long long state;
	bool lm; // indicating if this is a landmark. This bool is not needed in S-PATH and will not be calculated in memory usage. 
	tree_node* parent;	// pointer to parent. As we may need to move a subtree from one parent to another, a parent pointer will accelerate this procedure, as suggested by the authors.
	tree_node* child;
	tree_node* brother;	// first child and list of brother, classic method for tree maintaining
	tree_node(long long ID, long long state_, long long time, long long edge_time)
	{
		node_ID = ID;
		state = state_;
		timestamp = time;
		edge_timestamp = edge_time;
		parent = NULL;
		child = NULL;
		brother = NULL;
		lm = false;
	}
};

struct tree_node_index //  maps a vertex ID to the tree node in a spanning tree
{
	unordered_map<long long, tree_node*> index;
	~tree_node_index()
	{
		index.clear();
	}
};
struct time_info_index // maps a vertex ID to the timestamp in TI map;
{
	unordered_map<long long, long long> index;
	~time_info_index()
	{
		index.clear();
	}
};

class RPQ_tree // class for the spanning trees in the spanning forest.
{
public:
	tree_node* root;
	map<long long, tree_node_index*> node_map; // map from a state to the relevant node index. In the node index the reverse map from vertex ID to the tree node pointer is stored. The state in the first layer an the vertex ID in the second layer form a product graph node ID
	map<long long, time_info_index*> time_info; // TI map, used by LM-SRPQ, but not by S-PATH. Maps a state to the relevant time_info_index. In each the reverse index we map vertex ID to the timestamp in TI map.The state in the first layer an the vertex ID in the second layer form a product graph node ID
	unordered_set<unsigned long long> landmarks; // set of landmarks contained in this tree. Merge the vertex ID and state with merge_long_long. Used by LM-SRPQ.
	unordered_map<unsigned long long, long long> timed_landmarks; // this structure is used to directly get the landmarks and the timestamp of this landmark in the spanning tree.
	// This structure is used when we need to traverse forward in the dependency graph, and thus is only needed in the dependency-forest version of LM-SRPQ
	long long node_cnt;
	long long min_timestamp = MAX_INT;
	long long max_timestamp = -1;

	RPQ_tree()
	{
		root = NULL;
		node_cnt = 0;
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
			for (map<long long, tree_node_index*>::iterator iter = node_map.begin(); iter != node_map.end(); iter++)
				delete iter->second;
			for (map<long long, time_info_index*>::iterator iter = time_info.begin(); iter != time_info.end(); iter++)
				delete iter->second;
			node_map.clear();
			landmarks.clear();
			time_info.clear();
			root = NULL;
		}
	}
	void clear_time_info() // clear the TI map, used when a spanning tree is not an LM tree any more.
	{
		for (map<long long, time_info_index*>::iterator iter = time_info.begin(); iter != time_info.end(); iter++)
			delete iter->second;
		time_info.clear();
	}
	~RPQ_tree()
	{
		clear();
	}
	void add_time_info(long long v, long long state, long long time) // add a product graph node ID + timestamp pair to the TI map
	{
		if (time_info.find(state) == time_info.end())
			time_info[state] = new time_info_index;
		time_info[state]->index[v] = time;
	}
	long long get_time_info(long long v, long long state) // get the timestamp of a product graph node in the TI map
	{
		if (time_info.find(state) == time_info.end())
			return 0;
		if (time_info[state]->index.find(v) != time_info[state]->index.end())
			return time_info[state]->index[v];
		else
			return 0;
	}
	tree_node* add_node(long long v, long long state, tree_node* parent, long long time, long long edge_time, long long exp) // add a new tree node with given ID, state, node time ,edge time and parent
	{
		auto* tmp = new tree_node(v, state, time, edge_time);
		tmp->edge_expiration = exp;
		tmp->parent = parent;
		if (parent) {
			tmp->brother = parent->child; // add this node to the head of the child list of the parent
			parent->child = tmp;
		}
		else
			tmp->brother = nullptr;
		if (node_map.find(state) == node_map.end())
			node_map[state] = new tree_node_index;
		node_map[state]->index[v] = tmp; // add this node to the node map
		node_cnt++;
		if (edge_time < min_timestamp) {
			min_timestamp = edge_time;
		}
		if (edge_time > max_timestamp) {
			max_timestamp = edge_time;
		}
		return tmp;
	}

	void set_lm(long long v, long long state) // set the LM tag of a node to true;
	{
		if (node_map.find(state) != node_map.end() && node_map[state]->index.find(v) != node_map[state]->index.end()) {
			tree_node *tmp = node_map[state]->index[v];
			tmp->lm = true;
		}
	}

	void add_lm(unsigned long long lm) // add a node into the LM set.
	{
		landmarks.insert(lm);
	}
	void add_timed_lm(unsigned long long lm, long long timestamp)
	{
		timed_landmarks[lm] = timestamp;
	}
	void separate_node(tree_node* child) // separate a node from the spanning tree 
	{
		if (child->parent == NULL)
			return;
		if (child->parent->child == child) // find the node in the child list of the parent, and split it.
			child->parent->child = child->brother;
		else
		{
			tree_node* tmp = child->parent->child;
			while (tmp != NULL && tmp->brother != child)
				tmp = tmp->brother;
			tmp->brother = child->brother;
		}
		child->parent = NULL;
	}

	void remove_node(tree_node* node) // delete a node from the node map and the landmark set (if it is in the landmark set)
	{
		long long v = node->node_ID;
		if (node_map.find(node->state) != node_map.end())
		{
			node_map[node->state]->index.erase(node->node_ID);
			shrink(node_map[node->state]->index);
			if (node_map[node->state]->index.empty())
				node_map.erase(node->state);
		}
		node_cnt--; // need to modify the node index in the upper layer.
		if (node_cnt == 0) cout << "ERROR: Removing root node" << endl;
		landmarks.erase((unsigned long long)node->node_ID << 32 | node->state);
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

	tree_node* remove_node(long long v, long long state) // given a product graph node, delete its corresponding tree node from the node map and return the tree node pointer.
	{
		tree_node* ans = NULL;
		if (node_map.find(state) != node_map.end())
		{
			if (node_map[state]->index.find(v) != node_map[state]->index.end())
			{
				ans = node_map[state]->index[v];
				node_map[state]->index.erase(v);
				node_cnt--;
				landmarks.erase((unsigned long long)v << 32 | state);
				shrink(landmarks);
				shrink(node_map[state]->index);
				if (node_map[state]->index.empty())
					node_map.erase(state);
			}
		}
		return ans;
		
	}

	tree_node* delete_node(long long v, long long state) // given a product graph node, separate its corresponding tree node from the spanning tree and delete it from the nodemap, and return the tree node pointer.
	{
		tree_node* node = remove_node(v, state);
		separate_node(node);
		return node;
	}

	void substitute_parent(tree_node* parent, tree_node* child)// change the parent pointer of child to the given parent
	{
		if (child->parent->child == child)
			child->parent->child = child->brother;
		else
		{
			tree_node* tmp = child->parent->child;
			while (tmp != NULL && tmp->brother != child)
				tmp = tmp->brother;
			tmp->brother = child->brother;
		}
		child->parent = parent;
		child->brother = parent->child;
		parent->child = child;
	}

	tree_node* find_node(long long ID, long long state) // given a product graph node, find its corresponding tree node
	{
		tree_node* result = NULL;
		if (node_map.find(state) != node_map.end())
		{
			if (node_map[state]->index.find(ID) != node_map[state]->index.end())
				result = node_map[state]->index[ID];
		}
		return result;
	}


};

struct tree_info // structure for tree pointer list, used into the reverse map which maps product graph nodes to the spanning trees containing it.
{
	RPQ_tree* tree;
	tree_info* next;
	tree_info* prev;
	tree_info(RPQ_tree* t = NULL)
	{
		tree = t;
		next = NULL;
		prev = NULL;
	}
};

struct v2t_unit // structure stores a vertex id and a tree_root ID, used to find the tree_info structure in a vertex_to_tree map, namely reverse map between product graph node and normal trees containing it.
{
	long long vertex;
	long long tree_root;
	v2t_unit(long long v, long long tr)
	{
		tree_root = tr;
		vertex = v;
	}
};

bool operator<(const v2t_unit& v1, const v2t_unit& v2)
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

bool operator==(const v2t_unit& v1, const v2t_unit& v2)
{
	if (v1.vertex == v2.vertex && v1.tree_root == v2.tree_root)
		return true;
	else
		return false;
}

bool operator>(const v2t_unit& v1, const v2t_unit& v2)
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
	long long vertex;
	long long root_ID;
	long long root_state;
	v2l_unit(long long v, long long tr, long long state)
	{
		root_ID = tr;
		vertex = v;
		root_state = state;
	}
};

bool operator<(const v2l_unit& v1, const v2l_unit& v2)
{
	if (v1.vertex < v2.vertex)
		return true;
	else if (v1.vertex == v2.vertex)
	{
		if (v1.root_ID < v2.root_ID)
			return true;
		else if (v1.root_ID == v2.root_ID && v1.root_state < v2.root_state)
			return true;
	}
	return false;
}

bool operator==(const v2l_unit& v1, const v2l_unit& v2)
{
	if (v1.vertex == v2.vertex && v1.root_ID == v2.root_ID &&v1.root_state == v2.root_state)
		return true;
	else
		return false;
}

bool operator>(const v2l_unit& v1, const v2l_unit& v2)
{
	if (v1.vertex > v2.vertex)
		return true;
	else if (v1.vertex == v2.vertex)
	{
		if (v1.root_ID > v2.root_ID)
			return true;
		else if (v1.root_ID == v2.root_ID && v1.root_state > v2.root_state)
			return true;
	}
	return false;
}

class tree_info_index // reverse index from vertex ID to normal trees containing the product graph node. State of the product graph node is given in the upper layer. All product graph nodes in this index have the same state.
{
public:
	unordered_map<long long, tree_info*> tree_index; // map from vertex ID to normal trees
	map<v2t_unit, tree_info*> info_map; // map from combination of vertex ID and tree root to the tree_info unit, used in deletion to quickly delete a reverse index unit.
	tree_info_index() {}
	~tree_info_index()
	{
		for (unordered_map<long long, tree_info*>::iterator iter = tree_index.begin(); iter != tree_index.end(); iter++)
		{
			tree_info* cur = iter->second;
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
	void add_node(RPQ_tree* tree_pt, long long v, long long root_ID) // add a normal tree pointer + vertex ID to the reverse index
	{
		if (info_map.find(v2t_unit(v, root_ID)) != info_map.end())	// if the combination is already stored.
			return;
		unordered_map<long long, tree_info*>::iterator iter = tree_index.find(v);
		if (iter == tree_index.end())
		{
			tree_info* cur = new tree_info(tree_pt); //if there is no tree list before, add a new one
			tree_index[v] = cur;
			info_map[v2t_unit(v, root_ID)] = cur; // add this unit to the info_map
		}
		else
		{
			tree_info* cur = new tree_info(tree_pt);// If there is a tree lits, add the new tree_info to the head
			cur->next = iter->second;
			iter->second->prev = cur;
			iter->second = cur;
			info_map[v2t_unit(v, root_ID)] = cur;
		}
	}

	void delete_node(long long v, long long root_ID) // delete a tree info unit given the vertex ID -tree root pair, used when a node is deleted from a normal tree.
	{
		if (info_map.find(v2t_unit(v, root_ID)) != info_map.end()) // use the info_map to find the unit without scanning the list. 
		{
			map<v2t_unit, tree_info*>::iterator iter = info_map.find(v2t_unit(v, root_ID));
			tree_info* cur = iter->second;
			if (cur->prev) // if this unit is not the head of the tree list 
			{
				cur->prev->next = cur->next;
				if (cur->next)
					cur->next->prev = cur->prev;
			}
			else
			{
				unordered_map<long long, tree_info*>::iterator iter = tree_index.find(v); // else we need to change the head to the next
				if (iter != tree_index.end())
				{
					assert(iter->second == cur);
					if (cur->next)
					{
						iter->second = cur->next;
						cur->next->prev = NULL;
					}
					else {
						tree_index.erase(iter); // if the tree list is empty;
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
	unordered_map<long long, tree_info*> tree_index;
	map<v2l_unit, tree_info*> info_map;
	lm_info_index() {}
	~lm_info_index()
	{
		for (unordered_map<long long, tree_info*>::iterator iter = tree_index.begin(); iter != tree_index.end(); iter++)
		{
			tree_info* cur = iter->second;
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
	void add_node(RPQ_tree* tree_pt, long long v, long long root_ID, long long root_state)
	{
		if (info_map.find(v2l_unit(v, root_ID, root_state)) != info_map.end())
			return;
		unordered_map<long long, tree_info*>::iterator iter = tree_index.find(v);
		if (iter == tree_index.end())
		{
			tree_info* cur = new tree_info(tree_pt);
			tree_index[v] = cur;
			info_map[v2l_unit(v, root_ID, root_state)] = cur;
		}
		else
		{
			tree_info* cur = new tree_info(tree_pt);
			cur->next = iter->second;
			iter->second->prev = cur;
			iter->second = cur;
			info_map[v2l_unit(v, root_ID, root_state)] = cur;
		}
	}

	void delete_node(long long v, long long root_ID, long long root_state)
	{
		if (info_map.find(v2l_unit(v, root_ID, root_state)) != info_map.end())
		{
			map<v2l_unit, tree_info*>::iterator iter = info_map.find(v2l_unit(v, root_ID, root_state));
			tree_info* cur = iter->second;
			if (cur->prev)
			{
				cur->prev->next = cur->next;
				if (cur->next)
					cur->next->prev = cur->prev;
			}
			else
			{
				unordered_map<long long, tree_info*>::iterator iter = tree_index.find(v);
				if (iter != tree_index.end())
				{
					assert(iter->second == cur);
					if (cur->next)
					{
						iter->second = cur->next;
						cur->next->prev = NULL;
					}
					else {
						tree_index.erase(iter);
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
	long long ID;
	long long state;
	double score;
	vertex_score(long long v, long long state_, double s)
	{
		ID = v;
		state = state_;
		score = s;
	}
};

bool my_compare(const vertex_score& v1, const vertex_score& v2)
{
	return v1.score < v2.score;
}

bool operator<(const vertex_score& v1, const vertex_score& v2)
{
	return v1.score < v2.score;
}


bool operator>(const vertex_score& v1, const vertex_score& v2)
{
	return v1.score > v2.score;
}

struct pg_edge
{
	long long src;
	long long src_state;
	long long dst;
	long long dst_state;
	long long timestamp;
	pg_edge* src_prev;
	pg_edge* src_next;	// cross list, maintaining the graph structure
	pg_edge* dst_next;
	pg_edge* dst_prev;
	pg_edge(long long src_id_ = 0, long long src_state_ = 0, long long dst_id_ = 0, long long dst_state_ = 0, long long timestamp_ = 0)
	{
		src = src_id_;
		dst = dst_id_;
		src_state = src_state_;
		dst_state = dst_state_;
		timestamp = timestamp_;
		src_next = NULL;
		src_prev = NULL;
		dst_next = NULL;
		dst_prev = NULL;
	}
};

struct pg_node
{
	long long state;
	long long id;
	pg_edge* src_list;
	pg_edge* dst_list;
	pg_node(long long id_ =0 , long long state_ = 0)
	{
		id = id_;
		state = state_;
		src_list = NULL;
		dst_list = NULL;
	}
};
class product_graph
{
public:
	unordered_map<unsigned long long, pg_node> g;
	long long edge_cnt = 0;
	product_graph()
	{
		g.clear();
	}
	~product_graph()
	{
		for (unordered_map<unsigned long long, pg_node>::iterator iter = g.begin(); iter != g.end(); iter++)
		{
			pg_edge* tmp = iter->second.src_list;
			while (tmp)
			{
				pg_edge* cur = tmp;
				tmp = tmp->src_next;
				delete cur;
			}
			g.clear();
		}
	}
	void insert_edge(long long src, long long src_state, long long dst, long long dst_state, long long timestamp)
	{
		unsigned long long src_info = merge_long_long(src, src_state);
		unsigned long long dst_info = merge_long_long(dst, dst_state);
		if (g.find(src_info) != g.end())
		{
			pg_edge* tmp = g[src_info].src_list;
			while (tmp)
			{
				if (tmp->dst == dst && tmp->dst_state == dst_state)
				{
					tmp->timestamp = timestamp;
					return;
				}
				tmp = tmp->src_next;
			}
		}
		else {
			g[src_info].id = src;
			g[src_info].state = src_state;
		}

		edge_cnt++;
		pg_edge* tmp = new pg_edge(src, src_state, dst, dst_state, timestamp);
		tmp->src_next = g[src_info].src_list;
		if (g[src_info].src_list)
			g[src_info].src_list->src_prev = tmp;
		g[src_info].src_list = tmp;
		if (g.find(dst_info) != g.end())
		{
			tmp->dst_next = g[dst_info].dst_list;
			if (g[dst_info].dst_list)
				g[dst_info].dst_list->dst_prev = tmp;
			g[dst_info].dst_list = tmp;
		}
		else
		{
			g[dst_info].id = dst;
			g[dst_info].state = dst_state;
			g[dst_info].dst_list = tmp;
		}
	}
	bool expire_edge(long long src, long long src_state, long long dst, long long dst_state, long long expired_time)
	{
		unsigned long long src_info = merge_long_long(src, src_state);
		unsigned long long dst_info = merge_long_long(dst, dst_state);
		if (g.find(src_info) != g.end())
		{
			pg_edge* tmp = g[src_info].src_list;
			while (tmp)
			{
				if(tmp->dst==dst&&tmp->dst_state==dst_state)
				{
					if (tmp->timestamp < expired_time)
					{
						if (g[src_info].src_list == tmp)
						{
							if (tmp->src_next) {
								tmp->src_next->src_prev = NULL;
								g[src_info].src_list = tmp->src_next;
							}
							else
							{
								g[src_info].src_list = NULL;
								if (!g[src_info].dst_list)
									g.erase(src_info);
							}
						}
						else
						{
							if(tmp->src_prev)
								tmp->src_prev->src_next = tmp->src_next;
							if (tmp->src_next)
								tmp->src_next->src_prev = tmp->src_prev;
						}
						assert(g.find(dst_info) != g.end());
						if (g[dst_info].dst_list == tmp)
						{
							if (tmp->dst_next) {
								tmp->dst_next->dst_prev = NULL;
								g[dst_info].dst_list = tmp->dst_next;
							}
							else
							{
								g[dst_info].dst_list = NULL;
								if (!g[dst_info].src_list)
									g.erase(dst_info);
							}
						}
						else
						{
							if (tmp->dst_prev)
								tmp->dst_prev->dst_next = tmp->dst_next;
							if (tmp->dst_next)
								tmp->dst_next->dst_prev = tmp->dst_prev;
						}
						delete tmp;
						edge_cnt--;
						return true;
					}
					return false;
				}
				tmp = tmp->src_next;
			}
			return false;
		}
		return false;
	}

	void get_successor(long long src, long long src_state, vector < pair<unsigned long long, long long> >& suc)
	{
		unsigned long long src_info = merge_long_long(src, src_state);
		if (g.find(src_info) != g.end())
		{
			pg_edge* tmp = g[src_info].src_list;
			while (tmp)
			{
				suc.push_back(make_pair(merge_long_long(tmp->dst, tmp->dst_state), tmp->timestamp));
				tmp = tmp->src_next;
			}
		}
	}

	void get_precursor(long long dst, long long dst_state, vector < pair<unsigned long long, long long> >& pre)
	{
		unsigned long long dst_info = merge_long_long(dst, dst_state);
		if (g.find(dst_info) != g.end())
		{
			pg_edge* tmp = g[dst_info].dst_list;
			while (tmp)
			{
				pre.push_back(make_pair(merge_long_long(tmp->src, tmp->src_state), tmp->timestamp));
				tmp = tmp->dst_next;
			}
		}
	}
	
};

struct time_compare
{
	bool operator()(tree_node* &t1, tree_node* &t2)
	{
		return t1->timestamp<t2->timestamp;
	}
};

struct pair_compare
{
	bool operator()(pair<unsigned long long, long long>& p1, pair<unsigned long long, long long> &p2)
	{
		return p1.second<p2.second;
	}
};
