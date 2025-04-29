#include<vector>
#include<string>
#include<ctime>
#include<cstdlib>
#include <fstream>
#include <cmath>
#include <numeric>
#include <sstream>

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

double normalize_shift(double shift, double min_shift, double max_shift, double alpha) {
    if (max_shift == min_shift) return 0.0; // avoid division by zero
    double normalized = (shift - min_shift) / (max_shift - min_shift); // [0, 1]
    return normalized * alpha; // [0, alpha]
}

vector<long long> setup_automaton(long long query_type, FiniteStateAutomaton *aut, const vector<long long> &labels);

int main(int argc, char *argv[]) {
    string config_path = argv[1];
    config config = readConfig(config_path.c_str());

    long long algorithm = config.algorithm;
    ifstream fin(config.input_data_path.c_str());
    string output_base_folder = config.output_base_folder;
    long long size = config.size;
    long long slide = config.slide;
    long long query_type = config.query_type;
    double zscore = config.zscore;
    int lives = config.lives;
    bool BACKWARD_RETENTION = zscore != 0;

    long long watermark = config.watermark;
    long long ooo_strategy = config.ooo_strategy; // 0: Copy State, 1: Window Replay
    long long current_time = 0;
    bool handle_ooo = false;

    if (fin.fail()) {
        cerr << "Error opening file" << endl;
        exit(1);
    }

    if (algorithm != 3 and watermark != 0) {
        watermark = 0;
    }

    auto *aut = new FiniteStateAutomaton();
    vector<long long> scores = setup_automaton(query_type, aut, config.labels);
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

    long long checkpoint = 10000;

    /*
    double candidate_rate = 0.2;
    double benefit_threshold = 1.5;
    for (long long i = 0; i < scores.size(); i++)
        f2->aut_scores[i] = scores.at(i);
        */

    vector<long long> node_count;
    long long saved_edges = 0;

    std::vector<std::tuple<long long, long long, long long, long long>> edges;
    std::string line;

    while (std::getline(fin, line)) {
        std::istringstream iss(line);
        long long s, d, l, t;
        if (iss >> s >> d >> l >> t) {
            edges.emplace_back(s, d, l, t);
        }
    }

    auto query = new QueryHandler(*aut, *f, *sg, *sink); // Density-based Retention

    clock_t start = clock();
    for (const auto& [s, d, l, t] : edges) {
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
        handle_ooo = false;
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
        } else if (watermark_gap > 0 && watermark_gap <= watermark) {
            // out-of-order element before watermark expiration
            std::vector<std::pair<long long, long long> > windows_to_recover;
            double c_sup = ceil(static_cast<double>(time) / static_cast<double>(slide)) * slide;
            double o_i = c_sup - size;
            do {
                window_close = o_i + size;
                if (windows[last_window_index].t_close < window_close) {
                    cerr << "ERROR: OOO Window not found." << endl;
                    exit(1);
                }
                for (size_t i = last_expired_window; i < windows.size(); ++i) {
                    if (auto &win = windows[i]; win.t_open == o_i && window_close == win.t_close) {
                        if (win.evicted) windows_to_recover.emplace_back(o_i, window_close);
                        else handle_ooo = true; // true iff the element belongs to an active window
                    }
                }
                o_i += slide;
            } while (o_i <= time);

            // the element is in an expired window
            query->compute_missing_results(edge_number, s, d, l, time, window_close, windows_to_recover,
                                           windows_backup);
            if (!handle_ooo) continue;
        } else continue; // out-of-order element after watermark already expired

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

            if (watermark != 0 && existing_edge->timestamp > time) continue;

            // adjust window boundaries if needed
            for (size_t i = window_offset; i < windows.size(); i++) {
                if (windows[i].first == existing_edge->time_pos) {
                    if (windows[i].first != windows[i].last) {
                        // if the window has more than one element
                        if (existing_edge->time_pos->next) windows[i].first = existing_edge->time_pos->next;
                        else {
                            cerr << "ERROR: Time position not found." << endl;
                            exit(1);
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
                            cerr << "ERROR: Time position not found." << endl;
                            exit(1);
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
        if (algorithm == 1)  query->pattern_matching_tc(new_sgt);
        else if (algorithm == 2)  cerr << "ERROR: Query handler not implemented." << endl;
        else if (algorithm == 3)  query->pattern_matching_lc(new_sgt);

        /* EVICT */
        if (evict) {
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

                 if (cur_edge->lives == 1 || sg->get_zscore(cur_edge->s) > zscore || sg->get_zscore(cur_edge->d) > zscore) { // if (cur_edge->lives == 1 || (static_cast<double>(rand()) / RAND_MAX) < 0.005)
                     if (sg->get_zscore(cur_edge->s) > zscore || sg->get_zscore(cur_edge->d) > zscore) dense_edges++;
                    // check for parent switch before final deletion
                    candidate_for_deletion.emplace_back(cur_edge->s, cur_edge->d);
                    sg->remove_edge(cur_edge->s, cur_edge->d, cur_edge->label); // delete from adjacency list
                    sg->delete_timed_edge(current); // delete from window state store
                } else {
                    cur_edge->lives--;
                    saved_edges++;
                    /*
                    auto shift = static_cast<double>(last_window_index) - std::ceil(sg->get_zscore(cur_edge->s));
                    auto target_window_index = shift < (static_cast<double>(to_evict.back()) + 1) ? (static_cast<double>(to_evict.back()) + 1) : shift;
                    target_window_index > static_cast<double>(last_window_index) ? target_window_index = static_cast<double>(last_window_index) : target_window_index;
                    */
                    auto z_score_s = sg->get_zscore(cur_edge->s);
                    auto z_score_d = sg->get_zscore(cur_edge->d);
                    auto selected_score = z_score_s > z_score_d ? z_score_s : z_score_d;
                    double raw_shift = static_cast<double>(last_window_index) - std::ceil(selected_score);

                    // define min and max shift bounds based on your data/logic
                    auto propagation_start = static_cast<double>(size)/static_cast<double>(slide);
                    propagation_start = ceil(propagation_start/2);
                    double min_shift = static_cast<double>(to_evict.back()) + propagation_start;
                    auto max_shift = static_cast<double>(last_window_index); // or an empirical upper bound

                    // alpha is your desired max output shift
                    double resized_shift = normalize_shift(raw_shift, min_shift, max_shift, size/slide);

                    // finally compute the target index
                    auto target_window_index = std::clamp(min_shift + resized_shift, min_shift, static_cast<double>(last_window_index));

                    target_window_index = max_shift;
                    sg->shift_timed_edge(cur_edge->time_pos, windows[last_window_index].first);
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
            checkpoint += 10000;

            printf("processed edges: %lld\n", edge_number);
            printf("saved edges: %lld\n", saved_edges);
            printf("avg degree: %f\n", sg->mean);
            if (algorithm == 1) {
                cout << "resulting paths: " <<  sink->getResultSetSize() << "\n\n";
            } else if (algorithm == 3) {
                cout << "resulting paths: " << sink->getResultSetSize() << "\n\n";
            }
        }

    }

    clock_t finish = clock();
    long long time_used = (double) (finish - start) / CLOCKS_PER_SEC;

    cout << "resulting paths: " <<  sink->getResultSetSize() << "\n";
    printf("processed edges: %lld\n", edge_number);
    printf("saved edges: %lld\n", saved_edges);
    cout << "execution time: " << time_used << endl;
    cout << "windows created: " << windows.size() << "\n";
    cout << "dense edges: " << dense_edges << "\n";

    // compute average window size using the number of elements for each window in elements_count
    double avg_window_size = 0;
    for (const auto &win : windows) {
        avg_window_size += win.elements_count;
    }
    avg_window_size /= windows.size();
    cout << "avg window size: " << avg_window_size << "\n";

    delete sg;
    return 0;
}

// Set up the automaton correspondant for each query
vector<long long> setup_automaton(long long query_type, FiniteStateAutomaton *aut, const vector<long long> &labels) {
    vector<long long> scores;

    // fixme: some queries do not have minimal FSA due to final state equal to initial state bug in LM-SRPQ
    switch (query_type) {
        case 1: // a*
            aut->addFinalState(1);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 1, labels[0]);
            scores.emplace(scores.begin(), 6);
            break;
        case 2: // a(bc)*
            aut->addFinalState(3);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 2, labels[1]);
            aut->addTransition(2, 3, labels[2]);
            aut->addTransition(3, 2, labels[1]);
            break;
        case 3: // ab*
            aut->addFinalState(1);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 1, labels[1]);
            break;
        case 4: // (a|ab)
            aut->addFinalState(1);
            aut->addFinalState(2);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 2, labels[1]);
            break;
        case 5: // (abc)
            aut->addFinalState(3);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 2, labels[1]);
            aut->addTransition(2, 3, labels[2]);
            break;
        case 7: // (a|b|c)d*
            aut->addFinalState(1);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(0, 1, labels[1]);
            aut->addTransition(0, 1, labels[2]);
            aut->addTransition(1, 1, labels[3]);
            break;
        case 8: // a*b*
            aut->addFinalState(1);
            aut->addFinalState(2);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 1, labels[0]);
            aut->addTransition(1, 2, labels[1]);
            aut->addTransition(0, 2, labels[1]);
            aut->addTransition(2, 2, labels[1]);
            break;
        case 9: // ab*c*
            aut->addFinalState(1);
            aut->addFinalState(2);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 1, labels[1]);
            aut->addTransition(1, 2, labels[2]);
            aut->addTransition(2, 2, labels[2]);
            scores.emplace_back(0);
            scores.emplace_back(13); // 2 loops, 1 edge 1->2, thus 2*6+1 = 13
            scores.emplace_back(6);
            break;
        default:
            cout << "ERROR: Wrong query type" << endl;
            exit(1);
    }

    return scores;
}
