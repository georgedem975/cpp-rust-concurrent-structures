#ifndef CPP_BENCHMARKS_COMMON_LATENCY_H_
#define CPP_BENCHMARKS_COMMON_LATENCY_H_

#include <vector>

namespace bench {
    class Latency {
    public:
        static Latency fromSamples(std::vector<double> samplesNs);

        double meanNs() const noexcept;
        double medianNs() const noexcept;
        double p95Ns() const noexcept;
        double p99Ns() const noexcept;
        double p999Ns() const noexcept;
        double maximumNs() const noexcept;

    private:
        Latency(
            double meanNs,
            double medianNs,
            double p95Ns,
            double p99Ns,
            double p999Ns,
            double maximumNs
        );

        double meanNs_;
        double medianNs_;
        double p95Ns_;
        double p99Ns_;
        double p999Ns_;
        double maximumNs_;
    };
}

#endif