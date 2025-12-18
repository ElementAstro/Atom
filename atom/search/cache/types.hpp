/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file types.hpp
 * @brief Common types and threading abstractions for cache components.
 * @details This file provides unified type aliases for threading primitives,
 *          supporting both Boost and STL implementations based on compile
 * flags.
 */

#ifndef ATOM_SEARCH_CACHE_TYPES_HPP
#define ATOM_SEARCH_CACHE_TYPES_HPP

#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "atom/containers/high_performance.hpp"

#if defined(ATOM_USE_BOOST_THREAD) || defined(ATOM_USE_BOOST_LOCKFREE)
#include <boost/config.hpp>
#endif

#ifdef ATOM_USE_BOOST_THREAD
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/atomic.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#endif

namespace atom::search::cache {

using atom::containers::HashMap;
using atom::containers::String;
using atom::containers::Vector;

// =============================================================================
// Threading Abstractions
// =============================================================================

#if defined(ATOM_USE_BOOST_THREAD)
using SharedMutex = boost::shared_mutex;
using Mutex = boost::mutex;
using ConditionVariable = boost::condition_variable;
using ConditionVariableAny = boost::condition_variable_any;
using Thread = boost::thread;
using JThread = boost::thread;

template <typename T>
using SharedLock = boost::shared_lock<T>;

template <typename T>
using UniqueLock = boost::unique_lock<T>;

template <typename T>
using LockGuard = boost::lock_guard<T>;

template <typename... Args>
using Future = boost::future<Args...>;

template <typename... Args>
using Promise = boost::promise<Args...>;

#else
using SharedMutex = std::shared_mutex;
using Mutex = std::mutex;
using ConditionVariable = std::condition_variable;
using ConditionVariableAny = std::condition_variable_any;
using Thread = std::thread;
using JThread = std::jthread;

template <typename T>
using SharedLock = std::shared_lock<T>;

template <typename T>
using UniqueLock = std::unique_lock<T>;

template <typename T>
using LockGuard = std::lock_guard<T>;

template <typename... Args>
using Future = std::future<Args...>;

template <typename... Args>
using Promise = std::promise<Args...>;
#endif

// =============================================================================
// Atomic Types
// =============================================================================

#if defined(ATOM_USE_BOOST_LOCKFREE)
template <typename T>
using Atomic = boost::atomic<T>;
#else
template <typename T>
using Atomic = std::atomic<T>;
#endif

// =============================================================================
// Lock-Free Queue
// =============================================================================

#if defined(ATOM_USE_BOOST_LOCKFREE)
template <typename T>
class LockFreeQueue {
private:
    boost::lockfree::queue<T*> queue_;

public:
    explicit LockFreeQueue(size_t capacity) : queue_(capacity) {}

    ~LockFreeQueue() {
        T* ptr;
        while (queue_.pop(ptr)) {
            delete ptr;
        }
    }

    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    [[nodiscard]] bool push(const T& item) {
        T* ptr = new T(item);
        if (!queue_.push(ptr)) {
            delete ptr;
            return false;
        }
        return true;
    }

    [[nodiscard]] bool push(T&& item) {
        T* ptr = new T(std::move(item));
        if (!queue_.push(ptr)) {
            delete ptr;
            return false;
        }
        return true;
    }

    [[nodiscard]] bool pop(T& item) {
        T* ptr = nullptr;
        if (!queue_.pop(ptr)) {
            return false;
        }
        item = std::move(*ptr);
        delete ptr;
        return true;
    }

    [[nodiscard]] bool empty() const noexcept { return queue_.empty(); }
};
#else
template <typename T>
class LockFreeQueue {
private:
    mutable Mutex mutex_;
    std::deque<T> items_;
    size_t capacity_;

public:
    explicit LockFreeQueue(size_t capacity) : capacity_(capacity) {}

    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    [[nodiscard]] bool push(const T& item) {
        LockGuard<Mutex> lock(mutex_);
        if (items_.size() >= capacity_) {
            return false;
        }
        items_.push_back(item);
        return true;
    }

    [[nodiscard]] bool push(T&& item) {
        LockGuard<Mutex> lock(mutex_);
        if (items_.size() >= capacity_) {
            return false;
        }
        items_.push_back(std::move(item));
        return true;
    }

    [[nodiscard]] bool pop(T& item) {
        LockGuard<Mutex> lock(mutex_);
        if (items_.empty()) {
            return false;
        }
        item = std::move(items_.front());
        items_.pop_front();
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        LockGuard<Mutex> lock(mutex_);
        return items_.empty();
    }

    [[nodiscard]] size_t size() const noexcept {
        LockGuard<Mutex> lock(mutex_);
        return items_.size();
    }

    void clear() {
        LockGuard<Mutex> lock(mutex_);
        items_.clear();
    }
};
#endif

// =============================================================================
// Concepts
// =============================================================================

/**
 * @brief Concept for types that can be cached.
 */
template <typename T>
concept Cacheable = std::copy_constructible<T> && std::is_copy_assignable_v<T>;

/**
 * @brief Concept for hashable key types.
 */
template <typename T>
concept HashableKey = requires(T t) {
    { std::hash<T>{}(t) } -> std::convertible_to<size_t>;
};

// =============================================================================
// Common Types
// =============================================================================

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Duration = std::chrono::milliseconds;
using Seconds = std::chrono::seconds;

/**
 * @brief Cache statistics for monitoring performance and usage.
 * @note Uses snake_case for backward compatibility with existing code.
 */
struct CacheStatistics {
    size_t hits{0};
    size_t misses{0};
    size_t evictions{0};
    size_t expirations{0};
    size_t current_size{0};
    size_t max_capacity{0};
    double hit_rate{0.0};
    Duration avg_access_time{0};

    void updateHitRate() noexcept {
        size_t total = hits + misses;
        hit_rate = total > 0
                       ? static_cast<double>(hits) / static_cast<double>(total)
                       : 0.0;
    }
};

/**
 * @brief Configuration options for cache behavior.
 * @note Uses snake_case for backward compatibility with existing code.
 */
struct CacheConfig {
    bool enable_automatic_cleanup{true};
    bool enable_statistics{true};
    bool thread_safe{true};
    size_t cleanup_batch_size{100};
    double load_factor{0.75};
    Duration cleanup_interval{Duration(60000)};  // 1 minute default
};

// =============================================================================
// Hash Utilities
// =============================================================================

/**
 * @brief Hash function for pair keys.
 */
struct PairStringHash {
    [[nodiscard]] size_t operator()(
        const std::pair<std::string, std::string>& p) const noexcept {
        size_t h1 = std::hash<std::string>{}(p.first);
        size_t h2 = std::hash<std::string>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

}  // namespace atom::search::cache

#endif  // ATOM_SEARCH_CACHE_TYPES_HPP
