/*
 * time_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_WEB_TIME_MANAGER_HPP
#define ATOM_WEB_TIME_MANAGER_HPP

#include <chrono>
#include <ctime>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>

namespace atom::web {

class TimeManagerImpl;

/**
 * @class TimeManager
 * @brief A class for managing system time and synchronization.
 *
 * This class provides a high-level interface for time management operations
 * including system time retrieval/setting, timezone management, RTC
 * synchronization, and NTP time fetching. It uses the PIMPL idiom for
 * implementation hiding.
 */
class TimeManager {
public:
    /**
     * @brief Constructs a TimeManager.
     *
     * Initializes the time manager with default implementation.
     *
     * @throws std::runtime_error if initialization fails
     */
    TimeManager();

    /**
     * @brief Destructor to release resources.
     */
    ~TimeManager();

    TimeManager(const TimeManager&) = delete;
    TimeManager& operator=(const TimeManager&) = delete;

    TimeManager(TimeManager&&) noexcept;
    TimeManager& operator=(TimeManager&&) noexcept;

    /**
     * @brief Gets the current system time.
     *
     * @return The current system time as std::time_t.
     * @throws std::system_error on system call failure
     */
    auto getSystemTime() -> std::time_t;

    /**
     * @brief Gets the current system time with higher precision.
     *
     * @return The current system time as std::chrono::system_clock::time_point.
     * @throws std::system_error on system call failure
     */
    auto getSystemTimePoint() -> std::chrono::system_clock::time_point;

    /**
     * @brief Sets the system time.
     *
     * @param year The year to set (1970-2038).
     * @param month The month to set (1-12).
     * @param day The day to set (1-31).
     * @param hour The hour to set (0-23).
     * @param minute The minute to set (0-59).
     * @param second The second to set (0-59).
     * @return std::error_code containing the error if any
     *
     * @note Requires administrative privileges on most systems
     */
    auto setSystemTime(int year, int month, int day, int hour, int minute,
                       int second) -> std::error_code;

    /**
     * @brief Sets the system timezone.
     *
     * @param timezone The timezone to set (e.g., "UTC", "America/New_York").
     * @return std::error_code containing the error if any
     *
     * @note Requires administrative privileges on most systems
     */
    auto setSystemTimezone(std::string_view timezone) -> std::error_code;

    /**
     * @brief Synchronizes the system time from the Real-Time Clock (RTC).
     *
     * @return std::error_code containing the error if any
     *
     * @note Requires administrative privileges on most systems
     */
    auto syncTimeFromRTC() -> std::error_code;

    /**
     * @brief Gets the Network Time Protocol (NTP) time from a specified
     * hostname.
     *
     * @param hostname The NTP server hostname (e.g., "pool.ntp.org").
     * @param timeout Timeout in milliseconds for the NTP request (default: 5
     * seconds).
     * @return The NTP time as std::optional<std::time_t>, empty on error.
     *
     * @note Results are cached to improve performance
     */
    auto getNtpTime(std::string_view hostname,
                    std::chrono::milliseconds timeout = std::chrono::seconds(5))
        -> std::optional<std::time_t>;

    /**
     * @brief Sets the implementation for testing purposes.
     *
     * @param impl The implementation to set.
     *
     * @note This method is primarily intended for unit testing
     */
    void setImpl(std::unique_ptr<TimeManagerImpl> impl);

    /**
     * @brief Checks if the current process has administrative/root privileges.
     *
     * @return true if the process has admin/root privileges, false otherwise
     */
    bool hasAdminPrivileges() const;

private:
    std::unique_ptr<TimeManagerImpl> impl_;
};

}  // namespace atom::web

#endif  // ATOM_WEB_TIME_MANAGER_HPP
