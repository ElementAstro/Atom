/*
 * time_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "time_utils.hpp"

#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace atom::web {
namespace time_utils {

bool validateDateTime(int year, int month, int day, int hour, int minute,
                      int second) {
    if (year < MIN_VALID_YEAR || year > MAX_VALID_YEAR)
        return false;
    if (month < MIN_VALID_MONTH || month > MAX_VALID_MONTH)
        return false;
    if (day < MIN_VALID_DAY || day > MAX_VALID_DAY)
        return false;
    if (hour < MIN_VALID_HOUR || hour > MAX_VALID_HOUR)
        return false;
    if (minute < MIN_VALID_MINUTE || minute > MAX_VALID_MINUTE)
        return false;
    if (second < MIN_VALID_SECOND || second > MAX_VALID_SECOND)
        return false;

    // 检查月份的天数
    int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // 闰年检查
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        daysInMonth[2] = 29;
    }

    return day <= daysInMonth[month];
}

bool validateHostname(std::string_view hostname) {
    // 简单验证，非空且长度合理
    if (hostname.empty() || hostname.length() > 255)
        return false;

    // 确保不包含可疑字符
    for (char c : hostname) {
        if (!(std::isalnum(c) || c == '.' || c == '-')) {
            return false;
        }
    }

    return true;
}

auto formatISO8601(std::time_t time) -> std::string {
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &time);
#else
    gmtime_r(&time, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

auto formatRFC2822(std::time_t time) -> std::string {
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &time);
#else
    gmtime_r(&time, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%a, %d %b %Y %H:%M:%S GMT");
    return oss.str();
}

auto parseISO8601(std::string_view str) -> std::optional<std::time_t> {
    if (str.empty())
        return std::nullopt;

    std::tm tm_buf{};
    std::string strCopy{str};
    std::istringstream iss{strCopy};
    iss >> std::get_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");

    if (iss.fail()) {
        // Try alternative format without T
        iss.clear();
        iss.str(strCopy);
        iss >> std::get_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        if (iss.fail()) {
            return std::nullopt;
        }
    }

    tm_buf.tm_isdst = 0;
#ifdef _WIN32
    return _mkgmtime(&tm_buf);
#else
    return timegm(&tm_buf);
#endif
}

auto parseRFC2822(std::string_view str) -> std::optional<std::time_t> {
    if (str.empty())
        return std::nullopt;

    std::tm tm_buf{};
    std::string strCopy{str};
    std::istringstream iss{strCopy};
    iss >> std::get_time(&tm_buf, "%a, %d %b %Y %H:%M:%S");

    if (iss.fail()) {
        return std::nullopt;
    }

    tm_buf.tm_isdst = 0;
#ifdef _WIN32
    return _mkgmtime(&tm_buf);
#else
    return timegm(&tm_buf);
#endif
}

auto getCurrentTimeFormatted(std::string_view format) -> std::string {
    auto now = std::time(nullptr);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, std::string(format).c_str());
    return oss.str();
}

auto timeDifference(std::time_t start,
                    std::time_t end) -> std::chrono::seconds {
    return std::chrono::seconds(end - start);
}

auto utcToLocal(std::time_t utcTime) -> std::time_t {
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &utcTime);
    return mktime(&tm_buf);
#else
    gmtime_r(&utcTime, &tm_buf);
    tm_buf.tm_isdst = -1;
    return mktime(&tm_buf);
#endif
}

auto localToUtc(std::time_t localTime) -> std::time_t {
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &localTime);
    return _mkgmtime(&tm_buf);
#else
    localtime_r(&localTime, &tm_buf);
    return timegm(&tm_buf);
#endif
}

}  // namespace time_utils
}  // namespace atom::web
