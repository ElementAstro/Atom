/*
 * locks.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-26

Description: High-performance lock implementations for async messaging

**************************************************/

#ifndef ATOM_ASYNC_MESSAGING_LOCKS_HPP
#define ATOM_ASYNC_MESSAGING_LOCKS_HPP

#include <atomic>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <thread>

// Platform-specific pause instruction
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)
#include <emmintrin.h>
#define ATOM_CPU_PAUSE() _mm_pause()
#elif defined(__arm__) || defined(__aarch64__)
#define ATOM_CPU_PAUSE() __asm__ __volatile__("yield" ::: "memory")
#else
#define ATOM_CPU_PAUSE() std::this_thread::yield()
#endif

namespace atom::async {

/**
 * @brief High-performance spin lock implementation
 *
 * Uses atomic operations for low-contention scenarios.
 * Spins with exponential backoff for better performance.
 */
class SpinLock {
public:
    SpinLock() = default;
    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;

    void lock() noexcept {
        std::uint32_t backoff = 1;
        while (lock_.test_and_set(std::memory_order_acquire)) {
            for (std::uint32_t i = 0; i < backoff; ++i) {
                ATOM_CPU_PAUSE();
            }
            if (backoff < 1024) {
                backoff *= 2;
            } else {
                std::this_thread::yield();
            }
        }
    }

    [[nodiscard]] bool try_lock() noexcept {
        return !lock_.test_and_set(std::memory_order_acquire);
    }

    void unlock() noexcept { lock_.clear(std::memory_order_release); }

private:
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
};

/**
 * @brief Read-write lock wrapper for concurrent read access
 *
 * Allows multiple readers simultaneously, but exclusive write access.
 */
class SharedMutex {
public:
    SharedMutex() = default;
    SharedMutex(const SharedMutex&) = delete;
    SharedMutex& operator=(const SharedMutex&) = delete;

    void lock() noexcept { mutex_.lock(); }
    void unlock() noexcept { mutex_.unlock(); }
    void lock_shared() noexcept { mutex_.lock_shared(); }
    void unlock_shared() noexcept { mutex_.unlock_shared(); }
    [[nodiscard]] bool try_lock() noexcept { return mutex_.try_lock(); }
    [[nodiscard]] bool try_lock_shared() noexcept {
        return mutex_.try_lock_shared();
    }

private:
    std::shared_mutex mutex_;
};

/**
 * @brief Hybrid mutex with adaptive lock strategy
 *
 * Combines spinning and blocking approaches.
 * Spins for a short period before falling back to blocking.
 */
class HybridMutex {
public:
    static constexpr int SPIN_COUNT = 4000;

    HybridMutex() = default;
    HybridMutex(const HybridMutex&) = delete;
    HybridMutex& operator=(const HybridMutex&) = delete;

    void lock() noexcept {
        // First try spinning
        for (int i = 0; i < SPIN_COUNT; ++i) {
            if (try_lock()) {
                return;
            }
            ATOM_CPU_PAUSE();
        }
        // Fall back to blocking mutex
        mutex_.lock();
        isThreadLocked_.store(true, std::memory_order_relaxed);
    }

    [[nodiscard]] bool try_lock() noexcept {
        if (!spinLock_.test_and_set(std::memory_order_acquire)) {
            if (isThreadLocked_.load(std::memory_order_relaxed)) {
                spinLock_.clear(std::memory_order_release);
                return false;
            }
            return true;
        }
        return false;
    }

    void unlock() noexcept {
        if (isThreadLocked_.load(std::memory_order_relaxed)) {
            isThreadLocked_.store(false, std::memory_order_relaxed);
            mutex_.unlock();
        } else {
            spinLock_.clear(std::memory_order_release);
        }
    }

private:
    std::atomic_flag spinLock_ = ATOMIC_FLAG_INIT;
    std::mutex mutex_;
    std::atomic<bool> isThreadLocked_{false};
};

/**
 * @brief RAII scoped lock for any mutex type
 */
template <typename Mutex>
class ScopedLock {
public:
    explicit ScopedLock(Mutex& mutex) : mutex_(mutex) { mutex_.lock(); }
    ~ScopedLock() { mutex_.unlock(); }

    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;

private:
    Mutex& mutex_;
};

/**
 * @brief RAII shared lock for reader-writer mutexes
 */
template <typename Mutex>
class SharedLock {
public:
    explicit SharedLock(Mutex& mutex) : mutex_(mutex) { mutex_.lock_shared(); }
    ~SharedLock() { mutex_.unlock_shared(); }

    SharedLock(const SharedLock&) = delete;
    SharedLock& operator=(const SharedLock&) = delete;

private:
    Mutex& mutex_;
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_MESSAGING_LOCKS_HPP
