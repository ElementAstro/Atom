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
#include <chrono>
#include <concepts>
#include <functional>
#include <thread>

#ifdef ATOM_USE_BOOST_LOCKS
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/atomic.hpp>
#include <boost/lockfree/queue.hpp>
#endif

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
#define cpu_relax() \
    std::this_thread::yield()  // Fallback for unknown architectures
#endif

/**
 * @brief Lock concept defining the basic requirements for a lock type
 */
template <typename T>
concept Lock = requires(T lock) {
    { lock.lock() } -> std::same_as<void>;
    { lock.unlock() } -> std::same_as<void>;
};

/**
 * @brief Tryable lock concept extending Lock with try_lock capability
 */
template <typename T>
concept TryableLock = Lock<T> && requires(T lock) {
    { lock.tryLock() } -> std::same_as<bool>;
};

/**
 * @brief A simple spinlock implementation using atomic_flag with C++20
 * features.
 */
class Spinlock : public NonCopyable {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

// For deadlock detection (optional in debug builds)
#ifdef ATOM_DEBUG
    std::atomic<std::thread::id> owner_{};
#endif

public:
    /**
     * @brief Default constructor.
     */
    Spinlock() noexcept = default;

    /**
     * @brief Acquires the lock.
     * @throws std::system_error if the lock is already owned by the current
     * thread (in debug mode)
     */
    void lock();

    /**
     * @brief Releases the lock.
     * @throws std::system_error if the current thread doesn't own the lock (in
     * debug mode)
     */
    void unlock() noexcept;

    /**
     * @brief Tries to acquire the lock.
     * @return true if the lock was acquired, false otherwise.
     */
    [[nodiscard]] auto tryLock() noexcept -> bool;

    /**
     * @brief Tries to acquire the lock with timeout.
     * @param timeout Maximum time to wait
     * @return true if the lock was acquired, false otherwise.
     */
    template <class Rep, class Period>
    [[nodiscard]] auto tryLock(
        const std::chrono::duration<Rep, Period> &timeout) noexcept -> bool {
        auto start = std::chrono::steady_clock::now();
        while (!tryLock()) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                return false;
            }
            cpu_relax();
        }
        return true;
    }

    // C++20 compatible wait interfaces
    /**
     * @brief Waits until lock becomes available (C++20)
     */
    void wait() const noexcept {
#if defined(__cpp_lib_atomic_wait)
        while (flag_.test(std::memory_order_acquire)) {
            flag_.wait(true, std::memory_order_relaxed);
        }
#else
        // Fallback for compilers without wait support
        while (flag_.test(std::memory_order_acquire)) {
            cpu_relax();
        }
#endif
    }
};

/**
 * @brief A ticket spinlock implementation using atomic operations.
 * Provides fair locking with first-come-first-served ordering.
 */
class TicketSpinlock : public NonCopyable {
    std::atomic<uint64_t> ticket_{0};
    std::atomic<uint64_t> serving_{0};

    // Max spin count before yielding to prevent CPU starvation
    static constexpr uint32_t MAX_SPIN_COUNT = 1000;

public:
    /**
     * @brief Default constructor.
     */
    TicketSpinlock() noexcept = default;

    /**
     * @brief Lock guard for TicketSpinlock.
     */
    class LockGuard {
        TicketSpinlock &spinlock_;
        const uint64_t ticket_;
        bool locked_{true};

    public:
        /**
         * @brief Constructs the lock guard and acquires the lock.
         *
         * @param spinlock The TicketSpinlock to guard.
         */
        explicit LockGuard(TicketSpinlock &spinlock) noexcept
            : spinlock_(spinlock), ticket_(spinlock_.lock()) {}

        /**
         * @brief Destructs the lock guard and releases the lock.
         */
        ~LockGuard() {
            if (locked_) {
                spinlock_.unlock(ticket_);
            }
        }

        /**
         * @brief Explicitly unlocks the guarded lock.
         */
        void unlock() noexcept {
            if (locked_) {
                spinlock_.unlock(ticket_);
                locked_ = false;
            }
        }

        LockGuard(const LockGuard &) = delete;
        LockGuard &operator=(const LockGuard &) = delete;
        LockGuard(LockGuard &&) = delete;
        LockGuard &operator=(LockGuard &&) = delete;
    };

    using scoped_lock = LockGuard;

    /**
     * @brief Acquires the lock and returns the ticket number.
     *
     * @return The acquired ticket number.
     */
    [[nodiscard]] auto lock() noexcept -> uint64_t;

    /**
     * @brief Releases the lock given a specific ticket number.
     *
     * @param ticket The ticket number to release.
     * @throws std::invalid_argument if the ticket doesn't match the current
     * serving number
     */
    void unlock(uint64_t ticket);

    /**
     * @brief Tries to acquire the lock if immediately available.
     * @return true if the lock was acquired, false otherwise.
     */
    [[nodiscard]] auto tryLock() noexcept -> bool {
        auto expected = serving_.load(std::memory_order_acquire);
        if (ticket_.load(std::memory_order_acquire) == expected) {
            auto my_ticket = ticket_.fetch_add(1, std::memory_order_acq_rel);
            return my_ticket == expected;
        }
        return false;
    }
};

/**
 * @brief An unfair spinlock implementation using atomic_flag.
 * May lead to starvation but has lower overhead than fair locks.
 */
class UnfairSpinlock : public NonCopyable {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    /**
     * @brief Default constructor.
     */
    UnfairSpinlock() noexcept = default;

    /**
     * @brief Acquires the lock.
     */
    void lock() noexcept;

    /**
     * @brief Releases the lock.
     */
    void unlock() noexcept;

    /**
     * @brief Tries to acquire the lock without blocking.
     * @return true if the lock was acquired, false otherwise.
     */
    [[nodiscard]] auto tryLock() noexcept -> bool {
        return !flag_.test_and_set(std::memory_order_acquire);
    }
};

/**
 * @brief Scoped lock for any type of lock that meets the Lock concept.
 *
 * @tparam Mutex Type of the lock that satisfies the Lock concept.
 */
template <Lock Mutex>
class ScopedLock : public NonCopyable {
    Mutex &mutex_;
    bool locked_{true};

public:
    /**
     * @brief Constructs the scoped lock and acquires the lock on the provided
     * mutex.
     *
     * @param mutex The mutex to lock.
     */
    explicit ScopedLock(Mutex &mutex) noexcept(noexcept(mutex.lock()))
        : mutex_(mutex) {
        mutex_.lock();
    }

    /**
     * @brief Destructs the scoped lock and releases the lock if still held.
     */
    ~ScopedLock() noexcept(noexcept(std::declval<Mutex>().unlock())) {
        if (locked_) {
            mutex_.unlock();
        }
    }

    /**
     * @brief Explicitly unlocks the guarded mutex.
     */
    void unlock() noexcept(noexcept(std::declval<Mutex>().unlock())) {
        if (locked_) {
            mutex_.unlock();
            locked_ = false;
        }
    }

    ScopedLock(const ScopedLock &) = delete;
    ScopedLock &operator=(const ScopedLock &) = delete;
    ScopedLock(ScopedLock &&) = delete;
    ScopedLock &operator=(ScopedLock &&) = delete;
};

/**
 * @brief Scoped lock for TicketSpinlock.
 */
using ScopedTicketLock = TicketSpinlock::LockGuard;

/**
 * @brief Adaptive mutex that uses spinning for short waits and
 * blocking for longer waits to reduce CPU usage.
 */
class AdaptiveSpinlock : public NonCopyable {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
    static constexpr int SPIN_COUNT = 1000;

public:
    AdaptiveSpinlock() noexcept = default;

    void lock() noexcept {
        // First try spinning a few times
        for (int i = 0; i < SPIN_COUNT; ++i) {
            if (!flag_.test_and_set(std::memory_order_acquire)) {
                return;
            }
            cpu_relax();
        }

        // If spinning failed, yield to scheduler between attempts
        while (flag_.test_and_set(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }

    void unlock() noexcept {
        flag_.clear(std::memory_order_release);
#if defined(__cpp_lib_atomic_flag_test)
        // In C++20, we can notify waiters
        flag_.notify_one();
#endif
    }

    [[nodiscard]] auto tryLock() noexcept -> bool {
        return !flag_.test_and_set(std::memory_order_acquire);
    }
};

#ifdef ATOM_USE_BOOST_LOCKFREE
/**
 * @brief A lock optimized for high contention scenarios using boost::atomic
 *
 * This lock uses boost::atomic operations with memory ordering optimizations
 * and exponential backoff to reduce contention in high-throughput scenarios.
 */
class BoostSpinlock : public NonCopyable {
    boost::atomic<bool> flag_{false};

// For deadlock detection (optional in debug builds)
#ifdef ATOM_DEBUG
    boost::atomic<std::thread::id> owner_{};
#endif

public:
    /**
     * @brief Default constructor.
     */
    BoostSpinlock() noexcept = default;

    /**
     * @brief Acquires the lock with optimized spinning pattern.
     */
    void lock() noexcept;

    /**
     * @brief Releases the lock.
     */
    void unlock() noexcept;

    /**
     * @brief Tries to acquire the lock without blocking.
     * @return true if the lock was acquired, false otherwise.
     */
    [[nodiscard]] auto tryLock() noexcept -> bool;

    /**
     * @brief Tries to acquire the lock with timeout.
     * @param timeout Maximum time to wait
     * @return true if the lock was acquired, false otherwise.
     */
    template <class Rep, class Period>
    [[nodiscard]] auto tryLock(
        const std::chrono::duration<Rep, Period> &timeout) noexcept -> bool {
        auto start = std::chrono::steady_clock::now();
        while (!tryLock()) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                return false;
            }
            cpu_relax();
        }
        return true;
    }
};
#endif

#ifdef ATOM_USE_BOOST_LOCKS
/**
 * @brief A shared mutex wrapper around boost::shared_mutex.
 *
 * Provides both exclusive and shared locking capabilities using Boost's
 * implementation which may have better performance on some platforms.
 */
class BoostSharedMutex : public NonCopyable {
    boost::shared_mutex mutex_;

public:
    BoostSharedMutex() = default;

    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }
    bool tryLock() { return mutex_.try_lock(); }

    void lockShared() { mutex_.lock_shared(); }
    void unlockShared() { mutex_.unlock_shared(); }
    bool tryLockShared() { return mutex_.try_lock_shared(); }

    /**
     * @brief Scoped shared lock for BoostSharedMutex.
     */
    class SharedLock {
        BoostSharedMutex &mutex_;
        bool locked_{true};

    public:
        explicit SharedLock(BoostSharedMutex &mutex) : mutex_(mutex) {
            mutex_.lockShared();
        }

        ~SharedLock() {
            if (locked_) {
                mutex_.unlockShared();
            }
        }

        void unlock() {
            if (locked_) {
                mutex_.unlockShared();
                locked_ = false;
            }
        }

        SharedLock(const SharedLock &) = delete;
        SharedLock &operator=(const SharedLock &) = delete;
    };
};

/**
 * @brief A recursive mutex wrapper around boost::recursive_mutex.
 *
 * Allows the same thread to acquire the mutex multiple times without
 * deadlocking.
 */
class BoostRecursiveMutex : public NonCopyable {
    boost::recursive_mutex mutex_;

public:
    BoostRecursiveMutex() = default;

    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }
    bool tryLock() { return mutex_.try_lock(); }

    template <class Rep, class Period>
    bool tryLock(const std::chrono::duration<Rep, Period> &timeout) {
        return mutex_.try_lock_for(timeout);
    }
};

// Convenient type aliases for boost locks
template <typename Mutex>
using BoostScopedLock = boost::lock_guard<Mutex>;

template <typename Mutex>
using BoostUniqueLock = boost::unique_lock<Mutex>;
#endif

/**
 * @brief Factory to create the appropriate lock type based on configuration
 *
 * This allows runtime selection between different lock implementations
 * while maintaining a consistent interface.
 */
class LockFactory {
public:
    enum class LockType {
        SPINLOCK,
        TICKET_SPINLOCK,
        UNFAIR_SPINLOCK,
        ADAPTIVE_SPINLOCK,
#ifdef ATOM_USE_BOOST_LOCKFREE
        BOOST_SPINLOCK,
#endif
#ifdef ATOM_USE_BOOST_LOCKS
        BOOST_MUTEX,
        BOOST_RECURSIVE_MUTEX,
        BOOST_SHARED_MUTEX,
#endif
    };

    /**
     * @brief Creates a lock of the specified type wrapped in a unique_ptr
     *
     * @param type The type of lock to create
     * @return std::unique_ptr to the created lock
     * @throws std::invalid_argument if the lock type is invalid
     */
    static auto createLock(LockType type)
        -> std::unique_ptr<void, std::function<void(void *)>>;
};

}  // namespace atom::async

#endif
