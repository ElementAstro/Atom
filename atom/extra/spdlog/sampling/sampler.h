#pragma once

#include "../core/types.h"

#include <atomic>

namespace modern_log {

/**
 * @class LogSampler
 * @brief Advanced log sampler with intelligent sampling strategies.
 *
 * This class implements various log sampling strategies to control the rate at
 * which log messages are recorded. It supports uniform, adaptive, burst, and
 * priority-based sampling with advanced features like rate limiting and
 * intelligent system load adaptation. The sampler is thread-safe and optimized
 * for high-performance logging scenarios.
 *
 * Advanced features:
 * - Priority-based sampling (higher priority logs are less likely to be dropped)
 * - Rate limiting with token bucket algorithm
 * - Intelligent adaptive sampling based on real system metrics
 * - Burst detection and handling
 * - Statistical analysis and reporting
 */
class LogSampler {
public:
    /**
     * @brief Priority levels for priority-based sampling.
     */
    enum class Priority {
        low = 0,
        normal = 1,
        high = 2,
        critical = 3
    };

private:
    SamplingStrategy strategy_;  ///< Current sampling strategy.
    double sample_rate_;         ///< Sampling rate (fraction of logs to keep).
    std::atomic<size_t> counter_{0};  ///< Counter for processed logs.
    std::atomic<size_t> dropped_{0};  ///< Counter for dropped logs.
    mutable std::atomic<double> current_load_{
        0.0};  ///< Current system load estimate.

    // Advanced sampling features
    std::atomic<size_t> rate_limit_tokens_{100};  ///< Token bucket for rate limiting
    std::atomic<size_t> max_tokens_{100};  ///< Maximum tokens in bucket
    std::atomic<size_t> token_refill_interval_ms_{1000};  ///< Token refill interval in milliseconds

    // Priority-based sampling
    std::atomic<bool> priority_sampling_enabled_{false};
    std::array<std::atomic<double>, 4> priority_rates_{1.0, 1.0, 1.0, 1.0};  ///< Sampling rates per priority

    // Burst detection
    std::atomic<size_t> burst_threshold_{50};  ///< Messages per second to trigger burst mode

public:
    /**
     * @brief Construct a LogSampler with a given strategy and rate.
     * @param strategy The sampling strategy to use.
     * @param rate The sampling rate (default: 1.0, meaning no sampling).
     */
    explicit LogSampler(SamplingStrategy strategy = SamplingStrategy::none,
                        double rate = 1.0);

    /**
     * @brief Check whether the current log should be sampled (kept).
     *
     * This method applies the current sampling strategy and rate to decide
     * whether a log message should be recorded or dropped.
     *
     * @return True if the log should be kept, false if it should be dropped.
     */
    bool should_sample();

    /**
     * @brief Advanced sampling with priority and level consideration.
     *
     * @param level Log level for priority-based sampling.
     * @param priority Message priority (default: normal).
     * @return True if the log should be kept, false if it should be dropped.
     */
    bool should_sample_advanced(Level level, Priority priority = Priority::normal);

    /**
     * @brief Check if rate limiting allows this message.
     *
     * @return True if rate limit allows the message.
     */
    bool check_rate_limit();

    /**
     * @brief Get the number of logs that have been dropped by the sampler.
     * @return The count of dropped logs.
     */
    size_t get_dropped_count() const;

    /**
     * @brief Get the current effective sampling rate.
     *
     * This may reflect the actual rate of logs being kept, which can differ
     * from the configured rate in adaptive or burst modes.
     *
     * @return The current sampling rate as a double.
     */
    double get_current_rate() const;

    /**
     * @brief Set the sampling strategy and rate.
     * @param strategy The new sampling strategy.
     * @param rate The new sampling rate (default: 1.0).
     */
    void set_strategy(SamplingStrategy strategy, double rate = 1.0);

    /**
     * @brief Enable/disable priority-based sampling.
     * @param enabled Whether to enable priority sampling.
     */
    void set_priority_sampling(bool enabled);

    /**
     * @brief Set sampling rate for a specific priority level.
     * @param priority The priority level.
     * @param rate The sampling rate for this priority.
     */
    void set_priority_rate(Priority priority, double rate);

    /**
     * @brief Configure rate limiting.
     * @param max_tokens Maximum tokens in the bucket.
     * @param refill_interval Interval for token refill.
     */
    void set_rate_limit(size_t max_tokens, std::chrono::milliseconds refill_interval);

    /**
     * @brief Set burst detection threshold.
     * @param threshold Messages per second to trigger burst mode.
     */
    void set_burst_threshold(size_t threshold);

    /**
     * @brief Get comprehensive sampling statistics.
     * @return Tuple of (total_processed, dropped, current_rate, burst_detected).
     */
    std::tuple<size_t, size_t, double, bool> get_detailed_stats() const;

    /**
     * @brief Reset all internal statistics (counters and load).
     */
    void reset_stats();

private:
    /**
     * @brief Perform uniform sampling.
     * @return True if the log should be kept, false otherwise.
     */
    bool uniform_sample();

    /**
     * @brief Perform adaptive sampling based on system load or log rate.
     * @return True if the log should be kept, false otherwise.
     */
    bool adaptive_sample();

    /**
     * @brief Perform burst sampling for high-frequency log events.
     * @return True if the log should be kept, false otherwise.
     */
    bool burst_sample();

    /**
     * @brief Estimate the current system load for adaptive sampling.
     * @return The estimated system load as a double.
     */
    double get_system_load() const;

    /**
     * @brief Refill rate limiting tokens.
     */
    void refill_tokens() const;

    /**
     * @brief Check for burst conditions.
     * @return True if burst is detected.
     */
    bool detect_burst() const;

    /**
     * @brief Get priority-adjusted sampling rate.
     * @param priority Message priority.
     * @return Adjusted sampling rate.
     */
    double get_priority_rate(Priority priority) const;
};

}  // namespace modern_log
