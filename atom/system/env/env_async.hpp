/*
 * env_async.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_ENV_ASYNC_HPP
#define ATOM_SYSTEM_ENV_ASYNC_HPP

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "atom/containers/high_performance.hpp"
#include "env_config.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;

/**
 * @brief Task priority levels for environment operations
 */
enum class EnvTaskPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

/**
 * @brief Environment operation result
 */
template<typename T>
struct EnvResult {
    bool success;
    T value;
    String error;
    
    EnvResult(bool s = true, T v = T{}, const String& e = "")
        : success(s), value(std::move(v)), error(e) {}
    
    explicit operator bool() const { return success; }
};

/**
 * @brief Async task wrapper for environment operations
 */
class EnvTask {
public:
    template<typename F, typename... Args>
    EnvTask(EnvTaskPriority priority, F&& func, Args&&... args)
        : priority_(priority),
          task_(std::make_shared<std::packaged_task<void()>>(
              std::bind(std::forward<F>(func), std::forward<Args>(args)...))) {}
    
    void execute() {
        if (task_) {
            auto taskPtr = std::static_pointer_cast<std::packaged_task<void()>>(task_);
            (*taskPtr)();
        }
    }
    
    EnvTaskPriority getPriority() const { return priority_; }

private:
    EnvTaskPriority priority_;
    std::shared_ptr<void> task_;
};

/**
 * @brief Priority comparison for task queue
 */
struct EnvTaskComparator {
    bool operator()(const std::shared_ptr<EnvTask>& a, const std::shared_ptr<EnvTask>& b) const {
        return a->getPriority() < b->getPriority();
    }
};

/**
 * @brief High-performance thread pool for environment operations
 */
class EnvThreadPool {
public:
    explicit EnvThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~EnvThreadPool();
    
    /**
     * @brief Submit a task for execution
     */
    template<typename F, typename... Args>
    auto submit(EnvTaskPriority priority, F&& func, Args&&... args) 
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
            
            auto taskWrapper = std::make_shared<EnvTask>(priority, [task]() { (*task)(); });
            taskQueue_.push(taskWrapper);
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

private:
    std::vector<std::thread> workers_;
    std::priority_queue<std::shared_ptr<EnvTask>, 
                       std::vector<std::shared_ptr<EnvTask>>, 
                       EnvTaskComparator> taskQueue_;
    
    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<bool> shutdown_{false};
    std::atomic<size_t> activeThreads_{0};
    
    void workerLoop();
};

/**
 * @brief Async environment operations manager
 */
class EnvAsyncManager {
public:
    static EnvAsyncManager& getInstance();
    
    /**
     * @brief Asynchronously set environment variable
     */
    std::future<EnvResult<void>> setEnvAsync(const String& key, const String& value,
                                             EnvTaskPriority priority = EnvTaskPriority::NORMAL);
    
    /**
     * @brief Asynchronously get environment variable
     */
    std::future<EnvResult<String>> getEnvAsync(const String& key, const String& defaultValue = "",
                                               EnvTaskPriority priority = EnvTaskPriority::NORMAL);
    
    /**
     * @brief Asynchronously unset environment variable
     */
    std::future<EnvResult<void>> unsetEnvAsync(const String& key,
                                               EnvTaskPriority priority = EnvTaskPriority::NORMAL);
    
    /**
     * @brief Asynchronously set multiple environment variables
     */
    std::future<EnvResult<size_t>> setBatchAsync(const HashMap<String, String>& vars,
                                                 EnvTaskPriority priority = EnvTaskPriority::NORMAL);
    
    /**
     * @brief Asynchronously get multiple environment variables
     */
    std::future<EnvResult<HashMap<String, String>>> getBatchAsync(
        const std::vector<String>& keys,
        EnvTaskPriority priority = EnvTaskPriority::NORMAL);
    
    /**
     * @brief Asynchronously load environment from file
     */
    std::future<EnvResult<bool>> loadFromFileAsync(const String& filePath, bool overwrite = false,
                                                   EnvTaskPriority priority = EnvTaskPriority::NORMAL);
    
    /**
     * @brief Asynchronously save environment to file
     */
    std::future<EnvResult<bool>> saveToFileAsync(const String& filePath, 
                                                 const HashMap<String, String>& vars = {},
                                                 EnvTaskPriority priority = EnvTaskPriority::NORMAL);
    
    /**
     * @brief Asynchronously add path to PATH variable
     */
    std::future<EnvResult<bool>> addToPathAsync(const String& path, bool prepend = false,
                                                EnvTaskPriority priority = EnvTaskPriority::NORMAL);
    
    /**
     * @brief Asynchronously remove path from PATH variable
     */
    std::future<EnvResult<bool>> removeFromPathAsync(const String& path,
                                                     EnvTaskPriority priority = EnvTaskPriority::NORMAL);
    
    /**
     * @brief Get thread pool statistics
     */
    struct PoolStats {
        size_t activeThreads;
        size_t queueSize;
        bool isShutdown;
    };
    
    PoolStats getPoolStats() const;
    
    /**
     * @brief Shutdown async operations
     */
    void shutdown();

private:
    EnvAsyncManager();
    ~EnvAsyncManager();
    
    EnvAsyncManager(const EnvAsyncManager&) = delete;
    EnvAsyncManager& operator=(const EnvAsyncManager&) = delete;
    
    std::unique_ptr<EnvThreadPool> threadPool_;
};

// Note: Convenience functions would need access to thread pool
// They are omitted here to avoid circular dependencies

} // namespace atom::utils

#endif // ATOM_SYSTEM_ENV_ASYNC_HPP
