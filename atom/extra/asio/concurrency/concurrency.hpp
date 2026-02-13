#pragma once

/**
 * @file concurrency.hpp
 * @brief Comprehensive concurrency framework with cutting-edge C++23 primitives
 *
 * This header provides access to all advanced concurrency components:
 * - Lock-free data structures with hazard pointers
 * - Adaptive synchronization primitives
 * - Work-stealing thread pool
 * - Real-time performance monitoring
 * - NUMA-aware memory management
 */

#include "lockfree_queue.hpp"
#include "adaptive_spinlock.hpp"
#include "work_stealing_pool.hpp"
#include "performance_monitor.hpp"
#include "memory_manager.hpp"
#include "../asio_compatibility.hpp"

#include <memory>
#include <spdlog/spdlog.h>

namespace atom::extra::asio::concurrency {



/**
 * @brief Concurrent object pool for high-frequency allocations
 */
template<typename T>
class concurrent_object_pool {
private:
    lockfree_queue<std::unique_ptr<T>> available_objects_;
    numa_memory_pool<T> memory_pool_;
    cache_aligned<std::atomic<std::size_t>> total_allocated_{0};
    cache_aligned<std::atomic<std::size_t>> total_in_use_{0};

public:
    /**
     * @brief Construct concurrent object pool
     */
    concurrent_object_pool() {
        spdlog::debug("Concurrent object pool initialized for type: {}", typeid(T).name());
    }

    /**
     * @brief Acquire an object from the pool
     */
    template<typename... Args>
    std::unique_ptr<T> acquire(Args&&... args) {
        // Try to get from pool first
        if (auto obj = available_objects_.try_pop()) {
            total_in_use_.get().fetch_add(1, std::memory_order_relaxed);
            spdlog::trace("Object acquired from pool");
            return std::move(obj.value());
        }

        // Allocate new object
        auto* raw_ptr = memory_pool_.allocate(std::forward<Args>(args)...);
        auto obj = std::unique_ptr<T>(raw_ptr);

        total_allocated_.get().fetch_add(1, std::memory_order_relaxed);
        total_in_use_.get().fetch_add(1, std::memory_order_relaxed);

        spdlog::trace("New object allocated for pool");
        return obj;
    }

    /**
     * @brief Return an object to the pool
     */
    void release(std::unique_ptr<T> obj) {
        if (obj) {
            available_objects_.push(std::move(obj));
            total_in_use_.get().fetch_sub(1, std::memory_order_relaxed);
            spdlog::trace("Object returned to pool");
        }
    }

    /**
     * @brief Get pool statistics
     */
    struct pool_stats {
        std::size_t total_allocated;
        std::size_t total_in_use;
        std::size_t available;
    };

    pool_stats get_stats() const {
        return {
            total_allocated_.get().load(std::memory_order_relaxed),
            total_in_use_.get().load(std::memory_order_relaxed),
            available_objects_.size()
        };
    }
};

/**
 * @brief Global concurrency manager for coordinating all concurrency primitives
 */
class concurrency_manager {
private:
    std::unique_ptr<work_stealing_thread_pool> thread_pool_;
    performance_monitor& perf_monitor_;

    // Singleton instance
    static std::unique_ptr<concurrency_manager> instance_;
    static std::once_flag init_flag_;

    concurrency_manager() : perf_monitor_(performance_monitor::instance()) {
        // Initialize with optimal thread count
        auto thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) thread_count = 4;

        thread_pool_ = std::make_unique<work_stealing_thread_pool>(thread_count);

        spdlog::info("Concurrency manager initialized with {} threads", thread_count);
    }

public:
    /**
     * @brief Get the singleton instance
     */
    static concurrency_manager& instance() {
        std::call_once(init_flag_, []() {
            instance_ = std::unique_ptr<concurrency_manager>(new concurrency_manager());
        });
        return *instance_;
    }

    // Non-copyable, non-movable
    concurrency_manager(const concurrency_manager&) = delete;
    concurrency_manager& operator=(const concurrency_manager&) = delete;
    concurrency_manager(concurrency_manager&&) = delete;
    concurrency_manager& operator=(concurrency_manager&&) = delete;

    /**
     * @brief Get the work-stealing thread pool
     */
    work_stealing_thread_pool& thread_pool() {
        return *thread_pool_;
    }

    /**
     * @brief Get the performance monitor
     */
    performance_monitor& performance() {
        return perf_monitor_;
    }

    /**
     * @brief Submit a task to the thread pool with performance monitoring
     */
    template<typename F, typename... Args>
    auto submit_monitored(const std::string& task_name, F&& f, Args&&... args) {
        return thread_pool_->submit([task_name, f = std::forward<F>(f), args...]() mutable {
            ATOM_MEASURE_PERFORMANCE(task_name);
            return f(args...);
        });
    }

    /**
     * @brief Log comprehensive system statistics
     */
    void log_system_stats() const {
        spdlog::info("=== Concurrency System Statistics ===");
        spdlog::info("Thread pool size: {}", thread_pool_->size());
        spdlog::info("Pending tasks: {}", thread_pool_->pending_tasks());
        spdlog::info("Performance counters: {}", perf_monitor_.counter_count());

        perf_monitor_.log_statistics();

        spdlog::info("====================================");
    }
};



/**
 * @brief Convenience function to get the global concurrency manager
 */
inline concurrency_manager& get_concurrency_manager() {
    return concurrency_manager::instance();
}

/**
 * @brief Convenience function to submit a monitored task
 */
template<typename F, typename... Args>
auto submit_task(const std::string& name, F&& f, Args&&... args) {
    return get_concurrency_manager().submit_monitored(name, std::forward<F>(f), std::forward<Args>(args)...);
}

} // namespace atom::extra::asio::concurrency
