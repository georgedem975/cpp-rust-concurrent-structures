#ifndef CPP_BENCHMARKS_COMMON_RUN_H_
#define CPP_BENCHMARKS_COMMON_RUN_H_

#include <vector>

#include "clock.h"

namespace bench {
    struct RunMetrics {
        long long totalOperations = 0;
        TimeInterval elapsed;
    };

    struct RunSamples {
        std::vector<std::vector<double>> batchDurationsNs;
    };

    struct Run {
        RunMetrics metrics;
        RunSamples samples;
    };
}

#endif