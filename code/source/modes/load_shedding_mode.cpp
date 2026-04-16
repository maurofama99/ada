#include "load_shedding_mode.h"
#include <cmath>
#include <cassert>
#include <iostream>

using namespace std;

bool LoadSheddingMode::process_edge(long long s, long long d, long long l, long long time, ModeContext& ctx, sg_edge** new_sgt_out) {
    bool is_shedding = false;
    long long window_close = compute_window_boundaries(ctx, time);

    bool shedding_condition = false;
    switch (ctx.mode) {
        case 3:
            shedding_condition = (dist)(gen) < ctx.p_shed;
            break;
        case 4:
            shedding_condition = (dist)(gen) < ctx.max_shed;
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
        return is_shedding; // continue to next edge
    }
    
    (ctx.edge_number)++;
    sg_edge* new_sgt = ctx.sg->insert_edge(ctx.edge_number, s, d, l, time, window_close);

    if (!new_sgt) {
        std::cerr << "ERROR: new sgt is null, time: " << time << std::endl;
        exit(1);
    }

    *new_sgt_out = new_sgt;

    unsigned int new_sgt_id = new_sgt->id;

    bool evict_condition = update_window(ctx, new_sgt, time, s);

    if (ctx.mode == 5) {
        const int src_out = ctx.sg->out_degree.count(s) ? ctx.sg->out_degree.at(s) : 0;
        const int dst_in = ctx.sg->in_degree.count(d) ? ctx.sg->in_degree.at(d) : 0;
        assert(src_out >= 0 && dst_in >= 0);
        // FIXME: This is wrong because is non-local
        const double score = 100 * ((src_out + dst_in) / (src_out + dst_in + (ctx.sg->edge_num/ctx.sg->vertex_num)));
        if (score > 100 || score < 0) cerr << "score out of bounds" << endl;
        ranks[l].set_rank(new_sgt->id, ceil(score));
        types_counts[l]++;

        if (ctx.average_processing_time > 0.0) N_in = ctx.latency_max / ctx.average_processing_time;
        if (ctx.window_cardinality > N_in) {
            //cout << "shedding: window cardinality " << ctx.window_cardinality << ", N_in : " << N_in << ", average processing time: " << ctx.average_processing_time << endl;
            // print all the type counts
            for (size_t i = 0; i < types_counts.size(); ++i) {
                //cout << "type " << i << ": " << types_counts[i] << " edges" << endl;
            }

            is_shedding = true;

            double total = 0.0;
            for (int i = 0; i < types_counts.size(); i++) { // compute Z_t and R_t for each type
                if (types_counts[i] > 0) {
                    const double Z_t = ctx.cumulative_processing_time_type[i] / ctx.processed_elements_type[i];
                    const double R_t = ctx.input_rate_type[i];
                    total += Z_t * R_t;
                }
            }
            for (int i = 0; i < types_counts.size(); i++) { // compute Z_t and R_t for each type
                if (types_counts[i] > 0) {
                    std::vector<std::int64_t> bottom_k_edges;
                    std::vector<streaming_graph::expired_edge_info> deleted_edges;
                    const double Z_t = ctx.cumulative_processing_time_type[i] / ctx.processed_elements_type[i];
                    const double R_t = ctx.input_rate_type[i];
                    const double N_t = ceil(Z_t * R_t / total * N_in * 0.8);
                    //cout << "shedding: N_T[" << i << "] = " << N_t << ", type counts: " << types_counts[i] << endl;
                    if (N_t < types_counts[i]) bottom_k_edges = ranks[i].bottom_k(static_cast<size_t>(types_counts[i] - N_t));
                    else bottom_k_edges = ranks[i].bottom_k(static_cast<size_t>(types_counts[i]));
                    for (const auto& edge_id : bottom_k_edges) {
                        auto& cur_edge = ctx.sg->edge_id_to_edge[edge_id];
                        if (cur_edge == nullptr) {
                            std::cerr << "ERROR: Edge with ID " << edge_id << " not found for shedding." << std::endl;
                            continue; // Skip this edge and continue with the next one
                        }
                        assert(cur_edge->label == i);

                        if (cur_edge->id == new_sgt_id) *new_sgt_out = nullptr;

                        deleted_edges.push_back({cur_edge->s, cur_edge->d, cur_edge->label, cur_edge->id});
                        ranks[i].remove(cur_edge->id);
                        ctx.sg->delete_timed_edge(cur_edge->time_pos); // delete from time list
                        ctx.sg->remove_edge(cur_edge->s, cur_edge->d, cur_edge->label, time); // delete from adjacency list
                    }
                    //cout << "removed " << deleted_edges.size() << " edges" << endl;
                    ctx.window_cardinality -= deleted_edges.size();
                    types_counts[i] -= deleted_edges.size();
                    assert (types_counts[i] >= 0);
                    ctx.q->shed_edges(deleted_edges);
                }
            }
        }
    }

    if (evict_condition) {
        std::vector<streaming_graph::expired_edge_info> deleted_edges = evict(ctx, time);
        if (ctx.mode == 5) {
            for (auto& edge : deleted_edges) {
                types_counts[edge.label]--;
                assert (types_counts[edge.label] >= 0);
                ranks[edge.label].remove(edge.id);
            }
        }

        mark_windows_evicted(ctx);

        if (ctx.mode == 3) {
            double cost_diff = compute_load_estimation(ctx, 11);

            ctx.p_shed += cost_diff*10;

            double max_prob = (ctx.p_shed < ctx.max_shed ? ctx.p_shed : ctx.max_shed);
            ctx.p_shed = max_prob > 0.0 ? max_prob : 0.0;

            //cout << "p_shed: " << ctx.p_shed << ", cost diff: " << cost_diff << endl;
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

    return is_shedding;
}