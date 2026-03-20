#ifndef ADA_MODE_HANDLER_BASE_H
#define ADA_MODE_HANDLER_BASE_H

#include "mode_handler.h"


class ModeHandlerBase : public ModeHandler {

protected:

    double compute_load_estimation (ModeContext &ctx, int mode);

    long long compute_window_boundaries (ModeContext &ctx, long long time);

    bool update_window (ModeContext &ctx, sg_edge* new_sgt, long long time, long long s);

    std::vector<streaming_graph::expired_edge_info> evict (ModeContext &ctx, long long time);

    void mark_windows_evicted(ModeContext &ctx);

};

#endif //ADA_MODE_HANDLER_BASE_H