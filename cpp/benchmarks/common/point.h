#ifndef CPP_BENCHMARKS_COMMON_POINT_H_
#define CPP_BENCHMARKS_COMMON_POINT_H_

#include <cstddef>
#include <string>

namespace bench {
    struct BenchmarkTarget {
        std::string language;
        std::string primitive;
        std::string scenario;
    };

    struct PointConfig {
        int threads = 1;
        int repeat = 0;
        int batch = 1;
    };

    struct Point {
        BenchmarkTarget target;
        PointConfig config;
    };
}

#endif