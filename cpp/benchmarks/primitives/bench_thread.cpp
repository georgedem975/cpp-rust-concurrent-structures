#include <cstddef>
#include <iterator>
#include <thread>
#include <utility>
#include <vector>

#include "clock.h"
#include "config.h"
#include "csvReporter.h"
#include "metrics.h"
#include "result.h"
#include "run.h"

namespace {
    constexpr const char* kPrimitive = "thread";
    constexpr const char* kScenario = "spawn_join";

    struct SpawnRun {
        std::vector<double> samples;
        bench::TimeInterval elapsed;
    };

    SpawnRun spawnAndJoin(int threadCount) {
        std::vector<double> spawnSamples(static_cast<std::size_t>(threadCount));

        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(threadCount));

        const auto start = bench::now();

        for (int i = 0; i < threadCount; ++i) {
            const auto spawnStart = bench::now();

            workers.emplace_back([] {});

            spawnSamples[static_cast<std::size_t>(i)] = bench::toNanoseconds(bench::now() - spawnStart);
        }

        for (std::thread& worker : workers) {
            worker.join();
        }

        const auto end = bench::now();

        return SpawnRun {
            .samples = std::move(spawnSamples),
            .elapsed = bench::TimeInterval::between(start, end)
        };
    }

    std::vector<bench::BenchmarkResult> runPoint(
        int threadCount,
        int repeat,
        const bench::MetricsConfig& metrics
    ) {
        SpawnRun spawnRun = spawnAndJoin(threadCount);

        const bench::RunMetrics run {
            .totalOperations = threadCount,
            .elapsed = spawnRun.elapsed
        };

        const bench::Point point {
            .target = {
                .language = "cpp",
                .primitive = kPrimitive,
                .scenario = kScenario
            },
            .config = {
                .threads = threadCount,
                .repeat = repeat,
                .batch = 1
            }
        };

        std::vector<bench::BenchmarkResult> results;

        if (metrics.throughput) {
            results.push_back(bench::makeThroughputResult(point, run));
        }

        if (metrics.latency) {
            results.push_back(bench::makeLatencyResult(point, run, std::move(spawnRun.samples)));
        }

        return results;
    }

    std::vector<bench::BenchmarkResult> runGrid(const bench::Options& options) {
        std::vector<bench::BenchmarkResult> results;

        for (int repeat = 0; repeat < options.experiment.repeats; ++repeat) {
            for (int threads : options.experiment.threads) {
                auto pointResults = runPoint(threads, repeat, options.metrics);

                results.insert(
                    results.end(),
                    std::make_move_iterator(pointResults.begin()),
                    std::make_move_iterator(pointResults.end()));
            }
        }

        return results;
    }

}

int main(int argc, char** argv) {
    const bench::Options options = bench::parseOptions(argc, argv);

    bench::CsvReporter reporter;

    reporter.emit(options, runGrid(options));

    return 0;
}