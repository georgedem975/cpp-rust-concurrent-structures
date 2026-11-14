#include "sender.h"

#include "../../common/clock.h"
#include "../../common/protocol.h"
#include "generator/trade_generator.h"

#include <boost/asio.hpp>

#include <algorithm>
#include <array>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    void connectSocket(tcp::socket& socket, const Endpoint& endpoint) {
        tcp::resolver resolver(socket.get_executor());
        const auto results = resolver.resolve(endpoint.host, std::to_string(endpoint.port));

        std::vector<tcp::endpoint> candidates;
        candidates.reserve(std::distance(results.begin(), results.end()));

        for (const auto& result : results) {
            candidates.push_back(result.endpoint());
        }

        std::stable_partition(candidates.begin(), candidates.end(),
            [](const tcp::endpoint& candidate) {
                return candidate.address().is_v4();
            }
        );

        asio::connect(socket, candidates);
    }
}

Sender::Sender(Endpoint endpoint, std::uint64_t seed) 
    : endpoint_(std::move(endpoint)), seed_(seed) { }

Sender::~Sender() {
    stop();
    join();
}

void Sender::start() {
    if (thread_.joinable()) {
        throw std::logic_error("Sender is already running");
    }

    stop_.store(false, std::memory_order_relaxed);
    failed_.store(false, std::memory_order_relaxed);
    sent_.store(0, std::memory_order_relaxed);
    error_.clear();

    thread_ = std::thread(&Sender::run, this);
}

void Sender::stop() {
    stop_.store(true, std::memory_order_relaxed);
}

void Sender::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

std::string Sender::name() const {
    return endpoint_.name();
}

const std::string& Sender::error() const noexcept {
    return error_;
}

std::uint64_t Sender::sent() const noexcept {
    return sent_.load(std::memory_order_relaxed);
}

bool Sender::failed() const noexcept {
    return failed_.load(std::memory_order_relaxed);
}

void Sender::run() {
    try {
        asio::io_context io;
        tcp::socket socket(io);

        connectSocket(socket, endpoint_);

        TradeGenerator generator(seed_);
        std::array<service::Trade, service::kBatchSize> batch{};

        while (!stop_.load(std::memory_order_relaxed)) {
            generator.generate(batch.data(), batch.size(), service::nowNs());

            asio::write(socket, asio::buffer(batch));

            sent_.fetch_add(batch.size(), std::memory_order_relaxed);
        }
    } catch (const std::exception& exception) {
        error_ = exception.what();
        failed_.store(true, std::memory_order_relaxed);
    }
}