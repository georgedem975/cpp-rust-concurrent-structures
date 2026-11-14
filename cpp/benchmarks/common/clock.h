#ifndef CPP_BENCHMARKS_COMMON_CLOCK_H_
#define CPP_BENCHMARKS_COMMON_CLOCK_H_

#include <chrono>
#include <stdexcept>

namespace bench {
    using BenchmarkClock = std::chrono::steady_clock;
    using Nanoseconds = std::chrono::duration<double, std::nano>;

    inline constexpr double kNanosecondsPerSecond = static_cast<double>(std::nano::den);

    inline BenchmarkClock::time_point now() {
        return BenchmarkClock::now();
    }

    inline double toNanoseconds(BenchmarkClock::duration duration) {
        return Nanoseconds(duration).count();
    }

    struct TimeInterval {
        static TimeInterval between(BenchmarkClock::time_point start, BenchmarkClock::time_point end) {
            if (end < start) {
                throw std::invalid_argument("TimeInterval: end precedes start");
            }

            return TimeInterval(start, end);
        }

        double durationNs() const {
            return toNanoseconds(end_ - start_);
        }

    private:
        TimeInterval(BenchmarkClock::time_point start, BenchmarkClock::time_point end) 
            : start_(start), end_(end) {}

        BenchmarkClock::time_point start_;
        BenchmarkClock::time_point end_;
    };
}

#endif