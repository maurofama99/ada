#pragma once

#include "query_processor.h"   // IQueryProcessor + factory

// ---------------------------------------------------------------------------
// QueryHandler
//
// Owns a single IQueryProcessor selected at construction time via the factory.
// All routing logic lives in the factory and the processor implementations;
// this class is a pure thin wrapper that the rest of the application talks to.
// ---------------------------------------------------------------------------
class QueryHandler {
    std::unique_ptr<IQueryProcessor> processor_;

public:
    // Construct with an algorithm id and optional LM-SRPQ tuning parameters.
    // Throws std::invalid_argument for unknown algorithm ids.
    QueryHandler(FiniteStateAutomaton& fsa,
                 streaming_graph&       sg,
                 Sink&                  sink,
                 int                    algorithm,
                 QueryProcessorFactory::LmSrpqConfig lm_cfg = {})
        : processor_(QueryProcessorFactory::create(algorithm, fsa, sg, sink, lm_cfg))
    {}

    // Process one incoming edge.  Returns true if a new result was produced.
    bool run(const sg_edge* edge) const {
        return processor_->insert_edge(
            edge->s, edge->d, edge->label, edge->timestamp);
    }

    // Expire state after sliding the window.
    void update_state(
        long long eviction_time,
        const std::vector<streaming_graph::expired_edge_info>& deleted_edges) const
    {
        processor_->expire_forest(eviction_time, deleted_edges);
    }

    // Handle mid-window edge deletion (load shedding).
    void shed_edges(
        const std::vector<streaming_graph::expired_edge_info>& deleted_edges) const
    {
        processor_->shed_edges(deleted_edges);
    }

    // Snapshot export for ML counter projection training data.
    void dump_snapshot(std::ostream& vertex_csv, std::ostream& tree_csv,
                       std::ostream& forest_csv, int snapshot_id,
                       int window_id, long long current_time,
                       long long t_open, long long t_close,
                       int matched_paths) const {
        processor_->dump_snapshot(vertex_csv, tree_csv, forest_csv, snapshot_id,
                                  window_id, current_time, t_open, t_close, matched_paths);
    }

    int write_snapshot_headers(std::ostream& vertex_csv, std::ostream& tree_csv,
                                std::ostream& forest_csv) const {
        return processor_->write_snapshot_headers(vertex_csv, tree_csv, forest_csv);
    }
};