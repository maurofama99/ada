#ifndef LOAD_SHEDDING_MODE_H
#define LOAD_SHEDDING_MODE_H

#include "mode_handler_base.h"
#include <random>

class LoadSheddingMode : public ModeHandlerBase {
    std::mt19937* gen;
    std::uniform_real_distribution<double>* dist;

public:
    LoadSheddingMode(std::mt19937* generator, std::uniform_real_distribution<double>* distribution) : gen(generator), dist(distribution) {
        if (gen) {
            gen->seed(123456u);
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
