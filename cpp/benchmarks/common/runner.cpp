#include "runner.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "csvReporter.h"
#include "metrics.h"
#include "synchronization.h"

namespace bench {
    namespace {
        enum class SampleMode {
            None,
            Batches,
        };

        struct RunSettings {
            int threadCount;
            int batchSize;
            BenchmarkClock::duration duration;
            SampleMode sampling;
        };

        struct ThreadRunResult {
            BenchmarkClock::time_point finishTime;
            long long operations = 0;
            std::vector<double> batchDurationsNs;
            std::exception_ptr exception;
        };

        struct RunExecution {
            BenchmarkClock::time_point startTime;
            std::vector<ThreadRunResult> workers;
        };

        void validateRunSettings(int threadCount, double durationSeconds, int batchSize) {
            if (threadCount < 1) {
                throw std::invalid_argument("Thread count must be positive");
            }

            if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0) {
                throw std::invalid_argument("Duration must be finite and positive");
            }

            if (batchSize <= 0) {
                throw std::invalid_argument("Batch size must be positive");
            }
        }

        void rethrowWorkerException(const RunExecution& execution) {
            for (const ThreadRunResult& worker : execution.workers) {
                if (worker.exception) {
                    std::rethrow_exception(worker.exception);
                }
            }
        }

        ThreadRunResult runWorker(
            ThreadId id,
            const RunSettings& settings,
            StartBarrier& barrier,
            WorkerControl& control,
            const WorkerFunction& worker
        ) {
            ThreadRunResult result;

            const BenchmarkClock::time_point startTime = barrier.Wait();
            control.setDeadline(startTime + settings.duration);
            result.finishTime = startTime;

            const bool collectSamples = settings.sampling == SampleMode::Batches;

            try {
                while (true) {
                    long long reportedOperations = -1;
                    const WorkerContext context {
                        id,
                        settings.threadCount,
                        &control,
                        &reportedOperations
                    };
                    const auto batchStart = collectSamples ? now() : BenchmarkClock::time_point{};

                    worker(context);
                    result.finishTime = now();

                    const long long completedOperations = reportedOperations < 0
                        ? settings.batchSize
                        : reportedOperations;

                    result.operations += completedOperations;

                    if (collectSamples && completedOperations > 0) {
                        const double batchDurationNs =
                            toNanoseconds(result.finishTime - batchStart);
                        result.batchDurationsNs.push_back(
                            batchDurationNs * settings.batchSize / completedOperations
                        );
                    }

                    if (control.shouldStop()) {
                        break;
                    }
                }
            } catch (...) {
                result.exception = std::current_exception();
                result.finishTime = now();
            }

            return result;
        }

        RunExecution runWorkers(const RunSettings& settings, const WorkerFactory& makeWorker) {
            const std::size_t workerCount = static_cast<std::size_t>(settings.threadCount);

            StartBarrier barrier(settings.threadCount);
            WorkerControl control;

            std::vector<ThreadRunResult> results(workerCount);
            std::vector<WorkerFunction> workers;
            workers.reserve(workerCount);

            std::vector<std::thread> threadsPool;
            threadsPool.reserve(workerCount);

            for (ThreadId id = 0; id < settings.threadCount; ++id) {
                workers.push_back(
                    makeWorker(WorkerContext { .id = id, .totalThreads = settings.threadCount })
                );
            }

            for (ThreadId id = 0; id < settings.threadCount; ++id) {
                threadsPool.emplace_back([&, id] {
                    results[static_cast<std::size_t>(id)] =
                        runWorker(
                            id,
                            settings,
                            barrier,
                            control,
                            workers[static_cast<std::size_t>(id)]
                        );
                });
            }

            for (std::thread& thread : threadsPool) {
                thread.join();
            }

            return RunExecution { .startTime = barrier.startTime(), .workers = std::move(results) };
        }

        Run collectRunResult(const RunExecution& execution, SampleMode sampling) {
            rethrowWorkerException(execution);

            long long operations = 0;
            BenchmarkClock::time_point finishTime = execution.startTime;

            for (const ThreadRunResult& worker : execution.workers) {
                operations += worker.operations;
                finishTime = std::max(finishTime, worker.finishTime);
            }

            RunSamples samples;

            if (sampling == SampleMode::Batches) {
                samples.batchDurationsNs.reserve(execution.workers.size());

                for (const ThreadRunResult& worker : execution.workers) {
                    samples.batchDurationsNs.push_back(worker.batchDurationsNs);
                }
            }

            return Run {
                .metrics = RunMetrics {
                    .totalOperations = operations,
                    .elapsed = TimeInterval::between(execution.startTime, finishTime)
                },
                .samples = std::move(samples)
            };
        }

        Run executeRun(
            int threadCount,
            double durationSeconds,
            int batchSize,
            SampleMode sampling,
            const WorkerFactory& makeWorker
        ) {
            validateRunSettings(threadCount, durationSeconds, batchSize);

            const RunSettings settings {
                .threadCount = threadCount,
                .batchSize = batchSize,
                .duration = std::chrono::duration_cast<BenchmarkClock::duration>(
                    Nanoseconds(durationSeconds * kNanosecondsPerSecond)),
                .sampling = sampling
            };

            return collectRunResult(runWorkers(settings, makeWorker), settings.sampling);
        }

        BenchmarkResult measureThroughput(
            const Options& options,
            const Point& point,
            const WorkerFactory& makeWorker
        ) {
            const Run run = executeRun(
                point.config.threads,
                options.execution.duration,
                point.config.batch,
                SampleMode::None,
                makeWorker
            );

            return makeThroughputResult(point, run.metrics);
        }

        BenchmarkResult measureLatency(
            const Options& options,
            const Point& point,
            const WorkerFactory& makeWorker
        ) {
            const Run run = executeRun(
                point.config.threads,
                options.execution.duration,
                point.config.batch,
                SampleMode::Batches,
                makeWorker
            );

            const auto samples = perOperationSamples(run.samples, point.config.batch);

            return makeLatencyResult(point, run.metrics, samples);
        }

        Point makePoint(const BenchmarkTarget& target, int threads, int repeat, int batch) {
            return Point {
                .target = target,
                .config = PointConfig { .threads = threads, .repeat = repeat, .batch = batch }
            };
        }

        void runWarmup(const Options& options, const WorkerFactory& makeWorker) {
            if (options.execution.warmup <= 0.0 || options.experiment.threads.empty()) {
                return;
            }

            const int threads = options.experiment.threads.back();

            executeRun(
                threads,
                options.execution.warmup,
                options.execution.batch,
                SampleMode::None,
                makeWorker
            );
        }

        std::size_t resultsPerPoint(const Options& options) {
            return (options.metrics.throughput ? 1u : 0u) + (options.metrics.latency ? 1u : 0u);
        }

        std::vector<BenchmarkResult> runExperiments(
            const Options& options,
            const BenchmarkTarget& target,
            const WorkerFactory& makeWorker
        ) {
            const std::size_t perPoint = resultsPerPoint(options);

            std::vector<BenchmarkResult> results;

            results.reserve(
                static_cast<std::size_t>(options.experiment.repeats) *
                options.experiment.threads.size() *
                perPoint
            );

            for (int repeat = 0; repeat < options.experiment.repeats; ++repeat) {
                for (const int threads : options.experiment.threads) {
                    const Point point = makePoint(target, threads, repeat, options.execution.batch);

                    if (options.metrics.throughput) {
                        results.push_back(measureThroughput(options, point, makeWorker));
                    }

                    if (options.metrics.latency) {
                        results.push_back(measureLatency(options, point, makeWorker));
                    }
                }
            }

            return results;
        }

    }

    void runGrid(
        const Options& options,
        const BenchmarkTarget& target,
        const WorkerFactory& makeWorker,
        CsvReporter& reporter
    ) {
        runWarmup(options, makeWorker);

        const std::vector<BenchmarkResult> results = runExperiments(options, target, makeWorker);

        reporter.emit(options, results);
    }
}