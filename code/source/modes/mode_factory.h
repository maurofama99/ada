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
        const std::string& mode,
        Adwin* adwin_instance = nullptr,
        std::mt19937* generator = nullptr,
        std::uniform_real_distribution<double>* distribution = nullptr
    ) {
        if (mode == "sl" || mode == "ad") {
            return std::make_unique<SlidingWindowMode>();
        } else if (mode == "adwin") {
            if (!adwin_instance) {
                std::cerr << "ERROR: ADWIN mode requires adwin instance" << std::endl;
                exit(4);
            }
            return std::make_unique<AdwinMode>(adwin_instance);
        } else if (mode == "lshed") {
            if (!generator || !distribution) {
                std::cerr << "ERROR: Load shedding mode requires generator and distribution" << std::endl;
                exit(4);
            }
            return std::make_unique<LoadSheddingMode>(generator, distribution);
        } else {
            std::cerr << "unknown mode" << std::endl;
            exit(4);
        }
    }
};

#endif // MODE_FACTORY_H
