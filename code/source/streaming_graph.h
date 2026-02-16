#pragma once
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <cmath>
#include <iostream>

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

    edge_info(long long src, long long dst, long long time, long long label_, long long expiration_time_,
              long long id_) {
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

    sg_edge(const long long id_, const long long src, const long long dst, const long long label_, const long long time, const long long expiration_time_) {
        id = id_;
        s = src;
        d = dst;
        timestamp = time;
        label = label_;
        time_pos = nullptr;
        expiration_time = expiration_time_;
    }
};

struct pair_hash_aj {
    template<class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2> &p) const {
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
    static constexpr size_t ptr_size = sizeof(void *);
    static constexpr size_t node_overhead = 2 * sizeof(void *); // approx. per unordered_map node

    // size of one sg_edge object
    static size_t size_of_sg_edge() {
        return sizeof(sg_edge);
    }

    // estimate memory for unordered_map<long long, vector<pair<long long, sg_edge*>>>
    static size_t estimate_adjacency_list(
        const std::unordered_map<long long, std::vector<std::pair<long long, sg_edge *> > > &m) {
        size_t total = 0;
        // bucket array
        total += m.bucket_count() * ptr_size;

        for (const auto &kv: m) {
            total += sizeof(long long); // key
            total += sizeof(std::vector<std::pair<long long, sg_edge *> >); // vector object
            total += node_overhead; // unordered_map node overhead

            const auto &vec = kv.second;
            // vector capacity (not just size!) times element size
            total += vec.capacity() * sizeof(std::pair<long long, sg_edge *>);
        }
        return total;
    }
};

class streaming_graph {
public:
    unordered_map<long long, vector<pair<long long, sg_edge *> > > adjacency_list;

    double edge_num = 0; // number of edges in the window
    int EINIT_count = 0;

    timed_edge *time_list_head; // head of the time sequence list;
    timed_edge *time_list_tail; // tail of the time sequence list

    int first_transition; // the first label in the query

    unordered_map<long long, int> in_degree;
    unordered_map<long long, int> out_degree;
    long long vertex_num = 0; // number of vertices in the window

    // Edge importance ranking: edge_id -> rank (in_degree[dst] * out_degree[src])
    std::unordered_map<long long, double> edge_rank;
    // Sorted set for fast top-k edge queries: (rank, edge_id) sorted by rank descending
    std::set<std::pair<double, long long>, std::greater<>> ranked_edges;
    // Map edge_id to (src, dst) for rank updates when degrees change
    std::unordered_map<long long, std::pair<long long, long long>> edge_endpoints;
    // Lookup from the edge_id to the edge in the adjacency list
    std::unordered_map<long long, sg_edge *> edge_id_to_edge;

    // Compute edge rank based on in-degree of destination and out-degree of source
    [[nodiscard]] double computeEdgeRank(long long src, long long dst, long long id, long long current_time) const {
        const int src_out = out_degree.count(src) ? out_degree.at(src) : 1;
        const int dst_in = in_degree.count(dst) ? in_degree.at(dst) : 1;

        double time_deviation = 1 - static_cast<double>(current_time - edge_id_to_edge.at(id)->timestamp)/static_cast<double>(current_time);

        return static_cast<double>(src_out) * static_cast<double>(dst_in) * time_deviation;
    }

    // Update rank for a single edge
    void updateEdgeRank(long long edge_id, long long src, long long dst, long long current_time) {
        // Remove old rank entry if exists
        if (edge_rank.count(edge_id)) {
            double old_rank = edge_rank[edge_id];
            ranked_edges.erase({old_rank, edge_id});
        }

        // Compute and insert new rank
        double new_rank = computeEdgeRank(src, dst, edge_id, current_time);
        edge_rank[edge_id] = new_rank;
        ranked_edges.insert({new_rank, edge_id});
    }

    // Remove edge from ranking
    void removeEdgeFromRanking(long long edge_id) {
        if (edge_rank.count(edge_id)) {
            double old_rank = edge_rank[edge_id];
            ranked_edges.erase({old_rank, edge_id});
            edge_rank.erase(edge_id);
        }
        edge_endpoints.erase(edge_id);
    }

    // Update ranks for all edges incident to a vertex (called when degree changes)
    void updateRanksForVertex(long long vertex, long long current_time) {
        // Update all edges where this vertex is the source (out-degree matters)
        if (adjacency_list.count(vertex)) {
            for (const auto& [dst, edge] : adjacency_list.at(vertex)) {
                updateEdgeRank(edge->id, vertex, dst, current_time);
            }
        }
        // Update all edges where this vertex is the destination (in-degree matters)
        for (const auto& [src, edges] : adjacency_list) {
            for (const auto& [dst, edge] : edges) {
                if (dst == vertex) {
                    updateEdgeRank(edge->id, src, dst, current_time);
                }
            }
        }
    }

    // Get the rank of a specific edge
    [[nodiscard]] double getEdgeRank(long long edge_id) const {
        auto it = edge_rank.find(edge_id);
        return (it != edge_rank.end()) ? it->second : 0;
    }

    // Get top-k most important edges
    [[nodiscard]] std::vector<std::pair<long long, double>> getTopKEdges(size_t k) const {
        std::vector<std::pair<long long, double>> result;
        result.reserve(std::min(k, ranked_edges.size()));
        size_t count = 0;
        for (const auto& [rank, edge_id] : ranked_edges) {
            if (count++ >= k) break;
            result.emplace_back(edge_id, rank);
        }
        return result;
    }

    // Get bottom-k least important edges (lowest rank first)
    [[nodiscard]] std::vector<std::pair<long long, double>> getBottomKEdges(size_t k) const {
        std::vector<std::pair<long long, double>> result;
        result.reserve(std::min(k, ranked_edges.size()));
        size_t count = 0;
        // Iterate in reverse order (ascending rank)
        for (auto it = ranked_edges.rbegin(); it != ranked_edges.rend() && count < k; ++it, ++count) {
            result.emplace_back(it->second, it->first); // (edge_id, rank)
        }
        return result;
    }

    explicit streaming_graph(const int first_transition) : first_transition(first_transition) {
        time_list_head = nullptr;
        time_list_tail = nullptr;
    }

    ~streaming_graph() {
        // Free memory for all edges in the adjacency list
        for (auto &[_, edges]: adjacency_list) {
            for (auto &[_, edge]: edges) {
                delete edge;
            }
        }

        // Free memory for the timed edges list
        while (time_list_head) {
            timed_edge *temp = time_list_head;
            time_list_head = time_list_head->next;
            delete temp;
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

    sg_edge *search_existing_edge(long long from, long long to, long long label) {
        for (auto &[to_vertex, existing_edge]: adjacency_list[from]) {
            if (existing_edge->label == label && to_vertex == to) {
                return existing_edge;
            }
        }

        return nullptr;
    }

    sg_edge *insert_edge(const long long edge_id, const long long from, long long to, const long long label,
                         const long long timestamp,
                         const long long expiration_time) {
        // Check if the edge already exists in the adjacency list
        for (auto &[to_vertex, existing_edge]: adjacency_list[from]) {
            if (existing_edge->label == label && to_vertex == to) {
                // if (existing_edge->expiration_time < expiration_time) existing_edge->expiration_time = expiration_time;
                if (existing_edge->timestamp < timestamp) existing_edge->timestamp = timestamp;
                delete_timed_edge(existing_edge->time_pos); // remove the old timed edge from the time list
                return existing_edge;
            }
        }

        edge_num++;
        if (label == first_transition) EINIT_count++;

        auto *edge = new sg_edge(edge_id, from, to, label, timestamp, expiration_time);
        edge_id_to_edge[edge_id] = edge;

        // Add the edge to the adjacency list if it doesn't exist
        if (adjacency_list[from].empty()) {
            adjacency_list[from] = vector<pair<long long, sg_edge *> >();
        }
        adjacency_list.at(from).emplace_back(to, edge);

        // Track vertex count and degrees
        bool from_is_new = false;
        bool to_is_new = false;

        if (out_degree.find(from) == out_degree.end()) {
            out_degree[from] = 0;
            if (in_degree.find(from) == in_degree.end()) {
                in_degree[from] = 0;
            }
            vertex_num++;
            from_is_new = true;
        }
        if (in_degree.find(to) == in_degree.end()) {
            in_degree[to] = 0;
            if (out_degree.find(to) == out_degree.end()) {
                out_degree[to] = 0;
            }
            vertex_num++;
            to_is_new = true;
        }
        out_degree[from]++;
        in_degree[to]++;

        // Store edge endpoints for future rank updates
        edge_endpoints[edge_id] = {from, to};

        // Update ranks for all edges incident to 'from' (out-degree changed)
        // and all edges incident to 'to' (in-degree changed)
        if (!from_is_new) {
            updateRanksForVertex(from, timestamp);
        }
        if (!to_is_new) {
            updateRanksForVertex(to, timestamp);
        }

        // Add the new edge to the ranking
        updateEdgeRank(edge_id, from, to, timestamp);

        return edge;
    }

    bool remove_edge(long long from, long long to, long long label, long long current_time) {
        // delete an edge from the snapshot graph

        // Check if the vertex exists in the adjacency list
        if (adjacency_list[from].empty()) {
            return false; // Edge doesn't exist
        }

        auto &edges = adjacency_list[from];
        for (auto it = edges.begin(); it != edges.end(); ++it) {
            // Check if this is the edge to remove
            if (sg_edge *edge = it->second; it->first == to && edge->label == label) {
                // Remove the edge from ranking first (before modifying degrees)
                removeEdgeFromRanking(edge->id);

                // Remove the edge from the adjacency list
                edges.erase(it);
                edge_num--;
                if (label == first_transition) EINIT_count--;
                if (edges.empty()) {
                    adjacency_list.erase(from);
                }

                // remove the entry from the edge id to edge map
                edge_id_to_edge.erase(edge->id);

                // Update degrees
                out_degree[from]--;
                in_degree[to]--;

                // Update ranks for edges incident to these vertices (degrees changed)
                bool from_removed = false;
                bool to_removed = false;

                // Remove 'from' vertex if it has no incident edges
                if (out_degree[from] == 0 && in_degree[from] == 0) {
                    out_degree.erase(from);
                    in_degree.erase(from);
                    vertex_num--;
                    from_removed = true;
                }

                // Remove 'to' vertex if it has no incident edges
                if (out_degree[to] == 0 && in_degree[to] == 0) {
                    out_degree.erase(to);
                    in_degree.erase(to);
                    if (adjacency_list.count(to)) {
                        adjacency_list.erase(to);
                    }
                    vertex_num--;
                    to_removed = true;
                }

                // Update ranks for remaining edges incident to 'from' and 'to'
                if (!from_removed) {
                    updateRanksForVertex(from, current_time);
                }
                if (!to_removed) {
                    updateRanksForVertex(to, current_time);
                }

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
        std::cout << "║ Vertices: " << vertex_num << std::string(29 - std::to_string(vertex_num).length(), ' ') <<
                "║\n";
        std::cout << "║ Edges: " << static_cast<long long>(edge_num) << std::string(
            32 - std::to_string(static_cast<long long>(edge_num)).length(), ' ') << "║\n";
        std::cout << "╚════════════════════════════════════════╝\n\n";

        for (const auto &[from, edges]: adjacency_list) {
            std::cout << "Vertex " << from;

            for (size_t i = 0; i < edges.size(); ++i) {
                const auto &[to, edge] = edges[i];
                bool isLast = (i == edges.size() - 1);

                std::cout << (isLast ? "└──" : "├──") << "→ " << to;
                std::cout << " [label:" << edge->label
                        << " ts:" << edge->timestamp << "]\n";
            }
            std::cout << "\n";
        }
    }
};
