#ifndef SERVICES_BLOCKING_BLOCKING_QUEUE_H_
#define SERVICES_BLOCKING_BLOCKING_QUEUE_H_

#include "../common/protocol.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>

namespace service {
    class BlockingQueue {
    public:
        static constexpr std::size_t kCapacity = 1 << 15;

        bool push(const Trade& trade);
        bool pop(Trade& trade);
        void close();

        const std::atomic<std::size_t>& sizeCounter() const {
            return size_;
        }

    private:
        mutable std::mutex mutex_;
        std::condition_variable notEmpty_;
        std::condition_variable notFull_;

        std::deque<Trade> queue_;
        std::atomic<std::size_t> size_{0};
        bool closed_ = false;
    };
}

#endif