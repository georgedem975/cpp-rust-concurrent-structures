#include "blocking_queue.h"

namespace service {
    bool BlockingQueue::push(const Trade& trade) {
        std::unique_lock<std::mutex> lock(mutex_);

        notFull_.wait(lock, [this] {
            return closed_ || queue_.size() < kCapacity;
        });

        if (closed_) {
            return false;
        }

        queue_.push_back(trade);
        size_.fetch_add(1, std::memory_order_relaxed);

        notEmpty_.notify_one();
        return true;
    }

    bool BlockingQueue::pop(Trade& trade) {
        std::unique_lock<std::mutex> lock(mutex_);

        notEmpty_.wait(lock, [this] {
            return closed_ || !queue_.empty();
        });

        if (queue_.empty()) {
            return false;
        }

        trade = queue_.front();
        queue_.pop_front();

        size_.fetch_sub(1, std::memory_order_relaxed);

        notFull_.notify_one();
        return true;
    }

    void BlockingQueue::close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }

        notEmpty_.notify_all();
        notFull_.notify_all();
    }
}