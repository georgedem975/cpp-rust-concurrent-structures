#ifndef SERVICES_COMMON_TCP_SERVER_H_
#define SERVICES_COMMON_TCP_SERVER_H_

#include "protocol.h"

#include <boost/asio.hpp>

#include <array>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace service {
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    template <typename Queue>
    class TcpServer {
    public:
        TcpServer(std::uint16_t port, Queue& queue)
            : queue_(queue), acceptor_(io_), signals_(io_, SIGINT, SIGTERM) {
            const tcp::endpoint endpoint(tcp::v4(), port);

            acceptor_.open(endpoint.protocol());
            acceptor_.set_option(tcp::acceptor::reuse_address(true));
            acceptor_.bind(endpoint);
            acceptor_.listen();

            acceptNext();
        }

        TcpServer(const TcpServer&) = delete;
        TcpServer& operator=(const TcpServer&) = delete;

        void stop() {
            asio::post(io_, [this] { io_.stop(); });
        }

        void run() {
            signals_.async_wait([this](const boost::system::error_code& error, int) {
                    if (!error) {
                        io_.stop();
                    }
                }
            );

            io_.run();
        }

    private:
        class Connection : public std::enable_shared_from_this<Connection> {
        public:
            Connection(tcp::socket socket, Queue& queue) : socket_(std::move(socket)), queue_(queue) { }

            void start() {
                read();
            }

        private:
            void read() {
                auto self = this->shared_from_this();

                asio::async_read(socket_, asio::buffer(buffer_),
                    [self](const boost::system::error_code& error, std::size_t) {
                        if (error) {
                            return;
                        }

                        for (const Trade& trade : self->buffer_) {
                            if (!self->queue_.push(trade)) {
                                return;
                            }
                        }

                        self->read();
                    }
                );
            }

            tcp::socket socket_;
            Queue& queue_;
            std::array<Trade, kBatchSize> buffer_{};
        };

        void acceptNext() {
            acceptor_.async_accept([this](const boost::system::error_code& error, tcp::socket socket) {
                    if (!error) {
                        std::make_shared<Connection>(std::move(socket), queue_)->start();
                    }

                    if (acceptor_.is_open()) {
                        acceptNext();
                    }
                }
            );
        }

        Queue& queue_;

        asio::io_context io_;
        tcp::acceptor acceptor_;
        asio::signal_set signals_;
    };
}

#endif