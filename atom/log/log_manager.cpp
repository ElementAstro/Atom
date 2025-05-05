// log_manager.cpp
/*
 * log_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-4

Description: Log Manager Implementation

**************************************************/

#include "log_manager.hpp"

#include <future>
#include <stdexcept>

namespace atom::log {

// Configure default logger
void LogManager::configureDefaultLogger(const fs::path& file_name,
                                        LogLevel min_level,
                                        size_t max_file_size, int max_files) {
    std::unique_lock lock(loggers_mutex_);
    default_logger_ = std::make_shared<Logger>(file_name, min_level,
                                               max_file_size, max_files);
}

// Create standard synchronous logger
std::shared_ptr<Logger> LogManager::createLogger(std::string_view name,
                                                 const fs::path& file_name,
                                                 LogLevel min_level,
                                                 size_t max_file_size,
                                                 int max_files) {
    if (name.empty()) {
        throw std::invalid_argument("Logger name cannot be empty");
    }

    // Create a string copy from the string_view for map storage
    std::string name_str{name};

    std::unique_lock lock(loggers_mutex_);

    // Check if logger already exists
    auto it = loggers_.find(name_str);
    if (it != loggers_.end()) {
        return it->second;
    }

    // Create new logger
    auto logger = std::make_shared<Logger>(file_name, min_level, max_file_size,
                                           max_files);
    loggers_[name_str] = logger;

    // If no default logger exists, set this as default
    if (!default_logger_) {
        default_logger_ = logger;
    }

    return logger;
}

// Create asynchronous logger
std::shared_ptr<AsyncLogger> LogManager::createAsyncLogger(
    std::string_view name, const fs::path& file_name, LogLevel min_level,
    size_t max_file_size, int max_files, size_t thread_count) {
    if (name.empty()) {
        throw std::invalid_argument("Logger name cannot be empty");
    }

    // Create a string copy from the string_view for map storage
    std::string name_str{name};

    std::unique_lock lock(loggers_mutex_);

    // Check if logger already exists
    auto it = async_loggers_.find(name_str);
    if (it != async_loggers_.end()) {
        return it->second;
    }

    // Create new async logger
    auto logger = std::make_shared<AsyncLogger>(
        file_name, min_level, max_file_size, max_files, thread_count);
    async_loggers_[name_str] = logger;

    return logger;
}

// Create memory-mapped logger
std::shared_ptr<MmapLogger> LogManager::createMmapLogger(
    std::string_view name, const fs::path& file_name, LogLevel min_level,
    size_t buffer_size, int max_files) {
    if (name.empty()) {
        throw std::invalid_argument("Logger name cannot be empty");
    }

    // Create a string copy from the string_view for map storage
    std::string name_str{name};

    std::unique_lock lock(loggers_mutex_);

    // Check if logger already exists
    auto it = mmap_loggers_.find(name_str);
    if (it != mmap_loggers_.end()) {
        return it->second;
    }

    // Create new memory-mapped logger
    auto logger = std::make_shared<MmapLogger>(file_name, min_level,
                                               buffer_size, max_files);
    mmap_loggers_[name_str] = logger;

    return logger;
}

// Get default logger
std::shared_ptr<Logger> LogManager::getDefaultLogger() {
    std::unique_lock lock(loggers_mutex_);

    // Create a default logger if none exists
    if (!default_logger_) {
        fs::path default_path = "logs/atom.log";

        // Ensure directory exists
        try {
            fs::create_directories(default_path.parent_path());
        } catch (const std::exception& e) {
            // Fallback to current directory if creation fails
            default_path = "atom.log";
            fprintf(stderr, "Warning: Failed to create log directory: %s\n",
                    e.what());
        }

        default_logger_ = std::make_shared<Logger>(default_path);
    }
    return default_logger_;
}

// Get named logger
std::optional<std::shared_ptr<Logger>> LogManager::getLogger(
    std::string_view name) {
    std::shared_lock lock(loggers_mutex_);
    auto it = loggers_.find(std::string{name});
    if (it != loggers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// Get named async logger
std::optional<std::shared_ptr<AsyncLogger>> LogManager::getAsyncLogger(
    std::string_view name) {
    std::shared_lock lock(loggers_mutex_);
    auto it = async_loggers_.find(std::string{name});
    if (it != async_loggers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// Get named memory-mapped logger
std::optional<std::shared_ptr<MmapLogger>> LogManager::getMmapLogger(
    std::string_view name) {
    std::shared_lock lock(loggers_mutex_);
    auto it = mmap_loggers_.find(std::string{name});
    if (it != mmap_loggers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// Remove logger
bool LogManager::removeLogger(std::string_view name) {
    std::unique_lock lock(loggers_mutex_);

    std::string name_str{name};
    bool removed = false;

    // Try to remove from each map
    auto it1 = loggers_.find(name_str);
    if (it1 != loggers_.end()) {
        if (it1->second == default_logger_) {
            default_logger_ = nullptr;
        }
        loggers_.erase(it1);
        removed = true;
    }

    auto it2 = async_loggers_.find(name_str);
    if (it2 != async_loggers_.end()) {
        async_loggers_.erase(it2);
        removed = true;
    }

    auto it3 = mmap_loggers_.find(name_str);
    if (it3 != mmap_loggers_.end()) {
        mmap_loggers_.erase(it3);
        removed = true;
    }

    return removed;
}

// Set global log level
void LogManager::setGlobalLevel(LogLevel level) {
    std::shared_lock lock(loggers_mutex_);

    // Set default logger level
    if (default_logger_) {
        default_logger_->setLevel(level);
    }

    // Set all standard loggers
    for (auto& [name, logger] : loggers_) {
        logger->setLevel(level);
    }

    // Set all async loggers
    for (auto& [name, logger] : async_loggers_) {
        logger->setLevel(level);
    }

    // Set all memory-mapped loggers
    for (auto& [name, logger] : mmap_loggers_) {
        logger->setLevel(level);
    }
}

// Enable or disable system logging
void LogManager::enableSystemLogging(bool enable) {
    std::shared_lock lock(loggers_mutex_);

    // Set default logger
    if (default_logger_) {
        default_logger_->enableSystemLogging(enable);
    }

    // Set all standard loggers
    for (auto& [name, logger] : loggers_) {
        logger->enableSystemLogging(enable);
    }

    // Set all async loggers
    for (auto& [name, logger] : async_loggers_) {
        logger->enableSystemLogging(enable);
    }

    // Set all memory-mapped loggers
    for (auto& [name, logger] : mmap_loggers_) {
        logger->enableSystemLogging(enable);
    }
}

// Flush all loggers
void LogManager::flushAll() {
    // Create local copies to avoid holding the lock during potentially slow I/O
    // operations
    std::vector<std::shared_ptr<Logger>> loggers_copy;
    std::vector<std::shared_ptr<MmapLogger>> mmap_loggers_copy;
    std::vector<std::shared_ptr<AsyncLogger>> async_loggers_copy;

    {
        std::shared_lock lock(loggers_mutex_);

        // Reserve space to avoid reallocations
        loggers_copy.reserve(loggers_.size() + (default_logger_ ? 1 : 0));
        mmap_loggers_copy.reserve(mmap_loggers_.size());
        async_loggers_copy.reserve(async_loggers_.size());

        // Copy default logger if exists
        if (default_logger_) {
            loggers_copy.push_back(default_logger_);
        }

        // Copy standard loggers
        for (const auto& [name, logger] : loggers_) {
            // Skip if it's the default logger (already added)
            if (logger != default_logger_) {
                loggers_copy.push_back(logger);
            }
        }

        // Copy memory-mapped loggers
        for (const auto& [name, logger] : mmap_loggers_) {
            mmap_loggers_copy.push_back(logger);
        }

        // Copy async loggers
        for (const auto& [name, logger] : async_loggers_) {
            async_loggers_copy.push_back(logger);
        }
    }

    // Vector to store futures for async tasks
    std::vector<std::future<void>> async_futures;
    async_futures.reserve(async_loggers_copy.size());

    // Start async flush tasks for async loggers
    for (auto& logger : async_loggers_copy) {
        async_futures.push_back(std::async(std::launch::async, [logger]() {
            logger->flush().await_resume();
        }));
    }

    // Flush standard loggers in this thread
    for (auto& logger : loggers_copy) {
        logger->flush();
    }

    // Flush memory-mapped loggers in this thread
    for (auto& logger : mmap_loggers_copy) {
        logger->flush();
    }

    // Wait for all async loggers to complete flushing
    for (auto& future : async_futures) {
        future.wait();
    }
}

}  // namespace atom::log