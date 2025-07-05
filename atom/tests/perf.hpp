#pragma once

#include <spdlog/spdlog.h>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

// Platform-specific optimizations
#ifdef _WIN32
#include <intrin.h>
#define PERF_RDTSC() __rdtsc()
#elif defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#define PERF_RDTSC() __rdtsc()
#else
#define PERF_RDTSC() 0ULL
#endif

// SIMD support detection
#if defined(__AVX2__)
#define PERF_HAS_AVX2 1
#include <immintrin.h>
#endif
#if defined(__SSE4_2__)
#define PERF_HAS_SSE42 1
#include <nmmintrin.h>
#endif

// Forward declarations for optimized components
namespace perf_internal {
// String interning for reduced memory usage and faster comparisons
class StringPool {
public:
    const char* intern(std::string_view str);
    void clear();
    size_t size() const { return pool_.size(); }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<char[]>> pool_;
};

// Platform-specific high-resolution timer
class HighResTimer {
public:
    static std::uint64_t now() noexcept;
    static double to_nanoseconds(std::uint64_t ticks) noexcept;
    static void calibrate();

private:
    static std::atomic<double> ticks_per_ns_;
    static std::atomic<bool> calibrated_;
};

// SIMD-optimized string operations
namespace simd {
bool fast_strcmp(const char* a, const char* b) noexcept;
size_t fast_strlen(const char* str) noexcept;
void fast_memcpy(void* dst, const void* src, size_t size) noexcept;
}  // namespace simd
}  // namespace perf_internal

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
     * @brief Represents a source code location with optional tag (optimized)
     */
    struct Location {
        const char* func;
        const char* file;
        int line;
        const char* tag;

        // Hash for faster lookups - remove atomic to make copyable
        mutable std::size_t hash_cache_{0};

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
        bool operator==(const Location& rhs) const noexcept;
        std::size_t hash() const noexcept;
    };

    struct PerfTableEntry {
        std::thread::id threadId;
        std::uint64_t t0;
        std::uint64_t t1;
        Location location;

        // Optimized storage for attributes
        std::array<std::pair<const char*,
                             std::variant<int64_t, double, const char*, bool>>,
                   4>
            attributes;
        std::uint8_t attribute_count{0};

        // Default constructor for arrays
        PerfTableEntry()
            : t0(0), t1(0), location("", "", 0, ""), attribute_count(0) {}
        PerfTableEntry(std::uint64_t t0, std::uint64_t t1, Location location);

        // Fast access methods
        std::uint64_t duration() const noexcept { return t1 - t0; }
        bool has_attribute(const char* key) const noexcept;
        template <typename T>
        bool get_attribute(const char* key, T& value) const noexcept;
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
     * @brief Configuration options for the Perf system (enhanced)
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

        // New optimization options
        bool useStringInterning{
            true};  ///< Enable string interning for memory efficiency
        bool useLockFreeStructures{true};  ///< Enable lock-free data structures
        bool useHighResTimer{true};  ///< Use platform-specific high-res timer
        bool useSIMDOptimizations{
            true};                ///< Enable SIMD optimizations where available
        size_t bufferSize{8192};  ///< Size of circular buffers
        std::chrono::nanoseconds calibrationPeriod{
            std::chrono::seconds(1)};    ///< Timer calibration period
        double overheadThreshold{0.05};  ///< Maximum acceptable overhead ratio

        Config() {}  // Explicit constructor instead of default
    };

    /**
     * @brief Filter for selecting specific performance entries
     */
    struct PerfFilter {
        std::uint64_t minDuration{0};
        std::string funcContains;

        bool match(const PerfTableEntry& entry) const;
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

    /**
     * @brief Add a custom attribute to this measurement (optimized)
     * @param key Attribute name
     * @param value Attribute value (supports string, number, bool)
     */
    template <typename T>
    void addAttribute(const char* key, T value) {
        if (attribute_count_ >= attributes_.size())
            return;

        if constexpr (std::is_arithmetic_v<T> || std::is_same_v<T, bool>) {
            attributes_[attribute_count_++] = {key, value};
        } else if constexpr (std::is_same_v<T, std::string>) {
            // Store as const char* for efficiency
            attributes_[attribute_count_++] = {key, value.c_str()};
        } else if constexpr (std::is_convertible_v<T, const char*>) {
            attributes_[attribute_count_++] = {key, value};
        } else {
            // Convert to string - note: this creates a temporary
            std::string str_val = std::to_string(value);
            attributes_[attribute_count_++] = {key, str_val.c_str()};
        }
    }

    /**
     * @brief Finalize performance tracking and generate reports
     *
     * Call this at the end of your program to ensure all data is
     * properly flushed and reports are generated.
     */
    static void finalize();

    /**
     * @brief Initialize the Perf logging system (call before first use)
     */
    static void initialize();

private:
    Location location_;
    std::uint64_t t0_;
    std::array<std::pair<const char*,
                         std::variant<int64_t, double, const char*, bool>>,
               4>
        attributes_;
    std::uint8_t attribute_count_{0};

    // Optimized thread-local storage
    struct alignas(64) PerfThreadLocal {
        PerfThreadLocal()
            : stack_size{0},
              head{0},
              tail{0},
              total_measurements{0},
              total_duration{0},
              overhead_ticks{0} {
            // Initialize stack array
            stack.fill(0);
        }

        void startNested(std::uint64_t t0);
        void endNested(std::uint64_t t1);

        std::array<std::uint64_t, 32>
            stack;  // Fixed-size stack for better cache performance
        std::uint8_t stack_size;

        // Simple circular buffer for entries - Note: initialized by default
        // constructor
        std::array<PerfTableEntry, 1024>
            entries;  // Will use default initialization
        std::atomic<size_t> head;
        std::atomic<size_t> tail;

        // Statistics
        alignas(8) std::atomic<std::uint64_t> total_measurements;
        alignas(8) std::atomic<std::uint64_t> total_duration;
        alignas(8) std::atomic<std::uint64_t> overhead_ticks;

        // Helper methods for circular buffer
        bool try_push(const PerfTableEntry& entry);
        bool try_pop(PerfTableEntry& entry);
        size_t size() const;
    };

    // High-performance async logger
    class PerfAsyncLogger {
    public:
        PerfAsyncLogger();
        ~PerfAsyncLogger();
        bool try_log(const PerfTableEntry& entry) noexcept;
        void flush();
        void stop();

        // Statistics
        std::atomic<std::uint64_t> entries_logged{0};
        std::atomic<std::uint64_t> entries_dropped{0};

    private:
        void run();
        void process_batch();

        alignas(64) std::atomic<bool> done_{false};
        alignas(64) std::atomic<bool> flush_requested_{false};

        // Simple lock-free queue implementation
        std::array<PerfTableEntry, 16384> queue_;
        alignas(64) std::atomic<size_t> head_{0};
        alignas(64) std::atomic<size_t> tail_{0};

        std::thread worker_;
        std::shared_ptr<spdlog::logger> logger_;

        // Batch processing
        std::array<PerfTableEntry, 256> batch_buffer_;

        // Helper methods for circular buffer
        bool try_enqueue(const PerfTableEntry& entry);
        bool try_dequeue(PerfTableEntry& entry);
        size_t queue_size() const;
    };

    class PerfGather {
    public:
        PerfGather();
        void addEntry(const PerfTableEntry& entry);
        void exportToJSON(const std::string& filename);
        void generateThreadReport();
        void generateStatistics();

        // Lock-free statistics
        alignas(64) std::atomic<std::uint64_t> total_entries{0};
        alignas(64) std::atomic<std::uint64_t> total_duration{0};
        alignas(64) std::atomic<std::uint64_t> min_duration{UINT64_MAX};
        alignas(64) std::atomic<std::uint64_t> max_duration{0};

        mutable std::shared_mutex
            table_mutex;  // Reader-writer lock for better concurrency
        std::vector<PerfTableEntry> table;
        std::string output_path;
        const char* output;

    private:
        void update_statistics(const PerfTableEntry& entry) noexcept;
    };

    static inline thread_local PerfThreadLocal perthread;
    static inline PerfAsyncLogger asyncLogger;
    static inline PerfGather gathered;
    static inline Config config_;
    static inline std::shared_ptr<spdlog::logger> logger;
    static inline perf_internal::StringPool string_pool_;

    // Performance measurement overhead tracking
    static inline std::atomic<std::uint64_t> total_overhead_ticks_{0};
    static inline std::atomic<std::uint64_t> measurement_count_{0};
};

// Convenience macros
#define PERF_TAG(tag) Perf({__func__, __FILE__, __LINE__, tag})
#define PERF Perf(Perf::Location(__func__, __FILE__, __LINE__))

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