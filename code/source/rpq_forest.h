#ifndef RPQ_FOREST_H
#define RPQ_FOREST_H

#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <stack>
#include <algorithm>
#include <set>

#include "fsa.h"
#include "streaming_graph.h"
#include "ranking/buckets.h"
#include "ranking/vertex_cost_manager.h"

using namespace std;

struct Node {
    long long id;
    long long vertex;
    long long state;
    Node* child = nullptr;    // first child
    Node* brother = nullptr;  // next sibling (next child of same parent)
    Node* parent = nullptr;
    //std::vector<Node *> children;
    bool isCandidateParent = false;
    bool isValid = true;
    long long timestamp;
    long long expiration_time;
    bool isRoot = false;
    long long timestampRoot = -1;

    int final_state_descendants = 0;
    int insertion_cost = 0;

    Node(long long child_id, long long child_vertex, long long child_state, Node *parent) : id(child_id), vertex(child_vertex), state(child_state), parent(parent) {}
};

struct Tree {
    long long rootVertex;
    Node *rootNode;
    bool expired;
    std::unordered_map<long long, std::unordered_map<long long, Node*>> node_map; // node_map[state][vertex] = Node*

    bool operator==(const Tree &other) const {
        return rootVertex == other.rootVertex;
    }

    Tree(long long root_vertex, Node *node, bool cond) : rootVertex(root_vertex), rootNode(node), expired(cond) {}

};

struct TreeHash {
    size_t operator()(const Tree* p) const {
        return std::hash<long long>()(p->rootVertex);
    }
};

struct TreeEqual {
    bool operator()(const Tree* a, const Tree* b) const {
        return a->rootVertex == b->rootVertex;
    }
};

struct NodePtrHash {
    size_t operator()(const std::pair<Node*, Node*>& p) const {
        // Usa vertex e state di entrambi i Node* per l'hash
        return std::hash<long long>()(p.first->vertex) ^ (std::hash<long long>()(p.first->state) << 1)
             ^ (std::hash<long long>()(p.second->vertex) << 2) ^ (std::hash<long long>()(p.second->state) << 3);
    }
};

struct NodePtrEqual {
    bool operator()(const std::pair<Node*, Node*>& a, const std::pair<Node*, Node*>& b) const {
        return a.first->vertex == b.first->vertex &&
               a.first->state == b.first->state &&
               a.second->vertex == b.second->vertex &&
               a.second->state == b.second->state;
    }
};

struct pair_hash {
    template<class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2> &p) const {
        auto hash1 = std::hash<T1>()(p.first);
        auto hash2 = std::hash<T2>()(p.second);
        return hash1 ^ (hash2 << 1); // Combine the two hash values
    }
};


class Forest {
public:
    std::unordered_map<long long, Tree> trees; // Key: root vertex, Value: Tree
    //std::unordered_map<long long, std::unordered_set<Tree*, TreeHash, TreeEqual> > vertex_tree_map; // Maps vertex to tree to which it belongs to
    std::unordered_map<std::pair<long long, long long>, std::set<long long>, pair_hash> vertex_state_tree_map; // Maps a pair (vertex, state) to the tree (root vertex) it belongs to
    //std::unordered_map<long long, long long> tree_reference_counting; // Maps a tree (root vertex) to the number of references it has in the vertex tree map

    VertexCostManager vertex_cost_manager;
    std::unordered_map<long long, double> vertex_contribution;

    long long node_count = 0;
    int trees_count = 0;
    long long current_time = 1;

    RankBuckets rank;

    // Deferred deletion: nodes are marked invalid and collected here,
    // then actually freed at the end of expire_timestamped.
    std::vector<Node*> pending_deletes;

    FiniteStateAutomaton &fsa;

    Forest(FiniteStateAutomaton &fsa) : rank(RankBuckets(2601977, 36233450)), fsa(fsa) {}

    [[nodiscard]] double computeVertexRank(long long vertex_id) const {
        double contribution = vertex_contribution.count(vertex_id) ? vertex_contribution.at(vertex_id) : 0;
        double cost = vertex_cost_manager.getAverageCost(vertex_id);
        return contribution/cost+1;
    }

    // Try to insert or update an edge in the capped bottom-K set
    void updateVertexRank(long long vertex_id) {
        return;
        rank.set_rank(vertex_id, computeVertexRank(vertex_id));
    }

    // Remove from ranking
    void removeVertexFromRanking(long long vertex_id) {
        return;
        rank.remove(vertex_id);
        // remove the entry from vertex cost
        vertex_cost_manager.removeEntry(vertex_id);
        // erase entry from vertex contribution
        vertex_contribution.erase(vertex_id);
    }

    [[nodiscard]] vector<RankBuckets::Id> getTopKVertices(size_t k) const {
        return rank.top_k(k);
    }

    void propagateFinalStateCount(Node* node, int delta) {
        return;
        Node* current = node->parent;
        while (current) {
            current->final_state_descendants += delta;
            vertex_contribution[current->vertex] += delta;
            updateVertexRank(current->vertex);
            current = current->parent;
        }
    }

    int countFinalStatesInSubtree(Node* node, long long rootVertex) {

        if (!node) return 0;
        int count = fsa.isFinalState(node->state) ? 1 : 0;
        //for (Node* child : node->children) {
        for (Node* child = node->child; child != nullptr; child = child->brother){
            child->insertion_cost = child->parent->insertion_cost+1;
            vertex_cost_manager.insertCost(child->vertex, rootVertex, child->insertion_cost);
            updateVertexRank(child->vertex);
            count += countFinalStatesInSubtree(child, rootVertex);
        }
        return count;
    }

    // it exists only one pair vertex-state that can be root of a tree
    void addTree(const long long rootId, long long rootVertex, long long rootState, long long timestamp) {
        if (trees.find(rootVertex) == trees.end()) {
            trees.emplace(rootVertex, Tree(rootVertex, new Node(rootId, rootVertex, rootState, nullptr), false));

            // vertex_tree_map[rootVertex].insert(&trees.at(rootVertex));
            // tree_reference_counting[rootVertex] = 1;

            vertex_state_tree_map[pair(rootVertex, rootState)].insert(rootVertex);
            Node* rootNode = trees.at(rootVertex).rootNode;
            rootNode->timestamp = INT64_MAX;
            rootNode->expiration_time = INT64_MAX;
            rootNode->isRoot = true;
            rootNode->timestampRoot = timestamp;
            rootNode->insertion_cost = 0;

            trees.at(rootVertex).node_map[rootState][rootVertex] = rootNode;

            vertex_cost_manager.insertCost(rootVertex, rootVertex, 0);
            updateVertexRank(rootVertex);

            node_count++;
            trees_count++;
        } else {
            cout << "Tree already exists with root vertex " << rootVertex << endl;
            exit(1);
        }
    }

    bool addChildToParentTimestamped(long long rootVertex, Node* parent, long long childVertex, long long childState, long long timestamp, long long newExpiry) {
        if (!parent) return false;

        auto child = new Node(parent->id, childVertex, childState, parent);
        if (parent->isRoot) {
            child->timestamp = timestamp;
        } else {
            child->timestamp = std::max(timestamp, parent->timestamp);
        }
        child->expiration_time = newExpiry;
        //parent->children.emplace_back(child);
        // attach new_node as a child of parent
        //child->parent = parent;
        child->brother = parent->child;  // new node's brother = old first child
        parent->child = child;           // parent now points to new node
        node_count++;

        // if (fsa.isFinalState(childState)) {
        //     propagateFinalStateCount(child, +1);
        // }

        if (trees.find(rootVertex) == trees.end()) {
            cout << "ERROR: Tree with root vertex " << rootVertex << " not found" << endl;
            exit(1);
        }

        trees.at(rootVertex).node_map[childState][childVertex] = child;
        vertex_state_tree_map[pair(childVertex, childState)].insert(rootVertex);

        // vertex_tree_map[child->vertex].insert(&trees.at(rootVertex));
        // tree_reference_counting[rootVertex]++;

        // child->insertion_cost = parent->insertion_cost + 1;
        // vertex_cost_manager.insertCost(child->vertex, rootVertex, child->insertion_cost);
        //updateVertexRank(child->vertex);

        return true;
    }

    bool changeParentTimestamped(Node *child, Node *newParent, long long timestamp, long long rootVertex, long long newExpiry) {

        if (!newParent->isValid) return false;

        //child->insertion_cost = newParent->insertion_cost+1;
        // vertex_cost_manager.insertCost(child->vertex, rootVertex, child->insertion_cost);
        // updateVertexRank(child->vertex);

        // int finalCount = countFinalStatesInSubtree(child, rootVertex);
        //
        // Node* old = child->parent;
        // while (finalCount>0 && old) {
        //     old->final_state_descendants -= finalCount;
        //     vertex_contribution[old->vertex] -= finalCount;
        //     updateVertexRank(old->vertex);
        //     old = old->parent;
        // }

        // remove the child from its current parent's children list
        if (child->parent) {
            detachFromParent(child);
            // auto &siblings = child->parent->children;
            // auto it = std::remove(siblings.begin(), siblings.end(), child);
            // if (it == siblings.end()) {
            //     throw std::runtime_error("ERROR: Child not found in parent's children list");
            // }
            // siblings.erase(it, siblings.end());
        } else {
            cout << "ERROR: Could not find parent in tree" << endl;
            exit(1);
        }

        // Set the new parent
        child->parent = newParent;
        if (newParent->isRoot) {
            child->timestamp = timestamp;
        } else {
            child->timestamp = std::max(timestamp, newParent->timestamp);
        }
        child->expiration_time = newExpiry;

        //newParent->children.push_back(child);
        //child->parent = newParent;
        child->brother = newParent->child;  // new node's brother = old first child
        newParent->child = child;

        // Node* cur = newParent;
        // while (cur) {
        //     cur->final_state_descendants += finalCount;
        //     vertex_contribution[cur->vertex] += finalCount;
        //     updateVertexRank(cur->vertex);
        //     cur = cur->parent;
        // }

        return true;
    }

    bool hasTree(long long rootVertex) {
        return trees.find(rootVertex) != trees.end();
    }

    void detachFromParent(Node* node) {
        if (!node->parent) return;

        Node* parent = node->parent;
        if (parent->child == node) {
            // node is the first child — just advance the head
            parent->child = node->brother;
        } else {
            // scan siblings to find the predecessor
            Node* prev = parent->child;
            while (prev && prev->brother != node)
                prev = prev->brother;
            if (prev)
                prev->brother = node->brother;
        }
        node->parent = nullptr;
        node->brother = nullptr;
    }

    Node *findNodeInTreeBFS(long long rootVertex, long long vertex, long long state) {
        Node *root = trees.at(rootVertex).rootNode;
        auto [node, hops] = searchNodeWithDepth(root, vertex, state);
        if (node) {
            if (hops != node->insertion_cost) {
                cerr << "WARNING: node " << node->vertex << " has insertion cost " << node->insertion_cost << " but is found at depth " << hops << endl;
                vertex_cost_manager.insertCost(node->vertex, rootVertex, node->insertion_cost);
                updateVertexRank(node->vertex);
            }
        }
        return node;
        // return searchNode(root, vertex, state);
    }

    Node *findNodeInTree(long long rootVertex, long long vertex, long long state) {
        auto tree_it = trees.find(rootVertex);
        if (tree_it == trees.end()) return nullptr;

        auto& nm = tree_it->second.node_map;
        auto s_it = nm.find(state);
        if (s_it == nm.end()) return nullptr;
        auto v_it = s_it->second.find(vertex);
        if (v_it == s_it->second.end()) return nullptr;
        // Guard: if the node was invalidated (pending delete), treat as absent
        Node* node = v_it->second;
        if (!node->isValid) return nullptr;

        return node;
    }

    std::vector<Tree> findTreesWithNodeInstance(long long vertex, long long state) {
        std::vector<Tree> result;
        auto key = std::pair<long long, long long>(vertex, state);
        if (vertex_state_tree_map.count(key)) {
            for (auto rootVertex : vertex_state_tree_map.at(key)) {
                if (trees.count(rootVertex) && trees.at(rootVertex).rootNode->isValid) {
                    auto& t = trees.at(rootVertex);
                    result.emplace_back(t.rootVertex, t.rootNode, t.expired);
                }
            }
        }
        return result;
    }

    std::vector<long long> findTreeRootsWithNode(long long vertex, long long state) {
        std::vector<long long> result;
        auto key = std::make_pair(vertex, state);
        if (vertex_state_tree_map.count(key))
            for (auto rv : vertex_state_tree_map.at(key))
                if (trees.count(rv) && trees.at(rv).rootNode->isValid)
                    result.push_back(rv);
        return result;
    }

    void expire_per_tree(long long v, long long state, long long rootVertex, long long eviction_time) {
        auto tree_it = trees.find(rootVertex);
        if (tree_it == trees.end()) return;
        Tree& tree = tree_it->second;

        // O(1) lookup via node_map — no tree traversal
        auto s_it = tree.node_map.find(state);
        if (s_it == tree.node_map.end()) return;

        auto v_it = s_it->second.find(v);
        if (v_it == s_it->second.end()) return;

        Node* node = v_it->second;
        if (!node->isValid) return;

        // Check expiry: the node's interval has closed before the eviction threshold
        if (node->expiration_time < eviction_time) {
            // deleteSubTree disconnects from parent, cleans all indexes, marks invalid
            deleteSubTree(node, rootVertex);
        }
    }

    void expire_forest(long long eviction_time, const std::vector<streaming_graph::expired_edge_info>& deleted_edges) {
        // visited set: avoid processing the same (vertex, state) product-node twice
        // across multiple expired edges in the same expiry round
        std::unordered_set<long long> visited; // encodes (vertex << 16 | state)
        //auto encode = [](long long v, long long s) { return (v << 16) | s; };
        auto encode = [](long long v, long long s) {
            return static_cast<long long>((static_cast<unsigned long long>(v) << 32) | static_cast<unsigned long long>(s));
        };

        for (const auto &e: deleted_edges) {
            // Only the destination of an expired edge can head an expired subtree.
            // The source vertex is a parent in the tree — its subtree is checked
            // when its OWN incoming edges expire.
            long long dst = e.dst;

            // Use the edge label to get only the (src_state, dst_state) pairs
            // that are actually reachable via this label — no need to scan all states
            auto state_pairs = fsa.getStatePairsWithTransition(e.label);

            for (const auto &[sb, sd]: state_pairs) {
                if (visited.count(encode(dst, sd))) continue;
                visited.insert(encode(dst, sd));

                // Find all trees that contain the product-graph node (dst, sd)
                auto key = std::make_pair(dst, sd);
                if (!vertex_state_tree_map.count(key)) continue;

                // Copy root IDs: expire_per_tree may modify vertex_state_tree_map
                // during deleteSubTree, invalidating the iterator
                std::vector<long long> roots(
                    vertex_state_tree_map.at(key).begin(),
                    vertex_state_tree_map.at(key).end()
                );

                for (long long rootVertex: roots) {
                    if (!trees.count(rootVertex)) continue;

                    expire_per_tree(dst, sd, rootVertex, eviction_time);

                    // If the root has no children left, the tree is empty:
                    // all paths from this source have expired, delete the tree
                    auto tree_it = trees.find(rootVertex);
                    if (tree_it == trees.end()) continue; // already gone
                    Node *root = tree_it->second.rootNode;
                    //if (root->children.empty()) {
                     if (root->child == nullptr) {
                        // Clean up all remaining index entries for the root node itself
                        vertex_state_tree_map[{rootVertex, root->state}].erase(rootVertex);
                        // vertex_tree_map[rootVertex].erase(&tree_it->second);
                        // tree_reference_counting.erase(rootVertex);
                        // node_map is stored inside Tree, freed with it
                        root->isValid = false;
                        node_count--;
                        trees_count--;
                        delete root;
                        trees.erase(tree_it);
                    }
                }
            }
        }

        //flushPendingDeletes();
    }

    void expire_timestamped(long long eviction_time, const std::vector<pair<long long, long long> > &candidate_edges) {
        // std::vector<Tree*> treesToDelete;
        // for (auto [src, dst]: candidate_edges) {
        //     std::vector<long long> vertexes = {src, dst};
        //     for (auto vertex: vertexes) {
        //         //if (vertex_tree_map.find(vertex) == vertex_tree_map.end()) continue;
        //         //auto treesSet = vertex_tree_map.at(vertex);
        //         for (int i = 0; i < fsa.states_count; ++i) {
        //             auto suppl_treesSet = vertex_state_tree_map.count(pair(vertex, i)) ? vertex_state_tree_map.at(pair(vertex, i)) : std::set<long long>();
        //             for (auto tree : suppl_treesSet) {
        //                 if (trees.find(tree) == trees.end()) {
        //                     vertex_state_tree_map.at(pair(vertex, i)).erase(tree);
        //                 } //else if (treesSet.find(&trees.at(tree)) == treesSet.end()) {
        //                     //treesSet.insert(&trees.at(tree));
        //                 //}
        //             }
        //         }
        //         // defer deletion to avoid use-after-free
        //          for (auto tree : treesSet) {
        //              std::unordered_set<pair<Node*, Node*>, NodePtrHash, NodePtrEqual > sub_to_delete; // node, root
        //              std::unordered_set<pair<Node*, Node*>, NodePtrHash, NodePtrEqual > trees_to_delete;
        //              for (auto current: searchAllNodesNoState(tree->rootNode, vertex)) {
        //                  if (!current->parent && !current->isRoot)
        //                      cerr << "WARNING: root node has null parent" << endl;
        //                  if (current->expiration_time < eviction_time || (current->isRoot && current->timestampRoot < eviction_time)) {
        //                      if (!current->isRoot) {
        //                          sub_to_delete.emplace(current, tree->rootNode);
        //                      } else { // current vertex is the root node
        //                          if (current != tree->rootNode) {
        //                              cerr << "ERROR: root node is not the same as the current node" << endl;
        //                          }
        //                          tree->expired = true;
        //                          current->isValid = false;
        //                          trees_to_delete.emplace(tree->rootNode, tree->rootNode);
        //                          treesToDelete.push_back(tree);
        //                          trees_count--;
        //                          node_count--;
        //                      }
        //                  }
        //              }
        //              for (auto [node, rootVertex]: sub_to_delete) {
        //                  deleteSubTreeFromRootIterative(rootVertex, node);
        //              }
        //              for (auto [node, rootVertex]: trees_to_delete) {
        //                  deleteSubTreeFromRootIterative(rootVertex, node);
        //              }
        //              sub_to_delete.clear();
        //              trees_to_delete.clear();
        //         }
        //     }
        // }
        // for (auto tree : treesToDelete) {
        //     if (auto it = std::find_if(trees.begin(), trees.end(), [&](const auto& pair) { return pair.second.rootVertex == tree->rootVertex; }); it != trees.end()) {
        //         // complete garbage collection
        //         if (tree_reference_counting.at(it->first)<0) cout << "WARNING: negative tree reference counter" << endl;
        //         if (tree_reference_counting.at(it->first)<=0) trees.erase(it);
        //     }
        // }
        // treesToDelete.clear();
        //
        // // Flush deferred deletions now that all traversals are complete
        // flushPendingDeletes();
    }

    void flushPendingDeletes() {
        for (Node* node : pending_deletes) {
            delete node;
        }
        pending_deletes.clear();
    }


private:

    // Node *searchNode(Node *node, long long vertex, long long state) {
    //     if (!node) return nullptr;
    //     if (!node->isValid) return nullptr;
    //     if (node->vertex == vertex && node->state == state) return node;
    //     for (Node *child: node->children) {
    //         Node *found = searchNode(child, vertex, state);
    //         if (found) return found;
    //     }
    //     return nullptr;
    // }

    std::pair<Node *, int> searchNodeWithDepth(Node *root, long long vertex, long long state) {
        // NOT WORKING

        if (!root) return {nullptr, -1};

        std::stack<std::pair<Node *, int> > stack; // (node, depth)
        stack.emplace(root, 0);

        while (!stack.empty()) {
            auto [current, depth] = stack.top();
            stack.pop();

            if (!current) continue;
            //if (!current->isValid) continue;

            if (current->vertex == vertex && current->state == state) {
                return {current, depth};
            }

            // for (Node *child: current->children) {
            //     stack.emplace(child, depth + 1);
            // }
        }

        return {nullptr, -1};
    }

    // Node *searchNodeNoState(Node *node, long long vertex) {
    //     if (!node) return nullptr;
    //     if (!node->isValid) return nullptr;
    //     if (node->vertex == vertex) return node;
    //     for (Node *child: node->children) {
    //         if (Node *found = searchNodeNoState(child, vertex)) return found;
    //     }
    //     return nullptr;
    // }

    std::vector<Node *> searchAllNodesNoState(Node *node, long long vertex) {
        std::vector<Node *> foundNodes;
        if (!node) return foundNodes;
        if (!node->isValid) return foundNodes;
        if (node->vertex == vertex) foundNodes.push_back(node);
        //for (Node *child: node->children) {
        for (Node* child = node->child; child != nullptr; child = child->brother) {
            auto childFound = searchAllNodesNoState(child, vertex);
            foundNodes.insert(foundNodes.end(), childFound.begin(), childFound.end());
        }
        return foundNodes;
    }

    void deleteSubTreeFromRootIterative(Node *root, Node *target) {
        if (!root || !target) return;
        if (!target->isValid) return; // already invalidated by a previous deletion

        std::stack<Node*> stack;
        stack.push(root);

        while (!stack.empty()) {
            Node* current = stack.top();
            stack.pop();

            if (!current->isValid) continue;
            if (current == target) {
                deleteSubTree(current, root->vertex);
                return;
            }

            //for (Node* child : current->children) {
            for (Node* child = current->child; child != nullptr; child = child->brother) {
                stack.push(child);
            }
        }
    }

    void deleteSubTree(Node* node, long long rootVertex) {
        if (!node) return;
        // Only disconnect from parent — index cleanup is done in deleteSubTreeBFS
        if (node->parent) {
            detachFromParent(node);
        }
        deleteSubTreeBFS(node, rootVertex);
    }

    void deleteSubTreeBFS(Node* node, long long rootVertex) {
        std::queue<Node*> q;
        q.push(node);

        while (!q.empty()) {
            Node* cur = q.front();
            q.pop();

            // enqueue all children before we touch this node
            for (Node* c = cur->child; c != nullptr; c = c->brother)
                q.push(c);

            // auto& treeSet = vertex_tree_map[cur->vertex];
            // for (auto it = treeSet.begin(); it != treeSet.end(); ++it) {
            //     if ((*it)->rootVertex == rootVertex) {
            //         tree_reference_counting.at(rootVertex)--;
            //         treeSet.erase(it);
            //         break;
            //     }
            // }
            auto &treeRootsSet = vertex_state_tree_map[pair(cur->vertex, cur->state)];
            for (auto it = treeRootsSet.begin(); it != treeRootsSet.end(); ++it) {
                if (*it == rootVertex) {
                    treeRootsSet.erase(it);
                    break;
                }
            }
            if (trees.count(rootVertex)) {
                auto& nm = trees.at(rootVertex).node_map;
                auto s_it = nm.find(cur->state);
                if (s_it != nm.end()) {
                    s_it->second.erase(cur->vertex);
                    if (s_it->second.empty())
                        nm.erase(s_it);
                }
            }

            // if (fsa.isFinalState(cur->state)) {
            //     propagateFinalStateCount(cur, -1);
            // }
            // vertex_cost_manager.removeCost(cur->vertex, rootVertex);

            cur->isValid = false;
            if (!cur->isRoot) {
                node_count--;
                //pending_deletes.push_back(cur);
                delete cur;
            }
        }
    }

    void deleteSubTreeRecursive(Node *node, long long rootVertex) {
        if (!node) return;

        // std::vector<Node*> childrenCopy = node->children;
        // node->children.clear();

        for (Node* child = node->child; child != nullptr; child = child->brother) {
            deleteSubTreeRecursive(child, rootVertex);
        }
        // auto& treeSet = vertex_tree_map[node->vertex];
        // for (auto it = treeSet.begin(); it != treeSet.end(); ++it) {
        //     if ((*it)->rootVertex == rootVertex) {
        //         tree_reference_counting.at(rootVertex)--;
        //         treeSet.erase(it);
        //         break;
        //     }
        // }
        auto &treeRootsSet = vertex_state_tree_map[pair(node->vertex, node->state)];
        for (auto it = treeRootsSet.begin(); it != treeRootsSet.end(); ++it) {
            if (*it == rootVertex) {
                treeRootsSet.erase(it);
                break;
            }
        }
        if (trees.count(rootVertex)) {
            auto& nm = trees.at(rootVertex).node_map;
            auto s_it = nm.find(node->state);
            if (s_it != nm.end()) {
                s_it->second.erase(node->vertex);
                if (s_it->second.empty())
                    nm.erase(s_it);
            }
        }

        // if (fsa.isFinalState(node->state)) {
        //     propagateFinalStateCount(node, -1);
        // }
        // vertex_cost_manager.removeCost(node->vertex, rootVertex);

        node->isValid = false;
        if (!node->isRoot) {
            node_count--;
            pending_deletes.push_back(node);
        } else {
            node->parent = nullptr;
        }
    }
};


#endif //RPQ_FOREST_H
