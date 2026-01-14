#include "adwin_mode.h"
#include <iostream>

bool AdwinMode::process_edge(long long s, long long d, long long l, long long time, ModeContext& ctx, sg_edge** new_sgt_out) {
    (*ctx.edge_number)++;
    if (l == ctx.first_transition) (*ctx.EINIT_count)++;

    (*ctx.window_cardinality)++;
    (*ctx.windows)[*ctx.resizings].elements_count++;
    (*ctx.warmup)++;

    sg_edge* new_sgt = ctx.sg->insert_edge(*ctx.edge_number, s, d, l, time, time);

    if (!new_sgt) {
        std::cerr << "ERROR: new sgt is null, time: " << time << std::endl;
        exit(1);
    }
    
    // Set output parameter
    *new_sgt_out = new_sgt;

    // do W ← W ∪ {xt} (i.e., add xt to the head of W )
    timed_edge* t_edge = new timed_edge(new_sgt);
    ctx.sg->add_timed_edge(t_edge);
    new_sgt->time_pos = t_edge;

    // update max degree observed
    if (ctx.sg->density[new_sgt->s] > *ctx.max_deg) *ctx.max_deg = ctx.sg->density[new_sgt->s];

    // compute average out degree centrality incrementally
    *ctx.cumulative_degree += ctx.sg->density[new_sgt->s];
    *ctx.avg_deg = *ctx.cumulative_degree / *ctx.window_cardinality;

    // compute cost function
    double n = 0;
    for (int i = 0; i < *ctx.EINIT_count; i++) {
        n += ctx.sg->edge_num - i;
    }
    *ctx.cost = n / *ctx.max_deg;
    if (*ctx.cost > *ctx.cost_max) *ctx.cost_max = *ctx.cost;
    if (*ctx.cost < *ctx.cost_min) *ctx.cost_min = *ctx.cost;
    *ctx.cost_norm = (*ctx.cost - *ctx.cost_min) / (*ctx.cost_max - *ctx.cost_min);

    if (*ctx.warmup > 2700) (*ctx.csv_adwin_distribution) << *ctx.avg_deg << "," << *ctx.cost << "," << *ctx.cost_norm << "\n";

    // if (adwin.update(cost)) {
    if (adwin->update(*ctx.cost_norm*10) && *ctx.warmup > 2700) {
        std::cout << "\n>>> DRIFT DETECTED " << std::endl;
        std::cout << "    Current estimation: " << adwin->getEstimation() << std::endl;
        std::cout << "    Window length: " << adwin->length() << std::endl;
        (*ctx.windows)[*ctx.resizings].t_close = time;
        (*ctx.windows)[*ctx.resizings].latency = static_cast<double>(clock() - (*ctx.windows)[*ctx.resizings].start_time) / CLOCKS_PER_SEC;
        (*ctx.windows)[*ctx.resizings].total_matched_results = ctx.sink->matched_paths;
        (*ctx.windows)[*ctx.resizings].emitted_results = ctx.sink->getResultSetSize();
        (*ctx.resizings)++;
        ctx.windows->emplace_back(time, time, nullptr, nullptr);
        timed_edge *current = ctx.sg->time_list_head;
        std::vector<std::pair<long long, long long> > candidate_for_deletion;

        while (current && *ctx.window_cardinality > adwin->length()) {
            auto cur_edge = current->edge_pt;
            auto next = current->next;

            if (cur_edge->label == ctx.first_transition) (*ctx.EINIT_count)--;
            *ctx.cumulative_degree -= ctx.sg->density[cur_edge->s];

            candidate_for_deletion.emplace_back(cur_edge->s, cur_edge->d);

            ctx.sg->remove_edge(cur_edge->s, cur_edge->d, cur_edge->label);

            ctx.sg->delete_timed_edge(current);

            (*ctx.window_cardinality)--;

            ctx.sg->time_list_head = next;
            ctx.sg->time_list_head->prev = nullptr;

            current = next;
        }

        *ctx.max_deg = 1;
        while (current) {
            auto cur_edge = current->edge_pt;
            auto next = current->next;
            if (ctx.sg->density[cur_edge->s] > *ctx.max_deg) *ctx.max_deg = ctx.sg->density[cur_edge->s];
            current = next;
        }

        ctx.sink->refresh_resultSet(ctx.sg->time_list_head->edge_pt->timestamp);
        ctx.f->expire_timestamped(ctx.sg->time_list_head->edge_pt->timestamp, candidate_for_deletion);
        candidate_for_deletion.clear();
    }
    (*ctx.csv_tuples)
        << *ctx.cost << ","
        << *ctx.cost_norm << ","
        << (*ctx.windows)[*ctx.resizings].latency << ","
        << 0 << ","
        << (*ctx.windows)[*ctx.resizings].elements_count << ","
        << (*ctx.windows)[*ctx.resizings].t_close - (*ctx.windows)[*ctx.resizings].t_open << std::endl;
    
    return true;
}
