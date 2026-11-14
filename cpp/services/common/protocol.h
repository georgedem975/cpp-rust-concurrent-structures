#ifndef SERVICES_COMMON_PROTOCOL_H_
#define SERVICES_COMMON_PROTOCOL_H_

#include <cstddef>
#include <cstdint>

namespace service {
    struct Trade {
        std::int64_t sentNs;
        std::int64_t price;
        std::uint32_t symbol;
        std::uint32_t qty;
    };

    static_assert(sizeof(Trade) == 24, "Unexpected Trade size");

    constexpr std::size_t kBatchSize = 1024;
}

#endif