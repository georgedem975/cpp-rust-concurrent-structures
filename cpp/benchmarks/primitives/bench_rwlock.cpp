#include <mutex>
#include <shared_mutex>

#include "csvReporter.h"
#include "runner.h"

namespace {
    constexpr int kWritePeriod90To10 = 10;
    constexpr int kWritePeriod50To50 = 2;

    void executeRead(std::shared_mutex& mutex, int batchSize) {
        for (int i = 0; i < batchSize; ++i) {
            const std::shared_lock<std::shared_mutex> lock(mutex);
        }
    }

    void executeWrite(std::shared_mutex& mutex, int batchSize) {
        for (int i = 0; i < batchSize; ++i) {
            const std::unique_lock<std::shared_mutex> lock(mutex);
        }
    }

    void executeMixed(std::shared_mutex& mutex, int batchSize, int writePeriod) {
        for (int i = 1; i <= batchSize; ++i) {
            if (i % writePeriod == 0) {
                const std::unique_lock<std::shared_mutex> lock(mutex);
            } else {
                const std::shared_lock<std::shared_mutex> lock(mutex);
            }
        }
    }

    void runRead(const bench::Options& options, std::shared_mutex& mutex, bench::CsvReporter& reporter) {
        bench::runGrid(
            options,
            {
                .language = "cpp",
                .primitive = "rwlock",
                .scenario = "read"
            },
            [&mutex, &options](const bench::WorkerContext&) {
                return bench::WorkerFunction(
                    [&mutex, &options](const bench::WorkerContext&) {
                        executeRead(mutex, options.execution.batch);
                    });
            },
            reporter);
    }

    void runWrite(const bench::Options& options, std::shared_mutex& mutex, bench::CsvReporter& reporter) {
        bench::runGrid(
            options,
            {
                .language = "cpp",
                .primitive = "rwlock",
                .scenario = "write"
            },
            [&mutex, &options](const bench::WorkerContext&) {
                return bench::WorkerFunction(
                    [&mutex, &options](const bench::WorkerContext&) {
                        executeWrite(mutex, options.execution.batch);
                    });
            },
            reporter);
    }

    void runMixed(
        const bench::Options& options,
        std::shared_mutex& mutex,
        bench::CsvReporter& reporter,
        const char* scenario,
        int writePeriod) {

        bench::runGrid(
            options,
            {
                .language = "cpp",
                .primitive = "rwlock",
                .scenario = scenario
            },
            [&mutex, &options, writePeriod](const bench::WorkerContext&) {
                return bench::WorkerFunction(
                    [&mutex, &options, writePeriod](const bench::WorkerContext&) {
                        executeMixed(mutex, options.execution.batch, writePeriod);
                    });
            },
            reporter);
    }
}

int main(int argc, char** argv) {
    const bench::Options options = bench::parseOptions(argc, argv);
    
    bench::CsvReporter reporter;
    
    std::shared_mutex mutex;

    runRead(options, mutex, reporter);

    runWrite(options, mutex, reporter);

    runMixed(options, mutex, reporter, "mix_90_10", kWritePeriod90To10);

    runMixed(options, mutex, reporter, "mix_50_50", kWritePeriod50To50);

    return 0;
}