#include "statistics.h"

#include "clock.h"

#include <algorithm>

namespace service {
    double StatisticsSnapshot::averageLatencyUs() const {
        if (processed == 0) {
            return 0.0;
        }

        return static_cast<double>(latencySumNs) / static_cast<double>(processed) / kNanosecondsPerMicrosecond;
    }

    void Statistics::add(const Trade& trade) {
        const std::int64_t elapsedNs = std::max<std::int64_t>(nowNs() - trade.sentNs, 0);

        const auto latencyNs = static_cast<std::uint64_t>(elapsedNs);

        processed_.fetch_add(1, std::memory_order_relaxed);
        latencySumNs_.fetch_add(latencyNs, std::memory_order_relaxed);
        volume_.fetch_add(trade.qty, std::memory_order_relaxed);

        std::uint64_t currentMax = maxLatencyNs_.load(std::memory_order_relaxed);

        while (currentMax < latencyNs &&
            !maxLatencyNs_.compare_exchange_weak(currentMax, latencyNs, std::memory_order_relaxed)) { }
    }

    StatisticsSnapshot Statistics::snapshot() const {
        return {
            processed_.load(std::memory_order_relaxed),
            latencySumNs_.load(std::memory_order_relaxed),
            maxLatencyNs_.load(std::memory_order_relaxed),
            volume_.load(std::memory_order_relaxed)
        };
    }
}