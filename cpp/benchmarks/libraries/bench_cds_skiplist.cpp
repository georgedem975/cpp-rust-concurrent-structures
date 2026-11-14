#include <cds/container/skip_list_map_hp.h>
#include <cds/container/skip_list_set_hp.h>

#include <cstdint>

#include "cds_setup.h"
#include "csvReporter.h"
#include "runner.h"

namespace {
    constexpr int kKeySpace = 1'000'000;

    constexpr int kRollRange  = 10;
    constexpr int kInsertRoll = 2;
    constexpr int kReadRoll   = 9;

    constexpr std::uint32_t kMapSeed              = 0x9e3779b9u;
    constexpr std::uint32_t kSetSeed              = 0x85ebca6bu;
    constexpr std::uint32_t kThreadSeedMultiplier = 0x9e3779b9u;

    constexpr std::uint32_t kXorshiftShiftA = 13;
    constexpr std::uint32_t kXorshiftShiftB = 17;
    constexpr std::uint32_t kXorshiftShiftC = 5;
    constexpr std::uint32_t kNonZeroSeedFallback = 1u;

    inline std::uint32_t Next(std::uint32_t& state) {
        state ^= state << kXorshiftShiftA;
        state ^= state >> kXorshiftShiftB;
        state ^= state << kXorshiftShiftC;

        return state;
    }

    inline int Key(std::uint32_t& state) {
        return static_cast<int>(Next(state) % kKeySpace);
    }

    inline std::uint32_t MakeThreadSeed(std::uint32_t baseSeed, bench::ThreadId threadId) {
        std::uint32_t seed =
            baseSeed ^ (static_cast<std::uint32_t>(threadId + 1) * kThreadSeedMultiplier);

        return seed != 0 ? seed : kNonZeroSeedFallback;
    }

    template <typename Map>
    void executeMapBatch(Map& map, int batchSize, std::uint32_t& state) {
        for (int i = 0; i < batchSize; ++i) {
            const int key = Key(state);
            const int roll = static_cast<int>(Next(state) % kRollRange);

            if (roll < kInsertRoll) {
                map.insert(key, key);
            } else if (roll < kReadRoll) {
                map.find(key, [](std::pair<int const, int> const&) {});
            } else {
                map.erase(key);
            }
        }
    }

    template <typename Set>
    void executeSetBatch(Set& set, int batchSize, std::uint32_t& state) {
        for (int i = 0; i < batchSize; ++i) {
            const int key = Key(state);
            const int roll = static_cast<int>(Next(state) % kRollRange);

            if (roll < kInsertRoll) {
                set.insert(key);
            } else if (roll < kReadRoll) {
                set.contains(key);
            } else {
                set.erase(key);
            }
        }
    }
}

int main(int argc, char** argv) {
    const bench::Options options = bench::parseOptions(argc, argv);

    bench::CsvReporter reporter;

    cdsbench::Initialize();

    {
        using Map = cds::container::SkipListMap<cds::gc::HP, int, int>;
        using Set = cds::container::SkipListSet<cds::gc::HP, int>;

        cdsbench::AttachCurrentThread();

        Map map;
        Set set;

        for (int key = 0; key < kKeySpace; ++key) {
            map.insert(key, key);
            set.insert(key);
        }

        bench::runGrid(
            options,
            {
                .language = "cpp",
                .primitive = "cds_skiplist_map",
                .scenario = "mix_20_70_10",
            },
            [&](const bench::WorkerContext&) {
                return bench::WorkerFunction(
                    [&map, &options](const bench::WorkerContext& context) {
                        cdsbench::AttachCurrentThread();

                        std::uint32_t state = MakeThreadSeed(kMapSeed, context.id);

                        executeMapBatch(map, options.execution.batch, state);
                    }
                );
            },
            reporter
        );

        bench::runGrid(
            options,
            {
                .language = "cpp",
                .primitive = "cds_skiplist_set",
                .scenario = "mix_20_70_10",
            },
            [&](const bench::WorkerContext&) {
                return bench::WorkerFunction(
                    [&set, &options](const bench::WorkerContext& context) {
                        cdsbench::AttachCurrentThread();

                        std::uint32_t state = MakeThreadSeed(kSetSeed, context.id);

                        executeSetBatch(set, options.execution.batch, state);
                    }
                );
            },
            reporter
        );
    }

    cdsbench::Terminate();

    return 0;
}