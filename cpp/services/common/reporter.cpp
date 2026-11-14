#include "reporter.h"

#include "clock.h"
#include "format.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <thread>
#include <utility>

namespace {
    constexpr int kLabelWidth = 14;
    constexpr auto kTickInterval = std::chrono::milliseconds(100);

    constexpr float kWarningFillRatio = 0.9f;
    constexpr double kMaxFillRatio = 1.0;
    constexpr int kQueueSizeWidth = 18;
    constexpr int kSpinnerStyle = 13;
}

namespace service {
    Reporter::Reporter(
        std::string name,
        std::uint16_t port,
        const Statistics& statistics,
        const std::atomic<std::size_t>& queueSize,
        std::size_t capacity,
        std::function<void()> onInterrupt
    )
        : name_(std::move(name)),
        port_(port),
        statistics_(statistics),
        queueSize_(queueSize),
        capacity_(capacity),
        onInterrupt_(std::move(onInterrupt)),
        screen_(ftxui::ScreenInteractive::TerminalOutput()) {
    }

    Reporter::~Reporter() {
        stop();
    }

    void Reporter::start() {
        if (uiThread_.joinable()) {
            return;
        }

        stop_.store(false, std::memory_order_relaxed);
        uiThread_ = std::thread(&Reporter::run, this);
        timer_ = std::thread(&Reporter::tick, this);
    }

    void Reporter::stop() {
        finished_.store(true, std::memory_order_relaxed);

        screen_.PostEvent(ftxui::Event::Custom);
        screen_.Exit();

        stop_.store(true, std::memory_order_relaxed);

        if (timer_.joinable()) {
            timer_.join();
        }

        if (uiThread_.joinable()) {
            uiThread_.join();
        }

        if (interrupted_.exchange(false, std::memory_order_relaxed)) {
            printSummary();
        }
    }

    void Reporter::run() {
        screen_.Loop(ftxui::Renderer([this] { return render(); }));

        if (!finished_.exchange(true)) {
            interrupted_.store(true, std::memory_order_relaxed);

            if (onInterrupt_) {
                onInterrupt_();
            }
        }
    }

    void Reporter::tick() {
        while (!stop_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(kTickInterval);

            if (!stop_.load(std::memory_order_relaxed)) {
                screen_.PostEvent(ftxui::Event::Custom);
            }
        }
    }

    void Reporter::printSummary() const {
        const StatisticsSnapshot snapshot = statistics_.snapshot();
        const double elapsed = std::chrono::duration<double>(Clock::now() - started_).count();
        const double rate = elapsed > 0.0 ? static_cast<double>(snapshot.processed) / elapsed : 0.0;

        fmt::print(
            "\n{}: processed {} | rate {} trades/s | avg {:.2f} ms | max {:.2f} ms | volume {}\n",
            name_,
            formatNumber(static_cast<double>(snapshot.processed)),
            formatNumber(rate),
            snapshot.averageLatencyUs() / kMicrosecondsPerMillisecond,
            static_cast<double>(snapshot.maxLatencyNs) / kNanosecondsPerMillisecond,
            formatNumber(static_cast<double>(snapshot.volume))
        );
    }

    ftxui::Element Reporter::render() {
        using namespace ftxui;

        const StatisticsSnapshot snapshot = statistics_.snapshot();
        const std::size_t queue = queueSize_.load(std::memory_order_relaxed);
        const double elapsed = std::chrono::duration<double>(Clock::now() - started_).count();
        const double rate = elapsed > 0.0 ? static_cast<double>(snapshot.processed) / elapsed : 0.0;
        const float fill = capacity_ > 0
            ? static_cast<float>(std::min(static_cast<double>(queue) / static_cast<double>(capacity_), kMaxFillRatio)) : 0.0f;

        ++frame_;

        const auto line = [](std::string label, Element value) {
            return hbox({
                text(std::move(label)) | dim | size(WIDTH, EQUAL, kLabelWidth),
                std::move(value),
                filler()
            });
        };

        Elements rows{
            hbox({
                text("port") | dim,
                text(" "),
                text(std::to_string(port_)) | bold | color(Color::Yellow),
                filler()
            }),
            separator(),
            line("processed", text(formatNumber(static_cast<double>(snapshot.processed)))),
            line("rate", text(fmt::format("{} trades/s", formatNumber(rate))) | color(Color::Green)),
            line("avg latency", text(fmt::format("{:.2f} ms", snapshot.averageLatencyUs() / kMicrosecondsPerMillisecond))),
            line("max latency", text(fmt::format("{:.2f} ms", static_cast<double>(snapshot.maxLatencyNs) / kNanosecondsPerMillisecond))),
            line("volume", text(formatNumber(static_cast<double>(snapshot.volume)))),
            hbox({
                text("queue") | dim | size(WIDTH, EQUAL, kLabelWidth),
                text(fmt::format("{} / {}", queue, capacity_)) | size(WIDTH, EQUAL, kQueueSizeWidth),
                gauge(fill) | color(fill > kWarningFillRatio ? Color::Red : Color::Green) | flex
            }),
            separator()
        };

        if (finished_.load(std::memory_order_relaxed)) {
            rows.push_back(hbox({
                text("done") | bold | color(Color::Green),
                text(fmt::format(" in {:.2f}s", elapsed)),
                filler()
            }));
        } else {
            rows.push_back(hbox({
                spinner(kSpinnerStyle, frame_),
                text(fmt::format(" {:.1f}s", elapsed)),
                filler(),
                text("Ctrl+C") | bold | color(Color::Red),
                text(" stop") | dim
            }));
        }

        return window(text(" " + name_ + " ") | bold | color(Color::Cyan), vbox(std::move(rows)));
    }
}