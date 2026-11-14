#ifndef CPP_BENCHMARKS_COMMON_WORKER_H_
#define CPP_BENCHMARKS_COMMON_WORKER_H_

#include <atomic>
#include <limits>
#include <functional>

#include "clock.h"

namespace bench {
    using ThreadId = int;

    class WorkerControl {
    public:
        void setDeadline(BenchmarkClock::time_point deadline) {
            deadlineTicks_.store(deadline.time_since_epoch().count(), std::memory_order_release);
        }

        bool shouldStop() {
            const auto deadlineTicks = deadlineTicks_.load(std::memory_order_acquire);

            if (deadlineTicks != std::numeric_limits<ClockRep>::max() &&
                now().time_since_epoch().count() >= deadlineTicks) {
                stopRequested_.store(true, std::memory_order_release);
            }

            return stopRequested_.load(std::memory_order_acquire);
        }

    private:
        using ClockRep = BenchmarkClock::duration::rep;

        std::atomic<ClockRep> deadlineTicks_ { std::numeric_limits<ClockRep>::max() };
        std::atomic<bool> stopRequested_ { false };
    };

    struct WorkerContext {
        ThreadId id;
        int totalThreads;
        WorkerControl* control = nullptr;
        long long* completedOperations = nullptr;

        bool shouldStop() const {
            return control != nullptr && control->shouldStop();
        }

        void reportOperations(long long operations) const {
            if (completedOperations != nullptr) {
                *completedOperations = operations;
            }
        }
    };

    using WorkerFunction = std::function<void(const WorkerContext&)>;
    using WorkerFactory = std::function<WorkerFunction(const WorkerContext&)>;
}

#endif