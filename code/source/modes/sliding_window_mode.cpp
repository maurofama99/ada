#include "sliding_window_mode.h"
#include <cmath>
#include <algorithm>
#include <numeric>

bool SlidingWindowMode::process_edge(long long s, long long d, long long l, long long time, ModeContext& ctx, sg_edge** new_sgt_out) {
    (*ctx.edge_number)++;
    int acc_threshold = 0;
    if (ctx.mode >= 11 && ctx.mode <= 14) acc_threshold = 1;

    long long window_close;
    double o_i = std::floor(static_cast<double>(time) / ctx.slide) * ctx.slide;

    if (o_i > (*ctx.windows)[ctx.windows->size()-1].t_open) {
        ctx.beta_latency_end = (double) (clock() - ctx.beta_latency_start) / CLOCKS_PER_SEC;
        ctx.beta_latency_start = clock();
        ctx.beta_id++;
    }

    bool new_window = true;
    do {
        auto window_open = static_cast<long long>(o_i);
        window_close = static_cast<long long>(o_i + ctx.size);

        for (size_t i = *ctx.window_offset; i < ctx.windows->size(); i++) {
            if ((*ctx.windows)[i].t_open == window_open && (*ctx.windows)[i].t_close == window_close) {
                // computed window is already present in WSS
                new_window = false;
            }
        }

        if (new_window) {
            if (window_close < (*ctx.windows)[ctx.windows->size() - 1].t_close) {    // shrink
                for (size_t j = ctx.windows->size() - 1; j >= *ctx.window_offset; j--) {
                    if ((*ctx.windows)[j].t_close > window_close) {
                        (*ctx.windows)[j].evicted = true;
                        (*ctx.windows)[j].first = nullptr;
                        (*ctx.windows)[j].last = nullptr;
                        (*ctx.windows)[j].latency = -1;
                        (*ctx.windows)[j].normalized_latency = -1;
                        ctx.windows->pop_back();
                    }
                }
            }
            if (window_close > (*ctx.windows)[ctx.windows->size() - 1].t_close && window_open > (*ctx.windows)[ctx.windows->size() - 1].t_open) {  // expand
                if ((*ctx.windows)[ctx.windows->size() - 1].t_close < window_close) {
                    // report results
                    (*ctx.windows)[ctx.windows->size() - 1].total_matched_results = ctx.sink->matched_paths;
                    // matched paths until this window
                    (*ctx.windows)[ctx.windows->size() - 1].emitted_results = ctx.sink->getResultSetSize();
                    // paths emitted on this window close
                    (*ctx.windows)[ctx.windows->size() - 1].window_matches = (*ctx.windows)[ctx.windows->size() - 1].emitted_results;
                }
                ctx.windows->emplace_back(window_open, window_close, nullptr, nullptr);
            }
        }

        o_i += ctx.slide;
    } while (o_i < time);

    sg_edge* new_sgt = ctx.sg->insert_edge(*ctx.edge_number, s, d, l, time, window_close);

    if (!new_sgt) {
        // search for the duplicate
        std::cerr << "ERROR: new sgt is null, time: " << time << std::endl;
        exit(1);
    }
    
    // Set output parameter
    *new_sgt_out = new_sgt;

    // add edge to time list
    timed_edge* t_edge = new timed_edge(new_sgt); // associate the timed edge with the snapshot graph edge
    ctx.sg->add_timed_edge(t_edge); // append the element to the time list

    // update window boundaries and check for window eviction
    for (size_t i = *ctx.window_offset; i < ctx.windows->size(); i++) {
        if ((*ctx.windows)[i].t_open <= time && time < (*ctx.windows)[i].t_close) {
            // active window
            if (!(*ctx.windows)[i].first || time < (*ctx.windows)[i].first->edge_pt->timestamp) {
                if (!(*ctx.windows)[i].first) (*ctx.windows)[i].last = t_edge;
                (*ctx.windows)[i].first = t_edge;
                (*ctx.windows)[i].elements_count++;
                (*ctx.total_elements_count)++;
            } else if (!(*ctx.windows)[i].last || time >= (*ctx.windows)[i].last->edge_pt->timestamp) {
                (*ctx.windows)[i].last = t_edge;
                (*ctx.windows)[i].elements_count++;
                (*ctx.total_elements_count)++;
            }
            if (ctx.sg->out_degree[s] > (*ctx.windows)[i].max_degree) {
                (*ctx.windows)[i].max_degree = ctx.sg->out_degree[s];
            }
        } else if (time >= (*ctx.windows)[i].t_close) {
            // schedule window for eviction
            *ctx.window_offset = i + 1;
            ctx.to_evict->push_back(i);
            *ctx.evict = true;
            if ((*ctx.windows)[i].elements_count == 0) {
                std::cerr << "ERROR: Empty window: " << i << std::endl;
                exit(1);
            }
        }
    }
    new_sgt->time_pos = t_edge; // associate the snapshot graph edge with the timed edge

    *ctx.cumulative_size += ctx.size;
    (*ctx.size_count)++;
    *ctx.avg_size = *ctx.cumulative_size / *ctx.size_count;

    *ctx.cumulative_degree += ctx.sg->out_degree[new_sgt->s];
    (*ctx.window_cardinality)++;
    *ctx.avg_deg = *ctx.cumulative_degree / *ctx.window_cardinality;

    /* EVICT */
    if (*ctx.evict) {
        // to compute window cost, we take the size of the snapshot graph of the window here, since no more elements will be added and it can be considered complete and closed
        std::vector<std::pair<long long, long long> > candidate_for_deletion;
        timed_edge *evict_start_point = (*ctx.windows)[(*ctx.to_evict)[0]].first;
        timed_edge *evict_end_point = (*ctx.windows)[ctx.to_evict->back() + 1].first;
        long long to_evict_timestamp = (*ctx.windows)[ctx.to_evict->back() + 1].t_open;

        if (!evict_start_point) {
            std::cerr << "ERROR: Evict start point is null." << std::endl;
            exit(1);
        }

        if (evict_end_point == nullptr) {
            evict_end_point = ctx.sg->time_list_tail;
            std::cout << "WARNING: Evict end point is null, evicting whole buffer." << std::endl;
        }

        // if (*ctx.last_t_open != (*ctx.windows)[(*ctx.to_evict)[0]].t_open) ctx.sink->refresh_resultSet((*ctx.windows)[(*ctx.to_evict)[0]].t_open);
        *ctx.last_t_open = (*ctx.windows)[(*ctx.to_evict)[0]].t_open;

        timed_edge *current = evict_start_point;

        while (current && current != evict_end_point) {
            auto cur_edge = current->edge_pt;
            auto next = current->next;

            *ctx.cumulative_degree -= ctx.sg->out_degree[cur_edge->s];
            (*ctx.window_cardinality)--;

            candidate_for_deletion.emplace_back(cur_edge->s, cur_edge->d);
            // schedule for deletion from RPQ forest
            //if (to_evict_timestamp >= cur_edge->timestamp) {
            ctx.sg->remove_edge(cur_edge->s, cur_edge->d, cur_edge->label); // delete from adjacency list
            //}
            ctx.sg->delete_timed_edge(current); // delete from time list

            current = next;
        }

        // reset time list pointers
        ctx.sg->time_list_head = evict_end_point;
        ctx.sg->time_list_head->prev = nullptr;

        ctx.f->expire_timestamped(to_evict_timestamp, candidate_for_deletion);

        // mark window as evicted
        for (unsigned long i: *ctx.to_evict) {
            (*ctx.windows)[i].evicted = true;
            (*ctx.windows)[i].first = nullptr;
            (*ctx.windows)[i].last = nullptr;
            (*ctx.windows)[i].latency = static_cast<double>(clock() - (*ctx.windows)[i].start_time) / CLOCKS_PER_SEC;
            if ((*ctx.windows)[i].latency > *ctx.lat_max) *ctx.lat_max = (*ctx.windows)[i].latency;
            if ((*ctx.windows)[i].latency < *ctx.lat_min) *ctx.lat_min = (*ctx.windows)[i].latency;
            (*ctx.windows)[i].normalized_latency = ((*ctx.windows)[i].latency - *ctx.lat_min) / (*ctx.lat_max - *ctx.lat_min);
            *ctx.cumulative_window_latency += (*ctx.windows)[i].latency;
        }

        (*ctx.warmup)++;
        ctx.to_evict->clear();
        candidate_for_deletion.clear();
        *ctx.evict = false;

        if (ctx.mode >= 11 && ctx.mode <= 15 && *ctx.warmup > 10) {
            // max degree computation
            *ctx.max_deg = 1;
            for (size_t i = *ctx.window_offset; i < ctx.windows->size(); i++) {
                if ((*ctx.windows)[i].max_degree > *ctx.max_deg) *ctx.max_deg = (*ctx.windows)[i].max_degree;
            }

            *ctx.avg_deg = *ctx.cumulative_degree / *ctx.window_cardinality;

            // cost function
            double n = 0;
            for (int i = 0; i < ctx.sg->EINIT_count; i++) {
                n += ctx.sg->edge_num - i;
            }

            if (n==0) cout << "WARNING: n is 0." << std::endl;

            // if (ctx.mode == 11) {
            //     n = (ctx.sg->EINIT_count+1)*(2*ctx.sg->edge_num - ctx.sg->EINIT_count) / 2.0;
            // }

            switch (ctx.mode) {
                case 11:
                    *ctx.cost = n / *ctx.max_deg;
                    break;
                case 12:
                    *ctx.cost = *ctx.avg_deg;
                    break;
                case 13:
                    *ctx.cost = n;
                    break;
                case 14:
                    *ctx.cost = *ctx.max_deg;
                    break;
                case 15:
                    *ctx.cost = ctx.sg->EINIT_count * ctx.sg->edge_num;
                    break;
                default:
                    cerr << "ERROR: unknown cost mode." << endl;
                    exit(1);
            }

            (*ctx.csv_memory) << n / *ctx.max_deg << "," << *ctx.avg_deg << "," << n << "," << *ctx.max_deg << "," << ctx.sg->EINIT_count * ctx.sg->edge_num << std::endl;

            if (*ctx.cost > *ctx.cost_max) *ctx.cost_max = *ctx.cost;
            if (*ctx.cost < *ctx.cost_min) *ctx.cost_min = *ctx.cost;
            *ctx.cost_norm = (*ctx.cost - *ctx.cost_min) / (*ctx.cost_max - *ctx.cost_min);

            ctx.cost_window->push_back(*ctx.cost_norm);
            if (ctx.cost_window->size() > ctx.overlap)
                ctx.cost_window->pop_front();

            *ctx.cost_norm = std::accumulate(ctx.cost_window->begin(), ctx.cost_window->end(), 0.0) / ctx.cost_window->size();

            double cost_diff = *ctx.cost_norm - *ctx.last_cost;
            *ctx.last_cost = *ctx.cost_norm;

            if (std::isnan(*ctx.last_diff)) {
                *ctx.last_diff = 0;
            }
            if (std::isnan(cost_diff)) {
                cost_diff = 0;
            }

            if (cost_diff >= 0.01 || *ctx.cost_norm >= 0.95) {
                ctx.size -= ceil(cost_diff * 10) * ctx.slide;
            } else if (cost_diff <= 0.01 || *ctx.cost_norm <= 0.05) {
                ctx.size += ceil(-cost_diff * 10) * ctx.slide;
            }

            // cap to max and min size
            ctx.size = std::max(std::min(ctx.size, ctx.max_size), ctx.min_size);
            accumulator = 0;
        } else accumulator++;

        (*ctx.windows)[(*ctx.window_offset)].cost = *ctx.cost_norm;

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

    (*ctx.csv_tuples)
        << ctx.windows->size() << ","
        << ctx.beta_id << ","
        << time << ","
        << *ctx.cost << ","
        << *ctx.cost_norm << ","
        << (*ctx.windows)[*ctx.window_offset >= 1 ? *ctx.window_offset - 1 : 0].latency << ","
        << ctx.beta_latency_end << ","
        << (*ctx.windows)[*ctx.window_offset >= 1 ? *ctx.window_offset - 1 : 0].elements_count << ","
        << (*ctx.windows)[*ctx.window_offset >= 1 ? *ctx.window_offset - 1 : 0].t_close - (*ctx.windows)[*ctx.window_offset >= 1 ? *ctx.window_offset - 1 : 0].t_open << std::endl << ","
        << 0 << std::endl;

    return true;
}
