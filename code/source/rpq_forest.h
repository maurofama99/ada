#ifndef RPQ_FOREST_H
#define RPQ_FOREST_H

#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <stack>
#include <algorithm>
#include <queue>

#include "streaming_graph.h"

struct Node {
    long long id;
    long long vertex;
    long long state;
    std::vector<Node *> children;
    Node *parent;
    std::vector<Node *> candidate_parents;
    bool isCandidateParent = false;
    bool isValid = true;
    long long timestamp;
    bool isRoot = false;
    long long timestampRoot = -1;

    Node(long long child_id, long long child_vertex, long long child_state, Node *node) : id(child_id), vertex(child_vertex), state(child_state), parent(node) {}
};

struct Tree {
    long long rootVertex;
    Node *rootNode;
    bool expired;

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


// Custom hash function for std::pair<long long, long long>
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
    std::unordered_map<long long, Tree> trees; // Currently active trees, Key: root vertex, Value: Tree
    std::unordered_map<long long, std::unordered_set<Tree*, TreeHash, TreeEqual> > vertex_tree_map; // Maps vertex to tree to which it belongs to
    std::unordered_map<long long, long long> tree_reference_counting; // Maps a tree (root vertex) to the number of references it has in the vertex tree map

    long long node_count = 0;

    // a vertex can be root of only one tree
    // proof: since we have only one initial state and the tree has an initial state as root
    // it exists only one pair vertex-state that can be root of a tree
    void addTree(long long rootId, long long rootVertex, long long rootState, long long timestamp) {
        if (trees.find(rootVertex) == trees.end()) {
            trees.emplace(rootVertex, Tree(rootVertex, new Node(rootId, rootVertex, rootState, nullptr), false));
            vertex_tree_map[rootVertex].insert(&trees.at(rootVertex));
            tree_reference_counting[rootVertex] = 1;

            trees.at(rootVertex).rootNode->timestamp = INT64_MAX;
            trees.at(rootVertex).rootNode->isRoot = true;
            trees.at(rootVertex).rootNode->timestampRoot = timestamp;

            node_count++;
        } else {
            cout << "Tree already exists with root vertex " << rootVertex << endl;
            exit(1);
        }
    }

    bool addChildToParent(long long rootVertex, long long parentVertex, long long parentState, long long childId, long long childVertex, long long childState) {
        if (Node *parent = findNodeInTree(rootVertex, parentVertex, parentState)) {
            parent->children.push_back(new Node(childId, childVertex, childState, parent));
            node_count++;
            vertex_tree_map[childVertex].insert(&trees.at(rootVertex));
            tree_reference_counting[rootVertex]++;
            return true;
        }
        return false;
    }

    bool addChildToParentTimestamped(long long rootVertex, long long parentVertex, long long parentState, long long childVertex, long long childState, long long timestamp) {
        if (Node *parent = findNodeInTree(rootVertex, parentVertex, parentState)) {
            auto child = new Node(-1, childVertex, childState, parent);
            child->timestamp = timestamp < parent->timestamp ? timestamp : parent->timestamp;
            parent->children.emplace_back(child);
            node_count++;
            vertex_tree_map[childVertex].insert(&trees.at(rootVertex));
            tree_reference_counting[rootVertex]++;
            return true;
        }
        // cout << "Could not find parent in tree" << endl;
        return false;
    }

    bool changeParent(Node *child, Node *newParent) {
        if (!newParent->isValid) {
            return false;
        }
        if (child) {
            // Remove child from its current parent's children list
            if (child->parent) {
                auto &siblings = child->parent->children;
                auto it = std::remove(siblings.begin(), siblings.end(), child);
                if (it == siblings.end()) {
                    throw std::runtime_error("ERROR: Child not found in parent's children list");
                }
                siblings.erase(it, siblings.end());
            } else {
                cout << "ERROR: Could not find new parent in tree" << endl;
                exit(1);
            }
            // Set the new parent
            child->parent = newParent;
            newParent->children.push_back(child);
        } else {
            std::cout << "ERROR: Could not find child in tree" << std::endl;
            exit(1);
        }
        return true;
    }

    bool changeParentTimestamped(Node *child, Node *newParent, long long timestamp) {
        if (!newParent->isValid) {
            return false;
        }
        if (child) {
            // Remove child from its current parent's children list
            if (child->parent) {
                auto &siblings = child->parent->children;
                auto it = std::remove(siblings.begin(), siblings.end(), child);
                if (it == siblings.end()) {
                    throw std::runtime_error("ERROR: Child not found in parent's children list");
                }
                siblings.erase(it, siblings.end());
            } else {
                cout << "ERROR: Could not find new parent in tree" << endl;
                exit(1);
            }
            // Set the new parent
            child->parent = newParent;
            child->timestamp = timestamp < newParent->timestamp ? timestamp : newParent->timestamp;
            newParent->children.push_back(child);
        } else {
            std::cout << "ERROR: Could not find child in tree" << std::endl;
            exit(1);
        }
        return true;
    }

    bool hasTree(long long rootVertex) {
        return trees.find(rootVertex) != trees.end();
    }

    Node *findNodeInTree(long long rootVertex, long long vertex, long long state) {
        Node *root = trees.at(rootVertex).rootNode;
        return searchNode(root, vertex, state);
    }

    Node *findCandidateParentInTree(long long rootVertex, long long candidateparentVertex, long long candidateparentState, long long childVertex, long long childState) {
        Node *root = trees.at(rootVertex).rootNode;
        std::queue<Node *> node_queue;
        node_queue.push(root);

        while (!node_queue.empty()) {
            Node *current = node_queue.front();
            node_queue.pop();

            if (current->vertex == candidateparentVertex && current->state == candidateparentState) {
                return current;
            }

            if (current->vertex == childVertex && current->state == childState) {
                return nullptr;
            }

            for (Node *child: current->children) {
                node_queue.push(child);
            }
        }
        return nullptr;
    }

    /**
     * @param vertex id of the vertex in the AL
     * @param state state associated to the vertex node
     * @return the set of trees to which the node @param vertex @param state belongs to
     */
    std::vector<Tree> findTreesWithNode(long long vertex, long long state) {
        std::vector<Tree> result;
        std::vector<Tree*> treesEntryToDelete;
        if (vertex_tree_map.count(vertex)) {
            for (auto &tree: vertex_tree_map.at(vertex)) {
                // catch a dangling tree, decrease reference counter
                if (!tree->rootNode->isValid) {
                    // remove the tree from the vertex tree map
                    tree_reference_counting.at(tree->rootVertex)--;
                    treesEntryToDelete.push_back(tree);
                } else if (searchNode(tree->rootNode, vertex, state))
                    result.emplace_back(tree->rootVertex, tree->rootNode, tree->expired);
            }
        }
        for (auto tree : treesEntryToDelete) {
            vertex_tree_map.at(vertex).erase(tree);
        }
        return result;
    }

    void expire(const std::vector<pair<long long, long long> > &candidate_edges) {
        for (auto [src, dst]: candidate_edges) {
            std::vector<long long> vertexes = {src, dst};
            for (auto vertex: vertexes) {
                if (vertex_tree_map.find(vertex) == vertex_tree_map.end()) continue;
                auto treesSet = vertex_tree_map.at(vertex);
                for (auto tree : treesSet) {
                    if (Node *current = searchNodeNoState(tree->rootNode, vertex)) {
                        if (current->isRoot) {
                            // current vertex is the root node
                            deleteTreeIterative(tree->rootNode, tree->rootVertex);
                            trees.erase(tree->rootVertex);
                        } else if (current->parent){
                            if (!current->candidate_parents.empty()) { // there is at least a new candidate parent before deletion
                                bool parentFound = false;
                                for (auto candidateParent: current->candidate_parents) {
                                    if (changeParent(current, candidateParent)) {
                                        parentFound = true;
                                        current->candidate_parents.pop_back();
                                        break;
                                    }
                                    current->candidate_parents.pop_back();
                                }
                                if (!parentFound) deleteSubTree(current, tree->rootVertex);
                            } else {
                                deleteSubTree(current, tree->rootVertex);
                            }
                        } else {
                            cerr << "ERROR: child node has null parent" << endl;
                            exit(1);
                        }
                    }
                }
                vertex_tree_map.erase(vertex);
            }
        }
    }

    void expire_timestamped(long long eviction_time, const std::vector<pair<long long, long long> > &candidate_edges) {
        std::vector<Tree*> treesToDelete;
        for (auto [src, dst]: candidate_edges) {
            std::vector<long long> vertexes = {src, dst};
            for (auto vertex: vertexes) {
                if (vertex_tree_map.find(vertex) == vertex_tree_map.end()) continue;
                auto treesSet = vertex_tree_map.at(vertex);
                // defer deletion to avoid use-after-free
                for (auto tree : treesSet) {
                    if (Node *current = searchNodeNoState(tree->rootNode, vertex)) {
                        if (!current->parent && !current->isRoot) cerr << "WARNING: root node has null parent" << endl;
                        if (current->timestamp < eviction_time || (current->isRoot && current->timestampRoot < eviction_time)) {
                            if (!current->isRoot) {
                                deleteSubTree(current, tree->rootVertex);
                            } else { // current vertex is the root node
                                if (current != tree->rootNode) {
                                    cerr << "ERROR: root node is not the same as the current node" << endl;
                                }
                                tree->expired = true;
                                current->isValid = false;
                                deleteTreeRecursive(tree->rootNode, tree->rootVertex);
                                treesToDelete.push_back(tree);
                            }
                        }
                    }
                }
                vertex_tree_map.erase(vertex);
            }
        }
        for (auto tree : treesToDelete) {
            if (auto it = std::find_if(trees.begin(), trees.end(), [&](const auto& pair) { return pair.second.rootVertex == tree->rootVertex; }); it != trees.end()) {
                // complete garbage collection
                if (tree_reference_counting.at(it->first)<0) cout << "WARNING: negative tree reference counter" << endl;
                if (tree_reference_counting.at(it->first)<=0) trees.erase(it);
            }
        }
        treesToDelete.clear();
    }

    // get the trees root vertexes of a vertex
    std::vector<long long> getTreesRootVertex(long long vertex) {
        std::vector<long long> result;
        if (vertex_tree_map.find(vertex) == vertex_tree_map.end()) return result;

        auto treesSet = vertex_tree_map.at(vertex);
        for (auto tree : treesSet) {
            result.push_back(tree->rootVertex);
        }
        if (result.empty()) {
            std::cout << "WARNING: no trees found for vertex " << vertex << std::endl;
        }
        return result;
    }

    void printTree(Node *node, long long depth = 0) const {
        if (!node) return;
        for (long long i = 0; i < depth; ++i) std::cout << "  ";
        std::cout << "Node (id: " << node->id << ", vertex: " << node->vertex << ", state: " << node->state << ")\n";
        for (Node *child: node->children) {
            printTree(child, depth + 1);
        }
    }

    void printForest() const {
        if (trees.empty()) {
            std::cout << "Empty forest\n";
            return;
        }
        for (const auto &pair: trees) {
            std::cout << "Tree with root vertex: " << pair.first << "\n";
            printTree(pair.second.rootNode);
            std::cout << std::endl;
        }
    }

private:

    Node *searchNode(Node *node, long long vertex, long long state) {
        if (!node) return nullptr;
        if (node->vertex == vertex && node->state == state) return node;
        for (Node *child: node->children) {
            Node *found = searchNode(child, vertex, state);
            if (found) return found;
        }
        return nullptr;
    }

    Node *searchNodeNoState(Node *node, long long vertex) {
        if (!node) return nullptr;
        if (!node->isValid) return nullptr;
        if (node->vertex == vertex) return node;
        for (Node *child: node->children) {
            Node *found = searchNodeNoState(child, vertex);
            if (found) return found;
        }
        return nullptr;
    }

    void deleteSubTree(Node *node, long long rootVertex) {
        if (!node) return;

        // remove the node from the parent's children list
        if (node->parent) {
            auto &siblings = node->parent->children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), node), siblings.end());
        } else {
            std::cout << "WARNING: Deleting from root node in a subtree deletion procedure" << std::endl;
        }

        // deleteTreeIterative(node, rootVertex);
        deleteTreeRecursive(node, rootVertex);
    }

    void deleteTreeIterative(Node *node, long long rootVertex) {
        if (!node) return;
        std::stack<Node *> stack;
        stack.push(node);
        while (!stack.empty()) {
            Node *current = stack.top();
            stack.pop();
            for (Node *child: current->children) {
                stack.push(child);
            }
            // if the vertex_tree_map at current->vertex contains the tree with rootVertex, remove it
            // remove from the vertex tree map at current vertex entry the tree which root is rootVertex
            auto& treeSet = vertex_tree_map[current->vertex];
            for (auto it = treeSet.begin(); it != treeSet.end(); ++it) {
                if ((*it)->rootVertex == rootVertex) {
                    treeSet.erase(it);
                    break;  // Exit after one match
                }
            }

            // if the vertex_tree_map at current->vertex is empty, remove the entry
            if (vertex_tree_map.at(current->vertex).empty()) {
                vertex_tree_map.erase(current->vertex);
            }
            if (!current->isCandidateParent) {
                delete current;
            }
            else {
                current->isValid = false;
            }
            node_count--;
        }
    }

    void deleteTreeRecursive(Node *node, long long rootVertex) {
        if (!node) return;
        for (Node *child: node->children) {
            deleteTreeRecursive(child, rootVertex);
        }
        auto& treeSet = vertex_tree_map[node->vertex];
        for (auto it = treeSet.begin(); it != treeSet.end(); ++it) {
            if ((*it)->rootVertex == rootVertex) {
                tree_reference_counting.at(rootVertex)--;
                treeSet.erase(it);
                break;  // Exit after one match
            }
        }

        // if the vertex_tree_map at current->vertex is empty, remove the entry
        if (vertex_tree_map[node->vertex].empty()) {
            vertex_tree_map.erase(node->vertex);
        }
        if (!node->isRoot) delete node;
        node_count--;
    }
};

#endif //RPQ_FOREST_H
