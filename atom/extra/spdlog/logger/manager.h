#pragma once

#include "../core/error.h"
#include "../core/types.h"
#include "../events/event_system.h"
#include "../utils/archiver.h"
#include "logger.h"


#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>


#include <memory>
#include <shared_mutex>
#include <thread>
#include <unordered_map>


namespace modern_log {

/**
 * @class LogManager
 * @brief Singleton, thread-safe manager for all loggers in the system.
 *
 * The LogManager class is responsible for the lifecycle and configuration of
 * all Logger instances in the application. It provides thread-safe creation,
 * retrieval, and removal of loggers, manages the global event system and log
 * archiver, and supports both synchronous and asynchronous logging backends.
 *
 * Key features:
 * - Singleton pattern for global access
 * - Thread-safe logger registry
 * - Support for multiple logger types (console, file, rotating, async)
 * - Centralized event system and log archiving
 * - Global statistics and maintenance thread for background tasks
 */
class LogManager {
private:
    std::unordered_map<std::string, std::shared_ptr<Logger>>
        loggers_;  ///< Map of logger name to Logger instance.
    mutable std::shared_mutex mutex_;  ///< Mutex for thread-safe access.
    std::unique_ptr<LogEventSystem>
        event_system_;  ///< Central event system for log events.
    std::unique_ptr<LogArchiver>
        archiver_;  ///< Optional log archiver for backups.

    std::atomic<bool> shutdown_requested_{
        false};  ///< Flag to request shutdown of background thread.
    std::thread maintenance_thread_;  ///< Background maintenance thread.

    /**
     * @brief Private constructor for singleton pattern.
     */
    LogManager();

public:
    /**
     * @brief Destructor. Ensures proper shutdown and resource cleanup.
     */
    ~LogManager();

    /**
     * @brief Get the singleton instance of LogManager.
     * @return Reference to the global LogManager instance.
     */
    static LogManager& instance();

    /**
     * @brief Create a new logger with the given configuration.
     * @param config LogConfig structure describing logger settings.
     * @return Result containing the created Logger or an error.
     */
    Result<std::shared_ptr<Logger>> create_logger(const LogConfig& config);

    /**
     * @brief Retrieve a logger by name.
     * @param name Name of the logger.
     * @return Result containing the Logger or an error if not found.
     */
    Result<std::shared_ptr<Logger>> get_logger(const std::string& name);

    /**
     * @brief Remove a logger by name.
     * @param name Name of the logger to remove.
     * @return True if the logger was removed, false if not found.
     */
    bool remove_logger(const std::string& name);

    /**
     * @brief Get a list of all registered logger names.
     * @return Vector of logger names.
     */
    std::vector<std::string> get_logger_names() const;

    /**
     * @brief Get the number of registered loggers.
     * @return Logger count.
     */
    size_t logger_count() const;

    /**
     * @brief Flush all loggers, ensuring all buffered logs are written.
     */
    void flush_all();

    /**
     * @brief Set the log level for all loggers globally.
     * @param level Log level to set.
     */
    void set_global_level(Level level);

    /**
     * @brief Get the global event system for log events.
     * @return Reference to the LogEventSystem.
     */
    LogEventSystem& get_event_system() { return *event_system_; }

    /**
     * @brief Set the log archiver for backup or archival purposes.
     * @param archiver Unique pointer to a LogArchiver instance.
     */
    void set_archiver(std::unique_ptr<LogArchiver> archiver);

    /**
     * @brief Get the current log archiver.
     * @return Pointer to the LogArchiver, or nullptr if not set.
     */
    LogArchiver* get_archiver() const { return archiver_.get(); }

    /**
     * @brief Get the global default logger.
     * @return Reference to the default Logger.
     */
    static Logger& default_logger();

    /**
     * @brief Create a simple logger with default configuration.
     * @param name Logger name.
     * @param level Log level (default: info).
     * @param console Whether to log to console (default: true).
     * @return Result containing the created Logger or an error.
     */
    static Result<std::shared_ptr<Logger>> create_simple_logger(
        const std::string& name, Level level = Level::info,
        bool console = true);

    /**
     * @brief Create a file logger.
     * @param name Logger name.
     * @param filename File path for log output.
     * @param level Log level (default: info).
     * @param rotating Whether to use rotating file sink (default: false).
     * @return Result containing the created Logger or an error.
     */
    static Result<std::shared_ptr<Logger>> create_file_logger(
        const std::string& name, const std::string& filename,
        Level level = Level::info, bool rotating = false);

    /**
     * @brief Create an asynchronous logger.
     * @param name Logger name.
     * @param config LogConfig structure describing logger settings.
     * @return Result containing the created Logger or an error.
     */
    static Result<std::shared_ptr<Logger>> create_async_logger(
        const std::string& name, const LogConfig& config);

    /**
     * @struct GlobalStats
     * @brief Aggregated statistics for all loggers managed by LogManager.
     */
    struct GlobalStats {
        size_t total_loggers;  ///< Total number of loggers.
        size_t total_logs;     ///< Total number of log messages.
        size_t total_errors;   ///< Total number of errors encountered.
        std::chrono::steady_clock::time_point
            start_time;              ///< Start time of the manager.
        double avg_logs_per_second;  ///< Average logs per second since start.
    };

    /**
     * @brief Get global statistics for all loggers.
     * @return GlobalStats structure with aggregated statistics.
     */
    GlobalStats get_global_stats() const;

    /**
     * @brief Shutdown the LogManager, stopping background tasks and flushing
     * logs.
     */
    void shutdown();

private:
    /**
     * @brief Background maintenance loop for periodic tasks.
     */
    void maintenance_loop();

    /**
     * @brief Create default sinks for a logger based on configuration.
     * @param config LogConfig structure.
     * @param sinks Vector to populate with created sinks.
     */
    void create_default_sinks(const LogConfig& config,
                              std::vector<spdlog::sink_ptr>& sinks);

    /**
     * @brief Setup asynchronous logging for a logger.
     * @param config LogConfig structure.
     */
    void setup_async_logging(const LogConfig& config);
};

}  // namespace modern_log
