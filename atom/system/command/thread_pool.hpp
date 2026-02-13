/*
 * thread_pool.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_THREAD_POOL_HPP
#define ATOM_SYSTEM_COMMAND_THREAD_POOL_HPP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "atom/macro.hpp"
#include "config.hpp"

namespace atom::system {

/**
 * @brief Task priority levels for command execution
 */
enum class TaskPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

/**
 * @brief Task wrapper for the thread pool
 */
class Task {
public:
    template<typename F, typename... Args>
    Task(TaskPriority priority, F&& func, Args&&... args)
        : priority_(priority),
          task_(std::make_shared<std::packaged_task<void()>>(
              std::bind(std::forward<F>(func), std::forward<Args>(args)...))) {}

    void execute() {
        if (task_) {
            auto taskPtr = std::static_pointer_cast<std::packaged_task<void()>>(task_);
            (*taskPtr)();
        }
    }

    TaskPriority getPriority() const { return priority_; }



private:
    TaskPriority priority_;
    std::shared_ptr<void> task_;
};

/**
 * @brief Priority comparison for task queue
 */
struct TaskComparator {
    bool operator()(const std::shared_ptr<Task>& a, const std::shared_ptr<Task>& b) const {
        return a->getPriority() < b->getPriority();
    }
};

/**
 * @brief High-performance thread pool optimized for command execution
 */
class CommandThreadPool {
public:
    /**
     * @brief Construct thread pool with configuration
     */
    explicit CommandThreadPool(const CommandSystemConfig& config = COMMAND_CONFIG());

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~CommandThreadPool();

    /**
     * @brief Submit a task for execution
     * @param priority Task priority
     * @param func Function to execute
     * @param args Function arguments
     * @return Future for the task result
     */
    template<typename F, typename... Args>
    auto submit(TaskPriority priority, F&& func, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {

        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(func), std::forward<Args>(args)...)
        );

        auto future = task->get_future();

        {
            std::lock_guard<std::mutex> lock(queueMutex_);

            if (shutdown_) {
                throw std::runtime_error("Cannot submit task to shutdown thread pool");
            }

            auto taskWrapper = std::make_shared<Task>(priority, [task]() { (*task)(); });
            taskQueue_.push(taskWrapper);

            const_cast<CommandSystemMetrics&>(COMMAND_METRICS()).queuedTasks++;
        }

        condition_.notify_one();
        return future;
    }

    /**
     * @brief Get current number of active threads
     */
    size_t getActiveThreadCount() const;

    /**
     * @brief Get current queue size
     */
    size_t getQueueSize() const;

    /**
     * @brief Shutdown the thread pool gracefully
     */
    void shutdown();

    /**
     * @brief Check if thread pool is shutdown
     */
    bool isShutdown() const;

    /**
     * @brief Get singleton instance
     */
    static CommandThreadPool& getInstance();

private:
    std::vector<std::thread> workers_;
    std::priority_queue<std::shared_ptr<Task>,
                       std::vector<std::shared_ptr<Task>>,
                       TaskComparator> taskQueue_;

    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<bool> shutdown_{false};
    std::atomic<size_t> activeThreads_{0};

    CommandSystemConfig config_;

    void workerLoop();
    void adjustThreadCount();
};

/**
 * @brief RAII wrapper for thread pool task execution
 */
class TaskExecutionGuard {
public:
    TaskExecutionGuard();
    ~TaskExecutionGuard();

private:
    std::chrono::steady_clock::time_point startTime_;
};

// Convenience functions for common operations
template<typename F, typename... Args>
auto executeAsync(F&& func, Args&&... args) {
    return CommandThreadPool::getInstance().submit(
        TaskPriority::NORMAL,
        std::forward<F>(func),
        std::forward<Args>(args)...
    );
}

template<typename F, typename... Args>
auto executeAsyncHigh(F&& func, Args&&... args) {
    return CommandThreadPool::getInstance().submit(
        TaskPriority::HIGH,
        std::forward<F>(func),
        std::forward<Args>(args)...
    );
}

template<typename F, typename... Args>
auto executeAsyncCritical(F&& func, Args&&... args) {
    return CommandThreadPool::getInstance().submit(
        TaskPriority::CRITICAL,
        std::forward<F>(func),
        std::forward<Args>(args)...
    );
}

} // namespace atom::system

#endif // ATOM_SYSTEM_COMMAND_THREAD_POOL_HPP
