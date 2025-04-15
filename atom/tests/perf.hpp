#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <source_location>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <variant>

/**
 * @class Perf
 * @brief A modern C++ performance measurement and profiling utility.
 *
 * Perf provides a RAII-style interface for measuring code execution time
 * with minimal overhead. It supports nested measurements, multi-threading,
 * asynchronous logging, and various reporting formats.
 *
 * Usage example:
 * ```cpp
 * void my_function() {
 *     PERF; // Automatically measures this function's execution time
 *
 *     // For custom tag:
 *     PERF_TAG("custom operation");
 *
 *     // Manual scope:
 *     {
 *         Perf p("manual measurement");
 *         // Code to measure...
 *     }
 * }
 *
 * int main() {
 *     // Configure perf settings
 *     Perf::Config config;
 *     config.outputPath = "perf_results.json";
 *     config.asyncLogging = true;
 *     config.minimumDuration = std::chrono::milliseconds(1);
 *     Perf::setConfig(config);
 *
 *     // Run your code...
 *
 *     // Generate reports at the end
 *     Perf::finalize();
 *     return 0;
 * }
 * ```
 */
class Perf {
public:
    /**
     * @struct Location
     * @brief Represents a source code location with optional tag
     */
    struct Location {
        const char* func;
        const char* file;
        int line;
        const char* tag;

#if __cpp_lib_source_location
        /**
         * @brief Construct location from source_location with optional tag
         * @param loc Source location (automatically provided by compiler)
         * @param tag Optional descriptive tag
         */
        explicit Location(
            std::source_location const& loc = std::source_location::current(),
            const char* tag = "");

        /**
         * @brief Construct location with custom function name and source
         * location
         * @param func Custom function name
         * @param loc Source location
         * @param tag Optional descriptive tag
         */
        explicit Location(
            const char* func,
            std::source_location const& loc = std::source_location::current(),
            const char* tag = "");
#endif
        /**
         * @brief Manual location constructor
         */
        Location(const char* func = "?", const char* file = "???", int line = 0,
                 const char* tag = "");

        bool operator<(const Location& rhs) const;
    };

    struct PerfTableEntry {
        std::thread::id threadId;
        std::uint64_t t0;
        std::uint64_t t1;
        Location location;

        PerfTableEntry(std::uint64_t t0, std::uint64_t t1, Location location);
    };

    class PerfEntry {
    public:
        PerfEntry(std::chrono::high_resolution_clock::time_point start,
                  std::chrono::high_resolution_clock::time_point end,
                  Location location, std::thread::id threadId);

        std::chrono::nanoseconds duration() const;
        auto startTimeRaw() const;
        auto endTimeRaw() const;
        const Location& location() const;
        std::thread::id threadId() const;

    private:
        std::chrono::high_resolution_clock::time_point start_;
        std::chrono::high_resolution_clock::time_point end_;
        Location location_;
        std::thread::id threadId_;
    };

    /**
     * @enum OutputFormat
     * @brief Supported output formats for performance data
     */
    enum class OutputFormat {
        JSON,       ///< JSON format (most detailed)
        CSV,        ///< CSV format (good for importing to spreadsheets)
        TEXT,       ///< Human-readable text format
        FLAMEGRAPH  ///< Format suitable for flamegraph visualization
    };

    /**
     * @struct Config
     * @brief Configuration options for the Perf system
     */
    struct Config {
        std::optional<std::filesystem::path>
            outputPath;  ///< Path for output files (nullptr for no file output)
        bool asyncLogging{true};          ///< Enable asynchronous logging
        bool captureNestedEvents{true};   ///< Track parent-child relationships
        bool generateThreadReport{true};  ///< Generate per-thread reports
        std::chrono::nanoseconds minimumDuration{
            0};  ///< Minimum duration to record
        size_t maxEventsPerThread{
            10000};  ///< Maximum events to store per thread
        size_t maxQueueSize{
            100000};  ///< Maximum async queue size before blocking
        std::vector<OutputFormat> outputFormats{
            OutputFormat::JSON};  ///< Output formats to generate

        Config() : outputPath(std::nullopt) {}
    };

    /**
     * @brief Set global configuration for the Perf system
     * @param config Configuration object
     */
    static void setConfig(const Config& config);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    static const Config& getConfig();

    /**
     * @brief Filter for selecting specific performance entries
     */
    struct PerfFilter {
        std::uint64_t minDuration{0};
        std::string funcContains;

        bool match(const PerfTableEntry& entry) const;
    };

    /**
     * @brief Generate a filtered report of performance data
     * @param filter Filter criteria
     */
    static void generateFilteredReport(const PerfFilter& filter);

    /**
     * @brief Construct a performance measurement object
     * @param location Source location
     *
     * Starts timing when constructed and automatically stops and records
     * when destroyed.
     */
    explicit Perf(Location location);

    /**
     * @brief Destructor - records elapsed time
     */
    ~Perf();

    Perf(const Perf&) = delete;
    Perf& operator=(const Perf&) = delete;

#define PERF_TAG(tag) Perf({__func__, __FILE__, __LINE__, tag})
#define PERF Perf(Perf::Location(__func__, __FILE__, __LINE__))

    /**
     * @brief Add a custom attribute to this measurement
     * @param key Attribute name
     * @param value Attribute value (supports string, number, bool)
     */
    template <typename T>
    void addAttribute(std::string key, T value) {
        if constexpr (std::is_arithmetic_v<T> ||
                      std::is_same_v<T, std::string> ||
                      std::is_same_v<T, bool>) {
            attributes_.emplace(std::move(key), value);
        } else {
            attributes_.emplace(std::move(key), std::to_string(value));
        }
    }

    /**
     * @brief Finalize performance tracking and generate reports
     *
     * Call this at the end of your program to ensure all data is
     * properly flushed and reports are generated.
     */
    static void finalize();

    class PerfGather {
    public:
        PerfGather();
        void exportToJSON(const std::string& filename);
        void generateThreadReport();

        std::mutex lock;
        std::vector<PerfTableEntry> table;
        std::string output_path;
        const char* output;
    };

private:
    Location location_;
    std::uint64_t t0_;
    std::unordered_map<std::string,
                       std::variant<int64_t, double, std::string, bool>>
        attributes_;

    struct PerfThreadLocal {
        void startNested(std::uint64_t t0);
        void endNested(std::uint64_t t1);

        std::vector<std::uint64_t> stack;
        std::deque<PerfTableEntry> table;
    };

    class PerfAsyncLogger {
    public:
        PerfAsyncLogger();
        ~PerfAsyncLogger();
        void log(const PerfTableEntry& entry);
        void stop();

    private:
        void run();

        std::mutex mutex;
        std::condition_variable cv;
        std::queue<PerfTableEntry> queue;
        bool done = false;
        std::thread worker;
    };

    static inline thread_local PerfThreadLocal perthread;
    static inline PerfAsyncLogger asyncLogger;
    static inline PerfGather gathered;
    static inline Config config_;
};

/**
 * @brief Measure performance of a function and automatically name it
 * @param func Function to measure
 * @param args Arguments for the function
 * @return Result of the function
 */
template <typename Func, typename... Args>
auto measure(Func&& func, Args&&... args) {
    PERF;
    return std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
}

/**
 * @brief Measure performance with a custom tag
 * @param tag Custom measurement tag
 * @param func Function to measure
 * @param args Arguments for the function
 * @return Result of the function
 */
template <typename Func, typename... Args>
auto measureWithTag(const char* tag, Func&& func, Args&&... args) {
    Perf p({__func__, __FILE__, __LINE__, tag});
    return std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
}
