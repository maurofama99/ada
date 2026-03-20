#ifndef LOAD_SHEDDING_MODE_H
#define LOAD_SHEDDING_MODE_H

#include "mode_handler_base.h"
#include "../ranking/buckets.h"
#include <random>
#include <vector>

class LoadSheddingMode : public ModeHandlerBase {
    std::mt19937 gen = std::mt19937(std::random_device{}());
    std::uniform_real_distribution<> dist = std::uniform_real_distribution<>(0.0, 1.0);
    std::vector<RankBuckets> ranks;
    std::vector<int> types_counts;
    double N_in = INT32_MAX;

public:
    explicit LoadSheddingMode(const size_t label_size) {
        gen.seed(123456u);
        for (size_t i = 0; i <= label_size; ++i) {
            ranks.emplace_back(100);
            types_counts.emplace_back(0);
        }
    }
    ~LoadSheddingMode() override = default;
    
    bool process_edge(
        long long s,
        long long d,
        long long l,
        long long time,
        ModeContext& ctx,
        sg_edge** new_sgt_out
    ) override;
};

#endif // LOAD_SHEDDING_MODE_H
