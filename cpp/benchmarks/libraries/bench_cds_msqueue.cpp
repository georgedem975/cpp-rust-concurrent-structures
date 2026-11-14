#include <cds/container/msqueue.h>

#include "cds_setup.h"
#include "csvReporter.h"
#include "runner.h"

namespace {
    constexpr int kInitialItems = 1024;

    using Queue = cds::container::MSQueue<cds::gc::HP, int>;

    void executeProducer(Queue& queue, int batchSize) {
        for (int i = 0; i < batchSize; ++i) {
            queue.push(1);
        }
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

    cdsbench::Initialize();

    {
        cdsbench::AttachCurrentThread();

        Queue queue;

        for (int i = 0; i < kInitialItems; ++i) {
            queue.push(0);
        }

        bench::runGrid(
            options,
            {
                .language = "cpp",
                .primitive = "cds_msqueue",
                .scenario = "mixed_50_50",
            },
            [&](const bench::WorkerContext& context) {
                const int producerCount = context.totalThreads / 2;
                const bool isProducer = context.id < producerCount;

                return bench::WorkerFunction(
                    [&queue, &options, isProducer](const bench::WorkerContext& context) {
                        cdsbench::AttachCurrentThread();

                        if (isProducer) {
                            executeProducer(queue, options.execution.batch);
                        } else {
                            executeConsumer(queue, options.execution.batch, context);
                        }
                    }
                );
            },
            reporter
        );
    }

    cdsbench::Terminate();

    return 0;
}