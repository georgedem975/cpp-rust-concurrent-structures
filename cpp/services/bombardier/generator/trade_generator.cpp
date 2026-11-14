#include "trade_generator.h"

namespace {
    constexpr std::int64_t kInitialPrice = 1'500'000;
    constexpr std::int64_t kPriceStep = 1'000;
    constexpr std::int64_t kPriceTick = 10;

    constexpr std::uint64_t kLcgMultiplier = 6364136223846793005ULL;
    constexpr std::uint64_t kLcgIncrement = 1442695040888963407ULL;

    constexpr std::uint64_t kPriceVariation = 21;
    constexpr std::uint64_t kMaxQuantity = 500;

    constexpr unsigned int kLcgShift = 33;
}

TradeGenerator::TradeGenerator(std::uint64_t seed) : state_(seed) {
    initializePrices();
}

void TradeGenerator::initializePrices() {
    for (std::size_t i = 0; i < prices_.size(); ++i) {
        prices_[i] = kInitialPrice + i * kPriceStep;
    }
}

std::uint64_t TradeGenerator::next() {
    state_ = state_ * kLcgMultiplier + kLcgIncrement;
    return state_ >> kLcgShift;
}

service::Trade TradeGenerator::generateTrade(std::int64_t timestamp) {
    const std::size_t symbol = next() % prices_.size();

    const auto ticks = 
        static_cast<std::int64_t>(next() % kPriceVariation) - static_cast<std::int64_t>(kPriceVariation / 2);

    prices_[symbol] += ticks * kPriceTick;

    const auto quantity = 1 + next() % kMaxQuantity;

    return {
        timestamp,
        prices_[symbol],
        static_cast<std::uint32_t>(symbol),
        static_cast<std::uint32_t>(quantity)
    };
}

void TradeGenerator::generate(service::Trade* trades, std::size_t count, std::int64_t timestamp) {
    for (std::size_t i = 0; i < count; ++i) {
        trades[i] = generateTrade(timestamp);
    }
}