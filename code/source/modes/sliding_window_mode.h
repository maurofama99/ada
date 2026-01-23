#ifndef SLIDING_WINDOW_MODE_H
#define SLIDING_WINDOW_MODE_H

#include "mode_handler.h"
#include "../adwin/Adwin.h"

class SlidingWindowMode : public ModeHandler {
private:
    Adwin* adwin;
    double last_adwin_estimation = 0.0;

public:
    explicit SlidingWindowMode(Adwin* adwin_instance) : adwin(adwin_instance) {}
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
