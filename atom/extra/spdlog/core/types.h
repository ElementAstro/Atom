#pragma once

#include <atomic>
#include <chrono>
#include <optional>
#include <string>

namespace modern_log {

/**
 * @enum Level
 * @brief Log level enumeration for controlling log verbosity.
 *
 * This strongly-typed enum defines the severity levels for log messages.
 * It ensures type safety and allows filtering of logs based on importance.
 */
enum class Level : int {
    trace = 0,  ///< Fine-grained informational events for debugging.
    debug = 1,  ///< Debug-level messages for development and troubleshooting.
    info = 2,   ///< Informational messages that highlight application progress.
    warn = 3,   ///< Potentially harmful situations or warnings.
    error = 4,  ///< Error events that might still allow the application to
                ///< continue running.
    critical = 5,  ///< Severe error events that will presumably lead the
                   ///< application to abort.
    off = 6        ///< Special level to turn off logging.
};

/**
 * @enum LogType
 * @brief Enumeration for categorizing different types of logs.
 *
 * This enum allows logs to be classified by their domain or purpose,
 * enabling more granular filtering and analysis.
 */
enum class LogType {
    general,      ///< General-purpose logs.
    security,     ///< Security-related logs.
    performance,  ///< Performance and profiling logs.
    business,     ///< Business logic or domain-specific logs.
    audit,        ///< Audit trail logs for compliance and tracking.
    system,       ///< System-level logs (OS, hardware, etc.).
    network,      ///< Network-related logs.
    database      ///< Database operation logs.
};

/**
 * @enum LogEvent
 * @brief Enumeration of internal log system events.
 *
 * These events represent significant actions or state changes within the
 * logging system itself.
 */
enum class LogEvent {
    logger_created,     ///< A logger instance was created.
    logger_destroyed,   ///< A logger instance was destroyed.
    level_changed,      ///< The log level was changed.
    sink_added,         ///< A log sink (output target) was added.
    sink_removed,       ///< A log sink was removed.
    error_occurred,     ///< An error occurred in the logging system.
    rotation_occurred,  ///< Log file rotation event.
    flush_triggered,    ///< Log flush was triggered.
    archive_completed   ///< Log archive operation completed.
};

/**
 * @enum SamplingStrategy
 * @brief Enumeration for log sampling strategies.
 *
 * Sampling strategies control how log messages are selected for output,
 * which can help reduce log volume or focus on important events.
 */
enum class SamplingStrategy {
    none,      ///< No sampling; log all messages.
    uniform,   ///< Uniform sampling at regular intervals.
    adaptive,  ///< Adaptive sampling based on log rate or other heuristics.
    burst      ///< Burst sampling for high-frequency events.
};

/**
 * @struct LogConfig
 * @brief Configuration structure for logger instances.
 *
 * This structure holds all configuration options for a logger, including
 * log level, output formatting, asynchronous logging, file output, and more.
 */
struct LogConfig {
    std::string name = "default";  ///< Logger name.
    Level level = Level::info;     ///< Minimum log level for output.
    std::string pattern =
        "[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v";  ///< Log message format
                                                    ///< pattern.
    bool async = false;              ///< Enable asynchronous logging.
    size_t async_queue_size = 8192;  ///< Size of the async log queue.
    size_t async_thread_count = 1;   ///< Number of threads for async logging.

    /**
     * @struct FileConfig
     * @brief Configuration for file-based log output.
     *
     * Contains options for log file naming, rotation, and retention.
     */
    struct FileConfig {
        std::string filename;   ///< Log file name (with path).
        bool rotating = false;  ///< Enable file rotation by size.
        size_t max_size =
            1048576 * 5;  ///< Maximum file size before rotation (default: 5MB).
        size_t max_files = 3;  ///< Maximum number of rotated files to keep.
        bool daily_rotation = false;  ///< Enable daily log file rotation.
        int rotation_hour = 0;    ///< Hour of day for daily rotation (0-23).
        int rotation_minute = 0;  ///< Minute of hour for daily rotation (0-59).
    };

    std::optional<FileConfig>
        file_config;             ///< Optional file output configuration.
    bool console_output = true;  ///< Enable output to console.
    bool colored_output = true;  ///< Enable colored console output.
};

/**
 * @struct LogStats
 * @brief Structure for collecting and reporting logging performance statistics.
 *
 * This structure tracks counts of total, filtered, sampled, and failed logs,
 * as well as the start time for calculating log throughput.
 */
struct LogStats {
    std::atomic<size_t> total_logs{0};     ///< Total number of logs processed.
    std::atomic<size_t> filtered_logs{0};  ///< Number of logs filtered out.
    std::atomic<size_t> sampled_logs{
        0};  ///< Number of logs sampled (selected for output).
    std::atomic<size_t> failed_logs{
        0};  ///< Number of logs that failed to be written.
    std::chrono::steady_clock::time_point start_time{
        std::chrono::steady_clock::now()};  ///< Logging start time.

    /**
     * @brief Calculate the average number of logs processed per second.
     * @return Logs per second as a double.
     */
    double get_logs_per_second() const {
        auto duration = std::chrono::steady_clock::now() - start_time;
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        return seconds > 0 ? static_cast<double>(total_logs.load()) / seconds
                           : 0.0;
    }
};

}  // namespace modern_log