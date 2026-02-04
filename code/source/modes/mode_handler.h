#ifndef MODE_HANDLER_H
#define MODE_HANDLER_H

#include <vector>
#include <deque>
#include <iostream>
#include <fstream>
#include <ctime>
#include <unordered_set>
#include <cstdint>
#include "../streaming_graph.h"
#include "../rpq_forest.h"
#include "../sink.h"
#include "../fsa.h"

// Window class definition (originally from main.cpp)
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

// Context structure to hold shared state for mode handlers
struct ModeContext {
    int mode;

    // Windows and graph structures
    std::vector<window>* windows;
    streaming_graph* sg;
    Forest* f;
    Sink* sink;
    FiniteStateAutomaton* aut;
    
    // CSV output streams
    std::ofstream* csv_tuples;
    std::ofstream* csv_adwin_distribution;
    std::ofstream* csv_memory;
    
    // Configuration values
    long long size;
    long long slide;
    long long max_size;
    long long min_size;
    int first_transition;
    
    // Counters and state variables
    long long* edge_number;
    long long* window_offset;
    std::vector<size_t>* to_evict;
    bool* evict;
    long long* last_t_open;
    
    // Adaptive window specific
    bool ADAPTIVE_WINDOW;
    double* cumulative_size;
    long long* size_count;
    double* avg_size;
    double* cost_max;
    double* cost_min;
    double* lat_max;
    double* lat_min;
    double* cost;
    double* cost_norm;
    double* last_cost;
    double* last_diff;
    double* max_deg;
    int overlap;
    std::deque<double>* cost_window;
    std::deque<double>* normalization_window;
    
    // ADWIN specific
    int* warmup;
    double* cumulative_degree;
    double* avg_deg;
    int* resizings;
    int* window_cardinality;
    
    // Load shedding specific
    double* p_shed;
    double granularity;
    double max_shed;
    
    // Other
    int* total_elements_count;

    double* cumulative_window_latency;
    unsigned long beta_latency_start = 0;
    unsigned long beta_latency_end = 0;
    int beta_id = 0;
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
