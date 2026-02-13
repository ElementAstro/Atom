/*
 * cron_types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef CRON_TYPES_HPP
#define CRON_TYPES_HPP

#include <string>

/**
 * @brief Timezone information for scheduling and validation
 *
 * Unified timezone type used across the cron system for scheduling,
 * validation, and time conversion operations.
 */
struct TimezoneInfo {
    std::string timezone_id;
    int utc_offset_minutes;
    bool is_dst_aware;

    TimezoneInfo(std::string tz = "UTC", int offset = 0, bool dst = false)
        : timezone_id(std::move(tz)), utc_offset_minutes(offset),
          is_dst_aware(dst) {}
};

#endif  // CRON_TYPES_HPP
