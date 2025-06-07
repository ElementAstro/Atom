#pragma once

#include "filter.h"

#include <chrono>
#include <regex>


namespace modern_log {

/**
 * @class BuiltinFilters
 * @brief Factory for common built-in log filter implementations.
 *
 * This class provides static methods to create commonly used log filter
 * functions, such as level filtering, regex-based filtering, rate limiting,
 * user-based filtering, time window filtering, keyword filtering, sampling, and
 * duplicate message suppression. Each method returns a LogFilter::FilterFunc
 * suitable for use with LogFilter.
 */
class BuiltinFilters {
public:
    /**
     * @brief Create a filter that only allows logs at or above a minimum level.
     *
     * @param min_level The minimum log level to allow.
     * @return A filter function that returns true if the log level is >=
     * min_level.
     */
    static LogFilter::FilterFunc level_filter(Level min_level);

    /**
     * @brief Create a filter based on a regular expression match of the log
     * message.
     *
     * @param pattern The regex pattern to match against the log message.
     * @param include If true, only include messages that match; if false,
     * exclude matches.
     * @return A filter function for regex-based filtering.
     */
    static LogFilter::FilterFunc regex_filter(const std::regex& pattern,
                                              bool include = true);

    /**
     * @brief Create a rate-limiting filter to restrict the number of logs per
     * second.
     *
     * @param max_per_second The maximum number of logs allowed per second.
     * @return A filter function that enforces the rate limit.
     */
    static LogFilter::FilterFunc rate_limit_filter(size_t max_per_second);

    /**
     * @brief Create a filter that only allows logs from specific user IDs.
     *
     * @param allowed_users The list of user IDs to allow.
     * @return A filter function that checks the user ID in the log context.
     */
    static LogFilter::FilterFunc user_filter(
        const std::vector<std::string>& allowed_users);

    /**
     * @brief Create a filter that only allows logs within a specific time
     * window.
     *
     * @param start The start time of the allowed window.
     * @param end The end time of the allowed window.
     * @return A filter function that checks the current time against the
     * window.
     */
    static LogFilter::FilterFunc time_window_filter(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end);

    /**
     * @brief Create a filter that includes or excludes logs containing specific
     * keywords.
     *
     * @param keywords The list of keywords to check for in the log message.
     * @param include If true, include messages containing keywords; if false,
     * exclude them.
     * @return A filter function for keyword-based filtering.
     */
    static LogFilter::FilterFunc keyword_filter(
        const std::vector<std::string>& keywords, bool include = true);

    /**
     * @brief Create a sampling filter that allows logs through at a specified
     * rate.
     *
     * @param sample_rate The fraction of logs to allow (0.0 to 1.0).
     * @return A filter function that randomly samples logs.
     */
    static LogFilter::FilterFunc sampling_filter(double sample_rate);

    /**
     * @brief Create a filter that suppresses duplicate log messages within a
     * time window.
     *
     * @param window The time window for duplicate suppression (default: 60
     * seconds).
     * @return A filter function that filters out repeated messages within the
     * window.
     */
    static LogFilter::FilterFunc duplicate_filter(
        std::chrono::seconds window = std::chrono::seconds(60));
};

}  // namespace modern_log