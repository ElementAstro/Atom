/*
 * time_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "time_manager.hpp"
#include "time_manager_impl.hpp"

#include <spdlog/spdlog.h>

namespace atom::web {

TimeManager::TimeManager() : impl_(std::make_unique<TimeManagerImpl>()) {
    spdlog::debug("TimeManager initialized successfully");
}

TimeManager::~TimeManager() { spdlog::debug("TimeManager destroyed"); }

TimeManager::TimeManager(TimeManager&& other) noexcept
    : impl_(std::move(other.impl_)) {
    spdlog::trace("TimeManager moved");
}

TimeManager& TimeManager::operator=(TimeManager&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        spdlog::trace("TimeManager move assigned");
    }
    return *this;
}

auto TimeManager::getSystemTime() -> std::time_t {
    spdlog::trace("Getting system time");

    try {
        auto systemTime = impl_->getSystemTime();
        spdlog::trace("System time retrieved: {}", systemTime);
        return systemTime;
    } catch (const std::exception& e) {
        spdlog::error("Failed to get system time: {}", e.what());
        throw;
    }
}

auto TimeManager::getSystemTimePoint()
    -> std::chrono::system_clock::time_point {
    spdlog::trace("Getting system time point");

    try {
        auto timePoint = impl_->getSystemTimePoint();
        spdlog::trace("System time point retrieved");
        return timePoint;
    } catch (const std::exception& e) {
        spdlog::error("Failed to get system time point: {}", e.what());
        throw;
    }
}

auto TimeManager::setSystemTime(int year, int month, int day, int hour,
                                int minute, int second) -> std::error_code {
    spdlog::info(
        "Setting system time to: {}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", year,
        month, day, hour, minute, second);

    auto result = impl_->setSystemTime(year, month, day, hour, minute, second);

    if (result) {
        spdlog::error("Failed to set system time: {}", result.message());
    } else {
        spdlog::info("System time set successfully");
    }

    return result;
}

auto TimeManager::setSystemTimezone(std::string_view timezone)
    -> std::error_code {
    spdlog::info("Setting system timezone to: {}", timezone);

    auto result = impl_->setSystemTimezone(timezone);

    if (result) {
        spdlog::error("Failed to set system timezone: {}", result.message());
    } else {
        spdlog::info("System timezone set successfully to: {}", timezone);
    }

    return result;
}

auto TimeManager::syncTimeFromRTC() -> std::error_code {
    spdlog::info("Synchronizing time from RTC");

    auto result = impl_->syncTimeFromRTC();

    if (result) {
        spdlog::error("Failed to sync time from RTC: {}", result.message());
    } else {
        spdlog::info("Time synchronized from RTC successfully");
    }

    return result;
}

auto TimeManager::getNtpTime(std::string_view hostname,
                             std::chrono::milliseconds timeout)
    -> std::optional<std::time_t> {
    spdlog::debug("Getting NTP time from hostname: {} with timeout: {}ms",
                  hostname, timeout.count());

    auto ntpTime = impl_->getNtpTime(hostname, timeout);

    if (ntpTime) {
        spdlog::info("NTP time retrieved from {}: {}", hostname, *ntpTime);
    } else {
        spdlog::warn("Failed to get NTP time from: {}", hostname);
    }

    return ntpTime;
}

void TimeManager::setImpl(std::unique_ptr<TimeManagerImpl> impl) {
    spdlog::debug("Setting custom TimeManager implementation");
    impl_ = std::move(impl);
}

bool TimeManager::hasAdminPrivileges() const {
    return impl_->hasAdminPrivileges();
}

}  // namespace atom::web
