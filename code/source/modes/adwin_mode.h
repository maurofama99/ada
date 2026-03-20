#ifndef ADWIN_MODE_H
#define ADWIN_MODE_H

#include "mode_handler_base.h"
#include "../adwin/Adwin.h"

class AdwinMode : public ModeHandlerBase {
    Adwin* adwin;
    
public:
    explicit AdwinMode(const double delta) {
        adwin = new Adwin(5, 1, delta);
    }
    ~AdwinMode() override = default;
    
    bool process_edge(
        long long s,
        long long d,
        long long l,
        long long time,
        ModeContext& ctx,
        sg_edge** new_sgt_out
    ) override;
};

#endif // ADWIN_MODE_H
