#ifndef SERVICES_COMMON_CLOCK_H_
#define SERVICES_COMMON_CLOCK_H_

#include <chrono>
#include <cstdint>

namespace service {
    inline std::int64_t nowNs() {
        using namespace std::chrono;

        return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    }
}

#endif