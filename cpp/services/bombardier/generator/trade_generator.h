#ifndef BOMBARDIER_TRADE_GENERATOR_H_
#define BOMBARDIER_TRADE_GENERATOR_H_

#include "../../common/protocol.h"

#include <array>
#include <cstddef>
#include <cstdint>

class TradeGenerator {
public:
    explicit TradeGenerator(std::uint64_t seed);

    void generate(service::Trade* trades, std::size_t count, std::int64_t timestamp);

private:
    static constexpr std::size_t kSymbolCount = 8;

    void initializePrices();

    std::uint64_t next();
    service::Trade generateTrade(std::int64_t timestamp);

    std::uint64_t state_;
    std::array<std::int64_t, kSymbolCount> prices_{};
};

#endif