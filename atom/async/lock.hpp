/*
 * lock.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-13

Description: Some useful spinlock implementations

**************************************************/

#ifndef ATOM_ASYNC_LOCK_HPP
#define ATOM_ASYNC_LOCK_HPP

#include <atomic>

#include "atom/type/noncopyable.hpp"

namespace atom::async {

// Pause instruction to prevent excess processor bus usage
#if defined(_MSC_VER)
#define cpu_relax() std::this_thread::yield()
#elif defined(__i386__) || defined(__x86_64__)
#define cpu_relax() asm volatile("pause\n" : : : "memory")
#elif defined(__aarch64__)
#define cpu_relax() asm volatile("yield\n" : : : "memory")
#elif defined(__arm__)
#define cpu_relax() asm volatile("nop\n" : : : "memory")
#else
#error "Unknown architecture, CPU relax code required"
#endif

/**
 * @brief A simple spinlock implementation using atomic_flag.
 */
class Spinlock : public NonCopyable {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    /**
     * @brief Default constructor.
     */
    Spinlock() = default;

    /**
     * @brief Acquires the lock.
     */
    void lock();

    /**
     * @brief Releases the lock.
     */
    void unlock();

    /**
     * @brief Tries to acquire the lock.
     *
     * @return true if the lock was acquired, false otherwise.
     */
    auto tryLock() -> bool;
};

/**
 * @brief A ticket spinlock implementation using atomic operations.
 */
class TicketSpinlock : public NonCopyable {
    std::atomic<uint64_t> ticket_{0};
    std::atomic<uint64_t> serving_{0};

public:
    TicketSpinlock() = default;
    /**
     * @brief Lock guard for TicketSpinlock.
     */
    class LockGuard {
        TicketSpinlock &spinlock_;
        const uint64_t TICKET;

    public:
        /**
         * @brief Constructs the lock guard and acquires the lock.
         *
         * @param spinlock The TicketSpinlock to guard.
         */
        explicit LockGuard(TicketSpinlock &spinlock)
            : spinlock_(spinlock), TICKET(spinlock_.lock()) {}

        /**
         * @brief Destructs the lock guard and releases the lock.
         */
        ~LockGuard() { spinlock_.unlock(TICKET); }
    };

    using scoped_lock = LockGuard;

    /**
     * @brief Acquires the lock and returns the ticket number.
     *
     * @return The acquired ticket number.
     */
    auto lock() -> uint64_t;

    /**
     * @brief Releases the lock given a specific ticket number.
     *
     * @param ticket The ticket number to release.
     */
    void unlock(uint64_t TICKET);
};

/**
 * @brief An unfair spinlock implementation using atomic_flag.
 */
class UnfairSpinlock : public NonCopyable {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    UnfairSpinlock() = default;
    /**
     * @brief Acquires the lock.
     */
    void lock();

    /**
     * @brief Releases the lock.
     */
    void unlock();
};

/**
 * @brief Scoped lock for any type of spinlock.
 *
 * @tparam Mutex Type of the spinlock (e.g., Spinlock, TicketSpinlock,
 * UnfairSpinlock).
 */
template <typename Mutex>
class ScopedLock {
    Mutex &mutex_;

public:
    /**
     * @brief Constructs the scoped lock and acquires the lock on the provided
     * mutex.
     *
     * @param mutex The mutex to lock.
     */
    explicit ScopedLock(Mutex &mutex) : mutex_(mutex) { mutex_.lock(); }

    /**
     * @brief Destructs the scoped lock and releases the lock.
     */
    ~ScopedLock() { mutex_.unlock(); }

    ScopedLock(const ScopedLock &) = delete;
    ScopedLock &operator=(const ScopedLock &) = delete;
};

/**
 * @brief Scoped lock for TicketSpinlock.
 *
 * @tparam Mutex Type of the spinlock (i.e., TicketSpinlock).
 */
template <typename Mutex>
class ScopedTicketLock : public NonCopyable {
    Mutex &mutex_;
    const uint64_t TICKET;

public:
    /**
     * @brief Constructs the scoped lock and acquires the lock on the provided
     * mutex.
     *
     * @param mutex The mutex to lock.
     */
    explicit ScopedTicketLock(Mutex &mutex)
        : mutex_(mutex), TICKET(mutex_.lock()) {}

    /**
     * @brief Destructs the scoped lock and releases the lock.
     */
    ~ScopedTicketLock() { mutex_.unlock(TICKET); }
};

/**
 * @brief Scoped lock for UnfairSpinlock.
 *
 * @tparam Mutex Type of the spinlock (i.e., UnfairSpinlock).
 */
template <typename Mutex>
class ScopedUnfairLock : public NonCopyable {
    Mutex &mutex_;

public:
    /**
     * @brief Constructs the scoped lock and acquires the lock on the provided
     * mutex.
     *
     * @param mutex The mutex to lock.
     */
    explicit ScopedUnfairLock(Mutex &mutex) : mutex_(mutex) { mutex_.lock(); }

    /**
     * @brief Destructs the scoped lock and releases the lock.
     */
    ~ScopedUnfairLock() { mutex_.unlock(); }
};

}  // namespace atom::async

#endif
