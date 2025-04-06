/*
 * time.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "time.hpp"

#include <cmath>
#include <ctime>
#include <fstream>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <sstream>

#ifdef __SSE2__
#include <emmintrin.h>  // SSE2 intrinsics
#endif

#ifdef _WIN32
// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <winreg.h>
#include <ws2tcpip.h>
// clang-format on
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#endif
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#endif

#include "atom/log/loguru.hpp"
#include "atom/system/user.hpp"

namespace {
// 常量定义
constexpr int MIN_VALID_YEAR = 1970;
constexpr int MAX_VALID_YEAR = 2038;
constexpr int MIN_VALID_MONTH = 1;
constexpr int MAX_VALID_MONTH = 12;
constexpr int MIN_VALID_DAY = 1;
constexpr int MAX_VALID_DAY = 31;
constexpr int MIN_VALID_HOUR = 0;
constexpr int MAX_VALID_HOUR = 23;
constexpr int MIN_VALID_MINUTE = 0;
constexpr int MAX_VALID_MINUTE = 59;
constexpr int MIN_VALID_SECOND = 0;
constexpr int MAX_VALID_SECOND = 59;

constexpr int NTP_PACKET_SIZE = 48;
constexpr uint16_t NTP_PORT = 123;
constexpr uint32_t NTP_DELTA = 2208988800UL;  // seconds between 1900 and 1970

// 自定义错误类型和错误代码
class time_error_category : public std::error_category {
public:
    const char* name() const noexcept override { return "time_error"; }

    std::string message(int ev) const override {
        switch (static_cast<atom::web::TimeError>(ev)) {
            case atom::web::TimeError::None:
                return "Success";
            case atom::web::TimeError::InvalidParameter:
                return "Invalid parameter";
            case atom::web::TimeError::PermissionDenied:
                return "Permission denied";
            case atom::web::TimeError::NetworkError:
                return "Network error";
            case atom::web::TimeError::SystemError:
                return "System error";
            case atom::web::TimeError::TimeoutError:
                return "Operation timed out";
            case atom::web::TimeError::NotSupported:
                return "Operation not supported";
            default:
                return "Unknown error";
        }
    }
};

const std::error_category& time_category() {
    static time_error_category category;
    return category;
}

std::error_code make_error_code(atom::web::TimeError e) {
    return {static_cast<int>(e), time_category()};
}

// 验证日期时间参数是否有效
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

// 检查主机名
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

}  // anonymous namespace

namespace atom::web {

class TimeManagerImpl {
public:
    TimeManagerImpl() : cache_ttl_(std::chrono::minutes(5)) {
        try {
            // 初始化缓存，确保首次请求时有值可用
            updateTimeCache();
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to initialize time cache: {}", e.what());
        }
    }

    auto getSystemTime() -> std::time_t {
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

    auto getSystemTimePoint() -> std::chrono::system_clock::time_point {
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

#ifdef _WIN32
    auto setSystemTime(int year, int month, int day, int hour, int minute,
                       int second) -> std::error_code {
        LOG_F(INFO, "Entering setSystemTime with values: {}-{}-{} {}:{}:{}",
              year, month, day, hour, minute, second);

        try {
            // Input validation
            if (!validateDateTime(year, month, day, hour, minute, second)) {
                LOG_F(ERROR, "Invalid date/time parameters");
                return make_error_code(TimeError::InvalidParameter);
            }

            std::unique_lock<std::shared_mutex> lock(mutex_);

            // Check permissions
            if (!atom::system::isRoot()) {
                LOG_F(ERROR,
                      "Permission denied. Need administrator privileges to set "
                      "system time.");
                return make_error_code(TimeError::PermissionDenied);
            }

            SYSTEMTIME sysTime{};
            sysTime.wYear = static_cast<WORD>(year);
            sysTime.wMonth = static_cast<WORD>(month);
            sysTime.wDayOfWeek = 0;  // Ignored by SetSystemTime
            sysTime.wDay = static_cast<WORD>(day);
            sysTime.wHour = static_cast<WORD>(hour);
            sysTime.wMinute = static_cast<WORD>(minute);
            sysTime.wSecond = static_cast<WORD>(second);
            sysTime.wMilliseconds = 0;

            if (SetSystemTime(&sysTime) == 0) {
                DWORD error = GetLastError();
                char errBuf[256];
                strerror_s(errBuf, sizeof(errBuf), error);
                const char* errStr = errBuf;
                LOG_F(ERROR,
                      "Failed to set system time to {}-{}-{} "
                      "{}:{}:{}. Error: {}",
                      year, month, day, hour, minute, second, errStr);
                return std::error_code(error, std::system_category());
            } else {
                DLOG_F(INFO, "System time has been set to {}-{}-{} {}:{}:{}.",
                       year, month, day, hour, minute, second);

                // 更新缓存
                updateTimeCache();
                return make_error_code(TimeError::None);
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in setSystemTime: {}", e.what());
            return make_error_code(TimeError::SystemError);
        }
    }

    auto setSystemTimezone(std::string_view timezone) -> std::error_code {
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
                LOG_F(ERROR,
                      "Permission denied. Need administrator privileges to set "
                      "system timezone.");
                return make_error_code(TimeError::PermissionDenied);
            }

            std::string timezoneStr(timezone);
            DWORD tzId;
            if (!getTimeZoneInformationByName(timezoneStr, &tzId)) {
                LOG_F(ERROR, "Error getting time zone id for {}: %lu",
                      timezoneStr.c_str(), GetLastError());
                return make_error_code(TimeError::InvalidParameter);
            }

            TIME_ZONE_INFORMATION tzInfo;
            if (GetTimeZoneInformation(&tzInfo) == TIME_ZONE_ID_INVALID) {
                DWORD error = GetLastError();
                char errBuf[256];
                strerror_s(errBuf, sizeof(errBuf), error);
                const char* errStr = errBuf;
                LOG_F(ERROR, "Error getting current time zone information: {}",
                      errStr);
                return std::error_code(error, std::system_category());
            }

            if (tzInfo.StandardBias != -static_cast<int>(tzId)) {
                LOG_F(ERROR,
                      "Time zone id obtained does not match offset: %lu != {}",
                      tzId, -tzInfo.StandardBias);
                return make_error_code(TimeError::InvalidParameter);
            }

            if (SetTimeZoneInformation(&tzInfo) == 0) {
                DWORD error = GetLastError();
                char errBuf[256];
                strerror_s(errBuf, sizeof(errBuf), error);
                const char* errStr = errBuf;
                LOG_F(ERROR, "Error setting time zone to {}: {}",
                      timezoneStr.c_str(), errStr);
                return std::error_code(error, std::system_category());
            }

            LOG_F(INFO, "Timezone successfully set to {}", timezoneStr.c_str());
            return make_error_code(TimeError::None);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in setSystemTimezone: {}", e.what());
            return make_error_code(TimeError::SystemError);
        }
    }

    auto syncTimeFromRTC() -> std::error_code {
        LOG_F(INFO, "Entering syncTimeFromRTC");
        try {
            std::unique_lock<std::shared_mutex> lock(mutex_);

            // 检查权限
            if (!atom::system::isRoot()) {
                LOG_F(ERROR,
                      "Permission denied. Need administrator privileges to set "
                      "system time.");
                return make_error_code(TimeError::PermissionDenied);
            }

            SYSTEMTIME localTime;
            GetLocalTime(&localTime);

            TIME_ZONE_INFORMATION tzInfo;
            if (GetTimeZoneInformation(&tzInfo) == TIME_ZONE_ID_INVALID) {
                DWORD error = GetLastError();
                char errBuf[256];
                strerror_s(errBuf, sizeof(errBuf), error);
                const char* errStr = errBuf;
                LOG_F(ERROR, "Error getting time zone information: {}", errStr);
                return std::error_code(error, std::system_category());
            }

            // RTC time retrieval on Windows is platform-specific
            // For demonstration, we'll use a simplified approach
            // In a real implementation, you would use Windows HAL/WMI to access
            // RTC

            SYSTEMTIME rtcTime;
            // This is a placeholder - real implementation would get time from
            // hardware
            GetSystemTime(&rtcTime);

            if (SetSystemTime(&rtcTime) == 0) {
                DWORD error = GetLastError();
                char errBuf[256];
                strerror_s(errBuf, sizeof(errBuf), error);
                const char* errStr = errBuf;
                LOG_F(ERROR, "Failed to set system time from RTC: {}", errStr);
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

    auto getTimeZoneInformationByName(const std::string& timezone, DWORD* tzId)
        -> bool {
        LOG_F(INFO, "Entering getTimeZoneInformationByName with timezone: {}",
              timezone.c_str());
        try {
            // RAII wrapper for registry key
            class RegKeyHandle {
            private:
                HKEY handle_ = nullptr;

            public:
                RegKeyHandle() = default;
                ~RegKeyHandle() {
                    if (handle_)
                        RegCloseKey(handle_);
                }
                RegKeyHandle(const RegKeyHandle&) = delete;
                RegKeyHandle& operator=(const RegKeyHandle&) = delete;

                HKEY get() const { return handle_; }
                PHKEY addr() { return &handle_; }
                bool isValid() const { return handle_ != nullptr; }
            };

            // Open main registry key
            RegKeyHandle mainKey;
            LPCTSTR regPath = TEXT(
                "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time "
                "Zones\\");
            LONG ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ,
                                    mainKey.addr());
            if (ret != ERROR_SUCCESS) {
                LOG_F(ERROR, "Failed to open registry key: {}", ret);
                return false;
            }

            // Get the total number of subkeys
            DWORD numSubKeys = 0;
            if (RegQueryInfoKey(mainKey.get(), nullptr, nullptr, nullptr,
                                &numSubKeys, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
                LOG_F(ERROR, "Failed to query registry key info");
                return false;
            }

            // Determine optimal thread count based on hardware and workload
            const unsigned int numThreads =
                std::min(std::thread::hardware_concurrency() > 0
                             ? std::thread::hardware_concurrency()
                             : 4,
                         static_cast<unsigned int>(numSubKeys));

            // Shared state between threads
            std::atomic<bool> found{false};
            std::atomic<DWORD> nextIndex{0};
            std::mutex resultMutex;

            // Function to process registry keys
            auto processRegistryKeys = [&]() {
                TCHAR subKeyName[MAX_PATH];
                TCHAR displayName[MAX_PATH];
                DWORD index;

                // Process keys until we either find a match or exhaust all keys
                while (!found &&
                       (index = nextIndex.fetch_add(1)) < numSubKeys) {
                    // Get the name of the subkey
                    DWORD subKeyNameSize = MAX_PATH;
                    if (RegEnumKeyEx(mainKey.get(), index, subKeyName,
                                     &subKeyNameSize, nullptr, nullptr, nullptr,
                                     nullptr) != ERROR_SUCCESS) {
                        continue;
                    }

                    // Open this subkey
                    RegKeyHandle subKey;
                    if (RegOpenKeyEx(mainKey.get(), subKeyName, 0, KEY_READ,
                                     subKey.addr()) != ERROR_SUCCESS) {
                        continue;
                    }

                    // Query the Display value
                    DWORD displayNameSize = sizeof(displayName);
                    if (RegQueryValueEx(subKey.get(), TEXT("Display"), nullptr,
                                        nullptr,
                                        reinterpret_cast<LPBYTE>(displayName),
                                        &displayNameSize) != ERROR_SUCCESS) {
                        continue;
                    }

// Compare timezone names - handle potential encoding issues
#ifdef UNICODE
                    // Convert wide string to UTF-8 for comparison
                    std::wstring wDisplayName(displayName);
                    std::string utf8DisplayName;
                    int utf8Size =
                        WideCharToMultiByte(CP_UTF8, 0, wDisplayName.c_str(),
                                            -1, nullptr, 0, nullptr, nullptr);
                    if (utf8Size > 0) {
                        utf8DisplayName.resize(utf8Size -
                                               1);  // -1 for null terminator
                        WideCharToMultiByte(CP_UTF8, 0, wDisplayName.c_str(),
                                            -1, &utf8DisplayName[0], utf8Size,
                                            nullptr, nullptr);

                        if (timezone == utf8DisplayName) {
                            // Found the timezone, now get the TZI information
                            TIME_ZONE_INFORMATION tzi;
                            DWORD tziSize = sizeof(tzi);

                            if (RegQueryValueEx(subKey.get(), TEXT("TZI"),
                                                nullptr, nullptr,
                                                reinterpret_cast<LPBYTE>(&tzi),
                                                &tziSize) == ERROR_SUCCESS) {
                                std::lock_guard<std::mutex> lock(resultMutex);
                                if (!found) {
                                    // Map TZI to a timezone ID as needed by the
                                    // caller This depends on what the caller
                                    // expects in tzId Here we'll assume it's
                                    // some hash or identifier
                                    *tzId = static_cast<DWORD>(tzi.Bias);
                                    found = true;
                                }
                            }
                        }
                    }
#else
                    // Direct comparison for non-Unicode build
                    if (timezone == displayName) {
                        // Found the timezone, now get the TZI information
                        TIME_ZONE_INFORMATION tzi;
                        DWORD tziSize = sizeof(tzi);

                        if (RegQueryValueEx(subKey.get(), TEXT("TZI"), nullptr,
                                            nullptr,
                                            reinterpret_cast<LPBYTE>(&tzi),
                                            &tziSize) == ERROR_SUCCESS) {
                            std::lock_guard<std::mutex> lock(resultMutex);
                            if (!found) {
                                // Map TZI to a timezone ID as needed by the
                                // caller
                                *tzId = static_cast<DWORD>(tzi.Bias);
                                found = true;
                            }
                        }
                    }
#endif
                }
            };

            // Launch threads to process registry keys in parallel
            std::vector<std::thread> threads;
            threads.reserve(numThreads);
            for (unsigned int i = 0; i < numThreads; ++i) {
                threads.emplace_back(processRegistryKeys);
            }

            // Wait for all threads to complete
            for (auto& t : threads) {
                if (t.joinable()) {
                    t.join();
                }
            }

            LOG_F(INFO, "Timezone search complete: {}",
                  found ? "found" : "not found");
            return found;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in getTimeZoneInformationByName: {}",
                  e.what());
            return false;
        }
    }

#else
public:
    auto setSystemTime(int year, int month, int day, int hour, int minute,
                       int second) -> std::error_code {
        LOG_F(INFO, "Entering setSystemTime with values: {}-{}-{} {}:{}:{}",
              year, month, day, hour, minute, second);

        try {
            // Input validation
            if (!validateDateTime(year, month, day, hour, minute, second)) {
                LOG_F(ERROR, "Invalid date/time parameters");
                return make_error_code(TimeError::InvalidParameter);
            }

            std::unique_lock<std::shared_mutex> lock(mutex_);

            // Check permissions
            if (!atom::system::isRoot()) {
                LOG_F(ERROR,
                      "Permission denied. Need root privileges to set system "
                      "time.");
                return make_error_code(TimeError::PermissionDenied);
            }

            constexpr int BASE_YEAR = 1900;
            struct tm newTime{};
            newTime.tm_sec = second;
            newTime.tm_min = minute;
            newTime.tm_hour = hour;
            newTime.tm_mday = day;
            newTime.tm_mon = month - 1;
            newTime.tm_year = year - BASE_YEAR;
            newTime.tm_isdst = -1;

            time_t timeValue = mktime(&newTime);
            if (timeValue == -1) {
                LOG_F(ERROR, "Failed to convert time values to time_t");
                return make_error_code(TimeError::InvalidParameter);
            }

            struct timeval tv;
            tv.tv_sec = timeValue;
            tv.tv_usec = 0;

            if (settimeofday(&tv, nullptr) == -1) {
                int errnum = errno;
                char errBuf[256];
                const char* errStr = strerror_r(errnum, errBuf, sizeof(errBuf));
                LOG_F(ERROR, "Failed to set system time: {}", errStr);
                return std::error_code(errnum, std::system_category());
            }

            // 更新缓存
            updateTimeCache();

            DLOG_F(INFO, "System time has been set to {}-{}-{} {}:{}:{}.", year,
                   month, day, hour, minute, second);
            return make_error_code(TimeError::None);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in setSystemTime: {}", e.what());
            return make_error_code(TimeError::SystemError);
        }
    }

    auto setSystemTimezone(std::string_view timezone) -> std::error_code {
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
                LOG_F(ERROR,
                      "Permission denied. Need root privileges to set system "
                      "timezone.");
                return make_error_code(TimeError::PermissionDenied);
            }

            std::string timezoneStr(timezone);

            // 验证时区文件是否存在
            std::ostringstream zonePath;
            zonePath << "/usr/share/zoneinfo/" << timezoneStr;
            struct stat buffer;
            if (stat(zonePath.str().c_str(), &buffer) != 0) {
                LOG_F(ERROR, "Timezone {} does not exist", timezoneStr.c_str());
                return make_error_code(TimeError::InvalidParameter);
            }

            // 尝试创建符号链接前先删除现有的
            if (unlink("/etc/localtime") != 0 && errno != ENOENT) {
                int errnum = errno;
                char errBuf[256];
                const char* errStr = strerror_r(errnum, errBuf, sizeof(errBuf));
                LOG_F(ERROR, "Failed to remove existing timezone link: {}",
                      errStr);
                return std::error_code(errnum, std::system_category());
            }

            // 创建符号链接
            if (symlink(zonePath.str().c_str(), "/etc/localtime") != 0) {
                int errnum = errno;
                char errBuf[256];
                const char* errStr = strerror_r(errnum, errBuf, sizeof(errBuf));
                LOG_F(ERROR, "Failed to set timezone to {}: {}",
                      timezoneStr.c_str(), errStr);
                return std::error_code(errnum, std::system_category());
            }

            // 更新TZ环境变量
            if (setenv("TZ", timezoneStr.c_str(), 1) != 0) {
                int errnum = errno;
                char errBuf[256];
                const char* errStr = strerror_r(errnum, errBuf, sizeof(errBuf));
                LOG_F(ERROR, "Error setting TZ environment variable to {}: {}",
                      timezoneStr.c_str(), errStr);
                return std::error_code(errnum, std::system_category());
            }

            // 更新系统配置文件
            std::ofstream tzFile("/etc/timezone",
                                 std::ios::out | std::ios::trunc);
            if (!tzFile.is_open()) {
                LOG_F(ERROR, "Failed to open /etc/timezone for writing");
                return make_error_code(TimeError::SystemError);
            }
            tzFile << timezoneStr << std::endl;
            tzFile.close();

            // 重新加载时区信息
            tzset();

            LOG_F(INFO, "Timezone successfully set to {}", timezoneStr.c_str());
            return make_error_code(TimeError::None);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in setSystemTimezone: {}", e.what());
            return make_error_code(TimeError::SystemError);
        }
    }

    auto syncTimeFromRTC() -> std::error_code {
        LOG_F(INFO, "Entering syncTimeFromRTC");

        try {
            std::unique_lock<std::shared_mutex> lock(mutex_);

            // 检查权限
            if (!atom::system::isRoot()) {
                LOG_F(ERROR,
                      "Permission denied. Need root privileges to set system "
                      "time.");
                return make_error_code(TimeError::PermissionDenied);
            }

            // 检查RTC设备是否存在
            const char* rtcPath = "/dev/rtc0";
            struct stat rtcStat;
            if (stat(rtcPath, &rtcStat) != 0) {
                rtcPath = "/dev/rtc";  // 尝试替代路径
                if (stat(rtcPath, &rtcStat) != 0) {
                    int errnum = errno;
                    char errBuf[256];
                    const char* errStr =
                        strerror_r(errnum, errBuf, sizeof(errBuf));
                    LOG_F(ERROR, "RTC device not found: {}", errStr);
                    return make_error_code(TimeError::NotSupported);
                }
            }

            // 使用hwclock命令同步时间
            int result = ::system("hwclock --hctosys");
            if (result != 0) {
                LOG_F(
                    ERROR,
                    "Failed to synchronize time from RTC, hwclock returned: {}",
                    result);
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
    auto getNtpTime(std::string_view hostname,
                    std::chrono::milliseconds timeout)
        -> std::optional<std::time_t> {
        LOG_F(INFO, "Entering getNtpTime with hostname: {}", hostname.data());

        try {
            // 输入验证
            if (!validateHostname(hostname)) {
                LOG_F(ERROR, "Invalid hostname: {}", hostname.data());
                return std::nullopt;
            }

            // 检查缓存 - 使用读锁
            {
                std::shared_lock<std::shared_mutex> lock(mutex_);
                auto now = std::chrono::system_clock::now();
                if (now - last_ntp_query_ < cache_ttl_ &&
                    cached_ntp_time_ > 0 && last_ntp_server_ == hostname) {
                    LOG_F(INFO, "Using cached NTP time for {}: {}",
                          hostname.data(), cached_ntp_time_);
                    return cached_ntp_time_;
                }
            }

            std::array<uint8_t, NTP_PACKET_SIZE> packetBuffer{};

            // 创建套接字并设置超时
            SocketHandler socketHandler;
            if (!socketHandler.isValid()) {
                LOG_F(ERROR, "Failed to initialize network socket");
                return std::nullopt;
            }

            // 解析主机名
            sockaddr_in serverAddr{};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(NTP_PORT);

            // 使用getaddrinfo获取IP地址(支持IPv4和IPv6)
            struct addrinfo hints{}, *result = nullptr;
            hints.ai_family = AF_INET;  // IPv4
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
            packetBuffer[0] = 0x23;  // 00100011

            // 发送请求
            if (sendto(socketHandler.getFd(),
                       reinterpret_cast<const char*>(packetBuffer.data()),
                       packetBuffer.size(), 0,
                       reinterpret_cast<sockaddr*>(&serverAddr),
                       sizeof(serverAddr)) < 0) {
#ifdef _WIN32
                DWORD error = WSAGetLastError();
                char errBuf[256] = {0};
                FormatMessageA(
                    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    errBuf, sizeof(errBuf), NULL);
                LOG_F(ERROR, "Failed to send NTP request: {}", errBuf);
#else
                char errBuf[256];
                const char* errStr = strerror_r(errno, errBuf, sizeof(errBuf));
                LOG_F(ERROR, "Failed to send NTP request: {}", errStr);
#endif
                return std::nullopt;
            }

            // 设置超时
            timeval tv{};
            tv.tv_sec = static_cast<long>(timeout.count() / 1000);
            tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

#ifdef _WIN32
            if (setsockopt(socketHandler.getFd(), SOL_SOCKET, SO_RCVTIMEO,
                           reinterpret_cast<char*>(&tv), sizeof(tv)) < 0) {
                DWORD error = WSAGetLastError();
                char errBuf[256] = {0};
                FormatMessageA(
                    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    errBuf, sizeof(errBuf), NULL);
                LOG_F(ERROR, "Failed to set socket timeout: {}", errBuf);
                return std::nullopt;
            }
#else
            if (setsockopt(socketHandler.getFd(), SOL_SOCKET, SO_RCVTIMEO,
                           reinterpret_cast<char*>(&tv), sizeof(tv)) < 0) {
                char errBuf[256];
                const char* errStr = strerror_r(errno, errBuf, sizeof(errBuf));
                LOG_F(ERROR, "Failed to set socket timeout: {}", errStr);
                return std::nullopt;
            }
#endif

            // 等待响应
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(socketHandler.getFd(), &readfds);

            int selectResult = select(socketHandler.getFd() + 1, &readfds,
                                      nullptr, nullptr, &tv);
            if (selectResult <= 0) {
                LOG_F(ERROR,
                      "Failed to receive NTP response: timeout or error");
                return std::nullopt;
            }

            // 接收响应
            sockaddr_in serverResponseAddr{};
            socklen_t addrLen = sizeof(serverResponseAddr);

            int received = recvfrom(
                socketHandler.getFd(),
                reinterpret_cast<char*>(packetBuffer.data()),
                packetBuffer.size(), 0,
                reinterpret_cast<sockaddr*>(&serverResponseAddr), &addrLen);

            if (received < 0) {
#ifdef _WIN32
                DWORD error = WSAGetLastError();
                char errBuf[256] = {0};
                FormatMessageA(
                    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    errBuf, sizeof(errBuf), NULL);
                LOG_F(ERROR, "Failed to receive NTP response: {}", errBuf);
#else
                char errBuf[256];
                const char* errStr = strerror_r(errno, errBuf, sizeof(errBuf));
                LOG_F(ERROR, "Failed to receive NTP response: {}", errStr);
#endif
                return std::nullopt;
            }

            // 验证响应包大小
            if (received < 48) {
                LOG_F(ERROR, "Received incomplete NTP packet: {} bytes",
                      received);
                return std::nullopt;
            }

            // 提取时间戳
            // NTP时间戳位于接收包的40-43字节处(秒)和44-47字节处(小数部分)
            uint32_t seconds = 0;

            // 使用SIMD指令加速数据处理
#ifdef __SSE2__
            // 如果支持SSE2指令集，使用它来快速处理数据
            const int NTP_TIMESTAMP_START = 40;
            __m128i data = _mm_loadu_si128(
                (__m128i*)(packetBuffer.data() + NTP_TIMESTAMP_START));

            // 提取前4个字节作为秒数
            seconds = _mm_cvtsi128_si32(
                _mm_shuffle_epi32(data, _MM_SHUFFLE(3, 2, 1, 0)));
            seconds =
                ((seconds & 0xFF) << 24) | (((seconds >> 8) & 0xFF) << 16) |
                (((seconds >> 16) & 0xFF) << 8) | ((seconds >> 24) & 0xFF);
#else
            // 标准方式解析数据
            const int NTP_TIMESTAMP_START = 40;
            const int NTP_TIMESTAMP_END = 43;
            constexpr size_t TIMESTAMP_BITS = 8;

            for (int i = NTP_TIMESTAMP_START; i <= NTP_TIMESTAMP_END; i++) {
                seconds = (seconds << TIMESTAMP_BITS) | packetBuffer[i];
            }
#endif

            // NTP时间戳是从1900年开始的，需要减去从1900到1970年的秒数
            if (seconds < NTP_DELTA) {
                LOG_F(ERROR, "Invalid NTP timestamp: %u", seconds);
                return std::nullopt;
            }

            time_t ntpTime = static_cast<time_t>(seconds - NTP_DELTA);

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

private:
    std::shared_mutex mutex_;
    std::chrono::minutes cache_ttl_;
    std::chrono::system_clock::time_point last_update_;
    std::time_t cached_time_{0};

    // NTP缓存
    std::chrono::system_clock::time_point last_ntp_query_;
    std::time_t cached_ntp_time_{0};
    std::string last_ntp_server_;

    // 使用RAII管理套接字资源
    class SocketHandler {
    public:
        SocketHandler() {
#ifdef _WIN32
            // 初始化Windows套接字
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                fd_ = INVALID_SOCKET;
                return;
            }
            fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
            fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#endif
        }

        ~SocketHandler() {
            if (isValid()) {
#ifdef _WIN32
                closesocket(fd_);
                WSACleanup();
#else
                close(fd_);
#endif
            }
        }

        bool isValid() const {
#ifdef _WIN32
            return fd_ != static_cast<int>(INVALID_SOCKET);
#else
            return fd_ >= 0;
#endif
        }

        int getFd() const { return fd_; }

        // 禁止复制
        SocketHandler(const SocketHandler&) = delete;
        SocketHandler& operator=(const SocketHandler&) = delete;

    private:
        int fd_;
    };

    void updateTimeCache() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        last_update_ = std::chrono::system_clock::now();
        cached_time_ = std::chrono::system_clock::to_time_t(last_update_);
    }
};

}  // namespace atom::web

// TimeManager methods

namespace atom::web {

TimeManager::TimeManager() : impl_(std::make_unique<TimeManagerImpl>()) {
    LOG_F(INFO, "TimeManager constructor called");
}

TimeManager::~TimeManager() { LOG_F(INFO, "TimeManager destructor called"); }

// 实现移动构造函数和移动赋值运算符
TimeManager::TimeManager(TimeManager&& other) noexcept
    : impl_(std::move(other.impl_)) {
    LOG_F(INFO, "TimeManager move constructor called");
}

TimeManager& TimeManager::operator=(TimeManager&& other) noexcept {
    LOG_F(INFO, "TimeManager move assignment operator called");
    if (this != &other) {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

auto TimeManager::getSystemTime() -> std::time_t {
    LOG_F(INFO, "TimeManager::getSystemTime called");
    try {
        auto systemTime = impl_->getSystemTime();
        LOG_F(INFO, "TimeManager::getSystemTime returning: {}", systemTime);
        return systemTime;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in TimeManager::getSystemTime: {}", e.what());
        throw;  // 重新抛出异常
    }
}

auto TimeManager::getSystemTimePoint()
    -> std::chrono::system_clock::time_point {
    LOG_F(INFO, "TimeManager::getSystemTimePoint called");
    try {
        auto timePoint = impl_->getSystemTimePoint();
        LOG_F(INFO, "TimeManager::getSystemTimePoint returning time point");
        return timePoint;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in TimeManager::getSystemTimePoint: {}", e.what());
        throw;  // 重新抛出异常
    }
}

auto TimeManager::setSystemTime(int year, int month, int day, int hour,
                                int minute, int second) -> std::error_code {
    LOG_F(INFO,
          "TimeManager::setSystemTime called with values: {}-{}-{} "
          "{}:{}:{}",
          year, month, day, hour, minute, second);

    auto result = impl_->setSystemTime(year, month, day, hour, minute, second);

    if (result) {
        LOG_F(INFO, "TimeManager::setSystemTime failed: {}",
              result.message().c_str());
    } else {
        LOG_F(INFO, "TimeManager::setSystemTime completed successfully");
    }

    return result;
}

auto TimeManager::setSystemTimezone(std::string_view timezone)
    -> std::error_code {
    LOG_F(INFO, "TimeManager::setSystemTimezone called with timezone: {}",
          timezone.data());

    auto result = impl_->setSystemTimezone(timezone);

    if (result) {
        LOG_F(INFO, "TimeManager::setSystemTimezone failed: {}",
              result.message().c_str());
    } else {
        LOG_F(INFO, "TimeManager::setSystemTimezone completed successfully");
    }

    return result;
}

auto TimeManager::syncTimeFromRTC() -> std::error_code {
    LOG_F(INFO, "TimeManager::syncTimeFromRTC called");

    auto result = impl_->syncTimeFromRTC();

    if (result) {
        LOG_F(INFO, "TimeManager::syncTimeFromRTC failed: {}",
              result.message().c_str());
    } else {
        LOG_F(INFO, "TimeManager::syncTimeFromRTC completed successfully");
    }

    return result;
}

auto TimeManager::getNtpTime(std::string_view hostname,
                             std::chrono::milliseconds timeout)
    -> std::optional<std::time_t> {
    LOG_F(INFO, "TimeManager::getNtpTime called with hostname: {}",
          hostname.data());

    auto ntpTime = impl_->getNtpTime(hostname, timeout);

    if (ntpTime) {
        LOG_F(INFO, "TimeManager::getNtpTime returning: {}", *ntpTime);
    } else {
        LOG_F(ERROR, "TimeManager::getNtpTime failed to get time from {}",
              hostname.data());
    }

    return ntpTime;
}

void TimeManager::setImpl(std::unique_ptr<TimeManagerImpl> impl) {
    LOG_F(INFO, "TimeManager::setImpl called");
    impl_ = std::move(impl);
}

}  // namespace atom::web
