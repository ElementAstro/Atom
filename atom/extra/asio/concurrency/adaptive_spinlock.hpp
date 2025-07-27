#pragma once

/**
 * @file adaptive_spinlock.hpp
 * @brief High-performance adaptive spinlock with exponential backoff and CPU pause optimization
 */

#include <atomic>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
#include "../asio_compatibility.hpp"

namespace atom::extra::asio::concurrency {

/**
 * @brief Adaptive spinlock with exponential backoff for optimal performance
 *
 * This spinlock implementation adapts its behavior based on contention levels,
 * using CPU pause instructions for short waits and yielding for longer waits.
 */
class adaptive_spinlock {
private:
    cache_aligned<std::atomic<bool>> locked_{false};

    // Backoff parameters
    static constexpr std::size_t initial_pause_count = 4;
    static constexpr std::size_t max_pause_count = 64;
    static constexpr std::size_t yield_threshold = 128;
    static constexpr std::chrono::microseconds sleep_threshold{100};

public:
    /**
     * @brief Construct an unlocked adaptive spinlock
     */
    adaptive_spinlock() = default;

    // Non-copyable, non-movable
    adaptive_spinlock(const adaptive_spinlock&) = delete;
    adaptive_spinlock& operator=(const adaptive_spinlock&) = delete;
    adaptive_spinlock(adaptive_spinlock&&) = delete;
    adaptive_spinlock& operator=(adaptive_spinlock&&) = delete;

    /**
     * @brief Acquire the lock with adaptive backoff strategy
     */
    void lock() noexcept {
        std::size_t pause_count = initial_pause_count;
        std::size_t iteration = 0;

        while (true) {
            // Fast path: try to acquire immediately
            if (!locked_.get().exchange(true, std::memory_order_acquire)) {
                if (iteration > 0) {
                    spdlog::trace("Adaptive spinlock acquired after {} iterations", iteration);
                }
                return;
            }

            // Adaptive backoff strategy
            if (iteration < yield_threshold) {
                // Phase 1: CPU pause with exponential backoff
                for (std::size_t i = 0; i < pause_count; ++i) {
                    cpu_pause();
                }

                // Exponential backoff up to maximum
                if (pause_count < max_pause_count) {
                    pause_count *= 2;
                }
            } else if (iteration < yield_threshold * 2) {
                // Phase 2: Yield to other threads
                std::this_thread::yield();
            } else {
                // Phase 3: Brief sleep for heavily contended locks
                std::this_thread::sleep_for(sleep_threshold);

                if (iteration % 1000 == 0) {
                    spdlog::warn("Adaptive spinlock heavily contended, iteration: {}", iteration);
                }
            }

            ++iteration;
        }
    }

    /**
     * @brief Try to acquire the lock without blocking
     * @return True if lock was acquired, false otherwise
     */
    bool try_lock() noexcept {
        bool acquired = !locked_.get().exchange(true, std::memory_order_acquire);
        if (acquired) {
            spdlog::trace("Adaptive spinlock acquired via try_lock");
        }
        return acquired;
    }

    /**
     * @brief Release the lock
     */
    void unlock() noexcept {
        locked_.get().store(false, std::memory_order_release);
        spdlog::trace("Adaptive spinlock released");
    }

    /**
     * @brief Check if the lock is currently held
     * @return True if locked, false otherwise
     */
    bool is_locked() const noexcept {
        return locked_.get().load(std::memory_order_acquire);
    }
};

/**
 * @brief RAII lock guard for adaptive spinlock
 */
class adaptive_lock_guard {
private:
    adaptive_spinlock& lock_;

public:
    /**
     * @brief Construct and acquire the lock
     */
    explicit adaptive_lock_guard(adaptive_spinlock& lock) : lock_(lock) {
        lock_.lock();
    }

    /**
     * @brief Destructor releases the lock
     */
    ~adaptive_lock_guard() {
        lock_.unlock();
    }

    // Non-copyable, non-movable
    adaptive_lock_guard(const adaptive_lock_guard&) = delete;
    adaptive_lock_guard& operator=(const adaptive_lock_guard&) = delete;
    adaptive_lock_guard(adaptive_lock_guard&&) = delete;
    adaptive_lock_guard& operator=(adaptive_lock_guard&&) = delete;
};

/**
 * @brief Reader-writer spinlock with priority inheritance
 *
 * Optimized for scenarios with many readers and few writers,
 * providing excellent read performance while ensuring writer fairness.
 */
class reader_writer_spinlock {
private:
    cache_aligned<std::atomic<std::int32_t>> state_{0};

    // State encoding: positive = reader count, -1 = writer, 0 = unlocked
    static constexpr std::int32_t writer_flag = -1;
    static constexpr std::int32_t max_readers = std::numeric_limits<std::int32_t>::max();

public:
    /**
     * @brief Construct an unlocked reader-writer spinlock
     */
    reader_writer_spinlock() = default;

    // Non-copyable, non-movable
    reader_writer_spinlock(const reader_writer_spinlock&) = delete;
    reader_writer_spinlock& operator=(const reader_writer_spinlock&) = delete;
    reader_writer_spinlock(reader_writer_spinlock&&) = delete;
    reader_writer_spinlock& operator=(reader_writer_spinlock&&) = delete;

    /**
     * @brief Acquire read lock
     */
    void lock_shared() noexcept {
        std::size_t iteration = 0;

        while (true) {
            std::int32_t current = state_.get().load(std::memory_order_acquire);

            // Can acquire read lock if no writer and not at max readers
            if (current >= 0 && current < max_readers) {
                if (state_.get().compare_exchange_weak(current, current + 1,
                                                      std::memory_order_acquire)) {
                    spdlog::trace("Reader lock acquired, reader count: {}", current + 1);
                    return;
                }
            }

            // Adaptive backoff for readers
            if (iteration < 32) {
                cpu_pause();
            } else {
                std::this_thread::yield();
            }

            ++iteration;
        }
    }

    /**
     * @brief Release read lock
     */
    void unlock_shared() noexcept {
        std::int32_t prev = state_.get().fetch_sub(1, std::memory_order_release);
        spdlog::trace("Reader lock released, reader count: {}", prev - 1);
    }

    /**
     * @brief Acquire write lock
     */
    void lock() noexcept {
        std::size_t iteration = 0;

        while (true) {
            std::int32_t expected = 0;
            if (state_.get().compare_exchange_weak(expected, writer_flag,
                                                  std::memory_order_acquire)) {
                spdlog::trace("Writer lock acquired");
                return;
            }

            // Adaptive backoff for writers
            if (iteration < 16) {
                cpu_pause();
            } else if (iteration < 64) {
                std::this_thread::yield();
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }

            ++iteration;
        }
    }

    /**
     * @brief Release write lock
     */
    void unlock() noexcept {
        state_.get().store(0, std::memory_order_release);
        spdlog::trace("Writer lock released");
    }

    /**
     * @brief Try to acquire read lock without blocking
     */
    bool try_lock_shared() noexcept {
        std::int32_t current = state_.get().load(std::memory_order_acquire);

        if (current >= 0 && current < max_readers) {
            return state_.get().compare_exchange_strong(current, current + 1,
                                                       std::memory_order_acquire);
        }

        return false;
    }

    /**
     * @brief Try to acquire write lock without blocking
     */
    bool try_lock() noexcept {
        std::int32_t expected = 0;
        return state_.get().compare_exchange_strong(expected, writer_flag,
                                                   std::memory_order_acquire);
    }
};

/**
 * @brief RAII shared lock guard for reader-writer spinlock
 */
class shared_lock_guard {
private:
    reader_writer_spinlock& lock_;

public:
    explicit shared_lock_guard(reader_writer_spinlock& lock) : lock_(lock) {
        lock_.lock_shared();
    }

    ~shared_lock_guard() {
        lock_.unlock_shared();
    }

    // Non-copyable, non-movable
    shared_lock_guard(const shared_lock_guard&) = delete;
    shared_lock_guard& operator=(const shared_lock_guard&) = delete;
    shared_lock_guard(shared_lock_guard&&) = delete;
    shared_lock_guard& operator=(shared_lock_guard&&) = delete;
};

} // namespace atom::extra::asio::concurrency
