#ifndef CPP_BENCHMARKS_COMMON_METRICS_H_
#define CPP_BENCHMARKS_COMMON_METRICS_H_

#include <vector>

#include "result.h"

namespace bench {
    BenchmarkResult makeThroughputResult(const Point& point, const RunMetrics& run);

    BenchmarkResult makeLatencyResult(const Point& point, 
                                      const RunMetrics& run,
                                      std::vector<double> operationSamplesNs);

    std::vector<double> perOperationSamples(const RunSamples& samples, int batch);
}

#endif