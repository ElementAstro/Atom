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
#include <source_location>
#include <thread>
#include <version>

#ifdef __cpp_lib_semaphore
#include <semaphore>
#endif
#ifdef __cpp_lib_atomic_wait
#define ATOM_HAS_ATOMIC_WAIT
#endif
#ifdef __cpp_lib_atomic_flag_test
#define ATOM_HAS_ATOMIC_FLAG_TEST
#endif
#ifdef __cpp_lib_hardware_interference_size
#define ATOM_CACHE_LINE_SIZE std::hardware_destructive_interference_size
#else
#define ATOM_CACHE_LINE_SIZE 64
#endif

// Platform-specific includes
#if defined(_WIN32) || defined(_WIN64)
#define ATOM_PLATFORM_WINDOWS
#include <synchapi.h>
#include <windows.h>
#elif defined(__APPLE__)
#define ATOM_PLATFORM_MACOS
#include <dispatch/dispatch.h>
#include <libkern/OSAtomic.h>
#include <os/lock.h>
#elif defined(__linux__)
#define ATOM_PLATFORM_LINUX
#include <linux/futex.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

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

// Architecture-specific CPU relax instruction optimization
#if defined(_MSC_VER)
#include <intrin.h>
#define cpu_relax() _mm_pause()
#elif defined(__i386__) || defined(__x86_64__)
#define cpu_relax() asm volatile("pause\n" : : : "memory")
#elif defined(__aarch64__)
#define cpu_relax() asm volatile("yield\n" : : : "memory")
#elif defined(__arm__)
#define cpu_relax() asm volatile("yield\n" : : : "memory")
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#define cpu_relax() asm volatile("or 27,27,27\n" : : : "memory")
#else
#define cpu_relax() std::this_thread::yield()  // Fallback for unknown architectures
#endif

/**
 * @brief Lock concept, defines the basic requirements for a lock type
 */
template <typename T>
concept Lock = requires(T lock) {
    { lock.lock() } -> std::same_as<void>;
    { lock.unlock() } -> std::same_as<void>;
};

/**
 * @brief TryableLock concept, extends Lock with tryLock capability
 */
template <typename T>
concept TryableLock = Lock<T> && requires(T lock) {
    { lock.tryLock() } -> std::same_as<bool>;
};

/**
 * @brief SharedLock concept, defines the basic requirements for a shared lock
 */
template <typename T>
concept SharedLock = Lock<T> && requires(T lock) {
    { lock.lockShared() } -> std::same_as<void>;
    { lock.unlockShared() } -> std::same_as<void>;
};

/**
 * @brief Error handling utility class for lock exceptions
 */
class LockError : public std::runtime_error {
public:
    explicit LockError(
        const std::string &message,
        std::source_location loc = std::source_location::current())
        : std::runtime_error(std::string(message) + " [" + loc.file_name() +
                             ":" + std::to_string(loc.line()) + " in " +
                             loc.function_name() + "]") {}
};

// A cache line padding helper class to avoid false sharing
template <typename T>
struct alignas(ATOM_CACHE_LINE_SIZE) CacheAligned {
    T value;

    CacheAligned() noexcept = default;
    explicit CacheAligned(const T &v) noexcept : value(v) {}

    operator T &() noexcept { return value; }
    operator const T &() const noexcept { return value; }

    T *operator&() noexcept { return &value; }
    const T *operator&() const noexcept { return &value; }

    T *operator->() noexcept { return &value; }
    const T *operator->() const noexcept { return &value; }
};

/**
 * @brief Simple spinlock implementation using atomic_flag with C++20 features
 */
class Spinlock : public NonCopyable {
    alignas(ATOM_CACHE_LINE_SIZE) std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

// For deadlock detection (optional in debug builds)
#ifdef ATOM_DEBUG
    std::atomic<std::thread::id> owner_{};
#endif

public:
    /**
     * @brief Default constructor
     */
    Spinlock() noexcept = default;

    /**
     * @brief Acquires the lock
     * @throws std::system_error if the current thread already owns the lock (in debug mode)
     */
    void lock();

    /**
     * @brief Releases the lock
     * @throws std::system_error if the current thread does not own the lock (in debug mode)
     */
    void unlock() noexcept;

    /**
     * @brief Tries to acquire the lock
     * @return true if the lock was acquired, false otherwise
     */
    [[nodiscard]] auto tryLock() noexcept -> bool;

    /**
     * @brief Tries to acquire the lock with a timeout
     * @param timeout Maximum duration to wait
     * @return true if the lock was acquired, false otherwise
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

    // C++20 compatible wait interface
    /**
     * @brief Waits until the lock becomes available (C++20)
     */
    void wait() const noexcept {
#ifdef ATOM_HAS_ATOMIC_WAIT
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

    /**
     * @brief Gets the thread ID currently owning the lock (debug mode only)
     * @return Thread ID or default value if no thread owns the lock or not in debug mode
     */
    [[nodiscard]] std::thread::id owner() const noexcept {
#ifdef ATOM_DEBUG
        return owner_.load(std::memory_order_relaxed);
#else
        return {};
#endif
    }
};

/**
 * @brief Ticket spinlock implementation using atomic operations
 * Provides fair locking in first-come, first-served order
 */
class TicketSpinlock : public NonCopyable {
    alignas(ATOM_CACHE_LINE_SIZE) std::atomic<uint64_t> ticket_{0};
    alignas(ATOM_CACHE_LINE_SIZE) std::atomic<uint64_t> serving_{0};

    // Maximum spin count before yielding the CPU to prevent excessive CPU usage
    static constexpr uint32_t MAX_SPIN_COUNT = 1000;

public:
    /**
     * @brief Default constructor
     */
    TicketSpinlock() noexcept = default;

    /**
     * @brief Lock guard for TicketSpinlock
     */
    class LockGuard {
        TicketSpinlock &spinlock_;
        const uint64_t ticket_;
        bool locked_{true};

    public:
        /**
         * @brief Constructs the lock guard and acquires the lock
         * @param spinlock The TicketSpinlock to guard
         */
        explicit LockGuard(TicketSpinlock &spinlock) noexcept
            : spinlock_(spinlock), ticket_(spinlock_.lock()) {}

        /**
         * @brief Destructs the lock guard and releases the lock
         */
        ~LockGuard() {
            if (locked_) {
                spinlock_.unlock(ticket_);
            }
        }

        /**
         * @brief Explicitly unlocks the guarded lock
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
     * @brief Acquires the lock and returns the ticket number
     * @return The acquired ticket number
     */
    [[nodiscard]] auto lock() noexcept -> uint64_t;

    /**
     * @brief Releases the lock using a specific ticket number
     * @param ticket The ticket number to release
     * @throws std::invalid_argument if the ticket does not match the current serving number
     */
    void unlock(uint64_t ticket);

    /**
     * @brief Tries to acquire the lock if immediately available
     * @return true if the lock was acquired, false otherwise
     */
    [[nodiscard]] auto tryLock() noexcept -> bool {
        auto expected = serving_.load(std::memory_order_acquire);
        if (ticket_.load(std::memory_order_acquire) == expected) {
            auto my_ticket = ticket_.fetch_add(1, std::memory_order_acq_rel);
            return my_ticket == expected;
        }
        return false;
    }

    /**
     * @brief Returns the number of threads currently waiting to acquire the lock
     * @return The number of waiting threads
     */
    [[nodiscard]] auto waitingThreads() const noexcept -> uint64_t {
        return ticket_.load(std::memory_order_acquire) -
               serving_.load(std::memory_order_acquire);
    }
};

/**
 * @brief Unfair spinlock implementation using atomic_flag
 * May cause starvation but has lower overhead than fair locks
 */
class UnfairSpinlock : public NonCopyable {
    alignas(ATOM_CACHE_LINE_SIZE) std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    /**
     * @brief Default constructor
     */
    UnfairSpinlock() noexcept = default;

    /**
     * @brief Acquires the lock
     */
    void lock() noexcept;

    /**
     * @brief Releases the lock
     */
    void unlock() noexcept;

    /**
     * @brief Tries to acquire the lock without blocking
     * @return true if the lock was acquired, false otherwise
     */
    [[nodiscard]] auto tryLock() noexcept -> bool {
        return !flag_.test_and_set(std::memory_order_acquire);
    }
};

/**
 * @brief Scoped lock for any lock type satisfying the Lock concept
 * @tparam Mutex The lock type satisfying the Lock concept
 */
template <Lock Mutex>
class ScopedLock : public NonCopyable {
    Mutex &mutex_;
    bool locked_{true};

public:
    /**
     * @brief Constructs the scoped lock and acquires the provided mutex
     * @param mutex The mutex to lock
     */
    explicit ScopedLock(Mutex &mutex) noexcept(noexcept(mutex.lock()))
        : mutex_(mutex) {
        mutex_.lock();
    }

    /**
     * @brief Destructs the scoped lock and releases the lock if still held
     */
    ~ScopedLock() noexcept(noexcept(std::declval<Mutex>().unlock())) {
        if (locked_) {
            mutex_.unlock();
        }
    }

    /**
     * @brief Explicitly unlocks the guarded mutex
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
 * @brief Scoped lock for TicketSpinlock
 */
using ScopedTicketLock = TicketSpinlock::LockGuard;

/**
 * @brief Adaptive mutex that spins for short waits and blocks for longer waits to reduce CPU usage
 */
class AdaptiveSpinlock : public NonCopyable {
    alignas(ATOM_CACHE_LINE_SIZE) std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
    static constexpr int SPIN_COUNT = 1000;

public:
    AdaptiveSpinlock() noexcept = default;

    void lock() noexcept {
        // Try spinning a few times first
        for (int i = 0; i < SPIN_COUNT; ++i) {
            if (!flag_.test_and_set(std::memory_order_acquire)) {
                return;
            }
            cpu_relax();
        }

        // If spinning fails, yield to the scheduler between attempts
        while (flag_.test_and_set(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }

    void unlock() noexcept {
        flag_.clear(std::memory_order_release);
#ifdef ATOM_HAS_ATOMIC_FLAG_TEST
        // In C++20, we can notify waiters
        flag_.notify_one();
#endif
    }

    [[nodiscard]] auto tryLock() noexcept -> bool {
        return !flag_.test_and_set(std::memory_order_acquire);
    }
};

// Platform-specific lock implementations
#ifdef ATOM_PLATFORM_WINDOWS
/**
 * @brief Windows platform-specific spinlock implementation
 * Uses Windows critical sections with spin count optimization
 */
class WindowsSpinlock : public NonCopyable {
    CRITICAL_SECTION cs_;

public:
    WindowsSpinlock() noexcept {
        // Set spin count to an optimal value to reduce kernel context switches
        InitializeCriticalSectionAndSpinCount(&cs_, 4000);
    }

    ~WindowsSpinlock() noexcept { DeleteCriticalSection(&cs_); }

    void lock() noexcept { EnterCriticalSection(&cs_); }

    void unlock() noexcept { LeaveCriticalSection(&cs_); }

    [[nodiscard]] auto tryLock() noexcept -> bool {
        return TryEnterCriticalSection(&cs_) != 0;
    }
};

/**
 * @brief Windows platform-specific shared mutex based on SRW locks
 */
class WindowsSharedMutex : public NonCopyable {
    SRWLOCK srwlock_ = SRWLOCK_INIT;

public:
    WindowsSharedMutex() noexcept = default;

    void lock() noexcept { AcquireSRWLockExclusive(&srwlock_); }

    void unlock() noexcept { ReleaseSRWLockExclusive(&srwlock_); }

    [[nodiscard]] auto tryLock() noexcept -> bool {
        return TryAcquireSRWLockExclusive(&srwlock_) != 0;
    }

    void lockShared() noexcept { AcquireSRWLockShared(&srwlock_); }

    void unlockShared() noexcept { ReleaseSRWLockShared(&srwlock_); }

    [[nodiscard]] auto tryLockShared() noexcept -> bool {
        return TryAcquireSRWLockShared(&srwlock_) != 0;
    }
};
#endif

#ifdef ATOM_PLATFORM_MACOS
/**
 * @brief macOS platform-specific spinlock implementation
 * Uses optimized OSSpinLock (before 10.12) or os_unfair_lock (10.12+)
 */
class DarwinSpinlock : public NonCopyable {
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101200
    OSSpinLock spinlock_ = OS_SPINLOCK_INIT;
#else
    os_unfair_lock unfairlock_ = OS_UNFAIR_LOCK_INIT;
#endif

public:
    DarwinSpinlock() noexcept = default;

    void lock() noexcept {
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101200
        OSSpinLockLock(&spinlock_);
#else
        os_unfair_lock_lock(&unfairlock_);
#endif
    }

    void unlock() noexcept {
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101200
        OSSpinLockUnlock(&spinlock_);
#else
        os_unfair_lock_unlock(&unfairlock_);
#endif
    }

    [[nodiscard]] auto tryLock() noexcept -> bool {
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101200
        return OSSpinLockTry(&spinlock_);
#else
        return os_unfair_lock_trylock(&unfairlock_);
#endif
    }
};
#endif

#ifdef ATOM_PLATFORM_LINUX
/**
 * @brief Linux platform-specific spinlock implementation
 * Uses futex system call for optimized long waits
 */
class LinuxFutexLock : public NonCopyable {
    // 0=unlocked, 1=locked, 2=contended (waiters exist)
    alignas(ATOM_CACHE_LINE_SIZE) std::atomic<int> state_{0};

    // futex system call wrapper
    static int futex(int *uaddr, int futex_op, int val,
                     const struct timespec *timeout = nullptr,
                     int *uaddr2 = nullptr, int val3 = 0) {
        return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
    }

public:
    LinuxFutexLock() noexcept = default;

    void lock() noexcept {
        // Try fast path: acquire lock uncontended
        int expected = 0;
        if (state_.compare_exchange_strong(expected, 1,
                                           std::memory_order_acquire,
                                           std::memory_order_relaxed)) {
            return;
        }

        // Contended path: potentially use futex wait
        int spins = 0;
        while (true) {
            // Spin briefly first
            if (spins < 100) {
                for (int i = 0; i < 10; ++i) {
                    cpu_relax();
                }
                spins++;

                // Check lock state again after spinning
                expected = 0;
                if (state_.compare_exchange_strong(expected, 1,
                                                   std::memory_order_acquire,
                                                   std::memory_order_relaxed)) {
                    return;
                }

                continue;
            }

            // Set state to contended (2)
            int current = state_.load(std::memory_order_relaxed);
            if (current == 0) {
                // State is 0, try to acquire the lock
                expected = 0;
                if (state_.compare_exchange_strong(expected, 1,
                                                   std::memory_order_acquire,
                                                   std::memory_order_relaxed)) {
                    return;
                }

                continue;
            }

            // Try to update state from 1 to 2, indicating someone is waiting
            if (current == 1 && state_.compare_exchange_strong(
                                    current, 2, std::memory_order_relaxed)) {
                // Call futex wait
                futex(reinterpret_cast<int *>(&state_), FUTEX_WAIT_PRIVATE, 2);
            }
        }
    }

    void unlock() noexcept {
        // Set state to 0 if no waiters
        int previous = state_.exchange(0, std::memory_order_release);

        // If there were waiters (state was 2), wake one up
        if (previous == 2) {
            futex(reinterpret_cast<int *>(&state_), FUTEX_WAKE_PRIVATE, 1);
        }
    }

    [[nodiscard]] auto tryLock() noexcept -> bool {
        int expected = 0;
        return state_.compare_exchange_strong(
            expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
    }
};
#endif

#ifdef ATOM_HAS_ATOMIC_WAIT
/**
 * @brief Spinlock implementation using C++20 atomic wait/notify
 * More efficient than plain spinlocks if supported by hardware
 */
class AtomicWaitLock : public NonCopyable {
    alignas(ATOM_CACHE_LINE_SIZE) std::atomic<bool> locked_{false};

public:
    AtomicWaitLock() noexcept = default;

    void lock() noexcept {
        bool expected = false;
        // Fast path: acquire lock uncontended
        if (locked_.compare_exchange_strong(expected, true,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed)) {
            return;
        }

        // Slow path: use atomic wait
        while (true) {
            expected = false;
            // Try acquiring the lock first
            if (locked_.compare_exchange_strong(expected, true,
                                                std::memory_order_acquire,
                                                std::memory_order_relaxed)) {
                return;
            }

            // If failed, wait for the value to change
            locked_.wait(true, std::memory_order_relaxed);
        }
    }

    void unlock() noexcept {
        locked_.store(false, std::memory_order_release);
        locked_.notify_one();
    }

    [[nodiscard]] auto tryLock() noexcept -> bool {
        bool expected = false;
        return locked_.compare_exchange_strong(expected, true,
                                               std::memory_order_acquire,
                                               std::memory_order_relaxed);
    }
};
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
/**
 * @brief Lock optimized for high contention scenarios using boost::atomic
 *
 * This lock uses boost::atomic operations and memory order optimizations
 * along with exponential backoff to reduce contention in high-throughput scenarios.
 */
class BoostSpinlock : public NonCopyable {
    alignas(ATOM_CACHE_LINE_SIZE) boost::atomic<bool> flag_{false};

// For deadlock detection (optional in debug builds)
#ifdef ATOM_DEBUG
    boost::atomic<std::thread::id> owner_{};
#endif

public:
    /**
     * @brief Default constructor
     */
    BoostSpinlock() noexcept = default;

    /**
     * @brief Acquires the lock using an optimized spinning pattern
     */
    void lock() noexcept;

    /**
     * @brief Releases the lock
     */
    void unlock() noexcept;

    /**
     * @brief Tries to acquire the lock without blocking
     * @return true if the lock was acquired, false otherwise
     */
    [[nodiscard]] auto tryLock() noexcept -> bool;

    /**
     * @brief Tries to acquire the lock with a timeout
     * @param timeout Maximum duration to wait
     * @return true if the lock was acquired, false otherwise
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
 * @brief Wrapper around boost::shared_mutex
 *
 * Provides exclusive and shared locking capabilities using the Boost implementation,
 * which might offer better performance on some platforms.
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
     * @brief Shared lock for BoostSharedMutex
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
 * @brief Wrapper around boost::recursive_mutex
 *
 * Allows the same thread to acquire the mutex multiple times without deadlocking.
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

// Convenience type aliases for Boost locks
template <typename Mutex>
using BoostScopedLock = boost::lock_guard<Mutex>;

template <typename Mutex>
using BoostUniqueLock = boost::unique_lock<Mutex>;
#endif

/**
 * @brief Optional alternative implementation for C++20 std::counting_semaphore
 * Uses a custom implementation when standard library support is unavailable.
 */
template <std::ptrdiff_t LeastMaxValue = 1>
class CountingSemaphore {
#ifdef __cpp_lib_semaphore
    std::counting_semaphore<LeastMaxValue> sem_;
#else
    // Fallback implementation when std::counting_semaphore is not available
    std::mutex mutex_;
    std::condition_variable cv_;
    std::ptrdiff_t count_ = 0;
#endif

public:
    static constexpr std::ptrdiff_t max() noexcept {
#ifdef __cpp_lib_semaphore
        return std::counting_semaphore<LeastMaxValue>::max();
#else
        return std::numeric_limits<std::ptrdiff_t>::max();
#endif
    }

    explicit CountingSemaphore(std::ptrdiff_t initial = 0) noexcept
#ifdef __cpp_lib_semaphore
        : sem_(initial)
#endif
    {
#ifndef __cpp_lib_semaphore
        count_ = initial;
#endif
    }

    CountingSemaphore(const CountingSemaphore &) = delete;
    CountingSemaphore &operator=(const CountingSemaphore &) = delete;

    void release(std::ptrdiff_t update = 1) {
#ifdef __cpp_lib_semaphore
        sem_.release(update);
#else
        std::lock_guard<std::mutex> lock(mutex_);
        count_ += update;
        if (update == 1) {
            cv_.notify_one();
        } else {
            cv_.notify_all();
        }
#endif
    }

    void acquire() {
#ifdef __cpp_lib_semaphore
        sem_.acquire();
#else
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return count_ > 0; });
        count_--;
#endif
    }

    bool try_acquire() noexcept {
#ifdef __cpp_lib_semaphore
        return sem_.try_acquire();
#else
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ > 0) {
            count_--;
            return true;
        }
        return false;
#endif
    }

    template <class Rep, class Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period> &rel_time) {
#ifdef __cpp_lib_semaphore
        return sem_.try_acquire_for(rel_time);
#else
        std::unique_lock<std::mutex> lock(mutex_);
        if (cv_.wait_for(lock, rel_time, [this] { return count_ > 0; })) {
            count_--;
            return true;
        }
        return false;
#endif
    }
};

/**
 * @brief Binary semaphore - a special case of CountingSemaphore
 */
using BinarySemaphore = CountingSemaphore<1>;

/**
 * @brief Factory for creating appropriate lock types based on configuration
 *
 * Allows selecting different lock implementations at runtime while maintaining
 * a consistent interface.
 */
class LockFactory {
public:
    enum class LockType {
        SPINLOCK,
        TICKET_SPINLOCK,
        UNFAIR_SPINLOCK,
        ADAPTIVE_SPINLOCK,
#ifdef ATOM_HAS_ATOMIC_WAIT
        ATOMIC_WAIT_LOCK,
#endif
#ifdef ATOM_PLATFORM_WINDOWS
        WINDOWS_SPINLOCK,
        WINDOWS_SHARED_MUTEX,
#endif
#ifdef ATOM_PLATFORM_MACOS
        DARWIN_SPINLOCK,
#endif
#ifdef ATOM_PLATFORM_LINUX
        LINUX_FUTEX_LOCK,
#endif
#ifdef ATOM_USE_BOOST_LOCKFREE
        BOOST_SPINLOCK,
#endif
#ifdef ATOM_USE_BOOST_LOCKS
        BOOST_MUTEX,
        BOOST_RECURSIVE_MUTEX,
        BOOST_SHARED_MUTEX,
#endif
        // Standard library locks
        STD_MUTEX,
        STD_RECURSIVE_MUTEX,
        STD_SHARED_MUTEX,

        // Automatically select the best lock
        AUTO_OPTIMIZED
    };

    /**
     * @brief Creates a lock of the specified type, wrapped in a unique_ptr
     *
     * @param type The type of lock to create
     * @return A std::unique_ptr to the created lock
     * @throws std::invalid_argument if the lock type is invalid
     */
    static auto createLock(LockType type)
        -> std::unique_ptr<void, std::function<void(void *)>>;

    /**
     * @brief Creates the most optimal lock implementation for the platform
     *
     * @return A std::unique_ptr to the lock optimized for the current platform
     */
    static auto createOptimizedLock()
        -> std::unique_ptr<void, std::function<void(void *)>>;
};

}  // namespace atom::async

#endif // ATOM_ASYNC_LOCK_HPP
