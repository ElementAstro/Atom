/*
 * logger.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-08-19

Description: Optimized Custom Logger Manager

**************************************************/

#ifndef ATOM_LOG_LOGGER_HPP
#define ATOM_LOG_LOGGER_HPP

#include <memory>
#include <string_view>

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"

// Use type aliases from high_performance.hpp
using atom::containers::String;
using atom::containers::Vector;

namespace lithium {

/**
 * @brief Struct representing a log entry.
 */
struct LogEntry {
    String
        fileName;    ///< The name of the file where the log entry was recorded.
    int lineNumber;  ///< The line number in the file where the log entry was
                     ///< recorded.
    String message;  ///< The log message.
} ATOM_ALIGNAS(128);

/**
 * @brief Logger manager class for scanning, analyzing, and uploading log files.
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
     */
    void scanLogsFolder(const String &folderPath);  // Use String

    /**
     * @brief Searches the logs for entries containing the specified keyword.
     * @param keyword The keyword to search for.
     * @return A vector of log entries containing the keyword.
     */
    Vector<LogEntry> searchLogs(
        std::string_view keyword);  // Return Vector, keep string_view for input

    /**
     * @brief Uploads the specified log file.
     * @param filePath The path to the log file to upload.
     */
    void uploadFile(const String &filePath);  // Use String

    /**
     * @brief Analyzes the collected log files.
     */
    void analyzeLogs();

private:
    class Impl;  ///< Forward declaration of the implementation class.
    std::unique_ptr<Impl>
        pImpl;  ///< Pointer to the implementation (Pimpl idiom).
};

}  // namespace lithium

#endif  // ATOM_LOG_LOGGER_HPP
