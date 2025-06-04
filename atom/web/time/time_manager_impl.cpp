/*
 * time_manager_impl.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "time_manager_impl.hpp"
#include "time_utils.hpp"

#include <array>
#include <ctime>
#include <mutex>
#include <shared_mutex>

#if __linux__
#include <cmath>
#include <fstream>
#include <future>
#include <sstream>
#endif

#include <spdlog/spdlog.h>
#include "atom/system/user.hpp"
#include "atom/web/time/time_error.hpp"

namespace atom::web {

TimeManagerImpl::TimeManagerImpl() : cache_ttl_(std::chrono::minutes(5)) {
    try {
        updateTimeCache();
        spdlog::debug("TimeManagerImpl initialized successfully");
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize time cache: {}", e.what());
    }
}

auto TimeManagerImpl::getSystemTime() -> std::time_t {
    try {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        auto systemTime = std::chrono::system_clock::to_time_t(now);
        spdlog::trace("Retrieved system time: {}", systemTime);
        return systemTime;
    } catch (const std::exception& e) {
        spdlog::error("Error in getSystemTime: {}", e.what());
        throw std::system_error(
            std::error_code(EFAULT, std::system_category()),
            "Failed to get system time: " + std::string(e.what()));
    }
}

auto TimeManagerImpl::getSystemTimePoint()
    -> std::chrono::system_clock::time_point {
    try {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        spdlog::trace("Retrieved system time point");
        return now;
    } catch (const std::exception& e) {
        spdlog::error("Error in getSystemTimePoint: {}", e.what());
        throw std::system_error(
            std::error_code(EFAULT, std::system_category()),
            "Failed to get system time point: " + std::string(e.what()));
    }
}

TimeManagerImpl::SocketHandler::SocketHandler() {
#ifdef _WIN32
    wsa_initialized_ = false;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        spdlog::error("Failed to initialize Winsock: {}", WSAGetLastError());
        fd_ = INVALID_SOCKET;
        return;
    }
    wsa_initialized_ = true;

    fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd_ == INVALID_SOCKET) {
        spdlog::error("Failed to create socket: {}", WSAGetLastError());
    }
#else
    fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd_ < 0) {
        spdlog::error("Failed to create socket: {}", strerror(errno));
    }
#endif
}

TimeManagerImpl::SocketHandler::~SocketHandler() {
    if (isValid()) {
#ifdef _WIN32
        closesocket(fd_);
        if (wsa_initialized_) {
            WSACleanup();
        }
#else
        close(fd_);
#endif
    }
}

bool TimeManagerImpl::SocketHandler::isValid() const {
#ifdef _WIN32
    return fd_ != INVALID_SOCKET;
#else
    return fd_ >= 0;
#endif
}

#ifdef _WIN32
SOCKET TimeManagerImpl::SocketHandler::getFd() const { return fd_; }
#else
int TimeManagerImpl::SocketHandler::getFd() const { return fd_; }
#endif

#ifdef _WIN32
auto TimeManagerImpl::setSystemTime(int year, int month, int day, int hour,
                                    int minute, int second) -> std::error_code {
    spdlog::debug(
        "Setting system time to: {}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", year,
        month, day, hour, minute, second);

    try {
        if (!time_utils::validateDateTime(year, month, day, hour, minute,
                                          second)) {
            spdlog::error("Invalid date/time parameters");
            return make_error_code(TimeError::InvalidParameter);
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (!atom::system::isRoot()) {
            spdlog::error("Insufficient permissions to set system time");
            return make_error_code(TimeError::PermissionDenied);
        }

        SYSTEMTIME sysTime{};
        sysTime.wYear = static_cast<WORD>(year);
        sysTime.wMonth = static_cast<WORD>(month);
        sysTime.wDay = static_cast<WORD>(day);
        sysTime.wHour = static_cast<WORD>(hour);
        sysTime.wMinute = static_cast<WORD>(minute);
        sysTime.wSecond = static_cast<WORD>(second);
        sysTime.wMilliseconds = 0;

        if (SetSystemTime(&sysTime) == 0) {
            DWORD error = GetLastError();
            spdlog::error("Failed to set system time: error code {}", error);
            return std::error_code(error, std::system_category());
        }

        updateTimeCache();
        spdlog::info("System time successfully set");
        return make_error_code(TimeError::None);

    } catch (const std::exception& e) {
        spdlog::error("Exception in setSystemTime: {}", e.what());
        return make_error_code(TimeError::SystemError);
    }
}

auto TimeManagerImpl::setSystemTimezone(std::string_view timezone)
    -> std::error_code {
    spdlog::debug("Setting system timezone to: {}", timezone);

    try {
        if (timezone.empty() || timezone.length() > 64) {
            spdlog::error("Invalid timezone parameter");
            return make_error_code(TimeError::InvalidParameter);
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (!atom::system::isRoot()) {
            spdlog::error("Insufficient permissions to set system timezone");
            return make_error_code(TimeError::PermissionDenied);
        }

        std::string timezoneStr(timezone);
        DWORD tzId;
        if (!getTimeZoneInformationByName(timezoneStr, &tzId)) {
            spdlog::error("Failed to find timezone information for {}",
                          timezoneStr);
            return make_error_code(TimeError::InvalidParameter);
        }

        TIME_ZONE_INFORMATION tzInfo;
        if (GetTimeZoneInformation(&tzInfo) == TIME_ZONE_ID_INVALID) {
            DWORD error = GetLastError();
            spdlog::error(
                "Failed to get current timezone information: error code {}",
                error);
            return std::error_code(error, std::system_category());
        }

        tzInfo.StandardBias = -static_cast<int>(tzId);

        if (SetTimeZoneInformation(&tzInfo) == 0) {
            DWORD error = GetLastError();
            spdlog::error("Failed to set timezone: error code {}", error);
            return std::error_code(error, std::system_category());
        }

        spdlog::info("Timezone successfully set to {}", timezoneStr);
        return make_error_code(TimeError::None);

    } catch (const std::exception& e) {
        spdlog::error("Exception in setSystemTimezone: {}", e.what());
        return make_error_code(TimeError::SystemError);
    }
}

auto TimeManagerImpl::syncTimeFromRTC() -> std::error_code {
    spdlog::debug("Synchronizing time from RTC");

    try {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (!atom::system::isRoot()) {
            spdlog::error("Insufficient permissions to sync from RTC");
            return make_error_code(TimeError::PermissionDenied);
        }

        SYSTEMTIME rtcTime;
        GetSystemTime(&rtcTime);

        if (SetSystemTime(&rtcTime) == 0) {
            DWORD error = GetLastError();
            spdlog::error("Failed to set system time from RTC: error code {}",
                          error);
            return std::error_code(error, std::system_category());
        }

        updateTimeCache();
        spdlog::info("System time successfully synchronized from RTC");
        return make_error_code(TimeError::None);

    } catch (const std::exception& e) {
        spdlog::error("Exception in syncTimeFromRTC: {}", e.what());
        return make_error_code(TimeError::SystemError);
    }
}

auto TimeManagerImpl::getTimeZoneInformationByName(const std::string& timezone,
                                                   DWORD* tzId) -> bool {
    constexpr struct {
        const char* name;
        int offset;
    } timezone_table[] = {{"UTC", 0},
                          {"EST", 300},
                          {"America/New_York", 300},
                          {"PST", 480},
                          {"America/Los_Angeles", 480},
                          {"CST", -480},
                          {"Asia/Shanghai", -480}};

    for (const auto& tz : timezone_table) {
        if (timezone == tz.name) {
            *tzId = tz.offset;
            return true;
        }
    }

    spdlog::error("Timezone not found: {}", timezone);
    return false;
}

#else
auto TimeManagerImpl::setSystemTime(int year, int month, int day, int hour,
                                    int minute, int second) -> std::error_code {
    spdlog::debug(
        "Setting system time to: {}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", year,
        month, day, hour, minute, second);

    try {
        if (!time_utils::validateDateTime(year, month, day, hour, minute,
                                          second)) {
            spdlog::error("Invalid date/time parameters");
            return make_error_code(TimeError::InvalidParameter);
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (!atom::system::isRoot()) {
            spdlog::error("Insufficient permissions to set system time");
            return make_error_code(TimeError::PermissionDenied);
        }

        struct tm timeinfo = {};
        timeinfo.tm_year = year - 1900;
        timeinfo.tm_mon = month - 1;
        timeinfo.tm_mday = day;
        timeinfo.tm_hour = hour;
        timeinfo.tm_min = minute;
        timeinfo.tm_sec = second;
        timeinfo.tm_isdst = -1;

        time_t rawtime = mktime(&timeinfo);
        if (rawtime == -1) {
            spdlog::error("Failed to convert time");
            return make_error_code(TimeError::SystemError);
        }

        struct timeval tv;
        tv.tv_sec = rawtime;
        tv.tv_usec = 0;

        if (settimeofday(&tv, nullptr) != 0) {
            spdlog::error("Failed to set system time: {}", strerror(errno));
            return std::error_code(errno, std::system_category());
        }

        updateTimeCache();
        spdlog::info("System time successfully set");
        return make_error_code(TimeError::None);

    } catch (const std::exception& e) {
        spdlog::error("Exception in setSystemTime: {}", e.what());
        return make_error_code(TimeError::SystemError);
    }
}

auto TimeManagerImpl::setSystemTimezone(std::string_view timezone)
    -> std::error_code {
    spdlog::debug("Setting system timezone to: {}", timezone);

    try {
        if (timezone.empty() || timezone.length() > 64) {
            spdlog::error("Invalid timezone parameter");
            return make_error_code(TimeError::InvalidParameter);
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (!atom::system::isRoot()) {
            spdlog::error("Insufficient permissions to set system timezone");
            return make_error_code(TimeError::PermissionDenied);
        }

        std::string tzPath = "/usr/share/zoneinfo/";
        std::string tzFile(timezone);
        std::string localtime = "/etc/localtime";

        struct stat buffer;
        std::string fullTzPath = tzPath + tzFile;
        if (stat(fullTzPath.c_str(), &buffer) != 0) {
            spdlog::error("Timezone file not found: {}", fullTzPath);
            return make_error_code(TimeError::InvalidParameter);
        }

        unlink(localtime.c_str());

        if (symlink(fullTzPath.c_str(), localtime.c_str()) != 0) {
            spdlog::error("Failed to set timezone: {}", strerror(errno));
            return std::error_code(errno, std::system_category());
        }

        setenv("TZ", tzFile.c_str(), 1);
        tzset();

        spdlog::info("Timezone successfully set to {}", tzFile);
        return make_error_code(TimeError::None);

    } catch (const std::exception& e) {
        spdlog::error("Exception in setSystemTimezone: {}", e.what());
        return make_error_code(TimeError::SystemError);
    }
}

auto TimeManagerImpl::syncTimeFromRTC() -> std::error_code {
    spdlog::debug("Synchronizing time from RTC");

    try {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (!atom::system::isRoot()) {
            spdlog::error("Insufficient permissions to sync from RTC");
            return make_error_code(TimeError::PermissionDenied);
        }

        FILE* pipe = popen("hwclock --hctosys", "r");
        if (!pipe) {
            spdlog::error("Failed to execute hwclock command: {}",
                          strerror(errno));
            return std::error_code(errno, std::system_category());
        }

        std::array<char, 128> buffer{};
        std::string result;

        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }

        int status = pclose(pipe);
        if (status != 0) {
            spdlog::error("hwclock command failed with status {}", status);
            return make_error_code(TimeError::SystemError);
        }

        updateTimeCache();
        spdlog::info("System time successfully synchronized from RTC");
        return make_error_code(TimeError::None);

    } catch (const std::exception& e) {
        spdlog::error("Exception in syncTimeFromRTC: {}", e.what());
        return make_error_code(TimeError::SystemError);
    }
}
#endif

auto TimeManagerImpl::getNtpTime(std::string_view hostname,
                                 std::chrono::milliseconds timeout)
    -> std::optional<std::time_t> {
    spdlog::debug("Getting NTP time from hostname: {}", hostname);

    try {
        if (!time_utils::validateHostname(hostname)) {
            spdlog::error("Invalid hostname parameter");
            return std::nullopt;
        }

        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto now = std::chrono::system_clock::now();
            if (now - last_ntp_query_ < cache_ttl_ && cached_ntp_time_ > 0 &&
                last_ntp_server_ == hostname) {
                spdlog::trace("Using cached NTP time: {}", cached_ntp_time_);
                return cached_ntp_time_;
            }
        }

        std::array<uint8_t, time_utils::NTP_PACKET_SIZE> packetBuffer{};

        SocketHandler socketHandler;
        if (!socketHandler.isValid()) {
            spdlog::error("Failed to create or initialize socket");
            return std::nullopt;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(time_utils::NTP_PORT);

        addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        std::string host_str(hostname);
        if (getaddrinfo(host_str.c_str(), nullptr, &hints, &result) != 0) {
            spdlog::error("Failed to resolve hostname: {}", hostname);
            return std::nullopt;
        }

        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> resultPtr(
            result, &freeaddrinfo);

        std::memcpy(&serverAddr.sin_addr,
                    &((sockaddr_in*)resultPtr->ai_addr)->sin_addr,
                    sizeof(serverAddr.sin_addr));

        packetBuffer[0] = 0x23;

        if (sendto(socketHandler.getFd(),
                   reinterpret_cast<const char*>(packetBuffer.data()),
                   packetBuffer.size(), 0,
                   reinterpret_cast<sockaddr*>(&serverAddr),
                   sizeof(serverAddr)) < 0) {
#ifdef _WIN32
            spdlog::error("Failed to send to NTP server: error code {}",
                          WSAGetLastError());
#else
            spdlog::error("Failed to send to NTP server: {}", strerror(errno));
#endif
            return std::nullopt;
        }

        timeval tv{};
        tv.tv_sec = static_cast<long>(timeout.count() / 1000);
        tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

        if (setsockopt(socketHandler.getFd(), SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char*>(&tv), sizeof(tv)) < 0) {
#ifdef _WIN32
            spdlog::error("Failed to set socket timeout: error code {}",
                          WSAGetLastError());
#else
            spdlog::error("Failed to set socket timeout: {}", strerror(errno));
#endif
            return std::nullopt;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(socketHandler.getFd(), &readfds);

        int selectResult =
            select(socketHandler.getFd() + 1, &readfds, nullptr, nullptr, &tv);
        if (selectResult <= 0) {
            spdlog::error("Failed to receive from NTP server (timeout)");
            return std::nullopt;
        }

        sockaddr_in serverResponseAddr{};
        socklen_t addrLen = sizeof(serverResponseAddr);

        int received = recvfrom(
            socketHandler.getFd(), reinterpret_cast<char*>(packetBuffer.data()),
            packetBuffer.size(), 0,
            reinterpret_cast<sockaddr*>(&serverResponseAddr), &addrLen);

        if (received < 0) {
#ifdef _WIN32
            spdlog::error("Failed to receive from NTP server: error code {}",
                          WSAGetLastError());
#else
            spdlog::error("Failed to receive from NTP server: {}",
                          strerror(errno));
#endif
            return std::nullopt;
        }

        if (received < 48) {
            spdlog::error("Received incomplete NTP packet: {} bytes", received);
            return std::nullopt;
        }

        uint32_t seconds = ((uint32_t)packetBuffer[40] << 24) |
                           ((uint32_t)packetBuffer[41] << 16) |
                           ((uint32_t)packetBuffer[42] << 8) | packetBuffer[43];

        if (seconds < time_utils::NTP_DELTA) {
            spdlog::error("Invalid NTP timestamp: {}", seconds);
            return std::nullopt;
        }

        time_t ntpTime = static_cast<time_t>(seconds - time_utils::NTP_DELTA);

        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            cached_ntp_time_ = ntpTime;
            last_ntp_query_ = std::chrono::system_clock::now();
            last_ntp_server_ = std::string(hostname);
        }

        spdlog::info("NTP time from {}: {}", hostname, ntpTime);
        return ntpTime;

    } catch (const std::exception& e) {
        spdlog::error("Exception in getNtpTime: {}", e.what());
        return std::nullopt;
    }
}

void TimeManagerImpl::updateTimeCache() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    last_update_ = std::chrono::system_clock::now();
    cached_time_ = std::chrono::system_clock::to_time_t(last_update_);
}

bool TimeManagerImpl::hasAdminPrivileges() const {
    return atom::system::isRoot();
}

}  // namespace atom::web
