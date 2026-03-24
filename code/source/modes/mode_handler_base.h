#ifndef ADA_MODE_HANDLER_BASE_H
#define ADA_MODE_HANDLER_BASE_H

#include "mode_handler.h"


class ModeHandlerBase : public ModeHandler {

protected:

    static double compute_load_estimation (ModeContext &ctx, int mode);

    static long long compute_window_boundaries (ModeContext &ctx, long long time);

    static bool update_window (ModeContext &ctx, sg_edge* new_sgt, long long time, long long s);

    static std::vector<streaming_graph::expired_edge_info> evict (ModeContext &ctx, long long time);

    static void mark_windows_evicted(ModeContext &ctx);

};

#endif //ADA_MODE_HANDLER_BASE_H