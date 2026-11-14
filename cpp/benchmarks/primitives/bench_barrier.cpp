#include <algorithm>
#include <barrier>
#include <cmath>
#include <cstddef>
#include <thread>
#include <vector>

#include "clock.h"
#include "config.h"
#include "csvReporter.h"
#include "metrics.h"
#include "result.h"
#include "run.h"

namespace {
    constexpr int kMaxRounds = 10'000'000;
    constexpr int kCalibrationRounds = 32;

    bench::Run runRounds(const bench::Options& options, int threadCount, int rounds) {
        std::barrier phaseBarrier(static_cast<std::ptrdiff_t>(threadCount));
        std::barrier startGate(static_cast<std::ptrdiff_t>(threadCount + 1));

        bench::RunSamples samples;
        samples.batchDurationsNs.resize(static_cast<std::size_t>(threadCount));

        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(threadCount));

        for (int i = 0; i < threadCount; ++i) {
            workers.emplace_back(
                [&phaseBarrier, &startGate, &samples, &options, i, rounds] {
                    std::vector<double> localSamples;
                    localSamples.reserve(static_cast<std::size_t>(rounds));
                    startGate.arrive_and_wait();

                    for (int round = 0; round < rounds; ++round) {
                        const auto batchStart = bench::now();

                        for (int operation = 0; operation < options.execution.batch; ++operation) {
                            phaseBarrier.arrive_and_wait();
                        }

                        localSamples.push_back(bench::toNanoseconds(bench::now() - batchStart));
                    }

                    samples.batchDurationsNs[static_cast<std::size_t>(i)] = std::move(localSamples);
                }
            );
        }

        const auto start = bench::now();
        startGate.arrive_and_wait();

        for (std::thread& worker : workers) {
            worker.join();
        }

        const auto end = bench::now();

        const bench::RunMetrics metrics{
            .totalOperations =
                static_cast<long long>(threadCount) *
                static_cast<long long>(rounds) *
                static_cast<long long>(options.execution.batch),

            .elapsed = bench::TimeInterval::between(start, end)
        };

        return bench::Run{ .metrics = metrics, .samples = std::move(samples) };
    }

    int calibrateRounds(const bench::Options& options, int threadCount, double targetDurationSeconds) {
        const bench::Run probe = runRounds(options, threadCount, kCalibrationRounds);

        const double elapsedNs = probe.metrics.elapsed.durationNs();

        if (elapsedNs <= 0.0) {
            return 1;
        }

        const double averageRoundNs =
            elapsedNs / static_cast<double>(kCalibrationRounds);

        const double targetDurationNs =
            targetDurationSeconds * bench::kNanosecondsPerSecond;

        const double estimatedRounds =
            targetDurationNs / averageRoundNs;

        const double boundedRounds = 
            std::clamp(std::ceil(estimatedRounds), 1.0, static_cast<double>(kMaxRounds));

        return static_cast<int>(boundedRounds);
    }

    int calibrateMeasurementRounds(const bench::Options& options, int threadCount) {
        return calibrateRounds(options, threadCount, options.execution.duration);
    }

    int calibrateWarmupRounds(const bench::Options& options, int threadCount) {
        return calibrateRounds(options, threadCount, options.execution.warmup);
    }
}

int main(int argc, char** argv) {
    const bench::Options options = bench::parseOptions(argc, argv);

    bench::CsvReporter reporter;

    if (options.execution.warmup > 0.0 && !options.experiment.threads.empty()) {
        const int warmupThreads = options.experiment.threads.back();

        const int warmupRounds = calibrateWarmupRounds(options, warmupThreads);

        runRounds(options, warmupThreads, warmupRounds);
    }

    std::vector<bench::BenchmarkResult> results;

    for (int repeat = 0; repeat < options.experiment.repeats; ++repeat) {
        for (int threadCount : options.experiment.threads) {
            const int rounds = calibrateMeasurementRounds(options, threadCount);

            const bench::Run run = runRounds(options, threadCount, rounds);

            const bench::Point point {
                .target = {
                    .language = "cpp",
                    .primitive = "barrier",
                    .scenario = "arrive_and_wait"
                },
                .config = {
                    .threads = threadCount,
                    .repeat = repeat,
                    .batch = options.execution.batch
                }
            };

            if (options.metrics.throughput) {
                results.push_back(bench::makeThroughputResult(point, run.metrics));
            }

            if (options.metrics.latency) {
                results.push_back(bench::makeLatencyResult(
                        point,
                        run.metrics,
                        bench::perOperationSamples(run.samples, options.execution.batch)
                    )
                );
            }
        }
    }

    reporter.emit(options, results);

    return 0;
}