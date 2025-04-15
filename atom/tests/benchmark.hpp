#ifndef ATOM_TESTS_BENCHMARK_HPP
#define ATOM_TESTS_BENCHMARK_HPP

#include <chrono>
#include <cmath>
#include <concepts>
#include <fstream> // Needed for exportResults
#include <functional>
#include <future>
#include <iomanip> // Needed for formatting output
#include <iostream> // Needed for default logging
#include <map>
#include <mutex>
#include <numeric> // Needed for std::accumulate
#include <optional>
#include <source_location>
#include <span>
#include <sstream> // Needed for formatting output
#include <stdexcept> // Needed for exceptions
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <atomic> // Needed for std::atomic
#include <algorithm> // Needed for std::sort

#include "atom/macro.hpp"

/**
 * @brief Class for benchmarking code performance.
 *
 * @example
 * // Basic usage example
 * Benchmark bench("MySuite", "FastAlgorithm");
 * bench.run(
 *     []() { return std::vector<int>{1, 2, 3}; },  // Setup
 *     [](auto& data) { std::sort(data.begin(), data.end()); return 1; }, // Test function
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
            result.instructionsExecuted = instructionsExecuted > other.instructionsExecuted ? instructionsExecuted - other.instructionsExecuted : 0;
            result.cyclesElapsed = cyclesElapsed > other.cyclesElapsed ? cyclesElapsed - other.cyclesElapsed : 0;
            result.branchMispredictions = branchMispredictions > other.branchMispredictions ? branchMispredictions - other.branchMispredictions : 0;
            result.cacheMisses = cacheMisses > other.cacheMisses ? cacheMisses - other.cacheMisses : 0;
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
        // Explicit default constructor to potentially resolve compiler issues with default arguments
        Config() :
            minIterations(10),
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
            outputFilePath(std::nullopt)
        {}


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
          sourceLocation_(std::source_location::current()) // Capture location here
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
        const std::source_location& location /* = std::source_location::current() */) // Default arg removed, handled by other ctor
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
     * @tparam Func Type of the function to benchmark, must accept setup data reference and return size_t (operation count).
     * @tparam TeardownFunc Type of the teardown function, must accept setup data reference.
     * @param setupFunc Function to set up the benchmark environment.
     * @param func Function to benchmark.
     * @param teardownFunc Function to clean up after the benchmark.
     */
    template <typename SetupFunc, typename Func, typename TeardownFunc>
        requires std::invocable<SetupFunc> &&
                 std::invocable<Func, std::invoke_result_t<SetupFunc>&> &&
                 std::same_as<std::invoke_result_t<Func, std::invoke_result_t<SetupFunc>&>, size_t> && // Ensure Func returns size_t
                 std::invocable<TeardownFunc, std::invoke_result_t<SetupFunc>&>
    void run(SetupFunc&& setupFunc, Func&& func, TeardownFunc&& teardownFunc) {
        log(LogLevel::Normal, "Starting benchmark: " + name_);
        auto runBenchmark = [&]() {
            std::vector<Duration> durations;
            std::vector<MemoryStats> memoryStats; // Stores stats *before* each iteration
            std::vector<CPUStats> cpuStats; // Stores stats *during* each iteration
            std::size_t totalOpCount = 0;
            MemoryStats startMemory;
            MemoryStats currentMemStat; // Temporary storage within loop

            // Capture initial memory state if enabled
            if (config_.enableMemoryStats) {
                startMemory = getMemoryUsage();
                log(LogLevel::Verbose,
                    "Initial memory usage: " +
                        std::to_string(startMemory.currentUsage / 1024) +
                        " KB, Peak: " + std::to_string(startMemory.peakUsage / 1024) + " KB");
            }

            if (config_.warmup) {
                log(LogLevel::Normal, "Warmup run for benchmark: " + name_);
                warmupRun(setupFunc, func, teardownFunc);
            }

            auto benchmarkStartTime = Clock::now();
            size_t iterationCount = 0;
            while (true) {
                 iterationCount++;
                 log(LogLevel::Verbose,
                    "Starting iteration " +
                        std::to_string(iterationCount) +
                        " for benchmark: " + name_);

                auto setupData = setupFunc(); // Setup for the current iteration

                if (config_.enableMemoryStats) {
                    currentMemStat = getMemoryUsage(); // Memory state *before* func execution
                    memoryStats.push_back(currentMemStat);
                }

                CPUStats cpuStatStart;
                if (config_.enableCpuStats) {
                    cpuStatStart = getCpuStats(); // CPU state *before* func execution
                }

                TimePoint iterStartTime = Clock::now();
                size_t opCount = func(setupData); // Execute the function to benchmark
                Duration elapsed = Clock::now() - iterStartTime;

                durations.push_back(elapsed);
                totalOpCount += opCount;

                if (config_.enableCpuStats) {
                    auto cpuStatEnd = getCpuStats(); // CPU state *after* func execution
                    cpuStats.push_back(cpuStatEnd.diff(cpuStatStart));
                }

                teardownFunc(setupData); // Teardown for the current iteration

                log(LogLevel::Verbose,
                    "Completed iteration " + std::to_string(iterationCount) +
                        " for benchmark: " + name_ + " in " +
                        std::to_string(
                            std::chrono::duration<double, std::micro>(elapsed)
                                .count()) +
                        " μs, Ops: " + std::to_string(opCount));

                // Check termination conditions
                auto currentTime = Clock::now();
                auto totalElapsedDuration = std::chrono::duration<double>(currentTime - benchmarkStartTime).count();

                bool minItersReached = iterationCount >= static_cast<size_t>(config_.minIterations);
                bool minDurationReached = totalElapsedDuration >= config_.minDurationSec;
                bool maxItersExceeded = config_.maxIterations && iterationCount >= *config_.maxIterations;
                bool maxDurationExceeded = config_.maxDurationSec && totalElapsedDuration >= *config_.maxDurationSec;

                // Stop if max limits reached OR if both min limits are met
                if (maxItersExceeded || maxDurationExceeded || (minItersReached && minDurationReached)) {
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
                    exportResults(*config_.outputFilePath, config_.format); // Use configured format
                 } catch (const std::exception& e) {
                    staticLog(LogLevel::Minimal, "Error auto-exporting results for " + name_ + " to " + *config_.outputFilePath + ": " + e.what());
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
                 staticLog(LogLevel::Minimal, "Exception caught in async benchmark task '" + name_ + "': " + e.what());
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
        -> const std::map<std::string, std::vector<Benchmark::Result>>; // Qualified Result

    /**
     * @brief Set global log level for all benchmarks
     *
     * @param level New log level
     */
    static void setGlobalLogLevel(LogLevel level) noexcept;

    /**
     * @brief Check if platform supports CPU statistics
     *        NOTE: This is a placeholder. Actual implementation is platform-specific.
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
        double averageDuration{};    ///< Average duration of the benchmark (microseconds).
        double minDuration{};        ///< Minimum duration of the benchmark (microseconds).
        double maxDuration{};        ///< Maximum duration of the benchmark (microseconds).
        double medianDuration{};     ///< Median duration of the benchmark (microseconds).
        double standardDeviation{};  ///< Standard deviation of the durations (microseconds).
        int iterations{};            ///< Number of iterations.
        double throughput{};         ///< Throughput of the benchmark (operations per second).
        std::optional<double> avgMemoryUsage{};   ///< Average memory usage during benchmark (bytes).
        std::optional<double> peakMemoryUsage{};  ///< Peak memory usage during benchmark (bytes).
        std::optional<CPUStats> avgCPUStats{};    ///< Average CPU statistics per iteration.
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
    void warmupRun(const SetupFunc& setupFunc, const Func& func,
                   const TeardownFunc& teardownFunc) {
        try {
            auto setupData = setupFunc();
            func(setupData);  // Warmup operation
            teardownFunc(setupData);
        } catch (const std::exception& e) {
            log(LogLevel::Minimal, "Exception during warmup for benchmark '" + name_ + "': " + e.what());
            // Decide if warmup failure should prevent the actual run? For now, just log.
        } catch (...) {
            log(LogLevel::Minimal, "Unknown exception during warmup for benchmark '" + name_ + "'");
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
         return std::accumulate(durations.begin(), durations.end(), Duration::zero());
    }

    /**
     * @brief Analyze the results of the benchmark.
     *
     * @param durations Vector of durations for each iteration.
     * @param memoryStats Vector of memory statistics captured *before* each iteration.
     * @param cpuStats Vector of CPU statistics captured *during* each iteration.
     * @param totalOpCount Total number of operations performed across all iterations.
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
    static auto calculateStandardDeviation(
        std::span<const double> values, double mean) noexcept -> double;


    /**
     * @brief Calculate the average CPU statistics from a vector of CPU
     * statistics.
     *
     * @param stats Vector of CPU statistics (diffs per iteration).
     * @return CPUStats Average CPU statistics per iteration. Returns default if input is empty.
     */
    static auto calculateAverageCpuStats(
        std::span<const CPUStats> stats) noexcept -> CPUStats;

    /**
     * @brief Get the current memory usage statistics.
     *        NOTE: This is a placeholder. Actual implementation is platform-specific.
     *
     * @return MemoryStats Current memory usage statistics.
     */
    static auto getMemoryUsage() noexcept -> MemoryStats;

    /**
     * @brief Get the current CPU usage statistics.
     *        NOTE: This is a placeholder. Actual implementation is platform-specific.
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
    inline static std::map<std::string, std::vector<Result>> results; ///< Map of benchmark results.
    inline static std::mutex resultsMutex;  ///< Mutex for accessing results.
    inline static std::mutex logMutex;      ///< Mutex for logging messages.
    inline static std::atomic<LogLevel> globalLogLevel = LogLevel::Normal; ///< Global log level
    inline static std::optional<std::function<void(const std::string&)>> globalLogger = std::nullopt; ///< Optional global logger
};

// --- Static Member Implementations (if not defined inline or need more complex logic) ---

// Placeholder implementations for platform-specific features
inline bool Benchmark::isCpuStatsSupported() noexcept {
    // TODO: Implement platform-specific check (e.g., using cpuid, performance counters)
    return false; // Default to not supported
}

inline auto Benchmark::getMemoryUsage() noexcept -> MemoryStats {
    // TODO: Implement platform-specific memory query (e.g., /proc/self/statm on Linux, GetProcessMemoryInfo on Windows)
    return {}; // Return default (zeroed) stats
}

inline auto Benchmark::getCpuStats() noexcept -> CPUStats {
    // TODO: Implement platform-specific CPU counter query (e.g., RDPMC, perf_event_open on Linux, QueryPerformanceCounter on Windows - though that's time, not CPU stats)
    return {}; // Return default (zeroed) stats
}

inline std::string Benchmark::getCurrentTimestamp() noexcept {
     try {
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        // Using std::put_time for safer formatting
        // Note: std::put_time requires #include <iomanip>
        // Note: std::gmtime might be preferred for timezone independence, but requires careful handling of the returned struct pointer.
        // Using localtime_s or localtime_r for thread-safety if available, otherwise fallback with mutex or accept potential issues.
        // For simplicity here, using std::put_time with std::localtime (potentially non-threadsafe without external sync)
        std::tm now_tm;
        #ifdef _WIN32
            localtime_s(&now_tm, &now_c); // Windows specific thread-safe version
        #elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
            localtime_r(&now_c, &now_tm); // POSIX specific thread-safe version
        #else
            // Fallback - potentially not thread-safe, consider adding mutex if needed
            static std::mutex timeMutex;
            std::lock_guard lock(timeMutex);
            now_tm = *std::localtime(&now_c);
        #endif
        ss << std::put_time(&now_tm, "%Y-%m-%dT%H:%M:%S");

        // Add milliseconds
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z'; // ISO 8601 format with UTC 'Z'
        return ss.str();
    } catch (...) { // Catch potential exceptions during time formatting
        return "YYYY-MM-DDTHH:MM:SS.sssZ"; // Fallback string
    }
}

inline void Benchmark::validateInputs() const {
    if (suiteName_.empty()) {
        throw std::invalid_argument("Benchmark suite name cannot be empty.");
    }
    if (name_.empty()) {
        throw std::invalid_argument("Benchmark name cannot be empty.");
    }
    if (config_.minIterations <= 0) {
        throw std::invalid_argument("Minimum iterations must be positive.");
    }
    if (config_.minDurationSec <= 0.0) {
        throw std::invalid_argument("Minimum duration must be positive.");
    }
    if (config_.maxIterations && *config_.maxIterations < static_cast<size_t>(config_.minIterations)) {
         throw std::invalid_argument("Maximum iterations cannot be less than minimum iterations.");
    }
     if (config_.maxDurationSec && *config_.maxDurationSec < config_.minDurationSec) {
         throw std::invalid_argument("Maximum duration cannot be less than minimum duration.");
    }
    if (config_.enableCpuStats && !isCpuStatsSupported()) {
        // Log a warning instead of throwing, allow benchmark to run without CPU stats
        staticLog(LogLevel::Minimal, "Warning: CPU statistics requested but not supported on this platform for benchmark '" + name_ + "'. Disabling.");
        // Ideally, modify the config_ instance, but it's const here.
        // This check should perhaps happen earlier or the config should be mutable.
        // For now, the run logic will simply not collect the stats if getCpuStats returns defaults.
    }
     // Add more validation as needed (e.g., file path validity if outputFilePath is set)
}

inline void Benchmark::staticLog(LogLevel level, const std::string& message) {
    if (level == LogLevel::Silent) return;

    LogLevel currentGlobalLevel = globalLogLevel.load(std::memory_order_relaxed);
    if (level > currentGlobalLevel && level != LogLevel::Minimal) return; // Minimal always logs if not Silent

    std::lock_guard lock(logMutex); // Ensure thread-safe logging
    if (globalLogger) {
        (*globalLogger)(message);
    } else {
        // Default logger: print to cerr for Minimal, cout otherwise
        auto& stream = (level == LogLevel::Minimal) ? std::cerr : std::cout;
        stream << "[" << getCurrentTimestamp() << "] " << message << std::endl;
    }
}

inline void Benchmark::log(LogLevel level, const std::string& message) const {
     // Use instance-specific level first, then fall back to global
     LogLevel effectiveLevel = (config_.logLevel != LogLevel::Normal) ? config_.logLevel : globalLogLevel.load(std::memory_order_relaxed);

     if (level == LogLevel::Silent || level > effectiveLevel) return;

     std::lock_guard lock(logMutex); // Ensure thread-safe logging
     if (config_.customLogger) {
         (*config_.customLogger)(message);
     } else if (globalLogger) {
         (*globalLogger)(message);
     } else {
         // Default logger: print to cerr for Minimal, cout otherwise
         auto& stream = (level == LogLevel::Minimal) ? std::cerr : std::cout;
         stream << "[" << getCurrentTimestamp() << "] [" << suiteName_ << "/" << name_ << "] " << message << std::endl;
     }
}


inline void Benchmark::setGlobalLogLevel(LogLevel level) noexcept {
    globalLogLevel.store(level, std::memory_order_relaxed);
}

inline void Benchmark::registerGlobalLogger(std::function<void(const std::string&)> logger) noexcept {
    std::lock_guard lock(logMutex); // Ensure thread-safe update
    globalLogger = std::move(logger);
}

inline void Benchmark::clearResults() noexcept {
    std::lock_guard lock(resultsMutex);
    results.clear();
}

// Define getResults using the qualified Benchmark::Result
inline auto Benchmark::getResults() -> const std::map<std::string, std::vector<Benchmark::Result>> {
    std::lock_guard lock(resultsMutex);
    // Return a copy to avoid external modification issues, though returning const& might be more efficient if caller is trusted
    return results;
}

// --- Analysis and Reporting Method Implementations ---

inline auto Benchmark::calculateStandardDeviation(std::span<const double> values, double mean) noexcept -> double {
    if (values.size() < 2) {
        return 0.0; // Standard deviation is undefined for less than 2 samples
    }
    double sq_sum = std::accumulate(values.begin(), values.end(), 0.0,
        [mean](double accumulator, double val) {
            return accumulator + (val - mean) * (val - mean);
        });
    return std::sqrt(sq_sum / (values.size() - 1)); // Use sample standard deviation (N-1)
}

inline auto Benchmark::calculateAverageCpuStats(std::span<const CPUStats> stats) noexcept -> CPUStats {
    if (stats.empty()) {
        return {};
    }
    CPUStats total{};
    for (const auto& s : stats) {
        total.instructionsExecuted += s.instructionsExecuted;
        total.cyclesElapsed += s.cyclesElapsed;
        total.branchMispredictions += s.branchMispredictions;
        total.cacheMisses += s.cacheMisses;
    }
    size_t count = stats.size();
    CPUStats result;
    result.instructionsExecuted = total.instructionsExecuted / static_cast<int64_t>(count);
    result.cyclesElapsed = total.cyclesElapsed / static_cast<int64_t>(count);
    result.branchMispredictions = total.branchMispredictions / static_cast<int64_t>(count);
    result.cacheMisses = total.cacheMisses / static_cast<int64_t>(count);
    return result;
}

inline void Benchmark::analyzeResults(std::span<const Duration> durations,
                                     std::span<const MemoryStats> memoryStats, // Before iteration stats
                                     std::span<const CPUStats> cpuStats,       // Per iteration diffs
                                     std::size_t totalOpCount) {
    if (durations.empty()) {
        log(LogLevel::Minimal, "No iterations were run for benchmark: " + name_);
        return;
    }

    Result result;
    result.name = name_;
    result.iterations = static_cast<int>(durations.size());
    result.timestamp = getCurrentTimestamp();
    result.sourceLine = std::string(sourceLocation_.file_name()) + ":" + std::to_string(sourceLocation_.line());


    // Convert durations to microseconds for analysis
    std::vector<double> durations_us;
    durations_us.reserve(durations.size());
    for (const auto& d : durations) {
        durations_us.push_back(std::chrono::duration<double, std::micro>(d).count());
    }

    // Basic time stats
    double total_duration_us = std::accumulate(durations_us.begin(), durations_us.end(), 0.0);
    result.averageDuration = total_duration_us / durations_us.size();
    std::sort(durations_us.begin(), durations_us.end());
    result.minDuration = durations_us.front();
    result.maxDuration = durations_us.back();
    result.medianDuration = (durations_us.size() % 2 != 0)
                               ? durations_us[durations_us.size() / 2]
                               : (durations_us[durations_us.size() / 2 - 1] + durations_us[durations_us.size() / 2]) / 2.0;
    result.standardDeviation = calculateStandardDeviation(durations_us, result.averageDuration);

    // Throughput (ops per second)
    double total_duration_sec = total_duration_us / 1'000'000.0;
    if (total_duration_sec > 0 && totalOpCount > 0) {
        result.throughput = static_cast<double>(totalOpCount) / total_duration_sec;
    } else {
        result.throughput = 0.0; // Or handle as NaN/optional if preferred
    }


    // Memory stats analysis
    if (config_.enableMemoryStats && !memoryStats.empty()) {
        double totalCurrentUsage = 0;
        size_t peakUsage = 0;
        for(const auto& mem : memoryStats) {
            totalCurrentUsage += static_cast<double>(mem.currentUsage);
            if (mem.peakUsage > peakUsage) {
                peakUsage = mem.peakUsage;
            }
        }
        result.avgMemoryUsage = totalCurrentUsage / memoryStats.size();
        result.peakMemoryUsage = static_cast<double>(peakUsage); // Use the overall peak observed
    }

    // CPU stats analysis
    if (config_.enableCpuStats && !cpuStats.empty()) {
        result.avgCPUStats = calculateAverageCpuStats(cpuStats);
        if (result.avgCPUStats->cyclesElapsed > 0) {
             result.instructionsPerCycle = static_cast<double>(result.avgCPUStats->instructionsExecuted) / result.avgCPUStats->cyclesElapsed;
        }
    }

    // Store the result
    std::lock_guard lock(resultsMutex);
    results[suiteName_].push_back(std::move(result));
}

// --- Result::toString Implementation ---
inline std::string Benchmark::Result::toString() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3); // Use 3 decimal places for times

    ss << "Benchmark: " << name << " (" << iterations << " iterations)\n";
    ss << "  Location: " << sourceLine << "\n";
    ss << "  Timestamp: " << timestamp << "\n";
    ss << "  Time (us): Avg=" << averageDuration << ", Min=" << minDuration
       << ", Max=" << maxDuration << ", Median=" << medianDuration
       << ", StdDev=" << standardDeviation << "\n";

    if (throughput > 0) {
        ss << "  Throughput: " << std::fixed << std::setprecision(2) << throughput << " ops/sec\n";
    }

    if (avgMemoryUsage.has_value()) {
        ss << std::fixed << std::setprecision(0); // No decimals for bytes
        ss << "  Memory (bytes): Avg=" << *avgMemoryUsage;
        if (peakMemoryUsage.has_value()) {
            ss << ", Peak=" << *peakMemoryUsage;
        }
        ss << "\n";
    }

    if (avgCPUStats.has_value()) {
        ss << "  CPU Stats (avg/iter):";
        if (avgCPUStats->instructionsExecuted != 0) ss << " Instr=" << avgCPUStats->instructionsExecuted;
        if (avgCPUStats->cyclesElapsed != 0) ss << ", Cycles=" << avgCPUStats->cyclesElapsed;
        if (avgCPUStats->branchMispredictions != 0) ss << ", BranchMispred=" << avgCPUStats->branchMispredictions;
        if (avgCPUStats->cacheMisses != 0) ss << ", CacheMiss=" << avgCPUStats->cacheMisses;
        if (instructionsPerCycle.has_value()) {
             ss << std::fixed << std::setprecision(3);
             ss << ", IPC=" << *instructionsPerCycle;
        }
        ss << "\n";
    }

    return ss.str();
}

// --- Reporting Function Implementations ---

inline void Benchmark::printResults(const std::string& suite) {
    std::lock_guard lock(resultsMutex);
    if (results.empty()) {
        staticLog(LogLevel::Normal, "No benchmark results available to print.");
        return;
    }

    staticLog(LogLevel::Normal, "--- Benchmark Results ---");
    for (const auto& [suiteName, suiteResults] : results) {
        if (suite.empty() || suite == suiteName) {
            staticLog(LogLevel::Normal, "Suite: " + suiteName);
            for (const auto& result : suiteResults) {
                staticLog(LogLevel::Normal, result.toString()); // Use staticLog for console output
            }
        }
    }
    staticLog(LogLevel::Normal, "-------------------------");
}

inline void Benchmark::exportResults(const std::string& filename) {
    // Default to PlainText format if not specified
    exportResults(filename, ExportFormat::PlainText);
}

inline void Benchmark::exportResults(const std::string& filename, ExportFormat format) {
    std::lock_guard lock(resultsMutex);
    if (results.empty()) {
        staticLog(LogLevel::Minimal, "No benchmark results available to export.");
        return; // Or throw? For now, just log and return.
    }

    std::ofstream outFile(filename);
    if (!outFile) {
        throw std::runtime_error("Failed to open file for exporting results: " + filename);
    }

    // Disable exceptions for the stream after opening, handle errors manually if needed
    outFile.exceptions(std::ios_base::goodbit);

    try {
        switch (format) {
            case ExportFormat::Json: {
                // Basic JSON structure: { "suiteName": [ {result1}, {result2} ], ... }
                outFile << "{\n";
                bool firstSuite = true;
                for (const auto& [suiteName, suiteResults] : results) {
                    if (!firstSuite) outFile << ",\n";
                    outFile << "  \"" << suiteName << "\": [\n";
                    bool firstResult = true;
                    for (const auto& res : suiteResults) {
                        if (!firstResult) outFile << ",\n";
                        outFile << "    {\n";
                        outFile << "      \"name\": \"" << res.name << "\",\n";
                        outFile << "      \"iterations\": " << res.iterations << ",\n";
                        outFile << "      \"source\": \"" << res.sourceLine << "\",\n";
                        outFile << "      \"timestamp\": \"" << res.timestamp << "\",\n";
                        outFile << "      \"time_avg_us\": " << res.averageDuration << ",\n";
                        outFile << "      \"time_min_us\": " << res.minDuration << ",\n";
                        outFile << "      \"time_max_us\": " << res.maxDuration << ",\n";
                        outFile << "      \"time_median_us\": " << res.medianDuration << ",\n";
                        outFile << "      \"time_stddev_us\": " << res.standardDeviation << ",\n";
                        outFile << "      \"throughput_ops_sec\": " << res.throughput; // No comma for last standard field
                        if(res.avgMemoryUsage) outFile << ",\n      \"memory_avg_bytes\": " << *res.avgMemoryUsage;
                        if(res.peakMemoryUsage) outFile << ",\n      \"memory_peak_bytes\": " << *res.peakMemoryUsage;
                        if(res.avgCPUStats) {
                            outFile << ",\n      \"cpu_avg_instructions\": " << res.avgCPUStats->instructionsExecuted;
                            outFile << ",\n      \"cpu_avg_cycles\": " << res.avgCPUStats->cyclesElapsed;
                            outFile << ",\n      \"cpu_avg_branch_mispredictions\": " << res.avgCPUStats->branchMispredictions;
                            outFile << ",\n      \"cpu_avg_cache_misses\": " << res.avgCPUStats->cacheMisses;
                        }
                        if(res.instructionsPerCycle) outFile << ",\n      \"cpu_ipc\": " << *res.instructionsPerCycle;
                        outFile << "\n    }";
                        firstResult = false;
                    }
                    outFile << "\n  ]";
                    firstSuite = false;
                }
                outFile << "\n}\n";
                break;
            }
            case ExportFormat::Csv: {
                // Header row
                outFile << "Suite,Name,Iterations,Source,Timestamp,AvgTime(us),MinTime(us),MaxTime(us),MedianTime(us),StdDevTime(us),Throughput(ops/sec)";
                // Add optional headers based on whether data exists for *any* result
                bool hasMem = std::any_of(results.begin(), results.end(), [](const auto& p){ return std::any_of(p.second.begin(), p.second.end(), [](const auto& r){ return r.avgMemoryUsage.has_value(); }); });
                bool hasPeakMem = std::any_of(results.begin(), results.end(), [](const auto& p){ return std::any_of(p.second.begin(), p.second.end(), [](const auto& r){ return r.peakMemoryUsage.has_value(); }); });
                bool hasCpu = std::any_of(results.begin(), results.end(), [](const auto& p){ return std::any_of(p.second.begin(), p.second.end(), [](const auto& r){ return r.avgCPUStats.has_value(); }); });
                bool hasIpc = std::any_of(results.begin(), results.end(), [](const auto& p){ return std::any_of(p.second.begin(), p.second.end(), [](const auto& r){ return r.instructionsPerCycle.has_value(); }); });

                if(hasMem) outFile << ",AvgMem(bytes)";
                if(hasPeakMem) outFile << ",PeakMem(bytes)";
                if(hasCpu) outFile << ",AvgInstr,AvgCycles,AvgBranchMispred,AvgCacheMiss";
                if(hasIpc) outFile << ",IPC";
                outFile << "\n";

                // Data rows
                for (const auto& [suiteName, suiteResults] : results) {
                    for (const auto& res : suiteResults) {
                        outFile << "\"" << suiteName << "\",\"" << res.name << "\"," << res.iterations << ",\"" << res.sourceLine << "\",\"" << res.timestamp << "\","
                                << res.averageDuration << "," << res.minDuration << "," << res.maxDuration << "," << res.medianDuration << "," << res.standardDeviation << "," << res.throughput;
                        if(hasMem) outFile << "," << (res.avgMemoryUsage ? std::to_string(*res.avgMemoryUsage) : "");
                        if(hasPeakMem) outFile << "," << (res.peakMemoryUsage ? std::to_string(*res.peakMemoryUsage) : "");
                        if(hasCpu) {
                            if(res.avgCPUStats) {
                                outFile << "," << res.avgCPUStats->instructionsExecuted << "," << res.avgCPUStats->cyclesElapsed << "," << res.avgCPUStats->branchMispredictions << "," << res.avgCPUStats->cacheMisses;
                            } else {
                                outFile << ",,,,"; // Empty cells if this result lacks CPU stats but others have it
                            }
                        }
                        if(hasIpc) outFile << "," << (res.instructionsPerCycle ? std::to_string(*res.instructionsPerCycle) : "");
                        outFile << "\n";
                    }
                }
                break;
            }
            case ExportFormat::Markdown: {
                 // Determine columns based on available data (similar to CSV)
                bool hasMem = std::any_of(results.begin(), results.end(), [](const auto& p){ return std::any_of(p.second.begin(), p.second.end(), [](const auto& r){ return r.avgMemoryUsage.has_value(); }); });
                bool hasPeakMem = std::any_of(results.begin(), results.end(), [](const auto& p){ return std::any_of(p.second.begin(), p.second.end(), [](const auto& r){ return r.peakMemoryUsage.has_value(); }); });
                bool hasCpu = std::any_of(results.begin(), results.end(), [](const auto& p){ return std::any_of(p.second.begin(), p.second.end(), [](const auto& r){ return r.avgCPUStats.has_value(); }); });
                bool hasIpc = std::any_of(results.begin(), results.end(), [](const auto& p){ return std::any_of(p.second.begin(), p.second.end(), [](const auto& r){ return r.instructionsPerCycle.has_value(); }); });

                for (const auto& [suiteName, suiteResults] : results) {
                    outFile << "## Suite: " << suiteName << "\n\n";
                    outFile << "| Benchmark | Iterations | Avg Time (us) | Min Time (us) | Max Time (us) | Median Time (us) | StdDev (us) | Throughput (ops/s) |";
                    if(hasMem) outFile << " Avg Mem (B) |";
                    if(hasPeakMem) outFile << " Peak Mem (B) |";
                    if(hasCpu) outFile << " Avg Instr | Avg Cycles | Avg Branch Mispred | Avg Cache Miss |";
                    if(hasIpc) outFile << " IPC |";
                    outFile << " Source |\n";

                    outFile << "|---|---|---|---|---|---|---|---|";
                     if(hasMem) outFile << "---|";
                     if(hasPeakMem) outFile << "---|";
                     if(hasCpu) outFile << "---|---|---|---|";
                     if(hasIpc) outFile << "---|";
                     outFile << "---|\n";


                    outFile << std::fixed << std::setprecision(3);
                    for (const auto& res : suiteResults) {
                        outFile << "| " << res.name << " | " << res.iterations << " | "
                                << res.averageDuration << " | " << res.minDuration << " | " << res.maxDuration << " | " << res.medianDuration << " | " << res.standardDeviation << " | ";
                        outFile << std::fixed << std::setprecision(2) << res.throughput << " |"; // Throughput precision

                        if(hasMem) outFile << (res.avgMemoryUsage ? std::to_string(static_cast<long long>(*res.avgMemoryUsage)) : "") << " |";
                        if(hasPeakMem) outFile << (res.peakMemoryUsage ? std::to_string(static_cast<long long>(*res.peakMemoryUsage)) : "") << " |";
                        if(hasCpu) {
                            if(res.avgCPUStats) {
                                outFile << res.avgCPUStats->instructionsExecuted << " | " << res.avgCPUStats->cyclesElapsed << " | " << res.avgCPUStats->branchMispredictions << " | " << res.avgCPUStats->cacheMisses << " |";
                            } else {
                                outFile << " | | | |"; // Empty cells
                            }
                        }
                        if(hasIpc) {
                             outFile << std::fixed << std::setprecision(3); // IPC precision
                             outFile << (res.instructionsPerCycle ? std::to_string(*res.instructionsPerCycle) : "") << " |";
                        }
                        outFile << " " << res.sourceLine << " |\n";
                    }
                    outFile << "\n"; // Add space between suites
                }
                break;
            }
            case ExportFormat::PlainText:
            default: {
                outFile << "--- Benchmark Results ---\n";
                for (const auto& [suiteName, suiteResults] : results) {
                    outFile << "Suite: " << suiteName << "\n";
                    for (const auto& result : suiteResults) {
                        outFile << result.toString() << "\n"; // Add extra newline for spacing
                    }
                }
                outFile << "-------------------------\n";
                break;
            }
        }
    } catch (const std::exception& e) {
        // Catch potential exceptions during formatting or writing
        outFile.close(); // Attempt to close the file even on error
        throw std::runtime_error("Error writing benchmark results to file '" + filename + "': " + e.what());
    } catch (...) {
        outFile.close();
        throw std::runtime_error("Unknown error writing benchmark results to file '" + filename + "'.");
    }


    if (!outFile) {
       // Check stream state after writing
       throw std::runtime_error("Failed to write results completely to file: " + filename);
    }

    outFile.close(); // Close the file explicitly
    staticLog(LogLevel::Normal, "Benchmark results successfully exported to " + filename + " in " + Config::formatToString(format) + " format.");
}


// --- Macros ---

/**
 * @brief Macro to define and run a benchmark.
 *
 * @param suiteName Name of the benchmark suite.
 * @param name Name of the benchmark.
 * @param setupFunc Function to set up the benchmark environment.
 * @param func Function to benchmark (must return size_t op count).
 * @param teardownFunc Function to clean up after the benchmark.
 * @param config Configuration settings for the benchmark (Benchmark::Config object).
 */
#define BENCHMARK(suiteName, name, setupFunc, func, teardownFunc, config) \
    do {                                                                  \
        try {                                                             \
            Benchmark bench(suiteName, name, config, std::source_location::current()); \
            bench.run(setupFunc, func, teardownFunc);                     \
        } catch (const std::exception& e) {                               \
            Benchmark::staticLog(Benchmark::LogLevel::Minimal,            \
                "Exception during benchmark setup or execution [" #suiteName "/" #name "]: " + std::string(e.what())); \
        } catch (...) {                                                   \
             Benchmark::staticLog(Benchmark::LogLevel::Minimal,           \
                "Unknown exception during benchmark setup or execution [" #suiteName "/" #name "]"); \
        }                                                                 \
    } while (false)


/**
 * @brief Macro to define and run a benchmark with default configuration.
 *
 * @param suiteName Name of the benchmark suite.
 * @param name Name of the benchmark.
 * @param setupFunc Function to set up the benchmark environment.
 * @param func Function to benchmark (must return size_t op count).
 * @param teardownFunc Function to clean up after the benchmark.
 */
#define BENCHMARK_DEFAULT(suiteName, name, setupFunc, func, teardownFunc) \
    BENCHMARK(suiteName, name, setupFunc, func, teardownFunc, Benchmark::Config{})

/**
 * @brief Macro to define and run a simple benchmark with empty setup and
 * teardown.
 *
 * @param suiteName Name of the benchmark suite.
 * @param name Name of the benchmark.
 * @param func Function to benchmark (void return type, assumes 1 operation).
 */
#define BENCHMARK_SIMPLE(suiteName, name, func)                           \
    BENCHMARK(                                                            \
        suiteName, name, []() -> int { return 0; }, /* Empty setup returns dummy int */ \
        [&]([[maybe_unused]] int& _) -> size_t {                          \
            func();                                                       \
            return 1; /* Assume 1 operation for simple benchmarks */      \
        },                                                                \
        []([[maybe_unused]] int& _) {}, /* Empty teardown */              \
        Benchmark::Config{})


#endif // ATOM_TESTS_BENCHMARK_HPP
