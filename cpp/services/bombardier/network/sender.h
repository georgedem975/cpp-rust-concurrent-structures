#ifndef BOMBARDIER_SENDER_H_
#define BOMBARDIER_SENDER_H_

#include "config/config.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

class Sender {
public:
    Sender(Endpoint endpoint, std::uint64_t seed);
    ~Sender();

    Sender(const Sender&) = delete;
    Sender& operator=(const Sender&) = delete;

    void start();
    void stop();
    void join();

    std::string name() const;
    const std::string& error() const noexcept;

    std::uint64_t sent() const noexcept;
    bool failed() const noexcept;

private:
    void run();

    Endpoint endpoint_;
    std::uint64_t seed_;

    std::atomic<bool> stop_{false};
    std::atomic<bool> failed_{false};
    std::atomic<std::uint64_t> sent_{0};

    std::thread thread_;
    std::string error_;
};

#endif