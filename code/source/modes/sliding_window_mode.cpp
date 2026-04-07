#include "sliding_window_mode.h"
#include <cmath>
#include <numeric>

bool SlidingWindowMode::process_edge(long long s, long long d, long long l, long long time, ModeContext& ctx, sg_edge** new_sgt_out) {
    long long window_close = compute_window_boundaries(ctx, time);

    (ctx.edge_number)++;
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

        if (ctx.mode >= 11 && ctx.mode <= 15) {
            double cost_diff = compute_load_estimation(ctx, ctx.mode);

            if (cost_diff >= 0.01 || ctx.cost_norm >= 0.95) {
                ctx.size -= ceil(cost_diff * 10) * ctx.slide;
            } else if (cost_diff <= 0.01 || ctx.cost_norm <= 0.05) {
                ctx.size += ceil(-cost_diff * 10) * ctx.slide;
            }

            // cap to max and min size
            long long max_size = (ctx.size < ctx.max_size ? ctx.size : ctx.max_size);
            ctx.size = max_size > ctx.min_size ? max_size : ctx.min_size;
        }

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
