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
#include <iomanip>
#include <sstream>

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#endif

namespace atom::utils {
constexpr int K_MILLISECONDS_IN_SECOND =
    1000;  // Named constant for magic number
constexpr int K_CHINA_TIMEZONE_OFFSET = 8;

auto getTimestampString() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()) %
                        K_MILLISECONDS_IN_SECOND;

    std::tm timeInfo{};
#ifdef _WIN32
    if (localtime_s(&timeInfo, &time) != 0) {
#else
    if (localtime_r(&time, &timeInfo) == nullptr) {
#endif
        THROW_TIME_CONVERT_ERROR("Failed to convert time to local time");
    }

    std::stringstream timestampStream;
    timestampStream << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S") << '.'
                    << std::setfill('0') << std::setw(3)
                    << milliseconds.count();

    return timestampStream.str();
}

auto convertToChinaTime(const std::string &utcTimeStr) -> std::string {
    // Parse UTC time string
    std::tm timeStruct = {};
    std::istringstream inputStream(utcTimeStr);
    inputStream >> std::get_time(&timeStruct, "%Y-%m-%d %H:%M:%S");

#ifdef ATOM_USE_BOOST
    boost::posix_time::ptime utc_ptime = boost::posix_time::from_tm(timeStruct);
    boost::posix_time::ptime china_ptime =
        utc_ptime + boost::posix_time::hours(K_CHINA_TIMEZONE_OFFSET);
    std::stringstream outputStream;
    outputStream << boost::posix_time::to_simple_string(china_ptime);
    return outputStream.str();
#else
    // Convert to time_point
    auto timePoint =
        std::chrono::system_clock::from_time_t(std::mktime(&timeStruct));

    std::chrono::hours offset(K_CHINA_TIMEZONE_OFFSET);
    auto localTimePoint = timePoint + offset;

    // Format as string
    auto localTime = std::chrono::system_clock::to_time_t(localTimePoint);
    std::tm localTimeStruct{};
#ifdef _WIN32
    if (localtime_s(&localTimeStruct, &localTime) != 0) {
#else
    if (localtime_r(&localTime, &localTimeStruct) == nullptr) {
#endif
        THROW_TIME_CONVERT_ERROR("Failed to convert time to local time");
    }

    std::stringstream outputStream;
    outputStream << std::put_time(&localTimeStruct, "%Y-%m-%d %H:%M:%S");

    return outputStream.str();
#endif
}

auto getChinaTimestampString() -> std::string {
    // Get current time point
    auto now = std::chrono::system_clock::now();

#ifdef ATOM_USE_BOOST
    boost::posix_time::ptime utc_ptime = boost::posix_time::from_time_t(
        std::chrono::system_clock::to_time_t(now));
    boost::posix_time::ptime china_ptime =
        utc_ptime + boost::posix_time::hours(K_CHINA_TIMEZONE_OFFSET);
    std::stringstream timestampStream;
    timestampStream << boost::posix_time::to_simple_string(china_ptime);
    return timestampStream.str();
#else
    // Convert to China time
    std::chrono::hours offset(K_CHINA_TIMEZONE_OFFSET);
    auto localTimePoint = now + offset;

    // Format as string
    auto localTime = std::chrono::system_clock::to_time_t(localTimePoint);
    std::tm localTimeStruct{};
#ifdef _WIN32
    if (localtime_s(&localTimeStruct, &localTime) != 0) {
#else
    if (localtime_r(&localTime, &localTimeStruct) == nullptr) {
#endif
        THROW_TIME_CONVERT_ERROR("Failed to convert time to local time");
    }

    std::stringstream timestampStream;
    timestampStream << std::put_time(&localTimeStruct, "%Y-%m-%d %H:%M:%S");

    return timestampStream.str();
#endif
}

auto timeStampToString(time_t timestamp) -> std::string {
    constexpr size_t K_BUFFER_SIZE = 80;  // Named constant for magic number
    std::array<char, K_BUFFER_SIZE> buffer{};
    std::tm timeStruct{};
#ifdef _WIN32
    if (localtime_s(&timeStruct, &timestamp) != 0) {
#else
    if (localtime_r(&timestamp, &timeStruct) == nullptr) {
#endif
        THROW_TIME_CONVERT_ERROR("Failed to convert timestamp to local time");
    }

#ifdef ATOM_USE_BOOST
    boost::posix_time::ptime pt = boost::posix_time::from_tm(timeStruct);
    std::stringstream ss;
    ss << boost::posix_time::to_simple_string(pt);
    return ss.str();
#else
    if (std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d %H:%M:%S",
                      &timeStruct) == 0) {
        THROW_TIME_CONVERT_ERROR("strftime failed");
    }

    return std::string(buffer.data());
#endif
}

// Specially for Astrometry.net
auto toString(const std::tm &tm, const std::string &format) -> std::string {
#ifdef ATOM_USE_BOOST
    std::stringstream oss;
    oss << boost::posix_time::ptime(
        boost::gregorian::date(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday),
        boost::posix_time::hours(tm.tm_hour) +
            boost::posix_time::minutes(tm.tm_min) +
            boost::posix_time::seconds(tm.tm_sec));
    return oss.str();
#else
    std::ostringstream oss;
    oss << std::put_time(&tm, format.c_str());
    return oss.str();
#endif
}

auto getUtcTime() -> std::string {
    const auto NOW = std::chrono::system_clock::now();
    const std::time_t NOW_TIME_T = std::chrono::system_clock::to_time_t(NOW);
    std::tm utcTime{};
#ifdef _WIN32
    if (gmtime_s(&utcTime, &NOW_TIME_T) != 0) {
        THROW_TIME_CONVERT_ERROR("Failed to convert time to UTC");
    }
#else
    if (gmtime_r(&NOW_TIME_T, &utcTime) == nullptr) {
        THROW_TIME_CONVERT_ERROR("Failed to convert time to UTC");
    }
#endif

#ifdef ATOM_USE_BOOST
    boost::posix_time::ptime pt = boost::posix_time::from_tm(utcTime);
    std::stringstream ss;
    ss << boost::posix_time::to_iso_extended_string(pt) << "Z";
    return ss.str();
#else
    return toString(utcTime, "%FT%TZ");
#endif
}

auto timestampToTime(long long timestamp) -> std::tm {
    auto time = static_cast<std::time_t>(timestamp / K_MILLISECONDS_IN_SECOND);

    std::tm timeStruct{};
#ifdef _WIN32
    if (localtime_s(&timeStruct, &time) != 0) {
#else
    if (localtime_r(&time, &timeStruct) == nullptr) {
#endif
        THROW_TIME_CONVERT_ERROR("Failed to convert timestamp to local time");
    }  // Use localtime_s for thread safety

    return timeStruct;
}

}  // namespace atom::utils