#include "summary_selection_mode.h"

#include "mode_handler.h"
#include "code/source/streaming_graph.h"

bool SummarySelectionMode::process_edge(const long long s, const long long d, const long long l, const long long time, ModeContext& ctx, sg_edge** new_sgt_out) {

    const long long window_close = compute_window_boundaries(ctx, time);

    if (ctx.sg->edge_num > budget) {
        std::vector<streaming_graph::expired_edge_info> deleted_edges;
        sg_edge* cur_edge;
        switch (ctx.mode) {
            case 60: // random
                cur_edge = ctx.sg->get_random_edge(gen);
                break;
            case 61: // fifo
                cur_edge = ctx.sg->time_list_head->edge_pt;
                break;
            case 62:
                cur_edge = ctx.sg->get_random_edge(gen);
                break;
            default:
                std::cerr << "ERROR: Unknown summary selection mode." << std::endl;
                exit(1);
        }
        deleted_edges.push_back({cur_edge->s, cur_edge->d, cur_edge->label, cur_edge->id});
        ctx.sg->delete_timed_edge(cur_edge->time_pos);
        ctx.sg->remove_edge(cur_edge->s, cur_edge->d, cur_edge->label, time);
        ctx.window_cardinality -= deleted_edges.size();
        ctx.q->shed_edges(deleted_edges);
    }

    // TODO, idee per risolvere overhead: aggiorna i counter ogni beta (o ad un rateo fissato)

    ctx.edge_number++;
    sg_edge* new_sgt = ctx.sg->insert_edge(ctx.edge_number, s, d, l, time, window_close);

    if (!new_sgt) {
        // search for the duplicate
        std::cerr << "ERROR: new sgt is null, time: " << time << std::endl;
        exit(1);
    }

    *new_sgt_out = new_sgt;

    /* EVICT */
    if (update_window(ctx, new_sgt, time, s)) {
        evict(ctx);

        // mark window as evicted
        mark_windows_evicted(ctx);
    }

    (*ctx.csv_tuples)
        << ctx.windows.size() << ","
        << ctx.beta_id << ","
        << time << ","
        << ctx.cost << ","
        << ctx.cost_norm << ","
        << (ctx.windows)[ctx.window_offset >= 1 ? ctx.window_offset - 1 : 0].latency << ","
        << ctx.beta_latency_end << ","
        << (ctx.windows)[ctx.window_offset >= 1 ? ctx.window_offset - 1 : 0].elements_count << ","
        << (ctx.windows)[ctx.window_offset >= 1 ? ctx.window_offset - 1 : 0].t_close - (ctx.windows)[ctx.window_offset >= 1 ? ctx.window_offset - 1 : 0].t_open << ","
        << 0 << std::endl;

    return false;
}
