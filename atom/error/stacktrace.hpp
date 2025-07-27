#ifndef ATOM_ERROR_STACKTRACE_HPP
#define ATOM_ERROR_STACKTRACE_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef ATOM_USE_BOOST
#include <boost/container/flat_map.hpp>
#include <boost/container/small_vector.hpp>
#endif

namespace atom::error {

/**
 * @brief Configuration for StackTrace behavior and performance tuning
 */
struct StackTraceConfig {
    size_t maxFrames = 128;          ///< Maximum number of frames to capture
    size_t cacheMaxSize = 1000;      ///< Maximum cache entries
    bool enableCaching = true;       ///< Enable symbol caching
    bool enablePrettify = true;      ///< Enable output prettification
    bool enableAsync = false;        ///< Enable asynchronous processing
    bool enableCompression = false;  ///< Enable trace compression
    size_t skipFrames = 1;           ///< Number of frames to skip
    std::chrono::milliseconds cacheTimeout{
        300000};  ///< Cache entry timeout (5 min)

    /// Output format options
    enum class OutputFormat {
        SIMPLE,    ///< Simple text format
        DETAILED,  ///< Detailed text with metadata
        JSON,      ///< JSON format
        XML        ///< XML format
    } outputFormat = OutputFormat::SIMPLE;

    /// Performance monitoring options
    bool enablePerfMonitoring = false;  ///< Enable performance metrics
    bool enableMemoryTracking = false;  ///< Enable memory usage tracking
};

/**
 * @brief Performance metrics for stacktrace operations
 */
struct StackTraceMetrics {
    std::atomic<uint64_t> captureCount{0};
    std::atomic<uint64_t> totalCaptureTime{0};  ///< In nanoseconds
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};
    std::atomic<uint64_t> memoryUsage{0};  ///< In bytes

    // Copy constructor
    StackTraceMetrics(const StackTraceMetrics& other)
        : captureCount(other.captureCount.load()),
          totalCaptureTime(other.totalCaptureTime.load()),
          cacheHits(other.cacheHits.load()),
          cacheMisses(other.cacheMisses.load()),
          memoryUsage(other.memoryUsage.load()) {}

    // Move constructor
    StackTraceMetrics(StackTraceMetrics&& other) noexcept
        : captureCount(other.captureCount.load()),
          totalCaptureTime(other.totalCaptureTime.load()),
          cacheHits(other.cacheHits.load()),
          cacheMisses(other.cacheMisses.load()),
          memoryUsage(other.memoryUsage.load()) {}

    // Default constructor
    StackTraceMetrics() = default;

    // Copy assignment
    StackTraceMetrics& operator=(const StackTraceMetrics& other) {
        if (this != &other) {
            captureCount = other.captureCount.load();
            totalCaptureTime = other.totalCaptureTime.load();
            cacheHits = other.cacheHits.load();
            cacheMisses = other.cacheMisses.load();
            memoryUsage = other.memoryUsage.load();
        }
        return *this;
    }

    // Move assignment
    StackTraceMetrics& operator=(StackTraceMetrics&& other) noexcept {
        if (this != &other) {
            captureCount = other.captureCount.load();
            totalCaptureTime = other.totalCaptureTime.load();
            cacheHits = other.cacheHits.load();
            cacheMisses = other.cacheMisses.load();
            memoryUsage = other.memoryUsage.load();
        }
        return *this;
    }

    void reset() {
        captureCount = 0;
        totalCaptureTime = 0;
        cacheHits = 0;
        cacheMisses = 0;
        memoryUsage = 0;
    }

    double getAverageCaptureTime() const {
        auto count = captureCount.load();
        return count > 0 ? static_cast<double>(totalCaptureTime.load()) / count
                         : 0.0;
    }

    double getCacheHitRatio() const {
        auto hits = cacheHits.load();
        auto misses = cacheMisses.load();
        auto total = hits + misses;
        return total > 0 ? static_cast<double>(hits) / total : 0.0;
    }
};

/**
 * @brief Information about a single stack frame
 */
struct FrameInfo {
    void* address = nullptr;   ///< Frame address
    std::string functionName;  ///< Demangled function name
    std::string moduleName;    ///< Module/library name
    std::string fileName;      ///< Source file name
    int lineNumber = 0;        ///< Line number
    uintptr_t offset = 0;      ///< Offset within module
    std::chrono::system_clock::time_point timestamp;  ///< Capture timestamp

    /// Convert to string representation
    [[nodiscard]] auto toString() const -> std::string;

    /// Convert to JSON representation
    [[nodiscard]] auto toJson() const -> std::string;

    /// Convert to XML representation
    [[nodiscard]] auto toXml() const -> std::string;
};

/**
 * @brief Class for capturing and representing a stack trace with enhanced
 * details and performance optimizations.
 *
 * This class captures the stack trace of the current execution context and
 * represents it in various formats, including file names, line numbers,
 * function names, module information, and memory addresses when available.
 * Features include intelligent caching, memory optimization, thread safety, and
 * performance monitoring.
 */
class StackTrace {
public:
    /**
     * @brief Default constructor that captures the current stack trace.
     */
    StackTrace();

    /**
     * @brief Constructor with custom configuration.
     * @param config Configuration for stacktrace behavior
     */
    explicit StackTrace(const StackTraceConfig& config);

    /**
     * @brief Copy constructor with optimized copying
     */
    StackTrace(const StackTrace& other);

    /**
     * @brief Move constructor
     */
    StackTrace(StackTrace&& other) noexcept;

    /**
     * @brief Copy assignment operator
     */
    StackTrace& operator=(const StackTrace& other);

    /**
     * @brief Move assignment operator
     */
    StackTrace& operator=(StackTrace&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~StackTrace() = default;

    /**
     * @brief Get the string representation of the stack trace.
     * @return A string representing the captured stack trace with enhanced
     * details.
     */
    [[nodiscard]] auto toString() const -> std::string;

    /**
     * @brief Get the string representation with custom format.
     * @param format Output format to use
     * @return Formatted string representation
     */
    [[nodiscard]] auto toString(StackTraceConfig::OutputFormat format) const
        -> std::string;

    /**
     * @brief Get structured frame information.
     * @return Vector of frame information structures
     */
    [[nodiscard]] auto getFrames() const -> std::vector<FrameInfo>;

    /**
     * @brief Get performance metrics for this instance.
     * @return Current performance metrics
     */
    [[nodiscard]] auto getMetrics() const -> const StackTraceMetrics&;

    /**
     * @brief Set configuration for this instance.
     * @param config New configuration
     */
    void setConfig(const StackTraceConfig& config);

    /**
     * @brief Get current configuration.
     * @return Current configuration
     */
    [[nodiscard]] auto getConfig() const -> const StackTraceConfig&;

    /**
     * @brief Clear internal caches.
     */
    void clearCache();

    /**
     * @brief Get cache statistics.
     * @return Cache hit ratio and size information
     */
    [[nodiscard]] auto getCacheStats() const -> std::pair<double, size_t>;

    /**
     * @brief Static method to set global default configuration.
     * @param config Default configuration for new instances
     */
    static void setDefaultConfig(const StackTraceConfig& config);

    /**
     * @brief Static method to get global performance metrics.
     * @return Global performance metrics across all instances
     */
    static auto getGlobalMetrics() -> StackTraceMetrics&;

    /**
     * @brief Filter function type for frame filtering
     */
    using FrameFilter = std::function<bool(const FrameInfo&)>;

    /**
     * @brief Add a filter for frame processing
     * @param filter Filter function to apply
     */
    void addFilter(const FrameFilter& filter);

    /**
     * @brief Remove all filters
     */
    void clearFilters();

    /**
     * @brief Get filtered frames
     * @return Vector of frames that pass all filters
     */
    [[nodiscard]] auto getFilteredFrames() const -> std::vector<FrameInfo>;

    /**
     * @brief Capture stacktrace asynchronously
     * @return Future containing the captured stacktrace
     */
    [[nodiscard]] static auto captureAsync() -> std::future<StackTrace>;

    /**
     * @brief Capture stacktrace asynchronously with custom config
     * @param config Configuration to use
     * @return Future containing the captured stacktrace
     */
    [[nodiscard]] static auto captureAsync(const StackTraceConfig& config)
        -> std::future<StackTrace>;

    /**
     * @brief Compress stacktrace string representation using atom::io
     * compression
     * @param input String to compress
     * @return Compressed string (base64 encoded) or original if compression
     * fails/not beneficial
     */
    [[nodiscard]] static auto compress(const std::string& input) -> std::string;

    /**
     * @brief Decompress stacktrace string representation using atom::io
     * decompression
     * @param compressed Compressed string (base64 encoded)
     * @return Decompressed string or original if decompression fails
     */
    [[nodiscard]] static auto decompress(const std::string& compressed)
        -> std::string;

    /**
     * @brief Batch process multiple stacktraces
     * @param traces Vector of stacktraces to process
     * @param format Output format
     * @return Combined formatted output
     */
    [[nodiscard]] static auto batchProcess(
        const std::vector<StackTrace>& traces,
        StackTraceConfig::OutputFormat format) -> std::string;

private:
    /**
     * @brief LRU Cache entry for symbol information
     */
    struct CacheEntry {
        std::string value;
        std::chrono::steady_clock::time_point lastAccess;
        size_t accessCount = 1;

        CacheEntry(std::string val)
            : value(std::move(val)),
              lastAccess(std::chrono::steady_clock::now()) {}
    };

    /**
     * @brief Thread-safe LRU cache for symbol resolution
     */
    class SymbolCache {
    public:
        explicit SymbolCache(size_t maxSize, std::chrono::milliseconds timeout);

        auto get(void* key) -> std::optional<std::string>;
        void put(void* key, const std::string& value);
        void clear();
        auto getStats() const -> std::pair<double, size_t>;

    private:
        mutable std::shared_mutex mutex_;
        std::unordered_map<void*, CacheEntry> cache_;
        size_t maxSize_;
        std::chrono::milliseconds timeout_;
        mutable std::atomic<uint64_t> hits_{0};
        mutable std::atomic<uint64_t> misses_{0};

        void evictOldEntries();
        void evictLRU();
    };

    /**
     * @brief Capture the current stack trace based on the operating system.
     */
    void capture();

    /**
     * @brief Process a stack frame to extract detailed information.
     * @param frame The stack frame to process.
     * @param frameIndex The index of the frame in the stack.
     * @return FrameInfo containing the processed frame information.
     */
    [[nodiscard]] auto processFrame(void* frame, int frameIndex) const
        -> FrameInfo;

    /**
     * @brief Format frames according to specified output format.
     * @param frames Vector of frame information
     * @param format Output format to use
     * @return Formatted string representation
     */
    [[nodiscard]] auto formatFrames(const std::vector<FrameInfo>& frames,
                                    StackTraceConfig::OutputFormat format) const
        -> std::string;

    /**
     * @brief Apply prettification to stacktrace output.
     * @param input Raw stacktrace string
     * @return Prettified string
     */
    [[nodiscard]] auto prettifyOutput(const std::string& input) const
        -> std::string;

    // Configuration and metrics
    StackTraceConfig config_;
    mutable StackTraceMetrics metrics_;

    // Frame filtering
    std::vector<FrameFilter> filters_;
    mutable std::shared_mutex filterMutex_;

    // Frame storage with optimized allocation
#ifdef ATOM_USE_BOOST
    boost::container::small_vector<void*, 32> frames_;
#else
    std::vector<void*> frames_;
#endif

    // Platform-specific members
#ifdef _WIN32
    mutable std::unique_ptr<SymbolCache> moduleCache_;
#elif defined(__APPLE__) || defined(__linux__)
    std::unique_ptr<char*, decltype(&free)> symbols_{nullptr, &free};
    int num_frames_ = 0;
    mutable std::unique_ptr<SymbolCache> symbolCache_;
#endif

    // Static members for global configuration and metrics
    static StackTraceConfig defaultConfig_;
    static StackTraceMetrics globalMetrics_;
    static std::mutex globalMutex_;
};

}  // namespace atom::error

#endif
