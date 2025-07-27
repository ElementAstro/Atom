#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include "shortcut.h"
#include "status.h"

namespace shortcut_detector {

// Forward declaration of the implementation
class ShortcutDetectorImpl;

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
 * @brief Main class for detecting if keyboard shortcuts are captured with intelligent caching
 */
class ShortcutDetector {
public:
    /**
     * @brief Construct a new Shortcut Detector with default configuration
     */
    ShortcutDetector();

    /**
     * @brief Construct a new Shortcut Detector with custom configuration
     *
     * @param config Cache configuration
     */
    explicit ShortcutDetector(const CacheConfig& config);

    /**
     * @brief Destroy the Shortcut Detector
     */
    ~ShortcutDetector();

    // Prevent copy and move
    ShortcutDetector(const ShortcutDetector&) = delete;
    ShortcutDetector& operator=(const ShortcutDetector&) = delete;
    ShortcutDetector(ShortcutDetector&&) = delete;
    ShortcutDetector& operator=(ShortcutDetector&&) = delete;

    /**
     * @brief Check if a shortcut is captured by the system or another application
     *
     * @param shortcut The shortcut to check
     * @return ShortcutCheckResult Result containing status and details
     */
    ShortcutCheckResult isShortcutCaptured(const Shortcut& shortcut);

    /**
     * @brief Check if a shortcut is captured with cache bypass option
     *
     * @param shortcut The shortcut to check
     * @param bypassCache If true, skip cache and perform fresh check
     * @return ShortcutCheckResult Result containing status and details
     */
    ShortcutCheckResult isShortcutCaptured(const Shortcut& shortcut, bool bypassCache);

    /**
     * @brief Check if a keyboard hook is currently installed
     *
     * @return true If a keyboard hook is detected
     * @return false If no keyboard hook is detected
     */
    bool hasKeyboardHookInstalled();

    /**
     * @brief Get a list of processes with keyboard hooks
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
    std::unique_ptr<ShortcutDetectorImpl> pImpl;
};

}  // namespace shortcut_detector
