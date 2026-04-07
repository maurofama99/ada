#pragma once
#include<iostream>
#include<fstream>
#include<map>
#include<unordered_map>
#include<unordered_set>
#include<string>
#include <queue>
#include <algorithm>
#include <climits>
#include "forest_struct.h"
#include "fsa.h"
#include "sink.h"
#include "streaming_graph.h"
#define merge_long_long(s, d) (((unsigned long long)s<<32)|d)
using namespace std;

class LM_SRPQ {
public:
    FiniteStateAutomaton &aut;
    streaming_graph &g;
    Sink &sink;

    map<unsigned int, double> aut_scores; // this map stores the depth we estimated for each state in the DFA
    unordered_map<unsigned long long, RPQ_tree *> forests; // unordered map that maps each vertex ID-state pair to the spanning tree rooted at it. the vertex ID and the state is merged into an Unsigned long long
    map<unsigned int, tree_info_index *> v2t_index; // Maps each state to a tree_info_index, reverse index that maps a graph vertex to the normal trees that contains it.
    map<unsigned int, lm_info_index *> v2l_index; // Maps each state to a lm_info_index, reverse index that maps a graph vertex to the LM trees that contains it.
    unordered_set<unsigned long long> landmarks; // set of landmarks, vertex ID and states are merged.

    LM_SRPQ(FiniteStateAutomaton &aut, streaming_graph &g, Sink &sink)
        : aut(aut), g(g), sink(sink) {
        for (int i=0; i < aut.states_count; i++) aut_scores[i] = aut.scores[i];
    }

    ~LM_SRPQ() {
        for (auto &[fst, snd] : forests) delete snd;
        forests.clear();
        for (auto &[fst, snd] : v2t_index) delete snd;
        v2t_index.clear();
        for (auto &[fst, snd] : v2l_index) delete snd;
        v2l_index.clear();

        landmarks.clear();
        aut_scores.clear();
    }

    void expire_forest(long long eviction_time, const std::vector<streaming_graph::expired_edge_info>& deleted_edges) //given current time, carry out an expiration in the forest.
    {
        unordered_set<unsigned long long> visited_pair;
        for (const auto & deleted_edge : deleted_edges) {
            unsigned int dst = deleted_edge.dst;
            long long label = deleted_edge.label;
            std::vector<std::pair<long long, long long> > vec = aut.getStatePairsWithTransition(label);
            // dst node of the expired edge may be root of expired subtrees, get all the possible dst states/
            for (auto &[fst, snd] : vec) {
                long long dst_state = snd;
                if (dst_state == -1)
                    continue;
                if (visited_pair.find(merge_long_long(dst, dst_state)) != visited_pair.end())
                    // some dst nodes may be checked before, and need not to be checked again.
                    continue;
                visited_pair.insert(merge_long_long(dst, dst_state));
                if (auto iter = v2t_index.find(dst_state); iter != v2t_index.end()) {
                    if (auto tree_iter = iter->second->tree_index.find(dst); tree_iter != iter->second->tree_index.end()) {
                        vector<RPQ_tree *> tree_to_delete;
                        tree_info *tmp = tree_iter->second;
                        while (tmp) {
                            tree_to_delete.push_back(tmp->tree);
                            // we first record the tree list and then check them one by one, as the deletion may change the tree list.
                            tmp = tmp->next;
                        }
                        for (auto & k : tree_to_delete) {
                            expire_per_tree(dst, dst_state, k, eviction_time);
                            if (k->root->child == nullptr) {
                                delete_index(k->root->node_ID, k->root->state,k->root->node_ID);
                                forests.erase(merge_long_long(k->root->node_ID,k->root->state));
                                delete k;
                            }
                        }
                        shrink(forests);
                        tree_to_delete.clear();
                    }
                }
                //expire in LM trees.
                if (auto iter2 = v2l_index.find(dst_state); iter2 != v2l_index.end()) {
                    if (auto tree_iter = iter2->second->tree_index.find(dst); tree_iter != iter2->second->tree_index.end()) {
                        vector<RPQ_tree *> tree_to_delete;
                        tree_info *tmp = tree_iter->second;
                        while (tmp) {
                            tree_to_delete.push_back(tmp->tree);
                            tmp = tmp->next;
                        }
                        for (auto & k : tree_to_delete) {
                            expire_per_lm_tree(dst, dst_state, k, eviction_time);
                            if (k->root->child == nullptr) {
                                delete_lm_index(k->root->node_ID, k->root->state,k->root->node_ID, k->root->state);
                                forests.erase(merge_long_long(k->root->node_ID,k->root->state));
                                if (landmarks.find(merge_long_long(k->root->node_ID,k->root->state)) != landmarks.end()) {
                                    landmarks.erase(merge_long_long(k->root->node_ID,k->root->state));
                                    recover_subtree(k->root->node_ID, k->root->state,nullptr);
                                    recover_subtree_lm(k->root->node_ID, k->root->state,nullptr);
                                }

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

    void insert_edge(unsigned int s, unsigned int d, unsigned int label, long long timestamp) //  a new edge is inserted.
    {
        if (aut.getNextState(0, label) != -1 && forests.find(merge_long_long(s, 0)) == forests.end())
        // we need to build a new tree
        {
            auto *new_tree = new RPQ_tree();
            if (landmarks.find(merge_long_long(s, 0)) == landmarks.end()) // a normal tree
                new_tree->root = add_node(new_tree, s, 0, s, nullptr, MAX_INT, MAX_INT);
            else {
                // an LM tree
                new_tree->root = add_lm_node(new_tree, s, 0, s, 0, nullptr, MAX_INT, MAX_INT);
                new_tree->add_time_info(s, 0, MAX_INT);
            }
            forests[merge_long_long(s, 0)] = new_tree;
        }
        vector<pair<long long, long long> > vec = aut.getStatePairsWithTransition(label); // find all the state paris that can accept this label
        for (auto &[fst, snd] : vec) {
            unordered_map<unsigned long long, vector<pair<unsigned int, unsigned int> > > lm_results;
            unsigned int src_state = fst;
            unsigned int dst_state = snd;
            if (landmarks.find(merge_long_long(s, src_state)) != landmarks.end()) // if (s, src_state) is a landmark
            {
                RPQ_tree *tree_pt = forests[merge_long_long(s, src_state)];
                insert_edge_lm_tree(s, d, label, timestamp, src_state, dst_state, tree_pt, lm_results);
                // update the lm tree, backtrack is also called, and the lm trees we find in backtrack and the updated reachable nodes of them are in lm_results.
                unordered_set<unsigned int> visited;
                // find the trees with the landmark (s, src_state) and update them. such update is a part of the backtrack. We will not expand the normal but only update the result set.
                if (auto index_iter = v2t_index.find(src_state); index_iter != v2t_index.end()) {
                    if (auto tree_iter = index_iter->second->tree_index.find(s); tree_iter != index_iter->second->tree_index.end()) {
                        tree_info *tmp = tree_iter->second;
                        while (tmp) {
                            visit_non_lm_tree(s, d, label, timestamp, src_state, dst_state, tmp->tree, lm_results,visited);
                            tmp = tmp->next;
                        }
                    }
                }
                for (auto & lm_result : lm_results) {
                    unsigned int lm_ID = (lm_result.first >> 32);
                    unsigned int lm_state = (lm_result.first & 0xFFFFFFFFF);
                    // find the normal trees containing other landmarks we find in backtrack and update them.
                    if (auto iterator = v2t_index.find(lm_state); iterator != v2t_index.end()) {
                        if (auto tree_iter = iterator->second->tree_index.find(lm_ID); tree_iter != iterator->second->tree_index.end()) {
                            tree_info *tmp = tree_iter->second;
                            while (tmp) {
                                visit_non_lm_tree(s, d, label, timestamp, src_state, dst_state, tmp->tree, lm_results, visited);
                                tmp = tmp->next;
                            }
                        }
                    }
                    lm_result.second.clear();
                }
                visited.clear();
            } else {
                // if (s, src_state) is not a landmark, we need to first update all the LM trees contianing it, and backtrack from them.
                if (auto index_iter = v2l_index.find(src_state); index_iter != v2l_index.end()) {
                    if (auto tree_iter = index_iter->second->tree_index.find(s); tree_iter != index_iter->second->tree_index.end()) {
                        tree_info *tmp = tree_iter->second;
                        while (tmp) {
                            insert_edge_lm_tree(s, d, label, timestamp, src_state, dst_state, tmp->tree, lm_results);
                            tmp = tmp->next;
                        }
                    }
                }
                unordered_set<unsigned int> visited;
                if (auto index_iter2 = v2t_index.find(src_state); index_iter2 != v2t_index.end()) {
                    if (auto tree_iter = index_iter2->second->tree_index.find(s); tree_iter != index_iter2->second->tree_index.end()) {
                        tree_info *tmp = tree_iter->second;
                        while (tmp) {
                            visit_non_lm_tree(s, d, label, timestamp, src_state, dst_state, tmp->tree, lm_results,visited);
                            tmp = tmp->next;
                        }
                    }
                }

                for (auto iter = lm_results.begin(); iter != lm_results.end(); iter++) {
                    unsigned int lm_ID = (iter->first >> 32);
                    unsigned int lm_state = (iter->first & 0xFFFFFFFFF);
                    // at last we find the normal trees containing landmarks that we found in backtrack. These normal trees are endpoint of bachtrack branches, we
                    // update them together at last so that we ensure every LM tree has been updated, and we can directly use their time info maps safely in the normal tree update.
                    if (auto index_iter = v2t_index.find(lm_state); index_iter != v2t_index.end()) {
                        if (auto tree_iter = index_iter->second->tree_index.find(lm_ID); tree_iter != index_iter->second->tree_index.end()) {
                            tree_info *tmp = tree_iter->second;
                            while (tmp) {
                                visit_non_lm_tree(s, d, label, timestamp, src_state, dst_state, tmp->tree, lm_results,visited);
                                tmp = tmp->next;
                            }
                        }
                    }
                    iter->second.clear();
                }
                visited.clear();
            }
            lm_results.clear();
        }
    }

    void dynamic_lm_select(double candidate_rate, double benefit_threshold)
    // the function to select landmarks, first parameter is the candidate selection rate, usually 0.2, the second is the benefit threshold, usually 1.5
    {
        vector<vertex_score> scores;
        unordered_map<unsigned long long, unsigned int> score_map; //store scores of nodes

        for (auto iter = g.adjacency_list.begin(); iter != g.adjacency_list.end(); ++iter) {
        //for (unordered_map<unsigned int, neighbor_list>::iterator iter = g.g.begin(); iter != g.g.end(); iter++) {
            unsigned int degree_sum = 0;
            unsigned int id = iter->first;
            for (auto &[fst, snd] : aut.transitions) {
                unsigned int state = fst;
                unsigned long long info = merge_long_long(id, state);
                if (count_presence(id, state, 2)) {
                    // filter out product graph nodes which appear in less than 2 trees.
                    map<unsigned int, unsigned int> degree_map = g.get_degree_map(id);
                    degree_sum = 0;
                    auto &tmp = snd;
                    // only include the edges with acceptable labels in degree counting.
                    for (auto transition: tmp) {
                        if (degree_map.find(transition.label) != degree_map.end()) {
                            degree_sum += degree_map[transition.label];
                        }
                    }
                    if (degree_sum > 0) {
                        // aut_scores record the approximated depth of the tree, computed based on the state.
                        if (double score = degree_sum * aut_scores[state]; score > 1) {
                            scores.emplace_back(id, state, score);
                            score_map[merge_long_long(id, state)] = static_cast<int>(score);
                        }
                    }
                }
            }
        }
        if (scores.empty())
            return;
        sort(scores.begin(), scores.end()); // socrt the scores.


        auto num = static_cast<unsigned long>(scores.size() * candidate_rate);
        double bar = scores[scores.size() - num].score;
        // nodes with score smaller than this bar is not in the candidate set.

        for (auto it = landmarks.begin(); it != landmarks.end();)
        // check current landmarks
        {
            unsigned long long info = *it;
            unsigned int v = (info >> 32);
            unsigned int state = (info & 0xFFFFFFFF);
            RPQ_tree *tree_pt = nullptr;
            if (forests.find(info) != forests.end())
            // if the LM tree of this landmark is already deleted because it becomes empty in expiration, this landmark need to be deleted from the landmark set.
                tree_pt = forests[info];
            else {
                it = landmarks.erase(it);
                continue;
            }

            if (score_map.find(info) == score_map.end() || score_map[info] < bar) // if it is not a candidate any more.
            {
                it = landmarks.erase(it);
                recover_subtree(v, state, tree_pt);
                recover_subtree_lm(v, state, tree_pt); // rcover the subtrees
                if (state == 0) {
                    // if the state is 0, we need to transform the LM tree back to a normal tree.
                    tree_pt->clear_time_info(); // delete the time info map
                    switch_tree_index_reverse(tree_pt); // swith the reverse index of the nodes in it from v2h to v2l
                } else // else we need to delete this tree.
                {
                    delete_v2h_index(tree_pt);
                    delete tree_pt;
                    forests.erase(info);
                }
                continue;
            } else if (state != 0)
            // if the state is 0, there should be a delta tree with (v state) any way. In this case, as long as there is another subtree with (v, state), selecting it will bring decrease to the forests, thus we donot need to check
            {
                double node_budget = tree_pt->node_cnt * benefit_threshold;
                // the number of omitedd nodes in the subtrees need to be larger than this threshold.
                node_budget = recover_subtree_lm_preview(v, state, tree_pt, node_budget);
                if (node_budget > 0)
                    node_budget = recover_subtree_preview(v, state, tree_pt, node_budget);
                if (node_budget > 0) // if not, delete it from the landmark
                {
                    it = landmarks.erase(it);
                    recover_subtree(v, state, tree_pt);
                    recover_subtree_lm(v, state, tree_pt);
                    delete_v2h_index(tree_pt);
                    delete tree_pt;
                    forests.erase(info);
                } else it++;
            } else it++;
        }

        shrink(landmarks);

        for (int i = scores.size() - 1; i >= scores.size() - num; i--) // scan candidates
        {
            unsigned int v = scores[i].ID;
            unsigned int state = scores[i].state;
            unsigned long long info = merge_long_long(v, state);
            if (landmarks.find(info) != landmarks.end()) // skip a node if it is already a landmark
                continue;

            RPQ_tree *tree_pt = nullptr;
            if (forests.find(info) != forests.end()) {
                // if there is already a normal tree for it, it mush have state 0, and should be selected as a landmark.
                tree_pt = forests[info];
                generate_time_info(tree_pt);
                switch_tree_index(tree_pt);
                retrieve_subtree(v, state); // delete the subtree in normal trees
                unordered_set<unsigned long long> necessary_nodes;
                retrieve_subtree_lm(v, state, tree_pt, necessary_nodes);
                // delete subtree in LM trees, and necessary nodes which may be missing in the LM tree building.
                fulfill_new_lm_tree(tree_pt, necessary_nodes); // fulfill the LM tree with necessary nodes.
                necessary_nodes.clear();
                landmarks.insert(info);
            } else // else we need to trade off the benefit and cost
            {
                tree_pt = build_lm_tree(v, state); // we build the LM tree first.
                unsigned int node_cost = tree_pt->node_cnt; // cost is the LM tree size
                unsigned int node_benefit = retrieve_subtree_lm_preview(v, state);
                node_benefit += retrieve_subtree_preview(v, state); // benefit is the size of subtrees.
                if (node_benefit < node_cost * benefit_threshold)
                // if the benefit is not enough, delete the LM tree we just build
                    delete tree_pt;
                else // this candidate is a landmark
                {
                    forests[info] = tree_pt; // add the LM tree into forest.
                    build_v2l_index(tree_pt); // build reverse index
                    retrieve_subtree(v, state);
                    unordered_set<unsigned long long> necessary_nodes;
                    retrieve_subtree_lm(v, state, tree_pt, necessary_nodes);
                    // delete subtrees and collect necessary nodes
                    fulfill_new_lm_tree(tree_pt, necessary_nodes); // fulfill the LM tree
                    necessary_nodes.clear();
                    landmarks.insert(info);
                }
            }
        }
    }

private:

    void update_result(unordered_map<unsigned int, unsigned int> &updated_nodes, unsigned int root_ID, unsigned int lm_time = MAX_INT)
    // first of updated nodes are vertex ID, second are path timestamps, root_ID can reach these vertices with a qualified regular path.
    // when lm time is set, updated_nodes store path timestamps from a landmark to the vertices, lm time is the path timestamp from root to the landmark, we need to first merge these two parts and then update the result set.
    {
        for (auto it = updated_nodes.begin(); it != updated_nodes.end(); it++) {
            unsigned int dst = it->first;
            unsigned int time = min(it->second, lm_time);
            // if (dst == root_ID) continue;
            sink.addEntry(root_ID, dst, time);
        }
    }

    void update_result(vector<pair<unsigned int, unsigned int> > &updated_nodes, unsigned int root_ID, unsigned int lm_time = MAX_INT) {
        for (auto &updated_node: updated_nodes) {
            unsigned int dst = updated_node.first;
            unsigned int time = min(updated_node.second, lm_time);
            // if (dst == root_ID) continue;
            sink.addEntry(root_ID, dst, time);
        }
    }


    void add_index(RPQ_tree *tree_pt, unsigned int v, unsigned int state, unsigned int root_ID)
    // modify the reverse index when a node is added into a normal tree;
    {
        auto iter = v2t_index.find(state);
        if (iter == v2t_index.end())
            v2t_index[state] = new tree_info_index;
        v2t_index[state]->add_node(tree_pt, v, root_ID);
    }

    void add_lm_index(RPQ_tree *tree_pt, unsigned int v, unsigned int state, unsigned int root_ID,
                      unsigned int root_state) // modify the reverse index when a node is added into an LM tree;
    {
        auto iter = v2l_index.find(state);
        if (iter == v2l_index.end())
            v2l_index[state] = new lm_info_index;
        v2l_index[state]->add_node(tree_pt, v, root_ID, root_state);
    }

    tree_node *add_node(RPQ_tree *tree_pt, unsigned int v, unsigned int state, unsigned int root_ID, tree_node *parent,
                        unsigned int timestamp, unsigned int edge_time,
                        bool lm = false) // add a node to a normal tree, bool lm indicating if this node is a landmark.
    {
        add_index(tree_pt, v, state, root_ID);
        tree_node *tmp = tree_pt->add_node(v, state, parent, timestamp, edge_time);
        tmp->lm = lm;
        return tmp;
    }


    tree_node *add_lm_node(RPQ_tree *lm_tree, unsigned int v, unsigned int state, unsigned int root_ID,
                           unsigned int root_state,
                           tree_node *parent, unsigned int timestamp, unsigned int edge_time,
                           bool lm = false) // add a node to the LM tree .
    {
        add_lm_index(lm_tree, v, state, root_ID, root_state);
        tree_node *tmp = lm_tree->add_node(v, state, parent, timestamp, edge_time);
        tmp->lm = lm;
        return tmp;
    }

    void delete_index(unsigned int v, unsigned int state, unsigned int root)
    // modify the reverse index when a node is deleted from a normal tree;
    {
        auto iter = v2t_index.find(state);
        if (iter != v2t_index.end()) {
            iter->second->delete_node(v, root);
            if (iter->second->tree_index.empty())
                v2t_index.erase(iter);
        }
    }

    void delete_lm_index(unsigned int v, unsigned int state, unsigned int root, unsigned int root_state)
    // modify the reverse index when a node is deleted from an LM tree;
    {
        auto iter = v2l_index.find(state);
        if (iter != v2l_index.end()) {
            iter->second->delete_node(v, root, root_state);
            if (iter->second->tree_index.empty())
                v2l_index.erase(iter);
        }
    }


    void lm_expand_in_lm_subtree(unsigned int lm, unsigned int state, RPQ_tree *root_lm_tree, unsigned int lm_time,
                                 unordered_map<unsigned long long, unsigned int> &updated_nodes)
    // this function is called when a landmark (lm, state) is added into another lm tree root_lm_tree, the timestamp of this node is lm_time, we scan the LM tree of this landmark and update the time_info map
    // in root_lm_tree, besides, we record the nodes where the timestamp in the time_info map of root_lm_tree is updated with updated_nodes, we need to update the result set in the upper layer with these nodes.
    {
        unsigned long long lm_info = merge_long_long(lm, state);
        if (forests.find(lm_info) == forests.end())
            return;
        RPQ_tree *lm_subtree = forests[lm_info]; // find the LM tree
        for (auto state_iter = lm_subtree->time_info.begin(); state_iter != lm_subtree->time_info.end(); state_iter++) {
            // scan its time info map
            time_info_index *subtree_index = state_iter->second;
            if (root_lm_tree->time_info.find(state_iter->first) == root_lm_tree->time_info.end())
                // if this state is not in the time info map of root_lm_tree yet, add a new lm_info_index
                root_lm_tree->time_info[state_iter->first] = new time_info_index;
            time_info_index *root_index = root_lm_tree->time_info[state_iter->first];
            for (auto iter = subtree_index->index.begin(); iter != subtree_index->index.end(); iter++) {
                unsigned int time = min(lm_time, iter->second);
                // compute the time of latest path from root of root_lm_tree to the node
                if (root_index->index.find(iter->first) == root_index->index.end()) {
                    root_index->index[iter->first] = time;
                    updated_nodes[merge_long_long(iter->first, state_iter->first)] = time;
                } else {
                    if (root_index->index[iter->first] < time)
                    // if the node is not in the time info map of root_lm_tree before or has a smaller timestamp, we need to update it.
                    {
                        root_index->index[iter->first] = time;
                        updated_nodes[merge_long_long(iter->first, state_iter->first)] = time;
                    }
                }
            }
        }
    }

    void lm_expand(tree_node *expand_node, RPQ_tree *lm_tree,
                   unordered_map<unsigned long long, unsigned int> &updated_nodes)
    // this function expand an LM tree given a new node in it, and record the nodes where the timestamp in the time_info map is updated with updated_nodes,
    {
        updated_nodes[merge_long_long(expand_node->node_ID, expand_node->state)] = expand_node->timestamp;
        lm_tree->add_time_info(expand_node->node_ID, expand_node->state, expand_node->timestamp);
        // first add the new node into the queue
        priority_queue<tree_node *, vector<tree_node *>, time_compare> q;
        q.push(expand_node);
        while (!q.empty()) {
            tree_node *tmp = q.top();
            q.pop();
            if (landmarks.find(merge_long_long(tmp->node_ID, tmp->state)) != landmarks.end()) {
                tmp->lm = true;
                lm_tree->add_lm(merge_long_long(tmp->node_ID, tmp->state));
                lm_expand_in_lm_subtree(tmp->node_ID, tmp->state, lm_tree, tmp->timestamp, updated_nodes);
                continue;
            }

            std::map<long long, long long> aut_edge = aut.getAllSuccessors(tmp->state);
            // get the edges acceptable to the src state
            //aut.get_all_suc(tmp->state, aut_edge); // get the edges acceptable to the src state
            vector<sg_edge *> sucs = g.get_all_suc_ptrs(tmp->node_ID); // get out edges of the src node
            for (auto &suc: sucs) {
                unsigned int successor = suc->d;
                unsigned int label = suc->label;
                unsigned int time = min(tmp->timestamp, (unsigned int)suc->timestamp);
                if (aut_edge.find(label) == aut_edge.end())
                    continue;
                long long dst_state = aut_edge[label];

                if (lm_tree->get_time_info(successor, dst_state) >= time)
                    // prune the branch if there is already a path with no smaller timestamp
                    continue;
                if (lm_tree->node_map.find(dst_state) == lm_tree->node_map.end() || lm_tree->node_map[dst_state]->index.
                    find(successor) == lm_tree->node_map[dst_state]->index.end()) // if this node does not exist yet.
                {
                    tree_node *new_node = add_lm_node(lm_tree, successor, dst_state, lm_tree->root->node_ID,
                                                      lm_tree->root->state, tmp, time, suc->timestamp);
                    lm_tree->add_time_info(successor, dst_state, time);
                    // add this new node and upadte the time info map
                    updated_nodes[merge_long_long(successor, dst_state)] = time;
                    q.push(new_node);
                } else {
                    tree_node *dst_pt = lm_tree->node_map[dst_state]->index[successor];
                    if (dst_pt->timestamp < time) // if the node exists but has a smaller timestamp
                    {
                        if (dst_pt->parent != tmp)
                            lm_tree->substitute_parent(tmp, dst_pt);
                        dst_pt->timestamp = time;
                        dst_pt->edge_timestamp = suc->timestamp;
                        lm_tree->add_time_info(successor, dst_state, time);
                        updated_nodes[merge_long_long(successor, dst_state)] = time;
                        q.push(dst_pt);
                    }
                }
            }
        }
    }

    void back_track_lm(unsigned int lm, unsigned int state, unsigned int src, unsigned int src_time, unsigned int dst,
                       unsigned int dst_time,
                       unsigned int label, unsigned int src_state, unsigned int dst_state,
                       unordered_map<unsigned long long, unsigned int> &updated_nodes,
                       unordered_map<unsigned long long, vector<pair<unsigned int, unsigned int> > > &lm_results)
    // this function performs the backward search from a landmark (lm, state), (src, src_state) and (dst, dst_state) are the src node and dst node of this update, src_time and dst_time are the timestamps of the latest paths from the landmark to these two nodes
    // updated_nodes record the nodes whose timestamp has been updated in the time info map of the landmark, lm_results record the final-state nodes to which the laste path timestamp has been updated for each updated LM tree, it is used in the following normal tree update.
    {
        auto it = v2l_index.find(state);
        if (it != v2l_index.end()) {
            auto iter = it->second->tree_index.find(lm); // find the list of LM trees containing this landmark.
            if (iter == it->second->tree_index.end())
                return;
            tree_info *cur = iter->second;
            while (cur) {
                RPQ_tree *tree_pt = cur->tree;
                unsigned int root_ID = tree_pt->root->node_ID;
                unsigned int root_state = tree_pt->root->state;
                unsigned long long root_info = merge_long_long(root_ID, root_state);
                if ((root_ID == lm && root_state == state) || lm_results.find(root_info) != lm_results.end())
                // skip this tree if it is the tree of the landmark itself, or if we have updated the LM tree.
                {
                    cur = cur->next;
                    continue;
                }
                assert(tree_pt->node_map.find(state) != tree_pt->node_map.end());
                assert(tree_pt->node_map[state]->index.find(lm) != tree_pt->node_map[state]->index.end());

                tree_node *lm_node = tree_pt->node_map[state]->index[lm];

                unordered_map<unsigned long long, unsigned int> tracked_nodes;
                unsigned int local_src_time = min(lm_node->timestamp, src_time);
                // timestamp of the path from the root of tree_pt to src node passing the landmark.
                unsigned int local_dst_time = min(lm_node->timestamp, dst_time);
                // timestamp of the path from the root of tree_pt to dst node passing the landmark.
                unsigned long long src_info = merge_long_long(src, src_state);
                unsigned long long dst_info = merge_long_long(dst, dst_state);
                if (tree_pt->get_time_info(src, src_state) > local_src_time)
                // if this is not the latest path to the src node, prune this backtrack
                {
                    cur = cur->next;
                    continue;
                }
                if (tree_pt->get_time_info(dst, dst_state) >= local_dst_time) {
                    // if there is an existing path to dst node with no smaller timestamp, prune this backtrack.
                    cur = cur->next;
                    continue;
                }
                for (auto iterator = updated_nodes.begin();
                     iterator != updated_nodes.end(); iterator++) {
                    unsigned int time = min(lm_node->timestamp, iterator->second);
                    if (tree_pt->get_time_info((iterator->first >> 32), (iterator->first & 0xFFFFFFFF)) >= time)
                        // if the time info of the node iter->first can not be updated, continue to next node
                        continue;
                    tree_pt->add_time_info((iterator->first >> 32), (iterator->first & 0xFFFFFFFF), time);
                    // update the time info.
                    tracked_nodes.insert(make_pair(iterator->first, time));
                    // record this node with another vector, and use it in further backtrack
                    if (iterator->first > LLONG_MAX) {
                        cout << "error in backtrack_lm: node ID exceeds the limit of long long" << endl;
                        exit(-1);
                    }
                    if (aut.isFinalState(iterator->first & 0xFFFFFFFF)) {
                        // if this is a final state node, record it in lm_result.
                        if (lm_results.find(root_info) == lm_results.end())
                            lm_results[root_info] = vector<pair<unsigned int, unsigned int> >();
                        lm_results[root_info].emplace_back((iterator->first >> 32), time);
                    }
                }
                if (!tracked_nodes.empty()) {
                    if (root_state == 0) // if this tree has a initial state root, update the result set.
                        update_result(lm_results[root_info], tree_pt->root->node_ID);
                    back_track_lm(root_ID, root_state, src, local_src_time, dst, local_dst_time, label, src_state,
                                  dst_state, tracked_nodes, lm_results);
                }
                tracked_nodes.clear();
                cur = cur->next;
            }
        }
    }


    void insert_edge_lm_tree(unsigned int s, unsigned int d, unsigned int label, unsigned int timestamp, unsigned int src_state,
                             unsigned int dst_state, RPQ_tree *lm_tree,
                             unordered_map<unsigned long long, vector<pair<unsigned int, unsigned int> > > &lm_results)
    // insert a new edge (s, src_state) (d, dst_state) with label and timestamp in an LM tree,  record the final-state nodes to which the laste path timestamp has been updated in lm_results.
    {
        unsigned int root_ID = lm_tree->root->node_ID;
        unsigned int root_state = lm_tree->root->state;
        unsigned long long root_info = merge_long_long(root_ID, root_state);
        if (lm_results.find(merge_long_long(root_ID, root_state)) != lm_results.end()) return;
        unordered_map<unsigned long long, unsigned int> updated_nodes;
        assert(lm_tree->node_map.find(src_state) != lm_tree->node_map.end());
        assert(lm_tree->node_map[src_state]->index.find(s) != lm_tree->node_map[src_state]->index.end());
        tree_node *src_pt = lm_tree->node_map[src_state]->index[s];
        merge_long_long(s, src_state);
        merge_long_long(d, dst_state);
        if (src_pt->timestamp < lm_tree->get_time_info(s, src_state))
            // if the local path is not the latest, prune this update.
            return;
        if (lm_tree->get_time_info(d, dst_state) >= min(src_pt->timestamp, timestamp))
            // if there is an exisiting path to dst with no smaller timestamp, prune this update.
            return;

        tree_node *dst_pt = nullptr;
        if (lm_tree->node_map.find(dst_state) == lm_tree->node_map.end() || lm_tree->node_map[dst_state]->index.find(d)
            == lm_tree->node_map[dst_state]->index.end()) // add the dst node if it is not in the tree yet.
            dst_pt = add_lm_node(lm_tree, d, dst_state, lm_tree->root->node_ID, lm_tree->root->state, src_pt,
                                 min(src_pt->timestamp, timestamp), timestamp);
        else {
            // else the new timestamp must be larger than the existing timestamp of dst node in this tree, otherwise we should have returned in the above check.
            dst_pt = lm_tree->node_map[dst_state]->index[d];
            if (dst_pt->timestamp < min(src_pt->timestamp, timestamp)) {
                if (dst_pt->parent != src_pt)
                    lm_tree->substitute_parent(src_pt, dst_pt);
                dst_pt->timestamp = min(src_pt->timestamp, timestamp);
                dst_pt->edge_timestamp = timestamp;
            }
        }
        lm_expand(dst_pt, lm_tree, updated_nodes); // expand from the dst node

        lm_results[root_info] = vector<pair<unsigned int, unsigned int> >();
        // we pick the final state nodes from updated_nodes and record them in lm_results.
        for (auto &updated_node: updated_nodes) {
            unsigned int node_ID = (updated_node.first >> 32);
            unsigned int state = (updated_node.first & 0xFFFFFFFF);
            if (aut.isFinalState(state))
                lm_results[root_info].emplace_back(node_ID, updated_node.second);
        }
        if (root_state == 0) // if the root has initial state, we need to update the result set.
            update_result(lm_results[root_info], lm_tree->root->node_ID);
        back_track_lm(root_ID, root_state, s, src_pt->timestamp, d, dst_pt->timestamp, label, src_state, dst_state,
                      updated_nodes, lm_results); // backtrack from this updated LM tree.
        updated_nodes.clear();
    }


    void get_lm_results(unsigned int lm, unsigned int state, unsigned int lm_time,
                        unordered_map<unsigned int, unsigned int> &updated_results) {
        // this function is called when a landmark is added into a normal tree, we compute timestamp of new paths passing this landmark and record their timestamps in updated_results.
        // lm_time is the time of landmark in a normal tree, we collect all the final state nodes in the time info map of (lm, state) and merge their timestamp with lm_time to get the timestamp of path from the
        // normal tree root to the final state node passing the landmark.
        unsigned long long lm_info = merge_long_long(lm, state);
        if (forests.find(lm_info) == forests.end())
            return;
        RPQ_tree *lm_tree = forests[lm_info];
        for (long long final_state: aut.finalStates) {
            if (lm_tree->time_info.find(final_state) != lm_tree->time_info.end()) {
                for (auto iter = lm_tree->time_info[final_state]->index.
                             begin(); iter != lm_tree->time_info[final_state]->index.end(); iter++) {
                    unsigned int v = iter->first;
                    unsigned int time = min(lm_time, iter->second);
                    if (updated_results.find(v) != updated_results.end())
                        updated_results[v] = max(updated_results[v], time);
                    else
                        updated_results[v] = time;
                }
            }
        }
    }

    void non_lm_expand(tree_node *expand_node, RPQ_tree *tree_pt)
    // this function is used to expand a normal tree given a new node expand_node;
    {
        unordered_map<unsigned int, unsigned int> updated_results;
        // this map records timestamps of final-state nodes to which the timestamp of latest path has been updated
        priority_queue<tree_node *, vector<tree_node *>, time_compare> q;
        q.push(expand_node);
        while (!q.empty()) {
            tree_node *tmp = q.top();
            q.pop();
            unsigned long long tmp_info = merge_long_long(tmp->node_ID, tmp->state);
            if (aut.isFinalState(tmp->state)) {
                if (updated_results.find(tmp->node_ID) != updated_results.end())
                    updated_results[tmp->node_ID] = max(updated_results[tmp->node_ID], tmp->timestamp);
                else
                    updated_results[tmp->node_ID] = tmp->timestamp;
            }
            if (landmarks.find(tmp_info) != landmarks.end()) {
                tmp->lm = true;
                tree_pt->add_lm(tmp_info);
                get_lm_results(tmp->node_ID, tmp->state, tmp->timestamp, updated_results);
                // if this node is a landmark, we directly get the timestamp of its successors from the time info map.
                continue;
            }
            vector<sg_edge*> vec = g.get_all_suc_ptrs(tmp->node_ID); // get all the out edge of the src node
            for (auto & i : vec) {
                unsigned int successor = i->d;
                unsigned int edge_label = i->label;
                long long dst_state = aut.getNextState(tmp->state, edge_label); // check if we can travel to a dst state
                if (dst_state == -1)
                    continue;
                unsigned int time = min(tmp->timestamp, (unsigned int)i->timestamp); // compute timestamp of the dst node
                if (tree_pt->node_map.find(dst_state) == tree_pt->node_map.end() || tree_pt->node_map[dst_state]->
                    index.find(successor) == tree_pt->node_map[dst_state]->index.end())
                // add dst node to the tree if it does not exist
                    q.push(add_node(tree_pt, successor, dst_state, tree_pt->root->node_ID, tmp, time,
                                    i->timestamp));
                else {
                    tree_node *dst_pt = tree_pt->node_map[dst_state]->index[successor];
                    if (dst_pt->timestamp < time) {
                        // if the timestamp of the new path is larger than the old node time, link dst node to the new path and update its timestamp
                        if (dst_pt->parent != tmp)
                            tree_pt->substitute_parent(tmp, dst_pt);
                        dst_pt->timestamp = time;
                        dst_pt->edge_timestamp = i->timestamp;
                        q.push(dst_pt);
                    }
                }
            }
        }
        update_result(updated_results, tree_pt->root->node_ID);
        // update the result set with the collected final-state nodes
        updated_results.clear();
    }

    void visit_non_lm_tree(unsigned int s, unsigned int d, unsigned int label, unsigned int timestamp, unsigned int src_state,
                           unsigned int dst_state,
                           RPQ_tree *tree_pt,
                           unordered_map<unsigned long long, vector<pair<unsigned int, unsigned int> > > &lm_results,
                           unordered_set<unsigned int> &visited) {
        // this function is used to update normal trees. normal trees are updated in 2 cases: it containing the src node of the new edge, or it is found in the backward search of a lm tree
        // we do not process normal trees in the back_track_lm fucntion as processing them is different from processing LM trees, and it is better to process LM trees after we processing all the LM
        // trees, so that we can directly update these LM trees according to the updated reachable nodes of the LM trees of landmarks in it.
        if (visited.find(tree_pt->root->node_ID) != visited.end())
            // we use a set to filter out the visited normal tree, as we may reach the same normal tree from different backward search branch.
            return;
        visited.insert(tree_pt->root->node_ID);

        unsigned int max_src_time = 0;
        unsigned long long max_src_lm = 0;
        unsigned int max_dst_time = 0;
        unsigned long long max_dst_lm = 0;

        for (auto iter = tree_pt->landmarks.begin();
             iter != tree_pt->landmarks.end(); iter++)
        //we scan the landmarks to check the timestamp of the path to src node and dst node passing them
        // and record the largest timestamp, which is the largest timestamp of paths to the src/ dst node passing landmarks.
        {
            if (unsigned long long lm_info = *iter; forests.find(lm_info) != forests.end()) {
                RPQ_tree *lm_tree = forests[lm_info];
                tree_node *lm_node = tree_pt->find_node((lm_info >> 32), (lm_info & 0xFFFFFFFF));
                unsigned int local_src_time = lm_tree->get_time_info(s, src_state);
                if (min(local_src_time, lm_node->timestamp) > max_src_time) {
                    max_src_time = min(local_src_time, lm_node->timestamp);
                    max_src_lm = lm_info;
                }
                if (unsigned int local_dst_time = lm_tree->get_time_info(d, dst_state); min(local_dst_time, lm_node->timestamp) > max_dst_time) {
                    max_dst_time = min(local_dst_time, lm_node->timestamp);
                    max_dst_lm = lm_info;
                }
            }
        }

        if (tree_pt->node_map.find(src_state) != tree_pt->node_map.end()) // if the normal tree contians the src node
        {
            if (tree_node_index *tmp_index = tree_pt->node_map[src_state]; tmp_index->index.find(s) != tmp_index->index.end()) {
                if (tree_node *src_pt = tmp_index->index[s]; !src_pt->lm && src_pt->timestamp > max_src_time && min(src_pt->timestamp, timestamp) > max_dst_time)
                // we expand this normal tree only if the local path has larger timestamp than the paths passing landmarks
                // and no exisiting path has larger, or equal timestamp than the new local path to the dst node
                {
                    unsigned int time = min(src_pt->timestamp, timestamp);
                    if (tree_pt->node_map.find(dst_state) == tree_pt->node_map.end() || tree_pt->node_map[dst_state]->
                        index.find(d) == tree_pt->node_map[dst_state]->index.end()) {
                        // need to be checked
                        tree_node *dst_pt = add_node(tree_pt, d, dst_state, tree_pt->root->node_ID, src_pt,
                                                     min(src_pt->timestamp, timestamp), timestamp);
                        non_lm_expand(dst_pt, tree_pt);
                    } else {
                        tree_node *dst_pt = tree_pt->node_map[dst_state]->index[d];
                        if (dst_pt->timestamp < time) {
                            if (dst_pt->parent != src_pt)
                                tree_pt->substitute_parent(src_pt, dst_pt);
                            dst_pt->timestamp = time;
                            dst_pt->edge_timestamp = timestamp;
                            non_lm_expand(dst_pt, tree_pt);
                        }
                    }
                    return;
                }
            }
        }

        // if we do not expand the normal tree, we need to update it with the paths pass landmarks.
        if (max_dst_time > min(max_src_time, timestamp)) // in this case the new edge cannot produce latest paths
            return;
        if (lm_results.find(max_src_lm) != lm_results.end()) {
            // otherwise we find the landmark passing which the timestamp of path to src is latest, and update the result sets with the recorded updated nodes.
            unsigned int lm_ID = max_src_lm >> 32;
            unsigned int lm_state = (max_src_lm & 0xFFFFFFFF);
            update_result(lm_results[max_src_lm], tree_pt->root->node_ID,
                          tree_pt->node_map[lm_state]->index[lm_ID]->timestamp);
        }
    }

    void expand_in_recover(tree_node *expand_node, RPQ_tree *tree_pt, RPQ_tree *lm_tree, bool lm_expand_tree = false)
    // this is the function to expand a tree when recovering
    // the subtree of a eliminated landmark. tree_pt is the tree in which we recover the subtree, lm_expand_tree indicating if it is an LM tree. lm_tree is the pointer to the LM tree of the eliminated landmark
    // we carry out expand following this LM tree rather than traverse the graph.
    {
        if (!lm_tree)
            return;

        queue<pair<tree_node *, tree_node *> > q;
        // each pair store a node in tree_pt and the corresponding node in lm_tree.
        q.emplace(expand_node, lm_tree->root);
        while (!q.empty()) {
            pair<tree_node *, tree_node *> tmp = q.front();
            q.pop();
            tree_node *expand_tree_node = tmp.first;
            tree_node *lm_tree_node = tmp.second;
            if (landmarks.find(merge_long_long(lm_tree_node->node_ID, lm_tree_node->state)) != landmarks.end()) {
                expand_tree_node->lm = true;
                tree_pt->add_lm(merge_long_long(lm_tree_node->node_ID, lm_tree_node->state));
                // if we encounter a landmark, we prune this branch, but we donot need to update time info map or the result set, as
                // no new path will be generated in subtree recover.
                continue;
            }
            tree_node *child = lm_tree_node->child;
            while (child) // scan the child of the lm tree node
            {
                unsigned int v = child->node_ID;
                unsigned int state = child->state;
                unsigned int time = min(child->edge_timestamp, expand_tree_node->timestamp);
                // compute the timestamp of this child in tree_pt
                if (tree_pt->node_map.find(state) == tree_pt->node_map.end() || tree_pt->node_map[state]->index.find(v)
                    == tree_pt->node_map[state]->index.end()) {
                    tree_node *new_node = nullptr; // if it does not exist, we add this node
                    if (lm_expand_tree) {
                        new_node = add_lm_node(tree_pt, v, state, tree_pt->root->node_ID, tree_pt->root->state,
                                               expand_tree_node, time, child->edge_timestamp);
                        if (tree_pt->get_time_info(v, state) < time)
                            // in fact I suppose this will not happen, as no new path will be build in recovering subtree, still check it to make sure.
                            tree_pt->add_time_info(v, state, time);
                    } else
                        new_node = add_node(tree_pt, v, state, tree_pt->root->node_ID, expand_tree_node, time,
                                            child->edge_timestamp);
                    q.emplace(new_node, child);
                } else {
                    tree_node *new_node = tree_pt->node_map[state]->index[v];
                    // of the node exists, we update its timestamp.
                    if (new_node->timestamp < time) {
                        if (new_node->parent != expand_tree_node)
                            tree_pt->substitute_parent(expand_tree_node, new_node);
                        new_node->edge_timestamp = child->edge_timestamp;
                        new_node->timestamp = time;
                        if (lm_expand_tree) {
                            if (tree_pt->get_time_info(v, state) < time)
                                tree_pt->add_time_info(v, state, time);
                        }
                        q.emplace(new_node, child);
                    } else {
                        q.emplace(new_node, child);
                        // it should be noted here, this is necessary. Even if the node already exisits in tree_pt, its successor may be pruned due to a path with larger timestamp in the lm_tree, thus we need to proceed to check.
                    }
                }
                child = child->brother;
            }
        }
    }

    void generate_time_info(RPQ_tree *tree_pt)
    // this function is used to generate time info map for new LM trees. Time info map is generated as a union of nodes in this LM tree, and the time info map of the landmarks in it.
    {
        for (auto iter = tree_pt->node_map.begin(); iter != tree_pt->node_map.end(); iter++) {
            unsigned int state = iter->first;
            if (tree_pt->time_info.find(state) == tree_pt->time_info.end())
                tree_pt->time_info[state] = new time_info_index;
            for (auto &[fst, snd] : iter->second->index)
                tree_pt->time_info[state]->index[fst] = snd->timestamp;
        }
        for (auto set_iter = tree_pt->landmarks.begin(); set_iter != tree_pt->landmarks.end(); set_iter++) {
            unsigned long long lm_info = *set_iter;
            unsigned int lm_ID = (lm_info >> 32);
            unsigned int lm_state = (lm_info & 0xFFFFFFFF);
            tree_node *lm_node = tree_pt->find_node(lm_ID, lm_state);
            unsigned int lm_time = lm_node->timestamp;
            if (forests.find(lm_info) != forests.end()) {
                RPQ_tree *lm_tree = forests[lm_info];
                for (auto &[fst, snd] : lm_tree->time_info) {
                    unsigned int state = fst;
                    if (tree_pt->time_info.find(state) == tree_pt->time_info.end())
                        tree_pt->time_info[state] = new time_info_index;
                    for (auto info_iter = snd->index.begin(); info_iter != snd->index.end(); info_iter++)
                        tree_pt->time_info[state]->index[info_iter->first] = max(tree_pt->time_info[state]->index[info_iter->first], min(info_iter->second, lm_time));
                }
            }
        }
    }

    void switch_tree_index(RPQ_tree *tree_pt)
    // this function switch the reverse index of nodes in tree_pt from v2t_index to v2l_index, used when tree_pt is transformed into an LM tree.
    {
        unsigned int root_ID = tree_pt->root->node_ID;
        unsigned int root_state = tree_pt->root->state;
        for (auto iter = tree_pt->node_map.begin();iter != tree_pt->node_map.end(); iter++) {
            for (auto &[fst, snd] : iter->second->index) {
                delete_index(fst, snd->state, root_ID);
                add_lm_index(tree_pt, fst, snd->state, root_ID, root_state);
            }
        }
    }

    void switch_tree_index_reverse(RPQ_tree *tree_pt)
    // this function switch the reverse index of nodes in tree_pt from v2l_index to v2t_index, used when tree_pt is transformed from an LM tree to a normal tree.
    {
        unsigned int root_ID = tree_pt->root->node_ID;
        unsigned int root_state = tree_pt->root->state;
        for (auto iter = tree_pt->node_map.begin(); iter != tree_pt->node_map.end(); iter++) {
            for (auto & iter2 : iter->second->index) {
                delete_lm_index(iter2.first, iter2.second->state, root_ID, root_state);
                add_index(tree_pt, iter2.first, iter2.second->state, root_ID);
            }
        }
    }

    void recover_subtree(unsigned int v, unsigned int state, RPQ_tree *lm_tree)
    // this function recovers the subtrees of a deleted landmark (v, state) in normal trees, lm_tree is the LM tree of this landmark.
    {
        auto iter = v2t_index.find(state);
        if (iter != v2t_index.end()) {
            auto tree_iter = iter->second->tree_index.find(v);
            if (tree_iter != iter->second->tree_index.end()) {
                tree_info *tmp = tree_iter->second;
                while (tmp) {
                    RPQ_tree *tree_pt = tmp->tree;
                    tree_pt->landmarks.erase(merge_long_long(v, state));
                    shrink(tree_pt->landmarks);
                    tree_node *lm_node = tree_pt->find_node(v, state);
                    lm_node->lm = false;
                    if (lm_tree) {
                        if (tree_pt->root->node_ID != v || tree_pt->root->state != state) {
                            bool latest = true;
                            for (auto lm_iter = tree_pt->landmarks.begin();
                                 lm_iter != tree_pt->landmarks.end(); lm_iter++)
                            // we first check if the landmark to make sure if the local path is latest
                            {
                                unsigned long long lm_info = *lm_iter;
                                if (forests.find(lm_info) != forests.end()) {
                                    tree_node *tmp_lm = tree_pt->find_node((lm_info >> 32), (lm_info & 0xFFFFFFFF));
                                    if (min(forests[lm_info]->get_time_info(v, state),
                                            tmp_lm->timestamp) >= lm_node->timestamp) {
                                        latest = false;
                                        break;
                                    }
                                }
                            }
                            if (latest) // we only recover the subtree if the local path is latest
                                expand_in_recover(lm_node, tree_pt, lm_tree);
                        }
                    }
                    tmp = tmp->next;
                }
            }
        }
    }

    void recover_subtree_lm(unsigned int v, unsigned int state, RPQ_tree *lm_tree)
    // this function recovers the subtrees of a deleted landmark (v, state) in other LM trees, lm_tree is the LM tree of this landmark.
    {
        auto iter = v2l_index.find(state);
        if (iter != v2l_index.end()) {
            auto tree_iter = iter->second->tree_index.find(v);
            if (tree_iter != iter->second->tree_index.end()) {
                tree_info *tmp = tree_iter->second;
                while (tmp) {
                    RPQ_tree *tree_pt = tmp->tree;
                    tree_pt->landmarks.erase(merge_long_long(v, state));
                    shrink(tree_pt->landmarks);
                    tree_node *lm_node = tree_pt->find_node(v, state);
                    lm_node->lm = false;
                    if (lm_tree) {
                        if (tree_pt->root->node_ID != v || tree_pt->root->state != state) {
                            // in LM trees, we can quickly  decide if the local path is latest with time info map
                            if (tree_pt->get_time_info(v, state) <= lm_node->timestamp) {
                                expand_in_recover(lm_node, tree_pt, lm_tree, true);
                            }
                        }
                    }
                    tmp = tmp->next;
                }
            }
        }
    }

    void retrieve_subtree(unsigned int v, unsigned int state)
    // this function delete the subtree of a landmark (v, state) in normal trees.
    {
        auto iter = v2t_index.find(state);
        if (iter != v2t_index.end()) {
            auto tree_iter = iter->second->tree_index.find(v);
            if (tree_iter != iter->second->tree_index.end()) {
                tree_info *tmp = tree_iter->second;
                while (tmp) {
                    RPQ_tree *tree_pt = tmp->tree;
                    tree_pt->landmarks.insert(merge_long_long(v, state));
                    // add the new landmark to the landmark set of the normal tree
                    tree_node *lm_node = tree_pt->find_node(v, state);
                    lm_node->lm = true;
                    tree_node *child = lm_node->child;
                    // carry out a BFS starting from childs of this landmark, and delete all the succesors in its subtree.
                    lm_node->child = nullptr;
                    queue<tree_node *> q;
                    while (child) {
                        q.push(child);
                        child = child->brother;
                    }
                    while (!q.empty()) {
                        tree_node *cur = q.front();
                        q.pop();
                        delete_index(cur->node_ID, cur->state, tree_pt->root->node_ID);
                        child = cur->child;
                        while (child) {
                            q.push(child);
                            child = child->brother;
                        }
                        assert(tree_pt->node_map.find(cur->state) != tree_pt->node_map.end());
                        tree_pt->node_map[cur->state]->index.erase(cur->node_ID);
                        shrink(tree_pt->node_map[cur->state]->index);
                        if (tree_pt->node_map[cur->state]->index.empty()) {
                            delete tree_pt->node_map[cur->state];
                            tree_pt->node_map.erase(cur->state);
                        }
                        if (cur->lm) {
                            tree_pt->landmarks.erase(merge_long_long(cur->node_ID, cur->state));
                            shrink(tree_pt->landmarks);
                        }
                        delete cur;
                        tree_pt->node_cnt--;
                    }
                    tmp = tmp->next;
                }
            }
        }
    }

    void retrieve_subtree_lm(unsigned int v, unsigned int state, RPQ_tree *lm_tree,
                             unordered_set<unsigned long long> &necessary_nodes)
    // this function delete the subtree of a landmark (v, state) in LM trees.
    // note that there are some "sensitive " nodes which may be missed in the LM tree of (v, state), we need to compare the LM tree of (v, state) (lm_tree) with each subtree to find these nodes, and add them into
    // nessary_nodes. They will be added into lm_tree later. Reasons about why they may be missed can be found in the technical report.
    {
        auto iter = v2l_index.find(state);
        if (iter != v2l_index.end()) {
            auto tree_iter = iter->second->tree_index.find(v);
            if (tree_iter != iter->second->tree_index.end()) {
                tree_info *tmp = tree_iter->second;
                while (tmp) {
                    RPQ_tree *tree_pt = tmp->tree;
                    if (tree_pt->root->node_ID == v && tree_pt->root->state == state) // we need to skip lm_tree itself;
                    {
                        tmp = tmp->next;
                        continue;
                    }
                    tree_pt->landmarks.insert(merge_long_long(v, state));
                    tree_node *lm_node = tree_pt->find_node(v, state);
                    lm_node->lm = true;
                    tree_node *child = lm_node->child;
                    lm_node->child = nullptr;
                    queue<tree_node *> q;
                    vector<tree_node *> vec;
                    unordered_map<unsigned long long, unsigned int> lm_time;
                    // this map stores timestamps of the path from the landmark (v, state) to each successor in the subtree, if we find a latest path in the subtree but not in lm_tree, we need to add nodes
                    // in this path to necessary nodes,
                    lm_time[merge_long_long(v, state)] = MAX_INT;
                    // (v, state) is the root of the subtree, and we set its timestamp of MAX_INT
                    while (child) {
                        q.push(child);
                        lm_time[merge_long_long(child->node_ID, child->state)] = child->edge_timestamp;
                        // timestamp of the path from the landmark to its child is just the edge between them.
                        vec.push_back(child);
                        child = child->brother;
                    }
                    while (!q.empty()) {
                        tree_node *cur = q.front();
                        q.pop();
                        unsigned int time = lm_time[merge_long_long(cur->node_ID, cur->state)];
                        if (time == lm_tree->get_time_info(cur->node_ID, cur->state) && lm_tree->
                            find_node(cur->node_ID, cur->state) == nullptr && necessary_nodes.
                            find(merge_long_long(cur->node_ID, cur->state)) == necessary_nodes.end())
                        // a latest path, but not in the new lm tree
                        {
                            tree_node *cur2 = cur;
                            //we find a successor to which the path in the subtree is the latest but not in the lm_tree
                            while (cur2 != lm_node) // we store nodes in the path from the landmark to this successor
                            {
                                necessary_nodes.insert(merge_long_long(cur2->node_ID, cur2->state));
                                cur2 = cur2->parent;
                            }
                        }
                        child = cur->child;
                        while (child) {
                            q.push(child);
                            lm_time[merge_long_long(child->node_ID, child->state)] = min(time, child->edge_timestamp);
                            vec.push_back(child);
                            // we store all the tree nodes in a vector, and delete them after we gather all the necessary nodes.
                            child = child->brother;
                        }
                    }
                    for (auto & i : vec) {
                        delete_lm_index(i->node_ID, i->state, tree_pt->root->node_ID, tree_pt->root->state);
                        tree_pt->node_map[i->state]->index.erase(i->node_ID);
                        shrink(tree_pt->node_map[i->state]->index);
                        if (tree_pt->node_map[i->state]->index.empty()) {
                            delete tree_pt->node_map[i->state];
                            tree_pt->node_map.erase(i->state);
                        }
                        if (i->lm) {
                            tree_pt->landmarks.erase(merge_long_long(i->node_ID, i->state));
                            shrink(tree_pt->landmarks);
                        }
                        delete i;
                        tree_pt->node_cnt--;
                    }
                    tmp = tmp->next;
                }
            }
        }
    }

    void fulfill_new_lm_tree(RPQ_tree *tree_pt, unordered_set<unsigned long long> necessary_nodes)
    // this function add necessary nodes to a new LM tree. these nodes are in paths which are latest but not in the LM tree, they are pruned because there is already a path with the same timestamp passing
    // other landmarks. However, due to existence of circles, these paths may be in the subtree of the new landmark, and once the subtree is deleted, these paths are missed. thus we need to add them back in
    // the LM tree. Details about how these nodes are missed can be found in the technical report
    {
        vector<tree_node *> original_vec;
        for (auto &[fst, snd] : tree_pt->node_map) {
            for (auto &[fst1, snd1] : snd->index)
                original_vec.push_back(snd1);
        }

        for (auto cur : original_vec)
        // we scan the nodes already in the lm tree one by one, try to expand them to add the necessary nodes.
        {
            queue<tree_node *> q;
            q.push(cur);
            while (!q.empty()) {
                cur = q.front();
                q.pop();
                if (cur->lm) {
                    tree_pt->landmarks.insert(merge_long_long(cur->node_ID, cur->state));
                    continue;
                }
                vector<sg_edge*> vec = g.get_all_suc_ptrs(cur->node_ID);
                for (auto & j : vec) {
                    unsigned int successor = j->d;
                    long long dst_state = aut.getNextState(cur->state, j->label);
                    if (dst_state == -1) continue;
                    unsigned int time = min(cur->timestamp, (unsigned int)j->timestamp);
                    if (necessary_nodes.find(merge_long_long(successor, dst_state)) == necessary_nodes.end() && tree_pt->get_time_info(successor, dst_state) > time)
                        // we prune a branch if it is not a necessary nodes and the path to it is not the latest.
                        continue;
                    if (tree_pt->node_map.find(dst_state) != tree_pt->node_map.end() && tree_pt->node_map[dst_state]->index.find(successor) != tree_pt->node_map[dst_state]->index.end()) {
                        tree_node *suc_pt = tree_pt->node_map[dst_state]->index[successor];
                        if (suc_pt->timestamp < time) {
                            if (suc_pt->parent != cur)
                                tree_pt->substitute_parent(cur, suc_pt);
                            suc_pt->edge_timestamp = j->timestamp;
                            suc_pt->timestamp = time;
                            q.push(suc_pt);
                        }
                    } else {
                        tree_node *suc_pt = add_lm_node(tree_pt, successor, dst_state, tree_pt->root->node_ID, tree_pt->root->state, cur, time, j->timestamp, landmarks.find(merge_long_long(successor, dst_state)) != landmarks.end());
                        q.push(suc_pt);
                    }
                }
            }
        }
    }

    RPQ_tree *build_lm_tree(unsigned int v, unsigned int state)
    // this function build new lm tree for a landmark, we use time info in prune and may miss some nodes, we will add them back with above fulfill_new_lm_tree later .
    {
        auto *new_tree = new RPQ_tree;
        new_tree->root = new_tree->add_node(v, state, nullptr, MAX_INT, MAX_INT);
        new_tree->add_time_info(v, state, MAX_INT);
        queue<tree_node *> q;
        q.push(new_tree->root);
        while (!q.empty()) {
            tree_node *tmp = q.front();
            q.pop();
            if (tmp != new_tree->root && landmarks.find(merge_long_long(tmp->node_ID, tmp->state)) != landmarks.end()) {
                tmp->lm = true;
                new_tree->add_lm(merge_long_long(tmp->node_ID, tmp->state));
                unordered_map<unsigned long long, unsigned int> tmp_map;
                lm_expand_in_lm_subtree(tmp->node_ID, tmp->state, new_tree, tmp->timestamp, tmp_map);
                // we only need to update the time info map of the new LM tree with the landmark we find
                tmp_map.clear();
                continue;
            }

            map<long long, long long> aut_edge = aut.getAllSuccessors(tmp->state);
            vector<sg_edge*> sucs = g.get_all_suc_ptrs(tmp->node_ID);
            for (auto & suc : sucs) {
                unsigned int successor = suc->d;
                unsigned int label = suc->label;
                unsigned int time = min(tmp->timestamp, (unsigned int)suc->timestamp);
                if (aut_edge.find(label) == aut_edge.end())
                    continue;
                long long dst_state = aut_edge[label];

                if (new_tree->get_time_info(successor, dst_state) >= time)
                    // we pruen the branch once there is already a path with no smaller timestamp. this may lead to some nodes missing. they will be added back later in fulfill_new_lm tree.
                    continue;

                if (new_tree->node_map.find(dst_state) == new_tree->node_map.end() || new_tree->node_map[dst_state]->
                    index.find(successor) == new_tree->node_map[dst_state]->index.end()) {
                    tree_node *new_node = new_tree->add_node(successor, dst_state, tmp, time, suc->timestamp);
                    new_tree->add_time_info(successor, dst_state, time);
                    q.push(new_node);
                } else {
                    tree_node *dst_pt = new_tree->node_map[dst_state]->index[successor];
                    if (dst_pt->timestamp < time) {
                        if (dst_pt->parent != tmp)
                            new_tree->substitute_parent(tmp, dst_pt);
                        dst_pt->timestamp = time;
                        dst_pt->edge_timestamp = suc->timestamp;
                        new_tree->add_time_info(successor, dst_state, time);
                        q.push(dst_pt);
                    }
                }
            }
        }
        return new_tree;
    }

    void build_v2l_index(RPQ_tree *new_tree)
    // this function build the reverse index given a new LM tree, we do not build the reverse index during LM tree building, as the LM tree may not become valid.
    {
        queue<tree_node *> q;
        q.push(new_tree->root);
        while (!q.empty()) {
            tree_node *tmp = q.front();
            q.pop();
            add_lm_index(new_tree, tmp->node_ID, tmp->state, new_tree->root->node_ID, new_tree->root->state);
            tree_node *child = tmp->child;
            while (child) {
                q.push(child);
                child = child->brother;
            }
        }
    }

    bool count_presence(unsigned int id, unsigned int state, unsigned int threshold)
    // this function count the number of presences of a node in the forest, and returns true once it exceeds the threshold.
    {
        unsigned int sum = 0;
        if (v2t_index.find(state) != v2t_index.end()) {
            tree_info_index *index = v2t_index[state];
            if (index->tree_index.find(id) != index->tree_index.end()) {
                tree_info *tmp = index->tree_index[id];
                while (tmp) {
                    sum++;
                    if (sum >= threshold)
                        return true;
                    tmp = tmp->next;
                }
            }
        }
        if (v2l_index.find(state) != v2l_index.end()) {
            lm_info_index *index = v2l_index[state];
            if (index->tree_index.find(id) != index->tree_index.end()) {
                tree_info *tmp = index->tree_index[id];
                while (tmp) {
                    sum++;
                    if (sum >= threshold)
                        return true;
                    tmp = tmp->next;
                }
            }
        }
        return false;
    }


    double recover_subtree_preview(unsigned int v, unsigned int state, RPQ_tree *lm_tree, double node_budget)
    // this function predicts the number of nodes we need to add back in normal trees once we delete a landmark from the landmark set.
    // if the number of nodes exceeds the node_budget, we stop the counting and return. lm_tree is the LM tree of the landmark we try to delete.
    {
        unsigned int node_cnt = 0;
        if (auto iter = v2t_index.find(state); iter != v2t_index.end()) {
            auto tree_iter = iter->second->tree_index.find(v);
            if (tree_iter != iter->second->tree_index.end()) {
                tree_info *tmp = tree_iter->second;
                while (tmp) {
                    RPQ_tree *tree_pt = tmp->tree;
                    for (auto &[fst, snd] : lm_tree->node_map) {
                        for (auto & node_iter2 : snd->index) {
                            if (tree_pt->find_node(node_iter2.first, fst) == nullptr) {
                                node_budget--;
                                // the number of nodes is predicted as the number of nodes in the lm_tree but not in the normal tree, in this prediction we do not bother to check if the local path to
                                // the landmark is latest, as it needs considerabel computation in normal trees. As a result the prediction will be larger than the fact.
                            }
                        }
                    }
                    if (node_budget <= 0)
                        return 0;
                    tmp = tmp->next;
                }
            }
        }
        return node_budget;
    }

    double recover_subtree_lm_preview(unsigned int v, unsigned int state, RPQ_tree *lm_tree, double node_budget)
    // this function predicts the number of nodes we need to add back in LM trees once we delete a landmark from the landmark set.
    // if the number of nodes exceeds the node_budget, we stop the counting and return. lm_tree is the LM tree of the landmark we try to delete.
    {
        unsigned int node_cnt = 0;
        if (auto iter = v2l_index.find(state); iter != v2l_index.end()) {
            if (auto tree_iter = iter->second->tree_index.find(v); tree_iter != iter->second->tree_index.end()) {
                tree_info *tmp = tree_iter->second;
                while (tmp) {
                    RPQ_tree *tree_pt = tmp->tree;
                    if (tree_pt->root->node_ID == v && tree_pt->root->state == state) // skip lm_tree itself.
                    {
                        tmp = tmp->next;
                        continue;
                    }
                    tree_node *lm_node = tree_pt->find_node(v, state);
                    if (tree_pt->get_time_info(v, state) == lm_node->timestamp)
                    // in LM tree we will check if the local path to the landmark is latest, as it costs little.
                    {
                        for (auto &[fst, snd] : lm_tree->node_map) {
                            for (auto &[fst1, snd2] : snd->index) {
                                if (tree_pt->find_node(fst1, fst) == nullptr) {
                                    node_budget--;
                                }
                            }
                        }
                    }
                    if (node_budget <= 0)
                        return 0;
                    tmp = tmp->next;
                }
            }
        }
        return node_budget;
    }

    unsigned int retrieve_subtree_preview(unsigned int v, unsigned int state)
    // the function count the number of nodes in the subtree of (v, state) in normal trees.
    {
        unsigned int node_cnt = 0;
        if (auto iter = v2t_index.find(state); iter != v2t_index.end()) {
            if (auto tree_iter = iter->second->tree_index.find(v); tree_iter != iter->second->tree_index.end()) {
                tree_info *tmp = tree_iter->second;
                while (tmp) {
                    RPQ_tree *tree_pt = tmp->tree;
                    if (tree_pt->root->node_ID == v && tree_pt->root->state == state)
                    // skip the normal tree of (v, state) itself. As (v, state) is not a landmark yet, there may be a normal tree rooted at it if state = 0;
                    {
                        tmp = tmp->next;
                        continue;
                    }
                    tree_node *lm_node = tree_pt->find_node(v, state);
                    queue<tree_node *> q;
                    q.push(lm_node);
                    while (!q.empty()) {
                        tree_node *cur = q.front();
                        q.pop();
                        node_cnt++;
                        tree_node *child = cur->child;
                        while (child) {
                            q.push(child);
                            child = child->brother;
                        }
                    }
                    node_cnt--; // the landmark itself should not be counted.
                    tmp = tmp->next;
                }
            }
        }
        return node_cnt;
    }

    unsigned int retrieve_subtree_lm_preview(unsigned int v, unsigned int state)
    // the function count the number of nodes in the subtree of (v, state) in LM trees.
    {
        unsigned int node_cnt = 0;
        if (auto iter2 = v2l_index.find(state); iter2 != v2l_index.end()) {
            if (auto tree_iter = iter2->second->tree_index.find(v); tree_iter != iter2->second->tree_index.end()) {
                tree_info *tmp = tree_iter->second;
                while (tmp) {
                    RPQ_tree *tree_pt = tmp->tree;
                    tree_node *lm_node = tree_pt->find_node(v, state);
                    queue<tree_node *> q;
                    q.push(lm_node);
                    while (!q.empty()) {
                        tree_node *cur = q.front();
                        q.pop();
                        node_cnt++;
                        tree_node *child = cur->child;
                        while (child) {
                            q.push(child);
                            child = child->brother;
                        }
                    }
                    node_cnt--; // the landmark itself should not be counted.
                    tmp = tmp->next;
                }
            }
        }
        return node_cnt;
    }

    void delete_v2h_index(RPQ_tree *tree_pt) // this function delete the reverse index of nodes in a LM tree.
    {
        queue<tree_node *> q;
        q.push(tree_pt->root);
        while (!q.empty()) {
            tree_node *tmp = q.front();
            q.pop();
            for (tree_node *cur = tmp->child; cur; cur = cur->brother)
                q.push(cur);
            delete_lm_index(tmp->node_ID, tmp->state, tree_pt->root->node_ID, tree_pt->root->state);
        }
    }




    void erase_tree_node(RPQ_tree *tree_pt, tree_node *child)
    // this function deletes subtree rooted at the given node (child) in a normal tree (tree_pt)
    {
        queue<tree_node *> q;
        q.push(child);
        tree_pt->separate_node(child);
        // 'child' is disconnected with its parent,other nodes donot need to call this function, as there parents and brothers are all deleted;
        while (!q.empty()) {
            tree_node *tmp = q.front();
            q.pop();
            for (tree_node *cur = tmp->child; cur; cur = cur->brother)
                q.push(cur);
            tree_pt->remove_node(tmp);
            delete_index(tmp->node_ID, tmp->state, tree_pt->root->node_ID);
            delete tmp;
        }
    }

    void erase_lm_tree_node(RPQ_tree *tree_pt, tree_node *child, vector<unsigned long long> &deleted)
    // this function deletes subtree rooted at the given node (child) in an LM tree (tree_pt), different from above,
    // we need to record the deleted nodes with a vectore deleted, we will use these nodes in a backward search later to delete time info map in precursors of this LM tree in the dependency graph.
    {
        queue<tree_node *> q;
        q.push(child);
        tree_pt->separate_node(child);
        // 'child' is disconnected with its parent,other nodes donot need to call this function, as there parents and brothers are all deleted;
        while (!q.empty()) {
            tree_node *tmp = q.front();
            q.pop();
            deleted.push_back(merge_long_long(tmp->node_ID, tmp->state));
            for (tree_node *cur = tmp->child; cur; cur = cur->brother)
                q.push(cur);
            tree_pt->remove_node(tmp);
            delete_lm_index(tmp->node_ID, tmp->state, tree_pt->root->node_ID, tree_pt->root->state);
            delete tmp;
        }
    }


    void expire_backtrack(unsigned int v, unsigned int state, unsigned int expired_time,
                          vector<unsigned long long> &deleted_results, unordered_set<unsigned long long> &visited)
    // this function performs a backward search from a landmark v, state). Deleted_results are the nodes where the path from landmark to them has expired. entry of these nodes in the time info map of its precursor LM trees
    // may also expire, we need to check them in the search, visited records the visited LM trees, in case of repeated check. expired_time is the tail of the silding window, entries with timestamp smaller than it expire.
    {
        if (auto iter = v2l_index.find(state); iter != v2l_index.end()) {
            if (auto tree_iter = iter->second->tree_index.find(v); tree_iter != iter->second->tree_index.end()) {
                tree_info *tmp = tree_iter->second;
                while (tmp) {
                    RPQ_tree *tree_pt = tmp->tree;
                    unsigned long long tree_info = merge_long_long(tree_pt->root->node_ID, tree_pt->root->state);
                    if (visited.find(tree_info) != visited.end()) {
                        tmp = tmp->next;
                        continue;
                    }
                    visited.insert(tree_info);
                    vector<unsigned long long> tracked_nodes;
                    for (unsigned long long dst_info : deleted_results) {
                        unsigned int dst_state = (dst_info & 0xFFFFFFFF);
                        unsigned int dst_ID = (dst_info >> 32);
                        if (tree_pt->time_info.find(dst_state) != tree_pt->time_info.end()) {
                            if (tree_pt->time_info[dst_state]->index.find(dst_ID) != tree_pt->time_info[dst_state]->
                                index.end()) {
                                if (tree_pt->time_info[dst_state]->index[dst_ID] < expired_time) {
                                    // check if the time info entry of a node is expired.
                                    tree_pt->time_info[dst_state]->index.erase(dst_ID);
                                    shrink(tree_pt->time_info[dst_state]->index);
                                }
                                if (tree_pt->time_info[dst_state]->index.empty())
                                    tree_pt->time_info.erase(dst_state);
                            }
                        }
                        tracked_nodes.push_back(dst_info);
                        // it should be noted that we need to futher backtrack up with all nodes in deleted_results, otherwise errors will happen, some expired time info entries will be left
                        // this is caused by circles in the depdency graph.
                    }
                    if (!tracked_nodes.empty())
                        expire_backtrack(tree_pt->root->node_ID, tree_pt->root->state, expired_time, tracked_nodes,
                                         visited);
                    tracked_nodes.clear();
                    tmp = tmp->next;
                }
            }
        }
    }

    void expire_per_lm_tree(unsigned int v, unsigned int state, RPQ_tree *tree_pt, unsigned int expired_time)
    // carry out expiration in an LM tree tree_pt given a possibly expired node (v, state) and tail of sliding window expired_time.
    {
        if (tree_pt->node_map.find(state) != tree_pt->node_map.end()) {
            if (tree_pt->node_map[state]->index.find(v) != tree_pt->node_map[state]->index.end()) {
                if (tree_node *dst_pt = tree_pt->node_map[state]->index[v]; dst_pt->timestamp < expired_time) {
                    // if this node indeex expireds, we need to erase its subtree and carry out expire_backtrack
                    vector<unsigned long long> erased;
                    vector<unsigned long long> deleted;
                    unordered_set<unsigned long long> visited;
                    erase_lm_tree_node(tree_pt, dst_pt, erased);
                    if (!erased.empty()) {
                        for (unsigned long long dst_info : erased) {
                            if (landmarks.find(dst_info) != landmarks.end())
                            // if a landmark is deleted, we need to check if it will influence the time info map
                            {
                                if (forests.find(dst_info) != forests.end()) {
                                    RPQ_tree *dst_tree = forests[dst_info];
                                    for (auto & dst_iter : dst_tree->time_info) {
                                        unsigned int k = dst_iter.first;
                                        if (tree_pt->time_info.find(k) == tree_pt->time_info.end())
                                            continue;
                                        time_info_index *target_index = tree_pt->time_info[k];
                                        // scan the time info in the LM tree of the deleted landmark, as the paths to nodes in this time info map passing the deleted landmark expire, time info of these nodes
                                        // in tree_pt may also expire, we need to check, and record the expired ones.
                                        for (auto time_iter = dst_iter.second->index.begin(); time_iter != dst_iter.second->index.end(); time_iter++) {
                                            if (target_index->index.find(time_iter->first) != target_index->index.end()) {
                                                if (target_index->index[time_iter->first] < expired_time) {
                                                    target_index->index.erase(time_iter->first);
                                                    deleted.push_back(merge_long_long(time_iter->first, k));
                                                }
                                            }
                                        }
                                        shrink(target_index->index);
                                        if (target_index->index.empty())
                                            tree_pt->time_info.erase(k);
                                    }
                                }
                            }
                            unsigned int dst_ID = (dst_info >> 32);
                            unsigned int dst_state = (dst_info & 0xFFFFFFFF);
                            if (tree_pt->time_info.find(dst_state) != tree_pt->time_info.end()) {
                                // check time info of this deleted node.
                                if (tree_pt->time_info[dst_state]->index.find(dst_ID) != tree_pt->time_info[dst_state]->
                                    index.end()) {
                                    if (tree_pt->time_info[dst_state]->index[dst_ID] < expired_time) {
                                        tree_pt->time_info[dst_state]->index.erase(dst_ID);
                                        shrink(tree_pt->time_info[dst_state]->index);
                                        if (tree_pt->time_info[dst_state]->index.empty())
                                            tree_pt->time_info.erase(dst_state);
                                        deleted.push_back(dst_info);
                                    }
                                }
                            }
                        }
                        erased.clear();
                        visited.insert(merge_long_long(tree_pt->root->node_ID, tree_pt->root->state));
                        if (!deleted.empty())
                            expire_backtrack(tree_pt->root->node_ID, tree_pt->root->state, expired_time, deleted,
                                             visited);
                        deleted.clear();
                        visited.clear();
                    }
                }
            }
        }
    }

    void expire_per_tree(unsigned int v, unsigned int state, RPQ_tree *tree_pt, unsigned int expired_time)
    // expire in normal tree, we only need to delete the nodes in the subtree.
    {
        if (tree_pt->node_map.find(state) != tree_pt->node_map.end()) {
            if (tree_pt->node_map[state]->index.find(v) != tree_pt->node_map[state]->index.end()) {
                tree_node *dst_pt = tree_pt->node_map[state]->index[v];
                if (dst_pt->timestamp < expired_time)
                    erase_tree_node(tree_pt, dst_pt);
            }
        }
    }
};
