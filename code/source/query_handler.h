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
        // Node* candidate_parent;
        auto statePairs = fsa.getStatePairsWithTransition(edge->label); // (sb, sd)
        for (const auto &sb_sd: statePairs) {
            // forall (sb, sd) where delta(sb, label) = sd
            if (sb_sd.first == 0 && !forest.hasTree(edge->s)) {
                // if sb=0 and there is no tree with root vertex vb
                forest.addTree(edge->id, edge->s, 0, edge->timestamp);
            }
            for (auto tree: forest.findTreesWithNode(edge->s, sb_sd.first)) {
                if (tree.expired) continue; // skip expired trees
                // for all Trees that contain ⟨vb,sb⟩
                // push (⟨vb,sb⟩, <vd,sd>, edge_id) into Q
                //priority_queue<tree_expansion*, vector<tree_expansion*>, time_comparator> Q; // (⟨vb,sb⟩, <vd,sd>, edge_id, edge_timestamp) // (⟨vb,sb⟩, <vd,sd>, edge_id, edge_timestamp)
                std::deque<tree_expansion*> Q;
                Q.push_back(new tree_expansion(edge->s, sb_sd.first, edge->d, sb_sd.second, edge->timestamp, edge->timestamp, edge->timestamp, edge->expiration_time));
                while (!Q.empty()) {
                    auto element = Q.front();
                    Q.pop_front(); // (⟨vi,si⟩, <vj,sj>, edge_id)
                    auto vj_sj_node = forest.findNodeInTree(tree.rootVertex, element->vd, element->sd);
                    auto vb_sb_node = forest.findNodeInTree(tree.rootVertex, element->vb, element->sb);

                    if (!vj_sj_node) {
                        // if tree does not contain <vj,sj>
                        // add <vj,sj> into tree with parent vi_si
                        if (!forest.addChildToParentTimestamped(tree.rootVertex, vb_sb_node, element->vd, element->sd, element->edge_timestamp, std::min(element->edge_expiration_time, vb_sb_node->expiration_time))) continue;
                    //} else if (vb_sb_node && vj_sj_node->timestamp < (element->edge_timestamp < vb_sb_node->timestamp ? element->edge_timestamp : vb_sb_node->timestamp)) {
                    } else if (vb_sb_node && vj_sj_node->expiration_time < std::min(element->edge_expiration_time, vb_sb_node->expiration_time)) {
                        // if tree already contains <vj,sj>
                        // change parent to vi_si if the timestamp is smaller
                        long long old_expiry = vj_sj_node->expiration_time;  // save before re-parenting
                        if (!forest.changeParentTimestamped(vj_sj_node, vb_sb_node, (element->edge_timestamp < vb_sb_node->timestamp ? element->edge_timestamp : vb_sb_node->timestamp), tree.rootVertex, std::min(element->edge_expiration_time, vb_sb_node->expiration_time))) continue;
                        for (auto successors : sg.get_all_suc(element->vd)) {
                            if (auto sq = fsa.getNextState(element->sd, successors.label); sq != -1) {
                                if (successors.expiration_time > old_expiry) {
                                    Q.push_back(new tree_expansion(element->vd, element->sd, successors.d, sq, successors.timestamp, element->d_timestamp, successors.timestamp, successors.expiration_time));
                                }
                            }
                        }
                    } else
                        continue;

                    // update result set
                    if (fsa.isFinalState(element->sd)) {
                        // cout << "Found a path from " << element->vb << " to " << element->vd << " at time " << element->edge_timestamp << endl;
                        // check if in the result set we already have a path from root to element.vd
                        sink.addEntry(tree.rootVertex, element->vd, edge->timestamp);
                    }
                    // for all vertex <vq,sq> where exists a successor vertex in the snapshot graph where the label the same as the transition in the automaton from the state sj to state sq, push into Q
                    // for (auto successors: sg.get_all_suc(element->vd)) {
                    //     // if the transition exits in the automaton from sj to sq
                    //     if (auto sq = fsa.getNextState(element->sd, successors.label); sq != -1) {
                    //         // if <vq,sq> is not in visited
                    //         Q.push_back(new tree_expansion(element->vd, element->sd, successors.d, sq, successors.timestamp, element->d_timestamp, successors.timestamp, successors.expiration_time));
                    //     }
                    // }
                    for (auto successors : sg.get_all_suc(element->vd)) {
                        if (auto sq = fsa.getNextState(element->sd, successors.label); sq != -1) {
                            // ADD: only if the successor edge's interval overlaps the current node's interval
                            Node* current_node = forest.findNodeInTree(tree.rootVertex, element->vd, element->sd);
                            if (current_node && successors.expiration_time > current_node->timestamp
                                && successors.timestamp < current_node->expiration_time) {
                                    Q.push_back(new tree_expansion(element->vd, element->sd, successors.d, sq, successors.timestamp, element->d_timestamp, successors.timestamp, successors.expiration_time));
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
