#ifndef SERVICES_COMMON_REPORTER_H_
#define SERVICES_COMMON_REPORTER_H_

#include "statistics.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

namespace service {
    class Reporter {
    public:
        using Clock = std::chrono::steady_clock;

        Reporter(
            std::string name,
            std::uint16_t port,
            const Statistics& statistics,
            const std::atomic<std::size_t>& queueSize,
            std::size_t capacity,
            std::function<void()> onInterrupt = {}
        );

        ~Reporter();

        Reporter(const Reporter&) = delete;
        Reporter& operator=(const Reporter&) = delete;

        void start();
        void stop();

    private:
        void run();
        void tick();
        void printSummary() const;
        ftxui::Element render();

        std::string name_;
        std::uint16_t port_;
        const Statistics& statistics_;
        const std::atomic<std::size_t>& queueSize_;
        std::size_t capacity_;
        std::function<void()> onInterrupt_;

        ftxui::ScreenInteractive screen_;
        Clock::time_point started_ = Clock::now();

        std::size_t frame_ = 0;
        std::atomic<bool> finished_{false};
        std::atomic<bool> interrupted_{false};
        std::atomic<bool> stop_{false};

        std::thread uiThread_;
        std::thread timer_;
    };
}

#endif