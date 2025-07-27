/*
 * thread_pool.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "thread_pool.hpp"

#include <spdlog/spdlog.h>

namespace atom::system {

// ============================================================================
// CommandThreadPool Implementation
// ============================================================================

CommandThreadPool::CommandThreadPool(const CommandSystemConfig& config)
    : config_(config) {
    
    // Start with minimum number of threads
    workers_.reserve(config_.maxThreads);
    
    for (size_t i = 0; i < config_.minThreads; ++i) {
        workers_.emplace_back([this] { workerLoop(); });
    }
    
    spdlog::info("CommandThreadPool initialized with {} threads", config_.minThreads);
}

CommandThreadPool::~CommandThreadPool() {
    shutdown();
}

size_t CommandThreadPool::getActiveThreadCount() const {
    return activeThreads_.load();
}

size_t CommandThreadPool::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return taskQueue_.size();
}

void CommandThreadPool::shutdown() {
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
    spdlog::info("CommandThreadPool shutdown complete");
}

bool CommandThreadPool::isShutdown() const {
    return shutdown_.load();
}

CommandThreadPool& CommandThreadPool::getInstance() {
    static CommandThreadPool instance;
    return instance;
}

void CommandThreadPool::workerLoop() {
    activeThreads_++;
    
    while (true) {
        std::shared_ptr<Task> task;
        
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
                const_cast<CommandSystemMetrics&>(COMMAND_METRICS()).queuedTasks--;
            }
        }
        
        if (task) {
            TaskExecutionGuard guard;
            
            try {
                task->execute();
                const_cast<CommandSystemMetrics&>(COMMAND_METRICS()).successfulCommands++;
            } catch (const std::exception& e) {
                spdlog::error("Task execution failed: {}", e.what());
                const_cast<CommandSystemMetrics&>(COMMAND_METRICS()).failedCommands++;
            } catch (...) {
                spdlog::error("Task execution failed with unknown exception");
                const_cast<CommandSystemMetrics&>(COMMAND_METRICS()).failedCommands++;
            }
        }
    }
    
    activeThreads_--;
}

void CommandThreadPool::adjustThreadCount() {
    // Dynamic thread adjustment based on queue size and load
    size_t queueSize = getQueueSize();
    size_t currentThreads = workers_.size();
    
    if (queueSize > currentThreads * 2 && currentThreads < config_.maxThreads) {
        // Add more threads if queue is backing up
        workers_.emplace_back([this] { workerLoop(); });
        spdlog::debug("Added thread to pool, now have {} threads", workers_.size());
    }
}

// ============================================================================
// TaskExecutionGuard Implementation
// ============================================================================

TaskExecutionGuard::TaskExecutionGuard() 
    : startTime_(std::chrono::steady_clock::now()) {
    const_cast<CommandSystemMetrics&>(COMMAND_METRICS()).totalCommands++;
}

TaskExecutionGuard::~TaskExecutionGuard() {
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime_);
    
    const_cast<CommandSystemMetrics&>(COMMAND_METRICS()).totalExecutionTime += 
        duration.count();
}

} // namespace atom::system
