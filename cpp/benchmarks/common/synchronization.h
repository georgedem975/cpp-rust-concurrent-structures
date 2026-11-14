#ifndef CPP_BENCHMARKS_COMMON_SYNCHRONIZATION_H_
#define CPP_BENCHMARKS_COMMON_SYNCHRONIZATION_H_

#include <atomic>
#include <thread>

#include "clock.h"

namespace bench {
    class StartBarrier {
    public:
        explicit StartBarrier(int participants) : participants_(participants) {}

        BenchmarkClock::time_point Wait() {
            if (arrived_.fetch_add(1, std::memory_order_acq_rel) + 1 == participants_) {
                startTime_ = now();
                open_.store(true, std::memory_order_release);
            } else {
                while (!open_.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
            }
            return startTime_;
        }

        BenchmarkClock::time_point startTime() const {
            return startTime_;
        }

    private:
        const int participants_;
        std::atomic<int> arrived_ { 0 };
        std::atomic<bool> open_ { false };
        BenchmarkClock::time_point startTime_;
    };
}

#endif