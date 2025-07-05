#pragma once

#include <chrono>
#include <string>
#include <vector>
#include "../core/types.h"

namespace modern_log {

// Forward declaration
class Logger;

/**
 * @class ScopedTimer
 * @brief Performance timer for RAII-style timing and logging.
 *
 * This class measures the elapsed time between its construction and destruction
 * (or manual finish), and logs the result using the provided Logger. It is
 * intended for easy performance monitoring of code scopes or functions. The
 * timer can be disabled or finished manually. Not copyable or movable.
 */
class ScopedTimer {
private:
    Logger* logger_;    ///< Pointer to the logger instance.
    std::string name_;  ///< Name or label for the timed scope.
    std::chrono::high_resolution_clock::time_point
        start_;     ///< Start time point.
    Level level_;   ///< Log level for reporting the timing result.
    bool enabled_;  ///< Whether the timer is currently enabled.

public:
    /**
     * @brief Construct a ScopedTimer.
     * @param logger Pointer to the logger to use for reporting.
     * @param name Name or label for the timed scope.
     * @param level Log level for the timing result (default: Level::info).
     */
    ScopedTimer(Logger* logger, std::string name, Level level = Level::info);

    /**
     * @brief Disable the timer (no timing or logging will occur).
     */
    void disable();

    /**
     * @brief Manually finish the timer and log the elapsed time.
     *
     * If the timer is enabled, this logs the elapsed time and disables the
     * timer. If already finished or disabled, this has no effect.
     */
    void finish();

    /**
     * @brief Get the elapsed time since construction (in microseconds).
     * @return Duration in microseconds.
     */
    std::chrono::microseconds elapsed() const;

    /**
     * @brief Destructor. Logs the elapsed time if the timer is still enabled.
     */
    ~ScopedTimer();

    // Non-copyable and non-movable
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;
};

/**
 * @class Benchmark
 * @brief Performance benchmarking utility for repeated measurements.
 *
 * This class allows running a function multiple times, recording the duration
 * of each run, and computing statistical summaries (min, max, average, median,
 * standard deviation). Results can be reported via a Logger.
 */
class Benchmark {
private:
    std::string name_;  ///< Name or label for the benchmark.
    std::vector<std::chrono::microseconds>
        measurements_;  ///< List of recorded durations.

public:
    /**
     * @brief Construct a Benchmark with a given name.
     * @param name Name or label for the benchmark.
     */
    explicit Benchmark(std::string name);

    /**
     * @brief Add a measurement result to the benchmark.
     * @param duration Duration of a single run (in microseconds).
     */
    void add_measurement(std::chrono::microseconds duration);

    /**
     * @brief Run the benchmark by executing a function multiple times.
     *
     * Measures the execution time of the provided function for the specified
     * number of iterations, and records each duration.
     *
     * @tparam Func The callable type to benchmark.
     * @param func The function to execute.
     * @param iterations Number of times to run the function (default: 1000).
     */
    template <typename Func>
    void run(Func&& func, size_t iterations = 1000) {
        measurements_.reserve(iterations);
        for (size_t i = 0; i < iterations; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            func();
            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);
            measurements_.push_back(duration);
        }
    }

    /**
     * @brief Statistical summary of benchmark results.
     */
    struct Stats {
        std::chrono::microseconds min;     ///< Minimum duration.
        std::chrono::microseconds max;     ///< Maximum duration.
        std::chrono::microseconds avg;     ///< Average duration.
        std::chrono::microseconds median;  ///< Median duration.
        double std_dev;                    ///< Standard deviation.
    };

    /**
     * @brief Compute statistics for the benchmark measurements.
     * @return Stats structure containing summary statistics.
     */
    Stats get_stats() const;

    /**
     * @brief Output a benchmark report using the provided Logger.
     * @param logger Pointer to the logger to use for reporting.
     */
    void report(Logger* logger) const;
};

}  // namespace modern_log
