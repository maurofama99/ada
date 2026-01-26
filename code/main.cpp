#include <vector>
#include <string>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <random>

#define MEMORY_PROFILER false

#include "sys/types.h"
// #include "sys/sysinfo.h"

#include "source/fsa.h"
#include "source/rpq_forest.h"
#include "source/streaming_graph.h"
#include "source/query_handler.h"
#include "source/adwin/Adwin.h"
#include "source/modes/mode_handler.h"
#include "source/modes/mode_factory.h"

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
    auto *sink = new Sink();
    auto *aut = new FiniteStateAutomaton();

    int maxBuckets = 5;
    int minLen = 1;
    double delta = 1.0 / static_cast<double>(min_size);
    if (config.adaptive == 15) delta = 0.15;
    Adwin adwin(maxBuckets, minLen, delta);

    if (config.adaptive == 2 || config.adaptive == 15) {
        // print adwin configuration
        cout << "ADWIN configuration: " << endl;
        cout << "  - maxBuckets: " << maxBuckets << endl;
        cout << "  - minLen: " << minLen << endl;
        cout << "  - delta: " << delta << endl;
    }

    f->possible_states = aut->setup_automaton(query_type, config.labels);

    auto *sg = new streaming_graph(config.labels[0]);

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
    long long checkpoint = 500000;

    vector<long long> node_count;

    std::string mode;
    switch (config.adaptive) {
        case 10: mode = "sl";
            break;
        case 11: mode = "ad_function";
            break;
        case 12: mode = "ad_degree";
            break;
        case 13: mode = "ad_einit";
            break;
        case 14: mode = "ad_latency";
            break;
        case 15: mode = "ad_adwin";
            break;
        case 2: mode = "adwin";
            break;
        case 3: mode = "lshed";
            break;
        default:
            cerr << "ERROR: Unknown mode" << endl;
            exit(4);
    }

    cout << "Modalità: " << mode << " (config. " << config.adaptive << ")" << endl;

    // ADAPTIVE WINDOW
    static double cumulative_size = 0.0;
    static long long size_count = 0;
    double avg_size = 0;
    double cost_max = 0.0;
    double cost_min = 922337203685470;
    double lat_max = 0.0;
    double lat_min = 922337203685470;
    double cost = 0;
    double cost_norm;
    double last_cost = 0.0;
    double last_diff = 0.0;
    double max_deg = 1;
    int overlap = -1;
    if (size >0 && slide >0) overlap = size / slide;
    std::deque<double> cost_window;
    std::deque<double> normalization_window;
    double cumulative_window_latency = 0.0;

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
    double max_shed = max_size / 100.0;
    if (config.adaptive == 3) {
        cout << "Load shedding activated." << endl;
        cout << "Load shedding granularity: " << granularity << endl;
        cout << "Max shedding step: " << max_shed << endl;
    }

    // output folder for csvs
    fs::path output_folder = "results_adwin";
    if (!fs::exists(output_folder)) {
        fs::create_directories(output_folder);
    }

    // Base filename (without folder)
    const std::string base =
        data_folder + "_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" +
        std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size);

    // Build full paths under output_folder
    const fs::path summary_path = output_folder / (base + "_summary_results.csv");
    const fs::path windows_path = output_folder / (base + "_window_results.csv");
    const fs::path tuples_path  = output_folder / (base + "_tuples_results.csv");
    const fs::path memory_path  = output_folder / (base + "_memory_results.csv");

    std::ofstream csv_summary(summary_path.string());
    csv_summary << "total_edges,matches,exec_time,windows_created,avg_window_cardinality,avg_window_size\n";

    std::ofstream csv_windows(windows_path.string());
    csv_windows << "index,t_open,t_close,window_results,incremental_matches,latency,window_cardinality,window_size\n";

    std::ofstream csv_tuples(tuples_path.string());
    csv_tuples << "estimated_cost,normalized_estimated_cost,latency,normalized_latency,window_cardinality,window_size\n";

    std::ofstream csv_memory(memory_path.string());
    csv_memory << "tot_virtual,used_virtual,tot_ram,used_ram,data_mem\n";

    // Create mode handler using factory
    auto mode_handler = ModeFactory::create_mode_handler(config.adaptive, &adwin, &gen, &dist);
    
    // Setup context for mode handlers
    ModeContext ctx;
    ctx.windows = &windows;
    ctx.sg = sg;
    ctx.f = f;
    ctx.sink = sink;
    ctx.aut = aut;
    ctx.csv_tuples = &csv_tuples;
    ctx.size = size;
    ctx.slide = slide;
    ctx.max_size = max_size;
    ctx.min_size = min_size;
    ctx.edge_number = &edge_number;
    ctx.window_offset = &window_offset;
    ctx.to_evict = &to_evict;
    ctx.evict = &evict;
    ctx.last_t_open = &last_t_open;
    ctx.mode = config.adaptive;
    ctx.cumulative_size = &cumulative_size;
    ctx.size_count = &size_count;
    ctx.avg_size = &avg_size;
    ctx.cost_max = &cost_max;
    ctx.cost_min = &cost_min;
    ctx.lat_max = &lat_max;
    ctx.lat_min = &lat_min;
    ctx.cost = &cost;
    ctx.cost_norm = &cost_norm;
    ctx.last_cost = &last_cost;
    ctx.last_diff = &last_diff;
    ctx.max_deg = &max_deg;
    ctx.overlap = overlap;
    ctx.cost_window = &cost_window;
    ctx.normalization_window = &normalization_window;
    ctx.warmup = &warmup;
    ctx.cumulative_degree = &cumulative_degree;
    ctx.avg_deg = &avg_deg;
    ctx.resizings = &resizings;
    ctx.window_cardinality = &window_cardinality;
    ctx.p_shed = &p_shed;
    ctx.granularity = granularity;
    ctx.max_shed = max_shed;
    ctx.total_elements_count = &total_elements_count;
    ctx.cumulative_window_latency = &cumulative_window_latency;

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

        sg_edge *new_sgt = nullptr;
        
        // Process edge using the appropriate mode handler
        mode_handler->process_edge(s, d, l, time, ctx, &new_sgt);

        /* QUERY */
        if (new_sgt) {  // Only process query if an edge was created (not shed in load shedding mode)
            query->pattern_matching_tc(new_sgt);
        }

        if (edge_number % checkpoint == 0) {
            printf("processed edges: %lld\n", edge_number);
            printf("avg degree: %f\n", *ctx.avg_deg);
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

    sink->exportResultSet(base + "_result_set.csv");

    csv_summary.close();
    csv_windows.close();
    csv_tuples.close();
    csv_memory.close();

    // cleanup
    delete sg;
    delete f;
    delete sink;
    delete aut;
    delete query;

    return 0;
}