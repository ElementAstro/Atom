/**
 * @file clipboard_config.hpp
 * @brief Configuration, metrics, and cache entry types for clipboard operations.
 *
 * Contains ClipboardConfig for runtime configuration, ClipboardMetrics for
 * performance tracking, and CacheEntry for clipboard data caching.
 */

#ifndef ATOM_SYSTEM_CLIPBOARD_CONFIG_HPP
#define ATOM_SYSTEM_CLIPBOARD_CONFIG_HPP

#include <chrono>
#include <cstddef>
#include <vector>

#include "clipboard_types.hpp"

namespace clip {

/**
 * @brief Runtime configuration for clipboard operations
 */
struct ClipboardConfig {
    /// Enable caching of clipboard data
    bool enable_caching = false;

    /// Enable metrics collection
    bool enable_metrics = false;

    /// Cache time-to-live duration
    std::chrono::milliseconds cache_ttl{5000};
};

/**
 * @brief Performance metrics for clipboard operations
 */
struct ClipboardMetrics {
    /// Total number of operations performed
    std::size_t operations_count = 0;

    /// Total number of errors encountered
    std::size_t errors_count = 0;

    /// Total bytes transferred through clipboard
    std::size_t bytes_transferred = 0;

    /// Number of cache hits
    std::size_t cache_hits = 0;

    /// Number of cache misses
    std::size_t cache_misses = 0;

    /**
     * @brief Reset all metrics to zero
     */
    void reset() noexcept {
        operations_count = 0;
        errors_count = 0;
        bytes_transferred = 0;
        cache_hits = 0;
        cache_misses = 0;
    }
};

/**
 * @brief Cache entry for storing clipboard data with timestamp
 */
struct CacheEntry {
    /// Cached binary data
    std::vector<std::byte> data;

    /// Timestamp when the entry was created
    std::chrono::steady_clock::time_point timestamp =
        std::chrono::steady_clock::now();

    /// Format of the cached data
    ClipboardFormat format{0};

    CacheEntry() = default;

    CacheEntry(std::vector<std::byte> d, ClipboardFormat f)
        : data(std::move(d)),
          timestamp(std::chrono::steady_clock::now()),
          format(f) {}
};

}  // namespace clip

#endif  // ATOM_SYSTEM_CLIPBOARD_CONFIG_HPP
