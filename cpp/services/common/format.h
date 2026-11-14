#ifndef SERVICES_COMMON_FORMAT_H_
#define SERVICES_COMMON_FORMAT_H_

#include <fmt/format.h>

#include <string>

namespace service {
    inline constexpr double kMillion = 1e6;
    inline constexpr double kThousand = 1e3;

    inline std::string formatNumber(double value) {
        if (value >= kMillion) {
            return fmt::format("{:.2f}M", value / kMillion);
        }

        if (value >= kThousand) {
            return fmt::format("{:.1f}k", value / kThousand);
        }

        return fmt::format("{:.0f}", value);
    }
}

#endif