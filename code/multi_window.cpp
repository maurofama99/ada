#include<vector>
#include<string>
#include<ctime>
#include<cstdlib>
#include <fstream>
#include <cmath>
#include <future>
#include <numeric>
#include <sstream>
#include <future>
#include <vector>

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
    long long reachability_threshold;
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
    config.reachability_threshold = std::stoi(configMap["reachability_threshold"]);

    return config;
}

struct window {
    long long t_open;
    long long t_close;
    bool evicted = false;

    window(const long long t_open, const long long t_close) {
        this->t_open = t_open;
        this->t_close = t_close;
    }

    bool operator==(const window &rhs) const {
        return this->t_open == rhs.t_open && this->t_close == rhs.t_close;
    }
};

struct joining_interval {
    long long open;
    long long close;

    bool operator==(const joining_interval &other) const {
        return open == other.open && close == other.close;
    }
};

// Hash function for joining interval
struct interval_hash {
    std::size_t operator()(const joining_interval &interval) const {
        return std::hash<long long>()(interval.open); // ^ (std::hash<long long>()(interval.close) << 1);
    }
};

class RPQForestMap {
    std::unordered_map<joining_interval, Forest, interval_hash> map;
public:
    void addEntry(long long open, long long close) {
        joining_interval key = {open, close};
        map[key] = Forest();
    }

    Forest* getForest(long long open, long long close) {
        joining_interval key = {open, close};
        auto it = map.find(key);
        if (it != map.end()) {
            return &(it->second);
        }
        return nullptr;
    }
};

class SnapshotGraphMap {
    std::unordered_map<joining_interval, streaming_graph, interval_hash> map;
public:
    void addEntry(long long open, long long close) {
        joining_interval key = {open, close};
        map[key] = streaming_graph();
    }

    streaming_graph* getSG(long long open, long long close) {
        joining_interval key = {open, close};
        auto it = map.find(key);
        if (it != map.end()) {
            return &(it->second);
        }
        return nullptr;
    }
};


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
    long long reachability_threshold = config.reachability_threshold;
    bool BACKWARD_RETENTION = zscore != 0;
    bool REACHABLE_EXTENSION = reachability_threshold != 0;

    long long current_time = 0;

    if (size % slide != 0) {
        printf("ERROR: Size is not a multiple of slide\n");
        exit(1);
    }

    if (fin.fail()) {
        cerr << "Error opening file" << endl;
        exit(1);
    }

    auto *aut = new FiniteStateAutomaton();
    vector<long long> scores = setup_automaton(query_type, aut, config.labels);

    auto *sink = new Sink();

    RPQForestMap forest_map;
    SnapshotGraphMap sg_map;

    auto current_window = window(0, size);

    long long t0 = 0;
    long long edge_number = 0;
    long long time;
    long long timestamp;

    bool evict = false;
    vector<window> to_evict;

    long long checkpoint = 1000000;

    vector<long long> node_count;


    std::vector<std::tuple<long long, long long, long long, long long>> edges;
    std::string line;

    cout << "MULTI WINDOW" << endl;
    cout << "reading input dataset..." << endl;
    while (std::getline(fin, line)) {
        std::istringstream iss(line);
        long long s, d, l, t;
        if (iss >> s >> d >> l >> t) {
            edges.emplace_back(s, d, l, t);
        }
    }
    cout << "read complete" << endl;

    clock_t start = clock();

    for (const auto& [s, d, l, t] : edges) {
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

        // idea: use only the joined intervals for the data structure logic. track the current sliding window to implement tuning logic (eviction)

        long long interval_close = INT64_MAX;
        long long interval_open = 0;

        vector<window> windows;
        if (time >= current_time) {
            if (time == current_time) time++;
            current_time = time;

            double c_sup = ceil(static_cast<double>(time) / static_cast<double>(slide)) * slide;
            double o_i = c_sup - size;

            long long window_close;
            //cout << "time: " << time << endl;
            do {
                window_close = o_i + size;
                // track current sliding window
                if (current_window.t_close < window_close) {
                    current_window.t_open = o_i;
                    current_window.t_close = window_close;
                }
                windows.emplace_back(o_i, window_close);
                // cout << "window: " << o_i << " - " << window_close << endl;
                o_i += slide;
            } while (o_i < time);
        } else continue; // out-of-order element

        vector<pair<long long, long long>> intervals;

        intervals.insert(intervals.begin(), make_pair(windows[size/slide-1].t_open, windows[0].t_close));

        for (int i = 1; i < size/slide; i++) {
            intervals.insert(intervals.begin() + i, make_pair(windows[size/slide-1].t_open + i * slide, windows[0].t_close + i * slide));
        }

        auto edge = new sg_edge(edge_number, s, d, l, time);

        //clock_t for_start = clock();

        /* SINGLE THREAD */


        for (auto [open, close] : intervals) {
            interval_open = open;
            interval_close = close;

            // add entry to the maps if it doesn't exist
            if (auto sg = sg_map.getSG(interval_open, interval_close); !sg) {
                sg_map.addEntry(interval_open, interval_close);
            }
            if (auto forest = forest_map.getForest(interval_open, interval_close); !forest) {
                forest_map.addEntry(interval_open, interval_close);
            }
            auto sg = sg_map.getSG(interval_open, interval_close);
            sg->insert_edge(edge_number, s, d, l, time, current_window.t_close);

            auto forest = forest_map.getForest(interval_open, interval_close);
            auto query = QueryHandler(*aut, *forest, *sg, *sink);
            query.pattern_matching_tc(edge);
        }
        intervals.clear();
        windows.clear();

        //long long for_time_used = (double) (clock() - for_start); // / CLOCKS_PER_SEC;
        // printf("%lld\n", for_time_used);

        if (time >= current_window.t_close) {
            // schedule window for eviction
            // to_evict.push_back(current_window);
            // mark the window as evicted
            current_window.evicted = true;
            // evict = true;
            // cout << "evicting window: " << current_window.t_open << " - " << current_window.t_close << endl;
            // auto future_forest = std::async(std::launch::async, &Forest::clean_up, &windowed_forest[current_window.t_open]);
            // auto future_sg = std::async(std::launch::async, [&windowed_sg, current_window]() {
            //     windowed_sg.erase(current_window.t_open);
            // });
        }

        /* EVICT */
        if (evict) {
            /*
            if ((BACKWARD_RETENTION && !REACHABLE_EXTENSION && sg->get_zscore(cur_edge->s) > zscore)
            || (BACKWARD_RETENTION && REACHABLE_EXTENSION && sg->get_zscore(cur_edge->s) > zscore && sg->dfs_with_threshold(cur_edge->s, reachability_threshold, evict_end_point->edge_pt->timestamp))) {
                sg->saved_edges++;
                sg->shift_timed_edge(cur_edge->time_pos, windows[target_window_index].first);
            }
            */
            // remove the evicted windows from the forest and snapshot graph
            evict = false;
        }


        if (edge_number >= checkpoint) {
            checkpoint += checkpoint;

            printf("processed edges: %lld\n", edge_number);
            /*
            printf("saved edges: %lld\n", sg->saved_edges);
            printf("avg degree: %f\n", sg->mean);
            */
            cout << "resulting paths: " <<  sink->getResultSetSize() << "\n\n";
            //cout << "nodes in forest: " << node_count.back() << "\n";
        }
    }

    clock_t finish = clock();
    long long time_used = (double) (finish - start) / CLOCKS_PER_SEC;
    cout << "execution time: " << time_used << endl;
    printf("processed edges: %lld\n", edge_number);
    /*
    printf("saved edges: %lld\n", sg->saved_edges);
    printf("avg degree: %f\n", sg->mean);
    */
    cout << "resulting paths: " <<  sink->getResultSetSize() << "\n\n";

    // Construct output file path
    std::string retention = BACKWARD_RETENTION ? std::to_string(static_cast<int>(zscore)) : "0";
    std::string reachability = REACHABLE_EXTENSION ? std::to_string(static_cast<int>(reachability_threshold)) : "0";
    std::string output_file = config.output_base_folder + "output_a" + std::to_string(algorithm) + "_S" +
                                std::to_string(size) + "_s" + std::to_string(slide) + "_q" + std::to_string(query_type) +
                                    "_z" + retention + "_r" + reachability + ".txt";
    std::string output_file_csv = config.output_base_folder + "output_a" + std::to_string(algorithm) + "_S" +
                                    std::to_string(size) + "_s" + std::to_string(slide) + "_q" + std::to_string(query_type) +
                                        "_z" + retention + "_r" + reachability + ".csv";

    // query->exportResultSet(output_file_csv);

    // Open file for writing
    std::ofstream outFile(output_file.c_str());
    if(!outFile) {
        std::cerr << "Error opening file for writing: " << output_file << std::endl;

        cout << "resulting paths: " <<  sink->getResultSetSize() << "\n\n";
        cout << "processed edges: " << edge_number << "\n";
        // cout << "saved edges: " << sg->saved_edges << "\n";
        cout << "execution time: " << time_used << "\n";
        return EXIT_FAILURE;
    }

    // Write data to the file

    outFile << "resulting paths: " <<  sink->getResultSetSize() << "\n";

    outFile << "processed edges: " << edge_number << "\n";
    // outFile << "saved edges: " << sg->saved_edges << "\n";
    outFile << "execution time: " << time_used << "\n";
    outFile << "average nodes in forest: " << std::accumulate(node_count.begin(), node_count.end(), 0.0) / node_count.size() << "\n";
    // Close the file
    outFile.close();

    std::cout<<"Results written to: "<<output_file<<std::endl;

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
        case 7: // (a|b)c*
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
