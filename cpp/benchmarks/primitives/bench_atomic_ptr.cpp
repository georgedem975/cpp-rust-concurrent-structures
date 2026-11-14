#include <atomic>

#include "csvReporter.h"
#include "runner.h"

namespace {
    struct Cell {
        int value = 0;
    };

    Cell gA{};
    Cell gB{};

    void executeLoadBatch(const std::atomic<Cell*>& pointer, int batchSize, std::atomic<long long>& sink) {
        long long local = 0;

        for (int i = 0; i < batchSize; ++i) {
            Cell* cell = pointer.load(std::memory_order_acquire);
            local += cell->value;
        }

        sink.fetch_add(local, std::memory_order_relaxed);
    }

    void executeCasBatch(std::atomic<Cell*>& pointer, int batchSize) {
        for (int i = 0; i < batchSize; ++i) {
            Cell* expected = pointer.load(std::memory_order_relaxed);
            Cell* desired = (expected == &gA) ? &gB : &gA;

            while (!pointer.compare_exchange_weak(
                expected,
                desired,
                std::memory_order_acq_rel,
                std::memory_order_acquire
            )) {
                desired = (expected == &gA) ? &gB : &gA;
            }
        }
    }
}

int main(int argc, char** argv) {
    const bench::Options options = bench::parseOptions(argc, argv);
    
    bench::CsvReporter reporter;

    std::atomic<Cell*> pointer{&gA};
    std::atomic<long long> sink{0};

    bench::runGrid(
        options,
        {
            .language = "cpp",
            .primitive = "atomic_ptr",
            .scenario = "load"
        },
        [&](const bench::WorkerContext&) {
            return bench::WorkerFunction(
                [&pointer, &options, &sink](const bench::WorkerContext&) {
                    executeLoadBatch(pointer, options.execution.batch, sink);
                }
            );
        },
        reporter
    );

    bench::runGrid(
        options,
        {
            .language = "cpp",
            .primitive = "atomic_ptr",
            .scenario = "cas_loop"
        },
        [&](const bench::WorkerContext&) {
            return bench::WorkerFunction(
                [&pointer, &options](const bench::WorkerContext&) {
                    executeCasBatch(pointer, options.execution.batch);
                }
            );
        },
        reporter
    );

    return 0;
}