#ifndef BOMBARDIER_PROGRESS_REPORTER_H_
#define BOMBARDIER_PROGRESS_REPORTER_H_

#include "config/config.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

struct SenderStatistics {
    std::string name;
    std::uint64_t sent = 0;
    bool failed = false;
};

class ProgressReporter {
public:
    using Clock = std::chrono::steady_clock;

    ProgressReporter(const std::vector<Endpoint>& targets, double duration, Clock::time_point started);

    void run();
    void update(const std::vector<SenderStatistics>& statistics);
    void finish(const std::vector<SenderStatistics>& statistics);

private:
    struct SenderProgress {
        std::string name;
        std::uint64_t sent = 0;
        double rate = 0.0;
        bool failed = false;
    };

    void refresh(const std::vector<SenderStatistics>& statistics);
    ftxui::Element render();

    ftxui::Element renderHeader() const;
    ftxui::Element renderTableHeader() const;
    ftxui::Element renderSender(const SenderProgress& sender, std::size_t index) const;
    ftxui::Element renderTotal() const;
    ftxui::Element renderStatus() const;

    std::string targetsLabel_;
    double duration_;
    Clock::time_point started_;
    ftxui::ScreenInteractive screen_;

    std::mutex mutex_;
    std::vector<SenderProgress> progress_;
    std::uint64_t totalSent_ = 0;
    double totalRate_ = 0.0;
    double elapsed_ = 0.0;
    std::size_t frame_ = 0;
    bool finished_ = false;
};

#endif