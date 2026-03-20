#ifndef RPQ_FOREST_H
#define RPQ_FOREST_H

#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <algorithm>
#include <set>

#include "fsa.h"
#include "streaming_graph.h"

using namespace std;

struct Node {
    long long id;
    long long vertex;
    long long state;
    Node* child = nullptr;    // first child
    Node* brother = nullptr;  // next sibling (next child of same parent)
    Node* parent = nullptr;
    bool isCandidateParent = false;
    bool isValid = true;
    long long timestamp{};
    long long expiration_time{};
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
    std::unordered_map<std::pair<long long, long long>, std::set<long long>, pair_hash> vertex_state_tree_map; // Maps a pair (vertex, state) to the tree (root vertex) it belongs to

    long long node_count = 0;
    int trees_count = 0;
    long long current_time = 1;

    FiniteStateAutomaton &fsa;

    Forest(FiniteStateAutomaton &fsa) : fsa(fsa) {}

    // it exists only one pair vertex-state that can be root of a tree
    void addTree(const long long rootId, long long rootVertex, long long rootState, long long timestamp) {
        if (trees.find(rootVertex) == trees.end()) {
            trees.emplace(rootVertex, Tree(rootVertex, new Node(rootId, rootVertex, rootState, nullptr), false));

            vertex_state_tree_map[pair(rootVertex, rootState)].insert(rootVertex);
            Node* rootNode = trees.at(rootVertex).rootNode;
            rootNode->timestamp = INT64_MAX;
            rootNode->expiration_time = INT64_MAX;
            rootNode->isRoot = true;
            rootNode->timestampRoot = timestamp;
            rootNode->insertion_cost = 0;

            trees.at(rootVertex).node_map[rootState][rootVertex] = rootNode;

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
        child->brother = parent->child;  // new node's brother = old first child
        parent->child = child;           // parent now points to new node
        node_count++;

        if (trees.find(rootVertex) == trees.end()) {
            cout << "ERROR: Tree with root vertex " << rootVertex << " not found" << endl;
            exit(1);
        }

        trees.at(rootVertex).node_map[childState][childVertex] = child;
        vertex_state_tree_map[pair(childVertex, childState)].insert(rootVertex);

        return true;
    }

    bool changeParentTimestamped(Node *child, Node *newParent, long long timestamp, long long rootVertex, long long newExpiry) {

        if (!newParent->isValid) return false;

        // remove the child from its current parent's children list
        if (child->parent) {
            detachFromParent(child);
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
        child->brother = newParent->child;  // new node's brother = old first child
        newParent->child = child;

        return true;
    }

    bool hasTree(long long rootVertex) {
        return trees.find(rootVertex) != trees.end();
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

    std::vector<long long> findTreeRootsWithNode(long long vertex, long long state) {
        std::vector<long long> result;
        auto key = std::make_pair(vertex, state);
        if (vertex_state_tree_map.count(key))
            for (auto rv : vertex_state_tree_map.at(key))
                if (trees.count(rv) && trees.at(rv).rootNode->isValid)
                    result.push_back(rv);
        return result;
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
                    if (Node *root = tree_it->second.rootNode; root->child == nullptr) {
                        // Clean up all remaining index entries for the root node itself
                        vertex_state_tree_map[{rootVertex, root->state}].erase(rootVertex);
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
    }

private:
    void detachFromParent(Node* node) {
        if (!node->parent) return;

        if (Node* parent = node->parent; parent->child == node) {
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

            cur->isValid = false;
            if (!cur->isRoot) {
                node_count--;
                delete cur;
            }
        }
    }

};


#endif //RPQ_FOREST_H
