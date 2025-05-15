#include <vector>
#include <string>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <sstream>

namespace fs = std::filesystem;

#include "source/fsa.h"
#include "source/rpq_forest.h"
#include "source/streaming_graph.h"
#include "source/query_handler.h"

using namespace std;

typedef struct Config {
    long long algorithm;
    std::string input_data_path;
    std::string output_base_folder;
    long long size;
    long long slide;
    long long query_type;
    std::vector<long long> labels;
    double zscore;
    int lives;
    long long watermark;
    long long ooo_strategy;
} config;

config readConfig(const std::string &filename) {
    config config;
    std::ifstream file(filename);
    std::string line;

    if (!file) {
        std::cerr << "Error opening configuration file: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    std::unordered_map<std::string, std::string> configMap;

    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string key, value;
        if (std::getline(ss, key, '=') && std::getline(ss, value)) {
            configMap[key] = value;
        }
    }

    // Convert values from the map
    config.algorithm = std::stoi(configMap["algorithm"]);
    config.input_data_path = configMap["input_data_path"];
    config.output_base_folder = configMap["output_base_folder"];
    config.size = std::stoi(configMap["size"]);
    config.slide = std::stoi(configMap["slide"]);
    config.query_type = std::stoi(configMap["query_type"]);

    // Parse extra_args
    std::istringstream extraArgsStream(configMap["labels"]);
    std::string arg;
    while (std::getline(extraArgsStream, arg, ',')) {
        config.labels.push_back(std::stoi(arg));
    }

    config.zscore = std::stod(configMap["zscore"]);
    config.watermark = std::stoi(configMap["watermark"]);
    config.ooo_strategy = std::stoi(configMap["ooo_strategy"]);
    config.lives = std::stoi(configMap["lives"]);

    return config;
}

class window {
public:
    long long t_open;
    long long t_close;
    timed_edge *first;
    timed_edge *last;
    bool evicted = false;
    long long elements_count = 0;

    // Constructor
    window(long long t_open, long long t_close, timed_edge *first, timed_edge *last) {
        this->t_open = t_open;
        this->t_close = t_close;
        this->first = first;
        this->last = last;
    }
};

/// Map score ∈ [minScore…maxScore] into an index ∈ [firstIndex…lastIndex].
/// Lower scores → values near lastIndex; higher scores → values near firstIndex.
int computeShiftedIndex(int firstIndex, int lastIndex,
                        double score,
                        double minScore, double maxScore) {
    // sanity‑check
    if (minScore >= maxScore) {
        cerr << "ERROR: minScore must be smaller than maxScore." << endl;
        exit(1);
    }
    // ensure firstIndex≤lastIndex
    if (firstIndex > lastIndex) {
        std::swap(firstIndex, lastIndex);
    }
    // clamp score into [minScore…maxScore]
    score = std::clamp(score, minScore, maxScore);

    // normalize into [0…1]
    double t = (score - minScore) / (maxScore - minScore);

    // we want t=0 → lastIndex, t=1 → firstIndex
    // range equals the number of scheduled adaptations until the current time
    double range = lastIndex - firstIndex;
    double offset = (1.0 - t) * range;

    // if  (offset >= 0) cout << "range: " << range << ", offset: " << offset << endl;

    // round to nearest integer index
    return firstIndex + static_cast<int>(std::round(offset));
}


void setup_automaton(long long query_type, FiniteStateAutomaton *aut, const vector<long long> &labels);

int main(int argc, char *argv[]) {
    fs::path exe_path = fs::canonical(fs::absolute(argv[0]));
    fs::path exe_dir = exe_path.parent_path();

    string config_path = argv[1];
    config config = readConfig(config_path);

    long long algorithm = config.algorithm;
    // ifstream fin(config.input_data_path.c_str());
    string output_base_folder = config.output_base_folder;
    long long size = config.size;
    long long slide = config.slide;
    long long query_type = config.query_type;
    double zscore = config.zscore;
    int lives = config.lives;

    long long watermark = config.watermark;
    long long current_time = 0;

    fs::path data_path = exe_dir / config.input_data_path;
    data_path = fs::absolute(data_path).lexically_normal();

    // 4. Open the data file
    std::ifstream fin(data_path);
    if (!fin.is_open()) {
        std::cerr << "Error: Failed to open " << data_path << std::endl;
        return 1;
    }

    if (algorithm != 3 and watermark != 0) {
        watermark = 0;
    }

    auto *aut = new FiniteStateAutomaton();
    setup_automaton(query_type, aut, config.labels);
    auto *f = new Forest();
    auto *sg = new streaming_graph(lives);
    auto *sink = new Sink();

    vector<window> windows;

    // Buffer to store evicted windows eventually needed for out-of-order elements computation
    unordered_map<long long, vector<sg_edge *> > windows_backup;
    // key: window opening time, value: vector of elements belonging to the window.


    long long t0 = 0;
    long long edge_number = 0;
    long long time;
    long long timestamp;
    int dense_edges = 0;

    long long last_window_index = 0;
    long long window_offset = 0;
    windows.emplace_back(0, size, nullptr, nullptr);

    bool evict = false;
    vector<size_t> to_evict;
    long long last_expired_window = 0;

    long long checkpoint = 9223372036854775807L;

    vector<long long> node_count;
    long long saved_edges = 0;

    auto query = new QueryHandler(*aut, *f, *sg, *sink); // Density-based Retention

    clock_t start = clock();
    //for (const auto& [s, d, l, t] : edges) {
    long long s, d, l, t;
    while (fin >> s >> d >> l >> t) {
        // cout << "Processing edge: " << s << " -> " << d << " with label: " << l << " at time: " << t << endl;
        edge_number++;
        if (t0 == 0) {
            t0 = t;
            timestamp = 1;
        } else timestamp = t - t0;

        if (timestamp < 0) continue;
        time = timestamp;

        // process the edge if the label is part of the query
        if (!aut->hasLabel(l))
            continue;

        long long watermark_gap = current_time - time;

        long long window_close;
        if (time >= current_time) {
            // in-order element
            double c_sup = ceil(static_cast<double>(time) / static_cast<double>(slide)) * slide;
            double o_i = c_sup - size;
            do {
                window_close = o_i + size;
                if (windows[last_window_index].t_close < window_close) {
                    windows.emplace_back(o_i, window_close, nullptr, nullptr);
                    last_window_index++;
                }
                o_i += slide;
            } while (o_i < time);
        }

        timed_edge *t_edge;
        sg_edge *new_sgt;

        new_sgt = sg->insert_edge(edge_number, s, d, l, time, window_close);

        // duplicate handling in tuple list, important to not evict an updated edge
        if (!new_sgt) {
            // TODO If existing edge is not in the same window, do not shift place but instead reinsert it
            // search for the duplicate
            auto existing_edge = sg->search_existing_edge(s, d, l);
            if (!existing_edge) {
                cerr << "ERROR: Existing edge not found." << endl;
                exit(1);
            }

            // adjust window boundaries if needed
            for (size_t i = window_offset; i < windows.size(); i++) {
                if (windows[i].first == existing_edge->time_pos) {
                    if (windows[i].first != windows[i].last) {
                        // if the window has more than one element
                        if (existing_edge->time_pos->next) windows[i].first = existing_edge->time_pos->next;
                        else {
                            windows[i].first = nullptr;
                            windows[i].last = nullptr;
                        }
                    } else {
                        windows[i].first = nullptr;
                        windows[i].last = nullptr;
                    }
                }
                if (windows[i].last == existing_edge->time_pos) {
                    if (windows[i].first != windows[i].last) {
                        // if the window has more than one element
                        if (existing_edge->time_pos->prev) windows[i].last = existing_edge->time_pos->prev;
                        else {
                            windows[i].first = nullptr;
                            windows[i].last = nullptr;
                        }
                    } else {
                        windows[i].first = nullptr;
                        windows[i].last = nullptr;
                    }
                }
            }

            // update edge list (erase and re-append)
            sg->delete_timed_edge(existing_edge->time_pos);
            new_sgt = existing_edge;
            new_sgt->timestamp = time;
            new_sgt->expiration_time = window_close;
        }

        // add edge to time list
        t_edge = new timed_edge(new_sgt);
        sg->add_timed_edge(t_edge); // io: append element to last window

        // add edge to window and check for window eviction
        for (size_t i = window_offset; i < windows.size(); i++) {
            if (windows[i].t_open <= time && time < windows[i].t_close) {
                // add new sgt to window
                if (!windows[i].first || time <= windows[i].first->edge_pt->timestamp) {
                    if (!windows[i].first) windows[i].last = t_edge;
                    windows[i].first = t_edge;
                    windows[i].elements_count++;
                } else if (!windows[i].last || time > windows[i].last->edge_pt->timestamp) {
                    windows[i].last = t_edge;
                    windows[i].elements_count++;
                }
            } else if (time >= windows[i].t_close) {
                if (windows[i].elements_count == 0) cout << "WARNING: Empty window." << endl;
                // schedule window for eviction
                window_offset = i + 1;
                to_evict.push_back(i);
                evict = true;
            }
        }

        new_sgt->time_pos = t_edge;

        /* QUERY */
        if (algorithm == 1) query->pattern_matching_tc(new_sgt);
        else if (algorithm == 2) cerr << "ERROR: Query handler not implemented." << endl;
        else if (algorithm == 3) query->pattern_matching_lc(new_sgt);

        /* EVICT */
        if (evict) {
            // to compute window cost, we take the size of the snapshot graph of the window here, since no more elements will be added and it can be considered complete and closed

            std::vector<pair<long long, long long> > candidate_for_deletion;
            timed_edge *evict_start_point = windows[to_evict[0]].first;
            timed_edge *evict_end_point = windows[to_evict.back() + 1].first;
            long long to_evict_timestamp = windows[to_evict.back() + 1].t_open;

            if (!evict_start_point) {
                cerr << "ERROR: Evict start point is null." << endl;
                exit(1);
            }

            if (evict_end_point == nullptr) {
                evict_end_point = sg->time_list_tail;
                cout << "WARNING: Evict end point is null, evicting whole buffer." << endl;
            }

            timed_edge *current = evict_start_point;

            while (current && current != evict_end_point) {
                auto cur_edge = current->edge_pt;
                auto next = current->next;

                if (cur_edge->lives == 1 || sg->get_zscore(cur_edge->s) > zscore || sg->get_zscore(cur_edge->d) >
                    zscore) {
                    // if (cur_edge->lives == 1 || (static_cast<double>(rand()) / RAND_MAX) < 0.005)
                    if (sg->get_zscore(cur_edge->s) > zscore || sg->get_zscore(cur_edge->d) > zscore) dense_edges++;
                    // check for parent switch before final deletion
                    candidate_for_deletion.emplace_back(cur_edge->s, cur_edge->d);
                    sg->remove_edge(cur_edge->s, cur_edge->d, cur_edge->label); // delete from adjacency list
                    sg->delete_timed_edge(current); // delete from window state store
                } else {
                    cur_edge->lives--;
                    saved_edges++;

                    auto z_score_s = sg->get_zscore(cur_edge->s);
                    auto z_score_d = sg->get_zscore(cur_edge->d);
                    auto score = z_score_d > z_score_s ? z_score_d : z_score_s;

                    auto index = computeShiftedIndex(to_evict.back()+1, last_window_index, score, -1, zscore);

                    sg->shift_timed_edge(cur_edge->time_pos, windows[index].first);
                    windows[last_window_index].elements_count++;
                }
                current = next;
            }

            // mark window as evicted
            for (unsigned long i: to_evict) {
                windows[i].evicted = true;
                windows[i].first = nullptr;
                windows[i].last = nullptr;
            }

            // reset time list pointers
            sg->time_list_head = evict_end_point;
            sg->time_list_head->prev = nullptr;

            if (algorithm == 1) {
                f->expire_timestamped(to_evict_timestamp, candidate_for_deletion);
            } else if (algorithm == 2) {
                cerr << "ERROR: Query handler not implemented." << endl;
            } else if (algorithm == 3) {
                f->expire(candidate_for_deletion);
            }

            to_evict.clear();
            candidate_for_deletion.clear();
            evict = false;

            node_count.emplace_back(f->node_count);
        }

        if (edge_number >= checkpoint) {
            checkpoint += checkpoint;

            printf("processed edges: %lld\n", edge_number);
            printf("saved edges: %lld\n", saved_edges);
            printf("avg degree: %f\n", sg->mean);
            if (algorithm == 1) {
                cout << "resulting paths: " << sink->getResultSetSize() << "\n\n";
            } else if (algorithm == 3) {
                cout << "resulting paths: " << sink->getResultSetSize() << "\n\n";
            }
        }
    }

    clock_t finish = clock();
    long long time_used = (double) (finish - start) / CLOCKS_PER_SEC;

    cout << "resulting paths: " << sink->getResultSetSize() << "\n";
    printf("processed edges: %lld\n", edge_number);
    printf("saved edges: %lld\n", saved_edges);
    cout << "execution time: " << time_used << endl;
    cout << "windows created: " << windows.size() << "\n";
    cout << "dense edges: " << dense_edges << "\n";

    // compute average window size using the number of elements for each window in elements_count
    double avg_window_size = 0;
    for (const auto &win: windows) {
        avg_window_size += win.elements_count;
    }
    avg_window_size /= windows.size();
    cout << "avg window size: " << avg_window_size << "\n";

    delete sg;
    return 0;
}

// Set up the automaton correspondant for each query
void setup_automaton(long long query_type, FiniteStateAutomaton *aut, const vector<long long> &labels) {

    switch (query_type) {
        case 1: // a*
            aut->addFinalState(1);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 1, labels[0]);
            break;
        case 5: // ab*c
            aut->addFinalState(2);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 1, labels[1]);
            aut->addTransition(1, 2, labels[2]);
            break;
        case 7: // abc*
            aut->addFinalState(2);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 2, labels[1]);
            aut->addTransition(2, 2, labels[2]);
            break;
        case 11: // abc
            aut->addFinalState(3);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 2, labels[1]);
            aut->addTransition(2, 3, labels[2]);
            break;
        case 2: // ab*
            aut->addFinalState(1);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 1, labels[1]);
            break;
        case 10: // (a|b)c*
            aut->addFinalState(1);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(0, 1, labels[1]);
            aut->addTransition(1, 1, labels[2]);
            break;
        case 6: // a*b*
            aut->addFinalState(1);
            aut->addFinalState(2);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 1, labels[0]);
            aut->addTransition(1, 2, labels[1]);
            aut->addTransition(0, 2, labels[1]);
            aut->addTransition(2, 2, labels[1]);
            break;
        case 3: // ab*c*
            aut->addFinalState(1);
            aut->addFinalState(2);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 1, labels[1]);
            aut->addTransition(1, 2, labels[2]);
            aut->addTransition(2, 2, labels[2]);
            break;
        default:
            cerr << "ERROR: Wrong query type" << endl;
            exit(1);
    }
}
