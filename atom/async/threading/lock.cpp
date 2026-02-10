/*
 * lock.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-13

Description: Some useful spinlock implementations

**************************************************/

#include "lock.hpp"

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>

namespace atom::async {

void Spinlock::lock() {
#ifdef ATOM_DEBUG
    // Check for recursive lock attempts in debug mode
    std::thread::id current_id = std::this_thread::get_id();
    std::thread::id no_thread;
    if (owner_.load(std::memory_order_relaxed) == current_id) {
        throw std::system_error(
            std::make_error_code(std::errc::resource_deadlock_would_occur),
            "Recursive lock attempt detected");
    }
#endif

    // Fast path first - single attempt
    if (!flag_.test_and_set(std::memory_order_acquire)) {
#ifdef ATOM_DEBUG
        owner_.store(current_id, std::memory_order_relaxed);
#endif
        return;
    }

    // Slow path - use exponential backoff utility
    exponentialBackoffSpin([this]() noexcept {
        bool acquired = !flag_.test_and_set(std::memory_order_acquire);
#ifdef ATOM_DEBUG
        if (acquired) {
            owner_.store(std::this_thread::get_id(), std::memory_order_relaxed);
        }
#endif
        return acquired;
    });
}

auto Spinlock::tryLock() noexcept -> bool {
    bool success = !flag_.test_and_set(std::memory_order_acquire);

#ifdef ATOM_DEBUG
    if (success) {
        owner_.store(std::this_thread::get_id(), std::memory_order_relaxed);
    }
#endif

    return success;
}

void Spinlock::unlock() noexcept {
#ifdef ATOM_DEBUG
    std::thread::id current_id = std::this_thread::get_id();
    if (owner_.load(std::memory_order_relaxed) != current_id) {
        // Log error instead of throwing from noexcept function
        std::terminate();  // Terminate in case of lock violation in debug mode
    }
    owner_.store(std::thread::id(), std::memory_order_relaxed);
#endif

    flag_.clear(std::memory_order_release);

#if defined(__cpp_lib_atomic_flag_test)
    // Use C++20's notify to wake waiting threads
    flag_.notify_one();
#endif
}

auto TicketSpinlock::lock() noexcept -> uint64_t {
    const auto ticket = ticket_.fetch_add(1, std::memory_order_acq_rel);
    auto current_serving = serving_.load(std::memory_order_acquire);

    // Fast path - check if we're next
    if (current_serving == ticket) {
        return ticket;
    }

    // Slow path with adaptive waiting strategy
    uint32_t spin_count = 0;
    while (true) {
        current_serving = serving_.load(std::memory_order_acquire);
        if (current_serving == ticket) {
            return ticket;
        }

        if (spin_count < MAX_SPIN_COUNT) {
            // Use CPU pause instruction for short spins
            cpu_relax();
            spin_count++;
        } else {
            // After spinning for a while, yield to scheduler to avoid CPU
            // starvation
            std::this_thread::yield();
            // Reset spin counter to give CPU time to other threads
            spin_count = 0;
        }
    }
}

void TicketSpinlock::unlock(uint64_t ticket) {
// Verify correct ticket in debug builds
#ifdef ATOM_DEBUG
    auto expected_ticket = serving_.load(std::memory_order_acquire);
    if (expected_ticket != ticket) {
        throw std::invalid_argument("Incorrect ticket provided to unlock");
    }
#endif

    serving_.store(ticket + 1, std::memory_order_release);
}

void UnfairSpinlock::lock() noexcept {
    // First attempt - optimistic fast path
    if (!flag_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    // Slow path - use exponential backoff utility
    exponentialBackoffSpin([this]() noexcept {
        return !flag_.test_and_set(std::memory_order_acquire);
    });
}

void UnfairSpinlock::unlock() noexcept {
    flag_.clear(std::memory_order_release);

#if defined(__cpp_lib_atomic_flag_test)
    // Wake any waiting threads (C++20 feature)
    flag_.notify_one();
#endif
}

#ifdef ATOM_USE_BOOST_LOCKFREE
void BoostSpinlock::lock() noexcept {
#ifdef ATOM_DEBUG
    // Check for recursive lock attempts in debug mode
    std::thread::id current_id = std::this_thread::get_id();
    std::thread::id no_thread;
    if (owner_.load(boost::memory_order_relaxed) == current_id) {
        // Cannot throw in noexcept function
        std::terminate();
    }
#endif

    // Fast path first - single attempt
    if (!flag_.exchange(true, boost::memory_order_acquire)) {
#ifdef ATOM_DEBUG
        owner_.store(current_id, boost::memory_order_relaxed);
#endif
        return;
    }

    // Slow path - use exponential backoff utility
    exponentialBackoffSpin([this]() noexcept {
        // First check if lock is free without doing an exchange
        if (!flag_.load(boost::memory_order_relaxed)) {
            // Lock appears free, try to acquire
            if (!flag_.exchange(true, boost::memory_order_acquire)) {
#ifdef ATOM_DEBUG
                owner_.store(std::this_thread::get_id(),
                             boost::memory_order_relaxed);
#endif
                return true;
            }
        }
        return false;
    });
}

auto BoostSpinlock::tryLock() noexcept -> bool {
    bool expected = false;
    bool success = flag_.compare_exchange_strong(expected, true,
                                                 boost::memory_order_acquire,
                                                 boost::memory_order_relaxed);

#ifdef ATOM_DEBUG
    if (success) {
        owner_.store(std::this_thread::get_id(), boost::memory_order_relaxed);
    }
#endif

    return success;
}

void BoostSpinlock::unlock() noexcept {
#ifdef ATOM_DEBUG
    std::thread::id current_id = std::this_thread::get_id();
    if (owner_.load(boost::memory_order_relaxed) != current_id) {
        // Log error instead of throwing from noexcept function
        std::terminate();  // Terminate in case of lock violation in debug mode
    }
    owner_.store(std::thread::id(), boost::memory_order_relaxed);
#endif

    flag_.store(false, boost::memory_order_release);
}
#endif

auto LockFactory::createLock(LockType type) -> std::unique_ptr<ILock> {
    switch (type) {
        case LockType::SPINLOCK:
            return std::make_unique<LockAdapter<Spinlock>>();
        case LockType::TICKET_SPINLOCK:
            return std::make_unique<TicketSpinlockAdapter>();
        case LockType::UNFAIR_SPINLOCK:
            return std::make_unique<LockAdapter<UnfairSpinlock>>();
        case LockType::ADAPTIVE_SPINLOCK:
            return std::make_unique<LockAdapter<AdaptiveSpinlock>>();
#ifdef ATOM_HAS_ATOMIC_WAIT
        case LockType::ATOMIC_WAIT_LOCK:
            return std::make_unique<LockAdapter<AtomicWaitLock>>();
#endif
#ifdef ATOM_PLATFORM_WINDOWS
        case LockType::WINDOWS_SPINLOCK:
            return std::make_unique<LockAdapter<WindowsSpinlock>>();
        case LockType::WINDOWS_SHARED_MUTEX:
            return std::make_unique<LockAdapter<WindowsSharedMutex>>();
#endif
#ifdef ATOM_PLATFORM_MACOS
        case LockType::DARWIN_SPINLOCK:
            return std::make_unique<LockAdapter<DarwinSpinlock>>();
#endif
#ifdef ATOM_PLATFORM_LINUX
        case LockType::LINUX_FUTEX_LOCK:
            return std::make_unique<LockAdapter<LinuxFutexLock>>();
#endif
#ifdef ATOM_USE_BOOST_LOCKFREE
        case LockType::BOOST_SPINLOCK:
            return std::make_unique<LockAdapter<BoostSpinlock>>();
#endif
#ifdef ATOM_USE_BOOST_LOCKS
        case LockType::BOOST_MUTEX:
            return std::make_unique<LockAdapter<boost::mutex>>();
        case LockType::BOOST_RECURSIVE_MUTEX:
            return std::make_unique<LockAdapter<BoostRecursiveMutex>>();
        case LockType::BOOST_SHARED_MUTEX:
            return std::make_unique<LockAdapter<BoostSharedMutex>>();
#endif
        case LockType::STD_MUTEX:
            return std::make_unique<LockAdapter<std::mutex>>();
        case LockType::STD_RECURSIVE_MUTEX:
            return std::make_unique<LockAdapter<std::recursive_mutex>>();
        case LockType::STD_SHARED_MUTEX:
            return std::make_unique<LockAdapter<std::shared_mutex>>();
        case LockType::AUTO_OPTIMIZED:
            return createOptimizedLock();
        default:
            throw std::invalid_argument("Invalid lock type");
    }
}

auto LockFactory::createOptimizedLock() -> std::unique_ptr<ILock> {
    // Select the best lock based on platform capabilities
#ifdef ATOM_PLATFORM_WINDOWS
    return std::make_unique<LockAdapter<WindowsSpinlock>>();
#elif defined(ATOM_PLATFORM_LINUX)
    return std::make_unique<LockAdapter<LinuxFutexLock>>();
#elif defined(ATOM_PLATFORM_MACOS)
    return std::make_unique<LockAdapter<DarwinSpinlock>>();
#elif defined(ATOM_HAS_ATOMIC_WAIT)
    return std::make_unique<LockAdapter<AtomicWaitLock>>();
#else
    return std::make_unique<LockAdapter<AdaptiveSpinlock>>();
#endif
}

}  // namespace atom::async
