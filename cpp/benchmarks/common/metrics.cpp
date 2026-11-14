#include "metrics.h"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "clock.h"
#include "latency.h"

namespace bench {
    BenchmarkResult makeThroughputResult(const Point& point, const RunMetrics& run) {
        const double wallSeconds = run.elapsed.durationNs() / kNanosecondsPerSecond;

        if (wallSeconds <= 0.0) {
            throw std::runtime_error("Benchmark duration must be positive");
        }

        return ThroughputResult {
            .point = point,
            .run = run,
            .operationsPerSecond = static_cast<double>(run.totalOperations) / wallSeconds
        };
    }

    BenchmarkResult makeLatencyResult(const Point& point, 
                                      const RunMetrics& run,
                                      std::vector<double> operationSamplesNs) {
        return LatencyResult {
            .point = point,
            .run = run,
            .latency = Latency::fromSamples(std::move(operationSamplesNs))
        };
    }

    std::vector<double> perOperationSamples(const RunSamples& samples, int batch) {
        if (batch <= 0) {
            throw std::invalid_argument("Batch size must be positive");
        }

        std::size_t count = 0;

        for (const auto& threadSamples : samples.batchDurationsNs) {
            count += threadSamples.size();
        }

        std::vector<double> operationSamples;
        operationSamples.reserve(count);

        for (const auto& threadSamples : samples.batchDurationsNs) {
            for (const double batchDurationNs : threadSamples) {
                operationSamples.push_back(batchDurationNs / static_cast<double>(batch));
            }
        }

        return operationSamples;
    }
}