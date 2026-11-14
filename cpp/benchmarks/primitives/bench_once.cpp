#include <barrier>
#include <iterator>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "clock.h"
#include "config.h"
#include "csvReporter.h"
#include "metrics.h"
#include "result.h"
#include "runner.h"

namespace {
    void executeHotPath(std::once_flag& flag, int batchSize) {
        for (int i = 0; i < batchSize; ++i) {
            std::call_once(flag, [] {});
        }
    }

    std::vector<bench::BenchmarkResult> runColdPath(
        int threadCount,
        int repeat,
        const bench::MetricsConfig& metrics) {
        std::once_flag flag;
        std::barrier startGate(threadCount + 1);

        std::vector<double> samples(static_cast<std::size_t>(threadCount));
        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(threadCount));

        for (int i = 0; i < threadCount; ++i) {
            workers.emplace_back([&flag, &startGate, &samples, i] {
                startGate.arrive_and_wait();

                const auto callStart = bench::now();
                std::call_once(flag, [] {});

                samples[static_cast<std::size_t>(i)] = bench::toNanoseconds(bench::now() - callStart);
            });
        }

        const auto start = bench::now();
        startGate.arrive_and_wait();

        for (std::thread& worker : workers) {
            worker.join();
        }

        const auto end = bench::now();

        const bench::RunMetrics run {
            .totalOperations = threadCount,
            .elapsed = bench::TimeInterval::between(start, end)
        };

        const bench::Point point {
            .target = {
                .language = "cpp",
                .primitive = "once",
                .scenario = "cold_path"
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
            results.push_back(bench::makeLatencyResult(point, run, std::move(samples)));
        }

        return results;
    }

    std::vector<bench::BenchmarkResult> runColdPathGrid(const bench::Options& options) {
        std::vector<bench::BenchmarkResult> results;

        for (int repeat = 0; repeat < options.experiment.repeats; ++repeat) {
            for (int threads : options.experiment.threads) {
                auto pointResults = runColdPath(threads, repeat, options.metrics);

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

    std::once_flag flag;
    std::call_once(flag, [] {});

    bench::runGrid(
        options,
        {
            .language = "cpp",
            .primitive = "once",
            .scenario = "hot_path"
        },
        [&options, &flag](const bench::WorkerContext&) {
            return bench::WorkerFunction(
                [&options, &flag](const bench::WorkerContext&) {
                    executeHotPath(flag, options.execution.batch);
                });
        },
        reporter);

    reporter.emit(options, runColdPathGrid(options));

    return 0;
}