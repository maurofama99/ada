#ifndef ADWIN_MODE_H
#define ADWIN_MODE_H

#include "mode_handler.h"
#include "../adwin/Adwin.h"

class AdwinMode : public ModeHandler {
private:
    Adwin* adwin;
    
public:
    explicit AdwinMode(Adwin* adwin_instance) : adwin(adwin_instance) {}
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
