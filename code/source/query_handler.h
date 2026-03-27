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
};