#include <vector>
#include <string>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <random>

#define MEMORY_PROFILER false
#define DEBUG false

#include "sys/types.h"

#include "source/fsa.h"
#include "source/streaming_graph.h"
#include "source/query_handler.h"
#include "source/modes/mode_handler.h"
#include "source/modes/mode_factory.h"

namespace fs = std::filesystem;
using namespace std;

int main(int argc, char *argv[]) {
    fs::path exe_path = fs::canonical(fs::absolute(argv[0]));
    fs::path exe_dir = exe_path.parent_path();

    string config_path = argv[1];
    config config = readConfig(config_path);

    fs::path data_path = fs::current_path() / config.input_data_path;
    std::string data_folder = data_path.parent_path().filename().string();
    data_path = fs::absolute(data_path).lexically_normal();

    cout << "Dataset: " << data_folder << endl;


    std::filesystem::path config_path_folder = std::filesystem::absolute(argv[1]).lexically_normal();
    std::string config_folder_name = config_path_folder.parent_path().filename().string();
    std::cout << "Config folder: " << config_folder_name << std::endl;


    std::ifstream fin(data_path);
    if (!fin.is_open()) {
        std::cerr << "Error: Failed to open " << data_path << std::endl;
        exit(1);
    }
    // if max size < min size, exit
    if (config.max_size < config.min_size) {
        cerr << "ERROR: max_size < min_size" << endl;
        exit(1);
    }

    ModeContext ctx;
    ctx.size = config.size;
    ctx.slide = config.slide;
    ctx.max_size = config.max_size;
    ctx.min_size = config.min_size;
    ctx.mode = config.mode;
    ctx.latency_max = config.l_max;
    if (config.size >0 && config.slide >0) ctx.overlap = config.size / config.slide;
    ctx.granularity = static_cast<double>(ctx.min_size) / 100.0;
    ctx.max_shed = static_cast<double>(ctx.max_size) / 100.0;

    ctx.sink = new Sink();
    ctx.aut = new FiniteStateAutomaton(config.query_type, config.labels);
    ctx.sg = new streaming_graph(config.labels[0], ctx.mode);
    ctx.q = new QueryHandler(*ctx.aut, *ctx.sg, *ctx.sink, config.path_algorithm);

    // Create mode handler using factory
    double delta = 1.0 / static_cast<double>(config.min_size);
    long long labels_size = *std::max_element(config.labels.begin(), config.labels.end());
    auto mode_handler = ModeFactory::create_mode_handler(config.mode, delta, labels_size, config.budget);
    ctx.cumulative_processing_time_type.resize(labels_size + 1, 0.0);
    ctx.processed_elements_type.resize(labels_size + 1, 0);
    ctx.input_rate_type.resize(labels_size + 1, 0.0);

    std::string mode;
    switch (config.mode) {
        case 10: mode = "sl";
            cout << "Window size: " << config.size << endl;
            cout << "Window slide: " << config.slide << endl;
            break;
        case 11: mode = "ad_function";
            cout << "Window size: " << config.size << endl;
            cout << "Window slide: " << config.slide << endl;
            cout << "Min window size: " << config.min_size << endl;
            cout << "Max window size: " << config.max_size << endl;
            break;
        case 12: mode = "ad_degree";
            cout << "Window size: " << config.size << endl;
            cout << "Window slide: " << config.slide << endl;
            cout << "Min window size: " << config.min_size << endl;
            cout << "Max window size: " << config.max_size << endl;
            break;
        case 13: mode = "ad_einit";
            cout << "Window size: " << config.size << endl;
            cout << "Window slide: " << config.slide << endl;
            cout << "Min window size: " << config.min_size << endl;
            cout << "Max window size: " << config.max_size << endl;
            break;
        case 14: mode = "maxdeg";
            cout << "Window size: " << config.size << endl;
            cout << "Window slide: " << config.slide << endl;
            cout << "Min window size: " << config.min_size << endl;
            cout << "Max window size: " << config.max_size << endl;
            break;
        case 15: mode = "ad_complexity";
            cout << "Window size: " << config.size << endl;
            cout << "Window slide: " << config.slide << endl;
            cout << "Min window size: " << config.min_size << endl;
            cout << "Max window size: " << config.max_size << endl;
            break;
        case 2: mode = "adwin";
            cout << "ADWIN configuration: " << endl;
            cout << "  - delta: " << delta << endl;
            break;
        case 3: mode = "lshed_prob";
            cout << "Load shedding mode activated." << endl;
            cout << "Window size: " << config.size << endl;
            cout << "Window slide: " << config.slide << endl;
            break;
        case 4: mode = "lshed_random";
            cout << "Load shedding mode activated." << endl;
            cout << "Window size: " << config.size << endl;
            cout << "Window slide: " << config.slide << endl;
            break;
        case 5: mode = "lshed_darling";
            cout << "Load shedding mode activated." << endl;
            cout << "Window size: " << config.size << endl;
            cout << "Window slide: " << config.slide << endl;
            cout << "Max latency: " << config.l_max << endl;
            break;
        case 60: mode = "summary_random";
            cout << "Random Summary Selection mode activated." << endl;
            cout << "Window size: " << config.size << endl;
            cout << "Window slide: " << config.slide << endl;
            cout << "Budget: " << config.budget << endl;
            break;
        case 61: mode = "summary_fifo";
            cout << "FIFO Summary Selection mode activated." << endl;
            cout << "Window size: " << config.size << endl;
            cout << "Window slide: " << config.slide << endl;
            cout << "Budget: " << config.budget << endl;
            break;
        case 62: mode = "summary_selection";
            cout << "Summary Selection mode activated." << endl;
            break;
        default:
            cerr << "ERROR: Unknown mode" << endl;
            exit(4);
    }

    cout << "Mode: " << mode << " (config. " << config.mode << ")" << endl;
    string algo;
    switch (config.path_algorithm) {
        case 1:
            cout << "S_PATH" << endl;
            algo = "SPATH";
            break;
        case 2:
            cout << "LM_SRPQ" << endl;
            algo = "LMSRPQ";
            break;
        default:
            cerr << "ERROR: Unknown path algorithm" << endl;
            exit(4);
    }

    // output folder for csvs
    fs::path output_folder = "results";
    if (!fs::exists(output_folder)) {
        fs::create_directories(output_folder);
    }

    // Base filename (without folder)
    const std::string base =
        data_folder + "_" + std::to_string(config.query_type) + "_" + std::to_string(config.size) + "_" +
        std::to_string(config.slide) + "_" + mode + "_" + std::to_string(config.min_size) + "_" + std::to_string(config.max_size)
        + "_" + algo;

    // Build full paths under output_folder
    const fs::path summary_path = output_folder / (base + "_summary_results.csv");
    const fs::path windows_path = output_folder / (base + "_window_results.csv");
    const fs::path tuples_path  = output_folder / (base + "_tuples_results.csv");
    const fs::path memory_path  = output_folder / (base + "_memory_results.csv");
    const fs::path slides_path  = output_folder / (base + "_slides_results.csv");

    // Snapshot CSVs for ML counter projection
    const fs::path snap_vertex_path = output_folder / (base + "_snapshot_vertices.csv");
    const fs::path snap_tree_path   = output_folder / (base + "_snapshot_trees.csv");
    const fs::path snap_forest_path = output_folder / (base + "_snapshot_forest.csv");
    std::ofstream csv_snap_vertex(snap_vertex_path.string());
    std::ofstream csv_snap_tree(snap_tree_path.string());
    std::ofstream csv_snap_forest(snap_forest_path.string());
    if (DEBUG) ctx.q->write_snapshot_headers(csv_snap_vertex, csv_snap_tree, csv_snap_forest);


    std::ofstream csv_summary(summary_path.string());
    csv_summary << "total_edges,shed_edges,matches,exec_time,windows_created,avg_window_cardinality,avg_window_size\n";

    std::ofstream csv_windows(windows_path.string());
    csv_windows << "window_id,t_open,t_close,normalized_estimated_cost,window_results,incremental_matches,latency,window_cardinality,window_size\n";

    std::ofstream csv_tuples(tuples_path.string());
    csv_tuples << "window_id,beta_id,timestamp,estimated_cost,normalized_estimated_cost,latency,beta_latency,window_cardinality,window_size,shedding\n";

    std::ofstream csv_memory(memory_path.string());
    csv_memory << "alef,avg_deg,lef,max_deg,nm\n";

    std::ofstream csv_slides(slides_path.string());
    csv_slides << "t_open,t_close,latency_sec,elements,new_results,cost_norm\n";

    ctx.csv_tuples = &csv_tuples;
    ctx.csv_memory = &csv_memory;
    int snapshot_id = 0;
    size_t last_slide_count = 0;

    int elements_processed = 0;
    double cumulative_processing_time = 0;

    clock_t start = clock();
    ctx.beta_latency_start = clock();

    long long t0 = 0;
    long long time;
    ctx.windows.emplace_back(0, config.size, nullptr, nullptr, 0);
    long long s, d, l, t;
    while (fin >> s >> d >> l >> t) {
        if (t0 == 0) t0 = t;
        time = t - t0;
        if (time < 0) continue;
        if (time == 0) time = 1;

        // process the edge if the label is part of the query
        if (!ctx.aut->hasLabel(l))
            continue;

        sg_edge *new_sgt = nullptr;
        bool is_shed = false;
        clock_t processing_time_start = clock();

        is_shed = mode_handler->process_edge(s, d, l, time, ctx, &new_sgt);
        if (new_sgt) {  // Only process query if an edge was created (not shed in load shedding mode)
            ctx.q->run(new_sgt);
        }

        double processing_time_used = static_cast<double>(clock() - processing_time_start) / CLOCKS_PER_SEC;
        if (!is_shed) { // compute the average time used do process an event in a steady state of the stream
            cumulative_processing_time += processing_time_used;
            elements_processed++;
        }
        ctx.average_processing_time = cumulative_processing_time / static_cast<double>(elements_processed);

        ctx.cumulative_processing_time_type[l] += processing_time_used;
        ctx.processed_elements_type[l]++;
        ctx.input_rate_type[l] = ctx.processed_elements_type[l] / static_cast<double>(clock() - start);

        // Take snapshot at each slide boundary
        if (ctx.slides.size() > last_slide_count && DEBUG) {
            last_slide_count = ctx.slides.size();
            int win_id = static_cast<int>(ctx.windows.size()) - 1;
            long long snap_t_open = ctx.windows[win_id >= 0 ? win_id : 0].t_open;
            long long snap_t_close = ctx.windows[win_id >= 0 ? win_id : 0].t_close;
            ctx.q->dump_snapshot(csv_snap_vertex, csv_snap_tree, csv_snap_forest,
                                 snapshot_id, win_id, time, snap_t_open, snap_t_close,
                                 ctx.sink->matched_paths);
            snapshot_id++;
        }

        if (long long checkpoint = 200000; elements_processed % checkpoint == 0) {
            printf("processed edges: %d\n", elements_processed);
            printf("avg degree: %f\n", ctx.sg->edge_num/ctx.sg->vertex_num);
            cout << std::fixed << std::setprecision(5);
            cout << "average processing time: " << ctx.average_processing_time << " seconds\n";
            cout << "matched paths: " << ctx.sink->matched_paths << "\n\n";
        }
    }
    if (!ctx.slides.empty()) {
        ctx.slides.back().wall_close = clock();
        ctx.slides.back().results_at_close = ctx.sink->matched_paths;
    }
    // Close the last window (never evicted, so results_at_close is still 0)
    if (!ctx.windows.empty() && ctx.windows.back().results_at_close == 0) {
        ctx.windows.back().results_at_close = ctx.sink->matched_paths;
    }
    clock_t finish = clock();
    long long time_used = static_cast<double> (finish - start) / CLOCKS_PER_SEC;

    cout << "Created windows: " << ctx.windows.size() << endl;
    cout << "Total execution time: " << time_used << " seconds" << endl;
    cout << "Shed edges: " << ctx.sg->shed_count << endl;

    double avg_window_size = static_cast<double>(ctx.total_elements_count) / ctx.windows.size();

    csv_summary
            << ctx.edge_number << ","
            << ctx.sg->shed_count << ","
            << ctx.sink->matched_paths << ","
            << time_used << ","
            << ctx.windows.size() << ","
            << avg_window_size << ","
            << ctx.avg_size << "\n";

    // index,t_open,t_close,window_results,incremental_matches,latency,window_cardinality,window_size
    for (size_t i = 0; i < ctx.windows.size(); ++i) {
        csv_windows
                << i << ","
                << ctx.windows[i].t_open << ","
                << ctx.windows[i].t_close << ","
                << ctx.windows[i].cost << ","
                << (ctx.windows[i].results_at_close - ctx.windows[i].results_at_open) << ","
                << ctx.windows[i].total_matched_results << ","
                << ctx.windows[i].latency << ","
                << ctx.windows[i].elements_count << ","
                << ctx.windows[i].t_close - ctx.windows[i].t_open << "\n";
    }

    for (auto&[t_open, t_close, wall_open, wall_close, elements_count, results_at_open, results_at_close, cost_norm] : ctx.slides) {
        double latency = static_cast<double>(wall_close - wall_open) / CLOCKS_PER_SEC;
        csv_slides << t_open << ","
                   << t_close << ","
                   << latency << ","
                   << elements_count << ","
                   << (results_at_close - results_at_open) << ","
                   << cost_norm << "\n";
    }

    //ctx.sink->exportResultSet(base + "_result_set.csv");

    if (mode == "adwin") {
        // print maximum and minimum window sizes
        int max = 0;
        int min = 922337368;
        for (auto & window : ctx.windows) {
            if (window.t_close - window.t_open > max) {
                max = window.t_close - window.t_open;
            }
            if (window.t_close - window.t_open < min) {
                min = window.t_close - window.t_open;
            }
        }
        cout << "min window size: " << min << endl;
        cout << "max window size: " << max << endl;
    }

    // Final snapshot at end of stream
    if (!ctx.windows.empty() && DEBUG) {
        int win_id = static_cast<int>(ctx.windows.size()) - 1;
        ctx.q->dump_snapshot(csv_snap_vertex, csv_snap_tree, csv_snap_forest,
                             snapshot_id, win_id, time,
                             ctx.windows[win_id].t_open, ctx.windows[win_id].t_close,
                             ctx.sink->matched_paths);
    }

    csv_summary.close();
    csv_windows.close();
    csv_tuples.close();
    csv_memory.close();
    csv_snap_vertex.close();
    csv_snap_tree.close();
    csv_snap_forest.close();

    // cleanup
    delete ctx.sg;
    delete ctx.sink;
    delete ctx.aut;
    delete ctx.q;

    return 0;
}
