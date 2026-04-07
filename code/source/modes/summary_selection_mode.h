#ifndef ADA_SUMMARY_SELECTION_MODE_H
#define ADA_SUMMARY_SELECTION_MODE_H

#include "mode_handler_base.h"
#include <random>

class SummarySelectionMode : public ModeHandlerBase {

    int budget;
    std::mt19937 gen{std::random_device{}()};

public:
    explicit SummarySelectionMode(const int budget_) {
        budget = budget_;
    }
    ~SummarySelectionMode() override = default;

    bool process_edge(
        long long s,
        long long d,
        long long l,
        long long time,
        ModeContext& ctx,
        sg_edge** new_sgt_out
    ) override;
};

#endif //ADA_SUMMARY_SELECTION_MODE_H
