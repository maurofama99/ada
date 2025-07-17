#include <vector>
#include <string>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <numeric>

namespace fs = std::filesystem;

#include "source/fsa.h"
#include "source/rpq_forest.h"
#include "source/streaming_graph.h"
#include "source/query_handler.h"

using namespace std;

typedef struct Config {
    std::string input_data_path;
    bool adaptive;
    long long size;
    long long slide;
    long long query_type;
    std::vector<long long> labels;
    int max_size;
    int min_size;
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
    config.input_data_path = configMap["input_data_path"];
    config.adaptive = std::stoi(configMap["adaptive"]);

    config.size = std::stoi(configMap["size"]);
    config.slide = std::stoi(configMap["slide"]);
    config.max_size = std::stoi(configMap["max_size"]);
    config.min_size = std::stoi(configMap["min_size"]);
    config.query_type = std::stoi(configMap["query_type"]);

    std::istringstream extraArgsStream(configMap["labels"]);
    std::string arg;
    while (std::getline(extraArgsStream, arg, ',')) {
        config.labels.push_back(std::stoi(arg));
    }

    return config;
}

class DensityTracker {
public:
    void addDensity(double value) {
        auto pos = std::lower_bound(sorted.begin(), sorted.end(), value);
        sorted.insert(pos, value);

        // maintain temporal order
        order.push_back(value);

        if (order.size() > maxDensitySize) {
            double oldest = order.front();
            order.pop_front();
            auto it = std::lower_bound(sorted.begin(), sorted.end(), oldest);
            if (it != sorted.end() && *it == oldest) {
                sorted.erase(it);
            }
        }
    }

    void addResult(double value) {
        results.push_back(value);
        if (results.size() > maxResultSize) {
            results.pop_front();
        }
    }

    [[nodiscard]] double zScore(double x) const {
        if (sorted.empty()) return 0.0;

        double mean = std::accumulate(sorted.begin(), sorted.end(), 0.0) / sorted.size();

        double sum_sq_diff = 0.0;
        for (double val : sorted) {
            double diff = val - mean;
            sum_sq_diff += diff * diff;
        }

        double stddev = std::sqrt(sum_sq_diff / sorted.size());

        if (stddev == 0.0) return 0.0; // Evita divisione per zero

        return (x - mean) / stddev;
    }

    [[nodiscard]] double medianResult() const {
        if (results.empty()) return 0.0;
        std::vector<double> temp(results.begin(), results.end());
        std::sort(temp.begin(), temp.end());
        size_t n = temp.size();
        if (n % 2 == 0) {
            return (temp[n / 2 - 1] + temp[n / 2]) / 2.0;
        }
        return temp[n / 2];
    }

    [[nodiscard]] const std::vector<double>& getSorted() const {
        return sorted;
    }

private:
    static constexpr size_t maxDensitySize = 30;
    static constexpr size_t maxResultSize = 30;
    std::vector<double> sorted;
    std::deque<double> order;
    std::deque<double> results;
};

class window {
public:
    long long t_open;
    long long t_close;
    timed_edge *first;
    timed_edge *last;
    bool evicted = false;
    clock_t start_time; // start time of the window
    double latency = 0.0; // latency of the window
    double normalized_latency = 0.0; // normalized latency of the window

    double max_degree = 0.0;

    long long elements_count = 0;
    int window_matches = 0;

    int total_matched_results = 0;
    int emitted_results = 0;

    // Constructor
    window(long long t_open, long long t_close, timed_edge *first, timed_edge *last) {
        this->t_open = t_open;
        this->t_close = t_close;
        this->first = first;
        this->last = last;
        this->start_time = clock();
    }
};

int setup_automaton(long long query_type, FiniteStateAutomaton *aut, const vector<long long> &labels);

int main(int argc, char *argv[]) {
    fs::path exe_path = fs::canonical(fs::absolute(argv[0]));
    fs::path exe_dir = exe_path.parent_path();

    string config_path = argv[1];
    config config = readConfig(config_path);

    long long size = config.size;
    long long slide = config.slide;
    long long max_size = config.max_size;
    long long min_size = config.min_size;
    long long query_type = config.query_type;

    fs::path data_path = exe_dir / config.input_data_path;
    data_path = fs::absolute(data_path).lexically_normal();

    std::ifstream fin(data_path);
    if (!fin.is_open()) {
        std::cerr << "Error: Failed to open " << data_path << std::endl;
        return 1;
    }

    auto *f = new Forest();
    auto *sg = new streaming_graph(2.5);
    auto *sink = new Sink();
    auto *aut = new FiniteStateAutomaton();

    f->possible_states = setup_automaton(query_type, aut, config.labels);

    int first_transition = config.labels[0];

    auto query = new QueryHandler(*aut, *f, *sg, *sink); // Density-based Retention

    vector<window> windows;
    int total_elements_count = 0;

    long long t0 = 0;
    long long edge_number = 0;
    long long time;
    long long timestamp;

    long long last_window_index = 0;
    long long window_offset = 0;
    windows.emplace_back(0, size, nullptr, nullptr);

    bool evict = false;
    vector<size_t> to_evict;
    long long last_t_open = -1;

    // long long checkpoint = 9223372036854775807L;
    long long checkpoint = 1000000;

    vector<long long> node_count;
    long long saved_edges = 0;

    std::string mode = config.adaptive ? "ad" : "sl";
    bool ADAPTIVE_WINDOW = config.adaptive > 0;
    DensityTracker density_tracker;
    static double cumulative_size = 0.0;
    static long long size_count = 0;
    double avg_size = 0;
    int EINIT_count = 0;
    double cost_max = 0.0;
    double cost_min = std::numeric_limits<double>::max();
    double lat_max = 0.0;
    double lat_min = std::numeric_limits<double>::max();
    double cost_norm;

    int warmup = 0;
    const int adap_rate = 10;
    int adap_count = adap_rate;
    double last_cost = -1;

    std::ofstream sizes_file("tuples_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" + std::to_string(slide) + "_" + mode + ".csv");
    sizes_file << "window_size,timestamp,mean_degree,incremental_matches\n";

    std::ofstream csv_summary("summary_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" + std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size) + ".csv");
    csv_summary << "total_edges,matches,exec_time,windows_created,avg_window_cardinality,avg_window_size\n";

    std::ofstream csv_file("window_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" + std::to_string(slide) + "_" + mode + ".csv");
    csv_file << "index,t_open,t_close,window_results,incremental_matches,latency,window_size\n";

    std::ofstream cost_csv("window_cost_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" + std::to_string(slide) + "_" + mode + ".csv");
    cost_csv << "state_size,estimated_cost,normalized_estimated_cost,latency,normalized_latency,widow_size\n";

    std::ofstream state_csv("state_size_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" + std::to_string(slide) + "_" + mode + ".csv");
    state_csv << "nodes_count,trees_count,trees\n";

    std::ofstream tuple_latency("tuples_latency_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" + std::to_string(slide) + "_" + mode + ".csv");
    tuple_latency << "tuple_latency\n";

    clock_t start = clock();
    long long s, d, l, t;
    while (fin >> s >> d >> l >> t) {
        clock_t start_tuple = clock();
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

        if (l == first_transition) EINIT_count++;

        long long window_close;
        double base = std::floor(static_cast<double>(time) / slide) * slide;
        double o_i  = base;
        do {
            auto window_open  = static_cast<long long>(o_i);
            window_close = static_cast<long long>(o_i + size);

            if (windows[last_window_index].t_close < window_close) { // new window opens
                // update metrics for window adaptation
                if (last_window_index >=1)
                    windows[last_window_index].window_matches = sink->matched_paths - windows[last_window_index-1].window_matches;
                else
                    windows[last_window_index].window_matches = sink->matched_paths;

                windows[last_window_index].total_matched_results = sink->matched_paths; // matched paths until this window
                windows[last_window_index].emitted_results = sink->getResultSetSize(); // paths emitted on this window close
                windows[last_window_index].window_matches = windows[last_window_index].emitted_results;

                //density_tracker.addDensity(sg->mean);
                //density_tracker.addResult(windows[last_window_index].window_matches);

                windows.emplace_back(window_open, window_close, nullptr, nullptr);
                last_window_index++;

            }
            o_i += slide;
        } while (o_i < time);

        // if the last-created window has closing time smaller than the active window, maintain the active window
        if (window_close < windows[last_window_index].t_close) window_close = windows[last_window_index].t_close;

        // try what happens if the next element is the next slide and the size changes

        // try what happens after a couple of slides when the size changed

        // cout << "window close : " << window_close << ", time: " << time << ", edge number: " << edge_number << endl;

        timed_edge *t_edge;
        sg_edge *new_sgt;

        new_sgt = sg->insert_edge(edge_number, s, d, l, time, window_close);

        if (!new_sgt) {
            // search for the duplicate
            cerr << "ERROR: new sgt is null, time: " << time << endl;
            exit(1);
        }

        // add edge to time list
        t_edge = new timed_edge(new_sgt); // associate the timed edge with the snapshot graph edge
        sg->add_timed_edge(t_edge); // append the element to the time list

        // update window boundaries and check for window eviction
        for (size_t i = window_offset; i < windows.size(); i++) {
            if (windows[i].t_open <= time && time < windows[i].t_close) { // active window
                if (!windows[i].first || time < windows[i].first->edge_pt->timestamp) {
                    if (!windows[i].first) windows[i].last = t_edge;
                    windows[i].first = t_edge;
                    windows[i].elements_count++;
                    total_elements_count++;
                } else if (!windows[i].last || time >= windows[i].last->edge_pt->timestamp) {
                    windows[i].last = t_edge;
                    windows[i].elements_count++;
                    total_elements_count++;
                }
                if (sg->density[s] > windows[i].max_degree) {
                    windows[i].max_degree = sg->density[s];
                }
            } else if (time >= windows[i].t_close) { // schedule window for eviction
                window_offset = i + 1;
                to_evict.push_back(i);
                evict = true;
                if (windows[i].elements_count == 0) {
                    cerr << "ERROR: Empty window: " << i << endl;
                    exit(1);
                }
            }
        }
        new_sgt->time_pos = t_edge; // associate the snapshot graph edge with the timed edge

        cumulative_size += size;
        size_count++;
        avg_size = cumulative_size / size_count;

        sizes_file << size << "," << timestamp+t0 << "," << sg->mean << "," << sink->matched_paths << endl;

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

            if (last_t_open != windows[to_evict[0]].t_open) sink->refresh_resultSet(windows[to_evict[0]].t_open);
            last_t_open = windows[to_evict[0]].t_open;

            timed_edge *current = evict_start_point;

            while (current && current != evict_end_point) {

                auto cur_edge = current->edge_pt;
                auto next = current->next;

                if (cur_edge->label == first_transition) EINIT_count--;

                candidate_for_deletion.emplace_back(cur_edge->s, cur_edge->d); // schedule for deletion from RPQ forest
                if (time >= cur_edge->expiration_time) {
                    sg->remove_edge(cur_edge->s, cur_edge->d, cur_edge->label); // delete from adjacency list
                }
                sg->delete_timed_edge(current); // delete from time list

                current = next;
            }

            // reset time list pointers
            sg->time_list_head = evict_end_point;
            sg->time_list_head->prev = nullptr;

            f->expire_timestamped(to_evict_timestamp, candidate_for_deletion);

            // mark window as evicted
            for (unsigned long i: to_evict) {
                warmup++;
                windows[i].evicted = true;
                windows[i].first = nullptr;
                windows[i].last = nullptr;
                windows[i].latency = static_cast<double>(clock() - windows[i].start_time) / CLOCKS_PER_SEC;
                if (windows[i].latency > lat_max) lat_max = windows[i].latency;
                if (windows[i].latency < lat_min) lat_min = windows[i].latency;
                windows[i].normalized_latency = (windows[i].latency - lat_min) / (lat_max - lat_min);
                // state_size,estimated_cost,normalized_estimated_cost,latency,normalized_latency
            }

            to_evict.clear();
            candidate_for_deletion.clear();
            evict = false;

            state_csv << f->node_count << "," << f->trees_count << "," << f->trees.size() << endl;
        }

        if (ADAPTIVE_WINDOW && last_window_index >= adap_count) {
            adap_count += adap_rate;

            // max degree computation
            double max_deg = 0;
            for (size_t i = window_offset; i < windows.size(); i++) {
                if (windows[i].max_degree > max_deg) max_deg = windows[i].max_degree;
            }

            double n = 0;
            if (EINIT_count > edge_number) cerr << "ERROR: more initial transitions than edges." << endl;
            for (int i = 0; i < EINIT_count; i++) {
                n += sg->edge_num-i;
            }
            double cost = n / max_deg;
            // cout << "Cost: " << cost << ", EINIT count: " << EINIT_count << ", edge number: " << edge_number << ", n: " << n << ", max degree: " << max_deg << endl;

            double alpha = 1; // smoothing factor for cost normalization
            double decay_rate = 0.001;
            if (cost > cost_max) cost_max = (cost * alpha) + (1-alpha) * cost_max;
            if (cost < cost_min) cost_min = (cost * alpha) + (1-alpha) * cost_min;

            cost_max *= (1 - decay_rate);
            cost_min *= (1 + decay_rate);
            cost_norm = (cost - cost_min) / (cost_max - cost_min);
            if (last_cost == -1) last_cost = cost_norm;

            double cost_diff = last_cost - cost_norm;
            last_cost = cost_norm;
            size = size + ceil((cost_diff*10) * slide);

            // cap to max and min size
            size = std::max(std::min(size, max_size), min_size);

            // state_size,estimated_cost,normalized_estimated_cost,latency,normalized_latency,widow_size
            cost_csv << f->node_count << "," << cost << "," << cost_norm << "," << windows[last_window_index].latency << "," << windows[last_window_index].normalized_latency << "," << windows[last_window_index].t_close - windows[last_window_index].t_open << endl;
        }

        /* QUERY */
        query->pattern_matching_tc(new_sgt);

        if (edge_number % checkpoint == 0) {
            printf("processed edges: %lld\n", edge_number);
            printf("saved edges: %lld\n", saved_edges);
            printf("avg degree: %f\n", sg->mean);
            cout << "matched paths: " << sink->matched_paths << "\n\n";
        }
        clock_t end_tuple = clock();
        tuple_latency << static_cast<double>(1000* (end_tuple - start_tuple)) / CLOCKS_PER_SEC << "\n";
    }

    cout << "last window index: " << last_window_index << endl;

    clock_t finish = clock();
    long long time_used = (double) (finish - start) / CLOCKS_PER_SEC;

    double avg_window_size = static_cast<double>(total_elements_count) / windows.size();

    csv_summary <<  edge_number << ",";
    csv_summary <<  sink->matched_paths << ",";
    csv_summary <<  time_used << ",";
    csv_summary <<  windows.size() << ",";
    csv_summary <<  avg_window_size << ",";
    csv_summary <<  avg_size << "\n";

    for (size_t i = 0; i < windows.size(); ++i) {
        csv_file << i << ","
        << windows[i].t_open << ","
        << windows[i].t_close << ","
        << windows[i].emitted_results << ","
        << windows[i].total_matched_results << ","
        << windows[i].latency << ","
        << windows[i].t_close - windows[i].t_open << "\n";
    }

    sizes_file.close();
    csv_file.close();
    cost_csv.close();
    state_csv.close();


    // cleanup
    delete sg;
    delete f;
    delete sink;
    delete aut;
    delete query;

 return 0;
}

// Set up the automaton correspondant for each query
int setup_automaton(long long query_type, FiniteStateAutomaton *aut, const vector<long long> &labels) {
    int states_count = 0;
    /*
     * 0 - initial state
     * 0 -> 1 - first transition
     * Always enumerate the states starting from 0 and incrementing by 1.
     */
    switch (query_type) {
        case 1: // a+
            aut->addFinalState(1);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 1, labels[0]);
            states_count = 2;
            break;
        case 5: // ab*c
            aut->addFinalState(2);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 1, labels[1]);
            aut->addTransition(1, 2, labels[2]);
            states_count = 3;
            break;
        case 7: // abc*
            aut->addFinalState(2);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 2, labels[1]);
            aut->addTransition(2, 2, labels[2]);
            states_count = 3;
            break;
        case 4: // (abc)+
            aut->addFinalState(3);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 2, labels[1]);
            aut->addTransition(2, 3, labels[2]);
            aut->addTransition(3, 1, labels[0]);
            states_count = 4;
            break;
        case 2: // ab*
            aut->addFinalState(1);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 1, labels[1]);
            states_count = 2;
            break;
        case 10: // (a|b)c*
            aut->addFinalState(1);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(0, 1, labels[1]);
            aut->addTransition(1, 1, labels[2]);
            states_count = 2;
            break;
        case 6: // a*b*
            aut->addFinalState(1);
            aut->addFinalState(2);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 1, labels[0]);
            aut->addTransition(1, 2, labels[1]);
            aut->addTransition(0, 2, labels[1]);
            aut->addTransition(2, 2, labels[1]);
            states_count = 3;
            break;
        case 3: // ab*c*
            aut->addFinalState(1);
            aut->addFinalState(2);
            aut->addTransition(0, 1, labels[0]);
            aut->addTransition(1, 1, labels[1]);
            aut->addTransition(1, 2, labels[2]);
            aut->addTransition(2, 2, labels[2]);
            states_count = 3;
            break;
        default:
            cerr << "ERROR: Wrong query type" << endl;
            exit(1);
    }
    return states_count;
}



/*
 *
 *if not !newsgt
*            auto existing_edge = sg->search_existing_edge(s, d, l);
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
 *
 *
 *
 *
 *
 *
 *
 *
 *

   double R_target = 922337203685477;
    double C_target = 0.0;
    double smoothed_delta = 0.0;
    /// SPIKY BURSTS

    const double RESULT_THRESHOLD = 0.6;  // More sensitive to result drops
    const double COST_THRESHOLD = 1.2;    // Tolerate less cost increase
    const double EMA_ALPHA = 0.4;         // Slower reaction for stability
    const double HYSTERISIS_THRESHOLD = 0.2; // Less sensitive hysteresis

    const double RESULT_THRESHOLD = 0.75;
    const double COST_THRESHOLD = 1.4;
    const double EMA_ALPHA = 0.1;
    const double HYSTERISIS_THRESHOLD = 0.3;


const int TREND_WINDOW = 5;
const double BASELINE_UPDATE_THRESHOLD = 0.6;

std::vector<double> warmup_results;
std::vector<double> warmup_costs;
const int WARMUP_WINDOWS = 10;
std::deque<double> recent_results;
std::deque<double> recent_costs;
 if (i < WARMUP_WINDOWS) {
                        warmup_results.push_back(windows[i].window_matches);
                        warmup_costs.push_back(windows[i].window_zscore);

                        if (i == WARMUP_WINDOWS - 1) {
                            // Compute median to reduce outlier impact
                            R_target = median(warmup_results) * 0.8; //80% peak results
                            C_target = median(warmup_costs) * 1.1; // 10% cost buffer
                        }
                        continue;
                    }

                    recent_results.push_back(windows[i].window_matches);
                    recent_costs.push_back(windows[i].window_zscore);
                    if (recent_results.size() > TREND_WINDOW) {
                        recent_results.pop_front();
                        recent_costs.pop_front();
                    }

                    double avg_results = std::accumulate(recent_results.begin(), recent_results.end(), 0.0) / TREND_WINDOW;
                    double avg_costs = std::accumulate(recent_costs.begin(), recent_costs.end(), 0.0) / TREND_WINDOW;

                    if (i % TREND_WINDOW == 0) {  // update baseline in stable state
                        if (avg_results/R_target < BASELINE_UPDATE_THRESHOLD ||
                            avg_costs/C_target > 1/BASELINE_UPDATE_THRESHOLD) {

                            R_target = 0.2 * R_target + 0.8 * avg_results;
                            C_target = 0.2 * C_target + 0.8 * avg_costs;
                        }
                    }

                    double result_ratio = avg_results / R_target;
                    double cost_ratio = avg_costs / C_target;

                    double delta = 0;

                    if (result_ratio < RESULT_THRESHOLD) {
                        double deficiency = (RESULT_THRESHOLD - result_ratio)/RESULT_THRESHOLD;
                        delta += slide * deficiency * (1 + (1 - result_ratio));
                    }
                    if (cost_ratio > COST_THRESHOLD) {
                        double excess = (cost_ratio - COST_THRESHOLD)/COST_THRESHOLD;
                        delta -= slide * excess * (1 + (cost_ratio - 1.0));
                    }

                    // EMA
                    smoothed_delta = ((1-EMA_ALPHA) * smoothed_delta) + (EMA_ALPHA * delta);

                    double dynamic_hysteresis = HYSTERISIS_THRESHOLD *
                          (1 + std::abs(smoothed_delta)/slide);

                    // Apply hysteresis to prevent micro-adjustments
                    if (std::abs(smoothed_delta) > (slide * dynamic_hysteresis)){
                        size = std::clamp(
                            size + static_cast<long long>(std::round(smoothed_delta)),
                            min_size,  // min size
                            max_size   // max size
                        );
                        if (size < min_size_observed) min_size_observed = size;
                        if (size > max_size_observed) max_size_observed = size;
                        if (std::abs(smoothed_delta) > slide) {
                            smoothed_delta *= 0.2;
                        }
                    }










if (ADAPTIVE_WINDOW && i > 30) {
                    double z = density_tracker.zScore(windows[i].window_zscore);
                    long long delta = std::abs(z)*slide;
                    if (z < -2) { // 1.9
                        if (windows[i].window_matches < 0.5*density_tracker.medianResult()) size = std::min(max_size, size + delta);
                    } else if (z > 1.5) { // 1.2
                        if (windows[i].window_matches > 1.5*density_tracker.medianResult()) size = std::max(min_size, size - delta);
                    }
                }
                    */
