/*
 * env_async.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "env_async.hpp"

#include <spdlog/spdlog.h>
#include "env_core.hpp"
#include "env_file_io.hpp"
#include "env_path.hpp"

namespace atom::utils {

// ============================================================================
// EnvThreadPool Implementation
// ============================================================================

EnvThreadPool::EnvThreadPool(size_t numThreads) {
    workers_.reserve(numThreads);
    
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this] { workerLoop(); });
    }
    
    spdlog::info("Environment thread pool initialized with {} threads", numThreads);
}

EnvThreadPool::~EnvThreadPool() {
    shutdown();
}

size_t EnvThreadPool::getActiveThreadCount() const {
    return activeThreads_.load();
}

size_t EnvThreadPool::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return taskQueue_.size();
}

void EnvThreadPool::shutdown() {
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
    spdlog::info("Environment thread pool shutdown complete");
}

bool EnvThreadPool::isShutdown() const {
    return shutdown_.load();
}

void EnvThreadPool::workerLoop() {
    activeThreads_++;
    
    while (true) {
        std::shared_ptr<EnvTask> task;
        
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
            }
        }
        
        if (task) {
            try {
                task->execute();
            } catch (const std::exception& e) {
                spdlog::error("Environment task execution failed: {}", e.what());
                ENV_EMIT_EVENT(EnvEventType::ERROR_OCCURRED, "", "", e.what());
            } catch (...) {
                spdlog::error("Environment task execution failed with unknown exception");
                ENV_EMIT_EVENT(EnvEventType::ERROR_OCCURRED, "", "", "unknown exception");
            }
        }
    }
    
    activeThreads_--;
}

// ============================================================================
// EnvAsyncManager Implementation
// ============================================================================

EnvAsyncManager& EnvAsyncManager::getInstance() {
    static EnvAsyncManager instance;
    return instance;
}

EnvAsyncManager::EnvAsyncManager() {
    const auto& config = ENV_CONFIG();
    size_t numThreads = config.enableAsyncOperations ? 
                       std::min(config.maxAsyncTasks, std::thread::hardware_concurrency()) : 1;
    
    threadPool_ = std::make_unique<EnvThreadPool>(numThreads);
    spdlog::info("Environment async manager initialized");
}

EnvAsyncManager::~EnvAsyncManager() {
    shutdown();
}

std::future<EnvResult<void>> EnvAsyncManager::setEnvAsync(const String& key, const String& value,
                                                          EnvTaskPriority priority) {
    return threadPool_->submit(priority, [key, value]() -> EnvResult<void> {
        ENV_TIMER(EnvEventType::VARIABLE_SET, key);
        
        try {
            bool success = EnvCore::setEnv(key, value);
            if (success) {
                return EnvResult<void>(true);
            } else {
                return EnvResult<void>(false, {}, "Failed to set environment variable");
            }
        } catch (const std::exception& e) {
            return EnvResult<void>(false, {}, String(e.what()));
        }
    });
}

std::future<EnvResult<String>> EnvAsyncManager::getEnvAsync(const String& key, const String& defaultValue,
                                                            EnvTaskPriority priority) {
    return threadPool_->submit(priority, [key, defaultValue]() -> EnvResult<String> {
        ENV_TIMER(EnvEventType::VARIABLE_GET, key);
        
        try {
            String value = EnvCore::getEnv(key, defaultValue);
            return EnvResult<String>(true, value);
        } catch (const std::exception& e) {
            return EnvResult<String>(false, defaultValue, String(e.what()));
        }
    });
}

std::future<EnvResult<void>> EnvAsyncManager::unsetEnvAsync(const String& key,
                                                            EnvTaskPriority priority) {
    return threadPool_->submit(priority, [key]() -> EnvResult<void> {
        ENV_TIMER(EnvEventType::VARIABLE_UNSET, key);
        
        try {
            EnvCore::unsetEnv(key);
            return EnvResult<void>(true);
        } catch (const std::exception& e) {
            return EnvResult<void>(false, {}, String(e.what()));
        }
    });
}

std::future<EnvResult<size_t>> EnvAsyncManager::setBatchAsync(const HashMap<String, String>& vars,
                                                              EnvTaskPriority priority) {
    return threadPool_->submit(priority, [vars]() -> EnvResult<size_t> {
        ENV_TIMER(EnvEventType::BATCH_OPERATION, "setBatch");
        
        try {
            size_t count = EnvCore::setBatch(vars);
            return EnvResult<size_t>(true, count);
        } catch (const std::exception& e) {
            return EnvResult<size_t>(false, 0, String(e.what()));
        }
    });
}

std::future<EnvResult<HashMap<String, String>>> EnvAsyncManager::getBatchAsync(
    const std::vector<String>& keys, EnvTaskPriority priority) {
    
    return threadPool_->submit(priority, [keys]() -> EnvResult<HashMap<String, String>> {
        ENV_TIMER(EnvEventType::BATCH_OPERATION, "getBatch");
        
        try {
            HashMap<String, String> result;
            for (const auto& key : keys) {
                result[key] = EnvCore::getEnv(key, "");
            }
            return EnvResult<HashMap<String, String>>(true, result);
        } catch (const std::exception& e) {
            return EnvResult<HashMap<String, String>>(false, {}, String(e.what()));
        }
    });
}

std::future<EnvResult<bool>> EnvAsyncManager::loadFromFileAsync(const String& filePath, bool overwrite,
                                                                EnvTaskPriority priority) {
    return threadPool_->submit(priority, [filePath, overwrite]() -> EnvResult<bool> {
        ENV_TIMER(EnvEventType::FILE_OPERATION, filePath);
        
        try {
            std::filesystem::path path(std::string(filePath.data(), filePath.size()));
            bool success = EnvFileIO::loadFromFile(path, overwrite);
            return EnvResult<bool>(success, success);
        } catch (const std::exception& e) {
            return EnvResult<bool>(false, false, String(e.what()));
        }
    });
}

std::future<EnvResult<bool>> EnvAsyncManager::saveToFileAsync(const String& filePath, 
                                                              const HashMap<String, String>& vars,
                                                              EnvTaskPriority priority) {
    return threadPool_->submit(priority, [filePath, vars]() -> EnvResult<bool> {
        ENV_TIMER(EnvEventType::FILE_OPERATION, filePath);
        
        try {
            std::filesystem::path path(std::string(filePath.data(), filePath.size()));
            bool success = EnvFileIO::saveToFile(path, vars);
            return EnvResult<bool>(success, success);
        } catch (const std::exception& e) {
            return EnvResult<bool>(false, false, String(e.what()));
        }
    });
}

std::future<EnvResult<bool>> EnvAsyncManager::addToPathAsync(const String& path, bool prepend,
                                                             EnvTaskPriority priority) {
    return threadPool_->submit(priority, [path, prepend]() -> EnvResult<bool> {
        ENV_TIMER(EnvEventType::PATH_OPERATION, path);
        
        try {
            auto result = EnvPath::addToPath(path, prepend);
            bool success = (result == PathOperationResult::SUCCESS);
            return EnvResult<bool>(success, success);
        } catch (const std::exception& e) {
            return EnvResult<bool>(false, false, String(e.what()));
        }
    });
}

std::future<EnvResult<bool>> EnvAsyncManager::removeFromPathAsync(const String& path,
                                                                  EnvTaskPriority priority) {
    return threadPool_->submit(priority, [path]() -> EnvResult<bool> {
        ENV_TIMER(EnvEventType::PATH_OPERATION, path);
        
        try {
            auto result = EnvPath::removeFromPath(path);
            bool success = (result == PathOperationResult::SUCCESS);
            return EnvResult<bool>(success, success);
        } catch (const std::exception& e) {
            return EnvResult<bool>(false, false, String(e.what()));
        }
    });
}

EnvAsyncManager::PoolStats EnvAsyncManager::getPoolStats() const {
    return {
        threadPool_->getActiveThreadCount(),
        threadPool_->getQueueSize(),
        threadPool_->isShutdown()
    };
}

void EnvAsyncManager::shutdown() {
    if (threadPool_) {
        threadPool_->shutdown();
    }
}

} // namespace atom::utils
