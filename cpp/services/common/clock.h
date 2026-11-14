#ifndef SERVICES_COMMON_CLOCK_H_
#define SERVICES_COMMON_CLOCK_H_

#include <chrono>
#include <cstdint>

namespace service {
    inline constexpr double kNanosecondsPerMicrosecond = 1000.0;
    inline constexpr double kMicrosecondsPerMillisecond = 1000.0;
    inline constexpr double kNanosecondsPerMillisecond = 1000000.0;

    inline std::int64_t nowNs() {
        using namespace std::chrono;

        return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    }
}

#endif