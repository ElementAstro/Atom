#include "timer.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include "../logger/logger.h"


namespace modern_log {

ScopedTimer::ScopedTimer(Logger* logger, std::string name, Level level)
    : logger_(logger),
      name_(std::move(name)),
      level_(level),
      enabled_(true),
      start_(std::chrono::high_resolution_clock::now()) {}

void ScopedTimer::disable() { enabled_ = false; }

void ScopedTimer::finish() {
    if (!enabled_ || !logger_)
        return;

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
    auto msg = std::format("{} took {}μs", name_, duration.count());

    logger_->log_internal(level_, msg);

    enabled_ = false;
}

std::chrono::microseconds ScopedTimer::elapsed() const {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - start_);
}

ScopedTimer::~ScopedTimer() { finish(); }

// Benchmark implementation
Benchmark::Benchmark(std::string name) : name_(std::move(name)) {}

void Benchmark::add_measurement(std::chrono::microseconds duration) {
    measurements_.push_back(duration);
}

Benchmark::Stats Benchmark::get_stats() const {
    if (measurements_.empty()) {
        return {std::chrono::microseconds{0}, std::chrono::microseconds{0},
                std::chrono::microseconds{0}, std::chrono::microseconds{0},
                0.0};
    }

    auto sorted = measurements_;
    std::ranges::sort(sorted);

    auto min = sorted.front();
    auto max = sorted.back();

    auto sum = std::accumulate(sorted.begin(), sorted.end(),
                               std::chrono::microseconds{0});
    auto avg = sum / sorted.size();

    auto median =
        sorted.size() % 2 == 0
            ? (sorted[sorted.size() / 2 - 1] + sorted[sorted.size() / 2]) / 2
            : sorted[sorted.size() / 2];

    double variance = 0.0;
    for (const auto& measurement : sorted) {
        double diff = measurement.count() - avg.count();
        variance += diff * diff;
    }
    variance /= sorted.size();
    double std_dev = std::sqrt(variance);

    return {min, max, avg, median, std_dev};
}

void Benchmark::report(Logger* logger) const {
    if (!logger || measurements_.empty())
        return;

    auto stats = get_stats();

    logger->log_internal(Level::info,
                         std::format("Benchmark Report for '{}':", name_));
    logger->log_internal(Level::info,
                         std::format("  Iterations: {}", measurements_.size()));
    logger->log_internal(Level::info,
                         std::format("  Min: {}μs", stats.min.count()));
    logger->log_internal(Level::info,
                         std::format("  Max: {}μs", stats.max.count()));
    logger->log_internal(Level::info,
                         std::format("  Avg: {}μs", stats.avg.count()));
    logger->log_internal(Level::info,
                         std::format("  Median: {}μs", stats.median.count()));
    logger->log_internal(Level::info,
                         std::format("  Std Dev: {:.2f}μs", stats.std_dev));
}

}  // namespace modern_log
