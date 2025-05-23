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

#include <thread>
#include <memory>
#include <functional>

namespace atom::async {

void Spinlock::lock() {
    #ifdef ATOM_DEBUG
    // Check for recursive lock attempts in debug mode
    std::thread::id current_id = std::this_thread::get_id();
    std::thread::id no_thread;
    if (owner_.load(std::memory_order_relaxed) == current_id) {
        throw std::system_error(
            std::make_error_code(std::errc::resource_deadlock_would_occur),
            "Recursive lock attempt detected"
        );
    }
    #endif

    // Fast path first - single attempt
    if (!flag_.test_and_set(std::memory_order_acquire)) {
        #ifdef ATOM_DEBUG
        owner_.store(current_id, std::memory_order_relaxed);
        #endif
        return;
    }

    // Slow path - exponential backoff
    uint32_t backoff_count = 1;
    constexpr uint32_t MAX_BACKOFF = 1024;
    
    while (true) {
        // Perform exponential backoff 
        for (uint32_t i = 0; i < backoff_count; ++i) {
            cpu_relax();
        }
        
        // Try to acquire the lock
        if (!flag_.test_and_set(std::memory_order_acquire)) {
            #ifdef ATOM_DEBUG
            owner_.store(current_id, std::memory_order_relaxed);
            #endif
            return;
        }
        
        // Increase backoff time (capped at maximum)
        backoff_count = std::min(backoff_count * 2, MAX_BACKOFF);
        
        // Yield to scheduler if we've been spinning for a while
        if (backoff_count >= MAX_BACKOFF / 2) {
            std::this_thread::yield();
        }
    }
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
        std::terminate(); // Terminate in case of lock violation in debug mode
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
            // After spinning for a while, yield to scheduler to avoid CPU starvation
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
    
    // Slow path with backoff
    uint32_t backoff_count = 1;
    constexpr uint32_t MAX_BACKOFF = 1024;
    
    while (true) {
        for (uint32_t i = 0; i < backoff_count; ++i) {
            cpu_relax();
        }
        
        if (!flag_.test_and_set(std::memory_order_acquire)) {
            return;
        }
        
        // Increase backoff time (capped at maximum)
        backoff_count = std::min(backoff_count * 2, MAX_BACKOFF);
        
        // Yield to scheduler if we've been spinning for a while
        if (backoff_count >= MAX_BACKOFF / 2) {
            std::this_thread::yield();
        }
    }
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

    // Slow path - exponential backoff
    uint32_t backoff_count = 1;
    constexpr uint32_t MAX_BACKOFF = 1024;
    
    // Wait until we acquire the lock
    while (true) {
        // First check if lock is free without doing an exchange
        if (!flag_.load(boost::memory_order_relaxed)) {
            // Lock appears free, try to acquire
            if (!flag_.exchange(true, boost::memory_order_acquire)) {
                #ifdef ATOM_DEBUG
                owner_.store(current_id, boost::memory_order_relaxed);
                #endif
                return;
            }
        }
        
        // Perform exponential backoff 
        for (uint32_t i = 0; i < backoff_count; ++i) {
            cpu_relax();
        }
        
        // Increase backoff time (capped at maximum)
        backoff_count = std::min(backoff_count * 2, MAX_BACKOFF);
        
        // Yield to scheduler if we've been spinning for a while
        if (backoff_count >= MAX_BACKOFF / 2) {
            std::this_thread::yield();
        }
    }
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
        std::terminate(); // Terminate in case of lock violation in debug mode
    }
    owner_.store(std::thread::id(), boost::memory_order_relaxed);
    #endif
    
    flag_.store(false, boost::memory_order_release);
}
#endif

auto LockFactory::createLock(LockType type) -> std::unique_ptr<void, std::function<void(void*)>> {
    switch (type) {
        case LockType::SPINLOCK: {
            auto lock = new Spinlock();
            return {lock, [](void* ptr) { delete static_cast<Spinlock*>(ptr); }};
        }
        case LockType::TICKET_SPINLOCK: {
            auto lock = new TicketSpinlock();
            return {lock, [](void* ptr) { delete static_cast<TicketSpinlock*>(ptr); }};
        }
        case LockType::UNFAIR_SPINLOCK: {
            auto lock = new UnfairSpinlock();
            return {lock, [](void* ptr) { delete static_cast<UnfairSpinlock*>(ptr); }};
        }
        case LockType::ADAPTIVE_SPINLOCK: {
            auto lock = new AdaptiveSpinlock();
            return {lock, [](void* ptr) { delete static_cast<AdaptiveSpinlock*>(ptr); }};
        }
#ifdef ATOM_USE_BOOST_LOCKFREE
        case LockType::BOOST_SPINLOCK: {
            auto lock = new BoostSpinlock();
            return {lock, [](void* ptr) { delete static_cast<BoostSpinlock*>(ptr); }};
        }
#endif
#ifdef ATOM_USE_BOOST_LOCKS
        case LockType::BOOST_MUTEX: {
            auto lock = new boost::mutex();
            return {lock, [](void* ptr) { delete static_cast<boost::mutex*>(ptr); }};
        }
        case LockType::BOOST_RECURSIVE_MUTEX: {
            auto lock = new BoostRecursiveMutex();
            return {lock, [](void* ptr) { delete static_cast<BoostRecursiveMutex*>(ptr); }};
        }
        case LockType::BOOST_SHARED_MUTEX: {
            auto lock = new BoostSharedMutex();
            return {lock, [](void* ptr) { delete static_cast<BoostSharedMutex*>(ptr); }};
        }
#endif
        default:
            throw std::invalid_argument("Invalid lock type");
    }
}

}  // namespace atom::async
