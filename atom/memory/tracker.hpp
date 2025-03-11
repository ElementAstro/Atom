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
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "atom/error/stacktrace.hpp"

namespace atom::memory {

/**
 * @brief Memory tracking system configuration options
 */
struct MemoryTrackerConfig {
    bool enabled = true;           // Whether tracking is enabled
    bool trackStackTrace = true;   // Whether to track call stack
    bool autoReportLeaks = true;   // Automatically report leaks at program exit
    bool logToConsole = true;      // Whether to output to console
    std::string logFilePath;       // Log file path (empty means no file output)
    size_t maxStackFrames = 16;    // Maximum number of stack frames
    size_t minAllocationSize = 0;  // Minimum allocation size to track
    bool trackAllocationCount =
        true;                     // Track allocation and deallocation counts
    bool trackPeakMemory = true;  // Track peak memory usage
    std::function<void(const std::string&)> errorCallback =
        nullptr;  // Error callback
};

/**
 * @brief Memory allocation information structure
 */
struct AllocationInfo {
    void* address;                                    // Memory address
    size_t size;                                      // Allocation size
    std::chrono::steady_clock::time_point timestamp;  // Allocation timestamp
    std::string sourceFile;                           // Source file
    int sourceLine;                                   // Source file line number
    std::string sourceFunction;                       // Source function
    std::thread::id threadId;                         // Thread ID
    std::vector<std::string> stackTrace;              // Call stack

    AllocationInfo(void* addr, size_t sz, const std::string& file = "",
                   int line = 0, const std::string& func = "")
        : address(addr),
          size(sz),
          timestamp(std::chrono::steady_clock::now()),
          sourceFile(file),
          sourceLine(line),
          sourceFunction(func),
          threadId(std::this_thread::get_id()) {}
};

/**
 * @brief Memory statistics information
 */
struct MemoryStatistics {
    std::atomic<size_t> currentAllocations{0};  // Current number of allocations
    std::atomic<size_t> currentMemoryUsage{0};  // Current memory usage
    std::atomic<size_t> totalAllocations{0};    // Total allocation count
    std::atomic<size_t> totalDeallocations{0};  // Total deallocation count
    std::atomic<size_t> totalMemoryAllocated{0};  // Total memory allocated
    std::atomic<size_t> peakMemoryUsage{0};       // Peak memory usage
    std::atomic<size_t> largestSingleAllocation{
        0};  // Largest single allocation

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
};

/**
 * @brief Advanced memory tracking system
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
        std::lock_guard<std::mutex> lock(mutex_);
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
            std::lock_guard<std::mutex> lock(mutex_);

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
            std::lock_guard<std::mutex> lock(mutex_);

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
            std::lock_guard<std::mutex> lock(mutex_);

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
        std::lock_guard<std::mutex> lock(mutex_);
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

    std::mutex mutex_;
    MemoryTrackerConfig config_;
    std::unordered_map<void*, std::shared_ptr<AllocationInfo>> allocations_;
    MemoryStatistics stats_;
    std::ofstream logFile_;
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