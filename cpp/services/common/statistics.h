#ifndef SERVICES_COMMON_STATISTICS_H_
#define SERVICES_COMMON_STATISTICS_H_

#include "protocol.h"

#include <atomic>
#include <cstdint>

namespace service {
    struct StatisticsSnapshot {
        std::uint64_t processed = 0;
        std::uint64_t latencySumNs = 0;
        std::uint64_t maxLatencyNs = 0;
        std::uint64_t volume = 0;

        double averageLatencyUs() const;
    };

    class Statistics {
    public:
        void add(const Trade& trade);

        StatisticsSnapshot snapshot() const;

    private:
        std::atomic<std::uint64_t> processed_{0};
        std::atomic<std::uint64_t> latencySumNs_{0};
        std::atomic<std::uint64_t> maxLatencyNs_{0};
        std::atomic<std::uint64_t> volume_{0};
    };
}

#endif