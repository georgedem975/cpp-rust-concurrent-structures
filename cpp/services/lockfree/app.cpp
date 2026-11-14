#include "../common/reporter.h"
#include "../common/statistics.h"
#include "../common/tcp_server.h"
#include "lockfree_queue.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace {
    constexpr std::uint16_t kPort = 9002;
    constexpr auto kIdleWait = std::chrono::microseconds(50);

    constexpr unsigned int kMinWorkerCount = 1;
    constexpr unsigned int kWorkerDivisor = 2;

    std::size_t workerCount() {
        return std::max(kMinWorkerCount, std::thread::hardware_concurrency() / kWorkerDivisor);
    }

    void workerLoop(service::LockfreeQueue& queue, service::Statistics& statistics, const std::atomic<bool>& stop) {
        service::Trade trade;

        while (true) {
            if (queue.pop(trade)) {
                statistics.add(trade);
                continue;
            }

            if (stop.load(std::memory_order_acquire)) {
                break;
            }

            std::this_thread::sleep_for(kIdleWait);
        }
    }
}

int main() {
    auto queue = std::make_unique<service::LockfreeQueue>();
    service::Statistics statistics;

    std::atomic<bool> stop{false};

    service::TcpServer<service::LockfreeQueue> server(kPort, *queue);

    std::vector<std::thread> workers;
    workers.reserve(workerCount());

    for (std::size_t i = 0; i < workerCount(); ++i) {
        workers.emplace_back(
            workerLoop,
            std::ref(*queue),
            std::ref(statistics),
            std::cref(stop)
        );
    }

    service::Reporter reporter(
        "lockfree",
        kPort,
        statistics,
        queue->sizeCounter(),
        service::LockfreeQueue::kCapacity,
        [&server] { server.stop(); }
    );

    reporter.start();
    server.run();

    stop.store(true, std::memory_order_release);

    for (std::thread& worker : workers) {
        worker.join();
    }

    reporter.stop();

    return 0;
}