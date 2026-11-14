#include <atomic>

#include "csvReporter.h"
#include "runner.h"

namespace {
    void executeSpinlockBatch(std::atomic_flag& flag, int batchSize) {
        for (int i = 0; i < batchSize; ++i) {
            while (flag.test_and_set(std::memory_order_acquire)) { }

            flag.clear(std::memory_order_release);
        }
    }

    void executeReadBatch(const std::atomic_flag& flag, int batchSize, std::atomic<long long>& sink) {
        long long local = 0;

        for (int i = 0; i < batchSize; ++i) {
            local += flag.test(std::memory_order_relaxed) ? 1 : 0;
        }

        sink.fetch_add(local, std::memory_order_relaxed);
    }
}

int main(int argc, char** argv) {
    const bench::Options options = bench::parseOptions(argc, argv);

    bench::CsvReporter reporter;

    std::atomic_flag flag = ATOMIC_FLAG_INIT;
    std::atomic<long long> sink{0};

    bench::runGrid(
        options,
        {
            .language = "cpp",
            .primitive = "atomic_flag",
            .scenario = "spinlock"
        },
        [&](const bench::WorkerContext&) {
            return bench::WorkerFunction(
                [&flag, &options](const bench::WorkerContext&) {
                    executeSpinlockBatch(flag, options.execution.batch);
                }
            );
        },
        reporter
    );

    bench::runGrid(
        options,
        {
            .language = "cpp",
            .primitive = "atomic_flag",
            .scenario = "read"
        },
        [&](const bench::WorkerContext&) {
            return bench::WorkerFunction(
                [&flag, &options, &sink](const bench::WorkerContext&) {
                    executeReadBatch(flag, options.execution.batch, sink);
                }
            );
        },
        reporter
    );

    return 0;
}