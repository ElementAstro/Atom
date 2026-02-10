// rate_limiter.hpp
/*
 * rate_limiter.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-01-29

Description: Rate limiter for DoS protection with sliding window algorithm

**************************************************/

#ifndef ATOM_CONNECTION_RATE_LIMITER_HPP
#define ATOM_CONNECTION_RATE_LIMITER_HPP

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace atom::connection {

/**
 * @class RateLimiter
 * @brief Thread-safe rate limiter using sliding window algorithm
 *
 * Provides rate limiting for connections and messages to protect against
 * DoS attacks. Uses a sliding window counter algorithm for efficient
 * memory usage and accurate rate limiting.
 */
class RateLimiter {
public:
    /**
     * @brief Construct a rate limiter
     * @param max_connections_per_ip Maximum concurrent connections per IP
     * @param max_messages_per_minute Maximum messages per minute per IP
     */
    RateLimiter(int max_connections_per_ip, int max_messages_per_minute)
        : max_connections_per_ip_(max_connections_per_ip),
          max_messages_per_minute_(max_messages_per_minute) {}

    /**
     * @brief Check if a new connection is allowed
     * @param ip_address The IP address to check
     * @return true if connection is allowed, false if rate limit exceeded
     */
    [[nodiscard]] bool canConnect(const std::string& ip_address) {
        std::unique_lock lock(mutex_);

        auto& count = connection_count_[ip_address];
        if (count >= max_connections_per_ip_) {
            return false;
        }

        ++count;
        return true;
    }

    /**
     * @brief Release a connection slot for an IP
     * @param ip_address The IP address to release
     */
    void releaseConnection(const std::string& ip_address) {
        std::unique_lock lock(mutex_);

        auto it = connection_count_.find(ip_address);
        if (it != connection_count_.end() && it->second > 0) {
            --it->second;
            if (it->second == 0) {
                connection_count_.erase(it);
            }
        }
    }

    /**
     * @brief Check if sending a message is allowed (sliding window algorithm)
     * @param ip_address The IP address to check
     * @return true if message is allowed, false if rate limit exceeded
     */
    [[nodiscard]] bool canSendMessage(const std::string& ip_address) {
        std::unique_lock lock(mutex_);

        auto now = std::chrono::steady_clock::now();
        auto& window = message_windows_[ip_address];

        // Check if we need to slide the window
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - window.window_start);

        if (elapsed >= std::chrono::seconds(60)) {
            // Start new window
            window.previous_count = window.current_count;
            window.current_count = 0;
            window.window_start = now;
            elapsed = std::chrono::seconds(0);
        }

        // Calculate weighted count using sliding window
        double weight =
            1.0 - (static_cast<double>(elapsed.count()) / 60.0);
        double weighted_count =
            window.previous_count * weight + window.current_count;

        if (weighted_count >= max_messages_per_minute_) {
            return false;
        }

        ++window.current_count;
        return true;
    }

    /**
     * @brief Get current connection count for an IP
     * @param ip_address The IP address to query
     * @return Current connection count
     */
    [[nodiscard]] int getConnectionCount(const std::string& ip_address) const {
        std::shared_lock lock(mutex_);
        auto it = connection_count_.find(ip_address);
        return it != connection_count_.end() ? it->second : 0;
    }

    /**
     * @brief Clear all rate limiting data
     */
    void clear() {
        std::unique_lock lock(mutex_);
        connection_count_.clear();
        message_windows_.clear();
    }

    /**
     * @brief Clean up expired entries to prevent memory growth
     */
    void cleanup() {
        std::unique_lock lock(mutex_);
        auto now = std::chrono::steady_clock::now();

        // Remove inactive message windows (no activity for 2 minutes)
        for (auto it = message_windows_.begin(); it != message_windows_.end();) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.window_start);
            if (elapsed >= std::chrono::seconds(120) &&
                it->second.current_count == 0) {
                it = message_windows_.erase(it);
            } else {
                ++it;
            }
        }

        // Remove zero connection counts
        for (auto it = connection_count_.begin();
             it != connection_count_.end();) {
            if (it->second == 0) {
                it = connection_count_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    struct SlidingWindow {
        size_t current_count{0};
        size_t previous_count{0};
        std::chrono::steady_clock::time_point window_start{
            std::chrono::steady_clock::now()};
    };

    int max_connections_per_ip_;
    int max_messages_per_minute_;
    std::unordered_map<std::string, int> connection_count_;
    std::unordered_map<std::string, SlidingWindow> message_windows_;
    mutable std::shared_mutex mutex_;
};

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_RATE_LIMITER_HPP
