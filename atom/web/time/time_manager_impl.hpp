/*
 * time_manager_impl.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_WEB_TIME_MANAGER_IMPL_HPP
#define ATOM_WEB_TIME_MANAGER_IMPL_HPP

#include <chrono>
#include <ctime>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <winreg.h>
#include <ws2tcpip.h>
// clang-format on
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace atom::web {

/**
 * @brief Implementation of time management functionality
 *
 * This class provides system time management, timezone handling,
 * and NTP synchronization capabilities with thread-safe operations.
 */
class TimeManagerImpl {
public:
    TimeManagerImpl();
    ~TimeManagerImpl() = default;

    /**
     * @brief Get current system time as time_t
     *
     * @return std::time_t Current system time
     * @throws std::system_error if system call fails
     */
    auto getSystemTime() -> std::time_t;

    /**
     * @brief Get current system time as time_point
     *
     * @return std::chrono::system_clock::time_point Current system time point
     * @throws std::system_error if system call fails
     */
    auto getSystemTimePoint() -> std::chrono::system_clock::time_point;

    /**
     * @brief Set system time
     *
     * @param year Year (e.g., 2024)
     * @param month Month (1-12)
     * @param day Day (1-31)
     * @param hour Hour (0-23)
     * @param minute Minute (0-59)
     * @param second Second (0-59)
     * @return std::error_code Error status
     */
    auto setSystemTime(int year, int month, int day, int hour, int minute,
                       int second) -> std::error_code;

    /**
     * @brief Set system timezone
     *
     * @param timezone Timezone identifier (e.g., "UTC", "America/New_York")
     * @return std::error_code Error status
     */
    auto setSystemTimezone(std::string_view timezone) -> std::error_code;

    /**
     * @brief Synchronize system time from RTC (Real-Time Clock)
     *
     * @return std::error_code Error status
     */
    auto syncTimeFromRTC() -> std::error_code;

    /**
     * @brief Get time from NTP server
     *
     * @param hostname NTP server hostname
     * @param timeout Request timeout
     * @return std::optional<std::time_t> NTP time if successful, nullopt
     * otherwise
     */
    auto getNtpTime(std::string_view hostname,
                    std::chrono::milliseconds timeout)
        -> std::optional<std::time_t>;

private:
    /**
     * @brief RAII wrapper for socket operations
     */
    class SocketHandler {
    public:
        SocketHandler();
        ~SocketHandler();

        bool isValid() const;

#ifdef _WIN32
        SOCKET getFd() const;
#else
        int getFd() const;
#endif

        SocketHandler(const SocketHandler&) = delete;
        SocketHandler& operator=(const SocketHandler&) = delete;
        SocketHandler(SocketHandler&&) = delete;
        SocketHandler& operator=(SocketHandler&&) = delete;

    private:
#ifdef _WIN32
        SOCKET fd_;
        bool wsa_initialized_;
#else
        int fd_;
#endif
    };

#ifdef _WIN32
    auto getTimeZoneInformationByName(const std::string& timezone, DWORD* tzId)
        -> bool;
#endif

    void updateTimeCache();

    mutable std::shared_mutex mutex_;
    std::chrono::minutes cache_ttl_;
    std::chrono::system_clock::time_point last_update_;
    std::time_t cached_time_{0};
    std::chrono::system_clock::time_point last_ntp_query_;
    std::time_t cached_ntp_time_{0};
    std::string last_ntp_server_;
};

}  // namespace atom::web

#endif  // ATOM_WEB_TIME_MANAGER_IMPL_HPP
