
#include "config/config.h"
#include "network/sender.h"
#include "statistics/progress_reporter.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

namespace {
    using clock = std::chrono::steady_clock;

    constexpr auto kUpdateInterval = std::chrono::milliseconds(100);

    std::atomic<bool> g_stop{false};

    void onSignal(int) {
        g_stop.store(true, std::memory_order_relaxed);
    }

    void registerSignalHandlers() {
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);
    }

    double elapsedSeconds(clock::time_point started) {
        const auto elapsed = clock::now() - started;
        return std::chrono::duration<double>(elapsed).count();
    }

    std::vector<std::unique_ptr<Sender>> createSenders(const std::vector<Endpoint>& targets) {
        std::vector<std::unique_ptr<Sender>> senders;
        senders.reserve(targets.size());

        std::uint64_t seed = 1;

        for (const auto& target : targets) {
            senders.push_back(std::make_unique<Sender>(target, seed++));
        }

        return senders;
    }

    void startSenders(const std::vector<std::unique_ptr<Sender>>& senders) {
        for (const auto& sender : senders) {
            sender->start();
        }
    }

    void stopSenders(const std::vector<std::unique_ptr<Sender>>& senders) {
        for (const auto& sender : senders) {
            sender->stop();
        }

        for (const auto& sender : senders) {
            sender->join();
        }
    }

    std::vector<SenderStatistics> collectStatistics(const std::vector<std::unique_ptr<Sender>>& senders) {
        std::vector<SenderStatistics> statistics;
        statistics.reserve(senders.size());

        for (const auto& sender : senders) {
            statistics.push_back({sender->name(), sender->sent(), sender->failed()});
        }

        return statistics;
    }

    bool allSendersFailed(const std::vector<std::unique_ptr<Sender>>& senders) {
        return std::all_of(senders.begin(), senders.end(), [](const auto& sender) {
                return sender->failed();
            }
        );
    }

    void monitor(
        const std::vector<std::unique_ptr<Sender>>& senders,
        ProgressReporter& reporter,
        clock::time_point started,
        double duration
    ) {
        while (!g_stop.load(std::memory_order_relaxed)) {
            reporter.update(collectStatistics(senders));

            if (duration > 0.0 && elapsedSeconds(started) >= duration) {
                break;
            }

            if (allSendersFailed(senders)) {
                break;
            }

            std::this_thread::sleep_for(kUpdateInterval);
        }

        g_stop.store(true, std::memory_order_relaxed);

        stopSenders(senders);
        reporter.finish(collectStatistics(senders));
    }

    void printErrors(const std::vector<std::unique_ptr<Sender>>& senders) {
        for (const auto& sender : senders) {
            if (sender->failed()) {
                std::fprintf(stderr, "%s: %s\n", sender->name().c_str(), sender->error().c_str());
            }
        }
    }
}

int main(int argc, char** argv) {
    const Config config = parseConfig(argc, argv);

    registerSignalHandlers();

    const auto senders = createSenders(config.targets);
    const auto started = clock::now();

    ProgressReporter reporter(config.targets, config.duration, started);

    startSenders(senders);

    std::thread monitorThread([&] {
        monitor(senders, reporter, started, config.duration);
    });

    reporter.run();

    g_stop.store(true, std::memory_order_relaxed);
    monitorThread.join();

    printErrors(senders);

    return allSendersFailed(senders) ? 1 : 0;
}