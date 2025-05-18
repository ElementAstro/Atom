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

#ifdef __SSE2__
#include <emmintrin.h>  // SSE2 intrinsics
#endif

#include "atom/log/loguru.hpp"
#include "atom/system/user.hpp"
#include "atom/web/time/time_error.hpp"

namespace atom::web {

TimeManagerImpl::TimeManagerImpl() : cache_ttl_(std::chrono::minutes(5)) {
    try {
        // 初始化缓存，确保首次请求时有值可用
        updateTimeCache();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to initialize time cache: {}", e.what());
    }
}

auto TimeManagerImpl::getSystemTime() -> std::time_t {
    LOG_F(INFO, "Entering getSystemTime");
    try {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        auto systemTime = std::chrono::system_clock::to_time_t(now);
        LOG_F(INFO, "Exiting getSystemTime with value: {}", systemTime);
        return systemTime;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in getSystemTime: {}", e.what());
        throw std::system_error(
            std::error_code(EFAULT, std::system_category()),
            "Failed to get system time: " + std::string(e.what()));
    }
}

auto TimeManagerImpl::getSystemTimePoint()
    -> std::chrono::system_clock::time_point {
    LOG_F(INFO, "Entering getSystemTimePoint");
    try {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        LOG_F(INFO, "Exiting getSystemTimePoint");
        return now;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in getSystemTimePoint: {}", e.what());
        throw std::system_error(
            std::error_code(EFAULT, std::system_category()),
            "Failed to get system time point: " + std::string(e.what()));
    }
}

// SocketHandler 类实现
TimeManagerImpl::SocketHandler::SocketHandler() {
#ifdef _WIN32
    // 初始化 Windows Socket
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LOG_F(ERROR, "Failed to initialize Winsock");
        fd_ = INVALID_SOCKET;
        return;
    }

    // 创建 UDP 套接字
    fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd_ == INVALID_SOCKET) {
        LOG_F(ERROR, "Failed to create socket: {}", WSAGetLastError());
    }
#else
    // 创建 UDP 套接字
    fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd_ < 0) {
        LOG_F(ERROR, "Failed to create socket: {}", strerror(errno));
    }
#endif
}

TimeManagerImpl::SocketHandler::~SocketHandler() {
    if (isValid()) {
#ifdef _WIN32
        closesocket(fd_);
        WSACleanup();
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
    LOG_F(INFO, "Entering setSystemTime with values: {}-{}-{} {}:{}:{}", year,
          month, day, hour, minute, second);

    try {
        // Input validation
        if (!time_utils::validateDateTime(year, month, day, hour, minute,
                                          second)) {
            LOG_F(ERROR, "Invalid date/time parameters");
            return make_error_code(TimeError::InvalidParameter);
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Check permissions
        if (!atom::system::isRoot()) {
            LOG_F(ERROR, "Insufficient permissions to set system time");
            return make_error_code(TimeError::PermissionDenied);
        }

        SYSTEMTIME sysTime{};
        sysTime.wYear = static_cast<WORD>(year);
        sysTime.wMonth = static_cast<WORD>(month);
        sysTime.wDayOfWeek = 0;  // 忽略，系统会自动计算
        sysTime.wDay = static_cast<WORD>(day);
        sysTime.wHour = static_cast<WORD>(hour);
        sysTime.wMinute = static_cast<WORD>(minute);
        sysTime.wSecond = static_cast<WORD>(second);
        sysTime.wMilliseconds = 0;

        if (SetSystemTime(&sysTime) == 0) {
            DWORD error = GetLastError();
            LOG_F(ERROR, "Failed to set system time: error code {}", error);
            return std::error_code(error, std::system_category());
        } else {
            // 更新缓存
            updateTimeCache();
            LOG_F(INFO, "System time successfully set to: {}-{}-{} {}:{}:{}",
                  year, month, day, hour, minute, second);
            return make_error_code(TimeError::None);
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in setSystemTime: {}", e.what());
        return make_error_code(TimeError::SystemError);
    }
}

auto TimeManagerImpl::setSystemTimezone(std::string_view timezone)
    -> std::error_code {
    LOG_F(INFO, "Entering setSystemTimezone with timezone: {}",
          timezone.data());
    try {
        // 输入验证
        if (timezone.empty() || timezone.length() > 64) {
            LOG_F(ERROR, "Invalid timezone parameter");
            return make_error_code(TimeError::InvalidParameter);
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);

        // 检查权限
        if (!atom::system::isRoot()) {
            LOG_F(ERROR, "Insufficient permissions to set system timezone");
            return make_error_code(TimeError::PermissionDenied);
        }

        std::string timezoneStr(timezone);
        DWORD tzId;
        if (!getTimeZoneInformationByName(timezoneStr, &tzId)) {
            LOG_F(ERROR, "Failed to find timezone information for {}",
                  timezoneStr.c_str());
            return make_error_code(TimeError::InvalidParameter);
        }

        TIME_ZONE_INFORMATION tzInfo;
        if (GetTimeZoneInformation(&tzInfo) == TIME_ZONE_ID_INVALID) {
            DWORD error = GetLastError();
            LOG_F(ERROR,
                  "Failed to get current timezone information: error code {}",
                  error);
            return std::error_code(error, std::system_category());
        }

        if (tzInfo.StandardBias != -static_cast<int>(tzId)) {
            tzInfo.StandardBias = -static_cast<int>(tzId);
        }

        if (SetTimeZoneInformation(&tzInfo) == 0) {
            DWORD error = GetLastError();
            LOG_F(ERROR, "Failed to set timezone: error code {}", error);
            return std::error_code(error, std::system_category());
        }

        LOG_F(INFO, "Timezone successfully set to {}", timezoneStr.c_str());
        return make_error_code(TimeError::None);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in setSystemTimezone: {}", e.what());
        return make_error_code(TimeError::SystemError);
    }
}

auto TimeManagerImpl::syncTimeFromRTC() -> std::error_code {
    LOG_F(INFO, "Entering syncTimeFromRTC");
    try {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        // 检查权限
        if (!atom::system::isRoot()) {
            LOG_F(ERROR, "Insufficient permissions to sync from RTC");
            return make_error_code(TimeError::PermissionDenied);
        }

        SYSTEMTIME localTime;
        GetLocalTime(&localTime);

        TIME_ZONE_INFORMATION tzInfo;
        if (GetTimeZoneInformation(&tzInfo) == TIME_ZONE_ID_INVALID) {
            DWORD error = GetLastError();
            LOG_F(ERROR, "Failed to get timezone information: error code {}",
                  error);
            return std::error_code(error, std::system_category());
        }

        // RTC time retrieval on Windows is platform-specific
        // For demonstration, we'll use a simplified approach
        // In a real implementation, you would use Windows HAL/WMI to access RTC

        SYSTEMTIME rtcTime;
        // This is a placeholder - real implementation would get time from
        // hardware
        GetSystemTime(&rtcTime);

        if (SetSystemTime(&rtcTime) == 0) {
            DWORD error = GetLastError();
            LOG_F(ERROR, "Failed to set system time from RTC: error code {}",
                  error);
            return std::error_code(error, std::system_category());
        }

        // 更新缓存
        updateTimeCache();
        LOG_F(INFO, "System time successfully synchronized from RTC");
        return make_error_code(TimeError::None);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in syncTimeFromRTC: {}", e.what());
        return make_error_code(TimeError::SystemError);
    }
}

auto TimeManagerImpl::getTimeZoneInformationByName(const std::string& timezone,
                                                   DWORD* tzId) -> bool {
    LOG_F(INFO, "Entering getTimeZoneInformationByName with timezone: {}",
          timezone.c_str());
    try {
        // 这里简化实现，实际应该查询注册表获取时区信息
        // 这里仅作为示例，实际应用中应该完善此函数
        if (timezone == "UTC") {
            *tzId = 0;  // UTC 的偏移量为 0
            return true;
        } else if (timezone == "EST" || timezone == "America/New_York") {
            *tzId = 300;  // EST 偏移量为 -5 小时
            return true;
        } else if (timezone == "PST" || timezone == "America/Los_Angeles") {
            *tzId = 480;  // PST 偏移量为 -8 小时
            return true;
        } else if (timezone == "CST" || timezone == "Asia/Shanghai") {
            *tzId = -480;  // CST 偏移量为 +8 小时
            return true;
        }

        LOG_F(ERROR, "Timezone not found: {}", timezone.c_str());
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getTimeZoneInformationByName: {}", e.what());
        return false;
    }
}

#else
auto TimeManagerImpl::setSystemTime(int year, int month, int day, int hour,
                                    int minute, int second) -> std::error_code {
    LOG_F(INFO, "Entering setSystemTime with values: {}-{}-{} {}:{}:{}", year,
          month, day, hour, minute, second);

    try {
        // Input validation
        if (!time_utils::validateDateTime(year, month, day, hour, minute,
                                          second)) {
            LOG_F(ERROR, "Invalid date/time parameters");
            return make_error_code(TimeError::InvalidParameter);
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Check permissions
        if (!atom::system::isRoot()) {
            LOG_F(ERROR, "Insufficient permissions to set system time");
            return make_error_code(TimeError::PermissionDenied);
        }

        // 设置系统时间
        struct tm timeinfo = {};
        timeinfo.tm_year = year - 1900;  // tm_year is years since 1900
        timeinfo.tm_mon = month - 1;     // tm_mon is 0-11
        timeinfo.tm_mday = day;
        timeinfo.tm_hour = hour;
        timeinfo.tm_min = minute;
        timeinfo.tm_sec = second;
        timeinfo.tm_isdst = -1;  // Let the system determine DST status

        // Convert to time_t
        time_t rawtime = mktime(&timeinfo);
        if (rawtime == -1) {
            LOG_F(ERROR, "Failed to convert time");
            return make_error_code(TimeError::SystemError);
        }

        // Set system time
        struct timeval tv;
        tv.tv_sec = rawtime;
        tv.tv_usec = 0;

        if (settimeofday(&tv, nullptr) != 0) {
            LOG_F(ERROR, "Failed to set system time: {}", strerror(errno));
            return std::error_code(errno, std::system_category());
        }

        // 更新缓存
        updateTimeCache();
        LOG_F(INFO, "System time successfully set to: {}-{}-{} {}:{}:{}", year,
              month, day, hour, minute, second);
        return make_error_code(TimeError::None);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in setSystemTime: {}", e.what());
        return make_error_code(TimeError::SystemError);
    }
}

auto TimeManagerImpl::setSystemTimezone(std::string_view timezone)
    -> std::error_code {
    LOG_F(INFO, "Entering setSystemTimezone with timezone: {}",
          timezone.data());

    try {
        // 输入验证
        if (timezone.empty() || timezone.length() > 64) {
            LOG_F(ERROR, "Invalid timezone parameter");
            return make_error_code(TimeError::InvalidParameter);
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);

        // 检查权限
        if (!atom::system::isRoot()) {
            LOG_F(ERROR, "Insufficient permissions to set system timezone");
            return make_error_code(TimeError::PermissionDenied);
        }

        std::string tzPath = "/usr/share/zoneinfo/";
        std::string tzFile(timezone);
        std::string localtime = "/etc/localtime";

        // Check if timezone file exists
        struct stat buffer;
        std::string fullTzPath = tzPath + tzFile;
        if (stat(fullTzPath.c_str(), &buffer) != 0) {
            LOG_F(ERROR, "Timezone file not found: {}", fullTzPath.c_str());
            return make_error_code(TimeError::InvalidParameter);
        }

        // Remove old symlink if it exists
        unlink(localtime.c_str());

        // Create new symlink
        if (symlink(fullTzPath.c_str(), localtime.c_str()) != 0) {
            LOG_F(ERROR, "Failed to set timezone: {}", strerror(errno));
            return std::error_code(errno, std::system_category());
        }

        // Also update TZ environment variable
        setenv("TZ", tzFile.c_str(), 1);
        tzset();  // Apply changes to current process

        LOG_F(INFO, "Timezone successfully set to {}", tzFile.c_str());
        return make_error_code(TimeError::None);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in setSystemTimezone: {}", e.what());
        return make_error_code(TimeError::SystemError);
    }
}

auto TimeManagerImpl::syncTimeFromRTC() -> std::error_code {
    LOG_F(INFO, "Entering syncTimeFromRTC");

    try {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        // 检查权限
        if (!atom::system::isRoot()) {
            LOG_F(ERROR, "Insufficient permissions to sync from RTC");
            return make_error_code(TimeError::PermissionDenied);
        }

        // On Linux systems, the RTC is usually accessible via /dev/rtc0
        // We would need to use ioctl calls to access it
        // For simplicity, we'll use the hwclock command-line tool

        FILE* pipe = popen("hwclock --hctosys", "r");
        if (!pipe) {
            LOG_F(ERROR, "Failed to execute hwclock command: {}",
                  strerror(errno));
            return std::error_code(errno, std::system_category());
        }

        char buffer[128];
        std::string result = "";

        while (!feof(pipe)) {
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr)
                result += buffer;
        }

        int status = pclose(pipe);

        if (status != 0) {
            LOG_F(ERROR, "hwclock command failed with status {}", status);
            return make_error_code(TimeError::SystemError);
        }

        // 更新缓存
        updateTimeCache();
        LOG_F(INFO, "System time successfully synchronized from RTC");
        return make_error_code(TimeError::None);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in syncTimeFromRTC: {}", e.what());
        return make_error_code(TimeError::SystemError);
    }
}
#endif

auto TimeManagerImpl::getNtpTime(std::string_view hostname,
                                 std::chrono::milliseconds timeout)
    -> std::optional<std::time_t> {
    LOG_F(INFO, "Entering getNtpTime with hostname: {}", hostname.data());

    try {
        // 输入验证
        if (!time_utils::validateHostname(hostname)) {
            LOG_F(ERROR, "Invalid hostname parameter");
            return std::nullopt;
        }

        // 检查缓存 - 使用读锁
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto now = std::chrono::system_clock::now();
            if (now - last_ntp_query_ < cache_ttl_ && cached_ntp_time_ > 0 &&
                last_ntp_server_ == hostname) {
                LOG_F(INFO, "Using cached NTP time: {}", cached_ntp_time_);
                return cached_ntp_time_;
            }
        }

        std::array<uint8_t, time_utils::NTP_PACKET_SIZE> packetBuffer{};

        // 创建套接字并设置超时
        SocketHandler socketHandler;
        if (!socketHandler.isValid()) {
            LOG_F(ERROR, "Failed to create or initialize socket");
            return std::nullopt;
        }

        // 解析主机名
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(time_utils::NTP_PORT);

        // 使用getaddrinfo获取IP地址(支持IPv4和IPv6)
        addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        std::string host_str(hostname);
        if (getaddrinfo(host_str.c_str(), nullptr, &hints, &result) != 0) {
            LOG_F(ERROR, "Failed to resolve hostname: {}", hostname.data());
            return std::nullopt;
        }

        // 使用智能指针管理addrinfo资源
        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> resultPtr(
            result, &freeaddrinfo);

        // 获取IP地址
        memcpy(&serverAddr.sin_addr,
               &((sockaddr_in*)resultPtr->ai_addr)->sin_addr,
               sizeof(serverAddr.sin_addr));

        // 设置NTP数据包
        // LI = 0 (no warning), VN = 4 (NTPv4), Mode = 3 (client)
        packetBuffer[0] = 0x23;

        // 发送请求
        if (sendto(socketHandler.getFd(),
                   reinterpret_cast<const char*>(packetBuffer.data()),
                   packetBuffer.size(), 0,
                   reinterpret_cast<sockaddr*>(&serverAddr),
                   sizeof(serverAddr)) < 0) {
#ifdef _WIN32
            LOG_F(ERROR, "Failed to send to NTP server: error code {}",
                  WSAGetLastError());
#else
            LOG_F(ERROR, "Failed to send to NTP server: {}", strerror(errno));
#endif
            return std::nullopt;
        }

        // 设置超时
        timeval tv{};
        tv.tv_sec = static_cast<long>(timeout.count() / 1000);
        tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

#ifdef _WIN32
        // 在Windows上设置超时
        if (setsockopt(socketHandler.getFd(), SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char*>(&tv), sizeof(tv)) < 0) {
            LOG_F(ERROR, "Failed to set socket timeout: error code {}",
                  WSAGetLastError());
            return std::nullopt;
        }
#else
        // 在Linux/Unix上设置超时
        if (setsockopt(socketHandler.getFd(), SOL_SOCKET, SO_RCVTIMEO, &tv,
                       sizeof(tv)) < 0) {
            LOG_F(ERROR, "Failed to set socket timeout: {}", strerror(errno));
            return std::nullopt;
        }
#endif

        // 等待响应
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(socketHandler.getFd(), &readfds);

        int selectResult =
            select(socketHandler.getFd() + 1, &readfds, nullptr, nullptr, &tv);
        if (selectResult <= 0) {
#ifdef _WIN32
            LOG_F(ERROR,
                  "Failed to receive from NTP server (timeout): error code {}",
                  selectResult == 0 ? WSAETIMEDOUT : WSAGetLastError());
#else
            LOG_F(ERROR, "Failed to receive from NTP server (timeout): {}",
                  selectResult == 0 ? "Timed out" : strerror(errno));
#endif
            return std::nullopt;
        }

        // 接收响应
        sockaddr_in serverResponseAddr{};
        socklen_t addrLen = sizeof(serverResponseAddr);

        int received = recvfrom(
            socketHandler.getFd(), reinterpret_cast<char*>(packetBuffer.data()),
            packetBuffer.size(), 0,
            reinterpret_cast<sockaddr*>(&serverResponseAddr), &addrLen);

        if (received < 0) {
#ifdef _WIN32
            LOG_F(ERROR, "Failed to receive from NTP server: error code {}",
                  WSAGetLastError());
#else
            LOG_F(ERROR, "Failed to receive from NTP server: {}",
                  strerror(errno));
#endif
            return std::nullopt;
        }

        // 验证响应包大小
        if (received < 48) {
            LOG_F(ERROR, "Received incomplete NTP packet: {} bytes", received);
            return std::nullopt;
        }

        // 提取时间戳
        // NTP时间戳位于接收包的40-43字节处(秒)和44-47字节处(小数部分)
        uint32_t seconds = ((uint32_t)packetBuffer[40] << 24) |
                           ((uint32_t)packetBuffer[41] << 16) |
                           ((uint32_t)packetBuffer[42] << 8) | packetBuffer[43];

        // SIMD 优化可以在这里添加，但为了清晰起见，保持简单实现

        // NTP时间戳是从1900年开始的，需要减去从1900到1970年的秒数
        if (seconds < time_utils::NTP_DELTA) {
            LOG_F(ERROR, "Invalid NTP timestamp: {}", seconds);
            return std::nullopt;
        }

        time_t ntpTime = static_cast<time_t>(seconds - time_utils::NTP_DELTA);

        // 更新缓存 - 使用写锁
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            cached_ntp_time_ = ntpTime;
            last_ntp_query_ = std::chrono::system_clock::now();
            last_ntp_server_ = std::string(hostname);
        }

        LOG_F(INFO, "NTP time from {}: {}", hostname.data(), ntpTime);
        return ntpTime;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getNtpTime: {}", e.what());
        return std::nullopt;
    }
}

void TimeManagerImpl::updateTimeCache() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    last_update_ = std::chrono::system_clock::now();
    cached_time_ = std::chrono::system_clock::to_time_t(last_update_);
}

}  // namespace atom::web
