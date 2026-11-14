#include "lockfree_queue.h"

#include <thread>

namespace service {
    bool LockfreeQueue::push(const Trade& trade) {
        while (true) {
            size_.fetch_add(1, std::memory_order_relaxed);

            if (queue_.push(trade)) {
                return true;
            }

            size_.fetch_sub(1, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    }

    bool LockfreeQueue::pop(Trade& trade) {
        if (!queue_.pop(trade)) {
            return false;
        }

        size_.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }
}