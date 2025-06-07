#pragma once

#include <functional>
#include <shared_mutex>
#include <string>
#include <vector>
#include "../core/context.h"
#include "../core/types.h"

namespace modern_log {

/**
 * @class LogFilter
 * @brief Base class for log filters supporting chainable filtering.
 *
 * LogFilter allows the registration of multiple filter functions that determine
 * whether a log message should be accepted or rejected. Filters can be added or
 * cleared at runtime, and are evaluated in sequence. Thread-safe for concurrent
 * filter checks and modifications.
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
        std::function<bool(const std::string&, Level, const LogContext&)>;

private:
    std::vector<FilterFunc> filters_;  ///< List of registered filter functions.
    mutable std::shared_mutex mutex_;  ///< Mutex for thread-safe access.

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
     * @brief Check if a log message should be accepted by all filters.
     *
     * Evaluates all registered filters in order. If any filter returns false,
     * the log is rejected.
     *
     * @param message The log message to check.
     * @param level The log level.
     * @param ctx The log context.
     * @return True if all filters accept the log, false otherwise.
     */
    bool should_log(const std::string& message, Level level,
                    const LogContext& ctx) const;

    /**
     * @brief Get the number of registered filter functions.
     * @return The count of filters.
     */
    size_t filter_count() const;
};

}  // namespace modern_log