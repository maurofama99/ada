#include "crow.h"
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
#include "SignalHandler.h"

#define MEMORY_PROFILER false

#include "sys/types.h"
// #include "sys/sysinfo.h"

#include "source/fsa.h"
#include "source/rpq_forest.h"
#include "source/streaming_graph.h"
#include "source/query_handler.h"

namespace fs = std::filesystem;
using namespace std;

// struct sysinfo memInfo;

typedef struct Config
{
    std::string input_data_path;
    bool adaptive{};
    long long size{};
    long long slide{};
    long long query_type{};
    std::vector<long long> labels;
    int max_size{};
    int min_size{};
} config;

config readConfig(const std::string &filename)
{
    config config;
    std::ifstream file(filename);
    std::string line;

    if (!file)
    {
        std::cerr << "Error opening configuration file: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    std::unordered_map<std::string, std::string> configMap;

    while (std::getline(file, line))
    {
        std::istringstream ss(line);
        std::string key, value;
        if (std::getline(ss, key, '=') && std::getline(ss, value))
        {
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
    while (std::getline(extraArgsStream, arg, ','))
    {
        config.labels.push_back(std::stoi(arg));
    }

    return config;
}

class window
{
public:
    long long t_open;
    long long t_close;
    timed_edge *first;
    timed_edge *last;
    bool evicted = false;
    clock_t start_time;              // start time of the window
    double latency = 0.0;            // latency of the window
    double normalized_latency = 0.0; // normalized latency of the window

    double max_degree = 0.0;

    long long elements_count = 0;
    int window_matches = 0;

    int total_matched_results = 0;
    int emitted_results = 0;

    // Constructor
    window(long long t_open, long long t_close, timed_edge *first, timed_edge *last)
    {
        this->t_open = t_open;
        this->t_close = t_close;
        this->first = first;
        this->last = last;
        this->start_time = clock();
    }
};

double normalize_shift(double shift, double min_shift, double max_shift, double alpha)
{
    if (max_shift == min_shift)
        return 0.0;                                                    // avoid division by zero
    double normalized = (shift - min_shift) / (max_shift - min_shift); // [0, 1]
    return normalized * alpha;                                         // [0, alpha]
}

int setup_automaton(long long query_type, FiniteStateAutomaton *aut, const vector<long long> &labels);

int main(int argc, char *argv[])
{
    SignalHandler signalHandler(18080);
    signalHandler.start();

    fs::path exe_path = fs::canonical(fs::absolute(argv[0]));
    fs::path exe_dir = exe_path.parent_path();

    string config_path = argv[1];
    config config = readConfig(config_path);

    long long size = config.size;
    long long slide = config.slide;
    long long max_size = config.max_size;
    long long min_size = config.min_size;
    long long query_type = config.query_type;
    // TODO: read z-score from config
    double zscore = 2.5;

    fs::path data_path = exe_dir / config.input_data_path;
    std::string data_folder = data_path.parent_path().filename().string();
    data_path = fs::absolute(data_path).lexically_normal();

    cout << "Dataset: " << data_folder << endl;

    std::ifstream fin(data_path);
    if (!fin.is_open())
    {
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
    bool first_edge = true;
    long long edge_number = 0;
    long long time;
    long long timestamp;

    long long window_offset = 0;

    bool evict = false;
    vector<size_t> to_evict;
    long long last_t_open = -1;

    // long long checkpoint = 9223372036854775807L;
    long long checkpoint = 1000000;

    vector<long long> node_count;
    long long saved_edges = 0;

    std::string mode = config.adaptive ? "ad" : "sl";
    bool ADAPTIVE_WINDOW = config.adaptive > 0;
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

    int warmup = 0;
    double last_cost = 0.0;
    double last_diff = 0.0;

    const int overlap = size / slide;
    std::deque<double> cost_window;
    std::deque<double> normalization_window;

    std::ofstream csv_summary(data_folder + "_summary_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" + std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size) + ".csv");
    csv_summary << "total_edges,matches,exec_time,windows_created,avg_window_cardinality,avg_window_size\n";

    std::ofstream csv_windows(data_folder + "_window_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" + std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size) + ".csv");
    csv_windows << "index,t_open,t_close,window_results,incremental_matches,latency,window_cardinality,window_size\n";

    std::ofstream csv_tuples(data_folder + "_tuples_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" + std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size) + ".csv");
    csv_tuples << "estimated_cost,normalized_estimated_cost,latency,normalized_latency,window_cardinality,widow_size\n";

    std::ofstream csv_memory(data_folder + "_memory_results_" + std::to_string(query_type) + "_" + std::to_string(size) + "_" + std::to_string(slide) + "_" + mode + "_" + std::to_string(min_size) + "_" + std::to_string(max_size) + ".csv");
    csv_memory << "tot_virtual,used_virtual,tot_ram,used_ram,data_mem\n";

    clock_t start = clock();
    long long s, d, l, t;
    while (fin >> s >> d >> l >> t)
    {
        signalHandler.waitForSignal();

        signalHandler.setNestedResponse("new_edge", "s", std::to_string(s));
        signalHandler.setNestedResponse("new_edge", "d", std::to_string(d));
        signalHandler.setNestedResponse("new_edge", "l", std::to_string(l));
        signalHandler.setNestedResponse("new_edge", "t", std::to_string(t));

        if (first_edge)
        {
            t0 = t;
            timestamp = 1;
            first_edge = false;
        }
        else
            timestamp = t - t0;

        if (timestamp < 0)
            exit(1);

        time = timestamp;

        // process the edge if the label is part of the query
        if (!aut->hasLabel(l))
            continue;

        edge_number++;

        if (l == first_transition)
            EINIT_count++;

        long long window_close;
        double base = std::floor(static_cast<double>(time) / slide) * slide;
        double o_i = base;
        bool new_window = true;
        do
        {
            auto window_open = static_cast<long long>(o_i);
            window_close = static_cast<long long>(o_i + size);

            for (size_t i = window_offset; i < windows.size(); i++)
            {
                if (windows[i].t_open == window_open && windows[i].t_close == window_close)
                { // computed window is already present in WSS
                    new_window = false;
                }
            }

            if (new_window)
            {
                if (windows.empty())
                {
                    windows.emplace_back(window_open, window_close, nullptr, nullptr);
                }
                else
                {
                    if (window_close < windows[windows.size() - 1].t_close)
                    {
                        for (size_t j = windows.size() - 1; j >= window_offset; j--)
                        {
                            if (windows[j].t_close > window_close)
                            {
                                windows[j].evicted = true;
                                windows[j].first = nullptr;
                                windows[j].last = nullptr;
                                windows[j].latency = -1;
                                windows[j].normalized_latency = -1;
                                windows.pop_back();
                            }
                        }
                    }
                    if (window_close > windows[windows.size() - 1].t_close && window_open > windows[windows.size() - 1].t_open)
                    {
                        if (windows[windows.size() - 1].t_close < window_close)
                        {
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
            }

            o_i += slide;
        } while (o_i < time);

        timed_edge *t_edge;
        sg_edge *new_sgt;

        new_sgt = sg->insert_edge(edge_number, s, d, l, time, window_close);

        if (!new_sgt)
        {
            // search for the duplicate
            cerr << "ERROR: new sgt is null, time: " << time << endl;
            exit(1);
        }

        // add edge to time list
        t_edge = new timed_edge(new_sgt); // associate the timed edge with the snapshot graph edge
        sg->add_timed_edge(t_edge);       // append the element to the time list

        // update window boundaries and check for window eviction
        for (size_t i = window_offset; i < windows.size(); i++)
        {
            if (windows[i].t_open <= time && time < windows[i].t_close)
            { // active window
                if (!windows[i].first || time < windows[i].first->edge_pt->timestamp)
                {
                    if (!windows[i].first)
                        windows[i].last = t_edge;
                    windows[i].first = t_edge;
                    windows[i].elements_count++;
                    total_elements_count++;
                }
                else if (!windows[i].last || time >= windows[i].last->edge_pt->timestamp)
                {
                    windows[i].last = t_edge;
                    windows[i].elements_count++;
                    total_elements_count++;
                }
                if (sg->density[s] > windows[i].max_degree)
                {
                    windows[i].max_degree = sg->density[s];
                }
            }
            else if (time >= windows[i].t_close)
            { // schedule window for eviction
                cout << "window close: " << windows[i].t_close << ", time: " << time << endl;
                window_offset = i + 1;
                to_evict.push_back(i);
                evict = true;
                if (windows[i].elements_count == 0)
                {
                    cerr << "ERROR: Empty window: " << i << endl;
                    exit(1);
                }
            }
        }
        new_sgt->time_pos = t_edge; // associate the snapshot graph edge with the timed edge

        cumulative_size += size;
        size_count++;
        avg_size = cumulative_size / size_count;

        /* EVICT */
        if (evict)
        {
            // to compute window cost, we take the size of the snapshot graph of the window here, since no more elements will be added and it can be considered complete and closed
            std::vector<pair<long long, long long>> candidate_for_deletion;
            timed_edge *evict_start_point = windows[to_evict[0]].first;
            timed_edge *evict_end_point = windows[to_evict.back() + 1].first;
            long long to_evict_timestamp = windows[to_evict.back() + 1].t_open;

            if (!evict_start_point)
            {
                cerr << "ERROR: Evict start point is null." << endl;
                exit(1);
            }

            if (evict_end_point == nullptr)
            {
                evict_end_point = sg->time_list_tail;
                cout << "WARNING: Evict end point is null, evicting whole buffer." << endl;
            }

            if (last_t_open != windows[to_evict[0]].t_open)
                sink->refresh_resultSet(windows[to_evict[0]].t_open);
            last_t_open = windows[to_evict[0]].t_open;

            timed_edge *current = evict_start_point;

            while (current && current != evict_end_point)
            {

                auto cur_edge = current->edge_pt;
                auto next = current->next;

                if (cur_edge->label == first_transition)
                    EINIT_count--;

                if (cur_edge->lives == 1 || sg->get_zscore(cur_edge->s) > zscore || sg->get_zscore(cur_edge->d) > zscore)
                { // if (cur_edge->lives == 1 || (static_cast<double>(rand()) / RAND_MAX) < 0.005)
                    // check for parent switch before final deletion
                    candidate_for_deletion.emplace_back(cur_edge->s, cur_edge->d);
                    sg->remove_edge(cur_edge->s, cur_edge->d, cur_edge->label); // delete from adjacency list
                    sg->delete_timed_edge(current);                             // delete from window state store
                }
                else
                {
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
                    double raw_shift = static_cast<double>(windows.size() - 1) - std::ceil(selected_score);

                    // define min and max shift bounds based on your data/logic
                    auto propagation_start = static_cast<double>(size) / static_cast<double>(slide);
                    propagation_start = ceil(propagation_start / 2);
                    double min_shift = static_cast<double>(to_evict.back()) + propagation_start;
                    auto max_shift = static_cast<double>(windows.size() - 1); // or an empirical upper bound

                    // alpha is your desired max output shift
                    double resized_shift = normalize_shift(raw_shift, min_shift, max_shift, size / slide);

                    // finally compute the target index
                    auto target_window_index = std::clamp(min_shift + resized_shift, min_shift, static_cast<double>(windows.size() - 1));

                    sg->shift_timed_edge(cur_edge->time_pos, windows[target_window_index].first);
                    windows[target_window_index].elements_count++;
                }

                current = next;
            }

            // reset time list pointers
            sg->time_list_head = evict_end_point;
            sg->time_list_head->prev = nullptr;

            f->expire_timestamped(to_evict_timestamp, candidate_for_deletion);

            // mark window as evicted
            for (unsigned long i : to_evict)
            {
                warmup++;
                windows[i].evicted = true;
                windows[i].first = nullptr;
                windows[i].last = nullptr;
                windows[i].latency = static_cast<double>(clock() - windows[i].start_time) / CLOCKS_PER_SEC;
                if (windows[i].latency > lat_max)
                    lat_max = windows[i].latency;
                if (windows[i].latency < lat_min)
                    lat_min = windows[i].latency;
                windows[i].normalized_latency = (windows[i].latency - lat_min) / (lat_max - lat_min);
                // state_size,estimated_cost,normalized_estimated_cost,latency,normalized_latency
            }

            to_evict.clear();
            candidate_for_deletion.clear();
            evict = false;

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

        if (new_window)
        {
            if (window_offset != windows.size() - 1)
            {
                cout << "ERROR " << windows.size() - window_offset << " active windows\n";
            }
            else
            {
                signalHandler.setNestedResponse(
                    "active_window",
                    "open",
                    std::to_string(windows[windows.size() - 1].t_open));
                signalHandler.setNestedResponse(
                    "active_window",
                    "close",
                    std::to_string(windows[windows.size() - 1].t_close));

                timed_edge *curr = windows[windows.size() - 1].first;
                std::vector<crow::json::wvalue> edges;

                while (curr)
                {
                    sg_edge *e = curr->edge_pt;
                    crow::json::wvalue edge_json;

                    edge_json["s"] = std::to_string(e->s);
                    edge_json["d"] = std::to_string(e->d);
                    edge_json["l"] = std::to_string(e->label);
                    edge_json["t"] = std::to_string(e->timestamp);

                    edges.push_back(std::move(edge_json));

                    curr = curr->next;
                }

                signalHandler.setNestedResponse(
                    "active_window",
                    "edges",
                    std::move(edges));

                // Snapshot Graph
                // for (const auto &[vertex, edges] : sg->adjacency_list)
                // {
                //     std::cout << "Vertex " << vertex << " -> ";
                //     if (edges.empty())
                //     {
                //         std::cout << "(no outgoing edges)";
                //     }
                //     else
                //     {
                //         for (size_t i = 0; i < edges.size(); ++i)
                //         {
                //             const auto &[to, edge] = edges[i];
                //             std::cout << "(" << to << ", label:" << edge->label
                //                       << ", t:" << edge->timestamp << ")";
                //             if (i < edges.size() - 1)
                //                 std::cout << ", ";
                //         }
                //     }
                //     std::cout << "\n";
                // }
            }
        }
        else
        {
            std::vector<crow::json::wvalue> edges;
            crow::json::wvalue edge_json;

            edge_json["s"] = std::to_string(t_edge->edge_pt->s);
            edge_json["d"] = std::to_string(t_edge->edge_pt->d);
            edge_json["l"] = std::to_string(t_edge->edge_pt->label);
            edge_json["t"] = std::to_string(t_edge->edge_pt->timestamp);

            edges.push_back(std::move(edge_json));
            signalHandler.setNestedResponse(
                "active_window",
                "edges",
                std::move(edges));
        }

        cout << "\nRemaining active windows:\n";
        for (size_t i = window_offset; i < windows.size(); i++)
        {
            if (!windows[i].evicted)
            {
                cout << "  Window [" << i << "]: [" << windows[i].t_open << ", "
                     << windows[i].t_close << ") - " << windows[i].elements_count
                     << " edges\n";

                // Print edges in this window
                if (windows[i].first)
                {
                    cout << "    Edges:\n";
                    timed_edge *curr = windows[i].first;
                    int edge_count = 0;

                    while (curr)
                    {
                        sg_edge *e = curr->edge_pt;
                        cout << "      [" << (edge_count + 1) << "] "
                             << e->s << " -> " << e->d
                             << " (L:" << e->label
                             << ", T:" << e->timestamp
                             << ", Exp:" << e->expiration_time
                             << ", Lives:" << e->lives << ")\n";

                        // Stop at last edge or when timestamp exceeds window
                        if (curr == windows[i].last || e->timestamp >= windows[i].t_close)
                        {
                            break;
                        }

                        curr = curr->next;
                        edge_count++;
                    }
                }
                else
                {
                    cout << "    No edges (first pointer is null)\n";
                }
            }
        }
        cout << "=======================================================\n\n";

        // if (ADAPTIVE_WINDOW && windows.size()-1 >= adap_count) {

        /* QUERY */
        query->pattern_matching_tc(new_sgt);

        if (edge_number % checkpoint == 0)
        {
            printf("processed edges: %lld\n", edge_number);
            printf("saved edges: %lld\n", saved_edges);
            printf("avg degree: %f\n", sg->mean);
            cout << "matched paths: " << sink->matched_paths << "\n\n";
        }

        // estimated_cost,normalized_estimated_cost,latency,normalized_latency,window_cardinality,widow_size
        csv_tuples
            << cost << ","
            << cost_norm << ","
            << windows[window_offset >= 1 ? window_offset - 1 : 0].latency << ","
            << windows[window_offset >= 1 ? window_offset - 1 : 0].normalized_latency << ","
            << windows[window_offset >= 1 ? window_offset - 1 : 0].elements_count << ","
            << windows[window_offset >= 1 ? window_offset - 1 : 0].t_close - windows[window_offset >= 1 ? window_offset - 1 : 0].t_open << endl;
    }

    cout << "Created windows: " << windows.size() << endl;

    clock_t finish = clock();
    long long time_used = (double)(finish - start) / CLOCKS_PER_SEC;

    double avg_window_size = static_cast<double>(total_elements_count) / windows.size();

    csv_summary
        << edge_number << ","
        << sink->matched_paths << ","
        << time_used << ","
        << windows.size() << ","
        << avg_window_size << ","
        << avg_size << "\n";

    // index,t_open,t_close,window_results,incremental_matches,latency,window_cardinality,window_size
    for (size_t i = 0; i < windows.size(); ++i)
    {
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

    signalHandler.stop();

    return 0;
}

// Set up the automaton correspondant for each query
int setup_automaton(long long query_type, FiniteStateAutomaton *aut, const vector<long long> &labels)
{
    int states_count = 0;
    /*
     * 0 - initial state
     * 0 -> 1 - first transition
     * Always enumerate the states starting from 0 and incrementing by 1.
     */
    switch (query_type)
    {
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