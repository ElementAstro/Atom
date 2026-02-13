#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>
#include <memory>
#include <vector>

#if ATOM_HAS_SPDLOG
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/fmt/fmt.h>
#endif

namespace dotenv::logging {

/**
 * @brief Log levels for structured logging
 */
enum class LogLevel : uint8_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5
};

/**
 * @brief Performance metrics for logging operations
 */
struct LogMetrics {
    std::atomic<uint64_t> total_logs{0};
    std::atomic<uint64_t> trace_logs{0};
    std::atomic<uint64_t> debug_logs{0};
    std::atomic<uint64_t> info_logs{0};
    std::atomic<uint64_t> warn_logs{0};
    std::atomic<uint64_t> error_logs{0};
    std::atomic<uint64_t> critical_logs{0};
    std::atomic<uint64_t> dropped_logs{0};
    std::atomic<uint64_t> total_bytes{0};

    void increment(LogLevel level, size_t bytes = 0) noexcept {
        total_logs.fetch_add(1, std::memory_order_relaxed);
        total_bytes.fetch_add(bytes, std::memory_order_relaxed);

        switch (level) {
            case LogLevel::Trace: trace_logs.fetch_add(1, std::memory_order_relaxed); break;
            case LogLevel::Debug: debug_logs.fetch_add(1, std::memory_order_relaxed); break;
            case LogLevel::Info: info_logs.fetch_add(1, std::memory_order_relaxed); break;
            case LogLevel::Warn: warn_logs.fetch_add(1, std::memory_order_relaxed); break;
            case LogLevel::Error: error_logs.fetch_add(1, std::memory_order_relaxed); break;
            case LogLevel::Critical: critical_logs.fetch_add(1, std::memory_order_relaxed); break;
        }
    }

    void increment_dropped() noexcept {
        dropped_logs.fetch_add(1, std::memory_order_relaxed);
    }
};

/**
 * @brief Lock-free log entry for high-performance logging
 */
struct LogEntry {
    LogLevel level;
    std::chrono::high_resolution_clock::time_point timestamp;
    std::thread::id thread_id;
    std::string_view category;
    std::string message;
    std::source_location location;

    template<typename... Args>
    LogEntry(LogLevel lvl, std::string_view cat, std::format_string<Args...> fmt,
             Args&&... args, std::source_location loc = std::source_location::current())
        : level(lvl)
        , timestamp(std::chrono::high_resolution_clock::now())
        , thread_id(std::this_thread::get_id())
        , category(cat)
        , message(std::format(fmt, std::forward<Args>(args)...))
        , location(loc) {}
};

/**
 * @brief High-performance logger with lock-free queues and spdlog integration
 */
class HighPerformanceLogger {
private:
    static constexpr size_t QUEUE_SIZE = 8192;
    static constexpr size_t MAX_MESSAGE_SIZE = 1024;

    using LogQueue = concurrency::WorkStealingQueue<LogEntry>;
    using LogPool = memory::LockFreeMemoryPool<sizeof(LogEntry), QUEUE_SIZE>;

    std::unique_ptr<LogQueue> log_queue_;
    std::unique_ptr<LogPool> log_pool_;
    std::thread worker_thread_;
    std::atomic<bool> shutdown_{false};
    LogMetrics metrics_;

#if ATOM_HAS_SPDLOG
    std::shared_ptr<spdlog::logger> spdlog_logger_;
#endif

    void worker_loop() {
        while (!shutdown_.load(std::memory_order_acquire)) {
            if (auto entry = log_queue_->steal()) {
                process_log_entry(*entry);
            } else {
                std::this_thread::yield();
            }
        }

        // Process remaining entries
        while (auto entry = log_queue_->steal()) {
            process_log_entry(*entry);
        }
    }

    void process_log_entry(const LogEntry& entry) {
#if ATOM_HAS_SPDLOG
        if (spdlog_logger_) {
            auto spdlog_level = convert_log_level(entry.level);

            spdlog_logger_->log(spdlog::source_loc{
                entry.location.file_name(),
                static_cast<int>(entry.location.line()),
                entry.location.function_name()
            }, spdlog_level, "[{}] {}", entry.category, entry.message);
        }
#endif

        metrics_.increment(entry.level, entry.message.size());
    }

#if ATOM_HAS_SPDLOG
    spdlog::level::level_enum convert_log_level(LogLevel level) const noexcept {
        switch (level) {
            case LogLevel::Trace: return spdlog::level::trace;
            case LogLevel::Debug: return spdlog::level::debug;
            case LogLevel::Info: return spdlog::level::info;
            case LogLevel::Warn: return spdlog::level::warn;
            case LogLevel::Error: return spdlog::level::err;
            case LogLevel::Critical: return spdlog::level::critical;
            default: return spdlog::level::info;
        }
    }
#endif

public:
    explicit HighPerformanceLogger(const std::string& logger_name = "dotenv")
        : log_queue_(std::make_unique<LogQueue>())
        , log_pool_(std::make_unique<LogPool>()) {

#if ATOM_HAS_SPDLOG
        try {
            // Initialize async logger with high-performance settings
            spdlog::init_thread_pool(8192, 1);

            auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                "logs/dotenv.log", 1024 * 1024 * 10, 3);

            std::vector<spdlog::sink_ptr> sinks{stdout_sink, file_sink};

            spdlog_logger_ = std::make_shared<spdlog::async_logger>(
                logger_name, sinks.begin(), sinks.end(), spdlog::thread_pool(),
                spdlog::async_overflow_policy::block);

            spdlog_logger_->set_level(spdlog::level::trace);
            spdlog_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

            spdlog::register_logger(spdlog_logger_);

        } catch (const std::exception& e) {
            // Fallback to console logging
            spdlog_logger_ = spdlog::stdout_color_mt(logger_name);
        }
#endif

        worker_thread_ = std::thread(&HighPerformanceLogger::worker_loop, this);
    }

    ~HighPerformanceLogger() {
        shutdown();
    }

    HighPerformanceLogger(const HighPerformanceLogger&) = delete;
    HighPerformanceLogger& operator=(const HighPerformanceLogger&) = delete;

    /**
     * @brief Log a message with specified level
     */
    template<typename... Args>
    void log(LogLevel level, std::string_view category,
             std::format_string<Args...> fmt, Args&&... args,
             std::source_location loc = std::source_location::current()) {

        if (shutdown_.load(std::memory_order_acquire)) {
            return;
        }

        try {
            LogEntry entry(level, category, fmt, std::forward<Args>(args)..., loc);

            if (entry.message.size() > MAX_MESSAGE_SIZE) {
                entry.message.resize(MAX_MESSAGE_SIZE);
                entry.message += "... [truncated]";
            }

            log_queue_->push_back(std::move(entry));

        } catch (const std::exception&) {
            metrics_.increment_dropped();
        }
    }

    /**
     * @brief Convenience logging methods
     */
    template<typename... Args>
    void trace(std::string_view category, std::format_string<Args...> fmt, Args&&... args,
               std::source_location loc = std::source_location::current()) {
        log(LogLevel::Trace, category, fmt, std::forward<Args>(args)..., loc);
    }

    template<typename... Args>
    void debug(std::string_view category, std::format_string<Args...> fmt, Args&&... args,
               std::source_location loc = std::source_location::current()) {
        log(LogLevel::Debug, category, fmt, std::forward<Args>(args)..., loc);
    }

    template<typename... Args>
    void info(std::string_view category, std::format_string<Args...> fmt, Args&&... args,
              std::source_location loc = std::source_location::current()) {
        log(LogLevel::Info, category, fmt, std::forward<Args>(args)..., loc);
    }

    template<typename... Args>
    void warn(std::string_view category, std::format_string<Args...> fmt, Args&&... args,
              std::source_location loc = std::source_location::current()) {
        log(LogLevel::Warn, category, fmt, std::forward<Args>(args)..., loc);
    }

    template<typename... Args>
    void error(std::string_view category, std::format_string<Args...> fmt, Args&&... args,
               std::source_location loc = std::source_location::current()) {
        log(LogLevel::Error, category, fmt, std::forward<Args>(args)..., loc);
    }

    template<typename... Args>
    void critical(std::string_view category, std::format_string<Args...> fmt, Args&&... args,
                  std::source_location loc = std::source_location::current()) {
        log(LogLevel::Critical, category, fmt, std::forward<Args>(args)..., loc);
    }

    /**
     * @brief Get logging metrics
     */
    const LogMetrics& get_metrics() const noexcept {
        return metrics_;
    }

    /**
     * @brief Shutdown the logger
     */
    void shutdown() {
        if (!shutdown_.exchange(true, std::memory_order_acq_rel)) {
            if (worker_thread_.joinable()) {
                worker_thread_.join();
            }

#if ATOM_HAS_SPDLOG
            if (spdlog_logger_) {
                spdlog_logger_->flush();
            }
#endif
        }
    }

    /**
     * @brief Flush all pending log entries
     */
    void flush() {
#if ATOM_HAS_SPDLOG
        if (spdlog_logger_) {
            spdlog_logger_->flush();
        }
#endif
    }

    /**
     * @brief Set log level
     */
    void set_level(LogLevel level) {
#if ATOM_HAS_SPDLOG
        if (spdlog_logger_) {
            spdlog_logger_->set_level(convert_log_level(level));
        }
#endif
    }
};

/**
 * @brief Global logger instance
 */
inline HighPerformanceLogger& get_logger() {
    static HighPerformanceLogger logger;
    return logger;
}

/**
 * @brief Convenience macros for logging
 */
#define DOTENV_LOG_TRACE(category, ...) \
    dotenv::logging::get_logger().trace(category, __VA_ARGS__)

#define DOTENV_LOG_DEBUG(category, ...) \
    dotenv::logging::get_logger().debug(category, __VA_ARGS__)

#define DOTENV_LOG_INFO(category, ...) \
    dotenv::logging::get_logger().info(category, __VA_ARGS__)

#define DOTENV_LOG_WARN(category, ...) \
    dotenv::logging::get_logger().warn(category, __VA_ARGS__)

#define DOTENV_LOG_ERROR(category, ...) \
    dotenv::logging::get_logger().error(category, __VA_ARGS__)

#define DOTENV_LOG_CRITICAL(category, ...) \
    dotenv::logging::get_logger().critical(category, __VA_ARGS__)

} // namespace dotenv::logging
