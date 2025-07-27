#include "rate_limiter.hpp"
#include <algorithm>
#include <thread>

namespace atom::extra::curl {

RateLimiter::RateLimiter(const Config& config) : config_(config) {
    uint64_t now = getCurrentTimeNanos();

    tokens_.store(config_.bucket_capacity * SCALE_FACTOR, std::memory_order_relaxed);
    last_refill_time_.store(now, std::memory_order_relaxed);
    tokens_per_nanosecond_.store(rateToTokensPerNano(config_.requests_per_second), std::memory_order_relaxed);
    max_tokens_.store(config_.bucket_capacity * SCALE_FACTOR, std::memory_order_relaxed);

    spdlog::info("Initialized lock-free rate limiter: {:.2f} req/s, bucket capacity: {}, burst: {}",
                 config_.requests_per_second, config_.bucket_capacity, config_.enable_burst);
}

RateLimiter::RateLimiter(double requests_per_second)
    : RateLimiter(Config{.requests_per_second = requests_per_second}) {}

RateLimiter::~RateLimiter() {
    spdlog::info("Rate limiter destroyed. Stats - Allowed: {}, Denied: {}, Waits: {}, Allow ratio: {:.2f}%",
                 stats_.requests_allowed.load(), stats_.requests_denied.load(),
                 stats_.wait_count.load(), stats_.getAllowedRatio() * 100.0);
}

void RateLimiter::wait() {
    stats_.wait_count.fetch_add(1, std::memory_order_relaxed);

    size_t attempt = 0;
    while (!try_acquire()) {
        adaptiveBackoff(attempt++);

        if (attempt % 1000 == 0) {
            stats_.contention_count.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

bool RateLimiter::try_acquire() noexcept {
    refillTokens();

    if (consumeToken()) {
        stats_.requests_allowed.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    stats_.requests_denied.fetch_add(1, std::memory_order_relaxed);
    return false;
}

bool RateLimiter::wait_for(std::chrono::nanoseconds timeout) {
    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + timeout;

    stats_.wait_count.fetch_add(1, std::memory_order_relaxed);

    size_t attempt = 0;
    while (std::chrono::steady_clock::now() < end_time) {
        if (try_acquire()) {
            return true;
        }

        adaptiveBackoff(attempt++);

        if (attempt % 100 == 0) {
            stats_.contention_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    return false;
}

void RateLimiter::set_rate(double requests_per_second) noexcept {
    uint64_t new_rate = rateToTokensPerNano(requests_per_second);
    tokens_per_nanosecond_.store(new_rate, std::memory_order_release);

    spdlog::debug("Rate limiter updated to {:.2f} req/s", requests_per_second);
}

double RateLimiter::get_rate() const noexcept {
    uint64_t rate_scaled = tokens_per_nanosecond_.load(std::memory_order_acquire);
    return static_cast<double>(rate_scaled) / SCALE_FACTOR * 1e9;  // Convert back to req/s
}

size_t RateLimiter::get_tokens() const noexcept {
    uint64_t tokens_scaled = tokens_.load(std::memory_order_acquire);
    return static_cast<size_t>(tokens_scaled / SCALE_FACTOR);
}

void RateLimiter::resetStatistics() noexcept {
    stats_.requests_allowed.store(0, std::memory_order_relaxed);
    stats_.requests_denied.store(0, std::memory_order_relaxed);
    stats_.wait_count.store(0, std::memory_order_relaxed);
    stats_.burst_count.store(0, std::memory_order_relaxed);
    stats_.contention_count.store(0, std::memory_order_relaxed);
}

void RateLimiter::refillTokens() noexcept {
    uint64_t now = getCurrentTimeNanos();
    uint64_t last_refill = last_refill_time_.load(std::memory_order_acquire);

    if (now <= last_refill) {
        return;  // Time hasn't advanced or went backwards
    }

    uint64_t elapsed = now - last_refill;
    uint64_t rate = tokens_per_nanosecond_.load(std::memory_order_acquire);
    uint64_t tokens_to_add = elapsed * rate / SCALE_FACTOR;

    if (tokens_to_add == 0) {
        return;  // Not enough time elapsed to add tokens
    }

    // Try to update last refill time first (prevents multiple threads from adding tokens)
    if (!last_refill_time_.compare_exchange_strong(last_refill, now,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
        return;  // Another thread already updated
    }

    // Add tokens with saturation at max capacity
    uint64_t max_tokens = max_tokens_.load(std::memory_order_acquire);
    uint64_t current_tokens = tokens_.load(std::memory_order_acquire);

    uint64_t new_tokens = std::min(current_tokens + tokens_to_add, max_tokens);

    // Use CAS loop to update tokens
    while (!tokens_.compare_exchange_weak(current_tokens, new_tokens,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire)) {
        new_tokens = std::min(current_tokens + tokens_to_add, max_tokens);
    }
}

bool RateLimiter::consumeToken() noexcept {
    uint64_t current_tokens = tokens_.load(std::memory_order_acquire);

    // Check if we have at least one token
    if (current_tokens < SCALE_FACTOR) {
        return false;
    }

    uint64_t new_tokens = current_tokens - SCALE_FACTOR;

    // Use CAS to atomically consume one token
    while (!tokens_.compare_exchange_weak(current_tokens, new_tokens,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire)) {
        if (current_tokens < SCALE_FACTOR) {
            return false;  // Not enough tokens
        }
        new_tokens = current_tokens - SCALE_FACTOR;
    }

    // Check if this was a burst (more than normal rate)
    if (config_.enable_burst && current_tokens > max_tokens_.load(std::memory_order_relaxed) / 2) {
        stats_.burst_count.fetch_add(1, std::memory_order_relaxed);
    }

    return true;
}

uint64_t RateLimiter::getCurrentTimeNanos() const noexcept {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

uint64_t RateLimiter::rateToTokensPerNano(double rate) const noexcept {
    // Convert requests per second to tokens per nanosecond (scaled by SCALE_FACTOR)
    return static_cast<uint64_t>(rate * SCALE_FACTOR / 1e9);
}

void RateLimiter::adaptiveBackoff(size_t attempt) const noexcept {
    if (attempt < 10) {
        // Spin for very short waits
        for (size_t i = 0; i < attempt * 10; i = i + 1) {
            // CPU pause/yield instruction would be ideal here
            std::this_thread::yield();
        }
    } else if (attempt < 100) {
        // Short sleep for medium waits
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    } else if (attempt < 1000) {
        // Longer sleep for extended waits
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    } else {
        // Maximum backoff
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

}  // namespace atom::extra::curl
