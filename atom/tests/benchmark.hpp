#ifndef ATOM_TESTS_BENCHMARK_HPP
#define ATOM_TESTS_BENCHMARK_HPP

#include <atomic>
#include <chrono>
#include <concepts>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <numeric>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "atom/macro.hpp"

/**
 * @brief Class for benchmarking code performance.
 *
 * @example
 * // Basic usage example
 * Benchmark bench("MySuite", "FastAlgorithm");
 * bench.run(
 *     []() { return std::vector<int>{1, 2, 3}; },  // Setup
 *     [](auto& data) { std::sort(data.begin(), data.end()); return 1; }, //
 * Test function
 *     [](auto&) {} // Teardown
 * );
 *
 * // With custom configuration
 * Benchmark::Config config;
 * config.minIterations = 100;
 * config.minDurationSec = 2.0;
 * config.format = Benchmark::ExportFormat::Json; // Corrected member name
 * config.logLevel = Benchmark::LogLevel::Verbose;
 *
 * Benchmark bench("MySuite", "ComplexAlgorithm", config);
 * bench.run(...);
 *
 * // Export results
 * Benchmark::exportResults("benchmark_results.json");
 */
class Benchmark {
private:
    // Forward declare nested types used before their definition
    struct Result;

public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;
    using Nanoseconds = std::chrono::nanoseconds;
    using Microseconds = std::chrono::microseconds;
    using Milliseconds = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;

    /**
     * @brief Log level for benchmark operations
     */
    enum class LogLevel {
        Silent,   ///< No logging
        Minimal,  ///< Only critical information
        Normal,   ///< Standard logging (default)
        Verbose   ///< Detailed logging including iterations
    };

    /**
     * @brief Export format options
     */
    enum class ExportFormat {
        Json,      ///< JSON format
        Csv,       ///< CSV format
        Markdown,  ///< Markdown table format
        PlainText  ///< Human-readable text format
    };

    /**
     * @brief Memory usage statistics.
     */
    struct MemoryStats {
        size_t currentUsage = 0;  ///< Current memory usage.
        size_t peakUsage = 0;     ///< Peak memory usage.

        MemoryStats() noexcept = default;

        /**
         * @brief Constructor with explicit initialization values
         * @param current Current memory usage
         * @param peak Peak memory usage
         */
        MemoryStats(size_t current, size_t peak) noexcept
            : currentUsage(current), peakUsage(peak) {}

        /**
         * @brief Calculate memory stats difference
         * @param other Other memory stats to subtract
         * @return Difference between this and other
         */
        [[nodiscard]] MemoryStats diff(
            const MemoryStats& other) const noexcept {
            return {
                currentUsage > other.currentUsage
                    ? currentUsage - other.currentUsage
                    : 0,
                peakUsage > other.peakUsage ? peakUsage - other.peakUsage : 0};
        }
    } ATOM_ALIGNAS(16);

    /**
     * @brief CPU usage statistics.
     */
    struct CPUStats {
        int64_t instructionsExecuted = 0;  ///< Number of instructions executed.
        int64_t cyclesElapsed = 0;         ///< Number of CPU cycles elapsed.
        int64_t branchMispredictions = 0;  ///< Number of branch mispredictions.
        int64_t cacheMisses = 0;           ///< Number of cache misses.

        CPUStats() noexcept = default;

        /**
         * @brief Calculate CPU stats difference
         * @param other Other CPU stats to subtract
         * @return Difference between this and other
         */
        [[nodiscard]] CPUStats diff(const CPUStats& other) const noexcept {
            CPUStats result;
            result.instructionsExecuted =
                instructionsExecuted > other.instructionsExecuted
                    ? instructionsExecuted - other.instructionsExecuted
                    : 0;
            result.cyclesElapsed = cyclesElapsed > other.cyclesElapsed
                                       ? cyclesElapsed - other.cyclesElapsed
                                       : 0;
            result.branchMispredictions =
                branchMispredictions > other.branchMispredictions
                    ? branchMispredictions - other.branchMispredictions
                    : 0;
            result.cacheMisses = cacheMisses > other.cacheMisses
                                     ? cacheMisses - other.cacheMisses
                                     : 0;
            return result;
        }

        /**
         * @brief Calculate instructions per cycle (IPC)
         * @return IPC value or std::nullopt if not available
         */
        [[nodiscard]] std::optional<double> getIPC() const noexcept {
            if (cyclesElapsed <= 0)
                return std::nullopt;
            return static_cast<double>(instructionsExecuted) / cyclesElapsed;
        }
    } ATOM_ALIGNAS(32);

    /**
     * @brief Configuration settings for the benchmark.
     */
    class Config {
    public:
        int minIterations = 10;       ///< Minimum number of iterations.
        double minDurationSec = 1.0;  ///< Minimum duration in seconds.
        bool async = false;           ///< Run benchmark asynchronously.
        bool warmup = true;           ///< Perform a warmup run.
        ExportFormat format =
            ExportFormat::Json;  ///< Format for exporting results.
        LogLevel logLevel = LogLevel::Normal;  ///< Logging verbosity
        bool enableCpuStats = true;     ///< Enable CPU statistics collection
        bool enableMemoryStats = true;  ///< Enable memory usage tracking
        std::optional<size_t> maxIterations =
            std::nullopt;  ///< Maximum iterations (optional)
        std::optional<double> maxDurationSec =
            std::nullopt;  ///< Maximum duration in seconds (optional)
        std::optional<std::function<void(const std::string&)>> customLogger =
            std::nullopt;  ///< Custom logging function
        std::optional<std::string> outputFilePath =
            std::nullopt;  ///< Auto-export results to this path when done

        /**
         * @brief Create a default configuration
         */
        // Explicit default constructor to potentially resolve compiler issues
        // with default arguments
        Config()
            : minIterations(10),
              minDurationSec(1.0),
              async(false),
              warmup(true),
              format(ExportFormat::Json),
              logLevel(LogLevel::Normal),
              enableCpuStats(true),
              enableMemoryStats(true),
              maxIterations(std::nullopt),
              maxDurationSec(std::nullopt),
              customLogger(std::nullopt),
              outputFilePath(std::nullopt) {}

        /**
         * @brief Set minimum iterations
         * @param iters Minimum iterations
         * @return Reference to this config for method chaining
         */
        Config& withMinIterations(int iters) noexcept {
            minIterations = iters;
            return *this;
        }

        /**
         * @brief Set minimum duration
         * @param seconds Minimum duration in seconds
         * @return Reference to this config for method chaining
         */
        Config& withMinDuration(double seconds) noexcept {
            minDurationSec = seconds;
            return *this;
        }

        /**
         * @brief Set async mode
         * @param value True to run benchmarks asynchronously
         * @return Reference to this config for method chaining
         */
        Config& withAsync(bool value = true) noexcept {
            async = value;
            return *this;
        }

        /**
         * @brief Set warmup mode
         * @param value True to perform a warmup run
         * @return Reference to this config for method chaining
         */
        Config& withWarmup(bool value = true) noexcept {
            warmup = value;
            return *this;
        }

        /**
         * @brief Set export format
         * @param exportFormat Format to use for exporting results
         * @return Reference to this config for method chaining
         */
        Config& withFormat(ExportFormat exportFormat) noexcept {
            format = exportFormat;
            return *this;
        }

        /**
         * @brief Set log level
         * @param level Logging verbosity level
         * @return Reference to this config for method chaining
         */
        Config& withLogLevel(LogLevel level) noexcept {
            logLevel = level;
            return *this;
        }

        /**
         * @brief Set maximum iterations
         * @param iters Maximum iterations
         * @return Reference to this config for method chaining
         */
        Config& withMaxIterations(size_t iters) noexcept {
            maxIterations = iters;
            return *this;
        }

        /**
         * @brief Set maximum duration
         * @param seconds Maximum duration in seconds
         * @return Reference to this config for method chaining
         */
        Config& withMaxDuration(double seconds) noexcept {
            maxDurationSec = seconds;
            return *this;
        }

        /**
         * @brief Set custom logger
         * @param logger Function to handle log messages
         * @return Reference to this config for method chaining
         */
        Config& withCustomLogger(
            std::function<void(const std::string&)> logger) noexcept {
            customLogger = std::move(logger);
            return *this;
        }

        /**
         * @brief Enable or disable CPU statistics
         * @param enable True to enable CPU statistics
         * @return Reference to this config for method chaining
         */
        Config& withCpuStats(bool enable = true) noexcept {
            enableCpuStats = enable;
            return *this;
        }

        /**
         * @brief Enable or disable memory statistics
         * @param enable True to enable memory statistics
         * @return Reference to this config for method chaining
         */
        Config& withMemoryStats(bool enable = true) noexcept {
            enableMemoryStats = enable;
            return *this;
        }

        /**
         * @brief Set auto-export file path
         * @param path Path where results will be automatically exported
         * @return Reference to this config for method chaining
         */
        Config& withAutoExport(std::string path) noexcept {
            outputFilePath = std::move(path);
            return *this;
        }

        /**
         * @brief Convert string to export format enum
         * @param format Format name ("json", "csv", "markdown", "text")
         * @return ExportFormat enum value
         * @throws std::invalid_argument if format is invalid
         */
        static ExportFormat parseFormat(std::string_view format) {
            if (format == "json")
                return ExportFormat::Json;
            if (format == "csv")
                return ExportFormat::Csv;
            if (format == "markdown" || format == "md")
                return ExportFormat::Markdown;
            if (format == "text" || format == "txt")
                return ExportFormat::PlainText;
            throw std::invalid_argument("Invalid export format: " +
                                        std::string(format));
        }

        /**
         * @brief Convert export format enum to string
         * @param format ExportFormat enum value
         * @return String representation of the format
         */
        static std::string formatToString(ExportFormat format) noexcept {
            switch (format) {
                case ExportFormat::Json:
                    return "json";
                case ExportFormat::Csv:
                    return "csv";
                case ExportFormat::Markdown:
                    return "markdown";
                case ExportFormat::PlainText:
                    return "text";
                default:
                    // Should not happen, but handle defensively
                    return "unknown";
            }
        }
    } ATOM_ALIGNAS(64);

    /**
     * @brief Construct a new Benchmark object.
     *
     * @param suiteName Name of the benchmark suite.
     * @param name Name of the benchmark.
     * @param config Configuration settings for the benchmark.
     */
    Benchmark(std::string suiteName, std::string name, Config config = {})
        : suiteName_(std::move(suiteName)),
          name_(std::move(name)),
          config_(std::move(config)),
          sourceLocation_(
              std::source_location::current())  // Capture location here
    {
        validateInputs();
    }

    /**
     * @brief Construct a new Benchmark object with source location.
     *
     * @param suiteName Name of the benchmark suite.
     * @param name Name of the benchmark.
     * @param config Configuration settings for the benchmark.
     * @param location Source location for automatic file/line information.
     */
    Benchmark(
        std::string suiteName, std::string name, Config config,
        const std::source_location&
            location /* = std::source_location::current() */)  // Default arg
                                                               // removed,
                                                               // handled by
                                                               // other ctor
        : suiteName_(std::move(suiteName)),
          name_(std::move(name)),
          config_(std::move(config)),
          sourceLocation_(location) {
        validateInputs();
    }

    /**
     * @brief Run the benchmark with setup, function, and teardown steps.
     *
     * @tparam SetupFunc Type of the setup function, must return a value.
     * @tparam Func Type of the function to benchmark, must accept setup data
     * reference and return size_t (operation count).
     * @tparam TeardownFunc Type of the teardown function, must accept setup
     * data reference.
     * @param setupFunc Function to set up the benchmark environment.
     * @param func Function to benchmark.
     * @param teardownFunc Function to clean up after the benchmark.
     */
    template <typename SetupFunc, typename Func, typename TeardownFunc>
        requires std::invocable<SetupFunc> &&
                 std::invocable<Func, std::invoke_result_t<SetupFunc>&> &&
                 std::same_as<std::invoke_result_t<
                                  Func, std::invoke_result_t<SetupFunc>&>,
                              size_t> &&  // Ensure Func returns size_t
                 std::invocable<TeardownFunc, std::invoke_result_t<SetupFunc>&>
    void run(SetupFunc&& setupFunc, Func&& func, TeardownFunc&& teardownFunc) {
        log(LogLevel::Normal, "Starting benchmark: " + name_);
        auto runBenchmark = [&]() {
            std::vector<Duration> durations;
            std::vector<MemoryStats>
                memoryStats;  // Stores stats *before* each iteration
            std::vector<CPUStats>
                cpuStats;  // Stores stats *during* each iteration
            std::size_t totalOpCount = 0;
            MemoryStats startMemory;
            MemoryStats currentMemStat;  // Temporary storage within loop

            // Capture initial memory state if enabled
            if (config_.enableMemoryStats) {
                startMemory = getMemoryUsage();
                log(LogLevel::Verbose,
                    "Initial memory usage: " +
                        std::to_string(startMemory.currentUsage / 1024) +
                        " KB, Peak: " +
                        std::to_string(startMemory.peakUsage / 1024) + " KB");
            }

            if (config_.warmup) {
                log(LogLevel::Normal, "Warmup run for benchmark: " + name_);
                warmup(setupFunc, func, teardownFunc);
            }

            auto benchmarkStartTime = Clock::now();
            size_t iterationCount = 0;
            while (true) {
                iterationCount++;
                log(LogLevel::Verbose, "Starting iteration " +
                                           std::to_string(iterationCount) +
                                           " for benchmark: " + name_);

                auto setupData =
                    setupFunc();  // Setup for the current iteration

                if (config_.enableMemoryStats) {
                    currentMemStat = getMemoryUsage();  // Memory state *before*
                                                        // func execution
                    memoryStats.push_back(currentMemStat);
                }

                CPUStats cpuStatStart;
                if (config_.enableCpuStats) {
                    cpuStatStart =
                        getCpuStats();  // CPU state *before* func execution
                }

                TimePoint iterStartTime = Clock::now();
                size_t opCount =
                    func(setupData);  // Execute the function to benchmark
                Duration elapsed = Clock::now() - iterStartTime;

                durations.push_back(elapsed);
                totalOpCount += opCount;

                if (config_.enableCpuStats) {
                    auto cpuStatEnd =
                        getCpuStats();  // CPU state *after* func execution
                    cpuStats.push_back(cpuStatEnd.diff(cpuStatStart));
                }

                teardownFunc(setupData);  // Teardown for the current iteration

                log(LogLevel::Verbose,
                    "Completed iteration " + std::to_string(iterationCount) +
                        " for benchmark: " + name_ + " in " +
                        std::to_string(
                            std::chrono::duration<double, std::micro>(elapsed)
                                .count()) +
                        " μs, Ops: " + std::to_string(opCount));

                // Check termination conditions
                auto currentTime = Clock::now();
                auto totalElapsedDuration =
                    std::chrono::duration<double>(currentTime -
                                                  benchmarkStartTime)
                        .count();

                bool minItersReached =
                    iterationCount >=
                    static_cast<size_t>(config_.minIterations);
                bool minDurationReached =
                    totalElapsedDuration >= config_.minDurationSec;
                bool maxItersExceeded =
                    config_.maxIterations &&
                    iterationCount >= *config_.maxIterations;
                bool maxDurationExceeded =
                    config_.maxDurationSec &&
                    totalElapsedDuration >= *config_.maxDurationSec;

                // Stop if max limits reached OR if both min limits are met
                if (maxItersExceeded || maxDurationExceeded ||
                    (minItersReached && minDurationReached)) {
                    break;
                }
            }

            log(LogLevel::Normal, "Analyzing results for benchmark: " + name_ +
                                      " (" + std::to_string(durations.size()) +
                                      " iterations)");
            analyzeResults(durations, memoryStats, cpuStats, totalOpCount);
            log(LogLevel::Normal, "Completed benchmark: " + name_);

            // Auto-export if configured
            if (config_.outputFilePath) {
                try {
                    exportResults(*config_.outputFilePath,
                                  config_.format);  // Use configured format
                } catch (const std::exception& e) {
                    staticLog(LogLevel::Minimal,
                              "Error auto-exporting results for " + name_ +
                                  " to " + *config_.outputFilePath + ": " +
                                  e.what());
                }
            }
        };

        if (config_.async) {
            std::future<void> future =
                std::async(std::launch::async, runBenchmark);
            // Consider adding error handling for future.get() if needed
            try {
                future.get();
            } catch (const std::exception& e) {
                staticLog(LogLevel::Minimal,
                          "Exception caught in async benchmark task '" + name_ +
                              "': " + e.what());
                // Optionally rethrow or handle differently
                throw;
            }
        } else {
            runBenchmark();
        }
    }

    /**
     * @brief Print the benchmark results.
     *
     * @param suite Optional suite name to filter results.
     */
    static void printResults(const std::string& suite = "");

    /**
     * @brief Export the benchmark results to a file using PlainText format.
     *
     * @param filename Name of the file to export results to.
     * @throws std::runtime_error if export fails
     */
    static void exportResults(const std::string& filename);

    /**
     * @brief Export the benchmark results to a file with specified format.
     *
     * @param filename Name of the file to export results to.
     * @param format Export format to use.
     * @throws std::runtime_error if export fails
     */
    static void exportResults(const std::string& filename, ExportFormat format);

    /**
     * @brief Clear all benchmark results.
     */
    static void clearResults() noexcept;

    /**
     * @brief Get a copy of all benchmark results.
     *
     * @return A copy of the results map
     */
    static auto getResults()
        -> const std::map<std::string,
                          std::vector<Benchmark::Result>>;  // Qualified Result

    /**
     * @brief Set global log level for all benchmarks
     *
     * @param level New log level
     */
    static void setGlobalLogLevel(LogLevel level) noexcept;

    /**
     * @brief Check if platform supports CPU statistics
     *        NOTE: This is a placeholder. Actual implementation is
     * platform-specific.
     *
     * @return true if CPU stats are supported, false otherwise
     */
    static bool isCpuStatsSupported() noexcept;

    /**
     * @brief Register a custom logger to be used for all benchmarks
     *
     * @param logger Function taking a string message
     */
    static void registerGlobalLogger(
        std::function<void(const std::string&)> logger) noexcept;

private:
    /**
     * @brief Structure to hold benchmark results.
     */
    struct Result {
        std::string name;            ///< Name of the benchmark.
        double averageDuration{};    ///< Average duration of the benchmark
                                     ///< (microseconds).
        double minDuration{};        ///< Minimum duration of the benchmark
                                     ///< (microseconds).
        double maxDuration{};        ///< Maximum duration of the benchmark
                                     ///< (microseconds).
        double medianDuration{};     ///< Median duration of the benchmark
                                     ///< (microseconds).
        double standardDeviation{};  ///< Standard deviation of the durations
                                     ///< (microseconds).
        int iterations{};            ///< Number of iterations.
        double throughput{};  ///< Throughput of the benchmark (operations per
                              ///< second).
        std::optional<double> avgMemoryUsage{};  ///< Average memory usage
                                                 ///< during benchmark (bytes).
        std::optional<double>
            peakMemoryUsage{};  ///< Peak memory usage during benchmark (bytes).
        std::optional<CPUStats>
            avgCPUStats{};  ///< Average CPU statistics per iteration.
        std::optional<double>
            instructionsPerCycle{};  ///< Instructions per cycle.
        std::string sourceLine{};    ///< Source code location (file:line).
        std::string timestamp{};     ///< Timestamp when benchmark was run.

        /**
         * @brief Convert result to a printable string (PlainText format)
         *
         * @return Formatted string representation of the result
         */
        [[nodiscard]] std::string toString() const;
    } ATOM_ALIGNAS(128);

    /**
     * @brief Perform a warmup run of the benchmark.
     *
     * @param setupFunc Function to set up the benchmark environment.
     * @param func Function to benchmark.
     * @param teardownFunc Function to clean up after the benchmark.
     */
    template <typename SetupFunc, typename Func, typename TeardownFunc>
        requires std::invocable<SetupFunc> &&
                 std::invocable<Func, std::invoke_result_t<SetupFunc>&> &&
                 std::invocable<TeardownFunc, std::invoke_result_t<SetupFunc>&>
    void warmup(SetupFunc&& setupFunc, Func&& func,
                TeardownFunc&& teardownFunc) {
        log(LogLevel::Verbose, "Performing warmup run...");

        try {
            auto data = setupFunc();
            func(data);
            teardownFunc(data);
            log(LogLevel::Verbose, "Warmup completed successfully");
        } catch (const std::exception& e) {
            log(LogLevel::Minimal, "Warmup failed: " + std::string(e.what()));
            throw;
        }
    }

    /**
     * @brief Calculate the total duration from a vector of durations.
     *
     * @param durations Vector of durations.
     * @return Duration Total duration.
     */
    static auto totalDuration(std::span<const Duration> durations) noexcept
        -> Duration {
        return std::accumulate(durations.begin(), durations.end(),
                               Duration::zero());
    }

    /**
     * @brief Analyze the results of the benchmark.
     *
     * @param durations Vector of durations for each iteration.
     * @param memoryStats Vector of memory statistics captured *before* each
     * iteration.
     * @param cpuStats Vector of CPU statistics captured *during* each
     * iteration.
     * @param totalOpCount Total number of operations performed across all
     * iterations.
     */
    void analyzeResults(std::span<const Duration> durations,
                        std::span<const MemoryStats> memoryStats,
                        std::span<const CPUStats> cpuStats,
                        std::size_t totalOpCount);

    /**
     * @brief Calculate the standard deviation of a vector of values.
     *
     * @param values Vector of values.
     * @param mean The pre-calculated mean of the values.
     * @return double Standard deviation.
     */
    static auto calculateStandardDeviation(std::span<const double> values,
                                           double mean) noexcept -> double;

    /**
     * @brief Calculate the average CPU statistics from a vector of CPU
     * statistics.
     *
     * @param stats Vector of CPU statistics (diffs per iteration).
     * @return CPUStats Average CPU statistics per iteration. Returns default if
     * input is empty.
     */
    static auto calculateAverageCpuStats(
        std::span<const CPUStats> stats) noexcept -> CPUStats;

    /**
     * @brief Get the current memory usage statistics.
     *        NOTE: This is a placeholder. Actual implementation is
     * platform-specific.
     *
     * @return MemoryStats Current memory usage statistics.
     */
    static auto getMemoryUsage() noexcept -> MemoryStats;

    /**
     * @brief Get the current CPU usage statistics.
     *        NOTE: This is a placeholder. Actual implementation is
     * platform-specific.
     *
     * @return CPUStats Current CPU usage statistics.
     */
    static auto getCpuStats() noexcept -> CPUStats;

    /**
     * @brief Log a message based on log level.
     *
     * @param level Log level for this message
     * @param message Message to log.
     */
    void log(LogLevel level, const std::string& message) const;

    /**
     * @brief Log a message using global static logger.
     *
     * @param level Log level for this message
     * @param message Message to log.
     */
    static void staticLog(LogLevel level, const std::string& message);

    /**
     * @brief Validate input parameters for the benchmark.
     *
     * @throws std::invalid_argument if inputs are invalid
     */
    void validateInputs() const;

    /**
     * @brief Get current date and time as string (ISO 8601 format).
     *
     * @return Formatted date and time string
     */
    static std::string getCurrentTimestamp() noexcept;

    // --- Member Variables ---
    std::string suiteName_;  ///< Name of the benchmark suite.
    std::string name_;       ///< Name of the benchmark.
    Config config_;          ///< Configuration settings for the benchmark.
    std::source_location sourceLocation_;  ///< Source location information

    // --- Static Member Variables ---
    // Use inline static members (C++17) for simpler definition
    inline static std::map<std::string, std::vector<Result>>
        results;                            ///< Map of benchmark results.
    inline static std::mutex resultsMutex;  ///< Mutex for accessing results.
    inline static std::mutex logMutex;      ///< Mutex for logging messages.
    inline static std::atomic<LogLevel> globalLogLevel =
        LogLevel::Normal;  ///< Global log level
    inline static std::optional<std::function<void(const std::string&)>>
        globalLogger = std::nullopt;  ///< Optional global logger
};

#endif  // ATOM_TESTS_BENCHMARK_HPP
