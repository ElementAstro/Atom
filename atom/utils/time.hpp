/*
 * time.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-10-27

Description: Some useful functions about time

**************************************************/

#ifndef ATOM_UTILS_TIME_HPP
#define ATOM_UTILS_TIME_HPP

#include <chrono>
#include <concepts>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>

#include "atom/error/exception.hpp"

namespace atom::utils {

// Forward declarations for concepts
template <typename T>
concept TimeFormattable = requires(const T& t, const std::string& format) {
    { toString(t, format) } -> std::convertible_to<std::string>;
};

class TimeConvertException : public atom::error::Exception {
    using atom::error::Exception::Exception;
};

#define THROW_TIME_CONVERT_ERROR(...)                                       \
    throw atom::utils::TimeConvertException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                            ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_NESTED_TIME_CONVERT_ERROR(...)         \
    atom::utils::RuntimeError::TimeConvertException( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Validates a timestamp string against a specified format
 *
 * @param timestampStr The timestamp string to validate
 * @param format The expected format (default: "%Y-%m-%d %H:%M:%S")
 * @return true if the timestamp matches the format, false otherwise
 */
[[nodiscard]] bool validateTimestampFormat(
    std::string_view timestampStr,
    std::string_view format = "%Y-%m-%d %H:%M:%S");

/**
 * @brief Retrieves the current timestamp as a formatted string.
 *
 * This function returns the current local time formatted as a string with the
 * pattern "%Y-%m-%d %H:%M:%S".
 *
 * @return std::string The current timestamp formatted as "%Y-%m-%d %H:%M:%S".
 * @throws TimeConvertException If time conversion fails
 */
[[nodiscard]] auto getTimestampString() -> std::string;

/**
 * @brief Converts a UTC time string to China Standard Time (CST, UTC+8).
 *
 * This function takes a UTC time string formatted as "%Y-%m-%d %H:%M:%S" and
 * converts it to China Standard Time (CST), returning the time as a string with
 * the same format.
 *
 * @param utcTimeStr A string representing the UTC time in the format "%Y-%m-%d
 * %H:%M:%S".
 *
 * @return std::string The corresponding time in China Standard Time, formatted
 * as "%Y-%m-%d %H:%M:%S".
 * @throws TimeConvertException If the input format is invalid or conversion
 * fails
 */
[[nodiscard]] auto convertToChinaTime(std::string_view utcTimeStr)
    -> std::string;

/**
 * @brief Retrieves the current China Standard Time (CST) as a formatted
 * timestamp string.
 *
 * This function returns the current local time in China Standard Time (CST),
 * formatted as a string with the pattern
 * "%Y-%m-%d %H:%M:%S".
 *
 * @return std::string The current China Standard Time formatted as "%Y-%m-%d
 * %H:%M:%S".
 * @throws TimeConvertException If time conversion fails
 */
[[nodiscard]] auto getChinaTimestampString() -> std::string;

/**
 * @brief Converts a timestamp to a formatted string.
 *
 * This function takes a timestamp (in seconds since the Unix epoch) and
 * converts it to a string representation. The default format is "%Y-%m-%d
 * %H:%M:%S", but it may be adapted based on implementation details.
 *
 * @param timestamp The timestamp to be converted, typically expressed in
 * seconds since the Unix epoch.
 * @param format Optional format string (defaults to "%Y-%m-%d %H:%M:%S")
 *
 * @return std::string The string representation of the timestamp.
 * @throws TimeConvertException If the timestamp is invalid or conversion fails
 */
[[nodiscard]] auto timeStampToString(
    time_t timestamp,
    std::string_view format = "%Y-%m-%d %H:%M:%S") -> std::string;

/**
 * @brief Converts a `tm` structure to a formatted string.
 *
 * This function takes a `std::tm` structure representing a date and time and
 * converts it to a formatted string according to the specified format.
 *
 * @param tm The `std::tm` structure to be converted to a string.
 * @param format A string representing the desired format for the output.
 *
 * @return std::string The formatted time string based on the `tm` structure and
 * format.
 * @throws TimeConvertException If formatting fails
 */
[[nodiscard]] auto toString(const std::tm& tm,
                            std::string_view format) -> std::string;

/**
 * @brief Retrieves the current UTC time as a formatted string.
 *
 * This function returns the current UTC time formatted as a string with the
 * pattern "%Y-%m-%d %H:%M:%S".
 *
 * @return std::string The current UTC time formatted as "%Y-%m-%d %H:%M:%S".
 * @throws TimeConvertException If time conversion fails
 */
[[nodiscard]] auto getUtcTime() -> std::string;

/**
 * @brief Converts a timestamp to a `tm` structure.
 *
 * This function takes a timestamp (in seconds since the Unix epoch) and
 * converts it to a `std::tm` structure, which represents a calendar date and
 * time.
 *
 * @param timestamp The timestamp to be converted, typically expressed in
 * seconds since the Unix epoch.
 *
 * @return std::optional<std::tm> The corresponding `std::tm` structure
 * representing the timestamp, or nullopt if conversion fails.
 */
[[nodiscard]] auto timestampToTime(long long timestamp)
    -> std::optional<std::tm>;

/**
 * @brief Get time elapsed since a specific time point in milliseconds
 *
 * @tparam Clock Clock type (default: std::chrono::steady_clock)
 * @param startTime The starting time point
 * @return int64_t Elapsed time in milliseconds
 */
template <typename Clock = std::chrono::steady_clock>
[[nodiscard]] int64_t getElapsedMilliseconds(
    const typename Clock::time_point& startTime) {
    auto now = Clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                 startTime)
        .count();
}

}  // namespace atom::utils

#endif
