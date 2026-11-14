#ifndef CPP_BENCHMARKS_COMMON_CONFIG_H_
#define CPP_BENCHMARKS_COMMON_CONFIG_H_

#include <string>
#include <vector>

namespace bench {
    inline constexpr int kDefaultRepeats = 10;
    inline constexpr int kDefaultBatch = 100;
    inline constexpr double kDefaultDurationSeconds = 1.0;
    inline constexpr double kDefaultWarmupSeconds = 1.0;

    struct MetricsConfig {
        bool throughput = false;
        bool latency = false;
    };

    struct ExperimentConfig {
        int repeats = kDefaultRepeats;
        std::vector<int> threads { 2, 4, 8, 16, 32 };
    };

    struct ExecutionConfig {
        double duration = kDefaultDurationSeconds;
        double warmup = kDefaultWarmupSeconds;
        int batch = kDefaultBatch;
    };

    struct OutputConfig {
        std::string path;
    };

    struct Options {
        ExperimentConfig experiment;
        ExecutionConfig execution;
        MetricsConfig metrics;
        OutputConfig output;
    };

    Options parseOptions(int argc, char** argv);
}

#endif