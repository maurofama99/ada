#ifndef MODE_FACTORY_H
#define MODE_FACTORY_H

#include "mode_handler.h"
#include "sliding_window_mode.h"
#include "adwin_mode.h"
#include "load_shedding_mode.h"
#include <string>
#include <memory>
#include "summary_selection_mode.h"

class ModeFactory {
public:
    static std::unique_ptr<ModeHandler> create_mode_handler(const int mode, const double delta, const size_t labels_size, const int budget) {
        if (mode >= 10 && mode < 20) {
            return std::make_unique<SlidingWindowMode>();
        }
        if (mode == 2) {
            return std::make_unique<AdwinMode>(delta);
        }
        if (mode == 3 || mode == 4 || mode == 5) {
            return std::make_unique<LoadSheddingMode>(labels_size);
        }
        if (mode >= 60 && mode < 70) {
            return std::make_unique<SummarySelectionMode>(budget);
        }
        std::cerr << "unknown mode" << std::endl;
        exit(4);
    }
};

#endif // MODE_FACTORY_H
