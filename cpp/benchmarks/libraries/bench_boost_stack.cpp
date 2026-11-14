#include <boost/lockfree/stack.hpp>

#include "csvReporter.h"
#include "runner.h"

namespace {
    constexpr int kInitialCapacity = 1024;

    using Stack = boost::lockfree::stack<int>;

    void executeProducer(Stack& stack, int batchSize, const bench::WorkerContext& context) {
        int completedOperations = 0;

        for (int i = 0; i < batchSize; ++i) {
            while (!stack.push(1)) {
                if (context.shouldStop()) {
                    context.reportOperations(completedOperations);
                    return;
                }
            }

            ++completedOperations;
        }

        context.reportOperations(completedOperations);
    }

    void executeConsumer(Stack& stack, int batchSize, const bench::WorkerContext& context) {
        int value = 0;
        int completedOperations = 0;

        for (int i = 0; i < batchSize; ++i) {
            while (!stack.pop(value)) {
                if (context.shouldStop()) {
                    context.reportOperations(completedOperations);
                    return;
                }
            }

            ++completedOperations;
        }

        context.reportOperations(completedOperations);
    }
}

int main(int argc, char** argv) {
    const bench::Options options = bench::parseOptions(argc, argv);

    bench::CsvReporter reporter;

    Stack stack(kInitialCapacity);

    for (int i = 0; i < kInitialCapacity; ++i) {
        stack.push(0);
    }

    bench::runGrid(
        options,
        {
            .language = "cpp",
            .primitive = "boost_stack",
            .scenario = "mixed_50_50",
        },
        [&](const bench::WorkerContext& context) {
            const int producerCount = context.totalThreads / 2;
            const bool isProducer = context.id < producerCount;

            return bench::WorkerFunction(
                [&stack, &options, isProducer](const bench::WorkerContext& context) {
                    if (isProducer) {
                        executeProducer(stack, options.execution.batch, context);
                    } else {
                        executeConsumer(stack, options.execution.batch, context);
                    }
                }
            );
        },
        reporter
    );

    return 0;
}