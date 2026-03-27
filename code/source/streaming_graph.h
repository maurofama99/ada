#ifndef STREAMING_GRAPH_H
#define STREAMING_GRAPH_H

#include <vector>
#include <unordered_map>
#include <iostream>
#include <map>

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
        long long src, dst, label, id;
    };

    int shed_count = 0;

    std::unordered_map<long long, std::vector<std::pair<long long, sg_edge *> > > adjacency_list;
    std::unordered_map<long long, std::vector<std::pair<long long, sg_edge *> > > reverse_adjacency_list; // dst -> [(src, edge*), ...]

    double edge_num = 0; // number of edges in the window
    int EINIT_count = 0;

    timed_edge *time_list_head = nullptr; // head of the time sequence list;
    timed_edge *time_list_tail = nullptr; // tail of the time sequence list

    int first_transition; // the first label in the query

    std::unordered_map<long long, int> in_degree;
    std::unordered_map<long long, int> out_degree;
    double vertex_num = 0; // number of vertices in the window

    // Lookup from the edge_id to the edge in the adjacency list
    std::unordered_map<long long, sg_edge *> edge_id_to_edge;

    // edge_id -> (src, dst)
    std::unordered_map<long long, std::pair<long long, long long> > edge_endpoints;

    streaming_graph(const int first_transition) : first_transition(first_transition) {}

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

        cur->edge_pt->time_pos = nullptr; // disassociate the sg_edge from the timed edge

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

    sg_edge *insert_edge(const long long edge_id, const long long from, long long to, const long long label,
                         const long long timestamp,
                         const long long expiration_time) {
        // Check if the edge already exists in the adjacency list
        for (auto &[to_vertex, existing_edge]: adjacency_list[from]) {
            if (existing_edge->label == label && to_vertex == to) {
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
            adjacency_list[from] = std::vector<std::pair<long long, sg_edge *> >();
        }
        adjacency_list.at(from).emplace_back(to, edge);
        if (reverse_adjacency_list[to].empty()) {
            reverse_adjacency_list[to] = std::vector<std::pair<long long, sg_edge *> >();
        }
        reverse_adjacency_list[to].emplace_back(from, edge);

        if (out_degree.find(from) == out_degree.end()) {
            out_degree[from] = 0;
            if (in_degree.find(from) == in_degree.end()) {
                in_degree[from] = 0;
            }
            vertex_num++;
        }
        if (in_degree.find(to) == in_degree.end()) {
            in_degree[to] = 0;
            if (out_degree.find(to) == out_degree.end()) {
                out_degree[to] = 0;
            }
            vertex_num++;
        }
        out_degree[from]++;
        in_degree[to]++;

        // Store edge endpoints for future rank updates
        edge_endpoints[edge_id] = {from, to};

        return edge;
    }

    bool remove_edge(long long from, long long to, long long label, long long current_time) {
        if (adjacency_list.find(from) == adjacency_list.end() || adjacency_list[from].empty()) {
            return false;
        }

        auto &edges = adjacency_list[from];
        for (auto it = edges.begin(); it != edges.end(); ++it) {
            if (sg_edge *edge = it->second; it->first == to && edge->label == label) {

                // Remove from edge_id_to_edge map
                edge_id_to_edge.erase(edge->id);

                // Remove from reverse adjacency list
                if (auto rit = reverse_adjacency_list.find(to); rit != reverse_adjacency_list.end()) {
                    auto& rev = rit->second;
                    for (auto rj = rev.begin(); rj != rev.end(); ++rj) {
                        if (rj->second == edge) {
                            rev.erase(rj);
                            break;
                        }
                    }
                    if (rev.empty()) reverse_adjacency_list.erase(rit);
                }

                // Remove the edge from the adjacency list
                bool should_erase_from = false;
                edges.erase(it);
                edge_num--;
                if (label == first_transition) EINIT_count--;
                if (edges.empty()) {
                    should_erase_from = true;
                }

                // Update degrees
                out_degree[from]--;
                in_degree[to]--;

                if (out_degree[from] == 0 && in_degree[from] == 0) {
                    out_degree.erase(from);
                    in_degree.erase(from);
                    vertex_num--;
                }

                if (out_degree[to] == 0 && in_degree[to] == 0) {
                    out_degree.erase(to);
                    in_degree.erase(to);
                    if (adjacency_list.count(to)) {
                        adjacency_list.erase(to);
                    }
                    vertex_num--;
                }

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

    void expire(long long eviction_time, std::vector<expired_edge_info>& deleted_edges) {
        while (time_list_head) {
            sg_edge* cur = time_list_head->edge_pt;
            // edges are ordered oldest-first; stop when the front edge is still alive
            if (cur->timestamp >= eviction_time)
                break;

            deleted_edges.push_back({cur->s, cur->d, cur->label, cur->id});

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

    // return a vector of pointers instead of copies
    std::vector<sg_edge*> get_all_suc_ptrs(long long s) {
        std::vector<sg_edge*> sucs;
        auto it = adjacency_list.find(s);
        if (it == adjacency_list.end()) return sucs;
        for (auto& [to, edge] : it->second)
            sucs.emplace_back(edge);
        return sucs;
    }

    // return all edges pointing TO d: each edge has edge->s = predecessor, edge->d = d
    std::vector<sg_edge*> get_all_pred_ptrs(long long d) {
        std::vector<sg_edge*> preds;
        auto it = reverse_adjacency_list.find(d);
        if (it == reverse_adjacency_list.end()) return preds;
        for (auto& [from, edge] : it->second)
            preds.emplace_back(edge);
        return preds;
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

    std::map<unsigned int, unsigned int> get_degree_map(const long long vertex_id) {
        std::map<unsigned int, unsigned int> degree_map;
        for (auto [edge_id, edge_ptr] : adjacency_list[vertex_id]) {
            if (const long long label = edge_ptr->label; degree_map.find(label) != degree_map.end())
                degree_map[label]++;
            else
                degree_map[label] = 1;
        }
        return degree_map;
    };

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