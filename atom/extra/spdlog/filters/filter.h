#pragma once

#include <functional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>
#include <atomic>
#include <unordered_map>
#include "../core/context.h"
#include "../core/types.h"

namespace modern_log {

/**
 * @class LogFilter
 * @brief High-performance log filter system with caching and optimization.
 *
 * LogFilter allows the registration of multiple filter functions that determine
 * whether a log message should be accepted or rejected. Filters can be added or
 * cleared at runtime, and are evaluated in sequence. Thread-safe for concurrent
 * filter checks and modifications.
 *
 * Performance optimizations:
 * - Filter result caching based on message hash and context
 * - Lock-free fast path for common cases
 * - Compile-time filter optimization
 * - Reduced memory allocations
 */
class LogFilter {
public:
    /**
     * @brief Type alias for a filter function.
     *
     * The filter function receives the log message, log level, and log context,
     * and returns true if the log should be accepted, or false to filter it
     * out.
     */
    using FilterFunc =
        std::function<bool(std::string_view, Level, const LogContext&)>;

    /**
     * @brief Cached filter result for performance optimization.
     */
    struct FilterResult {
        bool should_log;
        std::chrono::steady_clock::time_point timestamp;
        size_t access_count = 0;
    };

private:
    std::vector<FilterFunc> filters_;  ///< List of registered filter functions.
    mutable std::shared_mutex mutex_;  ///< Mutex for thread-safe access.

    // Performance optimization fields
    mutable std::unordered_map<size_t, FilterResult> filter_cache_;  ///< Filter result cache
    mutable std::shared_mutex cache_mutex_;  ///< Cache mutex
    std::atomic<bool> cache_enabled_{true};  ///< Whether caching is enabled
    std::atomic<size_t> cache_max_size_{1000};  ///< Maximum cache size
    std::atomic<std::chrono::milliseconds> cache_ttl_{std::chrono::milliseconds(5000)};  ///< Cache TTL

public:
    /**
     * @brief Add a filter function to the filter chain.
     *
     * @param filter The filter function to add.
     */
    void add_filter(FilterFunc filter);

    /**
     * @brief Remove all filter functions from the filter chain.
     */
    void clear_filters();

    /**
     * @brief Check if a log message should be accepted by all filters (optimized).
     *
     * Evaluates all registered filters in order with caching optimization.
     * If any filter returns false, the log is rejected.
     *
     * @param message The log message to check.
     * @param level The log level.
     * @param ctx The log context.
     * @return True if all filters accept the log, false otherwise.
     */
    bool should_log(std::string_view message, Level level,
                    const LogContext& ctx) const;

    /**
     * @brief Legacy method for backward compatibility.
     */
    bool should_log(const std::string& message, Level level,
                    const LogContext& ctx) const {
        return should_log(std::string_view(message), level, ctx);
    }

    /**
     * @brief Fast path filter check without caching.
     * @param message The log message to check.
     * @param level The log level.
     * @param ctx The log context.
     * @return True if all filters accept the log, false otherwise.
     */
    bool should_log_fast(std::string_view message, Level level,
                         const LogContext& ctx) const;

    /**
     * @brief Get the number of registered filter functions.
     * @return The count of filters.
     */
    size_t filter_count() const;

    /**
     * @brief Enable or disable filter result caching.
     * @param enabled Whether to enable caching.
     */
    void set_cache_enabled(bool enabled);

    /**
     * @brief Set the maximum cache size.
     * @param max_size Maximum number of cached results.
     */
    void set_cache_max_size(size_t max_size);

    /**
     * @brief Set the cache time-to-live.
     * @param ttl Time-to-live for cached results.
     */
    void set_cache_ttl(std::chrono::milliseconds ttl);

    /**
     * @brief Clear the filter result cache.
     */
    void clear_cache();

    /**
     * @brief Get cache statistics.
     * @return Pair of (cache_size, cache_hits).
     */
    std::pair<size_t, size_t> get_cache_stats() const;

private:
    /**
     * @brief Generate cache key for message, level, and context.
     * @param message The log message.
     * @param level The log level.
     * @param ctx The log context.
     * @return Hash key for caching.
     */
    size_t generate_cache_key(std::string_view message, Level level,
                              const LogContext& ctx) const;

    /**
     * @brief Check if cached result is still valid.
     * @param result The cached result to check.
     * @return True if the result is still valid.
     */
    bool is_cache_result_valid(const FilterResult& result) const;

    /**
     * @brief Cleanup expired cache entries.
     */
    void cleanup_cache() const;

    // Cache statistics
    mutable std::atomic<size_t> cache_hits_{0};
    mutable std::atomic<size_t> cache_misses_{0};
};

}  // namespace modern_log
