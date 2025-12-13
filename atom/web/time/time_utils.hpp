/**
 * @file time_utils.hpp
 *
 * @brief Time utility functions with C++20 chrono support
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Modern C++20 time utilities for web operations

**************************************************/

#ifndef ATOM_WEB_TIME_UTILS_HPP
#define ATOM_WEB_TIME_UTILS_HPP

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace atom::web {
namespace time_utils {

/** @brief Minimum valid year */
constexpr int MIN_VALID_YEAR = 1970;
/** @brief Maximum valid year */
constexpr int MAX_VALID_YEAR = 2038;
/** @brief Minimum valid month */
constexpr int MIN_VALID_MONTH = 1;
/** @brief Maximum valid month */
constexpr int MAX_VALID_MONTH = 12;
/** @brief Minimum valid day */
constexpr int MIN_VALID_DAY = 1;
/** @brief Maximum valid day */
constexpr int MAX_VALID_DAY = 31;
/** @brief Minimum valid hour */
constexpr int MIN_VALID_HOUR = 0;
/** @brief Maximum valid hour */
constexpr int MAX_VALID_HOUR = 23;
/** @brief Minimum valid minute */
constexpr int MIN_VALID_MINUTE = 0;
/** @brief Maximum valid minute */
constexpr int MAX_VALID_MINUTE = 59;
/** @brief Minimum valid second */
constexpr int MIN_VALID_SECOND = 0;
/** @brief Maximum valid second */
constexpr int MAX_VALID_SECOND = 59;

/** @brief NTP packet size */
constexpr int NTP_PACKET_SIZE = 48;
/** @brief NTP service port */
constexpr uint16_t NTP_PORT = 123;
/** @brief Seconds difference between 1900 and 1970 */
constexpr uint32_t NTP_DELTA = 2208988800UL;  // seconds between 1900 and 1970

/**
 * @brief Validates date and time parameters
 *
 * @param year Year value
 * @param month Month value
 * @param day Day value
 * @param hour Hour value
 * @param minute Minute value
 * @param second Second value
 *
 * @return true If parameters are valid
 * @return false If parameters are invalid
 */
bool validateDateTime(int year, int month, int day, int hour, int minute,
                      int second);

/**
 * @brief Validates if hostname is valid
 *
 * @param hostname Hostname string
 *
 * @return true If hostname is valid
 * @return false If hostname is invalid
 */
bool validateHostname(std::string_view hostname);

/**
 * @brief Check if a year is a leap year.
 * @param year The year to check.
 * @return True if leap year.
 */
[[nodiscard]] constexpr auto isLeapYear(int year) noexcept -> bool {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/**
 * @brief Get the number of days in a month.
 * @param year The year.
 * @param month The month (1-12).
 * @return Number of days in the month.
 */
[[nodiscard]] constexpr auto getDaysInMonth(int year,
                                            int month) noexcept -> int {
    constexpr int daysInMonth[] = {0,  31, 28, 31, 30, 31, 30,
                                   31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12)
        return 0;
    if (month == 2 && isLeapYear(year))
        return 29;
    return daysInMonth[month];
}

/**
 * @brief Format a time_t value to ISO 8601 string.
 * @param time The time value.
 * @return ISO 8601 formatted string.
 */
[[nodiscard]] auto formatISO8601(std::time_t time) -> std::string;

/**
 * @brief Format a time_t value to RFC 2822 string (for HTTP headers).
 * @param time The time value.
 * @return RFC 2822 formatted string.
 */
[[nodiscard]] auto formatRFC2822(std::time_t time) -> std::string;

/**
 * @brief Parse an ISO 8601 formatted string to time_t.
 * @param str The ISO 8601 string.
 * @return Parsed time value, or nullopt on failure.
 */
[[nodiscard]] auto parseISO8601(std::string_view str)
    -> std::optional<std::time_t>;

/**
 * @brief Parse an RFC 2822 formatted string to time_t.
 * @param str The RFC 2822 string.
 * @return Parsed time value, or nullopt on failure.
 */
[[nodiscard]] auto parseRFC2822(std::string_view str)
    -> std::optional<std::time_t>;

/**
 * @brief Get the current time as a formatted string.
 * @param format The format string (strftime compatible).
 * @return Formatted time string.
 */
[[nodiscard]] auto getCurrentTimeFormatted(
    std::string_view format = "%Y-%m-%d %H:%M:%S") -> std::string;

/**
 * @brief Calculate the difference between two time points.
 * @param start Start time.
 * @param end End time.
 * @return Duration in seconds.
 */
[[nodiscard]] auto timeDifference(std::time_t start,
                                  std::time_t end) -> std::chrono::seconds;

/**
 * @brief Convert UTC time to local time.
 * @param utcTime UTC time value.
 * @return Local time value.
 */
[[nodiscard]] auto utcToLocal(std::time_t utcTime) -> std::time_t;

/**
 * @brief Convert local time to UTC time.
 * @param localTime Local time value.
 * @return UTC time value.
 */
[[nodiscard]] auto localToUtc(std::time_t localTime) -> std::time_t;

// ============================================================================
// C++20 chrono-based functions
// ============================================================================

/**
 * @brief Time point type alias using system clock
 */
using TimePoint = std::chrono::system_clock::time_point;

/**
 * @brief Duration type alias
 */
using Duration = std::chrono::system_clock::duration;

/**
 * @brief Time parsing error codes
 */
enum class TimeError {
    Success = 0,
    InvalidFormat,
    OutOfRange,
    ParseError,
    Unknown
};

/**
 * @brief Convert TimeError to string
 */
[[nodiscard]] constexpr auto timeErrorToString(TimeError error) noexcept
    -> std::string_view {
    switch (error) {
        case TimeError::Success:
            return "Success";
        case TimeError::InvalidFormat:
            return "Invalid format";
        case TimeError::OutOfRange:
            return "Out of range";
        case TimeError::ParseError:
            return "Parse error";
        case TimeError::Unknown:
            return "Unknown error";
        default:
            return "Unknown error";
    }
}

/**
 * @brief Get current time point.
 * @return Current system time point.
 */
[[nodiscard]] inline auto now() noexcept -> TimePoint {
    return std::chrono::system_clock::now();
}

/**
 * @brief Convert time_t to TimePoint.
 * @param time The time_t value.
 * @return Corresponding TimePoint.
 */
[[nodiscard]] inline auto fromTimeT(std::time_t time) noexcept -> TimePoint {
    return std::chrono::system_clock::from_time_t(time);
}

/**
 * @brief Convert TimePoint to time_t.
 * @param tp The TimePoint.
 * @return Corresponding time_t value.
 */
[[nodiscard]] inline auto toTimeT(TimePoint tp) noexcept -> std::time_t {
    return std::chrono::system_clock::to_time_t(tp);
}

/**
 * @brief Format a TimePoint to ISO 8601 string.
 * @param tp The time point.
 * @return ISO 8601 formatted string.
 */
[[nodiscard]] auto formatISO8601(TimePoint tp) -> std::string;

/**
 * @brief Format a TimePoint to RFC 2822 string.
 * @param tp The time point.
 * @return RFC 2822 formatted string.
 */
[[nodiscard]] auto formatRFC2822(TimePoint tp) -> std::string;

/**
 * @brief Format a TimePoint with custom format string.
 * @param tp The time point.
 * @param format The format string (strftime compatible).
 * @return Formatted string.
 */
[[nodiscard]] auto format(TimePoint tp, std::string_view format) -> std::string;

/**
 * @brief Parse ISO 8601 string to TimePoint.
 * @param str The ISO 8601 string.
 * @return Expected containing TimePoint or TimeError.
 */
[[nodiscard]] auto parseISO8601ToTimePoint(std::string_view str)
    -> std::expected<TimePoint, TimeError>;

/**
 * @brief Parse RFC 2822 string to TimePoint.
 * @param str The RFC 2822 string.
 * @return Expected containing TimePoint or TimeError.
 */
[[nodiscard]] auto parseRFC2822ToTimePoint(std::string_view str)
    -> std::expected<TimePoint, TimeError>;

/**
 * @brief Calculate duration between two time points.
 * @param start Start time point.
 * @param end End time point.
 * @return Duration between the points.
 */
[[nodiscard]] inline auto duration(TimePoint start,
                                   TimePoint end) noexcept -> Duration {
    return end - start;
}

/**
 * @brief Calculate duration in seconds.
 * @param start Start time point.
 * @param end End time point.
 * @return Duration in seconds.
 */
[[nodiscard]] inline auto durationSeconds(
    TimePoint start, TimePoint end) noexcept -> std::chrono::seconds {
    return std::chrono::duration_cast<std::chrono::seconds>(end - start);
}

/**
 * @brief Calculate duration in milliseconds.
 * @param start Start time point.
 * @param end End time point.
 * @return Duration in milliseconds.
 */
[[nodiscard]] inline auto durationMillis(
    TimePoint start, TimePoint end) noexcept -> std::chrono::milliseconds {
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
}

/**
 * @brief Add duration to a time point.
 * @param tp The time point.
 * @param d The duration to add.
 * @return New time point.
 */
template <typename Rep, typename Period>
[[nodiscard]] inline auto add(
    TimePoint tp, std::chrono::duration<Rep, Period> d) noexcept -> TimePoint {
    return tp + d;
}

/**
 * @brief Subtract duration from a time point.
 * @param tp The time point.
 * @param d The duration to subtract.
 * @return New time point.
 */
template <typename Rep, typename Period>
[[nodiscard]] inline auto subtract(
    TimePoint tp, std::chrono::duration<Rep, Period> d) noexcept -> TimePoint {
    return tp - d;
}

/**
 * @brief Check if a time point is in the past.
 * @param tp The time point to check.
 * @return True if the time point is before now.
 */
[[nodiscard]] inline auto isPast(TimePoint tp) noexcept -> bool {
    return tp < now();
}

/**
 * @brief Check if a time point is in the future.
 * @param tp The time point to check.
 * @return True if the time point is after now.
 */
[[nodiscard]] inline auto isFuture(TimePoint tp) noexcept -> bool {
    return tp > now();
}

/**
 * @brief Get Unix timestamp (seconds since epoch).
 * @param tp The time point.
 * @return Unix timestamp.
 */
[[nodiscard]] inline auto toUnixTimestamp(TimePoint tp) noexcept -> int64_t {
    return std::chrono::duration_cast<std::chrono::seconds>(
               tp.time_since_epoch())
        .count();
}

/**
 * @brief Get Unix timestamp in milliseconds.
 * @param tp The time point.
 * @return Unix timestamp in milliseconds.
 */
[[nodiscard]] inline auto toUnixTimestampMillis(TimePoint tp) noexcept
    -> int64_t {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               tp.time_since_epoch())
        .count();
}

/**
 * @brief Create TimePoint from Unix timestamp.
 * @param timestamp Unix timestamp in seconds.
 * @return TimePoint.
 */
[[nodiscard]] inline auto fromUnixTimestamp(int64_t timestamp) noexcept
    -> TimePoint {
    return TimePoint{std::chrono::seconds{timestamp}};
}

/**
 * @brief Create TimePoint from Unix timestamp in milliseconds.
 * @param timestampMs Unix timestamp in milliseconds.
 * @return TimePoint.
 */
[[nodiscard]] inline auto fromUnixTimestampMillis(int64_t timestampMs) noexcept
    -> TimePoint {
    return TimePoint{std::chrono::milliseconds{timestampMs}};
}

/**
 * @brief Get the start of the day for a time point.
 * @param tp The time point.
 * @return TimePoint at midnight of the same day.
 */
[[nodiscard]] auto startOfDay(TimePoint tp) -> TimePoint;

/**
 * @brief Get the end of the day for a time point.
 * @param tp The time point.
 * @return TimePoint at 23:59:59 of the same day.
 */
[[nodiscard]] auto endOfDay(TimePoint tp) -> TimePoint;

/**
 * @brief Format duration as human-readable string.
 * @param d The duration.
 * @return Human-readable string (e.g., "2h 30m 15s").
 */
[[nodiscard]] auto formatDuration(Duration d) -> std::string;

/**
 * @brief Format duration as human-readable string.
 * @param seconds Duration in seconds.
 * @return Human-readable string.
 */
[[nodiscard]] auto formatDuration(std::chrono::seconds seconds) -> std::string;

/**
 * @brief Compare two time points with tolerance.
 * @param a First time point.
 * @param b Second time point.
 * @param tolerance Maximum allowed difference.
 * @return True if time points are within tolerance.
 */
template <typename Rep, typename Period>
[[nodiscard]] inline auto approximatelyEqual(
    TimePoint a, TimePoint b,
    std::chrono::duration<Rep, Period> tolerance) noexcept -> bool {
    auto diff = (a > b) ? (a - b) : (b - a);
    return diff <= tolerance;
}

}  // namespace time_utils
}  // namespace atom::web

#endif  // ATOM_WEB_TIME_UTILS_HPP
