#ifndef MODE_HANDLER_H
#define MODE_HANDLER_H

#include <vector>
#include <deque>
#include <fstream>
#include <ctime>
#include <sstream>

#include "../streaming_graph.h"
#include "../sink.h"
#include "../fsa.h"
#include "code/source/query_handler.h"

typedef struct Config {
    std::string input_data_path;
    int adaptive{};
    long long size{};
    long long slide{};
    long long query_type{};
    std::vector<long long> labels;
    int max_size{};
    int min_size{};
    double l_max{};
    int path_algorithm{};
} config;

inline config readConfig(const std::string &filename) {
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
    config.path_algorithm = std::stoi(configMap["path_algorithm"]);

    std::istringstream extraArgsStream(configMap["labels"]);
    std::string arg;
    while (std::getline(extraArgsStream, arg, ',')) {
        config.labels.push_back(std::stoi(arg));
    }

    if (config.adaptive == 5) {
        if (configMap.find("l_max") == configMap.end()) {
            std::cerr << "Error: l_max should be set" << std::endl;
            exit(1);
        }
        config.l_max = std::stod(configMap["l_max"]);
    } else {
        config.l_max = -1;
    }

    return config;
}

// Window class definition
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

    double cost = 0.0;

    double max_degree = 0.0;

    long long elements_count = 0;
    int window_matches = 0;
    int results_at_open = 0;   // matched_paths when window opened
    int results_at_close = 0;  // matched_paths when window was evicted

    int total_matched_results = 0;
    int emitted_results = 0;

    // Constructor
    window(long long t_open, long long t_close, timed_edge *first, timed_edge *last, int results_at_open) {
        this->t_open = t_open;
        this->t_close = t_close;
        this->first = first;
        this->last = last;
        this->start_time = clock();
        this->results_at_open = results_at_open;
    }
};

struct Slide {
    long long t_open;           // k * slide_size
    long long t_close;          // (k+1) * slide_size
    clock_t   wall_open;        // wall clock when this slide was created
    clock_t   wall_close;       // wall clock when the next slide was created
    long long elements_count = 0; // edges that arrived in [t_open, t_close)
    int       results_at_open = 0;  // snapshot of matched_paths at slide open
    int       results_at_close = 0; // snapshot of matched_paths at slide close
    double    cost_norm = 0.0;  // cost_norm computed at slide boundary
};

// Context structure to hold shared state for mode handlers
struct ModeContext {
    int mode;

    // Windows and graph structures
    std::vector<window> windows;
    streaming_graph* sg;
    Sink* sink;
    FiniteStateAutomaton* aut;
    QueryHandler* q;
    
    // CSV output streams
    std::ofstream* csv_tuples;
    std::ofstream* csv_memory;
    
    // Configuration values
    long long size;
    long long slide;
    long long max_size;
    long long min_size;
    int first_transition;
    
    // Counters and state variables
    long long edge_number = 0;
    long long window_offset = 0;
    std::vector<size_t> to_evict;

    double cumulative_size = 0.0;
    long long size_count = 0;
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
    std::deque<double> cost_window;

    int warmup = 0;
    int resizings = 0;
    int window_cardinality = 0;
    
    // Load shedding specific
    double p_shed = 0.0;
    double granularity;
    double max_shed;

    double average_processing_time = 0.0;
    double latency_max;

    // Other
    int total_elements_count = 0;

    double cumulative_window_latency = 0;
    double beta_latency_start = 0;
    double beta_latency_end = 0;
    int beta_id = 0;
    int beta_elements_cont = 0;
    double last_oi = 0.0;

    std::vector<Slide> slides;
    long long current_slide_open = -1; // t_open of the active slide

    vector<double> cumulative_processing_time_type;
    vector<double> processed_elements_type;
    vector<double> input_rate_type;

};

// Abstract base class for mode handlers
class ModeHandler {
public:
    virtual ~ModeHandler() = default;
    
    // Process a single edge according to the mode's logic
    // Returns true if processing should continue, false otherwise
    // new_sgt_out will be set to the created sg_edge pointer
    virtual bool process_edge(
        long long s,      // source node
        long long d,      // destination node
        long long l,      // edge label
        long long time,   // timestamp
        ModeContext& ctx, // shared context
        sg_edge** new_sgt_out  // output parameter for the created edge
    ) = 0;
};

#endif // MODE_HANDLER_H
