#include "load_shedding_mode.h"
#include <cmath>
#include <algorithm>
#include <iostream>

using namespace std;

bool LoadSheddingMode::process_edge(long long s, long long d, long long l, long long time, ModeContext& ctx, sg_edge** new_sgt_out) {
    ctx.f->current_time = time;
    long long window_close = compute_window_boundaries(ctx, time);

    bool shedding_condition = false;
    switch (ctx.mode) {
        case 3:
            shedding_condition = (*dist)(*gen) < ctx.p_shed;
            break;
        case 4:
            shedding_condition = ctx.average_processing_time > ctx.latency_max;
            break;
        case 5:
            shedding_condition = false;
            break;
        default:
            std::cerr << "ERROR: Unknown load shedding mode." << std::endl;
            exit(1);
    }

    if (shedding_condition && (ctx.windows)[ctx.windows.size()-1].elements_count > 0) {
        *new_sgt_out = nullptr; // no edge created when load shedding
        return true; // continue to next edge
    }
    
    (ctx.edge_number)++;
    sg_edge* new_sgt = ctx.sg->insert_edge(ctx.edge_number, s, d, l, time, window_close);

    if (!new_sgt) {
        // search for the duplicate
        std::cerr << "ERROR: new sgt is null, time: " << time << std::endl;
        exit(1);
    }
    
    // Set output parameter
    *new_sgt_out = new_sgt;

    if (update_window(ctx, new_sgt, time, s)) {
        // to compute window cost, we take the size of the snapshot graph of the window here, since no more elements will be added and it can be considered complete and closed
        std::vector<std::pair<long long, long long> > candidate_for_deletion = evict(ctx, time);

        if (ctx.mode == 5 && ctx.beta_latency_end > ctx.latency_max) { // if measured latency exceeds threshold

            double reduce_percentage = 1 - ctx.latency_max / ctx.beta_latency_end; // the higher the latency, the more we reduce

            size_t edges_to_shed = static_cast<int>(ctx.window_cardinality * reduce_percentage); // number of edges to shed to reduce the latency by the reduce_percentage
            size_t edges_to_keep = ctx.window_cardinality - edges_to_shed;
            cout << "edges to shed: " << edges_to_shed << std::endl;
            cout << "edges to keep: " << edges_to_keep << std::endl;

            std::vector<long long> top_k_vertices = ctx.f->getTopKVertices(edges_to_keep); // (vertex, rank)
            std::vector<std::int64_t> bottom_k_edges = ctx.sg->getBottomKEdges(edges_to_shed); // (edge_id, rank)

            // retrieve the last rank in the topK vertices, i.e. the lowest rank in the top k
            double shedding_threshold = top_k_vertices.empty() ? 0 : ctx.f->computeVertexRank(top_k_vertices.back());
            cout << "shedding threshold: " << shedding_threshold << endl;
            cout << "higest rank: " << ctx.f->computeVertexRank(top_k_vertices.front()) << endl;

            cout << "window cardinality before: " << ctx.window_cardinality << endl;
            int counter = 0;
            int recent_counter = 0;
            for (const auto& edge_id : bottom_k_edges) {
                auto* cur_edge = ctx.sg->edge_id_to_edge[edge_id];
                if (cur_edge == nullptr) {
                    std::cerr << "ERROR: Edge with ID " << edge_id << " not found for shedding." << std::endl;
                    continue; // Skip this edge and continue with the next one
                }

                // if any of the two vertices of the edge has higher rank than the shedding threshold, do not shed
                if (ctx.f->computeVertexRank(cur_edge->s) > shedding_threshold || ctx.f->computeVertexRank(cur_edge->d) > shedding_threshold) {
                    //cout << "Edge " << edge_id << " not shed because vertex " << cur_edge->s << " has rank " << ctx.f->getVertexRank(cur_edge->s) << " and vertex " << cur_edge->d << " has rank " << ctx.f->getVertexRank(cur_edge->d) << ", shedding threshold is " << shedding_threshold << endl;
                    continue; // Skip this edge and continue with the next one
                }

                ctx.cumulative_degree -= ctx.sg->out_degree[cur_edge->s];
                (ctx.window_cardinality)--;

                if (cur_edge->timestamp >= (ctx.windows)[ctx.windows.size()-2].t_open) recent_counter++;
                else {
                    candidate_for_deletion.emplace_back(cur_edge->s, cur_edge->d); // schedule for deletion from RPQ forest
                    ctx.sg->delete_timed_edge(cur_edge->time_pos); // delete from time list
                    ctx.sg->remove_edge(cur_edge->s, cur_edge->d, cur_edge->label, time); // delete from adjacency list
                    counter++;
                }

            }
            cout << "shed edges: " << counter << endl;
            cout << "recent edges: " << recent_counter << endl;
        }

        ctx.f->expire_timestamped((ctx.windows)[ctx.to_evict.back() + 1].t_open, candidate_for_deletion);
        mark_windows_evicted(ctx);

        if (ctx.mode == 3) {
            double cost_diff = compute_load_estimation(ctx, 11);

            if (cost_diff > 0 || ctx.cost_norm >= 0.95) {
                ctx.p_shed += ceil(cost_diff * 10 * ctx.granularity);
            } else if (cost_diff < 0 || ctx.cost_norm <= 0.05) {
                ctx.p_shed -= ceil(cost_diff * 10 * ctx.granularity);
            }

            ctx.p_shed = std::max(std::min(ctx.p_shed, ctx.max_shed), 0.0);
        }
    }

    *(ctx.csv_tuples)
        << ctx.windows.size() << ","
        << ctx.beta_id << ","
        << time << ","
        << ctx.cost << ","
        << ctx.cost_norm << ","
        << (ctx.windows)[ctx.window_offset >= 1 ? ctx.window_offset - 1 : 0].latency << ","
        << ctx.beta_latency_end << ","
        << (ctx.windows)[ctx.window_offset >= 1 ? ctx.window_offset - 1 : 0].elements_count << ","
        << (ctx.windows)[ctx.window_offset >= 1 ? ctx.window_offset - 1 : 0].t_close - (ctx.windows)[ctx.window_offset >= 1 ? ctx.window_offset - 1 : 0].t_open << ","
        << ctx.p_shed << std::endl;

    return true;
}