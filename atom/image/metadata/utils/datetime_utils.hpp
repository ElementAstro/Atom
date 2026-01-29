#ifndef ATOM_IMAGE_METADATA_DATETIME_UTILS_HPP
#define ATOM_IMAGE_METADATA_DATETIME_UTILS_HPP

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace atom::image::metadata {

/**
 * @brief Date/time utilities for metadata parsing and formatting
 */
class DateTimeUtils {
public:
    using TimePoint = std::chrono::system_clock::time_point;

    /**
     * @brief Parse EXIF datetime format "YYYY:MM:DD HH:MM:SS"
     * @param str Date/time string in EXIF format
     * @return Parsed time point or nullopt on failure
     */
    [[nodiscard]] static std::optional<TimePoint> parseExifDateTime(
        std::string_view str);

    /**
     * @brief Parse ISO 8601 datetime format
     * Supports formats:
     * - "YYYY-MM-DDTHH:MM:SS"
     * - "YYYY-MM-DDTHH:MM:SSZ"
     * - "YYYY-MM-DDTHH:MM:SS+HH:MM"
     * - "YYYY-MM-DDTHH:MM:SS.sss"
     * @param str Date/time string in ISO 8601 format
     * @return Parsed time point or nullopt on failure
     */
    [[nodiscard]] static std::optional<TimePoint> parseIso8601(
        std::string_view str);

    /**
     * @brief Parse GPS timestamp (separate date and time strings)
     * @param date Date string "YYYY:MM:DD"
     * @param time Time string "HH:MM:SS" or three rational values
     * @return Parsed time point or nullopt on failure
     */
    [[nodiscard]] static std::optional<TimePoint> parseGpsDateTime(
        std::string_view date, std::string_view time);

    /**
     * @brief Parse GPS timestamp from rationals
     * @param date Date string "YYYY:MM:DD"
     * @param hour Hour as rational numerator/denominator
     * @param minute Minute as rational
     * @param second Second as rational
     * @return Parsed time point or nullopt on failure
     */
    [[nodiscard]] static std::optional<TimePoint> parseGpsDateTime(
        std::string_view date, double hour, double minute, double second);

    /**
     * @brief Parse various datetime formats automatically
     * @param str Date/time string
     * @return Parsed time point or nullopt on failure
     */
    [[nodiscard]] static std::optional<TimePoint> parseAuto(
        std::string_view str);

    /**
     * @brief Format time point as EXIF datetime "YYYY:MM:DD HH:MM:SS"
     * @param tp Time point to format
     * @return Formatted string
     */
    [[nodiscard]] static std::string formatExifDateTime(const TimePoint& tp);

    /**
     * @brief Format time point as ISO 8601 "YYYY-MM-DDTHH:MM:SSZ"
     * @param tp Time point to format
     * @param includeTimezone Whether to include timezone
     * @return Formatted string
     */
    [[nodiscard]] static std::string formatIso8601(const TimePoint& tp,
                                                   bool includeTimezone = true);

    /**
     * @brief Format time point as GPS date "YYYY:MM:DD"
     * @param tp Time point to format
     * @return Formatted date string
     */
    [[nodiscard]] static std::string formatGpsDate(const TimePoint& tp);

    /**
     * @brief Format time point as GPS time "HH:MM:SS"
     * @param tp Time point to format
     * @return Formatted time string
     */
    [[nodiscard]] static std::string formatGpsTime(const TimePoint& tp);

    /**
     * @brief Get GPS time as rationals (hour, minute, second)
     * @param tp Time point
     * @param hour Output hour
     * @param minute Output minute
     * @param second Output second
     */
    static void getGpsTimeRationals(const TimePoint& tp, double& hour,
                                    double& minute, double& second);

    /**
     * @brief Get current time
     */
    [[nodiscard]] static TimePoint now() {
        return std::chrono::system_clock::now();
    }

    /**
     * @brief Parse subseconds from string like "123" (milliseconds)
     * @param str Subsecond string
     * @return Duration in microseconds
     */
    [[nodiscard]] static std::chrono::microseconds parseSubseconds(
        std::string_view str);

    /**
     * @brief Format subseconds to string
     * @param tp Time point
     * @param precision Number of decimal digits (1-6)
     * @return Subsecond string
     */
    [[nodiscard]] static std::string formatSubseconds(const TimePoint& tp,
                                                      int precision = 3);

    /**
     * @brief Add subseconds to time point
     * @param tp Base time point
     * @param subseconds Subsecond string
     * @return Time point with subseconds added
     */
    [[nodiscard]] static TimePoint addSubseconds(const TimePoint& tp,
                                                 std::string_view subseconds);

    /**
     * @brief Check if year is leap year
     */
    [[nodiscard]] static bool isLeapYear(int year) noexcept {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    /**
     * @brief Get days in month
     */
    [[nodiscard]] static int daysInMonth(int year, int month) noexcept;

    /**
     * @brief Validate date components
     */
    [[nodiscard]] static bool isValidDate(int year, int month,
                                          int day) noexcept;

    /**
     * @brief Validate time components
     */
    [[nodiscard]] static bool isValidTime(int hour, int minute,
                                          int second) noexcept {
        return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 &&
               second >= 0 && second <= 60;  // 60 for leap second
    }

    /**
     * @brief Convert local time to UTC
     * @param local Local time point
     * @param utcOffsetMinutes Offset in minutes (e.g., +540 for JST)
     * @return UTC time point
     */
    [[nodiscard]] static TimePoint localToUtc(const TimePoint& local,
                                              int utcOffsetMinutes);

    /**
     * @brief Convert UTC to local time
     * @param utc UTC time point
     * @param utcOffsetMinutes Offset in minutes
     * @return Local time point
     */
    [[nodiscard]] static TimePoint utcToLocal(const TimePoint& utc,
                                              int utcOffsetMinutes);

    /**
     * @brief Get system timezone offset in minutes
     */
    [[nodiscard]] static int getSystemTimezoneOffset();

private:
    friend struct DateTimeComponents;

    /**
     * @brief Parse timezone offset from string like "+09:00" or "Z"
     * @param str Timezone string
     * @return Offset in minutes, or nullopt for invalid input
     */
    [[nodiscard]] static std::optional<int> parseTimezoneOffset(
        std::string_view str);

    /**
     * @brief Build time_point from components
     */
    [[nodiscard]] static std::optional<TimePoint> buildTimePoint(
        int year, int month, int day, int hour, int minute, int second,
        int microseconds = 0);
};

/**
 * @brief Parsed date/time components
 */
struct DateTimeComponents {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int microsecond = 0;
    std::optional<int> utcOffsetMinutes;  // nullopt = local/unknown

    /**
     * @brief Check if all required fields are set
     */
    [[nodiscard]] bool isComplete() const noexcept {
        return year > 0 && month > 0 && day > 0;
    }

    /**
     * @brief Check if time components are set
     */
    [[nodiscard]] bool hasTime() const noexcept {
        return hour >= 0 || minute >= 0 || second >= 0;
    }

    /**
     * @brief Convert to time point
     */
    [[nodiscard]] std::optional<DateTimeUtils::TimePoint> toTimePoint() const;

    /**
     * @brief Create from time point
     */
    [[nodiscard]] static DateTimeComponents fromTimePoint(
        const DateTimeUtils::TimePoint& tp, bool utc = true);

    /**
     * @brief Format as EXIF datetime
     */
    [[nodiscard]] std::string formatExif() const;

    /**
     * @brief Format as ISO 8601
     */
    [[nodiscard]] std::string formatIso8601() const;
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_DATETIME_UTILS_HPP
