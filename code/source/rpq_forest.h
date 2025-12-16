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

struct MemoryEstimator {
    // assume 64-bit system
    static constexpr size_t ptr_size = sizeof(void*);
    static constexpr size_t node_overhead = 2 * sizeof(void*); // ~next + bucket pointer
    static constexpr size_t rb_node_overhead = 3 * sizeof(void*) + sizeof(bool); // set/map node

    template <typename T>
    static size_t estimate_unordered_map_ll_tree(
        const std::unordered_map<long long, T>& m)
    {
        size_t total = 0;
        for (const auto& kv : m) {
            total += sizeof(long long);     // key
            total += sizeof(T);             // value object
            total += node_overhead;         // node overhead
        }
        // bucket array
        total += m.bucket_count() * ptr_size;
        return total;
    }

    static size_t estimate_vertex_tree_map(
        const std::unordered_map<long long,
                                 std::unordered_set<Tree*, TreeHash, TreeEqual>>& m)
    {
        size_t total = 0;
        for (const auto& kv : m) {
            total += sizeof(long long);               // key
            total += sizeof(std::unordered_set<Tree*>); // set object
            total += node_overhead;                   // outer node overhead

            // now cost of inner set
            const auto& inner = kv.second;
            for (Tree* ptr : inner) {
                (void)ptr;
                total += sizeof(Tree*);  // payload
                total += node_overhead;  // node overhead
            }
            total += inner.bucket_count() * ptr_size;
        }
        total += m.bucket_count() * ptr_size;
        return total;
    }

    static size_t estimate_vertex_state_tree_map(
        const std::unordered_map<std::pair<long long,long long>,
                                 std::set<long long>,
                                 pair_hash>& m)
    {
        size_t total = 0;
        for (const auto& kv : m) {
            total += sizeof(std::pair<long long,long long>); // key
            total += sizeof(std::set<long long>);            // set object
            total += node_overhead;                         // outer node overhead

            const auto& inner = kv.second;
            for (long long val : inner) {
                (void)val;
                total += sizeof(long long);     // payload
                total += rb_node_overhead;      // pointers in RB-tree node
            }
        }
        total += m.bucket_count() * ptr_size;
        return total;
    }

    // Stima ricorsiva della memoria occupata dal sotto-albero di un Node:
    // include:
    //  - sizeof(Node) (contenitore + oggetti vector)
    //  - memoria dinamica dei vector children e candidate_parents
    //  - ricorsione sui figli
    static size_t estimate_node_subtree(const Node* node) {
        if (!node) return 0;
        size_t total = 0;
        total += sizeof(Node);
        total += node->children.capacity() * sizeof(Node*);
        for (const auto child : node->children) {
            total += estimate_node_subtree(child);
        }
        return total;
    }

    // Stima della unordered_map<long long, Tree>:
    // per ogni entry:
    //  - key (long long)
    //  - oggetto Tree
    //  - overhead nodo hash
    //  - intero sotto-albero a partire da rootNode
    // + bucket array finale
    static size_t estimate_trees(const std::unordered_map<long long, Tree>& m) {
        size_t total = 0;
        for (const auto& kv : m) {
            total += sizeof(long long); // key
            total += sizeof(Tree);      // value
            total += node_overhead;     // nodo della hash table
            total += estimate_node_subtree(kv.second.rootNode);
        }
        total += m.bucket_count() * ptr_size;
        return total;
    }
};

class Forest {
public:
    std::unordered_map<long long, Tree> trees; // Key: root vertex, Value: Tree
    std::unordered_map<long long, std::unordered_set<Tree*, TreeHash, TreeEqual> > vertex_tree_map; // Maps vertex to tree to which it belongs to
    std::unordered_map<std::pair<long long, long long>, std::set<long long>, pair_hash> vertex_state_tree_map; // Maps a pair (vertex, state) to the tree (root vertex) it belongs to
    std::unordered_map<long long, long long> tree_reference_counting; // Maps a tree (root vertex) to the number of references it has in the vertex tree map

    long long node_count = 0;
    int trees_count = 0;

    int possible_states; // possible states in the FSA

    // a vertex can be root of only one tree
    // proof: since we have only one initial state and the tree has an initial state as root
    // it exists only one pair vertex-state that can be root of a tree
    void addTree(const long long rootId, long long rootVertex, long long rootState, long long timestamp) {

        if (trees.find(rootVertex) == trees.end()) {
            trees.emplace(rootVertex, Tree(rootVertex, new Node(rootId, rootVertex, rootState, nullptr), false));

            vertex_tree_map[rootVertex].insert(&trees.at(rootVertex));
            vertex_state_tree_map[pair(rootVertex, rootState)].insert(rootVertex);

            tree_reference_counting[rootVertex] = 1;

            trees.at(rootVertex).rootNode->timestamp = INT64_MAX;
            trees.at(rootVertex).rootNode->isRoot = true;
            trees.at(rootVertex).rootNode->timestampRoot = timestamp;

            node_count++;
            trees_count++;
        } else {
            cout << "Tree already exists with root vertex " << rootVertex << endl;
            exit(1);
        }
    }

    bool addChildToParentTimestamped(long long rootVertex, Node* parent, long long childVertex, long long childState, long long timestamp) {
        if (parent) {
            auto child = new Node(parent->id, childVertex, childState, parent);
            child->timestamp = timestamp < parent->timestamp ? timestamp : parent->timestamp;
            parent->children.emplace_back(child);
            node_count++;
            if (trees.find(rootVertex) == trees.end()) {
                cout << "ERROR: Tree with root vertex " << rootVertex << " not found" << endl;
                exit(1);
            }
            vertex_tree_map[child->vertex].insert(&trees.at(rootVertex));
            vertex_state_tree_map[pair(childVertex, childState)].insert(rootVertex);
            tree_reference_counting[rootVertex]++;
            return true;
        }
        return false;
    }

    bool changeParentTimestamped(Node *child, Node *newParent, long long timestamp, long long rootVertex) {

        if (!newParent->isValid) {
            return false;
        }

        // remove the child from its current parent's children list
        if (child->parent) {
            auto &siblings = child->parent->children;
            auto it = std::remove(siblings.begin(), siblings.end(), child);
            if (it == siblings.end()) {
                throw std::runtime_error("ERROR: Child not found in parent's children list");
            }
            siblings.erase(it, siblings.end());
        } else {
            cout << "ERROR: Could not find parent in tree" << endl;
            exit(1);
        }
        
        // Set the new parent
        child->parent = newParent;
        child->timestamp = timestamp;
        newParent->children.push_back(child);

        return true;
    }

    bool hasTree(long long rootVertex) {
        return trees.find(rootVertex) != trees.end();
    }

    Node *findNodeInTree(long long rootVertex, long long vertex, long long state) {
        Node *root = trees.at(rootVertex).rootNode;
        return searchNode(root, vertex, state);
    }

    /**
     * @param vertex id of the vertex in the AL
     * @param state state associated to the vertex node
     * @return the set of trees to which the node @param vertex @param state belongs to
     */
    std::vector<Tree> findTreesWithNode(long long vertex, long long state) {
        std::vector<Tree> result;
        std::vector<Tree*> treesEntryToDelete;
        std::vector<long long> treesRootEntryToDelete;

         if (vertex_tree_map.count(vertex)) {
            for (auto &tree: vertex_tree_map.at(vertex)) {
                // catch a dangling tree, decrease reference counter
                if (!tree->rootNode->isValid) {
                    // remove the tree from the vertex tree map
                    treesEntryToDelete.push_back(tree);
                    tree_reference_counting.at(tree->rootVertex)--;
                } else if (searchNode(tree->rootNode, vertex, state))
                    result.emplace_back(tree->rootVertex, tree->rootNode, tree->expired);
            }
        }
        for (auto tree : treesEntryToDelete) {
            vertex_tree_map.at(vertex).erase(tree);
        }

        if (vertex_state_tree_map.count(pair(vertex,state))) {
            for (auto tree_root: vertex_state_tree_map.at(pair(vertex,state))) {
                // catch a dangling tree, decrease reference counter
                if (trees.count(tree_root) && !trees.at(tree_root).rootNode->isValid){
                    treesRootEntryToDelete.push_back(tree_root);
                }
            }
        }
        for (auto tree : treesRootEntryToDelete) {
            vertex_state_tree_map.at(pair(vertex,state)).erase(tree);
        }
        return result;
    }

    void expire_timestamped(long long eviction_time, const std::vector<pair<long long, long long> > &candidate_edges) {
        std::vector<Tree*> treesToDelete;
        for (auto [src, dst]: candidate_edges) {
            std::vector<long long> vertexes = {src, dst};
            for (auto vertex: vertexes) {
                if (vertex_tree_map.find(vertex) == vertex_tree_map.end()) continue;
                auto treesSet = vertex_tree_map.at(vertex);
                for (int i = 0; i < possible_states; ++i) {
                    auto suppl_treesSet = vertex_state_tree_map.count(pair(vertex, i)) ? vertex_state_tree_map.at(pair(vertex, i)) : std::set<long long>();
                    for (auto tree : suppl_treesSet) {
                        if (trees.find(tree) == trees.end()) {
                            vertex_state_tree_map.at(pair(vertex, i)).erase(tree);
                        } else if (treesSet.find(&trees.at(tree)) == treesSet.end()) {
                            treesSet.insert(&trees.at(tree));
                        }
                    }
                }
                // defer deletion to avoid use-after-free
                 for (auto tree : treesSet) {
                     std::unordered_set<pair<Node*, Node*>, NodePtrHash, NodePtrEqual > sub_to_delete; // node, root
                     std::unordered_set<pair<Node*, Node*>, NodePtrHash, NodePtrEqual > trees_to_delete;
                     for (auto current: searchAllNodesNoState(tree->rootNode, vertex)) {
                         if (!current->parent && !current->isRoot)
                             cerr << "WARNING: root node has null parent" << endl;
                         if (current->timestamp < eviction_time || (current->isRoot && current->timestampRoot < eviction_time)) {
                             if (!current->isRoot) {
                                 sub_to_delete.emplace(current, tree->rootNode);
                             } else { // current vertex is the root node
                                 if (current != tree->rootNode) {
                                     cerr << "ERROR: root node is not the same as the current node" << endl;
                                 }
                                 tree->expired = true;
                                 current->isValid = false;
                                 trees_to_delete.emplace(tree->rootNode, tree->rootNode);
                                 treesToDelete.push_back(tree);
                                 trees_count--;
                                 node_count--;
                             }
                         }
                     }
                     for (auto [node, rootVertex]: sub_to_delete) {
                         deleteSubTreeFromRootIterative(rootVertex, node);
                     }
                     for (auto [node, rootVertex]: trees_to_delete) {
                         deleteSubTreeFromRootIterative(rootVertex, node);
                     }
                     sub_to_delete.clear();
                     trees_to_delete.clear();
                }
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

    void printTree(Node *node, const std::string& prefix = "", bool isLast = true) const {
        if (!node) return;

        std::cout << prefix;
        std::cout << (isLast ? "└── " : "├── ");
        std::cout << "v:" << node->vertex << ", s:" << node->state << ", ts: " << !node->isRoot ? 0: node->timestamp;
        if (node->isRoot) std::cout << " ROOT";
        if (!node->isValid) std::cout << " INVALID";
        std::cout << ")\n";

        std::string childPrefix = prefix + (isLast ? "    " : "│   ");

        for (size_t i = 0; i < node->children.size(); ++i) {
            bool lastChild = (i == node->children.size() - 1);
            printTree(node->children[i], childPrefix, lastChild);
        }
    }

    void printForest() const {
        if (trees.empty()) {
            std::cout << "╔════════════════╗\n";
            std::cout << "║ Empty Forest   ║\n";
            std::cout << "╚════════════════╝\n";
            return;
        }

        std::cout << "╔════════════════════════════════════════╗\n";
        std::cout << "║ Forest Statistics                      ║\n";
        std::cout << "╠════════════════════════════════════════╣\n";
        std::cout << "║ Trees: " << trees_count << std::string(32 - std::to_string(trees_count).length(), ' ') << "║\n";
        std::cout << "║ Nodes: " << node_count << std::string(32 - std::to_string(node_count).length(), ' ') << "║\n";
        std::cout << "╚════════════════════════════════════════╝\n\n";

        size_t treeIndex = 0;
        for (const auto &pair: trees) {
            std::cout << "Tree #" << ++treeIndex << " [Root Vertex: " << pair.first << "]";
            if (pair.second.expired) std::cout << " (EXPIRED)";
            std::cout << "\n";
            printTree(pair.second.rootNode);
            std::cout << "\n";
        }
    }


    [[nodiscard]] size_t getUsedMemory() const {
        size_t total = 0;

        // Memoria delle trees (inclusi nodi interni)
        total += MemoryEstimator::estimate_trees(trees);
        //
        // // Memory used by vertex_tree_map
        // total += MemoryEstimator::estimate_vertex_tree_map(vertex_tree_map);
        //
        // // Memory used by vertex_state_tree_map
        // total += MemoryEstimator::estimate_vertex_state_tree_map(vertex_state_tree_map);
        //
        // // Memory used by tree_reference_counting
        // total += MemoryEstimator::estimate_unordered_map_ll_tree(tree_reference_counting);

        return total;
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
            if (Node *found = searchNodeNoState(child, vertex)) return found;
        }
        return nullptr;
    }

    std::vector<Node *> searchAllNodesNoState(Node *node, long long vertex) {
        std::vector<Node *> foundNodes;
        if (!node) return foundNodes;
        if (!node->isValid) return foundNodes;
        if (node->vertex == vertex) foundNodes.push_back(node);
        for (Node *child: node->children) {
            auto childFound = searchAllNodesNoState(child, vertex);
            foundNodes.insert(foundNodes.end(), childFound.begin(), childFound.end());
        }
        return foundNodes;
    }

    void deleteSubTreeFromRootIterative(Node *root, Node *target) {
        if (!root || !target) return;
        std::stack<Node*> stack;
        stack.push(root);

        while (!stack.empty()) {
            Node* current = stack.top();
            stack.pop();

            if (current == target) {
                deleteSubTree(current, root->vertex);
                return;
            }

            for (Node* child : current->children) {
                stack.push(child);
            }
        }
    }

    void deleteSubTree(Node *node, long long rootVertex) {
        if (!node) return;

        // remove the node from the parent's children list
        if (node->parent) {
            auto &siblings = node->parent->children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), node), siblings.end());

            auto& treeSet = vertex_tree_map[node->vertex];
            for (auto it = treeSet.begin(); it != treeSet.end(); ++it) {
                if ((*it)->rootVertex == rootVertex) {
                    tree_reference_counting.at(rootVertex)--;
                    treeSet.erase(it);
                    break;
                }
            }
            auto treeRootsSet = vertex_state_tree_map[pair(node->vertex, node->state)];
            for (auto it = treeRootsSet.begin(); it != treeRootsSet.end(); ++it) {
                if (*it == rootVertex) {
                    treeRootsSet.erase(it);
                    break;
                }
            }
        }
        deleteSubTreeRecursive(node, rootVertex);
    }

    void deleteSubTreeRecursive(Node *node, long long rootVertex) {
        if (!node) return;
        for (Node *child: node->children) {
            deleteSubTreeRecursive(child, rootVertex);
        }
        auto& treeSet = vertex_tree_map[node->vertex];
        for (auto it = treeSet.begin(); it != treeSet.end(); ++it) {
            if ((*it)->rootVertex == rootVertex) {
                tree_reference_counting.at(rootVertex)--;
                treeSet.erase(it);
                break;
            }
        }
        auto treeRootsSet = vertex_state_tree_map[pair(node->vertex, node->state)];
        for (auto it = treeRootsSet.begin(); it != treeRootsSet.end(); ++it) {
            if (*it == rootVertex) {
                treeRootsSet.erase(it);
                break;
            }
        }

        // if the vertex_tree_map at current->vertex is empty, remove the entry
        /*
        if (vertex_tree_map[node->vertex].empty()) {
            vertex_tree_map.erase(node->vertex);
        }
        */
        if (!node->isRoot) {
            node_count--;
            delete node;
        } else {
            // if the node is root, just mark it as invalid
            node->isValid = false;
            node->parent = nullptr; // detach from parent
        }
    }
};


#endif //RPQ_FOREST_H
