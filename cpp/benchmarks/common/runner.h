#ifndef CPP_BENCHMARKS_COMMON_RUNNER_H_
#define CPP_BENCHMARKS_COMMON_RUNNER_H_

#include "config.h"
#include "point.h"
#include "worker.h"

namespace bench {
    class CsvReporter;

    void runGrid(
        const Options& options,
        const BenchmarkTarget& target,
        const WorkerFactory& makeWorker,
        CsvReporter& reporter
    );
}

#endif