#ifndef SLIDING_WINDOW_MODE_H
#define SLIDING_WINDOW_MODE_H

#include "mode_handler.h"

class SlidingWindowMode : public ModeHandler {
public:
    SlidingWindowMode() = default;
    ~SlidingWindowMode() override = default;
    
    bool process_edge(
        long long s,
        long long d,
        long long l,
        long long time,
        ModeContext& ctx,
        sg_edge** new_sgt_out
    ) override;
};

#endif // SLIDING_WINDOW_MODE_H
