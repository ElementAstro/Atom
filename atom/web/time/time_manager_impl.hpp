/*
 * time_manager_impl.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Time Manager Implementation

**************************************************/

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

class TimeManagerImpl {
public:
    TimeManagerImpl();
    ~TimeManagerImpl() = default;

    auto getSystemTime() -> std::time_t;
    auto getSystemTimePoint() -> std::chrono::system_clock::time_point;

    auto setSystemTime(int year, int month, int day, int hour, int minute,
                       int second) -> std::error_code;
    auto setSystemTimezone(std::string_view timezone) -> std::error_code;
    auto syncTimeFromRTC() -> std::error_code;

    auto getNtpTime(std::string_view hostname,
                    std::chrono::milliseconds timeout)
        -> std::optional<std::time_t>;

private:
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

    private:
#ifdef _WIN32
        SOCKET fd_;
#else
        int fd_;
#endif
    };

#ifdef _WIN32
    auto getTimeZoneInformationByName(const std::string& timezone, DWORD* tzId)
        -> bool;
#endif

    void updateTimeCache();

    std::shared_mutex mutex_;

    std::chrono::minutes cache_ttl_;
    std::chrono::system_clock::time_point last_update_;
    std::time_t cached_time_{0};

    std::chrono::system_clock::time_point last_ntp_query_;
    std::time_t cached_ntp_time_{0};
    std::string last_ntp_server_;
};

}  // namespace atom::web

#endif  // ATOM_WEB_TIME_MANAGER_IMPL_HPP
