#include <cds/container/treiber_stack.h>

#include "cds_setup.h"
#include "csvReporter.h"
#include "runner.h"

namespace {
    constexpr int kInitialItems = 1024;

    using Stack = cds::container::TreiberStack<cds::gc::HP, int>;

    void executeProducer(Stack& stack, int batchSize) {
        for (int i = 0; i < batchSize; ++i) {
            stack.push(1);
        }
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

    cdsbench::Initialize();

    {
        cdsbench::AttachCurrentThread();

        Stack stack;

        for (int i = 0; i < kInitialItems; ++i) {
            stack.push(0);
        }

        bench::runGrid(
            options,
            {
                .language = "cpp",
                .primitive = "cds_stack",
                .scenario = "mixed_50_50"
            },
            [&](const bench::WorkerContext& context) {
                const int producerCount = context.totalThreads / 2;
                const bool isProducer = context.id < producerCount;

                return bench::WorkerFunction(
                    [&stack, &options, isProducer](const bench::WorkerContext& context) {
                        cdsbench::AttachCurrentThread();

                        if (isProducer) {
                            executeProducer(stack, options.execution.batch);
                        } else {
                            executeConsumer(stack, options.execution.batch, context);
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