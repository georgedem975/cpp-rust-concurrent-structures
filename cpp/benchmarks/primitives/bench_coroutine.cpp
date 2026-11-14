#include <coroutine>
#include <exception>
#include <memory>
#include <utility>

#include "csvReporter.h"
#include "runner.h"

namespace {
    class Resumable {
    public:
        struct promise_type {
            Resumable get_return_object() noexcept {
                return Resumable{ std::coroutine_handle<promise_type>::from_promise(*this) };
            }

            std::suspend_always initial_suspend() noexcept {
                return {};
            }

            std::suspend_always final_suspend() noexcept {
                return {};
            }

            void return_void() noexcept {}

            void unhandled_exception() noexcept {
                std::terminate();
            }
        };

        Resumable(const Resumable&) = delete;
        Resumable& operator=(const Resumable&) = delete;

        Resumable(Resumable&& other) noexcept
            : handle_(std::exchange(other.handle_, nullptr)) {}

        Resumable& operator=(Resumable&& other) noexcept {
            if (this != &other) {
                if (handle_) {
                    handle_.destroy();
                }

                handle_ = std::exchange(other.handle_, nullptr);
            }

            return *this;
        }

        ~Resumable() {
            if (handle_) {
                handle_.destroy();
            }
        }

        void resume() {
            if (handle_ && !handle_.done()) {
                handle_.resume();
            }
        }

    private:
        explicit Resumable(std::coroutine_handle<promise_type> handle) noexcept
            : handle_(handle) {}

        std::coroutine_handle<promise_type> handle_;
    };

    Resumable CoopLoop() {
        for (;;) {
            co_await std::suspend_always{};
        }
    }

}

int main(int argc, char** argv) {
    const bench::Options options = bench::parseOptions(argc, argv);
    
    bench::CsvReporter reporter;

    bench::runGrid(
        options,
        {
            .language = "cpp",
            .primitive = "coroutine",
            .scenario = "await_switch"
        },
        [&options](const bench::WorkerContext&) {
            auto coroutine = std::make_shared<Resumable>(CoopLoop());

            return bench::WorkerFunction(
                [coroutine, &options](const bench::WorkerContext&) {
                    for (int i = 0; i < options.execution.batch; ++i) {
                        coroutine->resume();
                    }
                });
        },
        reporter);

    return 0;
}