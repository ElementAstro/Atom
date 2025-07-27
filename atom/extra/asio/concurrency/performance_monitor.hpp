#pragma once

/**
 * @file performance_monitor.hpp
 * @brief Real-time performance monitoring with lock-free metrics collection
 */

#include <atomic>
#include <chrono>
#include <string>
#include <unordered_map>
#include <memory>
#include <spdlog/spdlog.h>
#include "lockfree_queue.hpp"
#include "adaptive_spinlock.hpp"
#include "../asio_compatibility.hpp"

namespace atom::extra::asio::concurrency {

/**
 * @brief High-resolution timer for performance measurements
 */
class high_resolution_timer {
private:
    std::chrono::high_resolution_clock::time_point start_time_;

public:
    /**
     * @brief Start the timer
     */
    high_resolution_timer() : start_time_(std::chrono::high_resolution_clock::now()) {}

    /**
     * @brief Get elapsed time in nanoseconds
     */
    std::chrono::nanoseconds elapsed() const noexcept {
        auto end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time_);
    }

    /**
     * @brief Get elapsed time in microseconds
     */
    std::chrono::microseconds elapsed_microseconds() const noexcept {
        return std::chrono::duration_cast<std::chrono::microseconds>(elapsed());
    }

    /**
     * @brief Get elapsed time in milliseconds
     */
    std::chrono::milliseconds elapsed_milliseconds() const noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed());
    }

    /**
     * @brief Reset the timer
     */
    void reset() noexcept {
        start_time_ = std::chrono::high_resolution_clock::now();
    }
};

/**
 * @brief Lock-free performance counter
 */
class performance_counter {
private:
    cache_aligned<std::atomic<std::uint64_t>> count_{0};
    cache_aligned<std::atomic<std::uint64_t>> total_time_{0};
    cache_aligned<std::atomic<std::uint64_t>> min_time_{std::numeric_limits<std::uint64_t>::max()};
    cache_aligned<std::atomic<std::uint64_t>> max_time_{0};

public:
    /**
     * @brief Record an operation with its duration
     */
    void record(std::chrono::nanoseconds duration) noexcept {
        auto duration_ns = static_cast<std::uint64_t>(duration.count());
        
        count_.get().fetch_add(1, std::memory_order_relaxed);
        total_time_.get().fetch_add(duration_ns, std::memory_order_relaxed);
        
        // Update min time
        auto current_min = min_time_.get().load(std::memory_order_relaxed);
        while (duration_ns < current_min && 
               !min_time_.get().compare_exchange_weak(current_min, duration_ns, 
                                                     std::memory_order_relaxed)) {
            // Retry until successful or no longer minimum
        }
        
        // Update max time
        auto current_max = max_time_.get().load(std::memory_order_relaxed);
        while (duration_ns > current_max && 
               !max_time_.get().compare_exchange_weak(current_max, duration_ns, 
                                                     std::memory_order_relaxed)) {
            // Retry until successful or no longer maximum
        }
    }

    /**
     * @brief Get operation count
     */
    std::uint64_t count() const noexcept {
        return count_.get().load(std::memory_order_relaxed);
    }

    /**
     * @brief Get average duration in nanoseconds
     */
    double average_ns() const noexcept {
        auto cnt = count();
        if (cnt == 0) return 0.0;
        return static_cast<double>(total_time_.get().load(std::memory_order_relaxed)) / cnt;
    }

    /**
     * @brief Get minimum duration in nanoseconds
     */
    std::uint64_t min_ns() const noexcept {
        auto min_val = min_time_.get().load(std::memory_order_relaxed);
        return min_val == std::numeric_limits<std::uint64_t>::max() ? 0 : min_val;
    }

    /**
     * @brief Get maximum duration in nanoseconds
     */
    std::uint64_t max_ns() const noexcept {
        return max_time_.get().load(std::memory_order_relaxed);
    }

    /**
     * @brief Reset all counters
     */
    void reset() noexcept {
        count_.get().store(0, std::memory_order_relaxed);
        total_time_.get().store(0, std::memory_order_relaxed);
        min_time_.get().store(std::numeric_limits<std::uint64_t>::max(), std::memory_order_relaxed);
        max_time_.get().store(0, std::memory_order_relaxed);
    }
};

/**
 * @brief RAII performance measurement scope
 */
class performance_scope {
private:
    performance_counter& counter_;
    high_resolution_timer timer_;

public:
    /**
     * @brief Start measuring performance for the given counter
     */
    explicit performance_scope(performance_counter& counter) : counter_(counter) {}

    /**
     * @brief Destructor records the elapsed time
     */
    ~performance_scope() {
        counter_.record(timer_.elapsed());
    }

    // Non-copyable, non-movable
    performance_scope(const performance_scope&) = delete;
    performance_scope& operator=(const performance_scope&) = delete;
    performance_scope(performance_scope&&) = delete;
    performance_scope& operator=(performance_scope&&) = delete;
};

/**
 * @brief Global performance monitoring system
 */
class performance_monitor {
private:
    mutable reader_writer_spinlock mutex_;
    std::unordered_map<std::string, std::unique_ptr<performance_counter>> counters_;
    
    // Singleton instance
    static std::unique_ptr<performance_monitor> instance_;
    static std::once_flag init_flag_;

    performance_monitor() = default;

public:
    /**
     * @brief Get the singleton instance
     */
    static performance_monitor& instance() {
        std::call_once(init_flag_, []() {
            instance_ = std::unique_ptr<performance_monitor>(new performance_monitor());
            spdlog::info("Performance monitor initialized");
        });
        return *instance_;
    }

    // Non-copyable, non-movable
    performance_monitor(const performance_monitor&) = delete;
    performance_monitor& operator=(const performance_monitor&) = delete;
    performance_monitor(performance_monitor&&) = delete;
    performance_monitor& operator=(performance_monitor&&) = delete;

    /**
     * @brief Get or create a performance counter
     */
    performance_counter& get_counter(const std::string& name) {
        // Try read lock first for existing counters
        {
            shared_lock_guard read_lock(mutex_);
            auto it = counters_.find(name);
            if (it != counters_.end()) {
                return *it->second;
            }
        }
        
        // Need write lock to create new counter
        mutex_.lock();
        
        // Double-check in case another thread created it
        auto it = counters_.find(name);
        if (it != counters_.end()) {
            return *it->second;
        }
        
        // Create new counter
        auto counter = std::make_unique<performance_counter>();
        auto* counter_ptr = counter.get();
        counters_[name] = std::move(counter);

        mutex_.unlock();

        spdlog::debug("Created performance counter: {}", name);
        return *counter_ptr;
    }

    /**
     * @brief Create a performance measurement scope
     */
    performance_scope measure(const std::string& name) {
        return performance_scope(get_counter(name));
    }

    /**
     * @brief Log performance statistics for all counters
     */
    void log_statistics() const {
        shared_lock_guard lock(mutex_);
        
        spdlog::info("=== Performance Statistics ===");
        for (const auto& [name, counter] : counters_) {
            auto count = counter->count();
            if (count > 0) {
                spdlog::info("{}: count={}, avg={:.2f}μs, min={:.2f}μs, max={:.2f}μs",
                           name, count,
                           counter->average_ns() / 1000.0,
                           counter->min_ns() / 1000.0,
                           counter->max_ns() / 1000.0);
            }
        }
        spdlog::info("==============================");
    }

    /**
     * @brief Reset all performance counters
     */
    void reset_all() {
        shared_lock_guard lock(mutex_);
        for (const auto& [name, counter] : counters_) {
            counter->reset();
        }
        spdlog::info("All performance counters reset");
    }

    /**
     * @brief Get number of registered counters
     */
    std::size_t counter_count() const {
        shared_lock_guard lock(mutex_);
        return counters_.size();
    }
};



/**
 * @brief Convenience macro for measuring function performance
 */
#define ATOM_MEASURE_PERFORMANCE(name) \
    auto _perf_scope = atom::extra::asio::concurrency::performance_monitor::instance().measure(name)

/**
 * @brief Convenience macro for measuring scope performance
 */
#define ATOM_MEASURE_SCOPE(name) \
    auto _perf_scope_##__LINE__ = atom::extra::asio::concurrency::performance_monitor::instance().measure(name)

} // namespace atom::extra::asio::concurrency
