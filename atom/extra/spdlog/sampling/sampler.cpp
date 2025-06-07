#include "sampler.h"

#include <random>

namespace modern_log {

LogSampler::LogSampler(SamplingStrategy strategy, double rate)
    : strategy_(strategy), sample_rate_(rate) {
    if (rate < 0.0 || rate > 1.0) {
        sample_rate_ = 1.0;
    }
}

bool LogSampler::should_sample() {
    switch (strategy_) {
        case SamplingStrategy::none:
            return true;
        case SamplingStrategy::uniform:
            return uniform_sample();
        case SamplingStrategy::adaptive:
            return adaptive_sample();
        case SamplingStrategy::burst:
            return burst_sample();
    }
    return true;
}

size_t LogSampler::get_dropped_count() const { return dropped_.load(); }

double LogSampler::get_current_rate() const {
    if (strategy_ == SamplingStrategy::adaptive) {
        double load = current_load_.load();
        return sample_rate_ * (1.0 - load);
    }
    return sample_rate_;
}

void LogSampler::set_strategy(SamplingStrategy strategy, double rate) {
    strategy_ = strategy;
    if (rate >= 0.0 && rate <= 1.0) {
        sample_rate_ = rate;
    }
}

void LogSampler::reset_stats() {
    counter_.store(0);
    dropped_.store(0);
}

bool LogSampler::uniform_sample() {
    if (sample_rate_ >= 1.0)
        return true;
    if (sample_rate_ <= 0.0) {
        dropped_.fetch_add(1);
        return false;
    }

    size_t current = counter_.fetch_add(1);
    bool should_log = (current % static_cast<size_t>(1.0 / sample_rate_)) == 0;

    if (!should_log) {
        dropped_.fetch_add(1);
    }

    return should_log;
}

bool LogSampler::adaptive_sample() {
    double load = get_system_load();
    current_load_.store(load);

    double adjusted_rate = sample_rate_ * (1.0 - load);
    if (adjusted_rate <= 0.0) {
        dropped_.fetch_add(1);
        return false;
    }

    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_real_distribution<> dis(0.0, 1.0);

    bool should_log = dis(gen) < adjusted_rate;
    if (!should_log) {
        dropped_.fetch_add(1);
    }

    return should_log;
}

bool LogSampler::burst_sample() {
    static thread_local size_t burst_counter = 0;
    static thread_local auto last_burst = std::chrono::steady_clock::now();

    auto now = std::chrono::steady_clock::now();
    if (now - last_burst > std::chrono::seconds(1)) {
        burst_counter = 0;
        last_burst = now;
    }

    size_t max_burst =
        static_cast<size_t>(sample_rate_ * 10);
    bool should_log = burst_counter++ < max_burst;

    if (!should_log) {
        dropped_.fetch_add(1);
    }

    return should_log;
}

double LogSampler::get_system_load() const {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_real_distribution<> dis(0.0, 1.0);

    return dis(gen) * 0.5;
}

}  // namespace modern_log