#include <barrier>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
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
    constexpr std::size_t kQueueCapacity = 64;

    class BoundedQueue {
    public:
        void push(int value) {
            std::unique_lock<std::mutex> lock(mutex_);

            notFull_.wait(lock, [this] {
                return items_.size() < kQueueCapacity;
            });

            items_.push_back(value);

            lock.unlock();

            notEmpty_.notify_one();
        }

        bool pop(int& value) {
            std::unique_lock<std::mutex> lock(mutex_);

            notEmpty_.wait(lock, [this] {
                return !items_.empty() || done_;
            });

            if (items_.empty()) {
                return false;
            }

            value = items_.front();
            items_.pop_front();

            lock.unlock();
            notFull_.notify_one();

            return true;
        }

        void finish() {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                done_ = true;
            }

            notEmpty_.notify_all();
        }

    private:
        std::mutex mutex_;
        std::condition_variable notEmpty_;
        std::condition_variable notFull_;

        std::deque<int> items_;
        bool done_ = false;
    };

    using ThreadSamples = std::vector<std::vector<double>>;
    using OperationCounts = std::vector<long long>;

    void recordSample(std::vector<double>& samples, bench::BenchmarkClock::time_point start) {
        samples.push_back(bench::toNanoseconds(bench::now() - start));
    }

    void runProducer(
        BoundedQueue& queue,
        bench::BenchmarkClock::time_point deadline,
        std::vector<double>& samples,
        long long& operations
    ) {
        while (bench::now() < deadline) {
            const auto start = bench::now();

            queue.push(1);

            recordSample(samples, start);

            ++operations;
        }

        queue.finish();
    }

    void runConsumer(
        BoundedQueue& queue,
        std::vector<double>& samples,
        long long& operations
    ) {
        int value = 0;

        while (true) {
            const auto start = bench::now();

            if (!queue.pop(value)) {
                break;
            }

            recordSample(samples, start);

            ++operations;
        }
    }

    void runSingleThread(
        BoundedQueue& queue,
        bench::BenchmarkClock::time_point deadline,
        std::vector<double>& samples,
        long long& operations
    ) {
        int value = 0;

        while (bench::now() < deadline) {
            auto start = bench::now();

            queue.push(1);

            recordSample(samples, start);

            ++operations;

            start = bench::now();

            if (queue.pop(value)) {
                recordSample(samples, start);
                ++operations;
            }
        }
    }

    bench::Run collectRun(
        const ThreadSamples& samples,
        const OperationCounts& operationCounts,
        bench::BenchmarkClock::time_point start,
        bench::BenchmarkClock::time_point end
    ) {
        long long totalOperations = 0;

        for (long long count : operationCounts) {
            totalOperations += count;
        }

        bench::RunSamples runSamples;
        runSamples.batchDurationsNs = samples;

        const bench::RunMetrics metrics {
            .totalOperations = totalOperations,
            .elapsed = bench::TimeInterval::between(start, end)
        };

        return bench::Run {
            .metrics = metrics,
            .samples = std::move(runSamples)
        };
    }

    bench::Run runHandoff(const bench::Options& options, int threadCount) {
        BoundedQueue queue;

        ThreadSamples samples(static_cast<std::size_t>(threadCount));

        OperationCounts operationCounts(static_cast<std::size_t>(threadCount), 0);

        std::barrier startGate(static_cast<std::ptrdiff_t>(threadCount + 1));

        std::vector<std::thread> workers;

        workers.reserve(static_cast<std::size_t>(threadCount));

        const auto duration = std::chrono::duration_cast<bench::BenchmarkClock::duration>(
            bench::Nanoseconds(options.execution.duration * bench::kNanosecondsPerSecond));

        const auto start = bench::now();

        const auto deadline = start + duration;

        for (int i = 0; i < threadCount; ++i) {
            workers.emplace_back([&, i] {
                auto& localSamples = samples[static_cast<std::size_t>(i)];
                auto& localOperations = operationCounts[static_cast<std::size_t>(i)];

                startGate.arrive_and_wait();

                if (threadCount == 1) {
                    runSingleThread(queue, deadline, localSamples, localOperations);
                } else if (i == 0) {
                    runProducer(queue, deadline, localSamples, localOperations);
                } else {
                    runConsumer(queue, localSamples, localOperations);
                }
            });
        }

        startGate.arrive_and_wait();

        for (std::thread& worker : workers) {
            worker.join();
        }

        const auto end = bench::now();

        return collectRun(samples, operationCounts, start, end);
    }
}

int main(int argc, char** argv) {
    const bench::Options options = bench::parseOptions(argc, argv);

    bench::CsvReporter reporter;

    std::vector<bench::BenchmarkResult> results;

    for (int repeat = 0; repeat < options.experiment.repeats; ++repeat) {
        for (int threadCount : options.experiment.threads) {
            const bench::Run run = runHandoff(options, threadCount);

            const bench::Point point{
                .target = {
                    .language = "cpp",
                    .primitive = "condvar",
                    .scenario = "handoff"
                },
                .config = {
                    .threads = threadCount,
                    .repeat = repeat,
                    .batch = 1
                }
            };

            if (options.metrics.throughput) {
                results.push_back(bench::makeThroughputResult(point, run.metrics));
            }

            if (options.metrics.latency) {
                results.push_back(
                    bench::makeLatencyResult(point, run.metrics, bench::perOperationSamples(run.samples, 1))
                );
            }
        }
    }

    reporter.emit(options, results);

    return 0;
}