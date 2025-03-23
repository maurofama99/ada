#pragma once
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <cmath>
#include <queue>
#include <limits>

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

    sg_edge(const long long id_, const long long src, const long long dst, const long long label_, const long long time) {
        id = id_;
        s = src;
        d = dst;
        timestamp = time;
        label = label_;
        time_pos = nullptr;
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

class streaming_graph {
public:
    unordered_map<long long, vector<pair<long long, sg_edge *> > > adjacency_list;

    long long edge_num=0; // number of edges in the window
    long long vertex_num=0; // number of vertices in the window
    timed_edge *time_list_head; // head of the time sequence list;
    timed_edge *time_list_tail; // tail of the time sequence list
    // key: pair ts open, ts close, value: adjacency list
    std::unordered_map<std::pair<long long, long long>, unordered_map<long long, vector<pair<long long, sg_edge *> > >, pair_hash_aj > backup_aj;

    // Z-score computation
    double mean = 0;
    double m2 = 0;
    unordered_map<long long, long long> density;
    long long slide_threshold = 10;
    long long saved_edges = 0;

    explicit streaming_graph() {
        edge_num = 0;
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

    void add_timed_edge_inorder(timed_edge *cur) // append an edge to the time sequence list
{
        if (!time_list_head) {
            time_list_head = cur;
            time_list_tail = cur;
        } else {
            timed_edge *temp = time_list_head;
            while (temp && temp->edge_pt->timestamp < cur->edge_pt->timestamp) {
                temp = temp->next;
            }
            if (!temp) {
                // Insert at the end
                time_list_tail->next = cur;
                cur->prev = time_list_tail;
                time_list_tail = cur;
            } else if (temp == time_list_head) {
                // Insert at the beginning
                cur->next = time_list_head;
                time_list_head->prev = cur;
                time_list_head = cur;
            } else {
                // Insert in the middle
                cur->next = temp;
                cur->prev = temp->prev;
                temp->prev->next = cur;
                temp->prev = cur;
            }
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

    sg_edge* insert_edge(long long edge_id, const long long from,  long long to, const long long label, const long long timestamp,
                         const long long expiration_time) {

        // Check if the edge already exists in the adjacency list
        for (auto &[to_vertex, existing_edge]: adjacency_list[from]) {
            if (existing_edge->label == label && to_vertex == to) {
                return nullptr;
            }
        }

        edge_num++;;
        // Otherwise, create a new edge
        auto *edge = new sg_edge(edge_id, from, to, label, timestamp);
        edge->expiration_time = expiration_time;

        // Add the edge to the adjacency list if it doesn't exist
        if (adjacency_list[from].empty()) {
            vertex_num++;
            adjacency_list[from] = vector<pair<long long, sg_edge *> >();
        }
        adjacency_list[from].emplace_back(to, edge);

        // update z score
        density[from]++;
        // cout << "density: " << density[from] << ", z_score: " << get_zscore(from) << endl;

        if (edge_num == 1) {
            mean = density[from];
            m2 = 0;
        } else {
            double old_mean = mean;
            mean += (density[from] - mean) / edge_num;
            m2 += (density[from] - old_mean) * (density[from] - mean);
        }

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

                // cout << "Removing edge (" << from << ", " << to << ", " << label << ")" << endl;
                // Remove the edge from the adjacency list
                edges.erase(it);

                // update z-score computation
                double old_mean = mean;
                mean -= (mean - density[from]) / (edge_num - 1);
                m2 -= (density[from] - old_mean) * (density[from] - mean);

                density[from]--;
                edge_num--;

                // If the vertex has no more edges, remove it from the adjacency list
                if (edges.empty()) {
                    adjacency_list.erase(from);
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

    map<long long, long long> get_src_degree(long long s) {
        map<long long, long long> degree_map;
        for (const auto &[_, edge]: adjacency_list[s]) {
            degree_map[edge->label]++;
        }
        return degree_map;
    }

    /**
     * Find the vertexes at distance lower than the threshold
     * from start vertex using DFS algorithm,
     * checking the timestamp of the edges to find a connection with a recent window.
     **/
    bool dfs_with_threshold(long long start, long long threshold, long long min_timestamp) {
        std::unordered_map<long long, bool> visited;
        return dfs_with_threshold_rec(start, 0, threshold, min_timestamp, visited);
    }

    bool dfs_with_threshold_rec(long long current_vertex, long long current_distance, long long threshold, long long min_timestamp, std::unordered_map<long long, bool> &visited) {
        if (current_distance > threshold) {
            return false;
        }

        visited[current_vertex] = true;

        for (const auto& [neighbor, edge] : adjacency_list[current_vertex]) {
            if (edge->timestamp > min_timestamp) {
                return true;
            }
            if (!visited[neighbor]) {
                if (dfs_with_threshold_rec(neighbor, current_distance + 1, threshold, min_timestamp, visited)) {
                    return true;
                }
            }
        }

        return false;
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

    void deep_copy_adjacency_list(long long ts_open, long long ts_close) {
        unordered_map<long long, vector<pair<long long, sg_edge *> > > copy;
        for (const auto &[from, edges]: adjacency_list) {
            vector<pair<long long, sg_edge *> > edges_copy;
            for (const auto &[to, edge]: edges) {
                auto *edge_copy = new sg_edge(*edge);
                edges_copy.emplace_back(to, edge_copy);
            }
            copy[from] = edges_copy;
        }
        auto key = std::make_pair(ts_open, ts_close);
        backup_aj[key] = copy;
    }

    void delete_expired_adj(long long ts_open, long long ts_close) {
        auto it = backup_aj.find(std::make_pair(ts_open, ts_close));
        if (it != backup_aj.end()) {
            backup_aj.erase(it);
        }
    }
};
