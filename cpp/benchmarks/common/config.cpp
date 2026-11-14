#include "config.h"

#include <cstdlib>

#include <CLI/CLI.hpp>

namespace {
    constexpr int kMaxRepeats = 1'000'000;
    constexpr int kMaxBatch = 1'000'000;
}

namespace bench {
    Options parseOptions(int argc, char** argv) {
        Options options;

        CLI::App app("Measurements of synchronization primitives");

        app.add_option("--threads", options.experiment.threads)
            ->delimiter(',')
            ->expected(1, -1)
            ->check(CLI::PositiveNumber)
            ->capture_default_str();

        app.add_option("--duration", options.execution.duration)
            ->check(CLI::PositiveNumber)
            ->capture_default_str();

        app.add_option("--repeats", options.experiment.repeats)
            ->check(CLI::Range(1, kMaxRepeats))
            ->capture_default_str();

        app.add_option("--batch", options.execution.batch)
            ->check(CLI::Range(1, kMaxBatch))
            ->capture_default_str();

        app.add_option("--warmup", options.execution.warmup)
            ->check(CLI::NonNegativeNumber)
            ->capture_default_str();

        app.add_option("--out", options.output.path);

        app.add_flag("--throughput", options.metrics.throughput, "Measure throughput");
        
        app.add_flag("--latency", options.metrics.latency, "Measure latency");

        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& error) {
            std::exit(app.exit(error));
        }

        return options;
    }
}