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
    bool adaptive{};
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
    std::string data_folder = data_path.parent_path().filename().string();
    data_path = fs::absolute(data_path).lexically_normal();

    cout << "Dataset: " << data_folder << endl;

    std::ifstream fin(data_path);
    if (!fin.is_open()) {
        std::cerr << "Error: Failed to open " << data_path << std::endl;
        return 1;
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

    f->possible_states = setup_automaton(query_type, aut, config.labels);

    int first_transition = config.labels[0];

    auto query = new QueryHandler(*aut, *f, *sg, *sink); // Density-based Retention

    int total_elements_count = 0;

    long long t0 = 0;
    long long edge_number = 0;
    long long time;
    long long timestamp;

    // long long checkpoint = 9223372036854775807L;
    long long checkpoint = 1000000;

    vector<long long> node_count;

    std::string mode = config.adaptive ? "ad" : "sl";
    bool ADAPTIVE_WINDOW = config.adaptive > 0;
    double avg_size = 0;
    int EINIT_count = 0;
    double cost = 0;
    int window_cardinality = 0;
    double max_deg = 1;

    std::ofstream csv_summary(data_folder + "_summary_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" + std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size) + ".csv");
    csv_summary << "total_edges,matches,exec_time,windows_created,avg_window_cardinality,avg_window_size\n";

    std::ofstream csv_windows(data_folder + "_window_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" + std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size) + ".csv");
    csv_windows << "index,t_open,t_close,window_results,incremental_matches,latency,window_cardinality,window_size\n";

    std::ofstream csv_tuples(data_folder + "_tuples_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" + std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size) + ".csv");
    csv_tuples << "estimated_cost,normalized_estimated_cost,latency,normalized_latency,window_cardinality,window_size\n";

    std::ofstream csv_memory(data_folder + "_memory_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" + std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size) + ".csv");
    csv_memory << "tot_virtual,used_virtual,tot_ram,used_ram,data_mem\n";

    int maxBuckets = 5;
    Adwin adwin(maxBuckets);

    clock_t start = clock();
    long long s, d, l, t;
    while (fin >> s >> d >> l >> t) {
        if (t0 == 0) {
            t0 = t;
            timestamp = 1;
        } else timestamp = t - t0;

        if (timestamp < 0) continue;
        time = timestamp;

        if (!aut->hasLabel(l))
            continue;

        edge_number++;
        if (l == first_transition) EINIT_count++;
        window_cardinality++;

        timed_edge *t_edge;
        sg_edge *new_sgt;

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

        // compute cost function
        double n = 0;
        for (int i = 0; i < EINIT_count; i++) {
            n += sg->edge_num - i;
        }
        cost = n / max_deg;

        if (adwin.update(cost)) { // drift detected
            timed_edge *current = sg->time_list_head;
            std::vector<pair<long long, long long> > candidate_for_deletion;

            while (current && window_cardinality > adwin.length()) {

                auto cur_edge = current->edge_pt;
                auto next = current->next;

                if (cur_edge->label == first_transition) EINIT_count--;

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

            f->expire_timestamped(sg->time_list_head->edge_pt->timestamp, candidate_for_deletion);
            candidate_for_deletion.clear();
        }

        /* QUERY */
        query->pattern_matching_tc(new_sgt);

        if (edge_number % checkpoint == 0) {
            printf("processed edges: %lld\n", edge_number);
            printf("avg degree: %f\n", sg->mean);
            cout << "matched paths: " << sink->matched_paths << "\n\n";
        }

        // sg->printGraph();

        // f->printForest();

        // cout << "============================" << endl;

        // estimated_cost,normalized_estimated_cost,latency,normalized_latency,window_cardinality,window_size
        csv_tuples
            << cost << endl;
    }

    clock_t finish = clock();
    long long time_used = (double) (finish - start) / CLOCKS_PER_SEC;

    double avg_window_size =

    csv_summary
        <<  edge_number << ","
        <<  sink->matched_paths << ","
        <<  time_used << ","
        <<  windows.size() << ","
        <<  avg_window_size << ","
        <<  avg_size << "\n";

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