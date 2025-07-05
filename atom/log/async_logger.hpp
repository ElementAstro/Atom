// async_logger.hpp
/*
 * async_logger.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-6

Description: Enhanced Asynchronous Logger using C++20/23 Coroutines

**************************************************/

#ifndef ATOM_LOG_ASYNC_LOGGER_HPP
#define ATOM_LOG_ASYNC_LOGGER_HPP

#include "atomlog.hpp"

#include <concepts>
#include <coroutine>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace fs = std::filesystem;

namespace atom::log {

/**
 * @brief Logging error codes for std::expected return types
 */
enum class LogErrorCode {
    Success,
    QueueFull,
    ShuttingDown,
    InvalidLogger,
    TaskTimeout,
    InternalError
};

/**
 * @brief Base exception class for AsyncLogger errors
 */
class AsyncLoggerException : public std::exception {
public:
    explicit AsyncLoggerException(std::string_view message)
        : message_(message) {}
    [[nodiscard]] const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    std::string message_;
};

/**
 * @brief Exception thrown when queue limit is exceeded
 */
class QueueFullException : public AsyncLoggerException {
    using AsyncLoggerException::AsyncLoggerException;
};

/**
 * @brief Exception thrown when logger is already shutting down
 */
class ShutdownException : public AsyncLoggerException {
    using AsyncLoggerException::AsyncLoggerException;
};

// Forward declaration for the specialization
struct TaskPromiseTypeVoid;

// Simple coroutine task type for asynchronous logging operations
template <typename T = void>
class Task {
public:
    // Promise type that satisfies C++20 coroutine promise concept
    struct promise_type {
        std::expected<T, LogErrorCode> result{T{}, LogErrorCode::Success};

        Task get_return_object() {
            return Task(
                std::coroutine_handle<promise_type>::from_promise(*this));
        }

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        void return_value(T value) { result = std::move(value); }

        void return_value(std::expected<T, LogErrorCode> value) {
            result = std::move(value);
        }

        void unhandled_exception() {
            try {
                std::rethrow_exception(std::current_exception());
            } catch (const QueueFullException&) {
                result = std::unexpected(LogErrorCode::QueueFull);
            } catch (const ShutdownException&) {
                result = std::unexpected(LogErrorCode::ShuttingDown);
            } catch (...) {
                result = std::unexpected(LogErrorCode::InternalError);
            }
        }
    };

    // Constructor and destructor
    explicit Task(std::coroutine_handle<promise_type> handle)
        : handle_(handle) {}
    ~Task() {
        if (handle_)
            handle_.destroy();
    }

    // Move operations
    Task(Task&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_)
                handle_.destroy();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    // Delete copy operations
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // Coroutine synchronization wait
    bool await_ready() const noexcept { return false; }
    void await_suspend(
        [[maybe_unused]] std::coroutine_handle<> awaiting) const noexcept {
        // Suspend the awaiting coroutine and resume this one
        handle_.resume();
    }

    std::expected<T, LogErrorCode> await_resume() const noexcept {
        return handle_.promise().result;
    }

private:
    std::coroutine_handle<promise_type> handle_ = nullptr;
};

// Specialization for promise_type<void>
template <>
struct Task<void>::promise_type {
    std::expected<void, LogErrorCode> result{};

    Task<void> get_return_object() {
        return Task<void>(
            std::coroutine_handle<promise_type>::from_promise(*this));
    }

    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }

    void return_void() { result = std::expected<void, LogErrorCode>{}; }

    void unhandled_exception() {
        try {
            std::rethrow_exception(std::current_exception());
        } catch (const QueueFullException&) {
            result = std::unexpected(LogErrorCode::QueueFull);
        } catch (const ShutdownException&) {
            result = std::unexpected(LogErrorCode::ShuttingDown);
        } catch (...) {
            result = std::unexpected(LogErrorCode::InternalError);
        }
    }
};

// Specialization for Task<void>::await_resume
template <>
inline std::expected<void, LogErrorCode> Task<void>::await_resume()
    const noexcept {
    return handle_.promise().result;
}

// Define a concept for types that can be logged
template <typename T>
concept Loggable = requires(T t) {
    { std::format("{}", t) } -> std::convertible_to<std::string>;
};

/**
 * @brief Configuration for AsyncLogger
 */
struct AsyncLoggerConfig {
    fs::path file_name;
    LogLevel min_level = LogLevel::TRACE;
    size_t max_file_size = 1048576;
    int max_files = 10;
    size_t thread_pool_size = 1;
    size_t queue_capacity = 10000;
    bool use_system_logging = false;
    std::chrono::milliseconds flush_interval{1000};  // Auto-flush interval
};

/**
 * @brief Asynchronous logger class implementing non-blocking logging with C++20
 * coroutines.
 *
 * This class provides an interface similar to the standard Logger, but all
 * logging methods return coroutine tasks. Logging operations are executed in a
 * background thread pool without blocking the calling thread.
 */
class AsyncLogger {
public:
    /**
     * @brief Constructs an AsyncLogger object with configuration struct.
     * @param config Logger configuration options
     */
    explicit AsyncLogger(const AsyncLoggerConfig& config);

    /**
     * @brief Constructs an AsyncLogger object.
     * @param file_name Log file name.
     * @param min_level Minimum log level to record.
     * @param max_file_size Maximum log file size in bytes.
     * @param max_files Maximum number of log files to retain.
     * @param thread_pool_size Size of the thread pool for asynchronous
     * processing.
     */
    explicit AsyncLogger(const fs::path& file_name,
                         LogLevel min_level = LogLevel::TRACE,
                         size_t max_file_size = 1048576, int max_files = 10,
                         size_t thread_pool_size = 1);

    /**
     * @brief Destructs the asynchronous logger object.
     */
    ~AsyncLogger();

    // Disable copy operations
    AsyncLogger(const AsyncLogger&) = delete;
    auto operator=(const AsyncLogger&) -> AsyncLogger& = delete;

    // Enable move operations
    AsyncLogger(AsyncLogger&&) noexcept;
    auto operator=(AsyncLogger&&) noexcept -> AsyncLogger&;

    /**
     * @brief Asynchronously logs a TRACE level message.
     * @tparam Args Format parameter types.
     * @param format Format string.
     * @param args Format arguments.
     * @param location Source code location information (automatically
     * captured).
     * @return Task<void> Coroutine task that can be awaited or ignored.
     */
    template <Loggable... Args>
    [[nodiscard]] Task<void> trace(const String& format, Args&&... args,
                                   const std::source_location& location =
                                       std::source_location::current()) {
        if constexpr (sizeof...(args) > 0) {
            auto msg = std::format(format.c_str(), std::forward<Args>(args)...);
            co_return co_await logAsync(LogLevel::TRACE, std::move(msg),
                                        location);
        } else {
            co_return co_await logAsync(LogLevel::TRACE, std::string(format),
                                        location);
        }
    }

    /**
     * @brief Asynchronously logs a DEBUG level message.
     * @tparam Args Format parameter types.
     * @param format Format string.
     * @param args Format arguments.
     * @param location Source code location information (automatically
     * captured).
     * @return Task<void> Coroutine task that can be awaited or ignored.
     */
    template <Loggable... Args>
    [[nodiscard]] Task<void> debug(const String& format, Args&&... args,
                                   const std::source_location& location =
                                       std::source_location::current()) {
        if constexpr (sizeof...(args) > 0) {
            auto msg = std::format(format.c_str(), std::forward<Args>(args)...);
            co_return co_await logAsync(LogLevel::DEBUG, std::move(msg),
                                        location);
        } else {
            co_return co_await logAsync(LogLevel::DEBUG, std::string(format),
                                        location);
        }
    }

    /**
     * @brief Asynchronously logs an INFO level message.
     * @tparam Args Format parameter types.
     * @param format Format string.
     * @param args Format arguments.
     * @param location Source code location information (automatically
     * captured).
     * @return Task<void> Coroutine task that can be awaited or ignored.
     */
    template <Loggable... Args>
    [[nodiscard]] Task<void> info(const String& format, Args&&... args,
                                  const std::source_location& location =
                                      std::source_location::current()) {
        if constexpr (sizeof...(args) > 0) {
            auto msg = std::format(format.c_str(), std::forward<Args>(args)...);
            co_return co_await logAsync(LogLevel::INFO, std::move(msg),
                                        location);
        } else {
            co_return co_await logAsync(LogLevel::INFO, std::string(format),
                                        location);
        }
    }

    /**
     * @brief Asynchronously logs a WARN level message.
     * @tparam Args Format parameter types.
     * @param format Format string.
     * @param args Format arguments.
     * @param location Source code location information (automatically
     * captured).
     * @return Task<void> Coroutine task that can be awaited or ignored.
     */
    template <Loggable... Args>
    [[nodiscard]] Task<void> warn(const String& format, Args&&... args,
                                  const std::source_location& location =
                                      std::source_location::current()) {
        if constexpr (sizeof...(args) > 0) {
            auto msg = std::format(format.c_str(), std::forward<Args>(args)...);
            co_return co_await logAsync(LogLevel::WARN, std::move(msg),
                                        location);
        } else {
            co_return co_await logAsync(LogLevel::WARN, std::string(format),
                                        location);
        }
    }

    /**
     * @brief Asynchronously logs an ERROR level message.
     * @tparam Args Format parameter types.
     * @param format Format string.
     * @param args Format arguments.
     * @param location Source code location information (automatically
     * captured).
     * @return Task<void> Coroutine task that can be awaited or ignored.
     */
    template <Loggable... Args>
    [[nodiscard]] Task<void> error(const String& format, Args&&... args,
                                   const std::source_location& location =
                                       std::source_location::current()) {
        if constexpr (sizeof...(args) > 0) {
            auto msg = std::format(format.c_str(), std::forward<Args>(args)...);
            co_return co_await logAsync(LogLevel::ERROR, std::move(msg),
                                        location);
        } else {
            co_return co_await logAsync(LogLevel::ERROR, std::string(format),
                                        location);
        }
    }

    /**
     * @brief Asynchronously logs a CRITICAL level message.
     * @tparam Args Format parameter types.
     * @param format Format string.
     * @param args Format arguments.
     * @param location Source code location information (automatically
     * captured).
     * @return Task<void> Coroutine task that can be awaited or ignored.
     */
    template <Loggable... Args>
    [[nodiscard]] Task<void> critical(const String& format, Args&&... args,
                                      const std::source_location& location =
                                          std::source_location::current()) {
        if constexpr (sizeof...(args) > 0) {
            auto msg = std::format(format.c_str(), std::forward<Args>(args)...);
            co_return co_await logAsync(LogLevel::CRITICAL, std::move(msg),
                                        location);
        } else {
            co_return co_await logAsync(LogLevel::CRITICAL, std::string(format),
                                        location);
        }
    }

    /**
     * @brief Sets the log level.
     * @param level Log level.
     */
    void setLevel(LogLevel level);

    /**
     * @brief Sets the thread name.
     * @param name Thread name.
     */
    void setThreadName(const String& name);

    /**
     * @brief Flushes all pending log messages and waits for processing to
     * complete.
     * @return Task<void> Coroutine task indicating flush completion.
     */
    Task<void> flush();

    /**
     * @brief Sets the internal base logger.
     * @param logger Base logger pointer.
     */
    void setUnderlyingLogger(std::shared_ptr<Logger> logger);

    /**
     * @brief Enables or disables system logging.
     * @param enable True to enable system logging, false to disable.
     */
    void enableSystemLogging(bool enable);

    /**
     * @brief Sets the auto-flush interval for log messages
     * @param interval Time between automatic flushes (in milliseconds)
     */
    void setAutoFlushInterval(std::chrono::milliseconds interval);

    /**
     * @brief Sets the queue capacity
     * @param capacity Maximum number of messages in queue
     */
    void setQueueCapacity(size_t capacity);

    /**
     * @brief Gets logging statistics
     * @return JSON formatted statistics string
     */
    [[nodiscard]] std::string getStatistics() const noexcept;

    /**
     * @brief Wait for all logs to be processed with timeout
     * @param timeout Maximum duration to wait
     * @return true if all logs processed, false if timeout occurred
     */
    [[nodiscard]] bool waitForCompletion(
        std::chrono::milliseconds timeout = std::chrono::seconds(5)) noexcept;

private:
    class AsyncLoggerImpl;  // Forward declaration
    std::unique_ptr<AsyncLoggerImpl> impl_;

    /**
     * @brief Asynchronously logs a message.
     * @param level Log level.
     * @param msg Message content.
     * @param location Source code location.
     * @return Task<void> Coroutine task indicating log operation completion.
     */
    Task<void> logAsync(LogLevel level, std::string msg,
                        const std::source_location& location);
};

}  // namespace atom::log

#endif  // ATOM_LOG_ASYNC_LOGGER_HPP