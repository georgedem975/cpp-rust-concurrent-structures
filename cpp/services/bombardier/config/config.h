#ifndef BOMBARDIER_CONFIG_H_
#define BOMBARDIER_CONFIG_H_

#include <cstdint>
#include <string>
#include <vector>

struct Endpoint {
    std::string host;
    std::uint16_t port = 0;

    std::string name() const {
        return host + ':' + std::to_string(port);
    }
};

struct Config {
    std::vector<Endpoint> targets;
    double duration = 10.0;
};

Config parseConfig(int argc, char** argv);

#endif