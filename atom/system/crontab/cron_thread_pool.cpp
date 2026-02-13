/*
 * cron_thread_pool.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "cron_thread_pool.hpp"

#include <spdlog/spdlog.h>
#include <cstdlib>
#include <sstream>

// ============================================================================
// CronThreadPool Implementation
// ============================================================================

CronThreadPool::CronThreadPool(size_t minThreads, size_t maxThreads)
    : minThreads_(minThreads), maxThreads_(maxThreads),
      lastScaleCheck_(std::chrono::steady_clock::now()) {

    // Create initial threads
    workers_.reserve(maxThreads);
    for (size_t i = 0; i < minThreads; ++i) {
        addWorker();
    }

    spdlog::info("Cron thread pool initialized with {}-{} threads", minThreads, maxThreads);
}

CronThreadPool::~CronThreadPool() {
    shutdown();
}

std::future<CronExecutionResult> CronThreadPool::executeCronJob(std::shared_ptr<CronJob> job,
                                                                CronTaskPriority priority) {
    if (!job) {
        auto promise = std::make_shared<std::promise<CronExecutionResult>>();
        promise->set_value(CronExecutionResult(false, -1, "", "Null job pointer"));
        return promise->get_future();
    }

    return submit(priority, job->getId(), [this, job]() -> CronExecutionResult {
        return executeJobInternal(job);
    });
}

size_t CronThreadPool::getActiveThreadCount() const {
    return activeThreads_.load();
}

size_t CronThreadPool::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return taskQueue_.size();
}

CronThreadPool::PoolStats CronThreadPool::getStats() const {
    std::lock_guard<std::mutex> lock(queueMutex_);

    auto completed = completedTasks_.load();
    auto failed = failedTasks_.load();
    auto totalTasks = completed + failed;

    std::chrono::milliseconds avgWait{0};
    std::chrono::milliseconds avgExec{0};

    if (totalTasks > 0) {
        avgWait = std::chrono::milliseconds(totalWaitTime_.load() / totalTasks);
        avgExec = std::chrono::milliseconds(totalExecutionTime_.load() / totalTasks);
    }

    return {
        activeThreads_.load(),
        workers_.size(),
        taskQueue_.size(),
        completed,
        failed,
        avgWait,
        avgExec,
        shutdown_.load()
    };
}

void CronThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        shutdown_ = true;
    }

    condition_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    workers_.clear();
    spdlog::info("Cron thread pool shutdown complete");
}

bool CronThreadPool::isShutdown() const {
    return shutdown_.load();
}

void CronThreadPool::setThreadLimits(size_t minThreads, size_t maxThreads) {
    std::lock_guard<std::mutex> lock(scalingMutex_);
    minThreads_ = minThreads;
    maxThreads_ = maxThreads;
    spdlog::info("Thread pool limits updated: {}-{}", minThreads, maxThreads);
}

void CronThreadPool::setDynamicScaling(bool enabled) {
    dynamicScaling_ = enabled;
    spdlog::info("Dynamic scaling {}", enabled ? "enabled" : "disabled");
}

void CronThreadPool::workerLoop() {
    activeThreads_++;

    while (true) {
        std::shared_ptr<CronTask> task;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);

            condition_.wait(lock, [this] {
                return shutdown_ || !taskQueue_.empty();
            });

            if (shutdown_ && taskQueue_.empty()) {
                break;
            }

            if (!taskQueue_.empty()) {
                task = taskQueue_.top();
                taskQueue_.pop();

                // Update wait time metrics
                auto waitTime = task->getWaitTime();
                totalWaitTime_ += waitTime.count();
            }
        }

        if (task) {
            auto startTime = std::chrono::steady_clock::now();

            try {
                CRON_TIMER(CronEventType::JOB_STARTED, task->getJobId());
                task->execute();
                completedTasks_++;
                CRON_EMIT_EVENT(CronEventType::JOB_COMPLETED, task->getJobId(), "", "");
            } catch (const std::exception& e) {
                failedTasks_++;
                spdlog::error("Cron task execution failed for job {}: {}", task->getJobId(), e.what());
                CRON_EMIT_EVENT(CronEventType::JOB_FAILED, task->getJobId(), "", e.what());
            } catch (...) {
                failedTasks_++;
                spdlog::error("Cron task execution failed for job {} with unknown exception", task->getJobId());
                CRON_EMIT_EVENT(CronEventType::JOB_FAILED, task->getJobId(), "", "unknown exception");
            }

            auto endTime = std::chrono::steady_clock::now();
            auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                endTime - startTime);
            totalExecutionTime_ += executionTime.count();
        }

        // Check if we should scale down
        if (dynamicScaling_.load()) {
            scaleDownIfNeeded();
        }
    }

    activeThreads_--;
}

void CronThreadPool::scaleUpIfNeeded() {
    if (!dynamicScaling_.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(scalingMutex_);

    auto now = std::chrono::steady_clock::now();
    if (now - lastScaleCheck_ < std::chrono::seconds(5)) {
        return; // Don't scale too frequently
    }
    lastScaleCheck_ = now;

    if (shouldScaleUp()) {
        addWorker();
    }
}

void CronThreadPool::scaleDownIfNeeded() {
    if (!dynamicScaling_.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(scalingMutex_);

    if (shouldScaleDown()) {
        removeWorker();
    }
}

void CronThreadPool::addWorker() {
    if (workers_.size() >= maxThreads_.load()) {
        return;
    }

    workers_.emplace_back([this] { workerLoop(); });
    spdlog::debug("Added worker thread, total: {}", workers_.size());
}

void CronThreadPool::removeWorker() {
    if (workers_.size() <= minThreads_.load()) {
        return;
    }

    // This is a simplified implementation
    // In practice, you'd need a more sophisticated way to remove specific threads
    spdlog::debug("Would remove worker thread, current: {}", workers_.size());
}

bool CronThreadPool::shouldScaleUp() const {
    size_t queueSize = getQueueSize();
    size_t activeThreads = activeThreads_.load();

    // Scale up if queue is growing and we have capacity
    return queueSize > activeThreads * 2 && workers_.size() < maxThreads_.load();
}

bool CronThreadPool::shouldScaleDown() const {
    size_t queueSize = getQueueSize();
    size_t activeThreads = activeThreads_.load();

    // Scale down if queue is small and we have excess threads
    return queueSize == 0 && activeThreads > minThreads_.load() && workers_.size() > minThreads_.load();
}

CronExecutionResult CronThreadPool::executeJobInternal(std::shared_ptr<CronJob> job) {
    auto startTime = std::chrono::steady_clock::now();

    try {
        // Build command
        std::string command = job->command_;

        // Execute command
        int exitCode = std::system(command.c_str());

        auto endTime = std::chrono::steady_clock::now();
        auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        bool success = (exitCode == 0);

        // Record execution in job
        job->recordExecution(success, success ? "" : "Command failed with exit code " + std::to_string(exitCode));

        return CronExecutionResult(success, exitCode, "", "", executionTime);

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        job->recordExecution(false, e.what());
        return CronExecutionResult(false, -1, "", e.what(), executionTime);
    }
}

// ============================================================================
// CronThreadPoolManager Implementation
// ============================================================================

CronThreadPoolManager& CronThreadPoolManager::getInstance() {
    static CronThreadPoolManager instance;
    return instance;
}

CronThreadPoolManager::~CronThreadPoolManager() {
    shutdown();
}

CronThreadPool& CronThreadPoolManager::getThreadPool() {
    if (!initialized_.load()) {
        initialize();
    }
    return *threadPool_;
}

void CronThreadPoolManager::initialize() {
    std::lock_guard<std::mutex> lock(initMutex_);

    if (initialized_.load()) {
        return;
    }

    const auto& config = CRON_CONFIG();
    threadPool_ = std::make_unique<CronThreadPool>(config.minThreads, config.maxThreads);
    threadPool_->setDynamicScaling(config.enableDynamicScaling);

    initialized_ = true;
    spdlog::info("Cron thread pool manager initialized");
}

void CronThreadPoolManager::shutdown() {
    if (threadPool_) {
        threadPool_->shutdown();
        threadPool_.reset();
    }
    initialized_ = false;
}
