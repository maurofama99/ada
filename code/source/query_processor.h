#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "fsa.h"
#include "sink.h"
#include "streaming_graph.h"
#include "S-PATH.h"
#include "LM-SRPQ.h"

// ---------------------------------------------------------------------------
// IQueryProcessor  —  common interface for all path-query algorithms
// ---------------------------------------------------------------------------
class IQueryProcessor {
public:
    virtual ~IQueryProcessor() = default;

    // Process one incoming edge; returns true if a new result was produced.
    virtual bool insert_edge(long long s, long long d, long long label,
                             long long timestamp) = 0;

    // Expire nodes/results older than eviction_time.
    // deleted_edges contains the edges removed from the streaming graph in
    // this expiration cycle; implementations may use them to prune forests.
    virtual void expire_forest(
        long long eviction_time,
        const std::vector<streaming_graph::expired_edge_info>& deleted_edges) = 0;

    // Handle mid-window edge deletion (load shedding). Unlike expire_forest,
    // this finds tree nodes whose parent link used a specific deleted edge and
    // tries to reconnect them via alternative paths still in the graph.
    virtual void shed_edges(
        const std::vector<streaming_graph::expired_edge_info>& deleted_edges) = 0;
};

// ---------------------------------------------------------------------------
// SPathProcessor  —  wraps S_PATH
// ---------------------------------------------------------------------------
class SPathProcessor final : public IQueryProcessor {
    S_PATH impl_;

public:
    SPathProcessor(FiniteStateAutomaton& fsa, streaming_graph& sg, Sink& sink)
        : impl_(fsa, sg, sink) {}

    bool insert_edge(long long s, long long d, long long label,
                     long long timestamp) override {
        return impl_.insert_edge(
            static_cast<unsigned int>(s),
            static_cast<unsigned int>(d),
            static_cast<unsigned int>(label),
            static_cast<long long>(timestamp));
    }

    void expire_forest(
        long long eviction_time,
        const std::vector<streaming_graph::expired_edge_info>& deleted_edges) override {
        impl_.expire_forest(eviction_time, deleted_edges);
    }

    void shed_edges(
        const std::vector<streaming_graph::expired_edge_info>& deleted_edges) override {
        impl_.shed_edges(deleted_edges);
    }
};

// ---------------------------------------------------------------------------
// LmSrpqProcessor  —  wraps LM_SRPQ
//
// dynamic_lm_select is called inside expire_forest so callers remain unaware
// of the landmark-maintenance step that is specific to this algorithm.
// ---------------------------------------------------------------------------
class LmSrpqProcessor final : public IQueryProcessor {
    LM_SRPQ impl_;
    double candidate_rate_;
    double benefit_threshold_;

public:
    LmSrpqProcessor(FiniteStateAutomaton& fsa, streaming_graph& sg, Sink& sink,
                    double candidate_rate   = 0.2,
                    double benefit_threshold = 1.5)
        : impl_(fsa, sg, sink)
        , candidate_rate_(candidate_rate)
        , benefit_threshold_(benefit_threshold) {}

    bool insert_edge(long long s, long long d, long long label,
                     long long timestamp) override {
        impl_.insert_edge(
            static_cast<unsigned int>(s),
            static_cast<unsigned int>(d),
            static_cast<unsigned int>(label),
            static_cast<long long>(timestamp));
        // LM_SRPQ::insert_edge does not return a bool in the original code;
        // return false to satisfy the interface (results live in Sink).
        return false;
    }

    void expire_forest(
        long long eviction_time,
        const std::vector<streaming_graph::expired_edge_info>& deleted_edges) override {
        impl_.expire_forest(eviction_time, deleted_edges);
        impl_.dynamic_lm_select(candidate_rate_, benefit_threshold_);
    }

    void shed_edges(
        const std::vector<streaming_graph::expired_edge_info>& deleted_edges) override {
        // LM-SRPQ does not yet have a dedicated shed_edges; fall back to expire_forest
        // with a max eviction_time so the timestamp check is effectively disabled.
        impl_.expire_forest(0, deleted_edges);
    }
};

// ---------------------------------------------------------------------------
// QueryProcessorFactory  —  maps algorithm IDs to concrete processors
//
// Algorithm registry:
//   1  →  S-PATH
//   2  →  LM-SRPQ
// ---------------------------------------------------------------------------
class QueryProcessorFactory {
public:
    // Supported algorithm IDs — use these constants instead of raw integers.
    static constexpr int ALGO_S_PATH  = 1;
    static constexpr int ALGO_LM_SRPQ = 2;

    struct LmSrpqConfig {
        double candidate_rate    = 0.2;
        double benefit_threshold = 1.5;
    };

    static std::unique_ptr<IQueryProcessor> create(
        int algorithm,
        FiniteStateAutomaton& fsa,
        streaming_graph& sg,
        Sink& sink,
        LmSrpqConfig lm_cfg = {0.2, 1.5})
    {
        switch (algorithm) {
            case ALGO_S_PATH:
                return std::make_unique<SPathProcessor>(fsa, sg, sink);

            case ALGO_LM_SRPQ:
                return std::make_unique<LmSrpqProcessor>(
                    fsa, sg, sink,
                    lm_cfg.candidate_rate,
                    lm_cfg.benefit_threshold);

            default:
                throw std::invalid_argument(
                    "QueryProcessorFactory: unknown algorithm id " +
                    std::to_string(algorithm));
        }
    }

private:
    QueryProcessorFactory() = delete; // pure static utility — not instantiable
};
