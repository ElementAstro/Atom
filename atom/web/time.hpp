/*
 * time.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Time

**************************************************/

#ifndef ATOM_WEB_TIME_HPP
#define ATOM_WEB_TIME_HPP

#include <chrono>
#include <ctime>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>

namespace atom::web {

/**
 * @enum TimeError
 * @brief Error codes for time operations
 */
enum class TimeError {
    None,
    InvalidParameter,
    PermissionDenied,
    NetworkError,
    SystemError,
    TimeoutError,
    NotSupported
};

/**
 * @class TimeManagerImpl
 * @brief Forward declaration of the implementation class for TimeManager.
 */
class TimeManagerImpl;

/**
 * @class TimeManager
 * @brief A class for managing system time and synchronization.
 */
class TimeManager {
public:
    /**
     * @brief Constructs a TimeManager.
     */
    TimeManager();

    /**
     * @brief Destructor to release resources.
     */
    ~TimeManager();

    // Delete copy constructor and assignment operator
    TimeManager(const TimeManager&) = delete;
    TimeManager& operator=(const TimeManager&) = delete;

    // Allow move semantics
    TimeManager(TimeManager&&) noexcept;
    TimeManager& operator=(TimeManager&&) noexcept;

    /**
     * @brief Gets the current system time.
     * @return The current system time as std::time_t.
     * @throw std::system_error on system call failure
     */
    auto getSystemTime() -> std::time_t;

    /**
     * @brief Gets the current system time with higher precision.
     * @return The current system time as std::chrono::system_clock::time_point.
     * @throw std::system_error on system call failure
     */
    auto getSystemTimePoint() -> std::chrono::system_clock::time_point;

    /**
     * @brief Sets the system time.
     * @param year The year to set (1970-2038).
     * @param month The month to set (1-12).
     * @param day The day to set (1-31).
     * @param hour The hour to set (0-23).
     * @param minute The minute to set (0-59).
     * @param second The second to set (0-59).
     * @return std::error_code containing the error if any
     */
    auto setSystemTime(int year, int month, int day, int hour, int minute,
                       int second) -> std::error_code;

    /**
     * @brief Sets the system timezone.
     * @param timezone The timezone to set (e.g., "UTC", "PST").
     * @return std::error_code containing the error if any
     */
    auto setSystemTimezone(std::string_view timezone) -> std::error_code;

    /**
     * @brief Synchronizes the system time from the Real-Time Clock (RTC).
     * @return std::error_code containing the error if any
     */
    auto syncTimeFromRTC() -> std::error_code;

    /**
     * @brief Gets the Network Time Protocol (NTP) time from a specified
     * hostname with a specified timeout.
     * @param hostname The NTP server hostname.
     * @param timeout_ms Timeout in milliseconds for the NTP request.
     * @return The NTP time as std::optional<std::time_t>, empty on error.
     */
    auto getNtpTime(std::string_view hostname,
                    std::chrono::milliseconds timeout = std::chrono::seconds(5))
        -> std::optional<std::time_t>;

    /**
     * @brief Sets the implementation for testing purposes.
     * @param impl The implementation to set.
     */
    void setImpl(std::unique_ptr<TimeManagerImpl> impl);

private:
    std::unique_ptr<TimeManagerImpl> impl_;  ///< Pointer to the implementation
};

}  // namespace atom::web

#endif  // ATOM_WEB_TIME_HPP
