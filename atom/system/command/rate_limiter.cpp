/*
 * rate_limiter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "rate_limiter.hpp"

#include <algorithm>

namespace atom::system {

RateLimiter::RateLimiter(size_t maxRequests, std::chrono::milliseconds window)
    : maxRequests_(maxRequests), window_(window) {}

bool RateLimiter::allowRequest(const std::string& identifier) {
    std::lock_guard<std::mutex> globalLock(globalMutex_);

    auto& window = windows_[identifier];
    if (!window) {
        window = std::make_unique<RequestWindow>();
    }

    std::lock_guard<std::mutex> windowLock(window->mutex);

    auto now = std::chrono::steady_clock::now();
    cleanupOldRequests(*window);

    if (window->requests.size() >= maxRequests_) {
        return false;
    }

    window->requests.push_back(now);
    return true;
}

size_t RateLimiter::getCurrentCount(const std::string& identifier) const {
    std::lock_guard<std::mutex> globalLock(globalMutex_);

    auto it = windows_.find(identifier);
    if (it == windows_.end()) {
        return 0;
    }

    std::lock_guard<std::mutex> windowLock(it->second->mutex);
    cleanupOldRequests(*it->second);
    return it->second->requests.size();
}

void RateLimiter::reset() {
    std::lock_guard<std::mutex> lock(globalMutex_);
    windows_.clear();
}

void RateLimiter::cleanupOldRequests(RequestWindow& window) const {
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - window_;

    window.requests.erase(
        std::remove_if(window.requests.begin(), window.requests.end(),
                      [cutoff](const auto& timestamp) {
                          return timestamp < cutoff;
                      }),
        window.requests.end());
}

}  // namespace atom::system
