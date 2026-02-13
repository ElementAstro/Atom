#ifndef ATOM_EXTRA_CURL_RATE_LIMITER_HPP
#define ATOM_EXTRA_CURL_RATE_LIMITER_HPP

#include <atomic>
#include <chrono>
#include <spdlog/spdlog.h>

namespace atom::extra::curl {

/**
 * @brief Lock-free rate limiter using atomic token bucket algorithm
 *
 * This implementation provides thread-safe rate limiting without traditional
 * mutex locking, using atomic operations and memory ordering semantics for
 * optimal performance in high-concurrency scenarios.
 */
class RateLimiter {
public:
    /**
     * @brief Configuration for rate limiter behavior
     */
    struct Config {
        double requests_per_second = 10.0;
        size_t bucket_capacity = 100;  // Maximum burst size
        bool enable_burst = true;
        bool enable_statistics = true;
        std::chrono::nanoseconds precision = std::chrono::microseconds(100);

        static Config createDefault() {
            return Config{};
        }

        static Config createHighThroughput() {
            Config config;
            config.requests_per_second = 1000.0;
            config.bucket_capacity = 1000;
            config.precision = std::chrono::microseconds(10);
            return config;
        }

        static Config createLowLatency() {
            Config config;
            config.requests_per_second = 100.0;
            config.bucket_capacity = 10;
            config.enable_burst = false;
            config.precision = std::chrono::microseconds(1);
            return config;
        }
    };

    /**
     * @brief Statistics for rate limiter performance
     */
    struct Statistics {
        std::atomic<uint64_t> requests_allowed{0};
        std::atomic<uint64_t> requests_denied{0};
        std::atomic<uint64_t> wait_count{0};
        std::atomic<uint64_t> burst_count{0};
        std::atomic<uint64_t> contention_count{0};

        double getAllowedRatio() const noexcept {
            uint64_t total = requests_allowed.load(std::memory_order_relaxed) +
                           requests_denied.load(std::memory_order_relaxed);
            return total > 0 ? static_cast<double>(requests_allowed.load(std::memory_order_relaxed)) / total : 1.0;
        }
    };

    /**
     * @brief Constructor with configuration
     */
    explicit RateLimiter(const Config& config = Config::createDefault());

    /**
     * @brief Legacy constructor for compatibility
     */
    explicit RateLimiter(double requests_per_second);

    /**
     * @brief Destructor
     */
    ~RateLimiter();

    /**
     * @brief Wait for permission to make a request (blocking)
     */
    void wait();

    /**
     * @brief Try to acquire permission without blocking
     * @return true if permission granted, false if rate limit exceeded
     */
    bool try_acquire() noexcept;

    /**
     * @brief Wait with timeout for permission
     * @param timeout Maximum time to wait
     * @return true if permission granted within timeout
     */
    bool wait_for(std::chrono::nanoseconds timeout);

    /**
     * @brief Set new rate limit (thread-safe)
     */
    void set_rate(double requests_per_second) noexcept;

    /**
     * @brief Get current rate limit
     */
    double get_rate() const noexcept;

    /**
     * @brief Get current token count (approximate)
     */
    size_t get_tokens() const noexcept;

    /**
     * @brief Get statistics
     */
    const Statistics& getStatistics() const noexcept { return stats_; }

    /**
     * @brief Reset statistics
     */
    void resetStatistics() noexcept;

private:
    const Config config_;
    mutable Statistics stats_;

    // Token bucket state (all atomic for lock-free operation)
    alignas(64) std::atomic<uint64_t> tokens_;  // Current token count (scaled by 1e9)
    alignas(64) std::atomic<uint64_t> last_refill_time_;  // Nanoseconds since epoch
    alignas(64) std::atomic<uint64_t> tokens_per_nanosecond_;  // Rate scaled by 1e9
    alignas(64) std::atomic<uint64_t> max_tokens_;  // Bucket capacity scaled by 1e9

    static constexpr uint64_t SCALE_FACTOR = 1000000000ULL;  // 1e9 for precision

    /**
     * @brief Refill tokens based on elapsed time (lock-free)
     */
    void refillTokens() noexcept;

    /**
     * @brief Try to consume one token (lock-free)
     */
    bool consumeToken() noexcept;

    /**
     * @brief Get current time in nanoseconds
     */
    uint64_t getCurrentTimeNanos() const noexcept;

    /**
     * @brief Convert rate to tokens per nanosecond (scaled)
     */
    uint64_t rateToTokensPerNano(double rate) const noexcept;

    /**
     * @brief Adaptive backoff for contention
     */
    void adaptiveBackoff(size_t attempt) const noexcept;
};

}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_RATE_LIMITER_HPP
