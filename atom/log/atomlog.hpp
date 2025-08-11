// atomlog.hpp
/*
 * atomlog.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Enhanced Logger for Atom with C++20 Features

**************************************************/

#ifndef ATOM_LOG_ATOMLOG_HPP
#define ATOM_LOG_ATOMLOG_HPP

#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <source_location>
#include <string_view>

#include "atom/containers/high_performance.hpp"

namespace fs = std::filesystem;

namespace atom::log {

using atom::containers::String;

/**
 * @brief Enum class representing the log levels.
 * Extended to support custom log levels.
 */
enum class LogLevel : int {
    TRACE = 0,  ///< Trace level logging.
    DEBUG,      ///< Debug level logging.
    INFO,       ///< Info level logging.
    WARN,       ///< Warn level logging.
    ERROR,      ///< Error level logging.
    CRITICAL,   ///< Critical level logging.
    OFF         ///< Used to disable logging.
};

/**
 * @brief Structure representing a custom log level.
 */
struct CustomLogLevel {
    String name;  // Use String
    int severity;
};

/**
 * @brief Enum class representing the log formats.
 */
enum class LogFormat {
    SIMPLE,  ///< Simple log format.
    JSON,    ///< JSON log format.
    XML,     ///< XML log format.
    CUSTOM   ///< Custom log format.
};

/**
 * @brief Type alias for the log filter function.
 */
using LogFilter = std::function<bool(LogLevel, std::string_view)>;

/**
 * @brief Type alias for the log formatter function.
 */
using LogFormatter = std::function<std::string(
    LogLevel, std::string_view, const std::source_location&,
    std::string_view timestamp, std::string_view thread_name)>;

/**
 * @brief Logger class for logging messages with different severity levels.
 */
class Logger {
public:
    /**
     * @brief Constructs a Logger object.
     * @param file_name The name of the log file.
     * @param min_level The minimum log level to log.
     * @param max_file_size The maximum size of the log file in bytes.
     * @param max_files The maximum number of log files to keep.
     */
    explicit Logger(const fs::path& file_name,
                    LogLevel min_level = LogLevel::TRACE,
                    size_t max_file_size = 1048576, int max_files = 10);

    /**
     * @brief Destructor for the Logger object.
     */
    ~Logger();

    // Delete copy constructor and copy assignment operator
    Logger(const Logger&) = delete;
    auto operator=(const Logger&) -> Logger& = delete;
    Logger(Logger&&) noexcept = default;
    Logger& operator=(Logger&&) noexcept = default;

    /**
     * @brief Logs a trace level message with source location information.
     * @tparam Args The types of the arguments.
     * @param format The format string.
     * @param args The arguments to format.
     * @param location The source location information (auto-captured).
     */
    template <typename... Args>
    void trace(std::string_view format, Args&&... args,
               const std::source_location& location =
                   std::source_location::current()) {
        if (shouldLog(LogLevel::TRACE)) {
            log(LogLevel::TRACE,
                std::vformat(format, std::make_format_args(args...)), location);
        }
    }

    /**
     * @brief Logs a debug level message with source location information.
     * @tparam Args The types of the arguments.
     * @param format The format string.
     * @param args The arguments to format.
     * @param location The source location information (auto-captured).
     */
    template <typename... Args>
    void debug(std::string_view format, Args&&... args,
               const std::source_location& location =
                   std::source_location::current()) {
        if (shouldLog(LogLevel::DEBUG)) {
            log(LogLevel::DEBUG,
                std::vformat(format, std::make_format_args(args...)), location);
        }
    }

    /**
     * @brief Logs an info level message with source location information.
     * @tparam Args The types of the arguments.
     * @param format The format string.
     * @param args The arguments to format.
     * @param location The source location information (auto-captured).
     */
    template <typename... Args>
    void info(std::string_view format, Args&&... args,
              const std::source_location& location =
                  std::source_location::current()) {
        if (shouldLog(LogLevel::INFO)) {
            log(LogLevel::INFO,
                std::vformat(format, std::make_format_args(args...)), location);
        }
    }

    /**
     * @brief Logs a warn level message with source location information.
     * @tparam Args The types of the arguments.
     * @param format The format string.
     * @param args The arguments to format.
     * @param location The source location information (auto-captured).
     */
    template <typename... Args>
    void warn(std::string_view format, Args&&... args,
              const std::source_location& location =
                  std::source_location::current()) {
        if (shouldLog(LogLevel::WARN)) {
            log(LogLevel::WARN,
                std::vformat(format, std::make_format_args(args...)), location);
        }
    }

    /**
     * @brief Logs an error level message with source location information.
     * @tparam Args The types of the arguments.
     * @param format The format string.
     * @param args The arguments to format.
     * @param location The source location information (auto-captured).
     */
    template <typename... Args>
    void error(std::string_view format, Args&&... args,
               const std::source_location& location =
                   std::source_location::current()) {
        if (shouldLog(LogLevel::ERROR)) {
            log(LogLevel::ERROR,
                std::vformat(format, std::make_format_args(args...)), location);
        }
    }

    /**
     * @brief Logs a critical level message with source location information.
     * @tparam Args The types of the arguments.
     * @param format The format string.
     * @param args The arguments to format.
     * @param location The source location information (auto-captured).
     */
    template <typename... Args>
    void critical(std::string_view format, Args&&... args,
                  const std::source_location& location =
                      std::source_location::current()) {
        if (shouldLog(LogLevel::CRITICAL)) {
            log(LogLevel::CRITICAL,
                std::vformat(format, std::make_format_args(args...)), location);
        }
    }

    /**
     * @brief Sets the logging level.
     * @param level The log level to set.
     */
    void setLevel(LogLevel level);

    /**
     * @brief Sets the logging pattern.
     * @param pattern The pattern to set.
     */
    void setPattern(std::string_view pattern);  // Use String

    /**
     * @brief Sets the thread name for logging.
     * @param name The thread name to set.
     */
    void setThreadName(std::string_view name);  // Use String

    /**
     * @brief Sets the logging format.
     * @param format The log format to set.
     */
    void setFormat(LogFormat format);

    /**
     * @brief Sets a custom formatter for logging.
     * @param formatter The custom formatter to set.
     */
    void setCustomFormatter(LogFormatter formatter);

    /**
     * @brief Adds a filter for logging.
     * @param filter The filter to add.
     */
    void addFilter(LogFilter filter);

    /**
     * @brief Clears all filters for logging.
     */
    void clearFilters();

    /**
     * @brief Sets the batch size for asynchronous logging.
     * @param size The batch size to set.
     */
    void setBatchSize(size_t size);

    /**
     * @brief Sets the flush interval for asynchronous logging.
     * @param interval The flush interval to set.
     */
    void setFlushInterval(std::chrono::milliseconds interval);

    /**
     * @brief Enables or disables compression for logging.
     * @param enabled True to enable compression, false to disable.
     */
    void setCompressionEnabled(bool enabled);

    /**
     * @brief Sets the encryption key for logging.
     * @param key The encryption key to set.
     */
    void setEncryptionKey(std::string_view key);

    /**
     * @brief Registers a sink logger.
     * @param logger The logger to register as a sink.
     */
    void registerSink(std::shared_ptr<Logger> logger);

    /**
     * @brief Removes a sink logger.
     * @param logger The logger to remove as a sink.
     */
    void removeSink(std::shared_ptr<Logger> logger);

    /**
     * @brief Clears all registered sink loggers.
     */
    void clearSinks();

    /**
     * @brief Flushes any buffered log messages to disk.
     */
    void flush();

    /**
     * @brief Forces flushing of log messages to disk.
     */
    void forceFlush();

    /**
     * @brief Enables or disables system logging.
     * @param enable True to enable system logging, false to disable.
     */
    void enableSystemLogging(bool enable);

    /**
     * @brief Enables or disables asynchronous logging.
     * @param enable True to enable asynchronous logging, false to disable.
     */
    void enableAsyncLogging(bool enable);

    /**
     * @brief Enables or disables color output for logging.
     * @param enable True to enable color output, false to disable.
     */
    void enableColorOutput(bool enable);

    /**
     * @brief Enables or disables memory logging.
     * @param enable True to enable memory logging, false to disable.
     * @param max_entries The maximum number of entries to keep in memory.
     */
    void enableMemoryLogging(bool enable, size_t max_entries = 1000);

    /**
     * @brief Registers a custom log level.
     * @param name The name of the custom log level.
     * @param severity The severity of the custom log level.
     */
    void registerCustomLogLevel(std::string_view name, int severity);

    /**
     * @brief Logs a message with a custom log level.
     * @param level_name The name of the custom log level.
     * @param msg The message to log.
     * @param location The source location information (auto-captured).
     */
    void logCustomLevel(
        std::string_view level_name, std::string_view msg,
        const std::source_location& location = std::source_location::current());

    /**
     * @brief Checks if a specific log level should be logged.
     * @param level The log level to check.
     * @return True if the log level should be logged, false otherwise.
     */
    [[nodiscard]] bool shouldLog(LogLevel level) const noexcept;

    /**
     * @brief Gets the current log level.
     * @return The current log level.
     */
    [[nodiscard]] LogLevel getLevel() const noexcept;

    /**
     * @brief Gets the current queue size for asynchronous logging.
     * @return The current queue size.
     */
    [[nodiscard]] size_t getQueueSize() const noexcept;

    /**
     * @brief Checks if the logger is enabled.
     * @return True if the logger is enabled, false otherwise.
     */
    [[nodiscard]] bool isEnabled() const noexcept;

    /**
     * @brief Gets the in-memory logs.
     * @return A vector of in-memory log entries.
     */
    [[nodiscard]] std::vector<std::string> getMemoryLogs() const;

    /**
     * @brief Gets the logger statistics.
     * @return A string containing the logger statistics.
     */
    [[nodiscard]] std::string getStats() const;

    /**
     * @brief Rotates the log files.
     */
    void rotate();

    /**
     * @brief Closes the logger.
     */
    void close();

    /**
     * @brief Reopens the logger.
     */
    void reopen();

    /**
     * @brief Gets the default logger instance.
     * @return A shared pointer to the default logger.
     */
    static std::shared_ptr<Logger> getDefault();

    /**
     * @brief Sets the default logger instance.
     * @param logger A shared pointer to the logger to set as default.
     */
    static void setDefault(std::shared_ptr<Logger> logger);

    /**
     * @brief Creates a new logger instance.
     * @param file_name The name of the log file.
     * @return A shared pointer to the newly created logger.
     */
    static std::shared_ptr<Logger> create(const fs::path& file_name);

private:
    class LoggerImpl;  // Forward declaration
    std::shared_ptr<LoggerImpl>
        impl_;  ///< Pointer to the Logger implementation.

    /**
     * @brief Logs a message with a specified log level.
     * @param level The log level.
     * @param msg The message to log (std::format produces std::string).
     * @param location The source location information.
     */
    // Keep std::string here as std::format returns std::string
    void log(
        LogLevel level, std::string msg,
        const std::source_location& location = std::source_location::current());
};

/**
 * @brief Converts a log level to a string representation.
 * @param level The log level to convert.
 * @return The string representation of the log level.
 */
[[nodiscard]] constexpr std::string_view logLevelToString(
    LogLevel level) noexcept;

/**
 * @brief Converts a string representation to a log level.
 * @param str The string to convert.
 * @return The corresponding log level.
 */
[[nodiscard]] LogLevel stringToLogLevel(std::string_view str);

}  // namespace atom::log

#endif  // ATOM_LOG_ATOMLOG_HPP
