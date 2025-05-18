/*
 * time_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "time_utils.hpp"

#include <cctype>

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

}  // namespace time_utils
}  // namespace atom::web
