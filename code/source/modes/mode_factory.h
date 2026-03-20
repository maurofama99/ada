#ifndef MODE_FACTORY_H
#define MODE_FACTORY_H

#include "mode_handler.h"
#include "sliding_window_mode.h"
#include "adwin_mode.h"
#include "load_shedding_mode.h"
#include <string>
#include <memory>

class ModeFactory {
public:
    static std::unique_ptr<ModeHandler> create_mode_handler(const int mode, const double delta, const size_t labels_size) {
        if (mode >= 10) {
            return std::make_unique<SlidingWindowMode>();
        }
        if (mode == 2) {
            return std::make_unique<AdwinMode>(delta);
        }
        if (mode == 3 || mode == 4 || mode == 5) {
            return std::make_unique<LoadSheddingMode>(labels_size);
        }
        std::cerr << "unknown mode" << std::endl;
        exit(4);
    }
};

#endif // MODE_FACTORY_H
