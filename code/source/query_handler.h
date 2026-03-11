#ifndef S_PATH_H
#define S_PATH_H

#include <queue>
#include "fsa.h"
#include "rpq_forest.h"
#include "sink.h"
#include "streaming_graph.h"

struct tree_expansion {
    long long vb;
    long long sb;
    long long vd;
    long long sd;
    long long edge_timestamp;
    long long edge_expiration_time;
    long long s_timestamp;
    long long d_timestamp;

    tree_expansion(long long vb, long long sb, long long vd, long long sd, long long edge_timestamp, long long s_timestamp, long long d_timestamp, long long edge_expiration_time)
        : vb(vb), sb(sb), vd(vd), sd(sd), edge_timestamp(edge_timestamp), s_timestamp(s_timestamp), d_timestamp(d_timestamp), edge_expiration_time(edge_expiration_time) {}
};

struct time_comparator
{
    bool operator()(tree_expansion* &t1, tree_expansion* &t2)
    {
        return t1->d_timestamp<t2->d_timestamp;
    }
};

class QueryHandler {
public:
    FiniteStateAutomaton &fsa;
    Forest &forest;
    streaming_graph &sg;
    Sink &sink;

    QueryHandler(FiniteStateAutomaton &fsa, Forest &forest, streaming_graph &sg, Sink &sink)
        : fsa(fsa), forest(forest), sg(sg), sink(sink) {
    }

    void pattern_matching_tc(const sg_edge *edge) {
        auto statePairs = fsa.getStatePairsWithTransition(edge->label);
        for (const auto &sb_sd: statePairs) {
            if (sb_sd.first == 0 && !forest.hasTree(edge->s)) {
                forest.addTree(edge->id, edge->s, 0, edge->timestamp);
            }
            for (auto tree: forest.findTreesWithNode(edge->s, sb_sd.first)) {
                if (tree.expired) continue;
                std::deque<tree_expansion*> Q;
                Q.push_back(new tree_expansion(edge->s, sb_sd.first, edge->d, sb_sd.second, edge->timestamp, edge->timestamp, edge->timestamp, edge->expiration_time));
                while (!Q.empty()) {
                    auto element = Q.front();
                    Q.pop_front();
                    auto vj_sj_node = forest.findNodeInTree(tree.rootVertex, element->vd, element->sd);
                    auto vb_sb_node = forest.findNodeInTree(tree.rootVertex, element->vb, element->sb);

                    // FIX: compute the candidate interval with root guard
                    // If parent is root, interval = edge interval (root has dummy [INT64_MAX, INT64_MAX])
                    // Otherwise, interval = intersect(edge_interval, parent_interval)
                    long long candidate_expiry, candidate_ts;
                    if (vb_sb_node && vb_sb_node->isRoot) {
                        candidate_ts = element->edge_timestamp;
                        candidate_expiry = element->edge_expiration_time;
                    } else if (vb_sb_node) {
                        candidate_ts = std::max(element->edge_timestamp, vb_sb_node->timestamp);
                        candidate_expiry = std::min(element->edge_expiration_time, vb_sb_node->expiration_time);
                    } else {
                        delete element;
                        continue;
                    }

                    if (!vj_sj_node) {
                        // Case 1: node does not exist → add it
                        if (!forest.addChildToParentTimestamped(tree.rootVertex, vb_sb_node, element->vd, element->sd, element->edge_timestamp, candidate_expiry)) {
                            delete element;
                            continue;
                        }
                    } else if (vj_sj_node->expiration_time < candidate_expiry) {
                        // Case 2: node exists but new path offers later expiry → re-parent
                        long long old_expiry = vj_sj_node->expiration_time;
                        if (!forest.changeParentTimestamped(vj_sj_node, vb_sb_node, candidate_ts, tree.rootVertex, candidate_expiry)) {
                            delete element;
                            continue;
                        }

                        // Emit result if final state
                        if (fsa.isFinalState(element->sd)) {
                            sink.addEntry(tree.rootVertex, element->vd, edge->timestamp);
                        }

                        // Only enqueue successors whose expiry exceeds the OLD expiry
                        for (auto successors : sg.get_all_suc(element->vd)) {
                            if (auto sq = fsa.getNextState(element->sd, successors.label); sq != -1) {
                                if (successors.expiration_time > old_expiry) {
                                    Q.push_back(new tree_expansion(element->vd, element->sd, successors.d, sq, successors.timestamp, element->d_timestamp, successors.timestamp, successors.expiration_time));
                                }
                            }
                        }

                        delete element;
                        continue;
                    } else {
                        // Case 3: node exists and current path is not better → skip
                        delete element;
                        continue;
                    }

                    // This code only runs for Case 1 (new node added)
                    if (fsa.isFinalState(element->sd)) {
                        sink.addEntry(tree.rootVertex, element->vd, edge->timestamp);
                    }

                    // Enqueue all valid successors
                    Node* current_node = forest.findNodeInTree(tree.rootVertex, element->vd, element->sd);
                    if (current_node) {
                        for (auto successors : sg.get_all_suc(element->vd)) {
                            if (auto sq = fsa.getNextState(element->sd, successors.label); sq != -1) {
                                if (successors.expiration_time > current_node->timestamp
                                    && successors.timestamp < current_node->expiration_time) {
                                    Q.push_back(new tree_expansion(element->vd, element->sd, successors.d, sq, successors.timestamp, element->d_timestamp, successors.timestamp, successors.expiration_time));
                                }
                            }
                        }
                    }
                    delete element;
                }
                while (!Q.empty()) {
                    delete Q.front();
                    Q.pop_front();
                }
            }
        }
    }
};

#endif //S_PATH_H