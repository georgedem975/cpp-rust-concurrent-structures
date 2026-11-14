#include <mutex>

#include "csvReporter.h"
#include "runner.h"

namespace {
    void executeLockUnlock(std::mutex& mutex, int batchSize) {
        for (int i = 0; i < batchSize; ++i) {
            const std::lock_guard<std::mutex> lock(mutex);
        }
    }

    void executeTryLock(std::mutex& mutex, int batchSize) {
        for (int i = 0; i < batchSize; ++i) {
            if (mutex.try_lock()) {
                mutex.unlock();
            }
        }
    }
}

int main(int argc, char** argv) {
    const bench::Options options = bench::parseOptions(argc, argv);
    
    bench::CsvReporter reporter;

    std::mutex mutex;

    bench::runGrid(
        options,
        {
            .language = "cpp",
            .primitive = "mutex",
            .scenario = "lock_unlock"
        },
        [&mutex, &options](const bench::WorkerContext&) {
            return bench::WorkerFunction(
                [&mutex, &options](const bench::WorkerContext&) {
                    executeLockUnlock(mutex, options.execution.batch);
                });
        },
        reporter);

    bench::runGrid(
        options,
        {
            .language = "cpp",
            .primitive = "mutex",
            .scenario = "try_lock"
        },
        [&mutex, &options](const bench::WorkerContext&) {
            return bench::WorkerFunction(
                [&mutex, &options](const bench::WorkerContext&) {
                    executeTryLock(mutex, options.execution.batch);
                });
        },
        reporter);

    return 0;
}