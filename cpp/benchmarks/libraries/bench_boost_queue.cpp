#include <boost/lockfree/queue.hpp>

#include "csvReporter.h"
#include "runner.h"

namespace {
    constexpr int kInitialCapacity = 1024;

    using Queue = boost::lockfree::queue<int>;

    void executeProducer(Queue& queue, int batchSize, const bench::WorkerContext& context) {
        int completedOperations = 0;

        for (int i = 0; i < batchSize; ++i) {
            while (!queue.push(1)) {
                if (context.shouldStop()) {
                    context.reportOperations(completedOperations);
                    return;
                }
            }

            ++completedOperations;
        }

        context.reportOperations(completedOperations);
    }

    void executeConsumer(Queue& queue, int batchSize, const bench::WorkerContext& context) {
        int value = 0;
        int completedOperations = 0;

        for (int i = 0; i < batchSize; ++i) {
            while (!queue.pop(value)) {
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

    Queue queue(kInitialCapacity);

    for (int i = 0; i < kInitialCapacity; ++i) {
        queue.push(0);
    }

    bench::runGrid(
        options,
        {
            .language = "cpp",
            .primitive = "boost_queue",
            .scenario = "mixed_50_50",
        },
        [&](const bench::WorkerContext& context) {
            const int producerCount = context.totalThreads / 2;
            const bool isProducer = context.id < producerCount;

            return bench::WorkerFunction(
                [&queue, &options, isProducer](const bench::WorkerContext& context) {
                    if (isProducer) {
                        executeProducer(queue, options.execution.batch, context);
                    } else {
                        executeConsumer(queue, options.execution.batch, context);
                    }
                }
            );
        },
        reporter
    );

    return 0;
}