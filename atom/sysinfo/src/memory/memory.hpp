/**
 * @file memory.hpp
 * @brief System memory information functionality
 *
 * This file contains definitions for retrieving and monitoring system memory
 * information across different platforms. It provides utilities for querying
 * physical memory, virtual memory, and swap memory statistics.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_MEMORY_HPP
#define ATOM_SYSTEM_MODULE_MEMORY_HPP

#include <string>
#include <utility>
#include <vector>
#include <functional>
#include <chrono>
#include <map>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @struct MemoryInfo
 * @brief Comprehensive information about system memory
 *
 * Contains detailed information about physical memory slots,
 * virtual memory, swap memory, and process-specific memory metrics.
 */
struct MemoryInfo {
    /**
     * @struct MemorySlot
     * @brief Information about a physical memory module/slot
     *
     * Contains details about a specific physical memory module
     * including capacity, speed, and memory type.
     */
    struct MemorySlot {
        std::string capacity;   /**< Memory module capacity (e.g., "8GB") */
        std::string clockSpeed; /**< Memory clock speed (e.g., "3200MHz") */
        std::string type;       /**< Memory type (e.g., "DDR4", "DDR5") */

        /**
         * @brief Default constructor
         */
        MemorySlot() = default;

        /**
         * @brief Parameterized constructor
         * @param capacity Memory module capacity
         * @param clockSpeed Memory clock speed
         * @param type Memory module type
         */
        MemorySlot(std::string capacity, std::string clockSpeed,
                   std::string type)
            : capacity(std::move(capacity)),
              clockSpeed(std::move(clockSpeed)),
              type(std::move(type)) {}
    } ATOM_ALIGNAS(128);

    std::vector<MemorySlot> slots; /**< Collection of physical memory slots */
    unsigned long long
        virtualMemoryMax; /**< Maximum virtual memory size in bytes */
    unsigned long long virtualMemoryUsed; /**< Used virtual memory in bytes */
    unsigned long long swapMemoryTotal;   /**< Total swap memory in bytes */
    unsigned long long swapMemoryUsed;    /**< Used swap memory in bytes */

    double memoryLoadPercentage; /**< Current memory usage percentage */
    unsigned long long totalPhysicalMemory; /**< Total physical RAM in bytes */
    unsigned long long
        availablePhysicalMemory;       /**< Available physical RAM in bytes */
    unsigned long long pageFaultCount; /**< Number of page faults */
    unsigned long long
        peakWorkingSetSize;            /**< Peak working set size in bytes */
    unsigned long long workingSetSize; /**< Current working set size in bytes */
    unsigned long long
        quotaPeakPagedPoolUsage; /**< Peak paged pool usage in bytes */
    unsigned long long
        quotaPagedPoolUsage; /**< Current paged pool usage in bytes */
} ATOM_ALIGNAS(64);

/**
 * @struct MemoryPerformance
 * @brief Enhanced memory performance monitoring structure
 *
 * Contains comprehensive metrics for memory read/write speeds, bandwidth usage,
 * latency, cache performance, and memory controller statistics.
 */
struct MemoryPerformance {
    // Basic performance metrics
    double readSpeed;        /**< Memory read speed in MB/s */
    double writeSpeed;       /**< Memory write speed in MB/s */
    double bandwidthUsage;   /**< Memory bandwidth usage percentage */
    double latency;          /**< Memory latency in nanoseconds */
    std::vector<double> latencyHistory; /**< Historical latency data */

    // Enhanced performance metrics
    double sequentialReadSpeed;  /**< Sequential read speed in MB/s */
    double sequentialWriteSpeed; /**< Sequential write speed in MB/s */
    double randomReadSpeed;      /**< Random read speed in MB/s */
    double randomWriteSpeed;     /**< Random write speed in MB/s */

    // Cache performance metrics
    double l1CacheHitRate;       /**< L1 cache hit rate percentage */
    double l2CacheHitRate;       /**< L2 cache hit rate percentage */
    double l3CacheHitRate;       /**< L3 cache hit rate percentage */
    double tlbHitRate;           /**< Translation Lookaside Buffer hit rate */

    // Memory controller statistics
    double memoryControllerUtilization; /**< Memory controller utilization percentage */
    unsigned long long pageFaultsPerSecond; /**< Page faults per second */
    unsigned long long majorFaultsPerSecond; /**< Major page faults per second */
    unsigned long long contextSwitchesPerSecond; /**< Context switches per second */

    // Bandwidth breakdown
    double systemBandwidthUsage; /**< System-wide bandwidth usage */
    double processBandwidthUsage; /**< Current process bandwidth usage */

    // Timing metrics
    double averageAccessTime;    /**< Average memory access time in ns */
    double peakAccessTime;       /**< Peak memory access time in ns */
    double minimumAccessTime;    /**< Minimum memory access time in ns */

    std::chrono::steady_clock::time_point timestamp; /**< When measurement was taken */
} ATOM_ALIGNAS(64);

/**
 * @enum MemoryPressureLevel
 * @brief Memory pressure severity levels
 */
enum class MemoryPressureLevel {
    NONE = 0,     /**< No memory pressure */
    LOW = 1,      /**< Low memory pressure */
    MEDIUM = 2,   /**< Medium memory pressure */
    HIGH = 3,     /**< High memory pressure */
    CRITICAL = 4  /**< Critical memory pressure */
};

/**
 * @struct MemoryPressureInfo
 * @brief Detailed memory pressure information
 *
 * Contains information about current memory pressure including
 * severity level, contributing factors, and recommendations.
 */
struct MemoryPressureInfo {
    MemoryPressureLevel level;           /**< Current pressure level */
    double pressureScore;                /**< Pressure score (0-100) */
    std::vector<std::string> factors;    /**< Contributing factors */
    std::vector<std::string> recommendations; /**< Recommended actions */

    // Detailed metrics
    double memoryUsagePercent;           /**< Current memory usage percentage */
    double swapUsagePercent;             /**< Current swap usage percentage */
    double pageFaultRate;                /**< Page faults per second */
    double majorFaultRate;               /**< Major page faults per second */
    double allocationFailureRate;        /**< Memory allocation failure rate */

    std::chrono::steady_clock::time_point timestamp; /**< When measurement was taken */
} ATOM_ALIGNAS(64);

/**
 * @struct MemoryAllocation
 * @brief Information about a memory allocation
 */
struct MemoryAllocation {
    void* address;                    /**< Memory address */
    size_t size;                      /**< Allocation size in bytes */
    std::string allocationType;       /**< Type of allocation (malloc, new, mmap, etc.) */
    std::string sourceLocation;      /**< Source file and line where allocated */
    std::chrono::steady_clock::time_point timestamp; /**< When allocated */
    size_t threadId;                  /**< Thread that made the allocation */
    std::vector<void*> callStack;     /**< Call stack at allocation time */
} ATOM_ALIGNAS(32);

/**
 * @struct HeapInfo
 * @brief Detailed heap analysis information
 */
struct HeapInfo {
    size_t totalHeapSize;             /**< Total heap size in bytes */
    size_t usedHeapSize;              /**< Used heap size in bytes */
    size_t freeHeapSize;              /**< Free heap size in bytes */
    size_t largestFreeBlock;          /**< Largest contiguous free block */
    size_t numberOfAllocations;       /**< Number of active allocations */
    size_t numberOfFreeBlocks;        /**< Number of free blocks */
    double fragmentationRatio;        /**< Heap fragmentation ratio */

    // Allocation size distribution
    std::map<size_t, size_t> allocationSizeHistogram; /**< Size -> count mapping */

    // Memory pools information
    std::vector<std::pair<std::string, size_t>> memoryPools; /**< Pool name -> size */

    std::chrono::steady_clock::time_point timestamp; /**< When measured */
} ATOM_ALIGNAS(64);

/**
 * @struct AllocationTracker
 * @brief Memory allocation tracking information
 */
struct AllocationTracker {
    bool isActive;                    /**< Whether tracking is active */
    size_t totalAllocations;          /**< Total number of allocations */
    size_t totalDeallocations;        /**< Total number of deallocations */
    size_t currentAllocations;        /**< Current active allocations */
    size_t peakAllocations;           /**< Peak number of allocations */
    size_t totalBytesAllocated;       /**< Total bytes allocated */
    size_t totalBytesFreed;           /**< Total bytes freed */
    size_t currentBytesAllocated;     /**< Current bytes allocated */
    size_t peakBytesAllocated;        /**< Peak bytes allocated */

    // Leak detection
    std::vector<MemoryAllocation> suspectedLeaks; /**< Potential memory leaks */

    // Performance metrics
    double averageAllocationTime;     /**< Average allocation time in microseconds */
    double averageDeallocationTime;   /**< Average deallocation time in microseconds */

    std::chrono::steady_clock::time_point startTime; /**< When tracking started */
    std::chrono::steady_clock::time_point lastUpdate; /**< Last update time */
} ATOM_ALIGNAS(64);

/**
 * @enum CompressionAlgorithm
 * @brief Memory compression algorithms
 */
enum class CompressionAlgorithm {
    NONE = 0,        /**< No compression */
    LZ4 = 1,         /**< LZ4 compression */
    ZSTD = 2,        /**< Zstandard compression */
    LZO = 3,         /**< LZO compression */
    SNAPPY = 4,      /**< Snappy compression */
    DEFLATE = 5,     /**< Deflate compression */
    UNKNOWN = 99     /**< Unknown compression algorithm */
};

/**
 * @struct MemoryCompressionInfo
 * @brief Memory compression status and statistics
 */
struct MemoryCompressionInfo {
    bool isEnabled;                   /**< Whether memory compression is enabled */
    bool isSupported;                 /**< Whether system supports memory compression */
    CompressionAlgorithm algorithm;   /**< Compression algorithm in use */

    // Compression statistics
    size_t compressedPages;           /**< Number of compressed memory pages */
    size_t uncompressedPages;         /**< Number of uncompressed memory pages */
    size_t totalPages;                /**< Total number of memory pages */

    // Size information
    size_t originalSize;              /**< Original size before compression (bytes) */
    size_t compressedSize;            /**< Size after compression (bytes) */
    size_t savedBytes;                /**< Bytes saved through compression */

    // Compression ratios and effectiveness
    double compressionRatio;          /**< Compression ratio (compressed/original) */
    double spaceSavings;              /**< Space savings percentage */
    double compressionEfficiency;     /**< Overall compression efficiency */

    // Performance metrics
    double compressionLatency;        /**< Average compression latency (microseconds) */
    double decompressionLatency;      /**< Average decompression latency (microseconds) */
    double compressionThroughput;     /**< Compression throughput (MB/s) */
    double decompressionThroughput;   /**< Decompression throughput (MB/s) */

    // System impact
    double cpuOverhead;               /**< CPU overhead percentage */
    double memoryOverhead;            /**< Memory overhead for compression structures */

    // Historical data
    std::vector<double> compressionRatioHistory; /**< Historical compression ratios */
    std::vector<double> latencyHistory;          /**< Historical latency data */

    std::chrono::steady_clock::time_point timestamp; /**< When measurement was taken */
} ATOM_ALIGNAS(64);

/**
 * @struct NumaNode
 * @brief Information about a NUMA node
 */
struct NumaNode {
    int nodeId;                       /**< NUMA node ID */
    size_t totalMemory;               /**< Total memory in bytes */
    size_t freeMemory;                /**< Free memory in bytes */
    size_t usedMemory;                /**< Used memory in bytes */
    double memoryUsagePercent;        /**< Memory usage percentage */

    // CPU information
    std::vector<int> cpuList;         /**< List of CPU cores in this node */
    int cpuCount;                     /**< Number of CPU cores */

    // Memory access latency
    double localAccessLatency;        /**< Local memory access latency (ns) */
    std::map<int, double> remoteAccessLatency; /**< Remote node access latencies */

    // Memory bandwidth
    double localBandwidth;            /**< Local memory bandwidth (MB/s) */
    std::map<int, double> remoteBandwidth; /**< Remote node bandwidths */

    // Memory allocation statistics
    size_t localAllocations;          /**< Number of local allocations */
    size_t remoteAllocations;         /**< Number of remote allocations */
    double numaHitRatio;              /**< NUMA hit ratio percentage */

    // Distance matrix
    std::map<int, int> nodeDistances; /**< Distance to other NUMA nodes */

    std::chrono::steady_clock::time_point timestamp; /**< When measured */
} ATOM_ALIGNAS(64);

/**
 * @struct NumaTopology
 * @brief Complete NUMA topology information
 */
struct NumaTopology {
    bool isNumaSystem;                /**< Whether system has NUMA architecture */
    int nodeCount;                    /**< Number of NUMA nodes */
    std::vector<NumaNode> nodes;      /**< Information about each NUMA node */

    // System-wide NUMA statistics
    double overallNumaHitRatio;       /**< Overall NUMA hit ratio */
    size_t totalNumaAllocations;      /**< Total NUMA allocations */
    size_t totalRemoteAccesses;       /**< Total remote memory accesses */

    // Memory migration statistics
    size_t pageMigrations;            /**< Number of page migrations */
    size_t automaticMigrations;       /**< Automatic NUMA balancing migrations */
    size_t manualMigrations;          /**< Manual page migrations */

    // Performance impact
    double numaPerformanceImpact;     /**< Performance impact percentage */
    std::vector<std::string> recommendations; /**< NUMA optimization recommendations */

    std::chrono::steady_clock::time_point timestamp; /**< When topology was analyzed */
} ATOM_ALIGNAS(64);

/**
 * @enum LeakSeverity
 * @brief Memory leak severity levels
 */
enum class LeakSeverity {
    LOW = 1,        /**< Low severity leak */
    MEDIUM = 2,     /**< Medium severity leak */
    HIGH = 3,       /**< High severity leak */
    CRITICAL = 4    /**< Critical severity leak */
};

/**
 * @enum LeakType
 * @brief Types of memory leaks
 */
enum class LeakType {
    GRADUAL = 1,    /**< Gradual memory leak */
    SUDDEN = 2,     /**< Sudden memory leak */
    PERIODIC = 3,   /**< Periodic memory leak */
    PERSISTENT = 4, /**< Persistent memory leak */
    UNKNOWN = 99    /**< Unknown leak pattern */
};

/**
 * @struct MemoryLeak
 * @brief Detailed information about a detected memory leak
 */
struct MemoryLeak {
    std::string leakId;               /**< Unique leak identifier */
    LeakSeverity severity;            /**< Leak severity level */
    LeakType type;                    /**< Type of leak pattern */

    // Leak characteristics
    size_t leakRate;                  /**< Leak rate in bytes per second */
    size_t totalLeakedBytes;          /**< Total bytes leaked */
    size_t estimatedLossPerHour;      /**< Estimated loss per hour */

    // Source information
    std::string sourceLocation;       /**< Source code location */
    std::string processName;          /**< Process name */
    int processId;                    /**< Process ID */
    std::vector<void*> callStack;     /**< Call stack where leak originated */

    // Temporal information
    std::chrono::steady_clock::time_point firstDetected; /**< When first detected */
    std::chrono::steady_clock::time_point lastSeen;      /**< When last observed */
    std::chrono::minutes duration;    /**< How long leak has been active */

    // Pattern analysis
    std::vector<size_t> allocationSizes; /**< Sizes of leaked allocations */
    std::vector<std::chrono::steady_clock::time_point> allocationTimes; /**< Times of allocations */
    double confidence;                /**< Confidence level (0-100%) */

    // Impact assessment
    double memoryImpact;              /**< Memory impact percentage */
    double performanceImpact;         /**< Performance impact estimate */
    std::vector<std::string> recommendations; /**< Fix recommendations */

    std::chrono::steady_clock::time_point timestamp; /**< When leak was analyzed */
} ATOM_ALIGNAS(64);

/**
 * @struct LeakDetectionConfig
 * @brief Configuration for memory leak detection
 */
struct LeakDetectionConfig {
    bool enableAdvancedDetection;     /**< Enable advanced detection algorithms */
    bool enablePatternRecognition;    /**< Enable pattern recognition */
    bool enableHistoricalAnalysis;    /**< Enable historical analysis */

    // Detection thresholds
    size_t minimumLeakSize;           /**< Minimum leak size to report (bytes) */
    std::chrono::minutes minimumAge;  /**< Minimum age before considering as leak */
    double confidenceThreshold;       /**< Minimum confidence level */

    // Analysis parameters
    std::chrono::minutes analysisWindow; /**< Time window for analysis */
    size_t maxTrackedAllocations;     /**< Maximum allocations to track */
    bool trackCallStacks;             /**< Whether to track call stacks */

    // Reporting options
    bool reportGradualLeaks;          /**< Report gradual leaks */
    bool reportSuddenLeaks;           /**< Report sudden leaks */
    bool reportPeriodicLeaks;         /**< Report periodic leaks */

    LeakDetectionConfig() {
        // Default configuration
        enableAdvancedDetection = true;
        enablePatternRecognition = true;
        enableHistoricalAnalysis = true;
        minimumLeakSize = 1024;       // 1KB
        minimumAge = std::chrono::minutes(5);
        confidenceThreshold = 70.0;
        analysisWindow = std::chrono::minutes(30);
        maxTrackedAllocations = 10000;
        trackCallStacks = false;
        reportGradualLeaks = true;
        reportSuddenLeaks = true;
        reportPeriodicLeaks = true;
    }
} ATOM_ALIGNAS(32);

/**
 * @brief Get the memory usage percentage
 *
 * Calculates and returns the percentage of physical memory currently in use
 * by the system.
 *
 * @return Float value representing memory usage percentage (0-100)
 */
auto getMemoryUsage() -> float;

/**
 * @brief Get the total physical memory size
 *
 * Retrieves the total amount of physical RAM installed in the system.
 *
 * @return Total physical memory size in bytes
 */
auto getTotalMemorySize() -> unsigned long long;

/**
 * @brief Get the available physical memory size
 *
 * Retrieves the amount of physical RAM currently available for allocation.
 *
 * @return Available physical memory size in bytes
 */
auto getAvailableMemorySize() -> unsigned long long;

/**
 * @brief Get information about physical memory modules
 *
 * Retrieves details about the physical memory modules installed in the system.
 *
 * @return MemorySlot object containing information about the memory modules
 */
auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot;

/**
 * @brief Get the maximum virtual memory size
 *
 * Retrieves the maximum amount of virtual memory available to processes.
 *
 * @return Maximum virtual memory size in bytes
 */
auto getVirtualMemoryMax() -> unsigned long long;

/**
 * @brief Get the currently used virtual memory
 *
 * Retrieves the amount of virtual memory currently in use by the system.
 *
 * @return Used virtual memory size in bytes
 */
auto getVirtualMemoryUsed() -> unsigned long long;

/**
 * @brief Get the total swap/page file size
 *
 * Retrieves the total amount of swap space or page file configured on the
 * system.
 *
 * @return Total swap memory size in bytes
 */
auto getSwapMemoryTotal() -> unsigned long long;

/**
 * @brief Get the used swap/page file size
 *
 * Retrieves the amount of swap space or page file currently in use.
 *
 * @return Used swap memory size in bytes
 */
auto getSwapMemoryUsed() -> unsigned long long;

/**
 * @brief Get the committed memory size
 *
 * Retrieves the amount of memory that has been committed by the system.
 * This is physical memory that has been allocated to processes.
 *
 * @return Committed memory size in bytes
 */
auto getCommittedMemory() -> size_t;

/**
 * @brief Get the uncommitted memory size
 *
 * Retrieves the amount of memory that is available for commitment.
 * This is physical memory that is available but not yet allocated.
 *
 * @return Uncommitted memory size in bytes
 */
auto getUncommittedMemory() -> size_t;

/**
 * @brief Get comprehensive memory statistics
 *
 * Retrieves detailed information about the system's memory including
 * physical memory, virtual memory, swap space, and various performance metrics.
 *
 * @return MemoryInfo structure containing comprehensive memory statistics
 */
auto getDetailedMemoryStats() -> MemoryInfo;

/**
 * @brief Get the peak working set size of the current process
 *
 * Retrieves the maximum amount of physical memory used by the current process
 * since it was started.
 *
 * @return Peak working set size in bytes
 */
auto getPeakWorkingSetSize() -> size_t;

/**
 * @brief Get the current working set size of the process
 *
 * Retrieves the current amount of physical memory used by the process.
 * The working set is the set of memory pages currently visible to the process.
 *
 * @return Current working set size in bytes
 */
auto getCurrentWorkingSetSize() -> size_t;

/**
 * @brief Get the page fault count
 *
 * Retrieves the number of page faults that have occurred in the system.
 * Page faults occur when a process accesses a memory page that is not
 * currently mapped into its address space.
 *
 * @return Number of page faults
 */
auto getPageFaultCount() -> size_t;

/**
 * @brief Get memory load percentage
 *
 * Retrieves the percentage of memory currently in use by the system.
 * This is a normalized value indicating the overall memory pressure.
 *
 * @return Memory load as a percentage (0-100)
 */
auto getMemoryLoadPercentage() -> double;

/**
 * @brief Get memory performance metrics
 *
 * Retrieves detailed performance metrics for memory including read/write
 * speeds, bandwidth usage, and latency.
 *
 * @return MemoryPerformance structure containing performance metrics
 */
auto getMemoryPerformance() -> MemoryPerformance;

/**
 * @brief Get enhanced memory performance metrics
 *
 * Retrieves comprehensive performance metrics including cache hit rates,
 * memory controller statistics, and detailed bandwidth analysis.
 *
 * @return MemoryPerformance structure with enhanced metrics
 */
auto getEnhancedMemoryPerformance() -> MemoryPerformance;

/**
 * @brief Benchmark memory performance with custom parameters
 *
 * Performs detailed memory benchmarking with configurable test parameters
 * to measure various aspects of memory performance.
 *
 * @param testSizeBytes Size of test buffer in bytes
 * @param iterations Number of test iterations
 * @param testPattern Test pattern (sequential, random, mixed)
 * @return MemoryPerformance structure with benchmark results
 */
auto benchmarkMemoryPerformance(size_t testSizeBytes = 64 * 1024 * 1024,
                                int iterations = 10,
                                const std::string& testPattern = "mixed") -> MemoryPerformance;

/**
 * @brief Get memory cache statistics
 *
 * Retrieves detailed cache performance statistics including hit rates
 * for different cache levels.
 *
 * @return MemoryPerformance structure focused on cache metrics
 */
auto getMemoryCacheStats() -> MemoryPerformance;

/**
 * @brief Start memory monitoring
 *
 * Initiates memory monitoring and invokes the provided callback function
 * with updated memory information.
 *
 * @param callback Function to be called with memory information updates
 */
auto startMemoryMonitoring(std::function<void(const MemoryInfo&)> callback) -> void;

/**
 * @brief Stop memory monitoring
 *
 * Stops the ongoing memory monitoring process.
 */
auto stopMemoryMonitoring() -> void;

/**
 * @brief Get memory timeline
 *
 * Retrieves a timeline of memory statistics over a specified duration.
 *
 * @param duration Duration for which memory statistics are collected
 * @return Vector of MemoryInfo objects representing the memory timeline
 */
auto getMemoryTimeline(std::chrono::minutes duration) -> std::vector<MemoryInfo>;

/**
 * @brief Detect memory leaks
 *
 * Analyzes the system for potential memory leaks and returns a list of
 * detected issues.
 *
 * @return Vector of strings describing detected memory leaks
 */
auto detectMemoryLeaks() -> std::vector<std::string>;

/**
 * @brief Get memory fragmentation percentage
 *
 * Calculates the percentage of memory fragmentation in the system.
 *
 * @return Memory fragmentation percentage
 */
auto getMemoryFragmentation() -> double;

/**
 * @brief Optimize memory usage
 *
 * Attempts to optimize memory usage by defragmenting and reallocating resources.
 *
 * @return Boolean indicating success or failure of optimization
 */
auto optimizeMemoryUsage() -> bool;

/**
 * @brief Analyze memory bottlenecks
 *
 * Identifies potential bottlenecks in memory usage and provides suggestions
 * for improvement.
 *
 * @return Vector of strings describing memory bottlenecks
 */
auto analyzeMemoryBottlenecks() -> std::vector<std::string>;

/**
 * @brief Detect current memory pressure level
 *
 * Analyzes system memory state to determine current pressure level
 * and provides detailed information about contributing factors.
 *
 * @return MemoryPressureInfo structure with detailed pressure analysis
 */
auto detectMemoryPressure() -> MemoryPressureInfo;

/**
 * @brief Get memory pressure level only
 *
 * Quick function to get just the current memory pressure level
 * without detailed analysis.
 *
 * @return Current memory pressure level
 */
auto getMemoryPressureLevel() -> MemoryPressureLevel;

/**
 * @brief Monitor memory pressure continuously
 *
 * Starts continuous monitoring of memory pressure and calls the provided
 * callback when pressure level changes or exceeds thresholds.
 *
 * @param callback Function to be called with pressure updates
 * @param threshold Minimum pressure level to trigger callback
 */
auto startMemoryPressureMonitoring(
    std::function<void(const MemoryPressureInfo&)> callback,
    MemoryPressureLevel threshold = MemoryPressureLevel::LOW) -> void;

/**
 * @brief Stop memory pressure monitoring
 *
 * Stops the ongoing memory pressure monitoring process.
 */
auto stopMemoryPressureMonitoring() -> void;

/**
 * @brief Get memory pressure history
 *
 * Retrieves historical memory pressure data over a specified duration.
 *
 * @param duration Duration for which pressure data is collected
 * @return Vector of MemoryPressureInfo objects representing pressure timeline
 */
auto getMemoryPressureHistory(std::chrono::minutes duration) -> std::vector<MemoryPressureInfo>;

/**
 * @brief Start memory allocation tracking
 *
 * Begins tracking memory allocations and deallocations for the current process.
 * This enables detailed analysis of memory usage patterns and leak detection.
 *
 * @param trackCallStacks Whether to capture call stacks for allocations
 * @return True if tracking was successfully started
 */
auto startAllocationTracking(bool trackCallStacks = false) -> bool;

/**
 * @brief Stop memory allocation tracking
 *
 * Stops the ongoing memory allocation tracking and returns final statistics.
 *
 * @return AllocationTracker structure with final tracking results
 */
auto stopAllocationTracking() -> AllocationTracker;

/**
 * @brief Get current allocation tracking status
 *
 * Retrieves the current state of memory allocation tracking including
 * statistics and detected issues.
 *
 * @return AllocationTracker structure with current tracking data
 */
auto getAllocationTracker() -> AllocationTracker;

/**
 * @brief Analyze heap memory
 *
 * Performs detailed analysis of heap memory including fragmentation,
 * allocation patterns, and memory pool usage.
 *
 * @return HeapInfo structure with detailed heap analysis
 */
auto analyzeHeapMemory() -> HeapInfo;

/**
 * @brief Get memory allocation statistics
 *
 * Retrieves statistics about memory allocations including size distribution,
 * allocation patterns, and performance metrics.
 *
 * @return Vector of MemoryAllocation objects representing current allocations
 */
auto getMemoryAllocations() -> std::vector<MemoryAllocation>;

/**
 * @brief Detect memory leaks using allocation tracking
 *
 * Analyzes tracked allocations to identify potential memory leaks
 * based on allocation patterns and lifetime analysis.
 *
 * @return Vector of MemoryAllocation objects representing suspected leaks
 */
auto detectMemoryLeaksAdvanced() -> std::vector<MemoryAllocation>;

/**
 * @brief Profile memory allocation patterns
 *
 * Analyzes memory allocation patterns to identify hot spots,
 * frequent allocation sizes, and optimization opportunities.
 *
 * @param duration Duration to profile allocations
 * @return HeapInfo structure with profiling results
 */
auto profileMemoryAllocations(std::chrono::minutes duration) -> HeapInfo;

/**
 * @brief Detect memory compression status
 *
 * Analyzes the system to determine if memory compression is enabled
 * and retrieves detailed compression statistics.
 *
 * @return MemoryCompressionInfo structure with compression details
 */
auto detectMemoryCompression() -> MemoryCompressionInfo;

/**
 * @brief Check if memory compression is supported
 *
 * Determines whether the current system supports memory compression.
 *
 * @return True if memory compression is supported
 */
auto isMemoryCompressionSupported() -> bool;

/**
 * @brief Get memory compression algorithm
 *
 * Identifies the compression algorithm currently in use.
 *
 * @return CompressionAlgorithm enum value
 */
auto getMemoryCompressionAlgorithm() -> CompressionAlgorithm;

/**
 * @brief Benchmark memory compression performance
 *
 * Performs benchmarking of memory compression to measure performance
 * characteristics including compression ratio and throughput.
 *
 * @param testSizeBytes Size of test data in bytes
 * @param algorithm Compression algorithm to test (optional)
 * @return MemoryCompressionInfo with benchmark results
 */
auto benchmarkMemoryCompression(size_t testSizeBytes = 64 * 1024 * 1024,
                               CompressionAlgorithm algorithm = CompressionAlgorithm::UNKNOWN) -> MemoryCompressionInfo;

/**
 * @brief Monitor memory compression continuously
 *
 * Starts continuous monitoring of memory compression statistics
 * and calls the provided callback with updates.
 *
 * @param callback Function to be called with compression updates
 */
auto startMemoryCompressionMonitoring(std::function<void(const MemoryCompressionInfo&)> callback) -> void;

/**
 * @brief Stop memory compression monitoring
 *
 * Stops the ongoing memory compression monitoring process.
 */
auto stopMemoryCompressionMonitoring() -> void;

/**
 * @brief Get memory compression history
 *
 * Retrieves historical memory compression data over a specified duration.
 *
 * @param duration Duration for which compression data is collected
 * @return Vector of MemoryCompressionInfo objects representing compression timeline
 */
auto getMemoryCompressionHistory(std::chrono::minutes duration) -> std::vector<MemoryCompressionInfo>;

/**
 * @brief Detect NUMA topology
 *
 * Analyzes the system to detect NUMA topology and gather detailed
 * information about each NUMA node.
 *
 * @return NumaTopology structure with complete NUMA information
 */
auto detectNumaTopology() -> NumaTopology;

/**
 * @brief Check if system has NUMA architecture
 *
 * Determines whether the current system has NUMA (Non-Uniform Memory Access) architecture.
 *
 * @return True if system has NUMA architecture
 */
auto isNumaSystem() -> bool;

/**
 * @brief Get NUMA node count
 *
 * Returns the number of NUMA nodes in the system.
 *
 * @return Number of NUMA nodes (1 for non-NUMA systems)
 */
auto getNumaNodeCount() -> int;

/**
 * @brief Get NUMA node information
 *
 * Retrieves detailed information about a specific NUMA node.
 *
 * @param nodeId NUMA node ID
 * @return NumaNode structure with node information
 */
auto getNumaNodeInfo(int nodeId) -> NumaNode;

/**
 * @brief Get current process NUMA policy
 *
 * Retrieves the NUMA memory policy for the current process.
 *
 * @return String describing the current NUMA policy
 */
auto getNumaPolicy() -> std::string;

/**
 * @brief Analyze NUMA performance
 *
 * Analyzes NUMA performance characteristics including hit ratios,
 * memory access patterns, and potential optimizations.
 *
 * @return NumaTopology structure with performance analysis
 */
auto analyzeNumaPerformance() -> NumaTopology;

/**
 * @brief Benchmark NUMA memory access
 *
 * Performs benchmarking of memory access across NUMA nodes to measure
 * latency and bandwidth characteristics.
 *
 * @param testSizeBytes Size of test data per node
 * @return NumaTopology structure with benchmark results
 */
auto benchmarkNumaMemoryAccess(size_t testSizeBytes = 64 * 1024 * 1024) -> NumaTopology;

/**
 * @brief Get NUMA memory statistics
 *
 * Retrieves detailed memory statistics for each NUMA node including
 * allocation patterns and usage distribution.
 *
 * @return Vector of NumaNode objects with current statistics
 */
auto getNumaMemoryStats() -> std::vector<NumaNode>;

/**
 * @brief Optimize NUMA memory allocation
 *
 * Provides recommendations for optimizing memory allocation patterns
 * based on NUMA topology analysis.
 *
 * @return Vector of optimization recommendations
 */
auto optimizeNumaAllocation() -> std::vector<std::string>;

/**
 * @brief Enhanced memory leak detection
 *
 * Performs sophisticated memory leak detection using pattern recognition,
 * historical analysis, and advanced algorithms.
 *
 * @param config Detection configuration parameters
 * @return Vector of detected memory leaks
 */
auto detectMemoryLeaksEnhanced(const LeakDetectionConfig& config = LeakDetectionConfig{}) -> std::vector<MemoryLeak>;

/**
 * @brief Analyze memory leak patterns
 *
 * Analyzes detected memory leaks to identify patterns and classify
 * leak types for better understanding and remediation.
 *
 * @param leaks Vector of detected leaks to analyze
 * @return Vector of analyzed leaks with enhanced information
 */
auto analyzeLeakPatterns(const std::vector<MemoryLeak>& leaks) -> std::vector<MemoryLeak>;

/**
 * @brief Start continuous leak monitoring
 *
 * Begins continuous monitoring for memory leaks with real-time detection
 * and pattern analysis.
 *
 * @param callback Function to be called when leaks are detected
 * @param config Detection configuration
 */
auto startLeakMonitoring(std::function<void(const std::vector<MemoryLeak>&)> callback,
                        const LeakDetectionConfig& config = LeakDetectionConfig{}) -> void;

/**
 * @brief Stop leak monitoring
 *
 * Stops the ongoing memory leak monitoring process.
 */
auto stopLeakMonitoring() -> void;

/**
 * @brief Get leak detection statistics
 *
 * Retrieves statistics about the leak detection process including
 * detection rates, false positives, and performance metrics.
 *
 * @return Map of statistic names to values
 */
auto getLeakDetectionStats() -> std::map<std::string, double>;

/**
 * @brief Predict future memory leaks
 *
 * Uses historical data and trend analysis to predict potential
 * future memory leaks.
 *
 * @param predictionWindow Time window for prediction
 * @return Vector of predicted memory leaks
 */
auto predictMemoryLeaks(std::chrono::hours predictionWindow) -> std::vector<MemoryLeak>;

/**
 * @brief Generate leak remediation report
 *
 * Creates a comprehensive report with leak analysis, impact assessment,
 * and detailed remediation recommendations.
 *
 * @param leaks Vector of detected leaks
 * @return Formatted report string
 */
auto generateLeakReport(const std::vector<MemoryLeak>& leaks) -> std::string;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_MODULE_MEMORY_HPP
