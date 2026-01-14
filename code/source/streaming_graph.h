#pragma once
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <cmath>

using namespace std;

// this streaming graph class suppose that there may be duplicate edges in the streaming graph. It means the same edge (s, d, label) may appear multiple times in the stream.

class sg_edge;

struct timed_edge
        // this is the structure to maintain the time sequence list. It stores the tuples in the stream with time order, each tuple is an edge;
{
    timed_edge *next;
    timed_edge *prev; // the two pointers to maintain the double linked list;
    sg_edge *edge_pt; // pointer to the sg edge;
    explicit timed_edge(sg_edge *edge) {
        edge_pt = edge;
        next = nullptr;
        prev = nullptr;
    }
};

struct edge_info // the structure as query result, include all the information of an edge;
{
    long long s, d;
    long long label;
    long long timestamp;
    long long expiration_time;
    long long id;
    edge_info(long long src,  long long dst, long long time, long long label_, long long expiration_time_, long long id_) {
        s = src;
        d = dst;
        timestamp = time;
        expiration_time = expiration_time_;
        label = label_;
        id = id_;
    }
};

class sg_edge {
public:
    long long label;
    long long timestamp;
    long long expiration_time;
    timed_edge *time_pos;
    long long s, d;
    long long id;
    int lives;

    sg_edge(const long long id_, const long long src, const long long dst, const long long label_, const long long time, int lives_, const long long expiration_time_) {
        id = id_;
        s = src;
        d = dst;
        timestamp = time;
        label = label_;
        time_pos = nullptr;
        lives = lives_;
        expiration_time = expiration_time_;
    }
};

struct pair_hash_aj {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto hash1 = std::hash<T1>()(p.first);
        auto hash2 = std::hash<T2>()(p.second);
        return hash1 ^ (hash2 << 1); // Combine the two hash values
    }
};

// Custom hash function for std::pair<double, long long>
struct pair_hash {
    template<class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2> &p) const {
        auto hash1 = std::hash<T1>()(p.first);
        auto hash2 = std::hash<T2>()(p.second);
        return hash1 ^ (hash2 << 1); // Combine the two hash values
    }
};

struct MemoryEstimatorAdjL {
    static constexpr size_t ptr_size = sizeof(void*);
    static constexpr size_t node_overhead = 2 * sizeof(void*); // approx. per unordered_map node

    // size of one sg_edge object
    static size_t size_of_sg_edge() {
        return sizeof(sg_edge);
    }

    // estimate memory for unordered_map<long long, vector<pair<long long, sg_edge*>>>
    static size_t estimate_adjacency_list(
        const std::unordered_map<long long, std::vector<std::pair<long long, sg_edge*>>>& m)
    {
        size_t total = 0;
        // bucket array
        total += m.bucket_count() * ptr_size;

        for (const auto& kv : m) {
            total += sizeof(long long);               // key
            total += sizeof(std::vector<std::pair<long long, sg_edge*>>); // vector object
            total += node_overhead;                   // unordered_map node overhead

            const auto& vec = kv.second;
            // vector capacity (not just size!) times element size
            total += vec.capacity() * sizeof(std::pair<long long, sg_edge*>);
        }
        return total;
    }
};

class streaming_graph {
public:
    unordered_map<long long, vector<pair<long long, sg_edge *> > > adjacency_list;

    double edge_num=0; // number of edges in the window
    long long vertex_num=0; // number of vertices in the window
    timed_edge *time_list_head; // head of the time sequence list;
    timed_edge *time_list_tail; // tail of the time sequence list

    int lives = 1;

    // Z-score computation
    double mean = 0;
    double m2 = 0;
    unordered_map<long long, double> density;
    double zscore_threshold;
    unordered_set<long long> high_zscore_vertices;

    explicit streaming_graph(double zscore_threshold_)
        : zscore_threshold(zscore_threshold_) {
        time_list_head = nullptr;
        time_list_tail = nullptr;
    }

    ~streaming_graph() {
        // Free memory for all edges in the adjacency list
        for (auto& [_, edges] : adjacency_list) {
            for (auto& [_, edge] : edges) {
                delete edge;
            }
        }

        // Free memory for the timed edges list
        while (time_list_head) {
            timed_edge* temp = time_list_head;
            time_list_head = time_list_head->next;
            delete temp;
        }
    }

    void update_zscore_tracking(long long vertex) {
        double z = get_zscore(vertex);
        if (z > zscore_threshold) {
            high_zscore_vertices.insert(vertex);
        } else {
            high_zscore_vertices.erase(vertex);
        }
    }

    void add_timed_edge(timed_edge *cur) // append an edge to the time sequence list
    {
        if (!time_list_head) {
            time_list_head = cur;
            time_list_tail = cur;
        } else {
            time_list_tail->next = cur;
            cur->prev = time_list_tail;
            time_list_tail = cur;
        }
    }

    void delete_timed_edge(timed_edge *cur) // delete an edge from the time sequence list
    {
        if (!cur)
            return;

        if (cur == time_list_head) {
            time_list_head = cur->next;
            if (time_list_head)
                time_list_head->prev = nullptr;
        }

        if (cur == time_list_tail) {
            time_list_tail = cur->prev;
            if (time_list_tail)
                time_list_tail->next = nullptr;
        }

        if (cur->prev) cur->prev->next = cur->next;
        if (cur->next) cur->next->prev = cur->prev;

        delete cur;
    }

    sg_edge * search_existing_edge(long long from, long long to, long long label) {

        for (auto &[to_vertex, existing_edge]: adjacency_list[from]) {
            if (existing_edge->label == label && to_vertex == to) {
                return existing_edge;
            }
        }

        return nullptr;
    }

    sg_edge* insert_edge(const long long edge_id, const long long from,  long long to, const long long label, const long long timestamp,
                         const long long expiration_time) {

        // Check if the edge already exists in the adjacency list
        for (auto &[to_vertex, existing_edge]: adjacency_list[from]) {
            if (existing_edge->label == label && to_vertex == to) {
                // if (existing_edge->expiration_time < expiration_time) existing_edge->expiration_time = expiration_time;
                if (existing_edge->timestamp < timestamp) existing_edge->timestamp = timestamp;
                return existing_edge;
            }
        }

        edge_num++;
        auto *edge = new sg_edge(edge_id, from, to, label, timestamp, lives, expiration_time);

        // Add the edge to the adjacency list if it doesn't exist
        if (adjacency_list[from].empty()) {
            vertex_num++;
            adjacency_list[from] = vector<pair<long long, sg_edge *> >();
        }
        adjacency_list.at(from).emplace_back(to, edge);

        density[from]++;

        // update z score
        if (edge_num == 1) {
            // density[to]++;
            // mean = (density[from] + density[to]) / 2;
            mean = density[from];
            m2 = 0;
        } else {
            double old_mean = mean;
            mean += (density[from] - mean) / vertex_num;
            m2 += (density[from] - old_mean) * (density[from] - mean);
            /*
            density[to]++;
            old_mean = mean;
            mean += (density[to] - mean) / vertex_num;
            m2 += (density[to] - old_mean) * (density[to] - mean);
            */
        }

        update_zscore_tracking(from);
        // update_zscore_tracking(to);
        return edge;
    }

    bool remove_edge(long long from, long long to, long long label) { // delete an edge from the snapshot graph

        // Check if the vertex exists in the adjacency list
        if (adjacency_list[from].empty()) {
            return false; // Edge doesn't exist
        }

        auto &edges = adjacency_list[from];
        for (auto it = edges.begin(); it != edges.end(); ++it) {
            sg_edge *edge = it->second;

            // Check if this is the edge to remove
            if (it->first == to && edge->label == label) {

                // Remove the edge from the adjacency list
                edges.erase(it);
                edge_num--;

                if (edges.empty()) {
                    adjacency_list.erase(from);
                    vertex_num--;
                }

                // update z-score computation
                double old_mean = mean;
                mean -= (mean - density[from]) / (vertex_num);
                m2 -= (density[from] - old_mean) * (density[from] - mean);

                density[from]--;

                /*
                old_mean = mean;
                mean -= (mean - density[to]) / (vertex_num);
                m2 -= (density[to] - old_mean) * (density[to] - mean);
                density[to]--;
                */

                update_zscore_tracking(from);
                // update_zscore_tracking(to);
                return true; // Successfully removed
            }
        }
        return false; // Edge not found
    }

    std::vector<edge_info> get_all_suc(long long s) {
        std::vector<edge_info> sucs;
        if (adjacency_list[s].empty()) {
            return sucs; // No outgoing edges for vertex s
        }

        for (const auto &[to, edge]: adjacency_list[s]) {
            sucs.emplace_back(s, to, edge->timestamp, edge->label, edge->expiration_time, edge->id);
        }
        return sucs;
    }

    double get_zscore(long long vertex) {
        double variance = m2 / edge_num;
        double std_dev = std::sqrt(variance);

        if (std_dev == 0) return 0;

        return (density[vertex] - mean) / std_dev;
    }

    void shift_timed_edge(timed_edge *to_insert, timed_edge *target) {
        if (!to_insert || !target) return;

        // Remove to_insert from its current position
        if (to_insert->prev) to_insert->prev->next = to_insert->next;
        if (to_insert->next) to_insert->next->prev = to_insert->prev;
        if (to_insert == time_list_head) time_list_head = to_insert->next;
        if (to_insert == time_list_tail) time_list_tail = to_insert->prev;

        // Insert to_insert right after target
        to_insert->next = target->next;
        to_insert->prev = target;
        if (target->next) target->next->prev = to_insert;
        target->next = to_insert;

        // Update tail if necessary
        if (target == time_list_tail) time_list_tail = to_insert;

        to_insert->edge_pt->expiration_time = target->edge_pt->expiration_time;
        to_insert->edge_pt->timestamp = target->edge_pt->timestamp;
    }

    [[nodiscard]] size_t getUsedMemory() const {
        return MemoryEstimatorAdjL::estimate_adjacency_list(adjacency_list);
    }

void printGraph() const {
    if (adjacency_list.empty()) {
        std::cout << "╔════════════════╗\n";
        std::cout << "║ Empty Graph    ║\n";
        std::cout << "╚════════════════╝\n";
        return;
    }

    std::cout << "╔════════════════════════════════════════╗\n";
    std::cout << "║ Graph Statistics                       ║\n";
    std::cout << "╠════════════════════════════════════════╣\n";
    std::cout << "║ Vertices: " << vertex_num << std::string(29 - std::to_string(vertex_num).length(), ' ') << "║\n";
    std::cout << "║ Edges: " << static_cast<long long>(edge_num) << std::string(32 - std::to_string(static_cast<long long>(edge_num)).length(), ' ') << "║\n";
    std::cout << "╚════════════════════════════════════════╝\n\n";

    for (const auto& [from, edges] : adjacency_list) {
        std::cout << "Vertex " << from;

        for (size_t i = 0; i < edges.size(); ++i) {
            const auto& [to, edge] = edges[i];
            bool isLast = (i == edges.size() - 1);

            std::cout << (isLast ? "└──" : "├──") << "→ " << to;
            std::cout << " [label:" << edge->label
                      << " ts:" << edge->timestamp << "]\n";
        }
        std::cout << "\n";
    }
}


};
