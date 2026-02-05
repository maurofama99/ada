#ifndef MODE_FACTORY_H
#define MODE_FACTORY_H

#include "mode_handler.h"
#include "sliding_window_mode.h"
#include "adwin_mode.h"
#include "load_shedding_mode.h"
#include <string>
#include <memory>
#include <random>

class ModeFactory {
public:
    static std::unique_ptr<ModeHandler> create_mode_handler(
        const int mode,
        Adwin* adwin_instance = nullptr,
        std::mt19937* generator = nullptr,
        std::uniform_real_distribution<double>* distribution = nullptr
    ) {
        if (mode >= 10) {
            return std::make_unique<SlidingWindowMode>(adwin_instance);
        }
        if (mode == 2) {
            if (!adwin_instance) {
                std::cerr << "ERROR: ADWIN mode requires adwin instance" << std::endl;
                exit(4);
            }
            return std::make_unique<AdwinMode>(adwin_instance);
        }
        if (mode == 3 || mode == 4) {
            if (!generator || !distribution) {
                std::cerr << "ERROR: Load shedding mode requires generator and distribution" << std::endl;
                exit(4);
            }
            return std::make_unique<LoadSheddingMode>(generator, distribution);
        }
        std::cerr << "unknown mode" << std::endl;
        exit(4);
    }
};

#endif // MODE_FACTORY_H
