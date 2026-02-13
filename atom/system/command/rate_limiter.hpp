/*
 * rate_limiter.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_RATE_LIMITER_HPP
#define ATOM_SYSTEM_COMMAND_RATE_LIMITER_HPP

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace atom::system {

/**
 * @brief Rate limiter for command execution
 */
class RateLimiter {
public:
    explicit RateLimiter(size_t maxRequests, std::chrono::milliseconds window);

    /**
     * @brief Check if a request is allowed
     * @param identifier Optional identifier for per-user/per-source limiting
     * @return true if request is allowed
     */
    bool allowRequest(const std::string& identifier = "");

    /**
     * @brief Get current request count for identifier
     */
    size_t getCurrentCount(const std::string& identifier = "") const;

    /**
     * @brief Reset rate limiter
     */
    void reset();

private:
    struct RequestWindow {
        std::vector<std::chrono::steady_clock::time_point> requests;
        mutable std::mutex mutex;
    };

    size_t maxRequests_;
    std::chrono::milliseconds window_;
    mutable std::mutex globalMutex_;
    std::unordered_map<std::string, std::unique_ptr<RequestWindow>> windows_;

    void cleanupOldRequests(RequestWindow& window) const;
};

}  // namespace atom::system

#endif  // ATOM_SYSTEM_COMMAND_RATE_LIMITER_HPP
