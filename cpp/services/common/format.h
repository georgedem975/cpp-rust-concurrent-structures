#ifndef SERVICES_COMMON_FORMAT_H_
#define SERVICES_COMMON_FORMAT_H_

#include <fmt/format.h>

#include <string>

namespace service {
    inline std::string formatNumber(double value) {
        if (value >= 1e6) {
            return fmt::format("{:.2f}M", value / 1e6);
        }

        if (value >= 1e3) {
            return fmt::format("{:.1f}k", value / 1e3);
        }

        return fmt::format("{:.0f}", value);
    }
}

#endif