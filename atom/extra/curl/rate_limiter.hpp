#ifndef ATOM_EXTRA_CURL_RATE_LIMITER_HPP
#define ATOM_EXTRA_CURL_RATE_LIMITER_HPP

#include <chrono>
#include <mutex>

namespace atom::extra::curl {
/**
 * @brief Class for limiting the rate of requests.
 *
 * This class provides a mechanism to control the rate at which requests are
 * made, ensuring that the number of requests per second does not exceed a
 * specified limit. It uses a mutex to ensure thread safety.
 */
class RateLimiter {
public:
    /**
     * @brief Constructor for the RateLimiter class.
     *
     * @param requests_per_second The maximum number of requests allowed per
     * second.
     */
    RateLimiter(double requests_per_second);

    /**
     * @brief Waits to ensure that the rate limit is not exceeded.
     *
     * This method blocks the current thread until the rate limit allows
     * another request to be made.
     */
    void wait();

    /**
     * @brief Sets a new rate limit.
     *
     * @param requests_per_second The new maximum number of requests allowed per
     * second.
     */
    void set_rate(double requests_per_second);

private:
    /** @brief The maximum number of requests allowed per second. */
    double requests_per_second_;
    /** @brief The minimum delay between requests, in microseconds. */
    std::chrono::microseconds min_delay_;
    /** @brief The time of the last request. */
    std::chrono::steady_clock::time_point last_request_time_;
    /** @brief Mutex to protect the rate limiter from concurrent access. */
    std::mutex mutex_;
};

}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_RATE_LIMITER_HPP