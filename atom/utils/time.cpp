/*
 * time.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-10-27

Description: Some useful functions about time

**************************************************/

#include "time.hpp"

#include <array>
#include <chrono>
#include <format>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <unordered_map>

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#endif

namespace atom::utils {
namespace {
// Thread-safe caching mechanism for time conversions
std::mutex timeCacheMutex;
std::unordered_map<std::string, std::string> timeConversionCache;
constexpr size_t MAX_CACHE_SIZE = 1000;

// Constants
constexpr int K_MILLISECONDS_IN_SECOND = 1000;
constexpr int K_CHINA_TIMEZONE_OFFSET = 8;
constexpr int K_SECONDS_PER_HOUR = 3600;
constexpr time_t K_MAX_TIMESTAMP = std::numeric_limits<time_t>::max();
constexpr size_t K_BUFFER_SIZE = 80;

thread_local std::array<char, K_BUFFER_SIZE> tls_buffer{};
thread_local std::tm tls_timeInfo{};

// Check if a timestamp is valid
bool isValidTimestamp(time_t timestamp) {
    return timestamp >= 0 && timestamp < K_MAX_TIMESTAMP;
}

// Safe conversion of time to tm structure
std::optional<std::tm> safeLocalTime(const time_t* time) {
    std::tm result{};
#ifdef _WIN32
    if (localtime_s(&result, time) != 0) {
        return std::nullopt;
    }
#else
    if (localtime_r(time, &result) == nullptr) {
        return std::nullopt;
    }
#endif
    return result;
}

// Safe conversion of time to UTC tm structure
std::optional<std::tm> safeGmTime(const time_t* time) {
    std::tm result{};
#ifdef _WIN32
    if (gmtime_s(&result, time) != 0) {
        return std::nullopt;
    }
#else
    if (gmtime_r(time, &result) == nullptr) {
        return std::nullopt;
    }
#endif
    return result;
}

}  // anonymous namespace

bool validateTimestampFormat(std::string_view timestampStr,
                             std::string_view format) {
    std::tm tm{};
    // 修复函数声明错误
    std::istringstream ss((std::string(timestampStr)));
    ss >> std::get_time(&tm, format.data());
    return !ss.fail() && ss.eof();
}

auto getTimestampString() -> std::string {
    try {
        // Use chrono literals for readability
        using namespace std::chrono;

        auto now = system_clock::now();
        auto time = system_clock::to_time_t(now);
        // 修复变量自引用初始化问题
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) %
                  K_MILLISECONDS_IN_SECOND;

        auto timeInfoOpt = safeLocalTime(&time);
        if (!timeInfoOpt) {
            THROW_TIME_CONVERT_ERROR("Failed to convert time to local time");
        }

        auto& timeInfo = *timeInfoOpt;

        // Use C++20 std::format if available
#if __cpp_lib_format >= 202106L
        std::stringstream timestampStream;
        timestampStream << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S") << '.'
                        << std::setfill('0') << std::setw(3) << ms.count();
        return timestampStream.str();
#else
        std::stringstream timestampStream;
        timestampStream << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S") << '.'
                        << std::setfill('0') << std::setw(3) << ms.count();
        return timestampStream.str();
#endif
    } catch (const std::exception& e) {
        THROW_TIME_CONVERT_ERROR(std::string("Error generating timestamp: ") +
                                 e.what());
    }
}

auto convertToChinaTime(std::string_view utcTimeStr) -> std::string {
    // Input validation
    if (utcTimeStr.empty()) {
        THROW_TIME_CONVERT_ERROR("Empty UTC time string provided");
    }

    if (!validateTimestampFormat(utcTimeStr)) {
        THROW_TIME_CONVERT_ERROR("Invalid UTC time string format: " +
                                 std::string(utcTimeStr));
    }

    // Check cache first
    {
        std::unique_lock<std::mutex> lock(timeCacheMutex);
        auto it = timeConversionCache.find(std::string(utcTimeStr));
        if (it != timeConversionCache.end()) {
            return it->second;
        }
    }

    try {
        // Parse UTC time string
        std::tm timeStruct = {};
        // 修复函数声明错误
        std::istringstream inputStream((std::string(utcTimeStr)));
        inputStream >> std::get_time(&timeStruct, "%Y-%m-%d %H:%M:%S");
        if (inputStream.fail()) {
            THROW_TIME_CONVERT_ERROR("Failed to parse UTC time string: " +
                                     std::string(utcTimeStr));
        }

#ifdef ATOM_USE_BOOST
        boost::posix_time::ptime utc_ptime =
            boost::posix_time::from_tm(timeStruct);
        boost::posix_time::ptime china_ptime =
            utc_ptime + boost::posix_time::hours(K_CHINA_TIMEZONE_OFFSET);
        std::stringstream outputStream;
        outputStream << boost::posix_time::to_simple_string(china_ptime);
        auto result = outputStream.str();
#else
        // Convert to time_point
        using namespace std::chrono;
        auto timePoint = system_clock::from_time_t(std::mktime(&timeStruct));
        if (timePoint == system_clock::time_point{}) {
            THROW_TIME_CONVERT_ERROR("Invalid time conversion for: " +
                                     std::string(utcTimeStr));
        }

        hours offset(K_CHINA_TIMEZONE_OFFSET);
        auto localTimePoint = timePoint + offset;

        // Format as string
        auto localTime = system_clock::to_time_t(localTimePoint);
        auto localTimeStructOpt = safeLocalTime(&localTime);
        if (!localTimeStructOpt) {
            THROW_TIME_CONVERT_ERROR("Failed to convert time to local time");
        }

        auto& localTimeStruct = *localTimeStructOpt;

#if __cpp_lib_format >= 202106L
        // 使用 put_time 替代 std::format
        std::stringstream outputStream;
        outputStream << std::put_time(&localTimeStruct, "%Y-%m-%d %H:%M:%S");
        auto result = outputStream.str();
#else
        std::stringstream outputStream;
        outputStream << std::put_time(&localTimeStruct, "%Y-%m-%d %H:%M:%S");
        auto result = outputStream.str();
#endif
#endif

        // Update cache
        {
            std::unique_lock<std::mutex> lock(timeCacheMutex);
            if (timeConversionCache.size() >= MAX_CACHE_SIZE) {
                timeConversionCache.clear();  // Simple cache eviction policy
            }
            timeConversionCache[std::string(utcTimeStr)] = result;
        }

        return result;
    } catch (const TimeConvertException&) {
        throw;  // Re-throw our own exceptions
    } catch (const std::exception& e) {
        THROW_TIME_CONVERT_ERROR(
            std::string("Error converting to China time: ") + e.what());
    }
}

auto getChinaTimestampString() -> std::string {
    try {
        // Get current time point
        using namespace std::chrono;
        auto now = system_clock::now();

#ifdef ATOM_USE_BOOST
        boost::posix_time::ptime utc_ptime =
            boost::posix_time::from_time_t(system_clock::to_time_t(now));
        boost::posix_time::ptime china_ptime =
            utc_ptime + boost::posix_time::hours(K_CHINA_TIMEZONE_OFFSET);
        std::stringstream timestampStream;
        timestampStream << boost::posix_time::to_simple_string(china_ptime);
        return timestampStream.str();
#else
        // Convert to China time (UTC+8)
        hours offset(K_CHINA_TIMEZONE_OFFSET);
        auto localTimePoint = now + offset;

        // Format as string
        auto localTime = system_clock::to_time_t(localTimePoint);
        auto localTimeStructOpt = safeLocalTime(&localTime);
        if (!localTimeStructOpt) {
            THROW_TIME_CONVERT_ERROR("Failed to convert time to local time");
        }

        auto& localTimeStruct = *localTimeStructOpt;

#if __cpp_lib_format >= 202106L
        // 使用 put_time 替代 std::format
        std::stringstream timestampStream;
        timestampStream << std::put_time(&localTimeStruct, "%Y-%m-%d %H:%M:%S");
        return timestampStream.str();
#else
        std::stringstream timestampStream;
        timestampStream << std::put_time(&localTimeStruct, "%Y-%m-%d %H:%M:%S");
        return timestampStream.str();
#endif
#endif
    } catch (const TimeConvertException&) {
        throw;  // Re-throw our own exceptions
    } catch (const std::exception& e) {
        THROW_TIME_CONVERT_ERROR(
            std::string("Error getting China timestamp: ") + e.what());
    }
}

auto timeStampToString(time_t timestamp,
                       std::string_view format) -> std::string {
    // Input validation
    if (!isValidTimestamp(timestamp)) {
        THROW_TIME_CONVERT_ERROR("Invalid timestamp value: " +
                                 std::to_string(timestamp));
    }

    if (format.empty()) {
        THROW_TIME_CONVERT_ERROR("Empty format string provided");
    }

    try {
        auto timeStructOpt = safeLocalTime(&timestamp);
        if (!timeStructOpt) {
            THROW_TIME_CONVERT_ERROR(
                "Failed to convert timestamp to local time");
        }
        auto& timeStruct = *timeStructOpt;

#ifdef ATOM_USE_BOOST
        boost::posix_time::ptime pt = boost::posix_time::from_tm(timeStruct);
        std::stringstream ss;
        ss << boost::posix_time::to_simple_string(pt);
        return ss.str();
#else
        // C++20 format approach if available
#if __cpp_lib_format >= 202106L
        try {
            // 使用 strftime 替代 std::format
            if (std::strftime(tls_buffer.data(), tls_buffer.size(),
                              format.data(), &timeStruct) == 0) {
                THROW_TIME_CONVERT_ERROR("strftime failed with format: " +
                                         std::string(format));
            }
            return std::string(tls_buffer.data());
        } catch (const std::exception& e) {
            THROW_TIME_CONVERT_ERROR(std::string("Format error: ") + e.what());
        }
#else
        if (std::strftime(tls_buffer.data(), tls_buffer.size(), format.data(),
                          &timeStruct) == 0) {
            THROW_TIME_CONVERT_ERROR("strftime failed with format: " +
                                     std::string(format));
        }
        return std::string(tls_buffer.data());
#endif
#endif
    } catch (const TimeConvertException&) {
        throw;  // Re-throw our own exceptions
    } catch (const std::exception& e) {
        THROW_TIME_CONVERT_ERROR(
            std::string("Error converting timestamp to string: ") + e.what());
    }
}

// Specially for Astrometry.net
auto toString(const std::tm& tm, std::string_view format) -> std::string {
    if (format.empty()) {
        THROW_TIME_CONVERT_ERROR("Empty format string provided");
    }

    try {
#ifdef ATOM_USE_BOOST
        std::stringstream oss;
        oss << boost::posix_time::ptime(
            boost::gregorian::date(tm.tm_year + 1900, tm.tm_mon + 1,
                                   tm.tm_mday),
            boost::posix_time::hours(tm.tm_hour) +
                boost::posix_time::minutes(tm.tm_min) +
                boost::posix_time::seconds(tm.tm_sec));
        return oss.str();
#else
        // C++20 format approach if available
#if __cpp_lib_format >= 202106L
        try {
            // 使用 strftime 替代 std::format
            if (std::strftime(tls_buffer.data(), tls_buffer.size(),
                              format.data(), &tm) == 0) {
                THROW_TIME_CONVERT_ERROR("strftime failed with format: " +
                                         std::string(format));
            }
            return std::string(tls_buffer.data());
        } catch (const std::exception& e) {
            THROW_TIME_CONVERT_ERROR(std::string("Format error: ") + e.what());
        }
#else
        std::ostringstream oss;
        oss << std::put_time(&tm, format.data());
        if (oss.fail()) {
            THROW_TIME_CONVERT_ERROR("Failed to format time with format: " +
                                     std::string(format));
        }
        return oss.str();
#endif
#endif
    } catch (const TimeConvertException&) {
        throw;  // Re-throw our own exceptions
    } catch (const std::exception& e) {
        THROW_TIME_CONVERT_ERROR(
            std::string("Error converting tm to string: ") + e.what());
    }
}

auto getUtcTime() -> std::string {
    try {
        using namespace std::chrono;
        const auto NOW = system_clock::now();
        const std::time_t NOW_TIME_T = system_clock::to_time_t(NOW);

        auto utcTimeOpt = safeGmTime(&NOW_TIME_T);
        if (!utcTimeOpt) {
            THROW_TIME_CONVERT_ERROR("Failed to convert time to UTC");
        }
        auto& utcTime = *utcTimeOpt;

#ifdef ATOM_USE_BOOST
        boost::posix_time::ptime pt = boost::posix_time::from_tm(utcTime);
        std::stringstream ss;
        ss << boost::posix_time::to_iso_extended_string(pt) << "Z";
        return ss.str();
#else
        // C++20 format approach if available
#if __cpp_lib_format >= 202106L
        // 使用 strftime 替代 std::format
        if (std::strftime(tls_buffer.data(), tls_buffer.size(), "%FT%TZ",
                          &utcTime) == 0) {
            THROW_TIME_CONVERT_ERROR("strftime failed with format %FT%TZ");
        }
        return std::string(tls_buffer.data());
#else
        return toString(utcTime, "%FT%TZ");
#endif
#endif
    } catch (const TimeConvertException&) {
        throw;  // Re-throw our own exceptions
    } catch (const std::exception& e) {
        THROW_TIME_CONVERT_ERROR(std::string("Error getting UTC time: ") +
                                 e.what());
    }
}

auto timestampToTime(long long timestamp) -> std::optional<std::tm> {
    // Input validation
    if (timestamp < 0) {
        return std::nullopt;
    }

    try {
        auto time =
            static_cast<std::time_t>(timestamp / K_MILLISECONDS_IN_SECOND);

        if (!isValidTimestamp(time)) {
            return std::nullopt;
        }

        return safeLocalTime(&time);
    } catch (const std::exception&) {
        // Return nullopt instead of throwing for this function
        return std::nullopt;
    }
}

}  // namespace atom::utils