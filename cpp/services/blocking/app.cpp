#include "../common/reporter.h"
#include "../common/statistics.h"
#include "../common/tcp_server.h"
#include "blocking_queue.h"

#include <algorithm>
#include <cstdint>
#include <thread>
#include <vector>

namespace {
    constexpr std::uint16_t kPort = 9001;

    constexpr unsigned int kMinWorkerCount = 1;
    constexpr unsigned int kWorkerDivisor = 2;

    std::size_t workerCount() {
        return std::max(kMinWorkerCount, std::thread::hardware_concurrency() / kWorkerDivisor);
    }

    void workerLoop(service::BlockingQueue& queue, service::Statistics& statistics) {
        service::Trade trade;

        while (queue.pop(trade)) {
            statistics.add(trade);
        }
    }
}

int main() {
    service::BlockingQueue queue;
    service::Statistics statistics;

    service::TcpServer<service::BlockingQueue> server(kPort, queue);

    std::vector<std::thread> workers;
    workers.reserve(workerCount());

    for (std::size_t i = 0; i < workerCount(); ++i) {
        workers.emplace_back(workerLoop, std::ref(queue), std::ref(statistics));
    }

    service::Reporter reporter(
        "blocking",
        kPort,
        statistics,
        queue.sizeCounter(),
        service::BlockingQueue::kCapacity,
        [&server] { server.stop(); }
    );

    reporter.start();
    server.run();

    queue.close();

    for (std::thread& worker : workers) {
        worker.join();
    }

    reporter.stop();

    return 0;
}