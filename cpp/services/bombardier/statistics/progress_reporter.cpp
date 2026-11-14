#include "progress_reporter.h"

#include "../../common/format.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <utility>

namespace {
    constexpr int kNameWidth = 22;
    constexpr int kNumberWidth = 12;
    constexpr int kShareWidth = 8;
    constexpr int kStatusWidth = 15;
    constexpr int kSpinnerStyle = 13;
    constexpr double kPercentFactor = 100.0;
}

using service::formatNumber;

ProgressReporter::ProgressReporter(const std::vector<Endpoint>& targets, double duration, Clock::time_point started)
    : duration_(duration), started_(started), screen_(ftxui::ScreenInteractive::TerminalOutput()) {
    for (const Endpoint& target : targets) {
        targetsLabel_ += (targetsLabel_.empty() ? "" : ", ") + target.name();
    }
}

void ProgressReporter::run() {
    screen_.Loop(ftxui::Renderer([this] { return render(); }));
}

void ProgressReporter::update(const std::vector<SenderStatistics>& statistics) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        refresh(statistics);
        ++frame_;
    }

    screen_.PostEvent(ftxui::Event::Custom);
}

void ProgressReporter::finish(const std::vector<SenderStatistics>& statistics) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        refresh(statistics);
        finished_ = true;
    }

    screen_.PostEvent(ftxui::Event::Custom);
    screen_.Exit();
}

void ProgressReporter::refresh(const std::vector<SenderStatistics>& statistics) {
    elapsed_ = std::chrono::duration<double>(Clock::now() - started_).count();

    progress_.resize(statistics.size());

    totalSent_ = 0;
    totalRate_ = 0.0;

    for (std::size_t i = 0; i < statistics.size(); ++i) {
        SenderProgress& entry = progress_[i];

        entry.name = statistics[i].name;
        entry.sent = statistics[i].sent;
        entry.failed = statistics[i].failed;
        entry.rate = elapsed_ > 0.0 ? static_cast<double>(entry.sent) / elapsed_ : 0.0;

        totalSent_ += entry.sent;
        totalRate_ += entry.rate;
    }
}


ftxui::Element ProgressReporter::render() {
    using namespace ftxui;

    std::lock_guard<std::mutex> lock(mutex_);

    Elements rows {
        renderHeader(),
        separator(),
        renderTableHeader()
    };

    for (std::size_t i = 0; i < progress_.size(); ++i) {
        rows.push_back(renderSender(progress_[i], i));
    }

    rows.push_back(separator());
    rows.push_back(renderTotal());
    rows.push_back(separator());
    rows.push_back(renderStatus());

    return window(
        text(" bombardier ") | bold | color(Color::Cyan),
        vbox(std::move(rows))
    );
}

ftxui::Element ProgressReporter::renderHeader() const {
    using namespace ftxui;

    return hbox({
        text("targets") | dim,
        text(" "),
        text(targetsLabel_) | bold | color(Color::Yellow) | flex_shrink,
        filler(),
        text(duration_ > 0.0 ? fmt::format("duration {:.1f}s", duration_) : "unlimited") | dim
    });
}

ftxui::Element ProgressReporter::renderTableHeader() const {
    using namespace ftxui;

    return hbox({
        text("sender") | bold | size(WIDTH, EQUAL, kNameWidth),
        text("msg/s") | bold | size(WIDTH, EQUAL, kNumberWidth) | align_right,
        text("sent") | bold | size(WIDTH, EQUAL, kNumberWidth) | align_right,
        text("share") | bold | size(WIDTH, EQUAL, kShareWidth) | align_right,
        filler()
    });
}

ftxui::Element ProgressReporter::renderSender(const SenderProgress& sender, std::size_t index) const {
    using namespace ftxui;

    const double share = 
        totalSent_ > 0 ? kPercentFactor * static_cast<double>(sender.sent) / totalSent_ : 0.0;

    const auto rate = 
        sender.failed ? text("failed") | color(Color::Red) : text(formatNumber(sender.rate)) | color(Color::Green);

    return hbox({
        text(sender.name)
            | color(index % 2 == 0 ? Color::Cyan : Color::Magenta)
            | size(WIDTH, EQUAL, kNameWidth),

        rate
            | size(WIDTH, EQUAL, kNumberWidth)
            | align_right,

        text(formatNumber(static_cast<double>(sender.sent)))
            | size(WIDTH, EQUAL, kNumberWidth)
            | align_right,

        text(fmt::format("{:.0f}%", share))
            | size(WIDTH, EQUAL, kShareWidth)
            | align_right,

        filler()
    });
}

ftxui::Element ProgressReporter::renderTotal() const {
    using namespace ftxui;

    return hbox({
        text("total")
            | bold
            | size(WIDTH, EQUAL, kNameWidth),

        text(formatNumber(totalRate_))
            | bold
            | color(Color::Yellow)
            | size(WIDTH, EQUAL, kNumberWidth)
            | align_right,

        text(formatNumber(static_cast<double>(totalSent_)))
            | bold
            | size(WIDTH, EQUAL, kNumberWidth)
            | align_right,

        filler()
    });
}

ftxui::Element ProgressReporter::renderStatus() const {
    using namespace ftxui;

    Elements status;

    if (finished_) {
        status = {
            text("done") | bold | color(Color::Green),
            text(fmt::format(" in {:.2f}s", elapsed_)),
            filler()
        };
    } else if (duration_ > 0.0) {
        status = {
            text(fmt::format("{:.1f}/{:.1f}s", elapsed_, duration_))
                | size(WIDTH, EQUAL, kStatusWidth),

            gauge(static_cast<float>(
                std::clamp(elapsed_ / duration_, 0.0, 1.0)
            )) | color(Color::Green) | flex
        };
    } else {
        status = {
            spinner(kSpinnerStyle, frame_),
            text(fmt::format(" {:.1f}s", elapsed_)),
            filler()
        };
    }

    if (!finished_) {
        status.push_back(text("Ctrl+C") | bold | color(Color::Red));
        status.push_back(text(" stop") | dim);
    }

    return hbox(std::move(status));
}