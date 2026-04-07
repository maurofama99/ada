
#include "mode_handler_base.h"
#include "../streaming_graph.h"
#include <iostream>
#include <numeric>

double ModeHandlerBase::compute_load_estimation(ModeContext &ctx, int mode) {
    ctx.max_deg = 1;
    for (size_t i = ctx.window_offset; i < ctx.windows.size(); i++) {
        if ((ctx.windows)[i].max_degree > ctx.max_deg) ctx.max_deg = (ctx.windows)[i].max_degree;
    }

    double avg_deg = ctx.sg->edge_num/ctx.sg->vertex_num;

    double n = (ctx.sg->EINIT_count + 1) * (2 * ctx.sg->edge_num - ctx.sg->EINIT_count) / 2.0;

    switch (mode) {
        case 11:
            ctx.cost = n / ctx.max_deg; // ALEF
            break;
        case 12:
            ctx.cost = avg_deg;
            break;
        case 13:
            ctx.cost = n; // LEF
            break;
        case 14:
            ctx.cost = ctx.max_deg;
            break;
        case 15:
            ctx.cost = ctx.sg->EINIT_count * ctx.sg->edge_num;
            break;
        default:
            std::cerr << "ERROR: unknown cost mode." << std::endl;
            exit(1);
    }

    *(ctx.csv_memory) << n / ctx.max_deg << "," << avg_deg << "," << n << "," << ctx.max_deg << "," << ctx.sg->EINIT_count * ctx.sg->edge_num << std::endl;

    if (ctx.cost > ctx.cost_max) ctx.cost_max = ctx.cost;
    if (ctx.cost < ctx.cost_min) ctx.cost_min = ctx.cost;
    ctx.cost_norm = (ctx.cost - ctx.cost_min) / (ctx.cost_max - ctx.cost_min);

    ctx.cost_window.push_back(ctx.cost_norm);
    if (ctx.cost_window.size() > ctx.overlap)
        ctx.cost_window.pop_front();

    ctx.cost_norm = std::accumulate(ctx.cost_window.begin(), ctx.cost_window.end(), 0.0) / ctx.cost_window.size();

    double cost_diff = ctx.cost_norm - ctx.last_cost;
    ctx.last_cost = ctx.cost_norm;

    if (std::isnan(ctx.last_diff)) {
        ctx.last_diff = 0;
    }
    if (std::isnan(cost_diff)) {
        cost_diff = 0;
    }
    
    (ctx.windows)[(ctx.window_offset)].cost = ctx.cost_norm;

    return cost_diff;

}

long long ModeHandlerBase::compute_window_boundaries(ModeContext &ctx, long long time) { 
    
    long long window_close;
    double o_i = std::floor(static_cast<double>(time) / ctx.slide) * ctx.slide;
    if (o_i > ctx.last_oi) {
        ctx.last_oi = o_i;
        clock_t now = clock();

        // Close the previous slide
        if (!ctx.slides.empty()) {
            ctx.slides.back().wall_close = now;
            ctx.slides.back().results_at_close = ctx.sink->matched_paths;
            // cost_norm will be filled after compute_load_estimation runs
        }

        // Open the new slide
        Slide s;
        s.t_open     = static_cast<long long>(o_i);
        s.t_close    = static_cast<long long>(o_i + ctx.slide);
        s.wall_open  = now;
        s.results_at_open = ctx.sink->matched_paths;
        ctx.slides.push_back(s);
        ctx.current_slide_open = s.t_open;

        if (!ctx.slides.empty()) {
            ctx.slides.back().cost_norm = ctx.cost_norm;
        }

        ctx.beta_latency_end = (clock() - ctx.beta_latency_start) / CLOCKS_PER_SEC;
        ctx.beta_latency_start = clock();
        ctx.beta_elements_cont = 0;
    }

    bool new_window = true;
    do {
        auto window_open = static_cast<long long>(o_i);
        window_close = static_cast<long long>(o_i + ctx.size);

        for (size_t i = ctx.window_offset; i < ctx.windows.size(); i++) {
            if ((ctx.windows)[i].t_open == window_open && (ctx.windows)[i].t_close == window_close) {
                // computed window is already present in WSS
                new_window = false;
            }
        }

        if (new_window) {
            if (window_close < (ctx.windows)[ctx.windows.size() - 1].t_close) {    // shrink
                for (size_t j = ctx.windows.size() - 1; j >= ctx.window_offset; j--) {
                    if ((ctx.windows)[j].t_close > window_close) {
                        (ctx.windows)[j].evicted = true;
                        (ctx.windows)[j].first = nullptr;
                        (ctx.windows)[j].last = nullptr;
                        (ctx.windows)[j].latency = -1;
                        (ctx.windows)[j].normalized_latency = -1;
                        ctx.windows.pop_back();
                    }
                }
            }
            if (window_close > (ctx.windows)[ctx.windows.size() - 1].t_close && window_open > (ctx.windows)[ctx.windows.size() - 1].t_open) {  // expand
                if ((ctx.windows)[ctx.windows.size() - 1].t_close < window_close) {
                    // report results
                    (ctx.windows)[ctx.windows.size() - 1].total_matched_results = ctx.sink->matched_paths;
                    // matched paths until this window
                    (ctx.windows)[ctx.windows.size() - 1].emitted_results = ctx.sink->getResultSetSize();
                    // paths emitted on this window close
                    (ctx.windows)[ctx.windows.size() - 1].window_matches = (ctx.windows)[ctx.windows.size() - 1].emitted_results;
                }
                ctx.windows.emplace_back(window_open, window_close, nullptr, nullptr, ctx.sink->matched_paths);
            }
        }

        o_i += ctx.slide;
    } while (o_i < time);
    
    return window_close;
}

bool ModeHandlerBase::update_window(ModeContext &ctx, sg_edge* new_sgt, long long time, long long s) {
    bool evict = false;
    auto* t_edge = new timed_edge(new_sgt); // associate the timed edge with the snapshot graph edge
    ctx.sg->add_timed_edge(t_edge); // append the element to the time list
    new_sgt->time_pos = t_edge; // associate the snapshot graph edge with the timed edge

    // update window boundaries and check for window eviction
    for (size_t i = ctx.window_offset; i < ctx.windows.size(); i++) {
        if ((ctx.windows)[i].t_open <= time && time < (ctx.windows)[i].t_close) {
            // active window
            (ctx.total_elements_count)++;
            (ctx.windows)[i].elements_count++;
            if (ctx.sg->out_degree[s] > (ctx.windows)[i].max_degree) {
                (ctx.windows)[i].max_degree = ctx.sg->out_degree[s];
            }
        } else if (time >= (ctx.windows)[i].t_close) {
            // schedule window for eviction
            ctx.window_offset = i + 1;
            ctx.to_evict.push_back(i);
            evict = true;
        }
    }

    if (!ctx.slides.empty() && time >= ctx.slides.back().t_open && time < ctx.slides.back().t_close) {
        ctx.slides.back().elements_count++;
    }

    ctx.cumulative_size += ctx.size;
    (ctx.size_count)++;
    ctx.avg_size = ctx.cumulative_size / ctx.size_count;

    (ctx.window_cardinality)++;
    ctx.beta_elements_cont++;

    return evict;
}

std::vector<streaming_graph::expired_edge_info> ModeHandlerBase::evict (ModeContext &ctx, long long time) {

    long long eviction_time = (ctx.windows)[ctx.to_evict.back() + 1].t_open;

    std::vector<streaming_graph::expired_edge_info> deleted_edges;
    ctx.sg->expire(eviction_time, deleted_edges);
    ctx.window_cardinality -= deleted_edges.size();
    ctx.q->update_state(eviction_time, deleted_edges);

    return deleted_edges;
}

void ModeHandlerBase::mark_windows_evicted(ModeContext &ctx) {
    for (unsigned long i: ctx.to_evict) {
        (ctx.windows)[i].evicted = true;
        ctx.windows[i].latency = static_cast<double>(clock() - (ctx.windows)[i].start_time) / CLOCKS_PER_SEC;
        if ((ctx.windows)[i].latency > ctx.lat_max) ctx.lat_max = (ctx.windows)[i].latency;
        if ((ctx.windows)[i].latency < ctx.lat_min) ctx.lat_min = (ctx.windows)[i].latency;
        (ctx.windows)[i].normalized_latency = ((ctx.windows)[i].latency - ctx.lat_min) / (ctx.lat_max - ctx.lat_min);
        ctx.cumulative_window_latency += (ctx.windows)[i].latency;
        ctx.windows[i].results_at_close = ctx.sink->matched_paths;
    }

    (ctx.warmup)++;
    ctx.to_evict.clear();
}
