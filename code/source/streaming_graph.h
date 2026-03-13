#ifndef STREAMING_GRAPH_H
#define STREAMING_GRAPH_H

#include <vector>
#include <unordered_map>
#include <iostream>

#include "ranking/buckets.h"

struct timed_edge;

struct sg_edge {
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

struct timed_edge {
    timed_edge *next;
    timed_edge *prev; // the two pointers to maintain the double linked list;
    sg_edge *edge_pt; // pointer to the sg edge;
    explicit timed_edge(sg_edge *edge) {
        edge_pt = edge;
        next = nullptr;
        prev = nullptr;
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
    struct expired_edge_info {
        long long src, dst, label;
    };

    std::unordered_map<long long, std::vector<std::pair<long long, sg_edge *> > > adjacency_list;

    // Inverted adjacency list: for each vertex, stores incoming edges
    // Key: destination vertex, Value: vector of (source vertex, edge pointer)
    std::unordered_map<long long, std::vector<std::pair<long long, sg_edge *>>> inverted_adjacency_list;

    double edge_num = 0; // number of edges in the window
    int EINIT_count = 0;

    timed_edge *time_list_head; // head of the time sequence list;
    timed_edge *time_list_tail; // tail of the time sequence list

    int first_transition; // the first label in the query

    std::unordered_map<long long, int> in_degree;
    std::unordered_map<long long, int> out_degree;
    long long vertex_num = 0; // number of vertices in the window

    // Lookup from the edge_id to the edge in the adjacency list
    std::unordered_map<long long, sg_edge *> edge_id_to_edge;

    RankBuckets rank;

    // edge_id -> (src, dst)
    std::unordered_map<long long, std::pair<long long, long long> > edge_endpoints;

    streaming_graph(const int first_transition) : first_transition(first_transition), rank(RankBuckets(2601977, 36233450)) {
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

    // Compute edge rank based on in-degree of destination and out-degree of source
    [[nodiscard]] int computeEdgeRank(long long src, long long dst) const {
        return 0;
        const int src_out = out_degree.count(src) ? out_degree.at(src) : 0;
        const int dst_in = in_degree.count(dst) ? in_degree.at(dst) : 0;

        return (src_out) + (dst_in);
    }

    // Try to insert or update an edge in the capped bottom-K set
    void updateEdgeRank(long long edge_id, long long src, long long dst) {
        return;
        rank.set_rank(edge_id, computeEdgeRank(src, dst));
    }

    // Remove edge from ranking
    void removeEdgeFromRanking(long long edge_id) {
        return;
        this->rank.remove(edge_id);
        edge_endpoints.erase(edge_id);
    }

    // Update ranks only for edges incident to a vertex that are already in the ranked set
    void updateRanksForVertex(long long vertex, long long current_time) {
        return;
        // Update all edges where this vertex is the source (out-degree matters)
        if (adjacency_list.count(vertex)) {
            for (const auto& [dst, edge] : adjacency_list.at(vertex)) {
                updateEdgeRank(edge->id, vertex, dst);
            }
        }

        // Update all edges where this vertex is the destination (in-degree matters)
        if (inverted_adjacency_list.count(vertex)) {
            for (const auto& [src, edge] : inverted_adjacency_list.at(vertex)) {
                updateEdgeRank(edge->id, src, vertex);
            }
        }

    }

    // Get bottom-k least important edges (already sorted ascending)
    [[nodiscard]] std::vector<RankBuckets::Id> getBottomKEdges(size_t k) const {
        return rank.bottom_k(k);
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
                // update the timestamp of the existing edge also in the inverted adjacency list
                for (auto &[src_vertex, inv_edge]: inverted_adjacency_list[to]) {
                    if (inv_edge == existing_edge) {
                        inv_edge->timestamp = existing_edge->timestamp;
                        break;
                    }
                }
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
            adjacency_list[from] = std::vector<std::pair<long long, sg_edge *> >();
        }
        adjacency_list.at(from).emplace_back(to, edge);

        // Add to inverted adjacency list (incoming edges for 'to')
        if (inverted_adjacency_list[to].empty()) {
            inverted_adjacency_list[to] = std::vector<std::pair<long long, sg_edge *>>();
        }
        inverted_adjacency_list.at(to).emplace_back(from, edge);

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
        // if (!from_is_new) {
        //     updateRanksForVertex(from, timestamp);
        // }
        // if (!to_is_new) {
        //     updateRanksForVertex(to, timestamp);
        // }
        //
        // // Add the new edge to the ranking
        // updateEdgeRank(edge_id, from, to);

        return edge;
    }

    bool remove_edge(long long from, long long to, long long label, long long current_time) {
        if (adjacency_list.find(from) == adjacency_list.end() || adjacency_list[from].empty()) {
            return false;
        }

        auto &edges = adjacency_list[from];
        for (auto it = edges.begin(); it != edges.end(); ++it) {
            if (sg_edge *edge = it->second; it->first == to && edge->label == label) {
                // Remove the edge from ranking first
                removeEdgeFromRanking(edge->id);

                // Remove from edge_id_to_edge map
                edge_id_to_edge.erase(edge->id);

                // Remove the edge from the adjacency list
                bool should_erase_from = false;
                edges.erase(it);
                edge_num--;
                if (label == first_transition) EINIT_count--;
                if (edges.empty()) {
                    should_erase_from = true;
                }

                // Remove from inverted adjacency list
                auto inv_it_map = inverted_adjacency_list.find(to);
                if (inv_it_map != inverted_adjacency_list.end()) {
                    auto &inv_edges = inv_it_map->second;
                    for (auto inv_it = inv_edges.begin(); inv_it != inv_edges.end(); ++inv_it) {
                        if (inv_it->first == from && inv_it->second == edge) {
                            inv_edges.erase(inv_it);
                            break;
                        }
                    }
                    if (inv_edges.empty()) {
                        inverted_adjacency_list.erase(inv_it_map);
                    }
                }

                // Update degrees
                out_degree[from]--;
                in_degree[to]--;

                bool from_removed = false;
                bool to_removed = false;

                if (out_degree[from] == 0 && in_degree[from] == 0) {
                    out_degree.erase(from);
                    in_degree.erase(from);
                    vertex_num--;
                    //forest.removeVertexFromRanking(from);
                    from_removed = true;
                }

                if (out_degree[to] == 0 && in_degree[to] == 0) {
                    out_degree.erase(to);
                    in_degree.erase(to);
                    if (adjacency_list.count(to)) {
                        adjacency_list.erase(to);
                    }
                    vertex_num--;
                    //forest.removeVertexFromRanking(to);
                    to_removed = true;
                }

                // Update ranks AFTER all structural changes are complete
                // if (!from_removed) {
                //     updateRanksForVertex(from, current_time);
                // }
                // if (!to_removed) {
                //     updateRanksForVertex(to, current_time);
                // }

                if (should_erase_from) {
                    adjacency_list.erase(from);
                }

                // Now safe to delete the edge object
                delete edge;

                return true;
            }
        }
        return false;
    }

    // Add this method to streaming_graph
    void expire(long long eviction_time, std::vector<expired_edge_info>& deleted_edges) {
        while (time_list_head) {
            sg_edge* cur = time_list_head->edge_pt;
            // edges are ordered oldest-first; stop when the front edge is still alive
            if (cur->timestamp >= eviction_time)
                break;

            deleted_edges.push_back({cur->s, cur->d, cur->label});

            // Detach and delete the timed_edge node from the list
            timed_edge* te = time_list_head;
            time_list_head = time_list_head->next;
            if (time_list_head)
                time_list_head->prev = nullptr;
            else
                time_list_tail = nullptr;
            te->edge_pt = nullptr;
            delete te;

            // Null out time_pos before remove_edge so the adjacency-list
            // removal does not try to touch the already-deleted timed_edge
            cur->time_pos = nullptr;

            // remove_edge handles adjacency list, degrees, ranking, and deletes cur
            remove_edge(cur->s, cur->d, cur->label, eviction_time);
        }
    }

    std::vector<sg_edge> get_all_suc(long long s) {
        std::vector<sg_edge> sucs;
        if (adjacency_list[s].empty()) {
            return sucs; // No outgoing edges for vertex s
        }

        for (const auto &[to, edge]: adjacency_list[s]) {
            sucs.emplace_back(edge->id, s, to, edge->label, edge->timestamp, edge->expiration_time);
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

#endif //STREAMING_GRAPH_H