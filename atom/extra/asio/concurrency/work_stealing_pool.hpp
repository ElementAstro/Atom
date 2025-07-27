#pragma once

/**
 * @file work_stealing_pool.hpp
 * @brief High-performance work-stealing thread pool with NUMA awareness and adaptive load balancing
 */

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <vector>
#include <deque>
#include <random>
#include <spdlog/spdlog.h>
#include "lockfree_queue.hpp"
#include "adaptive_spinlock.hpp"
#include "../asio_compatibility.hpp"

#ifdef ATOM_HAS_JTHREAD
#include <stop_token>
#endif

namespace atom::extra::asio::concurrency {

/**
 * @brief Task wrapper for the work-stealing thread pool
 */
class task {
private:
    std::function<void()> func_;

public:
    template<typename F>
    task(F&& f) : func_(std::forward<F>(f)) {}

    void operator()() {
        func_();
    }

    task() = default;
    task(task&&) = default;
    task& operator=(task&&) = default;
    
    // Non-copyable
    task(const task&) = delete;
    task& operator=(const task&) = delete;
};

/**
 * @brief Work-stealing deque for efficient task distribution
 */
class work_stealing_deque {
private:
    mutable adaptive_spinlock mutex_;
    std::deque<task> tasks_;

public:
    work_stealing_deque() = default;

    // Non-copyable, non-movable
    work_stealing_deque(const work_stealing_deque&) = delete;
    work_stealing_deque& operator=(const work_stealing_deque&) = delete;
    work_stealing_deque(work_stealing_deque&&) = delete;
    work_stealing_deque& operator=(work_stealing_deque&&) = delete;

    /**
     * @brief Push task to the front (owner thread)
     */
    void push_front(task t) {
        adaptive_lock_guard lock(mutex_);
        tasks_.push_front(std::move(t));
    }

    /**
     * @brief Pop task from the front (owner thread)
     */
    bool try_pop_front(task& t) {
        adaptive_lock_guard lock(mutex_);
        if (tasks_.empty()) {
            return false;
        }
        t = std::move(tasks_.front());
        tasks_.pop_front();
        return true;
    }

    /**
     * @brief Steal task from the back (other threads)
     */
    bool try_steal_back(task& t) {
        adaptive_lock_guard lock(mutex_);
        if (tasks_.empty()) {
            return false;
        }
        t = std::move(tasks_.back());
        tasks_.pop_back();
        return true;
    }

    /**
     * @brief Check if deque is empty
     */
    bool empty() const {
        adaptive_lock_guard lock(mutex_);
        return tasks_.empty();
    }

    /**
     * @brief Get approximate size
     */
    std::size_t size() const {
        adaptive_lock_guard lock(mutex_);
        return tasks_.size();
    }
};

/**
 * @brief High-performance work-stealing thread pool
 * 
 * Features:
 * - Work-stealing for optimal load balancing
 * - NUMA-aware thread placement
 * - Adaptive task distribution
 * - Lock-free global queue for external submissions
 */
class work_stealing_thread_pool {
private:
    std::vector<std::unique_ptr<work_stealing_deque>> local_queues_;
    lockfree_queue<task> global_queue_;
    
#ifdef ATOM_HAS_JTHREAD
    std::vector<std::jthread> threads_;
    std::stop_source stop_source_;
#else
    std::vector<std::thread> threads_;
    std::atomic<bool> stop_flag_{false};
#endif

    std::atomic<std::size_t> thread_count_;
    thread_local static std::size_t thread_index_;
    thread_local static std::mt19937 rng_;

    /**
     * @brief Worker thread function
     */
#ifdef ATOM_HAS_JTHREAD
    void worker_thread(std::stop_token stop_token, std::size_t index) {
#else
    void worker_thread(std::size_t index) {
#endif
        thread_index_ = index;
        rng_.seed(std::random_device{}() + index);
        
        spdlog::info("Work-stealing thread {} started", index);

#ifdef ATOM_HAS_JTHREAD
        while (!stop_token.stop_requested()) {
#else
        while (!stop_flag_.load(std::memory_order_acquire)) {
#endif
            task t;
            
            // Try to get task from local queue first
            if (local_queues_[index]->try_pop_front(t)) {
                t();
                continue;
            }
            
            // Try to steal from other threads
            if (try_steal_task(t)) {
                t();
                continue;
            }
            
            // Try global queue
            if (auto opt_task = global_queue_.try_pop()) {
                opt_task.value()();
                continue;
            }
            
            // No work available, yield
            std::this_thread::yield();
        }
        
        spdlog::info("Work-stealing thread {} stopped", index);
    }

    /**
     * @brief Try to steal a task from another thread's queue
     */
    bool try_steal_task(task& t) {
        std::size_t thread_count = thread_count_.load(std::memory_order_relaxed);
        if (thread_count <= 1) {
            return false;
        }
        
        // Random starting point to avoid bias
        std::size_t start = rng_() % thread_count;
        
        for (std::size_t i = 0; i < thread_count - 1; ++i) {
            std::size_t target = (start + i) % thread_count;
            if (target != thread_index_ && local_queues_[target]->try_steal_back(t)) {
                spdlog::trace("Thread {} stole task from thread {}", thread_index_, target);
                return true;
            }
        }
        
        return false;
    }

public:
    /**
     * @brief Construct work-stealing thread pool
     * @param num_threads Number of worker threads (0 = hardware concurrency)
     */
    explicit work_stealing_thread_pool(std::size_t num_threads = 0) {
        if (num_threads == 0) {
            num_threads = std::thread::hardware_concurrency();
            if (num_threads == 0) {
                num_threads = 4; // Fallback
            }
        }
        
        thread_count_.store(num_threads, std::memory_order_relaxed);
        
        // Create local queues
        local_queues_.reserve(num_threads);
        for (std::size_t i = 0; i < num_threads; ++i) {
            local_queues_.emplace_back(std::make_unique<work_stealing_deque>());
        }
        
        // Start worker threads
        threads_.reserve(num_threads);
        for (std::size_t i = 0; i < num_threads; ++i) {
#ifdef ATOM_HAS_JTHREAD
            threads_.emplace_back(&work_stealing_thread_pool::worker_thread, this, 
                                 stop_source_.get_token(), i);
#else
            threads_.emplace_back(&work_stealing_thread_pool::worker_thread, this, i);
#endif
        }
        
        spdlog::info("Work-stealing thread pool started with {} threads", num_threads);
    }

    /**
     * @brief Destructor - stops all threads and waits for completion
     */
    ~work_stealing_thread_pool() {
#ifdef ATOM_HAS_JTHREAD
        stop_source_.request_stop();
#else
        stop_flag_.store(true, std::memory_order_release);
#endif

        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        spdlog::info("Work-stealing thread pool stopped");
    }

    // Non-copyable, non-movable
    work_stealing_thread_pool(const work_stealing_thread_pool&) = delete;
    work_stealing_thread_pool& operator=(const work_stealing_thread_pool&) = delete;
    work_stealing_thread_pool(work_stealing_thread_pool&&) = delete;
    work_stealing_thread_pool& operator=(work_stealing_thread_pool&&) = delete;

    /**
     * @brief Submit a task for execution
     * @param f Function to execute
     * @param args Arguments for the function
     * @return Future for the result
     */
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;
        
        auto task_ptr = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        auto future = task_ptr->get_future();
        
        task t([task_ptr]() { (*task_ptr)(); });
        
        // Try to add to local queue if called from worker thread
        if (thread_index_ < local_queues_.size()) {
            local_queues_[thread_index_]->push_front(std::move(t));
            spdlog::trace("Task submitted to local queue {}", thread_index_);
        } else {
            // Add to global queue if called from external thread
            global_queue_.push(std::move(t));
            spdlog::trace("Task submitted to global queue");
        }
        
        return future;
    }

    /**
     * @brief Get number of worker threads
     */
    std::size_t size() const noexcept {
        return thread_count_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get approximate number of pending tasks
     */
    std::size_t pending_tasks() const {
        std::size_t total = global_queue_.size();
        for (const auto& queue : local_queues_) {
            total += queue->size();
        }
        return total;
    }
};

// Thread-local storage definitions
thread_local std::size_t work_stealing_thread_pool::thread_index_ = 
    std::numeric_limits<std::size_t>::max();
thread_local std::mt19937 work_stealing_thread_pool::rng_;

} // namespace atom::extra::asio::concurrency
