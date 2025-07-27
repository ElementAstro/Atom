#ifndef ATOM_MEMORY_TRACKER_HPP
#define ATOM_MEMORY_TRACKER_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <immintrin.h>  // For memory prefetching

// Cache line size for alignment optimizations
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#include "atom/error/stacktrace.hpp"

namespace atom::memory {

/**
 * @brief Enhanced memory tracking system configuration options
 */
struct MemoryTrackerConfig {
    // Basic tracking options
    bool enabled = true;           // Whether tracking is enabled
    bool trackStackTrace = true;   // Whether to track call stack
    bool autoReportLeaks = true;   // Automatically report leaks at program exit
    bool logToConsole = true;      // Whether to output to console
    std::string logFilePath;       // Log file path (empty means no file output)
    size_t maxStackFrames = 16;    // Maximum number of stack frames
    size_t minAllocationSize = 0;  // Minimum allocation size to track
    bool trackAllocationCount = true; // Track allocation and deallocation counts
    bool trackPeakMemory = true;   // Track peak memory usage

    // Advanced tracking features
    bool enableLeakPatternDetection = true;  // Enable leak pattern analysis
    bool enablePerformanceProfiling = true;  // Enable performance profiling
    bool enableMemoryHotspots = true;        // Track memory allocation hotspots
    bool enableFragmentationAnalysis = true; // Analyze memory fragmentation
    bool enableLifetimeAnalysis = true;      // Track allocation lifetimes
    bool enableThreadAnalysis = true;        // Per-thread memory analysis
    bool enableRealTimeMonitoring = false;   // Real-time memory monitoring
    bool enableMemoryPressureDetection = true; // Detect memory pressure

    // Performance and optimization
    bool enableCaching = true;               // Cache allocation info for performance
    bool enableBatchReporting = true;        // Batch leak reports for performance
    size_t reportingBatchSize = 100;         // Number of leaks to batch
    std::chrono::milliseconds samplingInterval{1000}; // Sampling interval for monitoring
    size_t maxCachedAllocations = 10000;     // Maximum cached allocations

    // Pattern detection settings
    size_t leakPatternThreshold = 5;         // Minimum occurrences for pattern
    size_t hotspotsTopN = 10;               // Number of top hotspots to track
    std::chrono::seconds maxAllocationAge{3600}; // Maximum age for active tracking

    // Callbacks and customization
    std::function<void(const std::string&)> errorCallback = nullptr;
    std::function<void(const std::string&)> leakPatternCallback = nullptr;
    std::function<void(const std::string&)> performanceCallback = nullptr;
    std::function<bool(const std::string&)> fileFilter = nullptr; // Filter files to track
};

/**
 * @brief Enhanced memory allocation information structure
 */
struct alignas(CACHE_LINE_SIZE) AllocationInfo {
    void* address;                                    // Memory address
    size_t size;                                      // Allocation size
    std::chrono::steady_clock::time_point timestamp;  // Allocation timestamp
    std::string sourceFile;                           // Source file
    int sourceLine;                                   // Source file line number
    std::string sourceFunction;                       // Source function
    std::thread::id threadId;                         // Thread ID
    std::vector<std::string> stackTrace;              // Call stack

    // Enhanced tracking data
    size_t allocationId;                              // Unique allocation ID
    std::chrono::nanoseconds allocationDuration{0};  // Time taken to allocate
    size_t alignmentRequirement;                      // Memory alignment used
    std::string allocationCategory;                   // Category/tag for allocation
    uint32_t accessCount{0};                         // Number of times accessed
    std::chrono::steady_clock::time_point lastAccess; // Last access time
    bool isHotspot{false};                           // Whether this is a hotspot
    size_t fragmentationScore{0};                    // Fragmentation contribution
    std::string allocatorType;                       // Type of allocator used

    // Pattern detection data
    std::string patternSignature;                    // Signature for pattern matching
    size_t sequenceNumber{0};                       // Sequence in allocation pattern
    bool isLeakCandidate{false};                    // Whether this might be a leak

    AllocationInfo(void* addr, size_t sz, const std::string& file = "",
                   int line = 0, const std::string& func = "")
        : address(addr),
          size(sz),
          timestamp(std::chrono::steady_clock::now()),
          sourceFile(file),
          sourceLine(line),
          sourceFunction(func),
          threadId(std::this_thread::get_id()),
          allocationId(0),
          alignmentRequirement(sizeof(void*)),
          lastAccess(timestamp) {

        // Generate pattern signature
        patternSignature = generatePatternSignature();
    }

private:
    std::string generatePatternSignature() const {
        // Create a signature based on file, line, and function for pattern detection
        return sourceFile + ":" + std::to_string(sourceLine) + ":" + sourceFunction;
    }
};

/**
 * @brief Enhanced memory statistics information with advanced metrics
 */
struct alignas(CACHE_LINE_SIZE) MemoryStatistics {
    // Basic statistics
    std::atomic<size_t> currentAllocations{0};    // Current number of allocations
    std::atomic<size_t> currentMemoryUsage{0};    // Current memory usage
    std::atomic<size_t> totalAllocations{0};      // Total allocation count
    std::atomic<size_t> totalDeallocations{0};    // Total deallocation count
    std::atomic<size_t> totalMemoryAllocated{0};  // Total memory allocated
    std::atomic<size_t> peakMemoryUsage{0};       // Peak memory usage
    std::atomic<size_t> largestSingleAllocation{0}; // Largest single allocation

    // Advanced performance metrics
    std::atomic<uint64_t> totalAllocationTime{0}; // Total allocation time (ns)
    std::atomic<uint64_t> totalDeallocationTime{0}; // Total deallocation time (ns)
    std::atomic<uint64_t> maxAllocationTime{0};   // Maximum allocation time (ns)
    std::atomic<uint64_t> maxDeallocationTime{0}; // Maximum deallocation time (ns)
    std::atomic<size_t> allocationHotspots{0};    // Number of allocation hotspots
    std::atomic<size_t> memoryFragmentationEvents{0}; // Fragmentation events

    // Leak detection metrics
    std::atomic<size_t> potentialLeaks{0};        // Potential memory leaks detected
    std::atomic<size_t> leakPatterns{0};          // Leak patterns identified
    std::atomic<size_t> longLivedAllocations{0};  // Long-lived allocations
    std::atomic<size_t> shortLivedAllocations{0}; // Short-lived allocations

    // Thread-specific metrics
    std::atomic<size_t> threadContentions{0};     // Thread contention events
    std::atomic<size_t> crossThreadDeallocations{0}; // Cross-thread deallocations

    // Memory pressure metrics
    std::atomic<size_t> memoryPressureEvents{0};  // Memory pressure events
    std::atomic<size_t> allocationFailures{0};    // Failed allocations
    std::atomic<size_t> emergencyCleanups{0};     // Emergency cleanup events

    auto operator=(const MemoryStatistics& other) -> MemoryStatistics& {
        currentAllocations = other.currentAllocations.load();
        currentMemoryUsage = other.currentMemoryUsage.load();
        totalAllocations = other.totalAllocations.load();
        totalDeallocations = other.totalDeallocations.load();
        totalMemoryAllocated = other.totalMemoryAllocated.load();
        peakMemoryUsage = other.peakMemoryUsage.load();
        largestSingleAllocation = other.largestSingleAllocation.load();
        return *this;
    }
    auto operator==(const MemoryStatistics& other) const -> bool {
        return currentAllocations == other.currentAllocations.load() &&
               currentMemoryUsage == other.currentMemoryUsage.load() &&
               totalAllocations == other.totalAllocations.load() &&
               totalDeallocations == other.totalDeallocations.load() &&
               totalMemoryAllocated == other.totalMemoryAllocated.load() &&
               peakMemoryUsage == other.peakMemoryUsage.load() &&
               largestSingleAllocation == other.largestSingleAllocation.load();
    }
    auto operator!=(const MemoryStatistics& other) const -> bool {
        return !(*this == other);
    }
    auto operator+=(const MemoryStatistics& other) -> MemoryStatistics& {
        currentAllocations += other.currentAllocations.load();
        currentMemoryUsage += other.currentMemoryUsage.load();
        totalAllocations += other.totalAllocations.load();
        totalDeallocations += other.totalDeallocations.load();
        totalMemoryAllocated += other.totalMemoryAllocated.load();
        peakMemoryUsage =
            std::max(peakMemoryUsage.load(), other.peakMemoryUsage.load());
        largestSingleAllocation =
            std::max(largestSingleAllocation.load(),
                     other.largestSingleAllocation.load());
        return *this;
    }

    // Performance calculation helpers
    double getAverageAllocationTime() const noexcept {
        size_t count = totalAllocations.load();
        return count > 0 ? static_cast<double>(totalAllocationTime.load()) / count : 0.0;
    }

    double getAverageDeallocationTime() const noexcept {
        size_t count = totalDeallocations.load();
        return count > 0 ? static_cast<double>(totalDeallocationTime.load()) / count : 0.0;
    }

    double getMemoryEfficiency() const noexcept {
        size_t peak = peakMemoryUsage.load();
        size_t total = totalMemoryAllocated.load();
        return total > 0 ? static_cast<double>(peak) / total : 0.0;
    }

    double getLeakRatio() const noexcept {
        size_t current = currentAllocations.load();
        size_t total = totalAllocations.load();
        return total > 0 ? static_cast<double>(current) / total : 0.0;
    }
};

/**
 * @brief Leak pattern information for pattern detection
 */
struct LeakPattern {
    std::string signature;                    // Pattern signature
    size_t occurrences{0};                   // Number of occurrences
    size_t totalSize{0};                     // Total memory leaked by this pattern
    std::vector<std::string> stackTraces;    // Representative stack traces
    std::chrono::steady_clock::time_point firstSeen; // First occurrence
    std::chrono::steady_clock::time_point lastSeen;  // Last occurrence
    double confidence{0.0};                  // Confidence score (0.0-1.0)

    LeakPattern(const std::string& sig)
        : signature(sig), firstSeen(std::chrono::steady_clock::now()), lastSeen(firstSeen) {}
};

/**
 * @brief Memory hotspot information for performance analysis
 */
struct MemoryHotspot {
    std::string location;                    // Source location (file:line:function)
    size_t allocationCount{0};              // Number of allocations
    size_t totalSize{0};                    // Total memory allocated
    size_t averageSize{0};                  // Average allocation size
    std::chrono::nanoseconds totalTime{0};  // Total time spent allocating
    std::chrono::nanoseconds averageTime{0}; // Average allocation time
    double hotspotScore{0.0};               // Hotspot score (0.0-1.0)

    void updateMetrics() {
        if (allocationCount > 0) {
            averageSize = totalSize / allocationCount;
            averageTime = totalTime / allocationCount;
            // Calculate hotspot score based on frequency and time
            hotspotScore = (allocationCount * 0.6) + (totalTime.count() * 0.4);
        }
    }
};

/**
 * @brief Thread-specific memory statistics
 */
struct ThreadMemoryStats {
    std::thread::id threadId;
    std::atomic<size_t> allocations{0};
    std::atomic<size_t> deallocations{0};
    std::atomic<size_t> currentMemory{0};
    std::atomic<size_t> peakMemory{0};
    std::atomic<size_t> crossThreadFrees{0};
    std::chrono::steady_clock::time_point firstActivity;
    std::chrono::steady_clock::time_point lastActivity;

    ThreadMemoryStats(std::thread::id id)
        : threadId(id), firstActivity(std::chrono::steady_clock::now()), lastActivity(firstActivity) {}
};

/**
 * @brief Enhanced memory tracking system with advanced leak detection and performance profiling
 */
class MemoryTracker {
public:
    /**
     * @brief Get singleton instance
     */
    static MemoryTracker& instance() {
        static MemoryTracker tracker;
        return tracker;
    }

    /**
     * @brief Initialize memory tracker
     */
    void initialize(const MemoryTrackerConfig& config = MemoryTrackerConfig()) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        config_ = config;

        if (!config_.enabled) {
            return;
        }

        // Initialize log file
        if (!config_.logFilePath.empty()) {
            try {
                logFile_.open(config_.logFilePath,
                              std::ios::out | std::ios::trunc);
                if (!logFile_.is_open()) {
                    reportError("Failed to open log file: " +
                                config_.logFilePath);
                }
            } catch (const std::exception& e) {
                reportError(std::string("Exception opening log file: ") +
                            e.what());
            }
        }

        // Record initialization information
        logMessage("Memory Tracker Initialized");
        logMessage("Configuration:");
        logMessage("  Track Stack Trace: " +
                   std::string(config_.trackStackTrace ? "Yes" : "No"));
        logMessage("  Auto Report Leaks: " +
                   std::string(config_.autoReportLeaks ? "Yes" : "No"));
        logMessage("  Min Allocation Size: " +
                   std::to_string(config_.minAllocationSize) + " bytes");

        if (config_.autoReportLeaks) {
            // Create at_exit handler
            std::atexit([]() { MemoryTracker::instance().reportLeaks(); });
        }
    }

    /**
     * @brief Register memory allocation
     * @param ptr Allocated memory pointer
     * @param size Allocation size
     * @param file Source file name
     * @param line Source file line number
     * @param function Function name
     */
    void registerAllocation(void* ptr, size_t size, const char* file = nullptr,
                            int line = 0, const char* function = nullptr) {
        if (!config_.enabled || ptr == nullptr ||
            size < config_.minAllocationSize) {
            return;
        }

        try {
            std::unique_lock<std::shared_mutex> lock(mutex_);

            std::string sourceFile = file ? file : "";
            std::string sourceFunction = function ? function : "";

            // Create allocation info
            auto info = std::make_shared<AllocationInfo>(ptr, size, sourceFile,
                                                         line, sourceFunction);

            // Capture call stack
            if (config_.trackStackTrace) {
                // Use the existing StackTrace class to capture stack trace
                atom::error::StackTrace trace;
                std::string traceStr = trace.toString();

                // Split the trace string into individual lines
                std::istringstream stream(traceStr);
                std::string line;
                while (std::getline(stream, line)) {
                    if (!line.empty()) {
                        info->stackTrace.push_back(line);
                    }
                }

                // Limit to configured max frames if needed
                if (info->stackTrace.size() > config_.maxStackFrames) {
                    info->stackTrace.resize(config_.maxStackFrames);
                }
            }

            // Store allocation info
            allocations_[ptr] = info;

            // Update statistics
            stats_.currentAllocations++;
            stats_.totalAllocations++;
            stats_.currentMemoryUsage += size;
            stats_.totalMemoryAllocated += size;

            if (stats_.currentMemoryUsage > stats_.peakMemoryUsage) {
                stats_.peakMemoryUsage.store(stats_.currentMemoryUsage);
            }

            if (size > stats_.largestSingleAllocation) {
                stats_.largestSingleAllocation = size;
            }

            // Optional: record allocation info
            if (logFile_.is_open() || config_.logToConsole) {
                std::stringstream ss;
                ss << "ALLOC [" << ptr << "] Size: " << size << " bytes";

                if (!sourceFile.empty()) {
                    ss << " at " << sourceFile << ":" << line;
                }

                if (!sourceFunction.empty()) {
                    ss << " in " << sourceFunction;
                }

                logMessage(ss.str());
            }
        } catch (const std::exception& e) {
            reportError(std::string("Exception in registerAllocation: ") +
                        e.what());
        }
    }

    /**
     * @brief Register memory deallocation
     * @param ptr Deallocated memory pointer
     */
    void registerDeallocation(void* ptr) {
        if (!config_.enabled || ptr == nullptr) {
            return;
        }

        try {
            std::unique_lock<std::shared_mutex> lock(mutex_);

            auto it = allocations_.find(ptr);
            if (it != allocations_.end()) {
                // Record deallocation info
                size_t size = it->second->size;

                if (logFile_.is_open() || config_.logToConsole) {
                    std::stringstream ss;
                    ss << "FREE  [" << ptr << "] Size: " << size << " bytes";
                    logMessage(ss.str());
                }

                // Update statistics
                stats_.currentAllocations--;
                stats_.totalDeallocations++;
                stats_.currentMemoryUsage -= size;

                // Remove allocation record
                allocations_.erase(it);
            } else {
                // Invalid free or double free
                logMessage("WARNING: Attempting to free untracked memory at " +
                           pointerToString(ptr));
            }
        } catch (const std::exception& e) {
            reportError(std::string("Exception in registerDeallocation: ") +
                        e.what());
        }
    }

    /**
     * @brief Report memory leaks
     */
    void reportLeaks() {
        if (!config_.enabled) {
            return;
        }

        try {
            std::unique_lock<std::shared_mutex> lock(mutex_);

            std::stringstream report;
            report << "\n===== MEMORY LEAK REPORT =====\n";

            if (allocations_.empty()) {
                report << "No memory leaks detected.\n";
            } else {
                report << "Detected " << allocations_.size()
                       << " memory leaks totaling " << stats_.currentMemoryUsage
                       << " bytes.\n\n";

                size_t index = 1;
                for (const auto& [ptr, info] : allocations_) {
                    report << "Leak #" << index++ << ": " << info->size
                           << " bytes at " << pointerToString(ptr) << "\n";

                    if (!info->sourceFile.empty()) {
                        report << "  Allocated at: " << info->sourceFile << ":"
                               << info->sourceLine;

                        if (!info->sourceFunction.empty()) {
                            report << " in " << info->sourceFunction;
                        }

                        report << "\n";
                    }

                    // Print call stack
                    if (!info->stackTrace.empty()) {
                        report << "  Stack trace:\n";
                        for (size_t i = 0; i < info->stackTrace.size(); ++i) {
                            report << "    #" << i << ": "
                                   << info->stackTrace[i] << "\n";
                        }
                    }

                    report << "\n";
                }
            }

            // Add statistics
            report << "===== MEMORY STATISTICS =====\n";
            report << "Total allocations:       " << stats_.totalAllocations
                   << "\n";
            report << "Total deallocations:     " << stats_.totalDeallocations
                   << "\n";
            report << "Peak memory usage:       " << stats_.peakMemoryUsage
                   << " bytes\n";
            report << "Largest single alloc:    "
                   << stats_.largestSingleAllocation << " bytes\n";
            report << "Total memory allocated:  " << stats_.totalMemoryAllocated
                   << " bytes\n";
            report << "==============================\n";

            // Output report
            logMessage(report.str());
        } catch (const std::exception& e) {
            reportError(std::string("Exception in reportLeaks: ") + e.what());
        }
    }

    /**
     * @brief Clear all tracking records
     */
    void reset() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        allocations_.clear();
        stats_.currentAllocations.store(0);
        stats_.currentMemoryUsage.store(0);
        stats_.totalAllocations.store(0);
        stats_.totalDeallocations.store(0);
        stats_.totalMemoryAllocated.store(0);
        stats_.peakMemoryUsage.store(0);
        stats_.largestSingleAllocation.store(0);
        logMessage("Memory tracker reset");
    }

    /**
     * @brief Get comprehensive performance metrics
     *
     * @return Tuple of (avg_alloc_time, avg_dealloc_time, efficiency, leak_ratio)
     */
    [[nodiscard]] auto getPerformanceMetrics() const -> std::tuple<double, double, double, double> {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return std::make_tuple(
            stats_.getAverageAllocationTime(),
            stats_.getAverageDeallocationTime(),
            stats_.getMemoryEfficiency(),
            stats_.getLeakRatio()
        );
    }

    /**
     * @brief Get detected leak patterns
     *
     * @return Vector of leak patterns sorted by confidence
     */
    [[nodiscard]] std::vector<LeakPattern> getLeakPatterns() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<LeakPattern> patterns;
        patterns.reserve(leakPatterns_.size());

        for (const auto& [signature, pattern] : leakPatterns_) {
            if (pattern.occurrences >= config_.leakPatternThreshold) {
                patterns.push_back(pattern);
            }
        }

        // Sort by confidence score
        std::sort(patterns.begin(), patterns.end(),
                 [](const LeakPattern& a, const LeakPattern& b) {
                     return a.confidence > b.confidence;
                 });

        return patterns;
    }

    /**
     * @brief Get memory hotspots
     *
     * @return Vector of hotspots sorted by score
     */
    [[nodiscard]] std::vector<MemoryHotspot> getMemoryHotspots() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<MemoryHotspot> hotspots;
        hotspots.reserve(std::min(memoryHotspots_.size(), config_.hotspotsTopN));

        for (const auto& [location, hotspot] : memoryHotspots_) {
            hotspots.push_back(hotspot);
        }

        // Sort by hotspot score
        std::sort(hotspots.begin(), hotspots.end(),
                 [](const MemoryHotspot& a, const MemoryHotspot& b) {
                     return a.hotspotScore > b.hotspotScore;
                 });

        // Return top N hotspots
        if (hotspots.size() > config_.hotspotsTopN) {
            hotspots.resize(config_.hotspotsTopN);
        }

        return hotspots;
    }

    /**
     * @brief Get thread-specific memory statistics
     *
     * @return Map of thread statistics
     */
    [[nodiscard]] std::unordered_map<std::thread::id, ThreadMemoryStats> getThreadStats() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return threadStats_;
    }

    /**
     * @brief Force leak pattern analysis
     */
    void analyzeLeaks() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        analyzeLeakPatterns();
    }

    /**
     * @brief Generate comprehensive performance report
     *
     * @return Detailed performance report string
     */
    [[nodiscard]] std::string generateDetailedReport() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::stringstream report;

        report << "\n===== COMPREHENSIVE MEMORY ANALYSIS REPORT =====\n";

        // Basic statistics
        report << "\n--- Basic Statistics ---\n";
        report << "Current Allocations: " << stats_.currentAllocations.load() << "\n";
        report << "Current Memory Usage: " << stats_.currentMemoryUsage.load() << " bytes\n";
        report << "Peak Memory Usage: " << stats_.peakMemoryUsage.load() << " bytes\n";
        report << "Total Allocations: " << stats_.totalAllocations.load() << "\n";
        report << "Total Deallocations: " << stats_.totalDeallocations.load() << "\n";

        // Performance metrics
        report << "\n--- Performance Metrics ---\n";
        report << "Average Allocation Time: " << stats_.getAverageAllocationTime() << " ns\n";
        report << "Average Deallocation Time: " << stats_.getAverageDeallocationTime() << " ns\n";
        report << "Memory Efficiency: " << (stats_.getMemoryEfficiency() * 100) << "%\n";
        report << "Leak Ratio: " << (stats_.getLeakRatio() * 100) << "%\n";

        // Leak patterns
        report << "\n--- Leak Patterns ---\n";
        for (const auto& [signature, pattern] : leakPatterns_) {
            if (pattern.occurrences >= config_.leakPatternThreshold) {
                report << "Pattern: " << signature << "\n";
                report << "  Occurrences: " << pattern.occurrences << "\n";
                report << "  Total Size: " << pattern.totalSize << " bytes\n";
                report << "  Confidence: " << (pattern.confidence * 100) << "%\n";
            }
        }

        // Memory hotspots
        report << "\n--- Memory Hotspots ---\n";
        auto hotspots = getMemoryHotspots();
        for (size_t i = 0; i < std::min(hotspots.size(), static_cast<size_t>(5)); ++i) {
            const auto& hotspot = hotspots[i];
            report << "Hotspot " << (i + 1) << ": " << hotspot.location << "\n";
            report << "  Allocations: " << hotspot.allocationCount << "\n";
            report << "  Total Size: " << hotspot.totalSize << " bytes\n";
            report << "  Average Size: " << hotspot.averageSize << " bytes\n";
            report << "  Score: " << hotspot.hotspotScore << "\n";
        }

        return report.str();
    }

    /**
     * @brief Enable or disable real-time monitoring
     *
     * @param enable Whether to enable monitoring
     */
    void setRealTimeMonitoring(bool enable) {
        if (enable && !stopMonitoring_.load()) {
            return;  // Already running
        }

        if (enable) {
            startRealTimeMonitoring();
        } else {
            stopRealTimeMonitoring();
        }
    }

    /**
     * @brief Destructor
     */
    ~MemoryTracker() {
        try {
            if (config_.enabled && config_.autoReportLeaks) {
                reportLeaks();
            }

            if (logFile_.is_open()) {
                logFile_.close();
            }
        } catch (...) {
            // Destructors should not throw exceptions
        }
    }

private:
    MemoryTracker() : config_() {}

    // Prevent copy and move
    MemoryTracker(const MemoryTracker&) = delete;
    MemoryTracker& operator=(const MemoryTracker&) = delete;
    MemoryTracker(MemoryTracker&&) = delete;
    MemoryTracker& operator=(MemoryTracker&&) = delete;

    // Convert pointer to string
    std::string pointerToString(void* ptr) {
        std::stringstream ss;
        ss << "0x" << std::hex << std::setw(2 * sizeof(void*))
           << std::setfill('0') << reinterpret_cast<uintptr_t>(ptr);
        return ss.str();
    }

    // Log message
    void logMessage(const std::string& message) {
        try {
            // Get current time
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream timestamp;
            timestamp << std::put_time(std::localtime(&now_time_t),
                                       "%Y-%m-%d %H:%M:%S");

            std::string formattedMessage =
                "[" + timestamp.str() + "] " + message;

            if (config_.logToConsole) {
                std::cout << formattedMessage << std::endl;
            }

            if (logFile_.is_open()) {
                logFile_ << formattedMessage << std::endl;
                logFile_.flush();
            }
        } catch (const std::exception& e) {
            reportError(std::string("Exception in logMessage: ") + e.what());
        }
    }

    // Report error
    void reportError(const std::string& errorMessage) {
        try {
            if (config_.errorCallback) {
                config_.errorCallback(errorMessage);
            } else {
                std::cerr << "Memory Tracker Error: " << errorMessage
                          << std::endl;
            }
        } catch (...) {
            // Ensure error handling doesn't throw exceptions
            std::cerr << "Critical error in Memory Tracker error handling"
                      << std::endl;
        }
    }

    mutable std::shared_mutex mutex_;
    MemoryTrackerConfig config_;
    std::unordered_map<void*, std::shared_ptr<AllocationInfo>> allocations_;
    MemoryStatistics stats_;
    std::ofstream logFile_;

    // Advanced tracking data structures
    std::unordered_map<std::string, LeakPattern> leakPatterns_;
    std::unordered_map<std::string, MemoryHotspot> memoryHotspots_;
    std::unordered_map<std::thread::id, ThreadMemoryStats> threadStats_;
    std::unordered_set<std::string> suspiciousPatterns_;

    // Performance optimization
    std::atomic<size_t> nextAllocationId_{1};
    std::chrono::steady_clock::time_point lastCleanup_;
    std::chrono::steady_clock::time_point lastReport_;

    // Real-time monitoring
    std::thread monitoringThread_;
    std::atomic<bool> stopMonitoring_{false};

    // Enhanced helper methods
    void analyzeLeakPatterns();
    void updateHotspots(const AllocationInfo& info, std::chrono::nanoseconds duration);
    void updateThreadStats(std::thread::id threadId, size_t size, bool isAllocation);
    void detectMemoryPressure();
    void performPeriodicCleanup();
    void generatePerformanceReport();
    bool shouldTrackAllocation(const std::string& file, size_t size) const;
    void prefetchAllocationData(void* ptr) const;
    std::string calculatePatternSignature(const AllocationInfo& info) const;
    void startRealTimeMonitoring();
    void stopRealTimeMonitoring();
};

}  // namespace atom::memory

/**
 * @brief Convenience macros for recording allocation locations
 */
#ifdef ATOM_MEMORY_TRACKING_ENABLED
#define ATOM_TRACK_ALLOC(ptr, size)                             \
    atom::memory::MemoryTracker::instance().registerAllocation( \
        ptr, size, __FILE__, __LINE__, __func__)

#define ATOM_TRACK_FREE(ptr) \
    atom::memory::MemoryTracker::instance().registerDeallocation(ptr)
#else
#define ATOM_TRACK_ALLOC(ptr, size) ((void)0)
#define ATOM_TRACK_FREE(ptr) ((void)0)
#endif

/**
 * @brief Overload global new and delete operators to automatically track memory
 */
#ifdef ATOM_MEMORY_TRACKING_ENABLED

// Basic new/delete
void* operator new(size_t size) {
    void* ptr = std::malloc(size);
    if (!ptr)
        throw std::bad_alloc();
    ATOM_TRACK_ALLOC(ptr, size);
    return ptr;
}

void operator delete(void* ptr) noexcept {
    ATOM_TRACK_FREE(ptr);
    std::free(ptr);
}

// Array versions
void* operator new[](size_t size) {
    void* ptr = std::malloc(size);
    if (!ptr)
        throw std::bad_alloc();
    ATOM_TRACK_ALLOC(ptr, size);
    return ptr;
}

void operator delete[](void* ptr) noexcept {
    ATOM_TRACK_FREE(ptr);
    std::free(ptr);
}

// nothrow versions
void* operator new(size_t size, const std::nothrow_t&) noexcept {
    void* ptr = std::malloc(size);
    if (ptr) {
        ATOM_TRACK_ALLOC(ptr, size);
    }
    return ptr;
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    ATOM_TRACK_FREE(ptr);
    std::free(ptr);
}

// Array nothrow versions
void* operator new[](size_t size, const std::nothrow_t&) noexcept {
    void* ptr = std::malloc(size);
    if (ptr) {
        ATOM_TRACK_ALLOC(ptr, size);
    }
    return ptr;
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    ATOM_TRACK_FREE(ptr);
    std::free(ptr);
}

#endif  // ATOM_MEMORY_TRACKING_ENABLED

#endif  // ATOM_MEMORY_TRACKER_HPP
