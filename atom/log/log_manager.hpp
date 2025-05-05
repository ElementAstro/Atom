// log_manager.hpp
/*
 * log_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-4

Description: Log Manager for centralized logging configuration and access

**************************************************/

#ifndef ATOM_LOG_LOG_MANAGER_HPP
#define ATOM_LOG_LOG_MANAGER_HPP

#include "async_logger.hpp"
#include "atomlog.hpp"
#include "mmap_logger.hpp"


#include <filesystem>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace fs = std::filesystem;

namespace atom::log {

/**
 * @brief Log Manager class providing singleton access to the logging system.
 *
 * This class manages different logger instances and provides configuration and
 * access functionality. Uses C++20 standards to ensure thread-safe singleton
 * implementation.
 */
class LogManager {
public:
    /**
     * @brief Get the singleton instance of LogManager.
     * @return LogManager& Reference to the LogManager singleton.
     */
    static LogManager& getInstance() {
        static LogManager instance;
        return instance;
    }

    /**
     * @brief Configure the default logger.
     * @param file_name Log file path.
     * @param min_level Minimum log level.
     * @param max_file_size Maximum log file size (bytes).
     * @param max_files Maximum number of log files to retain.
     */
    void configureDefaultLogger(const fs::path& file_name,
                                LogLevel min_level = LogLevel::TRACE,
                                size_t max_file_size = 1048576,
                                int max_files = 10);

    /**
     * @brief Create a standard synchronous logger.
     * @param name Logger name.
     * @param file_name Log file path.
     * @param min_level Minimum log level.
     * @param max_file_size Maximum log file size (bytes).
     * @param max_files Maximum number of log files to retain.
     * @return std::shared_ptr<Logger> Logger instance.
     */
    [[nodiscard]]
    std::shared_ptr<Logger> createLogger(std::string_view name,
                                         const fs::path& file_name,
                                         LogLevel min_level = LogLevel::TRACE,
                                         size_t max_file_size = 1048576,
                                         int max_files = 10);

    /**
     * @brief Create an asynchronous logger.
     * @param name Logger name.
     * @param file_name Log file path.
     * @param min_level Minimum log level.
     * @param max_file_size Maximum log file size (bytes).
     * @param max_files Maximum number of log files to retain.
     * @param thread_count Number of worker threads.
     * @return std::shared_ptr<AsyncLogger> Asynchronous logger instance.
     */
    [[nodiscard]]
    std::shared_ptr<AsyncLogger> createAsyncLogger(
        std::string_view name, const fs::path& file_name,
        LogLevel min_level = LogLevel::TRACE, size_t max_file_size = 1048576,
        int max_files = 10, size_t thread_count = 1);

    /**
     * @brief Create a memory-mapped logger.
     * @param name Logger name.
     * @param file_name Log file path.
     * @param min_level Minimum log level.
     * @param buffer_size Memory-mapped buffer size (bytes).
     * @param max_files Maximum number of log files to retain.
     * @return std::shared_ptr<MmapLogger> Memory-mapped logger instance.
     */
    [[nodiscard]]
    std::shared_ptr<MmapLogger> createMmapLogger(
        std::string_view name, const fs::path& file_name,
        LogLevel min_level = LogLevel::TRACE, size_t buffer_size = 1048576,
        int max_files = 10);

    /**
     * @brief Get the default logger.
     * @return std::shared_ptr<Logger> Default logger.
     */
    [[nodiscard]]
    std::shared_ptr<Logger> getDefaultLogger();

    /**
     * @brief Get a named logger.
     * @param name Logger name.
     * @return std::optional<std::shared_ptr<Logger>> Logger instance (nullopt
     * if not exists).
     */
    [[nodiscard]]
    std::optional<std::shared_ptr<Logger>> getLogger(std::string_view name);

    /**
     * @brief Get a named asynchronous logger.
     * @param name Logger name.
     * @return std::optional<std::shared_ptr<AsyncLogger>> Asynchronous logger
     * instance (nullopt if not exists).
     */
    [[nodiscard]]
    std::optional<std::shared_ptr<AsyncLogger>> getAsyncLogger(
        std::string_view name);

    /**
     * @brief Get a named memory-mapped logger.
     * @param name Logger name.
     * @return std::optional<std::shared_ptr<MmapLogger>> Memory-mapped logger
     * instance (nullopt if not exists).
     */
    [[nodiscard]]
    std::optional<std::shared_ptr<MmapLogger>> getMmapLogger(
        std::string_view name);

    /**
     * @brief Delete a named logger.
     * @param name Logger name.
     * @return bool Whether deletion was successful.
     */
    [[nodiscard]]
    bool removeLogger(std::string_view name);

    /**
     * @brief Set log level for all loggers.
     * @param level Log level.
     */
    void setGlobalLevel(LogLevel level);

    /**
     * @brief Enable or disable system logging.
     * @param enable Whether to enable system logging.
     */
    void enableSystemLogging(bool enable);

    /**
     * @brief Flush all loggers.
     * This method will wait for async loggers to finish processing all messages
     * in their queues.
     */
    void flushAll();

private:
    // Private constructors and destructors to prevent external instantiation
    LogManager() = default;
    ~LogManager() = default;

    // Disable copy and move
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    LogManager(LogManager&&) = delete;
    LogManager& operator=(LogManager&&) = delete;

    // Internal data and mutex
    mutable std::shared_mutex loggers_mutex_;
    std::unordered_map<std::string, std::shared_ptr<Logger>> loggers_;
    std::unordered_map<std::string, std::shared_ptr<AsyncLogger>>
        async_loggers_;
    std::unordered_map<std::string, std::shared_ptr<MmapLogger>> mmap_loggers_;
    std::shared_ptr<Logger> default_logger_;
};

// Define simplified global access functions

/**
 * @brief Get the default logger
 * @return std::shared_ptr<Logger> Default logger instance
 */
[[nodiscard]]
inline std::shared_ptr<Logger> getDefaultLogger() {
    return LogManager::getInstance().getDefaultLogger();
}

/**
 * @brief Get a logger with the specified name
 * @param name Logger name
 * @return std::optional<std::shared_ptr<Logger>> Logger instance
 */
[[nodiscard]]
inline std::optional<std::shared_ptr<Logger>> getLogger(std::string_view name) {
    return LogManager::getInstance().getLogger(name);
}

/**
 * @brief Get an asynchronous logger with the specified name
 * @param name Logger name
 * @return std::optional<std::shared_ptr<AsyncLogger>> Asynchronous logger
 * instance
 */
[[nodiscard]]
inline std::optional<std::shared_ptr<AsyncLogger>> getAsyncLogger(
    std::string_view name) {
    return LogManager::getInstance().getAsyncLogger(name);
}

/**
 * @brief Get a memory-mapped logger with the specified name
 * @param name Logger name
 * @return std::optional<std::shared_ptr<MmapLogger>> Memory-mapped logger
 * instance
 */
[[nodiscard]]
inline std::optional<std::shared_ptr<MmapLogger>> getMmapLogger(
    std::string_view name) {
    return LogManager::getInstance().getMmapLogger(name);
}

/**
 * @brief Create a global default logger macro for easy application use
 */
#define LOG_DEFAULT getDefaultLogger()

/**
 * @brief Create logging shortcut macros
 */
#define LOG_TRACE(fmt, ...) LOG_DEFAULT->trace(fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG_DEFAULT->debug(fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG_DEFAULT->info(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) LOG_DEFAULT->warn(fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG_DEFAULT->error(fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) LOG_DEFAULT->critical(fmt, ##__VA_ARGS__)

}  // namespace atom::log

#endif  // ATOM_LOG_LOG_MANAGER_HPP