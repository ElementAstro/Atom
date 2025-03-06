#ifndef ATOM_UTILS_QDATETIME_HPP
#define ATOM_UTILS_QDATETIME_HPP

#include <chrono>
#include <ctime>
#include <optional>
#include <string>

#include "atom/utils/qtimezone.hpp"

namespace atom::utils {
class QTimeZone;  // Forward declaration

/**
 * @brief A class representing a point in time with support for various date and
 * time operations.
 *
 * The `QDateTime` class provides functionalities to work with dates and times,
 * including creating `QDateTime` objects from strings, converting to and from
 * different formats, and performing arithmetic operations on dates and times.
 */
class QDateTime {
public:
    /// Type alias for the clock used in this class.
    using Clock = std::chrono::system_clock;

    /// Type alias for time points in the clock's timeline.
    using TimePoint = std::chrono::time_point<Clock>;

    /// Exception class for date-time parsing errors
    class ParseError : public std::runtime_error {
    public:
        explicit ParseError(const std::string& message)
            : std::runtime_error(message) {}
    };

    /**
     * @brief Default constructor for `QDateTime`.
     *
     * Initializes an invalid `QDateTime` instance.
     */
    QDateTime();

    /**
     * @brief Constructs a `QDateTime` object from a date-time string and
     * format.
     *
     * @param dateTimeString The date-time string to parse.
     * @param format The format string to use for parsing the date-time.
     * @throws ParseError if the date-time string cannot be parsed.
     *
     * This constructor parses the provided date-time string according to the
     * specified format and initializes the `QDateTime` object.
     */
    template <StringLike DateTimeStr, StringLike FormatStr>
    QDateTime(DateTimeStr&& dateTimeString, FormatStr&& format);

    /**
     * @brief Constructs a `QDateTime` object from a date-time string, format,
     * and time zone.
     *
     * @param dateTimeString The date-time string to parse.
     * @param format The format string to use for parsing the date-time.
     * @param timeZone The time zone to use for the date-time.
     * @throws ParseError if the date-time string cannot be parsed.
     *
     * This constructor parses the provided date-time string according to the
     * specified format and time zone, and initializes the `QDateTime` object.
     */
    template <StringLike DateTimeStr, StringLike FormatStr>
    QDateTime(DateTimeStr&& dateTimeString, FormatStr&& format,
              const QTimeZone& timeZone);

    /**
     * @brief Returns the current date and time.
     *
     * @return A `QDateTime` object representing the current date and time.
     *
     * This static method provides the current date and time based on the system
     * clock.
     */
    static auto currentDateTime() -> QDateTime;

    /**
     * @brief Returns the current date and time in the specified time zone.
     *
     * @param timeZone The time zone to use for the current date and time.
     *
     * @return A `QDateTime` object representing the current date and time in
     * the specified time zone.
     */
    static auto currentDateTime(const QTimeZone& timeZone) -> QDateTime;

    /**
     * @brief Constructs a `QDateTime` object from a date-time string and
     * format.
     *
     * @param dateTimeString The date-time string to parse.
     * @param format The format string to use for parsing the date-time.
     * @throws ParseError if the date-time string cannot be parsed.
     *
     * @return A `QDateTime` object initialized from the provided date-time
     * string and format.
     */
    template <StringLike DateTimeStr, StringLike FormatStr>
    static auto fromString(DateTimeStr&& dateTimeString,
                           FormatStr&& format) -> QDateTime;

    /**
     * @brief Constructs a `QDateTime` object from a date-time string, format,
     * and time zone.
     *
     * @param dateTimeString The date-time string to parse.
     * @param format The format string to use for parsing the date-time.
     * @param timeZone The time zone to use for the date-time.
     * @throws ParseError if the date-time string cannot be parsed.
     *
     * @return A `QDateTime` object initialized from the provided date-time
     * string, format, and time zone.
     */
    template <StringLike DateTimeStr, StringLike FormatStr>
    static auto fromString(DateTimeStr&& dateTimeString, FormatStr&& format,
                           const QTimeZone& timeZone) -> QDateTime;

    /**
     * @brief Converts the `QDateTime` object to a string in the specified
     * format.
     *
     * @param format The format string to use for the conversion.
     *
     * @return A string representation of the `QDateTime` object in the
     * specified format.
     *
     * This method converts the `QDateTime` object to a string according to the
     * provided format.
     */
    template <StringLike FormatStr>
    [[nodiscard]] auto toString(FormatStr&& format) const -> std::string;

    /**
     * @brief Converts the `QDateTime` object to a string in the specified
     * format and time zone.
     *
     * @param format The format string to use for the conversion.
     * @param timeZone The time zone to use for the conversion.
     *
     * @return A string representation of the `QDateTime` object in the
     * specified format and time zone.
     */
    template <StringLike FormatStr>
    [[nodiscard]] auto toString(FormatStr&& format,
                                const QTimeZone& timeZone) const -> std::string;

    /**
     * @brief Converts the `QDateTime` object to a `std::time_t` value.
     *
     * @return A `std::time_t` value representing the `QDateTime` object.
     *
     * This method converts the `QDateTime` object to a `std::time_t` value,
     * which is a time representation used by C++ standard library functions.
     */
    [[nodiscard]] auto toTimeT() const -> std::time_t;

    /**
     * @brief Checks if the `QDateTime` object is valid.
     *
     * @return `true` if the `QDateTime` object is valid, `false` otherwise.
     *
     * This method determines whether the `QDateTime` object represents a valid
     * date and time.
     */
    [[nodiscard]] auto isValid() const -> bool;

    /**
     * @brief Adds a number of days to the `QDateTime` object.
     *
     * @param days The number of days to add.
     *
     * @return A new `QDateTime` object representing the date and time after
     * adding the specified number of days.
     *
     * This method creates a new `QDateTime` object by adding the specified
     * number of days to the current `QDateTime` object.
     */
    [[nodiscard]] auto addDays(int days) const -> QDateTime;

    /**
     * @brief Adds a number of seconds to the `QDateTime` object.
     *
     * @param seconds The number of seconds to add.
     *
     * @return A new `QDateTime` object representing the date and time after
     * adding the specified number of seconds.
     *
     * This method creates a new `QDateTime` object by adding the specified
     * number of seconds to the current `QDateTime` object.
     */
    [[nodiscard]] auto addSecs(int seconds) const -> QDateTime;

    /**
     * @brief Computes the number of days between the current `QDateTime` object
     * and another `QDateTime` object.
     *
     * @param other The other `QDateTime` object to compare.
     *
     * @return The number of days between the current `QDateTime` object and the
     * `other` object.
     *
     * This method calculates the difference in days between the current
     * `QDateTime` object and the specified `other` object.
     */
    [[nodiscard]] auto daysTo(const QDateTime& other) const -> int;

    /**
     * @brief Computes the number of seconds between the current `QDateTime`
     * object and another `QDateTime` object.
     *
     * @param other The other `QDateTime` object to compare.
     *
     * @return The number of seconds between the current `QDateTime` object and
     * the `other` object.
     *
     * This method calculates the difference in seconds between the current
     * `QDateTime` object and the specified `other` object.
     */
    [[nodiscard]] auto secsTo(const QDateTime& other) const -> int;

    /**
     * @brief Compares the current `QDateTime` object with another `QDateTime`
     * object.
     *
     * @param other The other `QDateTime` object to compare.
     *
     * @return A comparison result: less than, equal to, or greater than the
     * `other` object.
     *
     * This method allows for comparison of `QDateTime` objects using the
     * spaceship operator (`<=>`), which supports three-way comparisons.
     */
    auto operator<=>(const QDateTime& other) const = default;

private:
    std::optional<TimePoint>
        dateTime_;  ///< Optional time point representing the date and time

    // Helper method to ensure valid datetime
    [[nodiscard]] auto ensureValid() const -> bool;
};

}  // namespace atom::utils

#endif

#ifndef ATOM_UTILS_QDATETIME_TPP
#define ATOM_UTILS_QDATETIME_TPP

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace atom::utils {

template <StringLike DateTimeStr, StringLike FormatStr>
QDateTime::QDateTime(DateTimeStr&& dateTimeString, FormatStr&& format) {
    std::string dtStr{std::forward<DateTimeStr>(dateTimeString)};
    std::string fmtStr{std::forward<FormatStr>(format)};

    std::istringstream ss(dtStr);
    std::tm t = {};
    ss >> std::get_time(&t, fmtStr.c_str());

    if (ss.fail()) {
        throw ParseError("Failed to parse datetime string: " + dtStr);
    }

    dateTime_ = Clock::from_time_t(std::mktime(&t));
}

template <StringLike DateTimeStr, StringLike FormatStr>
QDateTime::QDateTime(DateTimeStr&& dateTimeString, FormatStr&& format,
                     const QTimeZone& timeZone) {
    std::string dtStr{std::forward<DateTimeStr>(dateTimeString)};
    std::string fmtStr{std::forward<FormatStr>(format)};

    std::istringstream ss(dtStr);
    std::tm t = {};
    ss >> std::get_time(&t, fmtStr.c_str());

    if (ss.fail()) {
        throw ParseError("Failed to parse datetime string with timezone: " +
                         dtStr);
    }

    auto time = std::mktime(&t) - timeZone.offsetFromUtc(*this).count();
    dateTime_ = Clock::from_time_t(time);
}

template <StringLike DateTimeStr, StringLike FormatStr>
auto QDateTime::fromString(DateTimeStr&& dateTimeString,
                           FormatStr&& format) -> QDateTime {
    return QDateTime(std::forward<DateTimeStr>(dateTimeString),
                     std::forward<FormatStr>(format));
}

template <StringLike DateTimeStr, StringLike FormatStr>
auto QDateTime::fromString(DateTimeStr&& dateTimeString, FormatStr&& format,
                           const QTimeZone& timeZone) -> QDateTime {
    return QDateTime(std::forward<DateTimeStr>(dateTimeString),
                     std::forward<FormatStr>(format), timeZone);
}

template <StringLike FormatStr>
auto QDateTime::toString(FormatStr&& format) const -> std::string {
    if (!dateTime_) {
        return "";
    }

    try {
        std::string fmtStr{std::forward<FormatStr>(format)};
        std::time_t tt = Clock::to_time_t(dateTime_.value());
        std::tm tm = *std::localtime(&tt);
        std::ostringstream ss;
        ss << std::put_time(&tm, fmtStr.c_str());
        return ss.str();
    } catch (...) {
        return "";
    }
}

template <StringLike FormatStr>
auto QDateTime::toString(FormatStr&& format,
                         const QTimeZone& timeZone) const -> std::string {
    if (!dateTime_) {
        return "";
    }

    try {
        std::string fmtStr{std::forward<FormatStr>(format)};
        auto adjustedTime = dateTime_.value() + timeZone.offsetFromUtc(*this);
        std::time_t tt = Clock::to_time_t(adjustedTime);
        std::tm tm = *std::localtime(&tt);
        std::ostringstream ss;
        ss << std::put_time(&tm, fmtStr.c_str());
        return ss.str();
    } catch (...) {
        return "";
    }
}

}  // namespace atom::utils

#endif  // ATOM_UTILS_QDATETIME_TPP
