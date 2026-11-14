#ifndef SERVICES_LOCKFREE_LOCKFREE_QUEUE_H_
#define SERVICES_LOCKFREE_LOCKFREE_QUEUE_H_

#include "../common/protocol.h"

#include <boost/lockfree/queue.hpp>

#include <atomic>
#include <cstddef>

namespace service {
    class LockfreeQueue {
    public:
        static constexpr std::size_t kCapacity = 1 << 15;

        bool push(const Trade& trade);
        bool pop(Trade& trade);

        const std::atomic<std::size_t>& sizeCounter() const {
            return size_;
        }

    private:
        boost::lockfree::queue<Trade, boost::lockfree::capacity<kCapacity>> queue_;
        std::atomic<std::size_t> size_{0};
    };
}

#endif