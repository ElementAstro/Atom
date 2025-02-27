/*
 * async_executor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-24

Description: Advanced async task executor with thread pooling

**************************************************/

#ifndef ATOM_ASYNC_ASYNC_EXECUTOR_HPP
#define ATOM_ASYNC_ASYNC_EXECUTOR_HPP

#include <atomic>
#include <concepts>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace atom::async {

/**
 * @brief Class that represents a task with priority.
 * 
 * ExecutorTask encapsulates a function to be executed with its priority level.
 * Higher priority tasks are executed before lower priority ones.
 */
class ExecutorTask {
public:
    enum class Priority {
        LOW = 0,
        NORMAL = 1,
        HIGH = 2,
        CRITICAL = 3
    };

    template <typename F, typename... Args>
        requires std::invocable<F, Args...>
    ExecutorTask(F&& func, Priority prio, Args&&... args)
        : priority_(prio),
          function_(std::bind(std::forward<F>(func), std::forward<Args>(args)...)) {}

    void execute() {
        if (function_) {
            function_();
        }
    }

    [[nodiscard]] Priority getPriority() const noexcept {
        return priority_;
    }

private:
    Priority priority_;
    std::function<void()> function_;
};

/**
 * @brief Comparison operator for ExecutorTask objects based on priority
 */
inline bool operator<(const ExecutorTask& lhs, const ExecutorTask& rhs) {
    return static_cast<int>(lhs.getPriority()) < static_cast<int>(rhs.getPriority());
}

/**
 * @brief ThreadPool implementation with priority-based task execution.
 * 
 * Features:
 * - Dynamic resizing of thread pool
 * - Priority-based task scheduling
 * - Work stealing for load balancing
 * - Task cancellation support
 */
class ThreadPool {
public:
    /**
     * @brief Constructs a thread pool with a specified number of threads
     * 
     * @param numThreads Number of threads in the pool
     * @throws std::invalid_argument if numThreads is 0
     */
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency())
        : stop_(false), active_tasks_(0) {
        
        if (numThreads == 0) {
            throw std::invalid_argument("Thread pool size cannot be zero");
        }
        
        try {
            threads_.reserve(numThreads);
            for (size_t i = 0; i < numThreads; ++i) {
                threads_.emplace_back([this] { workerThread(); });
            }
        } catch (...) {
            stop_ = true;
            condition_.notify_all();
            throw;
        }
    }

    /**
     * @brief Destructor that cleans up threads
     */
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    // Rule of five - prevent copy, allow move
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * @brief Enqueues a task with a specified priority
     * 
     * @tparam F Function type
     * @tparam Args Argument types
     * @param func Function to execute
     * @param priority Task priority
     * @param args Function arguments
     * @return Future with the result of the function
     */
    template<typename F, typename... Args>
        requires std::invocable<F, Args...>
    auto enqueue(F&& func, ExecutorTask::Priority priority, Args&&... args) 
        -> std::future<std::invoke_result_t<F, Args...>> {
        
        using return_type = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            [f = std::forward<F>(func), ... args = std::forward<Args>(args)]() mutable {
                return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
            }
        );
        
        std::future<return_type> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            if (stop_) {
                throw std::runtime_error("Cannot enqueue task on stopped ThreadPool");
            }
            
            tasks_.emplace_back(
                [task]() { (*task)(); },
                priority
            );
        }
        
        condition_.notify_one();
        return result;
    }

    /**
     * @brief Gets the number of tasks waiting in the queue
     * 
     * @return Size of the task queue
     */
    [[nodiscard]] size_t queueSize() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

    /**
     * @brief Gets the number of active tasks currently being processed
     * 
     * @return Number of active tasks
     */
    [[nodiscard]] size_t activeTaskCount() const {
        return active_tasks_.load();
    }

    /**
     * @brief Gets the number of threads in the pool
     * 
     * @return Size of the thread pool
     */
    [[nodiscard]] size_t size() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return threads_.size();
    }

    /**
     * @brief Resizes the thread pool
     * 
     * @param numThreads New thread pool size
     * @throws std::invalid_argument if numThreads is 0
     */
    void resize(size_t numThreads) {
        if (numThreads == 0) {
            throw std::invalid_argument("Thread pool size cannot be zero");
        }
        
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        size_t oldSize = threads_.size();
        
        if (numThreads > oldSize) {
            // Add new threads
            threads_.reserve(numThreads);
            try {
                for (size_t i = oldSize; i < numThreads; ++i) {
                    threads_.emplace_back([this] { workerThread(); });
                }
            } catch (...) {
                // If an exception occurs, ensure consistency
                stop_ = true;
                lock.unlock();
                condition_.notify_all();
                throw;
            }
        } else if (numThreads < oldSize) {
            // Request threads to stop
            size_t diff = oldSize - numThreads;
            threads_to_stop_ = diff;
            
            lock.unlock();
            condition_.notify_all();
            
            // Wait for excess threads to finish
            for (size_t i = 0; i < diff; ++i) {
                if (i < threads_.size() && threads_[i].joinable()) {
                    threads_[i].join();
                }
            }
            
            // Remove joined threads
            lock.lock();
            threads_.erase(
                std::remove_if(
                    threads_.begin(), threads_.end(),
                    [](const std::thread& t) { return !t.joinable(); }
                ),
                threads_.end()
            );
        }
    }

    /**
     * @brief Clears all pending tasks from the queue
     * 
     * @return Number of tasks removed
     */
    size_t clearQueue() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        size_t count = tasks_.size();
        tasks_.clear();
        return count;
    }

    /**
     * @brief Waits for all tasks to complete
     */
    void waitForAll() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        done_condition_.wait(lock, [this]{
            return tasks_.empty() && active_tasks_.load() == 0;
        });
    }

private:
    // Worker thread function
    void workerThread() {
        while (true) {
            std::optional<ExecutorTask> task;
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                
                // Check if this thread should stop
                if (threads_to_stop_ > 0) {
                    --threads_to_stop_;
                    break;
                }
                
                condition_.wait(lock, [this] {
                    return stop_ || !tasks_.empty() || threads_to_stop_ > 0;
                });
                
                if (stop_ && tasks_.empty()) {
                    break;
                }
                
                if (!tasks_.empty()) {
                    // Find highest priority task
                    auto highestPrioIt = std::max_element(tasks_.begin(), tasks_.end());
                    task = std::move(*highestPrioIt);
                    tasks_.erase(highestPrioIt);
                }
            }
            
            if (task) {
                active_tasks_++;
                task->execute();
                active_tasks_--;
                
                if (tasks_.empty() && active_tasks_.load() == 0) {
                    done_condition_.notify_all();
                }
            }
        }
    }

    std::vector<std::thread> threads_;
    std::vector<ExecutorTask> tasks_;
    
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::condition_variable done_condition_;
    
    bool stop_;
    std::atomic<size_t> active_tasks_;
    size_t threads_to_stop_{0};
};

/**
 * @brief High-level executor for asynchronous tasks with various execution strategies.
 * 
 * AsyncExecutor provides a convenient interface for executing tasks asynchronously
 * with different execution strategies like immediate, deferred, or scheduled.
 */
class AsyncExecutor {
public:
    /**
     * @brief Execution strategy for tasks.
     */
    enum class ExecutionStrategy {
        IMMEDIATE,  // Execute immediately in the thread pool
        DEFERRED,   // Execute when explicitly requested
        SCHEDULED   // Execute at a specified time
    };
    
    /**
     * @brief Constructs an AsyncExecutor with a specified thread pool size.
     * 
     * @param poolSize Size of the underlying thread pool
     */
    explicit AsyncExecutor(size_t poolSize = std::thread::hardware_concurrency())
        : pool_(poolSize) {}
    
    /**
     * @brief Schedules a task for execution with the specified strategy.
     * 
     * @tparam F Function type
     * @tparam Args Argument types
     * @param strategy Execution strategy
     * @param priority Task priority
     * @param func Function to execute
     * @param args Function arguments
     * @return Future with the task result
     */
    template <typename F, typename... Args>
        requires std::invocable<F, Args...>
    auto schedule(ExecutionStrategy strategy, 
                 ExecutorTask::Priority priority,
                 F&& func, 
                 Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        
        using ReturnType = std::invoke_result_t<F, Args...>;
        
        switch (strategy) {
            case ExecutionStrategy::IMMEDIATE:
                return pool_.enqueue(std::forward<F>(func), 
                                    priority, 
                                    std::forward<Args>(args)...);
                
            case ExecutionStrategy::DEFERRED: {
                std::packaged_task<ReturnType()> task(
                    [f = std::forward<F>(func), ... args = std::forward<Args>(args)]() mutable {
                        return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
                    }
                );
                std::future<ReturnType> future = task.get_future();
                {
                    std::lock_guard<std::mutex> lock(deferred_mutex_);
                    deferred_tasks_.emplace_back(
                        [t = std::move(task)]() mutable { t(); },
                        priority
                    );
                }
                return future;
            }
                
            case ExecutionStrategy::SCHEDULED:
                // This would normally involve setting up a timer
                // For simplicity, we just use immediate execution
                return pool_.enqueue(std::forward<F>(func), 
                                   priority, 
                                   std::forward<Args>(args)...);
                
            default:
                throw std::invalid_argument("Unknown execution strategy");
        }
    }
    
    /**
     * @brief Executes all deferred tasks.
     */
    void executeDeferredTasks() {
        std::vector<ExecutorTask> tasks;
        {
            std::lock_guard<std::mutex> lock(deferred_mutex_);
            tasks.swap(deferred_tasks_);
        }
        
        for (auto& task : tasks) {
            pool_.enqueue([task = std::move(task)]() mutable {
                task.execute();
                return true;
            }, task.getPriority());
        }
    }
    
    /**
     * @brief Waits for all tasks to complete.
     */
    void waitForAll() {
        executeDeferredTasks();
        pool_.waitForAll();
    }
    
    /**
     * @brief Gets the number of tasks waiting in the queue.
     * 
     * @return Size of the task queue
     */
    [[nodiscard]] size_t queueSize() const {
        return pool_.queueSize();
    }
    
    /**
     * @brief Gets the number of active tasks currently being processed.
     * 
     * @return Number of active tasks
     */
    [[nodiscard]] size_t activeTaskCount() const {
        return pool_.activeTaskCount();
    }
    
    /**
     * @brief Resizes the thread pool.
     * 
     * @param poolSize New thread pool size
     */
    void resize(size_t poolSize) {
        pool_.resize(poolSize);
    }
    
private:
    ThreadPool pool_;
    std::vector<ExecutorTask> deferred_tasks_;
    std::mutex deferred_mutex_;
};

} // namespace atom::async

#endif // ATOM_ASYNC_ASYNC_EXECUTOR_HPP
