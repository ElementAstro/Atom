/*
 * logger.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-08-19

Description: Enhanced Custom Logger Manager with spdlog support

**************************************************/

#ifndef ATOM_LOG_LOGGER_HPP
#define ATOM_LOG_LOGGER_HPP

#include <chrono>
#include <memory>
#include <string_view>

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"

// Use type aliases from high_performance.hpp
using atom::containers::String;
using atom::containers::Vector;

namespace lithium {

/**
 * @brief Enum representing different log levels.
 */
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5,
    OFF = 6,
    UNKNOWN = 7
};

/**
 * @brief Convert log level to string.
 */
String logLevelToString(LogLevel level);

/**
 * @brief Convert string to log level.
 */
LogLevel stringToLogLevel(std::string_view levelStr);

/**
 * @brief Struct representing a parsed log entry with enhanced fields.
 */
struct LogEntry {
    String
        fileName;    ///< The name of the file where the log entry was recorded.
    int lineNumber;  ///< The line number in the file where the log entry was
                     ///< recorded.
    String message;  ///< The log message.
    LogLevel level;  ///< The log level.
    std::chrono::system_clock::time_point
        timestamp;      ///< Timestamp of the log entry.
    String threadId;    ///< Thread ID (if available).
    String logger;      ///< Logger name (if available).
    String function;    ///< Function name (if available).
    String sourceFile;  ///< Source file name (if available).
    int sourceLine;     ///< Source line number (if available).
    String category;    ///< Custom category for classification.
} ATOM_ALIGNAS(128);

/**
 * @brief Struct representing log format configuration.
 */
struct LogFormat {
    String name;                ///< Format name.
    String pattern;             ///< Regex pattern for parsing.
    Vector<String> fieldOrder;  ///< Order of fields in the pattern.
    String timestampFormat;     ///< Timestamp format string.
    bool enabled;               ///< Whether this format is enabled.
};

/**
 * @brief Struct for log analysis results.
 */
struct LogAnalysisResult {
    std::map<LogLevel, int> levelCount;    ///< Count by log level.
    std::map<String, int> categoryCount;   ///< Count by category.
    std::map<String, int> errorTypeCount;  ///< Count by error type.
    std::map<String, int> loggerCount;     ///< Count by logger name.
    Vector<LogEntry> criticalErrors;       ///< Critical error entries.
    Vector<LogEntry> frequentErrors;       ///< Most frequent errors.
    String summary;                        ///< Analysis summary.
    std::chrono::system_clock::time_point startTime;  ///< Earliest log entry.
    std::chrono::system_clock::time_point endTime;    ///< Latest log entry.
};

/**
 * @brief Enhanced Logger manager class for scanning, analyzing, and uploading
 * log files.
 */
class LoggerManager {
public:
    /**
     * @brief Constructs a LoggerManager object.
     */
    LoggerManager();

    /**
     * @brief Destructs the LoggerManager object.
     */
    ~LoggerManager();

    /**
     * @brief Scans the specified folder for log files.
     * @param folderPath The path to the folder containing log files.
     * @param recursive Whether to scan recursively.
     */
    void scanLogsFolder(const String &folderPath, bool recursive = false);

    /**
     * @brief Searches the logs for entries containing the specified keyword.
     * @param keyword The keyword to search for.
     * @param level Optional log level filter.
     * @param category Optional category filter.
     * @return A vector of log entries matching the criteria.
     */
    Vector<LogEntry> searchLogs(std::string_view keyword,
                                LogLevel level = LogLevel::UNKNOWN,
                                std::string_view category = "");

    /**
     * @brief Uploads the specified log file.
     * @param filePath The path to the log file to upload.
     */
    void uploadFile(const String &filePath);

    /**
     * @brief Analyzes the collected log files.
     * @return Analysis results.
     */
    LogAnalysisResult analyzeLogs();

    /**
     * @brief Add a custom log format.
     * @param format The log format configuration.
     */
    void addLogFormat(const LogFormat &format);

    /**
     * @brief Remove a log format by name.
     * @param formatName The name of the format to remove.
     */
    void removeLogFormat(const String &formatName);

    /**
     * @brief Set custom log level mappings.
     * @param levelMappings Map of string representations to log levels.
     */
    void setCustomLogLevels(const std::map<String, LogLevel> &levelMappings);

    /**
     * @brief Add custom category rules.
     * @param categoryRules Map of regex patterns to category names.
     */
    void addCategoryRules(const std::map<String, String> &categoryRules);

    /**
     * @brief Filter logs by time range.
     * @param startTime Start of time range.
     * @param endTime End of time range.
     * @return Filtered log entries.
     */
    Vector<LogEntry> filterLogsByTimeRange(
        const std::chrono::system_clock::time_point &startTime,
        const std::chrono::system_clock::time_point &endTime);

    /**
     * @brief Filter logs by log level.
     * @param minLevel Minimum log level to include.
     * @return Filtered log entries.
     */
    Vector<LogEntry> filterLogsByLevel(LogLevel minLevel);

    /**
     * @brief Export logs to different formats.
     * @param filePath Output file path.
     * @param format Export format ("json", "csv", "xml").
     * @param entries Entries to export (empty = all entries).
     */
    void exportLogs(const String &filePath, const String &format,
                    const Vector<LogEntry> &entries = {});

    /**
     * @brief Get statistics about the loaded logs.
     * @return Log statistics.
     */
    LogAnalysisResult getLogStatistics();

    /**
     * @brief Clear all loaded log entries.
     */
    void clearLogs();

private:
    class Impl;  ///< Forward declaration of the implementation class.
    std::unique_ptr<Impl>
        pImpl;  ///< Pointer to the implementation (Pimpl idiom).
};

}  // namespace lithium

#endif  // ATOM_LOG_LOGGER_HPP
