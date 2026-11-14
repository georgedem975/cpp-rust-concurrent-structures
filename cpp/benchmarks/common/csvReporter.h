#ifndef CPP_BENCHMARKS_COMMON_CSV_REPORTER_H_
#define CPP_BENCHMARKS_COMMON_CSV_REPORTER_H_

#include <memory>
#include <vector>

#include "config.h"
#include "result.h"

namespace bench {
    class CsvReporter {
    public:
        CsvReporter();
        ~CsvReporter();

        CsvReporter(const CsvReporter&) = delete;
        CsvReporter& operator=(const CsvReporter&) = delete;

        void emit(const Options& options, const std::vector<BenchmarkResult>& results);

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}

#endif