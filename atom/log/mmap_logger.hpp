// mmap_logger.hpp
/*
 * mmap_logger.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-4

Description: Memory-mapped File Logger for Atom with C++20 Features

**************************************************/

#ifndef ATOM_LOG_MMAP_LOGGER_HPP
#define ATOM_LOG_MMAP_LOGGER_HPP

#include "atomlog.hpp"

#include <concepts>
#include <expected>
#include <filesystem>
#include <memory>
#include <source_location>
#include <span>
#include <string_view>

namespace fs = std::filesystem;

namespace atom::log {

// Concept for valid log message arguments
template <typename T>
concept Loggable = requires(T t) {
    { std::format("{}", t) } -> std::convertible_to<std::string>;
};

/**
 * @brief Custom exception class hierarchy for MmapLogger
 */
class LoggerException : public std::exception {
public:
    explicit LoggerException(std::string_view message) : message_(message) {}
    [[nodiscard]] const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    std::string message_;
};

class FileException : public LoggerException {
    using LoggerException::LoggerException;
};

class MappingException : public LoggerException {
    using LoggerException::LoggerException;
};

class ConfigException : public LoggerException {
    using LoggerException::LoggerException;
};

// Error code enumeration for use with std::expected
enum class LoggerErrorCode {
    Success,
    FileOpenError,
    MappingError,
    UnmapError,
    ConfigError,
    RotationError,
    SystemLogError
};

/**
 * @brief Memory-mapped Logger class for high-performance logging.
 *
 * This class extends the standard Logger with memory-mapped file support
 * for improved I/O performance, especially for high-frequency logging.
 */
class MmapLogger {
public:
    // Log category for filtering based on category
    enum class Category {
        General,
        Network,
        Database,
        Security,
        Performance,
        UI,
        API,
        Custom
    };

    // Configuration struct for logger initialization
    struct Config {
        fs::path file_name;
        LogLevel min_level = LogLevel::TRACE;
        size_t buffer_size = 1048576;
        int max_files = 10;
        bool use_system_logging = false;
        bool auto_flush = false;
        std::string thread_name_prefix = "Thread-";

        // Builder pattern for fluent configuration
        [[nodiscard]] constexpr Config& withLevel(LogLevel level) noexcept {
            min_level = level;
            return *this;
        }

        [[nodiscard]] constexpr Config& withBufferSize(size_t size) noexcept {
            buffer_size = size;
            return *this;
        }

        [[nodiscard]] constexpr Config& withMaxFiles(int max) noexcept {
            max_files = max;
            return *this;
        }

        [[nodiscard]] constexpr Config& withSystemLogging(
            bool enable) noexcept {
            use_system_logging = enable;
            return *this;
        }

        [[nodiscard]] constexpr Config& withAutoFlush(bool enable) noexcept {
            auto_flush = enable;
            return *this;
        }

        [[nodiscard]] Config& withThreadNamePrefix(std::string_view prefix) {
            thread_name_prefix = prefix;
            return *this;
        }
    };

    /**
     * @brief Constructs a MmapLogger object with configuration.
     * @param config The configuration for the logger.
     */
    explicit MmapLogger(const Config& config);

    /**
     * @brief Constructs a MmapLogger object.
     * @param file_name The name of the log file.
     * @param min_level The minimum log level to log.
     * @param buffer_size The size of the memory-mapped buffer in bytes.
     * @param max_files The maximum number of log files to keep.
     */
    explicit MmapLogger(const fs::path& file_name,
                        LogLevel min_level = LogLevel::TRACE,
                        size_t buffer_size = 1048576, int max_files = 10);

    /**
     * @brief Destructor for the MmapLogger object.
     */
    ~MmapLogger();

    // Delete copy constructor and copy assignment operator
    MmapLogger(const MmapLogger&) = delete;
    auto operator=(const MmapLogger&) -> MmapLogger& = delete;

    // Move constructor and move assignment operator
    MmapLogger(MmapLogger&&) noexcept;
    auto operator=(MmapLogger&&) noexcept -> MmapLogger&;

    /**
     * @brief Logs a trace level message with source location information.
     * @tparam Args The types of the arguments.
     * @param format The format string.
     * @param args The arguments to format.
     * @param location The source location information (auto-captured).
     */
    template <typename... Args>
        requires(Loggable<Args> && ...)
    void trace(const String& format, Args&&... args,
               const std::source_location& location =
                   std::source_location::current()) {
        log(LogLevel::TRACE, Category::General,
            std::format(format.c_str(), std::forward<Args>(args)...), location);
    }

    /**
     * @brief Logs a debug level message with source location information.
     * @tparam Args The types of the arguments.
     * @param format The format string.
     * @param args The arguments to format.
     * @param location The source location information (auto-captured).
     */
    template <typename... Args>
        requires(Loggable<Args> && ...)
    void debug(const String& format, Args&&... args,
               const std::source_location& location =
                   std::source_location::current()) {
        log(LogLevel::DEBUG, Category::General,
            std::format(format.c_str(), std::forward<Args>(args)...), location);
    }

    /**
     * @brief Logs an info level message with source location information.
     * @tparam Args The types of the arguments.
     * @param format The format string.
     * @param args The arguments to format.
     * @param location The source location information (auto-captured).
     */
    template <typename... Args>
        requires(Loggable<Args> && ...)
    void info(const String& format, Args&&... args,
              const std::source_location& location =
                  std::source_location::current()) {
        log(LogLevel::INFO, Category::General,
            std::format(format.c_str(), std::forward<Args>(args)...), location);
    }

    /**
     * @brief Logs a warn level message with source location information.
     * @tparam Args The types of the arguments.
     * @param format The format string.
     * @param args The arguments to format.
     * @param location The source location information (auto-captured).
     */
    template <typename... Args>
        requires(Loggable<Args> && ...)
    void warn(const String& format, Args&&... args,
              const std::source_location& location =
                  std::source_location::current()) {
        log(LogLevel::WARN, Category::General,
            std::format(format.c_str(), std::forward<Args>(args)...), location);
    }

    /**
     * @brief Logs an error level message with source location information.
     * @tparam Args The types of the arguments.
     * @param format The format string.
     * @param args The arguments to format.
     * @param location The source location information (auto-captured).
     */
    template <typename... Args>
        requires(Loggable<Args> && ...)
    void error(const String& format, Args&&... args,
               const std::source_location& location =
                   std::source_location::current()) {
        log(LogLevel::ERROR, Category::General,
            std::format(format.c_str(), std::forward<Args>(args)...), location);
    }

    /**
     * @brief Logs a critical level message with source location information.
     * @tparam Args The types of the arguments.
     * @param format The format string.
     * @param args The arguments to format.
     * @param location The source location information (auto-captured).
     */
    template <typename... Args>
        requires(Loggable<Args> && ...)
    void critical(const String& format, Args&&... args,
                  const std::source_location& location =
                      std::source_location::current()) {
        log(LogLevel::CRITICAL, Category::General,
            std::format(format.c_str(), std::forward<Args>(args)...), location);
    }

    /**
     * @brief Logs a message with specified category and level
     * @tparam Args The types of the arguments
     * @param level The log level
     * @param category The log category
     * @param format The format string
     * @param args The arguments to format
     * @param location The source location information
     */
    template <typename... Args>
        requires(Loggable<Args> && ...)
    void logWithCategory(LogLevel level, Category category,
                         const String& format, Args&&... args,
                         const std::source_location& location =
                             std::source_location::current()) {
        log(level, category,
            std::format(format.c_str(), std::forward<Args>(args)...), location);
    }

    /**
     * @brief Sets the logging level.
     * @param level The log level to set.
     */
    void setLevel(LogLevel level);

    /**
     * @brief Sets the thread name for logging.
     * @param name The thread name to set.
     */
    void setThreadName(const String& name);

    /**
     * @brief Enables or disables system logging.
     * @param enable True to enable system logging, false to disable.
     */
    void enableSystemLogging(bool enable);

    /**
     * @brief Forces log buffer flush to disk.
     * @return std::expected with void or error code
     */
    [[nodiscard]] std::expected<void, LoggerErrorCode> flush() noexcept;

    /**
     * @brief Sets category filter to only log specific categories
     * @param categories Span of categories to include in logging
     */
    void setCategoryFilter(std::span<const Category> categories);

    /**
     * @brief Adds custom pattern to filter log messages
     * @param pattern Regex pattern to filter messages
     */
    void addFilterPattern(std::string_view pattern);

    /**
     * @brief Sets auto-flush interval in milliseconds
     * @param milliseconds Interval between auto-flushes (0 to disable)
     */
    void setAutoFlushInterval(uint32_t milliseconds);

    /**
     * @brief Returns statistics about logged messages
     * @return JSON string with statistics
     */
    [[nodiscard]] std::string getStatistics() const;

    /**
     * @brief Configure compression for rotated log files
     * @param enable Whether to enable compression
     */
    void enableCompression(bool enable);

private:
    class MmapLoggerImpl;  // Forward declaration
    std::unique_ptr<MmapLoggerImpl> impl_;

    /**
     * @brief Logs a message with a specified log level.
     * @param level The log level.
     * @param category The log category.
     * @param msg The message to log.
     * @param location The source location information.
     */
    void log(
        LogLevel level, Category category, std::string_view msg,
        const std::source_location& location = std::source_location::current());
};

// Type alias for logger config
using LoggerConfig = MmapLogger::Config;

}  // namespace atom::log

#endif  // ATOM_LOG_MMAP_LOGGER_HPP