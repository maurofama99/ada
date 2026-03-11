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
#include "ranking/buckets.h"
#include "ranking/vertex_cost_manager.h"

using namespace std;

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
        rank.set_rank(vertex_id, computeVertexRank(vertex_id));
    }

    // Remove from ranking
    void removeVertexFromRanking(long long vertex_id) {
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
        for (Node* child : node->children) {
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

            vertex_tree_map[rootVertex].insert(&trees.at(rootVertex));
            vertex_state_tree_map[pair(rootVertex, rootState)].insert(rootVertex);

            tree_reference_counting[rootVertex] = 1;

            trees.at(rootVertex).rootNode->timestamp = INT64_MAX;
            trees.at(rootVertex).rootNode->expiration_time = INT64_MAX;
            trees.at(rootVertex).rootNode->isRoot = true;
            trees.at(rootVertex).rootNode->timestampRoot = timestamp;
            trees.at(rootVertex).rootNode->insertion_cost = 0;
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
        if (parent) {
            auto child = new Node(parent->id, childVertex, childState, parent);
            //child->timestamp = timestamp < parent->timestamp ? timestamp : parent->timestamp;
            child->timestamp = std::max(timestamp, parent->timestamp);
            child->expiration_time = newExpiry;
            parent->children.emplace_back(child);
            node_count++;

            if (fsa.isFinalState(childState)) {
                propagateFinalStateCount(child, +1);
            }

            if (trees.find(rootVertex) == trees.end()) {
                cout << "ERROR: Tree with root vertex " << rootVertex << " not found" << endl;
                exit(1);
            }
            vertex_tree_map[child->vertex].insert(&trees.at(rootVertex));
            vertex_state_tree_map[pair(childVertex, childState)].insert(rootVertex);
            tree_reference_counting[rootVertex]++;

            child->insertion_cost = parent->insertion_cost+1;
            vertex_cost_manager.insertCost(child->vertex, rootVertex, child->insertion_cost);
            updateVertexRank(child->vertex);

            return true;
        }
        return false;
    }

    bool changeParentTimestamped(Node *child, Node *newParent, long long timestamp, long long rootVertex, long long newExpiry) {

        if (!newParent->isValid) return false;

        child->insertion_cost = newParent->insertion_cost+1;
        vertex_cost_manager.insertCost(child->vertex, rootVertex, child->insertion_cost);
        updateVertexRank(child->vertex);

        int finalCount = countFinalStatesInSubtree(child, rootVertex);

        Node* old = child->parent;
        while (finalCount>0 && old) {
            old->final_state_descendants -= finalCount;
            vertex_contribution[old->vertex] -= finalCount;
            updateVertexRank(old->vertex);
            old = old->parent;
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
        child->timestamp = std::max(timestamp, newParent->timestamp);
        child->expiration_time = newExpiry;
        newParent->children.push_back(child);

        Node* cur = newParent;
        while (cur) {
            cur->final_state_descendants += finalCount;
            vertex_contribution[cur->vertex] += finalCount;
            updateVertexRank(cur->vertex);
            cur = cur->parent;
        }

        return true;
    }

    bool hasTree(long long rootVertex) {
        return trees.find(rootVertex) != trees.end();
    }

    Node *findNodeInTree(long long rootVertex, long long vertex, long long state) {
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

    /**
     * @param vertex id of the vertex in the Adj. List
     * @param state state associated to the vertex node
     * @return the set of trees to which the node @param vertex @param state belongs to
     */
    // std::vector<Tree> findTreesWithNode(long long vertex, long long state) {
    //     std::vector<Tree> result;
    //     std::vector<Tree*> treesEntryToDelete;
    //     std::vector<long long> treesRootEntryToDelete;
    //
    //      if (vertex_tree_map.count(vertex)) {
    //         for (auto &tree: vertex_tree_map.at(vertex)) {
    //             // catch a dangling tree, decrease reference counter
    //             if (!tree->rootNode->isValid) {
    //                 // remove the tree from the vertex tree map
    //                 treesEntryToDelete.push_back(tree);
    //                 tree_reference_counting.at(tree->rootVertex)--;
    //             } else if (searchNode(tree->rootNode, vertex, state))
    //                 result.emplace_back(tree->rootVertex, tree->rootNode, tree->expired);
    //         }
    //     }
    //     for (auto tree : treesEntryToDelete) {
    //         vertex_tree_map.at(vertex).erase(tree);
    //     }
    //
    //     if (vertex_state_tree_map.count(pair(vertex,state))) {
    //         for (auto tree_root: vertex_state_tree_map.at(pair(vertex,state))) {
    //             // catch a dangling tree, decrease reference counter
    //             if (trees.count(tree_root) && !trees.at(tree_root).rootNode->isValid){
    //                 treesRootEntryToDelete.push_back(tree_root);
    //             }
    //         }
    //     }
    //     for (auto tree : treesRootEntryToDelete) {
    //         vertex_state_tree_map.at(pair(vertex,state)).erase(tree);
    //     }
    //     return result;
    // }

    std::vector<Tree> findTreesWithNode(long long vertex, long long state) {
        std::vector<Tree> result;

        if (vertex_tree_map.count(vertex)) {
            for (auto &tree : vertex_tree_map.at(vertex)) {
                if (!tree->rootNode->isValid) continue;
                if (searchNode(tree->rootNode, vertex, state))
                    result.emplace_back(tree->rootVertex, tree->rootNode, tree->expired);
            }
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
                for (int i = 0; i < fsa.states_count; ++i) {
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
                         if (current->expiration_time < eviction_time || (current->isRoot && current->timestampRoot < eviction_time)) {
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

        // Flush deferred deletions now that all traversals are complete
        flushPendingDeletes();
    }

    void flushPendingDeletes() {
        for (Node* node : pending_deletes) {
            delete node;
        }
        pending_deletes.clear();
    }

    void printTree(Node *node, const std::string& prefix = "", bool isLast = true) const {
        if (!node) return;

        std::cout << prefix;
        std::cout << (isLast ? "└── " : "├── ");
        std::cout << "v:" << node->vertex << ", s:" << node->state << ", ts: " << (node->isRoot ? node->timestamp : 0);
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
        if (!node->isValid) return nullptr;
        if (node->vertex == vertex && node->state == state) return node;
        for (Node *child: node->children) {
            Node *found = searchNode(child, vertex, state);
            if (found) return found;
        }
        return nullptr;
    }

    std::pair<Node *, int> searchNodeWithDepth(Node *root, long long vertex, long long state) {
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

            for (Node *child: current->children) {
                stack.emplace(child, depth + 1);
            }
        }

        return {nullptr, -1};
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
            auto &treeRootsSet = vertex_state_tree_map[pair(node->vertex, node->state)];
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

        std::vector<Node*> childrenCopy = node->children;
        node->children.clear();

        for (Node *child: childrenCopy) {
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
        //auto treeRootsSet = vertex_state_tree_map[pair(node->vertex, node->state)];
        auto &treeRootsSet = vertex_state_tree_map[pair(node->vertex, node->state)];
        for (auto it = treeRootsSet.begin(); it != treeRootsSet.end(); ++it) {
            if (*it == rootVertex) {
                treeRootsSet.erase(it);
                break;
            }
        }

        if (fsa.isFinalState(node->state)) {
            propagateFinalStateCount(node, -1);
        }
        vertex_cost_manager.removeCost(node->vertex, rootVertex);

        node->isValid = false;
        if (!node->isRoot) {
            node_count--;
            // Deferred delete: collect for later freeing
            pending_deletes.push_back(node);
        } else {
            node->parent = nullptr;
        }
    }
};


#endif //RPQ_FOREST_H
