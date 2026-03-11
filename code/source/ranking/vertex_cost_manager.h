#ifndef ADA_VERTEX_COST_MANAGER_H
#define ADA_VERTEX_COST_MANAGER_H
#include <unordered_map>

struct CostNode {
    long long tree_id;          // id dell'albero RPQ
    int insertion_cost;         // costo di inserimento
    CostNode* left = nullptr;
    CostNode* right = nullptr;

    CostNode(long long id, int cost) : tree_id(id), insertion_cost(cost) {}
};

struct CostTree {
    CostNode* root = nullptr;
    int node_count = 0;
    long long cost_sum = 0;

    void insert(long long tree_id, int cost) {
        root = insertNode(root, tree_id, cost);
        node_count++;
        cost_sum += cost;
    }

    bool remove(long long tree_id) {
        int removed_cost = 0;
        root = removeNode(root, tree_id, removed_cost);
        if (removed_cost != -1) {
            node_count--;
            cost_sum -= removed_cost;
            return true;
        }
        return false;
    }

    [[nodiscard]] double average() const {
        if (node_count == 0) return 0.0;
        return static_cast<double>(cost_sum) / node_count;
    }

    ~CostTree() {
        destroyTree(root);
    }

private:
    CostNode* insertNode(CostNode* node, long long tree_id, int cost) {
        if (!node) return new CostNode(tree_id, cost);
        if (tree_id < node->tree_id) {
            node->left = insertNode(node->left, tree_id, cost);
        } else if (tree_id > node->tree_id) {
            node->right = insertNode(node->right, tree_id, cost);
        } else {
            // Aggiorna il costo se esiste già
            cost_sum -= node->insertion_cost;
            node->insertion_cost = cost;
            cost_sum += cost;
            node_count--; // Compensazione per l'incremento in insert()
        }
        return node;
    }

    CostNode* removeNode(CostNode* node, long long tree_id, int& removed_cost) {
        if (!node) {
            removed_cost = -1;
            return nullptr;
        }
        if (tree_id < node->tree_id) {
            node->left = removeNode(node->left, tree_id, removed_cost);
        } else if (tree_id > node->tree_id) {
            node->right = removeNode(node->right, tree_id, removed_cost);
        } else {
            removed_cost = node->insertion_cost;
            if (!node->left) {
                CostNode* temp = node->right;
                delete node;
                return temp;
            }
            if (!node->right) {
                CostNode* temp = node->left;
                delete node;
                return temp;
            }
            CostNode* successor = findMin(node->right);
            node->tree_id = successor->tree_id;
            node->insertion_cost = successor->insertion_cost;
            int dummy;
            node->right = removeNode(node->right, successor->tree_id, dummy);
        }
        return node;
    }

    CostNode* findMin(CostNode* node) {
        while (node && node->left) node = node->left;
        return node;
    }

    void destroyTree(CostNode* node) {
        if (!node) return;
        destroyTree(node->left);
        destroyTree(node->right);
        delete node;
    }
};

class VertexCostManager {
public:
    std::unordered_map<long long, CostTree> vertex_costs;

    void insertCost(long long vertex_id, long long tree_id, int cost) {
        vertex_costs[vertex_id].insert(tree_id, cost);
    }

    bool removeCost(long long vertex_id, long long tree_id) {
        if (!vertex_costs.count(vertex_id)) return false;
        bool result = vertex_costs[vertex_id].remove(tree_id);
        if (vertex_costs[vertex_id].node_count == 0) {
            vertex_costs.erase(vertex_id);
        }
        return result;
    }

    bool removeEntry(long long vertex_id) {
        if (!vertex_costs.count(vertex_id)) return false;
        vertex_costs.erase(vertex_id);
        return true;
    }

    [[nodiscard]] double getAverageCost(long long vertex_id) const {
        if (!vertex_costs.count(vertex_id)) return 0.0;
        return vertex_costs.at(vertex_id).average();
    }

    [[nodiscard]] int getCostCount(long long vertex_id) const {
        if (!vertex_costs.count(vertex_id)) return 0;
        return vertex_costs.at(vertex_id).node_count;
    }
};


#endif //ADA_VERTEX_COST_MANAGER_H