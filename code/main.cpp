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
#include <random>

#define MEMORY_PROFILER false

#include "sys/types.h"
// #include "sys/sysinfo.h"

#include "source/fsa.h"
#include "source/rpq_forest.h"
#include "source/streaming_graph.h"
#include "source/query_handler.h"
#include "source/adwin/Adwin.h"

namespace fs = std::filesystem;
using namespace std;

// struct sysinfo memInfo;

typedef struct Config {
    std::string input_data_path;
    int adaptive{};
    long long size{};
    long long slide{};
    long long query_type{};
    std::vector<long long> labels;
    int max_size{};
    int min_size{};
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

    fs::path data_path = fs::current_path() / config.input_data_path;
    std::string data_folder = data_path.parent_path().filename().string();
    data_path = fs::absolute(data_path).lexically_normal();

    cout << "Dataset: " << data_folder << endl;

    std::ifstream fin(data_path);
    if (!fin.is_open()) {
        std::cerr << "Error: Failed to open " << data_path << std::endl;
        exit(1);
    }
    // if max size < min size, exit
    if (max_size < min_size) {
        cerr << "ERROR: max_size < min_size" << endl;
        exit(1);
    }

    auto *f = new Forest();
    auto *sg = new streaming_graph(2.5);
    auto *sink = new Sink();
    auto *aut = new FiniteStateAutomaton();

    int maxBuckets = 8;
    int minLen = size;
    double delta = 0.001;
    Adwin adwin(maxBuckets, minLen, delta);

    f->possible_states = setup_automaton(query_type, aut, config.labels);

    int first_transition = config.labels[0];

    auto query = new QueryHandler(*aut, *f, *sg, *sink); // Density-based Retention

    vector<window> windows;
    int total_elements_count = 0;

    long long t0 = 0;
    long long edge_number = 0;
    long long time;
    long long timestamp;

    long long window_offset = 0;

    bool evict = false;
    vector<size_t> to_evict;
    long long last_t_open = -1;

    // long long checkpoint = 9223372036854775807L;
    long long checkpoint = 100;

    vector<long long> node_count;
    long long saved_edges = 0;

    std::string mode;
    switch (config.adaptive) {
        case 0: mode = "sl";
            break;
        case 1: mode = "ad";
            break;
        case 2: mode = "adwin";
            break;
        case 3: mode = "lshed";
            break;
        default:
            cerr << "ERROR: Unknown mode" << endl;
            exit(4);
    }

    bool ADAPTIVE_WINDOW = config.adaptive == 1;

    // ADAPTIVE WINDOW
    static double cumulative_size = 0.0;
    static long long size_count = 0;
    double avg_size = 0;
    int EINIT_count = 0;
    double cost_max = 0.0;
    double cost_min = 922337203685470;
    double lat_max = 0.0;
    double lat_min = 922337203685470;
    double cost = 0;
    double cost_norm;
    double last_cost = 0.0;
    double last_diff = 0.0;
    double max_deg = 1;
    const int overlap = size / slide;
    std::deque<double> cost_window;
    std::deque<double> normalization_window;

    // ADWIN
    int warmup = 0;
    double cumulative_degree = 0.0;
    double avg_deg = 0.0;
    int resizings = 0;
    int window_cardinality = 0;

    // LOAD SHEDDING
    double p_shed = 0.0;
    std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double granularity = min_size / 100.0;
    cout << "Load shedding granularity: " << granularity << endl;
    double max_shed = max_size / 100.0;
    cout << "Max shedding step: " << max_shed << endl;

    std::ofstream csv_summary(
        data_folder + "_summary_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" +
        std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size) + ".csv");
    csv_summary << "total_edges,matches,exec_time,windows_created,avg_window_cardinality,avg_window_size\n";

    std::ofstream csv_windows(
        data_folder + "_window_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" +
        std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size) + ".csv");
    csv_windows << "index,t_open,t_close,window_results,incremental_matches,latency,window_cardinality,window_size\n";

    std::ofstream csv_tuples(
        data_folder + "_tuples_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" +
        std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size) + ".csv");
    csv_tuples <<
            "estimated_cost,normalized_estimated_cost,latency,normalized_latency,window_cardinality,window_size\n";

    std::ofstream csv_memory(
        data_folder + "_memory_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" +
        std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size) + ".csv");
    csv_memory << "tot_virtual,used_virtual,tot_ram,used_ram,data_mem\n";

    std::ofstream csv_adwin_distribution(
        data_folder + "_adwin_dist_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" +
        std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size) + ".csv");
    csv_adwin_distribution << "avg_deg,cost,cost_norm\n";

    clock_t start = clock();
    windows.emplace_back(0, size, nullptr, nullptr);
    long long s, d, l, t;
    while (fin >> s >> d >> l >> t) {
        if (t0 == 0) {
            t0 = t;
            timestamp = 1;
        } else timestamp = t - t0;

        if (timestamp < 0) continue;
        time = timestamp;

        // process the edge if the label is part of the query
        if (!aut->hasLabel(l))
            continue;

        timed_edge *t_edge;
        sg_edge *new_sgt;

        if (mode == "sl" || mode == "ad") {

            edge_number++;
            if (l == first_transition) EINIT_count++;

            long long window_close;
            double base = std::floor(static_cast<double>(time) / slide) * slide;
            double o_i = base;
            bool new_window = true;
            do {
                auto window_open = static_cast<long long>(o_i);
                window_close = static_cast<long long>(o_i + size);

                for (size_t i = window_offset; i < windows.size(); i++) {
                    if (windows[i].t_open == window_open && windows[i].t_close == window_close) {
                        // computed window is already present in WSS
                        new_window = false;
                    }
                }

                if (new_window) {
                    if (window_close < windows[windows.size() - 1].t_close) {    // shrink
                        for (size_t j = windows.size() - 1; j >= window_offset; j--) {
                            if (windows[j].t_close > window_close) {
                                windows[j].evicted = true;
                                windows[j].first = nullptr;
                                windows[j].last = nullptr;
                                windows[j].latency = -1;
                                windows[j].normalized_latency = -1;
                                windows.pop_back();
                            }
                        }
                    }
                    if (window_close > windows[windows.size() - 1].t_close && window_open > windows[windows.size() - 1].t_open) {  // expand
                        if (windows[windows.size() - 1].t_close < window_close) {
                            // report results
                            windows[windows.size() - 1].total_matched_results = sink->matched_paths;
                            // matched paths until this window
                            windows[windows.size() - 1].emitted_results = sink->getResultSetSize();
                            // paths emitted on this window close
                            windows[windows.size() - 1].window_matches = windows[windows.size() - 1].emitted_results;
                        }
                        windows.emplace_back(window_open, window_close, nullptr, nullptr);
                    }
                }

                o_i += slide;
            } while (o_i < time);

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
                if (windows[i].t_open <= time && time < windows[i].t_close) {
                    // active window
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
                } else if (time >= windows[i].t_close) {
                    // schedule window for eviction
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

            cumulative_degree += sg->density[new_sgt->s];
            window_cardinality++;
            avg_deg = cumulative_degree / window_cardinality;

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
                    cumulative_degree -= sg->density[cur_edge->s];
                    window_cardinality--;

                    candidate_for_deletion.emplace_back(cur_edge->s, cur_edge->d);
                    // schedule for deletion from RPQ forest
                    //if (to_evict_timestamp >= cur_edge->timestamp) {
                    sg->remove_edge(cur_edge->s, cur_edge->d, cur_edge->label); // delete from adjacency list
                    //}
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

                if (ADAPTIVE_WINDOW) {
                    // max degree computation
                    max_deg = 1;
                    for (size_t i = window_offset; i < windows.size(); i++) {
                        if (windows[i].max_degree > max_deg) max_deg = windows[i].max_degree;
                    }

                    double n = 0;
                    if (EINIT_count > edge_number) cerr << "ERROR: more initial transitions than edges." << endl;
                    for (int i = 0; i < EINIT_count; i++) {
                        n += sg->edge_num - i;
                    }
                    cost = n / max_deg;

                    if (cost > cost_max) cost_max = cost;
                    if (cost < cost_min) cost_min = cost;
                    cost_norm = (cost - cost_min) / (cost_max - cost_min);

                    cost_window.push_back(cost_norm);
                    if (cost_window.size() > overlap)
                        cost_window.pop_front();

                    cost_norm = std::accumulate(cost_window.begin(), cost_window.end(), 0.0) / cost_window.size();

                    double cost_diff = cost_norm - last_cost;
                    last_cost = cost_norm;

                    double cost_diff_diff = cost_diff - last_diff;
                    last_diff = cost_diff_diff;

                    if (std::isnan(last_diff)) {
                        last_diff = 0;
                    }
                    if (std::isnan(cost_diff)) {
                        cost_diff = 0;
                    }

                    if (cost_diff > 0 || cost_norm >= 0.9) {
                        cost_diff <= 0.1 ? size -= slide : size -= ceil(cost_diff * 10 * slide);
                    } else if (cost_diff < 0 || cost_norm <= 0.1) {
                        cost_diff <= 0.1 ? size += slide : size += ceil(cost_diff * 10 * slide);
                    }

                    // cap to max and min size
                    size = std::max(std::min(size, max_size), min_size);
                }

                /*
                * MEMORY PROFILING (https://stackoverflow.com/a/64166)
                */
                /*
                if (MEMORY_PROFILER) {
                    sysinfo (&memInfo);
                    long long totalVirtualMem = memInfo.totalram;
                    totalVirtualMem += memInfo.totalswap;
                    totalVirtualMem *= memInfo.mem_unit;

                    long long virtualMemUsed = memInfo.totalram - memInfo.freeram;
                    virtualMemUsed += memInfo.totalswap - memInfo.freeswap;
                    virtualMemUsed *= memInfo.mem_unit;

                    long long totalPhysMem = memInfo.totalram;
                    totalPhysMem *= memInfo.mem_unit;

                    long long physMemUsed = memInfo.totalram - memInfo.freeram;
                    physMemUsed *= memInfo.mem_unit;

                    csv_memory
                        << totalVirtualMem << ","
                        << virtualMemUsed << ","
                        << totalPhysMem << ","
                        << physMemUsed << ","
                        << sg->getUsedMemory() + f->getUsedMemory() << endl;
                }
                */
            }
            csv_tuples
                << cost << ","
                << cost_norm << ","
                << windows[window_offset >= 1 ? window_offset - 1 : 0].latency << ","
                << windows[window_offset >= 1 ? window_offset - 1 : 0].normalized_latency << ","
                << windows[window_offset >= 1 ? window_offset - 1 : 0].elements_count << ","
                << windows[window_offset >= 1 ? window_offset - 1 : 0].t_close - windows[window_offset >= 1 ? window_offset - 1 : 0].t_open << endl;



            csv_adwin_distribution << avg_deg << "," << cost << "," << cost_norm << "\n";
        }
        else if (mode == "adwin") {

            edge_number++;
            if (l == first_transition) EINIT_count++;

            window_cardinality++;
            windows[resizings].elements_count++;
            warmup++;

            new_sgt = sg->insert_edge(edge_number, s, d, l, time, time);

            if (!new_sgt) {
                cerr << "ERROR: new sgt is null, time: " << time << endl;
                exit(1);
            }

            // do W ← W ∪ {xt} (i.e., add xt to the head of W )
            t_edge = new timed_edge(new_sgt);
            sg->add_timed_edge(t_edge);
            new_sgt->time_pos = t_edge;

            // update max degree observed
            if (sg->density[new_sgt->s] > max_deg) max_deg = sg->density[new_sgt->s];

            // compute average out degree centrality incrementally
            cumulative_degree += sg->density[new_sgt->s];
            avg_deg = cumulative_degree / window_cardinality;

            // compute cost function
            double n = 0;
            for (int i = 0; i < EINIT_count; i++) {
                n += sg->edge_num - i;
            }
            cost = n / max_deg;
            if (cost > cost_max) cost_max = cost;
            if (cost < cost_min) cost_min = cost;
            cost_norm = (cost - cost_min) / (cost_max - cost_min);

            if (warmup > 2700) csv_adwin_distribution << avg_deg << "," << cost << "," << cost_norm << "\n";

            // if (adwin.update(cost)) {
            if (adwin.update(cost_norm*10) && warmup > 2700) {
                cout << "\n>>> DRIFT DETECTED " << endl;
                cout << "    Current estimation: " << adwin.getEstimation() << endl;
                cout << "    Window length: " << adwin.length() << endl;
                windows[resizings].t_close = time;
                windows[resizings].latency = static_cast<double>(clock() - windows[resizings].start_time) / CLOCKS_PER_SEC;
                windows[resizings].total_matched_results = sink->matched_paths;
                windows[resizings].emitted_results = sink->getResultSetSize();
                resizings++;
                windows.emplace_back(time, time, nullptr, nullptr);
                timed_edge *current = sg->time_list_head;
                std::vector<pair<long long, long long> > candidate_for_deletion;

                while (current && window_cardinality > adwin.length()) {
                    auto cur_edge = current->edge_pt;
                    auto next = current->next;

                    if (cur_edge->label == first_transition) EINIT_count--;
                    cumulative_degree -= sg->density[cur_edge->s];

                    candidate_for_deletion.emplace_back(cur_edge->s, cur_edge->d);

                    sg->remove_edge(cur_edge->s, cur_edge->d, cur_edge->label);

                    sg->delete_timed_edge(current);

                    window_cardinality--;

                    sg->time_list_head = next;
                    sg->time_list_head->prev = nullptr;

                    current = next;
                }

                max_deg = 1;
                while (current) {
                    auto cur_edge = current->edge_pt;
                    auto next = current->next;
                    if (sg->density[cur_edge->s] > max_deg) max_deg = sg->density[cur_edge->s];
                    current = next;
                }

                sink->refresh_resultSet(sg->time_list_head->edge_pt->timestamp);
                f->expire_timestamped(sg->time_list_head->edge_pt->timestamp, candidate_for_deletion);
                candidate_for_deletion.clear();
            }
            csv_tuples
                << cost << ","
                << cost_norm << ","
                << windows[resizings].latency << ","
                << 0 << ","
                << windows[resizings].elements_count << ","
                << windows[resizings].t_close - windows[resizings].t_open << endl;
        }
        else if (mode == "lshed") {

            if (dist(gen) < p_shed) { // load shedding
                continue;
            }

            edge_number++;
            if (l == first_transition) EINIT_count++;

            long long window_close;
            double base = std::floor(static_cast<double>(time) / slide) * slide;
            double o_i = base;
            bool new_window = true;
            do {
                auto window_open = static_cast<long long>(o_i);
                window_close = static_cast<long long>(o_i + size);

                for (size_t i = window_offset; i < windows.size(); i++) {
                    if (windows[i].t_open == window_open && windows[i].t_close == window_close) {
                        // computed window is already present in WSS
                        new_window = false;
                    }
                }

                if (new_window) { // trigger report and append new window
                        if (windows[windows.size() - 1].t_close < window_close) {
                            // report results
                            windows[windows.size() - 1].total_matched_results = sink->matched_paths;
                            // matched paths until this window
                            windows[windows.size() - 1].emitted_results = sink->getResultSetSize();
                            // paths emitted on this window close
                            windows[windows.size() - 1].window_matches = windows[windows.size() - 1].emitted_results;
                        }
                        windows.emplace_back(window_open, window_close, nullptr, nullptr);
                }

                o_i += slide;
            } while (o_i < time);

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
                if (windows[i].t_open <= time && time < windows[i].t_close) {
                    // active window
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
                } else if (time >= windows[i].t_close) {
                    // schedule window for eviction
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

            cumulative_degree += sg->density[new_sgt->s];
            window_cardinality++;
            avg_deg = cumulative_degree / window_cardinality;

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
                    cumulative_degree -= sg->density[cur_edge->s];
                    window_cardinality--;

                    candidate_for_deletion.emplace_back(cur_edge->s, cur_edge->d); // schedule for deletion from RPQ forest
                    sg->remove_edge(cur_edge->s, cur_edge->d, cur_edge->label); // delete from adjacency list
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
                }

                to_evict.clear();
                candidate_for_deletion.clear();
                evict = false;

                // max degree computation
                max_deg = 1;
                for (size_t i = window_offset; i < windows.size(); i++) {
                    if (windows[i].max_degree > max_deg) max_deg = windows[i].max_degree;
                }

                double n = 0;
                if (EINIT_count > edge_number) cerr << "ERROR: more initial transitions than edges." << endl;
                for (int i = 0; i < EINIT_count; i++) {
                    n += sg->edge_num - i;
                }
                cost = n / max_deg;

                if (cost > cost_max) cost_max = cost;
                if (cost < cost_min) cost_min = cost;
                cost_norm = (cost - cost_min) / (cost_max - cost_min);

                cost_window.push_back(cost_norm);
                if (cost_window.size() > overlap)
                    cost_window.pop_front();

                cost_norm = std::accumulate(cost_window.begin(), cost_window.end(), 0.0) / cost_window.size();

                double cost_diff = cost_norm - last_cost;
                last_cost = cost_norm;

                double cost_diff_diff = cost_diff - last_diff;
                last_diff = cost_diff_diff;

                if (std::isnan(last_diff)) {
                    last_diff = 0;
                }
                if (std::isnan(cost_diff)) {
                    cost_diff = 0;
                }

                if (cost_diff > 0 || cost_norm >= 0.9) {
                    cost_diff <= 0.1 ? p_shed += granularity : p_shed += ceil(cost_diff * 10 * granularity);
                } else if (cost_diff < 0 || cost_norm <= 0.1) {
                    cost_diff <= 0.1 ? p_shed -= granularity : p_shed -= ceil(cost_diff * 10 * granularity);
                }

                p_shed = std::max(std::min(p_shed, max_shed), 0.0);

                cout << ">>> Updated load shedding probability: " << p_shed << endl;
            }

            csv_tuples
                << cost << ","
                << cost_norm << ","
                << windows[window_offset >= 1 ? window_offset - 1 : 0].latency << ","
                << windows[window_offset >= 1 ? window_offset - 1 : 0].normalized_latency << ","
                << windows[window_offset >= 1 ? window_offset - 1 : 0].elements_count << ","
                << windows[window_offset >= 1 ? window_offset - 1 : 0].t_close - windows[window_offset >= 1 ? window_offset - 1 : 0].t_open << endl;

        } else {
            // error
            cerr << "unknown mode" << endl;
            exit(4);
        }

        /* QUERY */
        query->pattern_matching_tc(new_sgt);

        if (edge_number % checkpoint == 0) {
            printf("processed edges: %lld\n", edge_number);
            printf("avg degree: %f\n", sg->mean);
            cout << "matched paths: " << sink->matched_paths << "\n\n";
        }
    }

    cout << "Created windows: " << windows.size() << endl;

    clock_t finish = clock();
    long long time_used = (double) (finish - start) / CLOCKS_PER_SEC;

    double avg_window_size = static_cast<double>(total_elements_count) / windows.size();

    csv_summary
            << edge_number << ","
            << sink->matched_paths << ","
            << time_used << ","
            << windows.size() << ","
            << avg_window_size << ","
            << avg_size << "\n";

    // index,t_open,t_close,window_results,incremental_matches,latency,window_cardinality,window_size
    for (size_t i = 0; i < windows.size(); ++i) {
        csv_windows
                << i << ","
                << windows[i].t_open << ","
                << windows[i].t_close << ","
                << windows[i].emitted_results << ","
                << windows[i].total_matched_results << ","
                << windows[i].latency << ","
                << windows[i].elements_count << ","
                << windows[i].t_close - windows[i].t_open << "\n";
    }

    csv_summary.close();
    csv_windows.close();
    csv_tuples.close();
    csv_memory.close();
    csv_adwin_distribution.close();

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