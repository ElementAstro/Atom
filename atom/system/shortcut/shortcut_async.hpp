/*
 * shortcut_async.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef SHORTCUT_ASYNC_HPP
#define SHORTCUT_ASYNC_HPP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

#include "shortcut_config.hpp"
#include "shortcut.h"
#include "status.h"

namespace shortcut_detector {

/**
 * @brief Task priority levels for shortcut operations
 */
enum class ShortcutTaskPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

/**
 * @brief Async task wrapper for shortcut operations
 */
class ShortcutTask {
public:
    template<typename F, typename... Args>
    ShortcutTask(ShortcutTaskPriority priority, std::string taskId, F&& func, Args&&... args)
        : priority_(priority),
          taskId_(std::move(taskId)),
          task_(std::make_shared<std::packaged_task<void()>>(
              std::bind(std::forward<F>(func), std::forward<Args>(args)...))),
          creationTime_(std::chrono::steady_clock::now()) {}

    void execute() {
        if (task_) {
            auto taskPtr = std::static_pointer_cast<std::packaged_task<void()>>(task_);
            (*taskPtr)();
        }
    }

    ShortcutTaskPriority getPriority() const { return priority_; }
    const std::string& getTaskId() const { return taskId_; }
    std::chrono::steady_clock::time_point getCreationTime() const { return creationTime_; }

    std::chrono::milliseconds getWaitTime() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - creationTime_);
    }

private:
    ShortcutTaskPriority priority_;
    std::string taskId_;
    std::shared_ptr<void> task_;
    std::chrono::steady_clock::time_point creationTime_;
};

/**
 * @brief Priority comparison for task queue
 */
struct ShortcutTaskComparator {
    bool operator()(const std::shared_ptr<ShortcutTask>& a, const std::shared_ptr<ShortcutTask>& b) const {
        if (a->getPriority() != b->getPriority()) {
            return a->getPriority() < b->getPriority();
        }
        // If same priority, prefer older tasks (FIFO)
        return a->getCreationTime() > b->getCreationTime();
    }
};

/**
 * @brief High-performance thread pool for shortcut operations
 */
class ShortcutThreadPool {
public:
    explicit ShortcutThreadPool(size_t minThreads = 2, size_t maxThreads = 10);
    ~ShortcutThreadPool();

    /**
     * @brief Submit a task for execution
     */
    template<typename F, typename... Args>
    auto submit(ShortcutTaskPriority priority, const std::string& taskId, F&& func, Args&&... args)
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

            auto taskWrapper = std::make_shared<ShortcutTask>(priority, taskId, [task]() { (*task)(); });
            taskQueue_.push(taskWrapper);
        }

        condition_.notify_one();

        // Scale up if needed
        scaleUpIfNeeded();

        return future;
    }

    /**
     * @brief Execute shortcut detection asynchronously
     */
    std::future<ShortcutCheckResult> detectShortcutAsync(const Shortcut& shortcut,
                                                         ShortcutTaskPriority priority = ShortcutTaskPriority::NORMAL);

    /**
     * @brief Get current number of active threads
     */
    size_t getActiveThreadCount() const;

    /**
     * @brief Get current queue size
     */
    size_t getQueueSize() const;

    /**
     * @brief Get thread pool statistics
     */
    struct PoolStats {
        size_t activeThreads;
        size_t totalThreads;
        size_t queueSize;
        size_t completedTasks;
        size_t failedTasks;
        std::chrono::milliseconds averageWaitTime;
        std::chrono::milliseconds averageExecutionTime;
        bool isShutdown;
    };

    PoolStats getStats() const;

    /**
     * @brief Shutdown the thread pool gracefully
     */
    void shutdown();

    /**
     * @brief Check if thread pool is shutdown
     */
    bool isShutdown() const;

    /**
     * @brief Set thread pool limits
     */
    void setThreadLimits(size_t minThreads, size_t maxThreads);

    /**
     * @brief Enable or disable dynamic scaling
     */
    void setDynamicScaling(bool enabled);

private:
    std::vector<std::thread> workers_;
    std::priority_queue<std::shared_ptr<ShortcutTask>,
                       std::vector<std::shared_ptr<ShortcutTask>>,
                       ShortcutTaskComparator> taskQueue_;

    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<bool> shutdown_{false};

    // Thread management
    std::atomic<size_t> activeThreads_{0};
    std::atomic<size_t> minThreads_;
    std::atomic<size_t> maxThreads_;
    std::atomic<bool> dynamicScaling_{true};

    // Statistics
    std::atomic<size_t> completedTasks_{0};
    std::atomic<size_t> failedTasks_{0};
    std::atomic<uint64_t> totalWaitTime_{0};
    std::atomic<uint64_t> totalExecutionTime_{0};

    // Scaling management
    mutable std::mutex scalingMutex_;
    std::chrono::steady_clock::time_point lastScaleCheck_;

    void workerLoop();
    void scaleUpIfNeeded();
    void scaleDownIfNeeded();
    void addWorker();
    void removeWorker();
    bool shouldScaleUp() const;
    bool shouldScaleDown() const;
};

/**
 * @brief Async shortcut operations manager
 */
class ShortcutAsyncManager {
public:
    static ShortcutAsyncManager& getInstance();

    /**
     * @brief Asynchronously detect shortcut
     */
    std::future<ShortcutCheckResult> detectAsync(const Shortcut& shortcut,
                                                 ShortcutTaskPriority priority = ShortcutTaskPriority::NORMAL);

    /**
     * @brief Asynchronously check keyboard hooks
     */
    std::future<bool> checkHooksAsync(ShortcutTaskPriority priority = ShortcutTaskPriority::NORMAL);

    /**
     * @brief Asynchronously get processes with hooks
     */
    std::future<std::vector<std::string>> getProcessesAsync(ShortcutTaskPriority priority = ShortcutTaskPriority::NORMAL);

    /**
     * @brief Asynchronously batch detect multiple shortcuts
     */
    std::future<std::vector<ShortcutCheckResult>> detectBatchAsync(
        const std::vector<Shortcut>& shortcuts,
        ShortcutTaskPriority priority = ShortcutTaskPriority::NORMAL);

    /**
     * @brief Get thread pool statistics
     */
    ShortcutThreadPool::PoolStats getPoolStats() const;

    /**
     * @brief Shutdown async operations
     */
    void shutdown();

private:
    ShortcutAsyncManager();
    ~ShortcutAsyncManager();

    ShortcutAsyncManager(const ShortcutAsyncManager&) = delete;
    ShortcutAsyncManager& operator=(const ShortcutAsyncManager&) = delete;

    std::unique_ptr<ShortcutThreadPool> threadPool_;
};

// Convenience functions for common async operations
template<typename F, typename... Args>
auto executeShortcutAsync(const std::string& taskId, F&& func, Args&&... args) {
    return ShortcutAsyncManager::getInstance().getThreadPool().submit(
        ShortcutTaskPriority::NORMAL,
        taskId,
        std::forward<F>(func),
        std::forward<Args>(args)...
    );
}

template<typename F, typename... Args>
auto executeShortcutAsyncHigh(const std::string& taskId, F&& func, Args&&... args) {
    return ShortcutAsyncManager::getInstance().getThreadPool().submit(
        ShortcutTaskPriority::HIGH,
        taskId,
        std::forward<F>(func),
        std::forward<Args>(args)...
    );
}

template<typename F, typename... Args>
auto executeShortcutAsyncCritical(const std::string& taskId, F&& func, Args&&... args) {
    return ShortcutAsyncManager::getInstance().getThreadPool().submit(
        ShortcutTaskPriority::CRITICAL,
        taskId,
        std::forward<F>(func),
        std::forward<Args>(args)...
    );
}

} // namespace shortcut_detector

#endif // SHORTCUT_ASYNC_HPP
