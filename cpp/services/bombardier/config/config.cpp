#include "config.h"

#include <CLI/CLI.hpp>

#include <charconv>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {
    std::optional<Endpoint> parseEndpoint(const std::string& value) {
        const auto separator = value.rfind(':');

        if (separator == std::string::npos || separator == 0 || separator == value.size() - 1) {
            return std::nullopt;
        }

        const char* begin = value.data() + separator + 1;
        const char* end = value.data() + value.size();

        unsigned int port = 0;
        const auto result = std::from_chars(begin, end, port);

        if (result.ec != std::errc{} || result.ptr != end || port == 0 || port > 65535) {
            return std::nullopt;
        }

        return Endpoint { value.substr(0, separator), static_cast<std::uint16_t>(port) };
    }

    std::vector<Endpoint> parseEndpoints(const std::vector<std::string>& values) {
        std::vector<Endpoint> endpoints;
        endpoints.reserve(values.size());

        for (const auto& value : values) {
            const auto endpoint = parseEndpoint(value);

            if (!endpoint) {
                throw std::logic_error("Invalid endpoint: " + value);
            }

            endpoints.push_back(*endpoint);
        }

        return endpoints;
    }
}

Config parseConfig(int argc, char** argv) {
    Config config;
    std::vector<std::string> targetValues;

    CLI::App app{"Trade load generator", "bombardier"};

    app.footer(
        "Example: bombardier "
        "--target 127.0.0.1:9001 "
        "--target 127.0.0.1:9002 "
        "--duration 10"
    );

    app.add_option("--target", targetValues, "Service endpoint as host:port")
        ->delimiter(',')
        ->expected(1, -1)
        ->required()
        ->check([](const std::string& value) {
            return parseEndpoint(value) ? std::string{} : "Invalid endpoint: " + value;
        });

    app.add_option("--duration", config.duration, "Duration in seconds (0 = unlimited)")
        ->check(CLI::NonNegativeNumber)
        ->capture_default_str();

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& error) {
        std::exit(app.exit(error));
    }

    config.targets = parseEndpoints(targetValues);

    return config;
}