#pragma once

#include "../core/concepts.h"
#include "../core/context.h"
#include "../core/types.h"
#include "../events/event_system.h"
#include "../filters/filter.h"
#include "../sampling/sampler.h"
#include "../utils/structured_data.h"
#include "../utils/timer.h"

#include <spdlog/spdlog.h>
#include <format>
#include <memory>
#include <ranges>
#include <source_location>

/**
 * @file logger.h
 * @brief Defines the Logger class, the main logging interface for modern_log.
 */

namespace modern_log {

/**
 * @class Logger
 * @brief The main logging interface for modern_log.
 *
 * The Logger class provides a modern, flexible, and extensible logging
 * interface. It supports structured logging, context propagation, filtering,
 * sampling, event hooks, performance statistics, and integration with spdlog as
 * the backend.
 *
 * Key features:
 * - Level-based logging (trace, debug, info, warn, error, critical)
 * - Structured and batch logging
 * - Context-aware logging (with contextual enrichment)
 * - Range and structured data logging
 * - Conditional and exception logging
 * - Performance statistics tracking
 * - Log filtering and sampling
 * - Event system integration for log events
 * - RAII-based scoped timing utilities
 * - Thread-safe operations
 */
class Logger {
private:
    std::shared_ptr<spdlog::logger>
        logger_;                         ///< Underlying spdlog logger instance.
    LogContext context_;                 ///< Current logging context.
    std::unique_ptr<LogFilter> filter_;  ///< Log filter for dynamic filtering.
    std::unique_ptr<LogSampler>
        sampler_;  ///< Log sampler for sampling strategies.
    LogEventSystem*
        event_system_;  ///< Pointer to the event system for log events.
    LogType log_type_ = LogType::general;  ///< Type/category of this logger.
    mutable LogStats stats_;  ///< Performance and usage statistics.

public:
    /**
     * @brief Constructs a Logger with a given spdlog logger and optional event
     * system.
     * @param logger Shared pointer to the underlying spdlog logger.
     * @param event_system Optional pointer to a LogEventSystem for event hooks.
     */
    explicit Logger(std::shared_ptr<spdlog::logger> logger,
                    LogEventSystem* event_system = nullptr);

    /**
     * @brief Log a trace-level message with source location.
     * @tparam Args Format arguments.
     * @param fmt Format string.
     * @param args Arguments for formatting.
     * @param loc Source location (automatically captured).
     */
    template <Formattable... Args>
    void trace(
        std::format_string<Args...> fmt, Args&&... args,
        const std::source_location& loc = std::source_location::current());

    /**
     * @brief Log a debug-level message with source location.
     * @tparam Args Format arguments.
     * @param fmt Format string.
     * @param args Arguments for formatting.
     * @param loc Source location (automatically captured).
     */
    template <Formattable... Args>
    void debug(
        std::format_string<Args...> fmt, Args&&... args,
        const std::source_location& loc = std::source_location::current());

    /**
     * @brief Log an info-level message with source location.
     * @tparam Args Format arguments.
     * @param fmt Format string.
     * @param args Arguments for formatting.
     * @param loc Source location (automatically captured).
     */
    template <Formattable... Args>
    void info(
        std::format_string<Args...> fmt, Args&&... args,
        const std::source_location& loc = std::source_location::current());

    /**
     * @brief Log a warning-level message with source location.
     * @tparam Args Format arguments.
     * @param fmt Format string.
     * @param args Arguments for formatting.
     * @param loc Source location (automatically captured).
     */
    template <Formattable... Args>
    void warn(
        std::format_string<Args...> fmt, Args&&... args,
        const std::source_location& loc = std::source_location::current());

    /**
     * @brief Log an error-level message with source location.
     * @tparam Args Format arguments.
     * @param fmt Format string.
     * @param args Arguments for formatting.
     * @param loc Source location (automatically captured).
     */
    template <Formattable... Args>
    void error(
        std::format_string<Args...> fmt, Args&&... args,
        const std::source_location& loc = std::source_location::current());

    /**
     * @brief Log a critical-level message with source location.
     * @tparam Args Format arguments.
     * @param fmt Format string.
     * @param args Arguments for formatting.
     * @param loc Source location (automatically captured).
     */
    template <Formattable... Args>
    void critical(
        std::format_string<Args...> fmt, Args&&... args,
        const std::source_location& loc = std::source_location::current());

    /**
     * @brief Log a message with a custom context.
     * @tparam Args Format arguments.
     * @param level Log level.
     * @param ctx Log context to enrich the message.
     * @param fmt Format string.
     * @param args Arguments for formatting.
     */
    template <Formattable... Args>
    void log_with_context(Level level, const LogContext& ctx,
                          std::format_string<Args...> fmt, Args&&... args);

    /**
     * @brief Log a range of values as a structured message.
     * @tparam R Range type.
     * @param level Log level.
     * @param name Name of the range variable.
     * @param range The range to log.
     */
    template <std::ranges::range R>
        requires Formattable<std::ranges::range_value_t<R>>
    void log_range(Level level, std::string_view name, const R& range);

    /**
     * @brief Log structured data.
     * @param level Log level.
     * @param data StructuredData object to log.
     */
    void log_structured(Level level, const StructuredData& data);

    /**
     * @brief Log a message only if a condition is true.
     * @tparam Args Format arguments.
     * @param condition Condition to check.
     * @param level Log level.
     * @param fmt Format string.
     * @param args Arguments for formatting.
     */
    template <Formattable... Args>
    void log_if(bool condition, Level level, std::format_string<Args...> fmt,
                Args&&... args);

    /**
     * @brief Log an exception with optional context.
     * @param level Log level.
     * @param ex Exception to log.
     * @param context Optional context string.
     */
    void log_exception(Level level, const std::exception& ex,
                       std::string_view context = "");

    /**
     * @brief Start a scoped timer for performance measurement.
     * @param name Name of the timer.
     * @param level Log level for the timer result.
     * @return ScopedTimer object (RAII).
     */
    ScopedTimer time_scope(std::string name, Level level = Level::info);

    /**
     * @brief Log multiple messages in a batch.
     * @tparam Messages Variadic message types.
     * @param level Log level.
     * @param messages Messages to log.
     */
    template <typename... Messages>
    void log_batch(Level level, Messages&&... messages);

    /**
     * @brief Merge a new context into the logger.
     * @param ctx Context to merge.
     * @return Reference to this logger.
     */
    Logger& with_context(const LogContext& ctx);

    /**
     * @brief Clear the current logging context.
     * @return Reference to this logger.
     */
    Logger& clear_context();

    /**
     * @brief Get the current logging context.
     * @return Reference to the current LogContext.
     */
    const LogContext& get_context() const;

    /**
     * @brief Add a filter function to the logger.
     * @param filter Filter function to add.
     */
    void add_filter(LogFilter::FilterFunc filter);

    /**
     * @brief Clear all filters from the logger.
     */
    void clear_filters();

    /**
     * @brief Set the log sampling strategy and rate.
     * @param strategy Sampling strategy.
     * @param rate Sampling rate (default 1.0).
     */
    void set_sampling(SamplingStrategy strategy, double rate = 1.0);

    /**
     * @brief Set the log level for this logger.
     * @param level Log level to set.
     */
    void set_level(Level level);

    /**
     * @brief Get the current log level.
     * @return Current log level.
     */
    Level get_level() const;

    /**
     * @brief Check if a message at the given level would be logged.
     * @param level Log level to check.
     * @return True if the message would be logged.
     */
    bool should_log(Level level) const;

    /**
     * @brief Set the log type/category for this logger.
     * @param type LogType to set.
     */
    void set_log_type(LogType type);

    /**
     * @brief Get the log type/category for this logger.
     * @return Current LogType.
     */
    LogType get_log_type() const;

    /**
     * @brief Get the current logging statistics.
     * @return Reference to LogStats.
     */
    const LogStats& get_stats() const;

    /**
     * @brief Reset the logging statistics.
     */
    void reset_stats();

    /**
     * @brief Flush the logger (force output of all buffered logs).
     */
    void flush();

    /**
     * @brief Set the log level at which the logger will flush automatically.
     * @param level Log level to trigger flush.
     */
    void set_flush_level(Level level);

    /**
     * @brief Get the underlying spdlog logger instance.
     * @return Shared pointer to spdlog::logger.
     */
    std::shared_ptr<spdlog::logger> get_spdlog_logger() const;

    /**
     * @brief Log a message at the given level (internal).
     * @param level Log level.
     * @param message Message to log.
     */
    void log_internal(Level level, const std::string& message);

private:
    /**
     * @brief Check if a message at the given level should be logged (internal).
     * @param level Log level to check.
     * @return True if the message should be logged.
     */
    bool should_log_internal(Level level) const;

    /**
     * @brief Log a message with source location information.
     * @tparam Args Format arguments.
     * @param level Log level.
     * @param fmt Format string.
     * @param args Arguments for formatting.
     * @param loc Source location.
     */
    template <Formattable... Args>
    void log_with_location(Level level, std::format_string<Args...> fmt,
                           Args&&... args, const std::source_location& loc);

    /**
     * @brief Enrich a log message with context information.
     * @param message Original message.
     * @param ctx Context to add.
     * @return Enriched message string.
     */
    std::string enrich_message_with_context(const std::string& message,
                                            const LogContext& ctx) const;

    /**
     * @brief Emit a log event to the event system.
     * @param event LogEvent type.
     * @param data Optional event data.
     */
    void emit_event(LogEvent event, const std::any& data = {});
};

}  // namespace modern_log
