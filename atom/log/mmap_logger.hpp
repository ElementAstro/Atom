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

#include <filesystem>
#include <memory>
#include <source_location>

namespace fs = std::filesystem;

namespace atom::log {

/**
 * @brief Memory-mapped Logger class for high-performance logging.
 *
 * This class extends the standard Logger with memory-mapped file support
 * for improved I/O performance, especially for high-frequency logging.
 */
class MmapLogger {
public:
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
    void trace(const String& format, Args&&... args,
               const std::source_location& location =
                   std::source_location::current()) {
        log(LogLevel::TRACE,
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
    void debug(const String& format, Args&&... args,
               const std::source_location& location =
                   std::source_location::current()) {
        log(LogLevel::DEBUG,
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
    void info(const String& format, Args&&... args,
              const std::source_location& location =
                  std::source_location::current()) {
        log(LogLevel::INFO,
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
    void warn(const String& format, Args&&... args,
              const std::source_location& location =
                  std::source_location::current()) {
        log(LogLevel::WARN,
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
    void error(const String& format, Args&&... args,
               const std::source_location& location =
                   std::source_location::current()) {
        log(LogLevel::ERROR,
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
    void critical(const String& format, Args&&... args,
                  const std::source_location& location =
                      std::source_location::current()) {
        log(LogLevel::CRITICAL,
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
     */
    void flush();

private:
    class MmapLoggerImpl;  // Forward declaration
    std::unique_ptr<MmapLoggerImpl> impl_;

    /**
     * @brief Logs a message with a specified log level.
     * @param level The log level.
     * @param msg The message to log.
     * @param location The source location information.
     */
    void log(
        LogLevel level, std::string_view msg,
        const std::source_location& location = std::source_location::current());
};

}  // namespace atom::log

#endif  // ATOM_LOG_MMAP_LOGGER_HPP