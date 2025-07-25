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
    long long s_timestamp;
    long long d_timestamp;

    tree_expansion(long long vb, long long sb, long long vd, long long sd, long long edge_timestamp, long long s_timestamp, long long d_timestamp)
        : vb(vb), sb(sb), vd(vd), sd(sd), edge_timestamp(edge_timestamp), s_timestamp(s_timestamp), d_timestamp(d_timestamp) {}
};

struct time_comparator
{
    bool operator()(tree_expansion* &t1, tree_expansion* &t2)
    {
        return t1->d_timestamp<t2->d_timestamp;
    }
};

struct visited_pair {
    long long vertex;
    long long state;

    bool operator==(const visited_pair &other) const {
        return vertex == other.vertex && state == other.state;
    }
};

struct visitedpairHash {
    size_t operator()(const visited_pair &p) const {
        return hash<long long>()(p.vertex) ^ hash<long long>()(p.state);
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

    /*
    void pattern_matching_lc(const sg_edge *edge) {
        auto statePairs = fsa.getStatePairsWithTransition(edge->label); // (sb, sd)
        for (const auto &sb_sd: statePairs) {
            // forall (sb, sd) where delta(sb, label) = sd
            if (sb_sd.first == 0 && !forest.hasTree(edge->s)) {
                // if sb=0 and there is no tree with root vertex vb
                forest.addTree(edge->id, edge->s, 0, edge->timestamp);
                tree_monitor->registerTree(edge->s, edge->timestamp);
            }
            for (auto tree: forest.findTreesWithNode(edge->s, sb_sd.first)) {
                tree_monitor->accessTree(tree.rootVertex, edge->timestamp);
                // for all Trees that contain ⟨vb,sb⟩
                std::unordered_set<visited_pair, visitedpairHash> visited; // set of visited pairs (vertex, state)
                queue<tree_expansion*> Q; // (⟨vb,sb⟩, <vd,sd>, edge_id)
                Q.push(new tree_expansion (edge->s, sb_sd.first, edge->d, sb_sd.second, edge->id, -1, -1));
                // push (⟨vb,sb⟩, <vd,sd>, edge_id) into Q
                visited.insert((visited_pair){edge->s, sb_sd.first});
                while (!Q.empty()) {
                    auto element = Q.front();
                    Q.pop(); // (⟨vi,si⟩, <vj,sj>, edge_id)
                    if (auto vj_sj_node = forest.findNodeInTree(tree.rootVertex, element->vd, element->sd); !vj_sj_node) {
                        // if tree does not contain <vj,sj>
                        // add <vj,sj> into tree with parent vi_si
                        if (!forest.addChildToParent(tree.rootVertex, element->vb, element->sb, -1,
                                                     element->vd, element->sd)) continue;
                    } else {
                        // if tree already contains <vj,sj> (not descendant of <vi,si>)
                        // add vi_si to the list of candidate parents of <vj,sj>
                        auto candidate_parent = forest.findCandidateParentInTree(tree.rootVertex, element->vb, element->sb, element->vd, element->sd);

                        if (candidate_parent != nullptr) {
                            candidate_parent->isCandidateParent = true;
                            vj_sj_node->candidate_parents.emplace_back(candidate_parent);
                        } else continue;

                    }
                    // update result set
                    if (fsa.isFinalState(element->sd)) {
                        // check if in the result set we already have a path from root to element.vd
                        sink.addEntry(tree.rootVertex, element->vd, edge->timestamp);
                    }
                    // for all vertex <vq,sq> where exists a successor vertex in the snapshot graph where the label the same as the transition in the automaton from the state sj to state sq, push into Q
                    for (auto successors: sg.get_all_suc(element->vd)) {
                        // if the transition exits in the automaton from sj to sq
                        if (auto sq = fsa.getNextState(element->sd, successors.label); sq != -1) {
                            if (visited.count((visited_pair){successors.d, sq}) <= 0) {
                                // if <vq,sq> is not in visited
                                Q.push(new tree_expansion(element->vd, element->sd, successors.d, sq, successors.id, -1, -1));
                                visited.insert((visited_pair){element->vd, element->sd});
                            }
                        }
                    }
                }
            }
        }
    }
    */

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
                priority_queue<tree_expansion*, vector<tree_expansion*>, time_comparator> Q; // (⟨vb,sb⟩, <vd,sd>, edge_id, edge_timestamp) // (⟨vb,sb⟩, <vd,sd>, edge_id, edge_timestamp)
                Q.push(new tree_expansion(edge->s, sb_sd.first, edge->d, sb_sd.second, edge->timestamp, edge->timestamp, edge->timestamp));
                while (!Q.empty()) {
                    auto element = Q.top();
                    Q.pop(); // (⟨vi,si⟩, <vj,sj>, edge_id)
                    auto vj_sj_node = forest.findNodeInTree(tree.rootVertex, element->vd, element->sd);
                    auto vb_sb_node = forest.findNodeInTree(tree.rootVertex, element->vb, element->sb);

                    if (!vj_sj_node) {
                        // if tree does not contain <vj,sj>
                        // add <vj,sj> into tree with parent vi_si
                        if (!forest.addChildToParentTimestamped(tree.rootVertex, vb_sb_node, element->vd, element->sd, element->edge_timestamp)) continue;
                    } else if (vb_sb_node && vj_sj_node->timestamp < (element->edge_timestamp < vb_sb_node->timestamp ? element->edge_timestamp : vb_sb_node->timestamp)) {
                        // if tree already contains <vj,sj>
                        // change parent to vi_si if the timestamp is smaller
                        if (!forest.changeParentTimestamped(vj_sj_node, vb_sb_node, (element->edge_timestamp < vb_sb_node->timestamp ? element->edge_timestamp : vb_sb_node->timestamp), tree.rootVertex)) continue;
                    } else
                        continue;

                    // update result set
                    if (fsa.isFinalState(element->sd)) {
                        // cout << "Found a path from " << element->vb << " to " << element->vd << " at time " << element->edge_timestamp << endl;
                        // check if in the result set we already have a path from root to element.vd
                        sink.addEntry(tree.rootVertex, element->vd, edge->timestamp);
                    }
                    // for all vertex <vq,sq> where exists a successor vertex in the snapshot graph where the label the same as the transition in the automaton from the state sj to state sq, push into Q
                    for (auto successors: sg.get_all_suc(element->vd)) {
                        // if the transition exits in the automaton from sj to sq
                        if (auto sq = fsa.getNextState(element->sd, successors.label); sq != -1) {
                            // if <vq,sq> is not in visited
                            Q.push(new tree_expansion(element->vd, element->sd, successors.d, sq, successors.timestamp, element->d_timestamp, successors.timestamp));
                        }
                    }
                    delete element;
                }
            }
        }
    }


};

#endif //S_PATH_H
