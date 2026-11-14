#include "latency.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace bench {
    namespace {
        constexpr double kMedianFraction = 0.50;
        constexpr double kP95Fraction = 0.95;
        constexpr double kP99Fraction = 0.99;
        constexpr double kP999Fraction = 0.999;

        double percentile(const std::vector<double>& sortedSamples, double fraction) {
            const double position = fraction * static_cast<double>(sortedSamples.size() - 1);

            const auto lower = static_cast<std::size_t>(position);
            const auto upper = std::min(lower + 1, sortedSamples.size() - 1);

            const double weight = position - static_cast<double>(lower);

            return sortedSamples[lower] * (1.0 - weight) + sortedSamples[upper] * weight;
        }
    }

    Latency::Latency(
        double meanNs,
        double medianNs,
        double p95Ns,
        double p99Ns,
        double p999Ns,
        double maximumNs
    )
        : meanNs_(meanNs),
        medianNs_(medianNs),
        p95Ns_(p95Ns),
        p99Ns_(p99Ns),
        p999Ns_(p999Ns),
        maximumNs_(maximumNs) {}

    Latency Latency::fromSamples(std::vector<double> samplesNs) {
        if (samplesNs.empty()) {
            throw std::invalid_argument("Latency samples cannot be empty");
        }

        for (const double sample : samplesNs) {
            if (!std::isfinite(sample) || sample < 0.0) {
                throw std::invalid_argument("Latency samples must be finite and non-negative");
            }
        }

        std::sort(samplesNs.begin(), samplesNs.end());

        long double sum = 0.0L;
        for (const double sample : samplesNs) {
            sum += static_cast<long double>(sample);
        }

        const auto count = static_cast<long double>(samplesNs.size());
        const double meanNs = static_cast<double>(sum / count);

        return Latency(
            meanNs,
            percentile(samplesNs, kMedianFraction),
            percentile(samplesNs, kP95Fraction),
            percentile(samplesNs, kP99Fraction),
            percentile(samplesNs, kP999Fraction),
            samplesNs.back()
        );
    }

    double Latency::meanNs() const noexcept {
        return meanNs_;
    }

    double Latency::medianNs() const noexcept {
        return medianNs_;
    }

    double Latency::p95Ns() const noexcept {
        return p95Ns_;
    }

    double Latency::p99Ns() const noexcept {
        return p99Ns_;
    }

    double Latency::p999Ns() const noexcept {
        return p999Ns_;
    }

    double Latency::maximumNs() const noexcept {
        return maximumNs_;
    }
}