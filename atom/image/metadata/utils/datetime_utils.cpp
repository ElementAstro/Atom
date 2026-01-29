#include "datetime_utils.hpp"

#include <array>
#include <charconv>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace atom::image::metadata {

namespace {

// Helper to parse integer from string_view
std::optional<int> parseInt(std::string_view str) {
    if (str.empty())
        return std::nullopt;
    int value = 0;
    auto result = std::from_chars(str.data(), str.data() + str.size(), value);
    if (result.ec != std::errc{} || result.ptr != str.data() + str.size()) {
        return std::nullopt;
    }
    return value;
}

// Helper to parse double from string
std::optional<double> parseDouble(std::string_view str) {
    if (str.empty())
        return std::nullopt;
    try {
        return std::stod(std::string(str));
    } catch (...) {
        return std::nullopt;
    }
}

// Trim whitespace
std::string_view trim(std::string_view str) {
    size_t start = 0;
    while (start < str.size() &&
           std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    size_t end = str.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    return str.substr(start, end - start);
}

}  // namespace

std::optional<DateTimeUtils::TimePoint> DateTimeUtils::parseExifDateTime(
    std::string_view str) {
    str = trim(str);
    if (str.size() < 19)
        return std::nullopt;

    // Format: "YYYY:MM:DD HH:MM:SS"
    auto year = parseInt(str.substr(0, 4));
    auto month = parseInt(str.substr(5, 2));
    auto day = parseInt(str.substr(8, 2));
    auto hour = parseInt(str.substr(11, 2));
    auto minute = parseInt(str.substr(14, 2));
    auto second = parseInt(str.substr(17, 2));

    if (!year || !month || !day || !hour || !minute || !second) {
        return std::nullopt;
    }

    return buildTimePoint(*year, *month, *day, *hour, *minute, *second);
}

std::optional<DateTimeUtils::TimePoint> DateTimeUtils::parseIso8601(
    std::string_view str) {
    str = trim(str);
    if (str.size() < 10)
        return std::nullopt;

    // Parse date part "YYYY-MM-DD"
    auto year = parseInt(str.substr(0, 4));
    if (!year || str[4] != '-')
        return std::nullopt;

    auto month = parseInt(str.substr(5, 2));
    if (!month || str[7] != '-')
        return std::nullopt;

    auto day = parseInt(str.substr(8, 2));
    if (!day)
        return std::nullopt;

    int hour = 0, minute = 0, second = 0, microsecond = 0;
    int utcOffsetMinutes = 0;
    bool hasTimezone = false;

    // Check for time part
    if (str.size() > 10 && (str[10] == 'T' || str[10] == ' ')) {
        str = str.substr(11);

        if (str.size() >= 8) {
            auto h = parseInt(str.substr(0, 2));
            auto m = parseInt(str.substr(3, 2));
            auto s = parseInt(str.substr(6, 2));

            if (h && m && s) {
                hour = *h;
                minute = *m;
                second = *s;
            }

            str = str.substr(8);

            // Check for fractional seconds
            if (!str.empty() && str[0] == '.') {
                size_t fracEnd = 1;
                while (fracEnd < str.size() &&
                       std::isdigit(static_cast<unsigned char>(str[fracEnd]))) {
                    ++fracEnd;
                }
                std::string fracStr(str.substr(1, fracEnd - 1));
                while (fracStr.size() < 6)
                    fracStr += '0';
                if (fracStr.size() > 6)
                    fracStr = fracStr.substr(0, 6);
                if (auto us = parseInt(fracStr)) {
                    microsecond = *us;
                }
                str = str.substr(fracEnd);
            }

            // Check for timezone
            if (!str.empty()) {
                auto offset = parseTimezoneOffset(str);
                if (offset) {
                    utcOffsetMinutes = *offset;
                    hasTimezone = true;
                }
            }
        }
    }

    auto tp =
        buildTimePoint(*year, *month, *day, hour, minute, second, microsecond);
    if (!tp)
        return std::nullopt;

    // Adjust for timezone if present
    if (hasTimezone && utcOffsetMinutes != 0) {
        *tp -= std::chrono::minutes(utcOffsetMinutes);
    }

    return tp;
}

std::optional<DateTimeUtils::TimePoint> DateTimeUtils::parseGpsDateTime(
    std::string_view date, std::string_view time) {
    date = trim(date);
    time = trim(time);

    // Parse date "YYYY:MM:DD"
    if (date.size() < 10)
        return std::nullopt;

    auto year = parseInt(date.substr(0, 4));
    auto month = parseInt(date.substr(5, 2));
    auto day = parseInt(date.substr(8, 2));

    if (!year || !month || !day)
        return std::nullopt;

    int hour = 0, minute = 0, second = 0;

    // Parse time "HH:MM:SS" or "HH:MM:SS.ss"
    if (time.size() >= 8) {
        auto h = parseInt(time.substr(0, 2));
        auto m = parseInt(time.substr(3, 2));
        auto s = parseInt(time.substr(6, 2));

        if (h && m && s) {
            hour = *h;
            minute = *m;
            second = *s;
        }
    }

    return buildTimePoint(*year, *month, *day, hour, minute, second);
}

std::optional<DateTimeUtils::TimePoint> DateTimeUtils::parseGpsDateTime(
    std::string_view date, double hour, double minute, double second) {
    date = trim(date);

    if (date.size() < 10)
        return std::nullopt;

    auto year = parseInt(date.substr(0, 4));
    auto month = parseInt(date.substr(5, 2));
    auto day = parseInt(date.substr(8, 2));

    if (!year || !month || !day)
        return std::nullopt;

    int h = static_cast<int>(hour);
    int m = static_cast<int>(minute);
    int s = static_cast<int>(second);
    int us = static_cast<int>((second - s) * 1000000);

    return buildTimePoint(*year, *month, *day, h, m, s, us);
}

std::optional<DateTimeUtils::TimePoint> DateTimeUtils::parseAuto(
    std::string_view str) {
    str = trim(str);
    if (str.empty())
        return std::nullopt;

    // Try EXIF format first (has colons in date)
    if (str.size() >= 19 && str[4] == ':' && str[7] == ':') {
        return parseExifDateTime(str);
    }

    // Try ISO 8601 (has dashes in date)
    if (str.size() >= 10 && str[4] == '-' && str[7] == '-') {
        return parseIso8601(str);
    }

    return std::nullopt;
}

std::string DateTimeUtils::formatExifDateTime(const TimePoint& tp) {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << (tm.tm_year + 1900) << ':'
        << std::setw(2) << (tm.tm_mon + 1) << ':' << std::setw(2) << tm.tm_mday
        << ' ' << std::setw(2) << tm.tm_hour << ':' << std::setw(2) << tm.tm_min
        << ':' << std::setw(2) << tm.tm_sec;
    return oss.str();
}

std::string DateTimeUtils::formatIso8601(const TimePoint& tp,
                                         bool includeTimezone) {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << (tm.tm_year + 1900) << '-'
        << std::setw(2) << (tm.tm_mon + 1) << '-' << std::setw(2) << tm.tm_mday
        << 'T' << std::setw(2) << tm.tm_hour << ':' << std::setw(2) << tm.tm_min
        << ':' << std::setw(2) << tm.tm_sec;

    if (includeTimezone) {
        oss << 'Z';
    }

    return oss.str();
}

std::string DateTimeUtils::formatGpsDate(const TimePoint& tp) {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << (tm.tm_year + 1900) << ':'
        << std::setw(2) << (tm.tm_mon + 1) << ':' << std::setw(2) << tm.tm_mday;
    return oss.str();
}

std::string DateTimeUtils::formatGpsTime(const TimePoint& tp) {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << tm.tm_hour << ':'
        << std::setw(2) << tm.tm_min << ':' << std::setw(2) << tm.tm_sec;
    return oss.str();
}

void DateTimeUtils::getGpsTimeRationals(const TimePoint& tp, double& hour,
                                        double& minute, double& second) {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif

    // Get microseconds
    auto duration = tp.time_since_epoch();
    auto seconds_duration =
        std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        duration - seconds_duration);

    hour = static_cast<double>(tm.tm_hour);
    minute = static_cast<double>(tm.tm_min);
    second = static_cast<double>(tm.tm_sec) + micros.count() / 1000000.0;
}

std::chrono::microseconds DateTimeUtils::parseSubseconds(std::string_view str) {
    if (str.empty())
        return std::chrono::microseconds(0);

    std::string padded(str);
    while (padded.size() < 6)
        padded += '0';
    if (padded.size() > 6)
        padded = padded.substr(0, 6);

    if (auto us = parseInt(padded)) {
        return std::chrono::microseconds(*us);
    }
    return std::chrono::microseconds(0);
}

std::string DateTimeUtils::formatSubseconds(const TimePoint& tp,
                                            int precision) {
    auto duration = tp.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        duration - seconds);

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(6) << micros.count();
    std::string result = oss.str();

    if (precision < 6) {
        result = result.substr(0, precision);
    }
    return result;
}

DateTimeUtils::TimePoint DateTimeUtils::addSubseconds(
    const TimePoint& tp, std::string_view subseconds) {
    return tp + parseSubseconds(subseconds);
}

int DateTimeUtils::daysInMonth(int year, int month) noexcept {
    static constexpr std::array<int, 12> days = {31, 28, 31, 30, 31, 30,
                                                 31, 31, 30, 31, 30, 31};

    if (month < 1 || month > 12)
        return 0;

    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return days[month - 1];
}

bool DateTimeUtils::isValidDate(int year, int month, int day) noexcept {
    if (year < 1 || month < 1 || month > 12 || day < 1) {
        return false;
    }
    return day <= daysInMonth(year, month);
}

DateTimeUtils::TimePoint DateTimeUtils::localToUtc(const TimePoint& local,
                                                   int utcOffsetMinutes) {
    return local - std::chrono::minutes(utcOffsetMinutes);
}

DateTimeUtils::TimePoint DateTimeUtils::utcToLocal(const TimePoint& utc,
                                                   int utcOffsetMinutes) {
    return utc + std::chrono::minutes(utcOffsetMinutes);
}

int DateTimeUtils::getSystemTimezoneOffset() {
    std::time_t now = std::time(nullptr);
    std::tm local_tm{};
    std::tm utc_tm{};

#if defined(_WIN32)
    localtime_s(&local_tm, &now);
    gmtime_s(&utc_tm, &now);
#else
    localtime_r(&now, &local_tm);
    gmtime_r(&now, &utc_tm);
#endif

    // Calculate difference in minutes
    int local_minutes = local_tm.tm_hour * 60 + local_tm.tm_min;
    int utc_minutes = utc_tm.tm_hour * 60 + utc_tm.tm_min;

    int offset = local_minutes - utc_minutes;

    // Handle day boundary
    if (local_tm.tm_mday != utc_tm.tm_mday) {
        if (local_tm.tm_mday > utc_tm.tm_mday ||
            (local_tm.tm_mday == 1 && utc_tm.tm_mday > 1)) {
            offset += 24 * 60;
        } else {
            offset -= 24 * 60;
        }
    }

    return offset;
}

std::optional<int> DateTimeUtils::parseTimezoneOffset(std::string_view str) {
    if (str.empty())
        return std::nullopt;

    // UTC/Zulu
    if (str[0] == 'Z' || str[0] == 'z') {
        return 0;
    }

    // +/-HH:MM or +/-HHMM
    if (str[0] != '+' && str[0] != '-') {
        return std::nullopt;
    }

    int sign = (str[0] == '+') ? 1 : -1;
    str = str.substr(1);

    int hours = 0, minutes = 0;

    if (str.size() >= 5 && str[2] == ':') {
        // +HH:MM format
        auto h = parseInt(str.substr(0, 2));
        auto m = parseInt(str.substr(3, 2));
        if (!h || !m)
            return std::nullopt;
        hours = *h;
        minutes = *m;
    } else if (str.size() >= 4) {
        // +HHMM format
        auto h = parseInt(str.substr(0, 2));
        auto m = parseInt(str.substr(2, 2));
        if (!h || !m)
            return std::nullopt;
        hours = *h;
        minutes = *m;
    } else if (str.size() >= 2) {
        // +HH format
        auto h = parseInt(str.substr(0, 2));
        if (!h)
            return std::nullopt;
        hours = *h;
    } else {
        return std::nullopt;
    }

    return sign * (hours * 60 + minutes);
}

std::optional<DateTimeUtils::TimePoint> DateTimeUtils::buildTimePoint(
    int year, int month, int day, int hour, int minute, int second,
    int microseconds) {
    if (!isValidDate(year, month, day) || !isValidTime(hour, minute, second)) {
        return std::nullopt;
    }

    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    tm.tm_isdst = 0;

#if defined(_WIN32)
    std::time_t time = _mkgmtime(&tm);
#else
    std::time_t time = timegm(&tm);
#endif

    if (time == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }

    auto tp = std::chrono::system_clock::from_time_t(time);
    tp += std::chrono::microseconds(microseconds);
    return tp;
}

// DateTimeComponents implementation

std::optional<DateTimeUtils::TimePoint> DateTimeComponents::toTimePoint()
    const {
    if (!isComplete())
        return std::nullopt;

    auto tp = DateTimeUtils::buildTimePoint(year, month, day, hour, minute,
                                            second, microsecond);
    if (!tp)
        return std::nullopt;

    // Adjust for timezone if known
    if (utcOffsetMinutes.has_value() && *utcOffsetMinutes != 0) {
        *tp -= std::chrono::minutes(*utcOffsetMinutes);
    }

    return tp;
}

DateTimeComponents DateTimeComponents::fromTimePoint(
    const DateTimeUtils::TimePoint& tp, bool utc) {
    DateTimeComponents c;

    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};

    if (utc) {
#if defined(_WIN32)
        gmtime_s(&tm, &time);
#else
        gmtime_r(&time, &tm);
#endif
        c.utcOffsetMinutes = 0;
    } else {
#if defined(_WIN32)
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif
        c.utcOffsetMinutes = DateTimeUtils::getSystemTimezoneOffset();
    }

    c.year = tm.tm_year + 1900;
    c.month = tm.tm_mon + 1;
    c.day = tm.tm_mday;
    c.hour = tm.tm_hour;
    c.minute = tm.tm_min;
    c.second = tm.tm_sec;

    // Get microseconds
    auto duration = tp.time_since_epoch();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto micros =
        std::chrono::duration_cast<std::chrono::microseconds>(duration - secs);
    c.microsecond = static_cast<int>(micros.count());

    return c;
}

std::string DateTimeComponents::formatExif() const {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << year << ':' << std::setw(2)
        << month << ':' << std::setw(2) << day << ' ' << std::setw(2) << hour
        << ':' << std::setw(2) << minute << ':' << std::setw(2) << second;
    return oss.str();
}

std::string DateTimeComponents::formatIso8601() const {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << year << '-' << std::setw(2)
        << month << '-' << std::setw(2) << day << 'T' << std::setw(2) << hour
        << ':' << std::setw(2) << minute << ':' << std::setw(2) << second;

    if (microsecond > 0) {
        oss << '.' << std::setw(6) << microsecond;
    }

    if (utcOffsetMinutes.has_value()) {
        int offset = *utcOffsetMinutes;
        if (offset == 0) {
            oss << 'Z';
        } else {
            char sign = offset >= 0 ? '+' : '-';
            offset = std::abs(offset);
            int h = offset / 60;
            int m = offset % 60;
            oss << sign << std::setw(2) << h << ':' << std::setw(2) << m;
        }
    }

    return oss.str();
}

}  // namespace atom::image::metadata
