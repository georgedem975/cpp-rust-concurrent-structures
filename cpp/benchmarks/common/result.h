#ifndef CPP_BENCHMARKS_COMMON_RESULT_H_
#define CPP_BENCHMARKS_COMMON_RESULT_H_

#include <variant>

#include "latency.h"
#include "point.h"
#include "run.h"

namespace bench {
    struct ThroughputResult {
        Point point;
        RunMetrics run;
        double operationsPerSecond;
    };

    struct LatencyResult {
        Point point;
        RunMetrics run;
        Latency latency;
    };

    using BenchmarkResult = std::variant<ThroughputResult, LatencyResult>;
}

#endif