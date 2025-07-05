#pragma once

#include "../core/types.h"

#include <atomic>

namespace modern_log {

/**
 * @class LogSampler
 * @brief Log sampler for controlling log recording frequency.
 *
 * This class implements various log sampling strategies to control the rate at
 * which log messages are recorded. It supports uniform, adaptive, and burst
 * sampling, and provides statistics on dropped logs and current sampling rate.
 * The sampler is thread-safe.
 */
class LogSampler {
private:
    SamplingStrategy strategy_;  ///< Current sampling strategy.
    double sample_rate_;         ///< Sampling rate (fraction of logs to keep).
    std::atomic<size_t> counter_{0};  ///< Counter for processed logs.
    std::atomic<size_t> dropped_{0};  ///< Counter for dropped logs.
    mutable std::atomic<double> current_load_{
        0.0};  ///< Current system load estimate.

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
};

}  // namespace modern_log
