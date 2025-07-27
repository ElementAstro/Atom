/*
 * cron_thread_pool.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef CRON_THREAD_POOL_HPP
#define CRON_THREAD_POOL_HPP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <type_traits>

#include "cron_config.hpp"
#include "cron_job.hpp"

/**
 * @brief Task priority levels for cron job execution
 */
enum class CronTaskPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

/**
 * @brief Execution result for cron jobs
 */
struct CronExecutionResult {
    bool success;
    int exitCode;
    std::string output;
    std::string error;
    std::chrono::milliseconds executionTime;

    CronExecutionResult(bool s = false, int code = -1, std::string out = "",
                       std::string err = "", std::chrono::milliseconds time = std::chrono::milliseconds::zero())
        : success(s), exitCode(code), output(std::move(out)), error(std::move(err)), executionTime(time) {}
};

/**
 * @brief Cron task wrapper for thread pool execution
 */
class CronTask {
public:
    template<typename F, typename... Args>
    CronTask(CronTaskPriority priority, std::string jobId, F&& func, Args&&... args)
        : priority_(priority),
          jobId_(std::move(jobId)),
          task_(std::make_shared<std::packaged_task<void()>>(
              std::bind(std::forward<F>(func), std::forward<Args>(args)...))),
          creationTime_(std::chrono::steady_clock::now()) {}

    void execute() {
        if (task_) {
            auto taskPtr = std::static_pointer_cast<std::packaged_task<void()>>(task_);
            (*taskPtr)();
        }
    }

    CronTaskPriority getPriority() const { return priority_; }
    const std::string& getJobId() const { return jobId_; }
    std::chrono::steady_clock::time_point getCreationTime() const { return creationTime_; }

    std::chrono::milliseconds getWaitTime() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - creationTime_);
    }

private:
    CronTaskPriority priority_;
    std::string jobId_;
    std::shared_ptr<void> task_;
    std::chrono::steady_clock::time_point creationTime_;
};

/**
 * @brief Priority comparison for task queue
 */
struct CronTaskComparator {
    bool operator()(const std::shared_ptr<CronTask>& a, const std::shared_ptr<CronTask>& b) const {
        if (a->getPriority() != b->getPriority()) {
            return a->getPriority() < b->getPriority();
        }
        // If same priority, prefer older tasks (FIFO)
        return a->getCreationTime() > b->getCreationTime();
    }
};

/**
 * @brief High-performance thread pool for cron job execution
 */
class CronThreadPool {
public:
    explicit CronThreadPool(size_t minThreads = 2, size_t maxThreads = 20);
    ~CronThreadPool();

    /**
     * @brief Submit a cron job for execution
     */
    template<typename F, typename... Args>
    auto submit(CronTaskPriority priority, const std::string& jobId, F&& func, Args&&... args)
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

            auto taskWrapper = std::make_shared<CronTask>(priority, jobId, [task]() { (*task)(); });
            taskQueue_.push(taskWrapper);

            // Update metrics
            CRON_METRICS().schedulingLatency += taskWrapper->getWaitTime().count();
        }

        condition_.notify_one();

        // Scale up if needed
        scaleUpIfNeeded();

        return future;
    }

    /**
     * @brief Execute a cron job directly
     */
    std::future<CronExecutionResult> executeCronJob(std::shared_ptr<CronJob> job,
                                                    CronTaskPriority priority = CronTaskPriority::NORMAL);

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
    std::priority_queue<std::shared_ptr<CronTask>,
                       std::vector<std::shared_ptr<CronTask>>,
                       CronTaskComparator> taskQueue_;

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

    CronExecutionResult executeJobInternal(std::shared_ptr<CronJob> job);
};

/**
 * @brief Global thread pool manager for cron system
 */
class CronThreadPoolManager {
public:
    static CronThreadPoolManager& getInstance();

    /**
     * @brief Get the global thread pool
     */
    CronThreadPool& getThreadPool();

    /**
     * @brief Initialize thread pool with configuration
     */
    void initialize();

    /**
     * @brief Shutdown thread pool
     */
    void shutdown();

private:
    CronThreadPoolManager() = default;
    ~CronThreadPoolManager();

    CronThreadPoolManager(const CronThreadPoolManager&) = delete;
    CronThreadPoolManager& operator=(const CronThreadPoolManager&) = delete;

    std::unique_ptr<CronThreadPool> threadPool_;
    std::mutex initMutex_;
    std::atomic<bool> initialized_{false};
};

// Convenience functions for job execution
template<typename F, typename... Args>
auto executeCronAsync(const std::string& jobId, F&& func, Args&&... args) {
    return CronThreadPoolManager::getInstance().getThreadPool().submit(
        CronTaskPriority::NORMAL,
        jobId,
        std::forward<F>(func),
        std::forward<Args>(args)...
    );
}

template<typename F, typename... Args>
auto executeCronAsyncHigh(const std::string& jobId, F&& func, Args&&... args) {
    return CronThreadPoolManager::getInstance().getThreadPool().submit(
        CronTaskPriority::HIGH,
        jobId,
        std::forward<F>(func),
        std::forward<Args>(args)...
    );
}

template<typename F, typename... Args>
auto executeCronAsyncCritical(const std::string& jobId, F&& func, Args&&... args) {
    return CronThreadPoolManager::getInstance().getThreadPool().submit(
        CronTaskPriority::CRITICAL,
        jobId,
        std::forward<F>(func),
        std::forward<Args>(args)...
    );
}

#endif // CRON_THREAD_POOL_HPP
