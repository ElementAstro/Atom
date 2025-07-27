#pragma once

#include <windows.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>
#include <memory>
#include "shortcut.h"
#include "status.h"

namespace shortcut_detector {

/**
 * @brief Configuration for caching behavior
 */
struct CacheConfig {
    std::chrono::milliseconds shortcutCacheTTL{5000};     // 5 seconds
    std::chrono::milliseconds hookCacheTTL{1000};         // 1 second
    std::chrono::milliseconds processCacheTTL{2000};      // 2 seconds
    size_t maxCacheSize{1000};                            // Maximum cache entries
    bool enableCache{true};                               // Enable/disable caching
    bool enablePerformanceMonitoring{false};             // Performance metrics
};

/**
 * @brief Performance statistics for monitoring
 */
struct PerformanceStats {
    size_t totalQueries{0};
    size_t cacheHits{0};
    size_t cacheMisses{0};
    std::chrono::microseconds totalQueryTime{0};
    std::chrono::microseconds averageQueryTime{0};

    double getCacheHitRatio() const {
        return totalQueries > 0 ? static_cast<double>(cacheHits) / totalQueries : 0.0;
    }
};

/**
 * @brief Cache entry for shortcut detection results
 */
struct CacheEntry {
    ShortcutCheckResult result;
    std::chrono::steady_clock::time_point timestamp;

    CacheEntry(const ShortcutCheckResult& res)
        : result(res), timestamp(std::chrono::steady_clock::now()) {}

    bool isExpired(std::chrono::milliseconds ttl) const {
        auto now = std::chrono::steady_clock::now();
        return (now - timestamp) > ttl;
    }
};

/**
 * @brief Cache entry for process list
 */
struct ProcessCacheEntry {
    std::vector<std::string> processes;
    std::chrono::steady_clock::time_point timestamp;

    ProcessCacheEntry(const std::vector<std::string>& procs)
        : processes(procs), timestamp(std::chrono::steady_clock::now()) {}

    bool isExpired(std::chrono::milliseconds ttl) const {
        auto now = std::chrono::steady_clock::now();
        return (now - timestamp) > ttl;
    }
};

/**
 * @brief Implementation class for ShortcutDetector using PIMPL idiom with intelligent caching
 *
 * This class provides the actual implementation for keyboard shortcut detection
 * on Windows systems with performance optimizations through caching.
 */
class ShortcutDetectorImpl {
public:
    ShortcutDetectorImpl();
    explicit ShortcutDetectorImpl(const CacheConfig& config);
    ~ShortcutDetectorImpl();

    /**
     * @brief Check if a keyboard shortcut is captured by system or applications
     *
     * @param shortcut The shortcut to check
     * @return ShortcutCheckResult Result containing status and details
     */
    ShortcutCheckResult isShortcutCaptured(const Shortcut& shortcut);

    /**
     * @brief Check if a keyboard shortcut is captured with cache bypass option
     *
     * @param shortcut The shortcut to check
     * @param bypassCache If true, skip cache and perform fresh check
     * @return ShortcutCheckResult Result containing status and details
     */
    ShortcutCheckResult isShortcutCaptured(const Shortcut& shortcut, bool bypassCache);

    /**
     * @brief Check if any keyboard hook is currently installed
     *
     * @return true If keyboard hooks are detected
     * @return false If no keyboard hooks are detected
     */
    bool hasKeyboardHookInstalled();

    /**
     * @brief Get list of processes that have keyboard hooks installed
     *
     * @return std::vector<std::string> Process names with keyboard hooks
     */
    std::vector<std::string> getProcessesWithKeyboardHooks();

    /**
     * @brief Clear all cached data
     */
    void clearCache();

    /**
     * @brief Clear cached data for a specific shortcut
     *
     * @param shortcut The shortcut to remove from cache
     */
    void clearShortcutCache(const Shortcut& shortcut);

    /**
     * @brief Update cache configuration
     *
     * @param config New cache configuration
     */
    void updateCacheConfig(const CacheConfig& config);

    /**
     * @brief Get current cache configuration
     *
     * @return const CacheConfig& Current configuration
     */
    const CacheConfig& getCacheConfig() const;

    /**
     * @brief Get performance statistics
     *
     * @return PerformanceStats Current performance metrics
     */
    PerformanceStats getPerformanceStats() const;

    /**
     * @brief Reset performance statistics
     */
    void resetPerformanceStats();

    /**
     * @brief Get cache size information
     *
     * @return size_t Number of cached entries
     */
    size_t getCacheSize() const;

    /**
     * @brief Check if cache is enabled
     *
     * @return bool True if caching is enabled
     */
    bool isCacheEnabled() const;

    /**
     * @brief Enable or disable caching
     *
     * @param enabled True to enable, false to disable
     */
    void setCacheEnabled(bool enabled);

private:
    // Core detection methods
    bool isSystemReservedShortcut(const Shortcut& shortcut);
    bool attemptHotkeyRegistration(const Shortcut& shortcut);
    bool hasInterceptingKeyboardHook(const Shortcut& shortcut);
    std::string findCapturingApplication(const Shortcut& shortcut);
    std::string findKeyboardHookOwner();

    // Cache management methods
    ShortcutCheckResult performShortcutCheck(const Shortcut& shortcut);
    void cleanupExpiredEntries();
    void updatePerformanceStats(bool cacheHit, std::chrono::microseconds queryTime);

    /**
     * @brief Static map of Windows system-reserved keyboard shortcuts
     */
    static const std::unordered_map<uint32_t, std::vector<uint32_t>>
        systemReservedShortcuts;

    // Cache configuration and data
    CacheConfig config_;
    mutable std::mutex cacheMutex_;
    std::unordered_map<Shortcut, std::unique_ptr<CacheEntry>> shortcutCache_;
    std::unique_ptr<ProcessCacheEntry> processCache_;
    std::chrono::steady_clock::time_point lastHookCheck_;
    bool lastHookResult_;

    // Performance monitoring
    mutable std::mutex statsMutex_;
    PerformanceStats stats_;
};

}  // namespace shortcut_detector
