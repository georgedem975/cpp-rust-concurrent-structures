#include "csvReporter.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <fmt/format.h>
#include <quill/Backend.h>
#include <quill/CsvWriter.h>
#include <quill/Frontend.h>
#include <quill/Utility.h>
#include <quill/sinks/ConsoleSink.h>

#include "latency.h"

namespace bench {
    namespace detail {
        constexpr int kIntegerPrecision = 0;
        constexpr int kLatencyPrecision = 3;

        struct CsvSchema {
            static constexpr char const* header =
                "language,primitive,scenario,metric,threads,repeat,batch,ops,"
                "wall_ns,throughput_ops_s,lat_avg_ns,lat_p50_ns,lat_p95_ns,"
                "lat_p99_ns,lat_p99_9_ns,lat_max_ns";

            static constexpr char const* format =
                "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}";
        };

        using CsvWriter = quill::CsvWriter<CsvSchema, quill::FrontendOptions>;

        std::string formatInteger(double value) {
            return fmt::format("{:.{}f}", value, kIntegerPrecision);
        }

        std::string formatLatency(double value) {
            return fmt::format("{:.{}f}", value, kLatencyPrecision);
        }

        void validateOutputDirectory(const std::string& path) {
            const std::filesystem::path file(path);

            if (!file.has_parent_path()) {
                return;
            }

            std::error_code error;

            if (!std::filesystem::is_directory(file.parent_path(), error)) {
                throw std::runtime_error("Output directory does not exist: " + file.parent_path().string());
            }
        }

        std::unique_ptr<CsvWriter> createWriter(const std::string& path) {
            static std::once_flag flag;
            std::call_once(flag, [] () { 
                if (!quill::Backend::is_running()) { 
                    quill::Backend::start(); 
                } 
            });

            if (path.empty()) {
                quill::ConsoleSinkConfig config;
                config.set_stream("stdout");
                auto sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("stdout", config);
                return std::make_unique<CsvWriter>("stdout", std::move(sink), true);
            }

            return std::make_unique<CsvWriter>(path, 'a');
        }

        void appendRow(CsvWriter& writer, const ThroughputResult& result) {
            const Point& point = result.point;

            writer.append_row(
                quill::utility::csv_escape_field(point.target.language),
                quill::utility::csv_escape_field(point.target.primitive),
                quill::utility::csv_escape_field(point.target.scenario),
                "throughput",
                point.config.threads,
                point.config.repeat,
                point.config.batch,
                result.run.totalOperations,
                formatInteger(result.run.elapsed.durationNs()),
                formatInteger(result.operationsPerSecond),
                std::string{}, std::string{}, std::string{},
                std::string{}, std::string{}, std::string{}
            );
        }

        void appendRow(CsvWriter& writer, const LatencyResult& result) {
            const Point& point = result.point;
            const Latency& latency = result.latency;

            writer.append_row(
                quill::utility::csv_escape_field(point.target.language),
                quill::utility::csv_escape_field(point.target.primitive),
                quill::utility::csv_escape_field(point.target.scenario),
                "latency",
                point.config.threads,
                point.config.repeat,
                point.config.batch,
                result.run.totalOperations,
                formatInteger(result.run.elapsed.durationNs()),
                std::string{},
                formatLatency(latency.meanNs()),
                formatLatency(latency.medianNs()),
                formatLatency(latency.p95Ns()),
                formatLatency(latency.p99Ns()),
                formatLatency(latency.p999Ns()),
                formatLatency(latency.maximumNs())
            );
        }

        void appendResult(CsvWriter& writer, const BenchmarkResult& result) {
            std::visit([&writer](const auto& typed) { appendRow(writer, typed); }, result);
        }
    }

    class CsvReporter::Impl {
    public:
        void emit(const Options& options, const std::vector<BenchmarkResult>& results) {
            const std::string& path = options.output.path;
            const std::lock_guard<std::mutex> lock(mutex_);
            auto it = writers_.find(path);

            if (it == writers_.end()) {
                if (!path.empty()) {
                    detail::validateOutputDirectory(path);
                    validateHeader(path);
                }

                auto writer = detail::createWriter(path);
                it = writers_.emplace(path, std::move(writer)).first;
            }

            for (const auto& result : results) {
                detail::appendResult(*it->second, result);
            }

            it->second->flush();
        }

    private:
        void validateHeader(const std::string& path) {
            std::ifstream file(path);

            if (!file) {
                if (std::ifstream(path).fail() && std::filesystem::exists(path)) {
                    throw std::runtime_error("Cannot read CSV file: " + path);
                }

                return;
            }

            std::string header;

            if (std::getline(file, header)) {
                if (header != detail::CsvSchema::header) {
                    throw std::runtime_error("CSV header does not match the current schema: " + path);
                }
            } else if (file.bad()) {
                throw std::runtime_error("Failed to read CSV header: " + path);
            }
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<detail::CsvWriter>> writers_;
        std::mutex mutex_;
    };

    CsvReporter::CsvReporter() : impl_(std::make_unique<Impl>()) {}
    CsvReporter::~CsvReporter() = default;

    void CsvReporter::emit(const Options& options, const std::vector<BenchmarkResult>& results) {
        impl_->emit(options, results);
    }
}