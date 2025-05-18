/*
 * time_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "time_manager.hpp"
#include "time_manager_impl.hpp"

#include "atom/log/loguru.hpp"

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
