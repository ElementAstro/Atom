/*
 * logger.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-08-19

Description: Enhanced Custom Logger Manager Implementation with spdlog support

**************************************************/

#include "logger.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>
#include <string_view>
#include <thread>

// #include <vector> // Provided by high_performance.hpp
// #include <string> // Provided by high_performance.hpp

#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include "atom/containers/high_performance.hpp"
#include "atom/web/curl.hpp"

// Use type aliases from high_performance.hpp
using atom::containers::String;
using atom::containers::Vector;

#undef ERROR

namespace lithium {

// Utility functions
String logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:
            return "TRACE";
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARN:
            return "WARN";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::CRITICAL:
            return "CRITICAL";
        case LogLevel::OFF:
            return "OFF";
        default:
            return "UNKNOWN";
    }
}

LogLevel stringToLogLevel(std::string_view levelStr) {
    String level(levelStr);
    std::transform(level.begin(), level.end(), level.begin(), ::toupper);

    if (level == "TRACE" || level == "T")
        return LogLevel::TRACE;
    if (level == "DEBUG" || level == "D")
        return LogLevel::DEBUG;
    if (level == "INFO" || level == "I")
        return LogLevel::INFO;
    if (level == "WARN" || level == "WARNING" || level == "W")
        return LogLevel::WARN;
    if (level == "ERROR" || level == "ERR" || level == "E")
        return LogLevel::ERROR;
    if (level == "CRITICAL" || level == "CRIT" || level == "C" ||
        level == "FATAL")
        return LogLevel::CRITICAL;
    if (level == "OFF")
        return LogLevel::OFF;

    return LogLevel::UNKNOWN;
}

class LoggerManager::Impl {
public:
    Impl();
    void scanLogsFolder(const String &folderPath, bool recursive);
    auto searchLogs(std::string_view keyword, LogLevel level,
                    std::string_view category) -> Vector<LogEntry>;
    void uploadFile(const String &filePath);
    auto analyzeLogs() -> LogAnalysisResult;
    void addLogFormat(const LogFormat &format);
    void removeLogFormat(const String &formatName);
    void setCustomLogLevels(const std::map<String, LogLevel> &levelMappings);
    void addCategoryRules(const std::map<String, String> &categoryRules);
    auto filterLogsByTimeRange(
        const std::chrono::system_clock::time_point &startTime,
        const std::chrono::system_clock::time_point &endTime)
        -> Vector<LogEntry>;
    auto filterLogsByLevel(LogLevel minLevel) -> Vector<LogEntry>;
    void exportLogs(const String &filePath, const String &format,
                    const Vector<LogEntry> &entries);
    auto getLogStatistics() -> LogAnalysisResult;
    void clearLogs();

private:
    void parseLog(const String &filePath);
    auto parseLogLine(const String &line, const String &fileName,
                      int lineNumber) -> LogEntry;
    auto parseTimestamp(const String &timestampStr, const String &format)
        -> std::chrono::system_clock::time_point;
    auto extractErrorType(const String &message) -> String;
    auto categorizeLogEntry(const LogEntry &entry) -> String;
    auto encryptFileContent(const String &content) -> String;
    void initializeDefaultFormats();
    auto detectLogFormat(const String &line) -> const LogFormat *;

    Vector<LogEntry> logEntries_;
    Vector<LogFormat> logFormats_;
    std::map<String, LogLevel> customLogLevels_;
    std::map<String, String> categoryRules_;
    mutable std::mutex entriesMutex_;
};

LoggerManager::Impl::Impl() { initializeDefaultFormats(); }

void LoggerManager::Impl::initializeDefaultFormats() {
    // spdlog default format: [2023-12-25 10:30:45.123] [info] [logger_name]
    // message
    LogFormat spdlogFormat;
    spdlogFormat.name = "spdlog_default";
    spdlogFormat.pattern =
        R"(\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})\] \[(\w+)\] \[([^\]]+)\] (.+))";
    spdlogFormat.fieldOrder = {"timestamp", "level", "logger", "message"};
    spdlogFormat.timestampFormat = "%Y-%m-%d %H:%M:%S";
    spdlogFormat.enabled = true;
    logFormats_.push_back(spdlogFormat);

    // spdlog custom format: [2023-12-25 10:30:45.123] [thread 12345] [info]
    // message
    LogFormat spdlogThread;
    spdlogThread.name = "spdlog_thread";
    spdlogThread.pattern =
        R"(\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})\] \[thread (\d+)\] \[(\w+)\] (.+))";
    spdlogThread.fieldOrder = {"timestamp", "thread", "level", "message"};
    spdlogThread.timestampFormat = "%Y-%m-%d %H:%M:%S";
    spdlogThread.enabled = true;
    logFormats_.push_back(spdlogThread);

    // Simple format: LEVEL: message
    LogFormat simpleFormat;
    simpleFormat.name = "simple";
    simpleFormat.pattern = R"((\w+): (.+))";
    simpleFormat.fieldOrder = {"level", "message"};
    simpleFormat.timestampFormat = "";
    simpleFormat.enabled = true;
    logFormats_.push_back(simpleFormat);

    // Standard format with timestamp: 2023-12-25 10:30:45 [LEVEL] message
    LogFormat standardFormat;
    standardFormat.name = "standard";
    standardFormat.pattern =
        R"((\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) \[(\w+)\] (.+))";
    standardFormat.fieldOrder = {"timestamp", "level", "message"};
    standardFormat.timestampFormat = "%Y-%m-%d %H:%M:%S";
    standardFormat.enabled = true;
    logFormats_.push_back(standardFormat);

    // Detailed format: 2023-12-25 10:30:45.123 [LEVEL] [file.cpp:123] [func]
    // message
    LogFormat detailedFormat;
    detailedFormat.name = "detailed";
    detailedFormat.pattern =
        R"((\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}) \[(\w+)\] \[([^:]+):(\d+)\] \[([^\]]+)\] (.+))";
    detailedFormat.fieldOrder = {"timestamp", "level",    "file",
                                 "line",      "function", "message"};
    detailedFormat.timestampFormat = "%Y-%m-%d %H:%M:%S";
    detailedFormat.enabled = true;
    logFormats_.push_back(detailedFormat);
}

const LogFormat *LoggerManager::Impl::detectLogFormat(const String &line) {
    for (const auto &format : logFormats_) {
        if (!format.enabled)
            continue;

        std::regex pattern(format.pattern.c_str());
        if (std::regex_match(line.c_str(), pattern)) {
            return &format;
        }
    }
    return nullptr;
}

std::chrono::system_clock::time_point LoggerManager::Impl::parseTimestamp(
    const String &timestampStr, const String &format) {
    if (format.empty() || timestampStr.empty()) {
        return std::chrono::system_clock::now();
    }

    std::tm tm = {};
    std::istringstream ss(timestampStr.c_str());

    // Handle milliseconds separately if present
    String cleanFormat = format;
    String cleanTimestamp = timestampStr;

    if (timestampStr.find('.') != String::npos) {
        size_t dotPos = timestampStr.find('.');
        cleanTimestamp = timestampStr.substr(0, dotPos);
        cleanFormat = format.substr(0, format.find('.'));
    }

    ss >> std::get_time(&tm, cleanFormat.c_str());

    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

LogEntry LoggerManager::Impl::parseLogLine(const String &line,
                                           const String &fileName,
                                           int lineNumber) {
    LogEntry entry;
    entry.fileName = fileName;
    entry.lineNumber = lineNumber;
    entry.message = line;
    entry.level = LogLevel::UNKNOWN;
    entry.timestamp = std::chrono::system_clock::now();
    entry.sourceLine = -1;

    const LogFormat *format = detectLogFormat(line);
    if (!format) {
        // Fallback: try to extract log level from message
        for (const auto &[levelStr, level] : customLogLevels_) {
            if (line.find(levelStr) != String::npos) {
                entry.level = level;
                break;
            }
        }
        entry.category = categorizeLogEntry(entry);
        return entry;
    }

    std::regex pattern(format->pattern.c_str());
    std::smatch matches;
    std::string lineStr(line.c_str());

    if (std::regex_match(lineStr, matches, pattern)) {
        for (size_t i = 0;
             i < format->fieldOrder.size() && i + 1 < matches.size(); ++i) {
            const String &field = format->fieldOrder[i];
            String value(matches[i + 1].str());

            if (field == "timestamp") {
                entry.timestamp =
                    parseTimestamp(value, format->timestampFormat);
            } else if (field == "level") {
                entry.level = stringToLogLevel(value);
            } else if (field == "logger") {
                entry.logger = value;
            } else if (field == "thread") {
                entry.threadId = value;
            } else if (field == "message") {
                entry.message = value;
            } else if (field == "function") {
                entry.function = value;
            } else if (field == "file") {
                entry.sourceFile = value;
            } else if (field == "line") {
                try {
                    entry.sourceLine = std::stoi(value.c_str());
                } catch (...) {
                    entry.sourceLine = -1;
                }
            }
        }
    }

    entry.category = categorizeLogEntry(entry);
    return entry;
}

String LoggerManager::Impl::categorizeLogEntry(const LogEntry &entry) {
    // Apply custom category rules first
    for (const auto &[pattern, category] : categoryRules_) {
        std::regex regex(pattern.c_str());
        if (std::regex_search(entry.message.c_str(), regex)) {
            return category;
        }
    }

    // Default categorization based on content
    String message = entry.message;
    std::transform(message.begin(), message.end(), message.begin(), ::tolower);

    if (message.find("database") != String::npos ||
        message.find("sql") != String::npos) {
        return "Database";
    } else if (message.find("network") != String::npos ||
               message.find("connection") != String::npos) {
        return "Network";
    } else if (message.find("auth") != String::npos ||
               message.find("login") != String::npos) {
        return "Authentication";
    } else if (message.find("file") != String::npos ||
               message.find("io") != String::npos) {
        return "FileSystem";
    } else if (message.find("memory") != String::npos ||
               message.find("allocation") != String::npos) {
        return "Memory";
    } else if (entry.level == LogLevel::ERROR ||
               entry.level == LogLevel::CRITICAL) {
        return "Error";
    } else if (entry.level == LogLevel::WARN) {
        return "Warning";
    } else {
        return "General";
    }
}

// Implementation of public interface methods
LoggerManager::LoggerManager() : pImpl(std::make_unique<Impl>()) {}
LoggerManager::~LoggerManager() = default;

void LoggerManager::scanLogsFolder(const String &folderPath, bool recursive) {
    pImpl->scanLogsFolder(folderPath, recursive);
}

Vector<LogEntry> LoggerManager::searchLogs(std::string_view keyword,
                                           LogLevel level,
                                           std::string_view category) {
    return pImpl->searchLogs(keyword, level, category);
}

void LoggerManager::uploadFile(const String &filePath) {
    pImpl->uploadFile(filePath);
}

LogAnalysisResult LoggerManager::analyzeLogs() { return pImpl->analyzeLogs(); }

void LoggerManager::addLogFormat(const LogFormat &format) {
    pImpl->addLogFormat(format);
}

void LoggerManager::removeLogFormat(const String &formatName) {
    pImpl->removeLogFormat(formatName);
}

void LoggerManager::setCustomLogLevels(
    const std::map<String, LogLevel> &levelMappings) {
    pImpl->setCustomLogLevels(levelMappings);
}

void LoggerManager::addCategoryRules(
    const std::map<String, String> &categoryRules) {
    pImpl->addCategoryRules(categoryRules);
}

Vector<LogEntry> LoggerManager::filterLogsByTimeRange(
    const std::chrono::system_clock::time_point &startTime,
    const std::chrono::system_clock::time_point &endTime) {
    return pImpl->filterLogsByTimeRange(startTime, endTime);
}

Vector<LogEntry> LoggerManager::filterLogsByLevel(LogLevel minLevel) {
    return pImpl->filterLogsByLevel(minLevel);
}

void LoggerManager::exportLogs(const String &filePath, const String &format,
                               const Vector<LogEntry> &entries) {
    pImpl->exportLogs(filePath, format, entries);
}

LogAnalysisResult LoggerManager::getLogStatistics() {
    return pImpl->getLogStatistics();
}

void LoggerManager::clearLogs() { pImpl->clearLogs(); }

void LoggerManager::Impl::scanLogsFolder(const String &folderPath,
                                         bool recursive) {
    std::vector<std::jthread> threads;

    auto scanDirectory = [this, &threads](const std::filesystem::path &path,
                                          bool recursive) {
        if (recursive) {
            for (const auto &entry :
                 std::filesystem::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    auto extension = entry.path().extension().string();
                    if (extension == ".log" || extension == ".txt") {
                        threads.emplace_back(&Impl::parseLog, this,
                                             String(entry.path().string()));
                    }
                }
            }
        } else {
            for (const auto &entry :
                 std::filesystem::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    auto extension = entry.path().extension().string();
                    if (extension == ".log" || extension == ".txt") {
                        threads.emplace_back(&Impl::parseLog, this,
                                             String(entry.path().string()));
                    }
                }
            }
        }
    };

    try {
        scanDirectory(folderPath.c_str(), recursive);
    } catch (const std::filesystem::filesystem_error &e) {
        SPDLOG_ERROR("Filesystem error while scanning {}: {}",
                     folderPath.c_str(), e.what());
    }
}

void LoggerManager::Impl::parseLog(const String &filePath) {
    std::ifstream logFile(filePath.c_str());
    if (!logFile.is_open()) {
        SPDLOG_ERROR("Failed to open log file: {}", filePath.c_str());
        return;
    }

    std::string line;
    int lineNumber = 1;
    Vector<LogEntry> tempEntries;

    while (std::getline(logFile, line)) {
        if (!line.empty()) {
            LogEntry entry = parseLogLine(String(line), filePath, lineNumber);
            tempEntries.push_back(std::move(entry));
        }
        lineNumber++;
    }

    // Thread-safe insertion
    {
        std::lock_guard<std::mutex> lock(entriesMutex_);
        logEntries_.insert(logEntries_.end(), tempEntries.begin(),
                           tempEntries.end());
    }

    SPDLOG_INFO("Parsed {} log entries from {}", tempEntries.size(),
                filePath.c_str());
}

Vector<LogEntry> LoggerManager::Impl::searchLogs(std::string_view keyword,
                                                 LogLevel level,
                                                 std::string_view category) {
    std::lock_guard<std::mutex> lock(entriesMutex_);
    Vector<LogEntry> results;

    for (const auto &entry : logEntries_) {
        bool matches = true;

        // Keyword filter
        if (!keyword.empty() && entry.message.find(keyword) == String::npos) {
            matches = false;
        }

        // Level filter
        if (level != LogLevel::UNKNOWN && entry.level != level) {
            matches = false;
        }

        // Category filter
        if (!category.empty() &&
            entry.category.find(category) == String::npos) {
            matches = false;
        }

        if (matches) {
            results.push_back(entry);
        }
    }

    return results;
}

LogAnalysisResult LoggerManager::Impl::analyzeLogs() {
    std::lock_guard<std::mutex> lock(entriesMutex_);
    LogAnalysisResult result;

    if (logEntries_.empty()) {
        result.summary = "No log entries to analyze";
        return result;
    }

    // Initialize time range
    result.startTime = logEntries_[0].timestamp;
    result.endTime = logEntries_[0].timestamp;

    // Analyze entries
    for (const auto &entry : logEntries_) {
        // Count by level
        result.levelCount[entry.level]++;

        // Count by category
        result.categoryCount[entry.category]++;

        // Count by logger
        if (!entry.logger.empty()) {
            result.loggerCount[entry.logger]++;
        }

        // Collect critical errors
        if (entry.level == LogLevel::CRITICAL) {
            result.criticalErrors.push_back(entry);
        }

        // Extract error types for errors
        if (entry.level == LogLevel::ERROR ||
            entry.level == LogLevel::CRITICAL) {
            String errorType = extractErrorType(entry.message);
            result.errorTypeCount[errorType]++;
        }

        // Update time range
        if (entry.timestamp < result.startTime) {
            result.startTime = entry.timestamp;
        }
        if (entry.timestamp > result.endTime) {
            result.endTime = entry.timestamp;
        }
    }

    // Find frequent errors (top 10)
    Vector<std::pair<String, int>> errorFreq;
    for (const auto &[error, count] : result.errorTypeCount) {
        errorFreq.emplace_back(error, count);
    }

    std::sort(errorFreq.begin(), errorFreq.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    // Generate summary
    std::ostringstream summary;
    summary << "Log Analysis Summary:\n";
    summary << "Total entries: " << logEntries_.size() << "\n";
    summary << "Critical errors: " << result.criticalErrors.size() << "\n";
    summary << "Error types: " << result.errorTypeCount.size() << "\n";
    summary << "Categories: " << result.categoryCount.size() << "\n";

    result.summary = String(summary.str());

    return result;
}

String LoggerManager::Impl::extractErrorType(const String &message) {
    // Extract error types using common patterns
    std::regex patterns[] = {
        std::regex(R"((Exception|Error|Failure)\s*:\s*([^:\n]+))"),
        std::regex(R"(([A-Z][a-zA-Z]*(?:Exception|Error|Failure)))"),
        std::regex(R"((failed|error|exception)\s+([a-zA-Z0-9_]+))"),
    };

    for (const auto &pattern : patterns) {
        std::smatch matches;
        std::string msgStr(message.c_str());
        if (std::regex_search(msgStr, matches, pattern)) {
            return String(matches[1].str());
        }
    }

    return "Generic Error";
}

void LoggerManager::Impl::uploadFile(const String &filePath) {
    try {
        std::ifstream file(filePath.c_str(), std::ios::binary);
        if (!file.is_open()) {
            SPDLOG_ERROR("Failed to open file for upload: {}",
                         filePath.c_str());
            return;
        }

        // Read file content
        std::ostringstream buffer;
        buffer << file.rdbuf();
        String fileContent(buffer.str());

        // Encrypt file content for secure upload
        String encryptedContent = encryptFileContent(fileContent);

        // Create upload request using atom::web::CurlWrapper
        atom::web::CurlWrapper curl;
        curl.setUrl("https://api.logserver.example.com/upload");
        curl.addHeader("Content-Type", "application/octet-stream");
        curl.addHeader(
            "X-File-Name",
            std::filesystem::path(filePath.c_str()).filename().string());
        curl.setRequestBody(encryptedContent.c_str());

        std::string response = curl.perform();
        // Check response - CurlWrapper doesn't return HTTP codes directly
        if (!response.empty()) {
            SPDLOG_INFO("Successfully uploaded log file: {}", filePath.c_str());
        } else {
            SPDLOG_ERROR("Failed to upload log file: {}", filePath.c_str());
        }
    } catch (const std::exception &e) {
        SPDLOG_ERROR("Exception during file upload: {}", e.what());
    }
}

Vector<LogEntry> LoggerManager::Impl::filterLogsByTimeRange(
    const std::chrono::system_clock::time_point &startTime,
    const std::chrono::system_clock::time_point &endTime) {
    std::lock_guard<std::mutex> lock(entriesMutex_);
    Vector<LogEntry> results;

    for (const auto &entry : logEntries_) {
        if (entry.timestamp >= startTime && entry.timestamp <= endTime) {
            results.push_back(entry);
        }
    }

    SPDLOG_INFO("Filtered {} log entries by time range", results.size());
    return results;
}

Vector<LogEntry> LoggerManager::Impl::filterLogsByLevel(LogLevel minLevel) {
    std::lock_guard<std::mutex> lock(entriesMutex_);
    Vector<LogEntry> results;

    for (const auto &entry : logEntries_) {
        if (entry.level >= minLevel) {
            results.push_back(entry);
        }
    }

    SPDLOG_INFO("Filtered {} log entries by level >= {}", results.size(),
                logLevelToString(minLevel).c_str());
    return results;
}

void LoggerManager::Impl::exportLogs(const String &filePath,
                                     const String &format,
                                     const Vector<LogEntry> &entries) {
    Vector<LogEntry> entriesToExport = entries.empty() ? logEntries_ : entries;

    try {
        std::ofstream outFile(filePath.c_str());
        if (!outFile.is_open()) {
            SPDLOG_ERROR("Failed to open export file: {}", filePath.c_str());
            return;
        }

        String formatLower = format;
        std::transform(formatLower.begin(), formatLower.end(),
                       formatLower.begin(), ::tolower);

        if (formatLower == "json") {
            outFile << "[\n";
            for (size_t i = 0; i < entriesToExport.size(); ++i) {
                const auto &entry = entriesToExport[i];
                auto timestamp_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        entry.timestamp.time_since_epoch())
                        .count();

                outFile << "  {\n";
                outFile << "    \"fileName\": \"" << entry.fileName << "\",\n";
                outFile << "    \"lineNumber\": " << entry.lineNumber << ",\n";
                outFile << "    \"message\": \"" << entry.message << "\",\n";
                outFile << "    \"level\": \"" << logLevelToString(entry.level)
                        << "\",\n";
                outFile << "    \"timestamp\": " << timestamp_ms << ",\n";
                outFile << "    \"threadId\": \"" << entry.threadId << "\",\n";
                outFile << "    \"logger\": \"" << entry.logger << "\",\n";
                outFile << "    \"category\": \"" << entry.category << "\"\n";
                outFile << "  }" << (i < entriesToExport.size() - 1 ? "," : "")
                        << "\n";
            }
            outFile << "]\n";
        } else if (formatLower == "csv") {
            outFile << "FileName,LineNumber,Message,Level,Timestamp,ThreadId,"
                       "Logger,Category\n";
            for (const auto &entry : entriesToExport) {
                auto timestamp_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        entry.timestamp.time_since_epoch())
                        .count();

                outFile << "\"" << entry.fileName << "\"," << entry.lineNumber
                        << ","
                        << "\"" << entry.message << "\","
                        << "\"" << logLevelToString(entry.level) << "\","
                        << timestamp_ms << ","
                        << "\"" << entry.threadId << "\","
                        << "\"" << entry.logger << "\","
                        << "\"" << entry.category << "\"\n";
            }
        } else if (formatLower == "xml") {
            outFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<logs>\n";
            for (const auto &entry : entriesToExport) {
                auto timestamp_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        entry.timestamp.time_since_epoch())
                        .count();

                outFile << "  <entry>\n";
                outFile << "    <fileName>" << entry.fileName
                        << "</fileName>\n";
                outFile << "    <lineNumber>" << entry.lineNumber
                        << "</lineNumber>\n";
                outFile << "    <message><![CDATA[" << entry.message
                        << "]]></message>\n";
                outFile << "    <level>" << logLevelToString(entry.level)
                        << "</level>\n";
                outFile << "    <timestamp>" << timestamp_ms
                        << "</timestamp>\n";
                outFile << "    <threadId>" << entry.threadId
                        << "</threadId>\n";
                outFile << "    <logger>" << entry.logger << "</logger>\n";
                outFile << "    <category>" << entry.category
                        << "</category>\n";
                outFile << "  </entry>\n";
            }
            outFile << "</logs>\n";
        } else {
            SPDLOG_ERROR("Unsupported export format: {}", format.c_str());
            return;
        }

        SPDLOG_INFO("Exported {} log entries to {} in {} format",
                    entriesToExport.size(), filePath.c_str(), format.c_str());
    } catch (const std::exception &e) {
        SPDLOG_ERROR("Exception during log export: {}", e.what());
    }
}

void LoggerManager::Impl::addLogFormat(const LogFormat &format) {
    // Check if format with same name already exists
    auto it = std::find_if(logFormats_.begin(), logFormats_.end(),
                           [&format](const LogFormat &existing) {
                               return existing.name == format.name;
                           });

    if (it != logFormats_.end()) {
        *it = format;  // Replace existing format
        SPDLOG_INFO("Updated existing log format: {}", format.name.c_str());
    } else {
        logFormats_.push_back(format);
        SPDLOG_INFO("Added new log format: {}", format.name.c_str());
    }
}

void LoggerManager::Impl::removeLogFormat(const String &formatName) {
    auto it = std::remove_if(logFormats_.begin(), logFormats_.end(),
                             [&formatName](const LogFormat &format) {
                                 return format.name == formatName;
                             });

    if (it != logFormats_.end()) {
        logFormats_.erase(it, logFormats_.end());
        SPDLOG_INFO("Removed log format: {}", formatName.c_str());
    } else {
        SPDLOG_WARN("Log format not found for removal: {}", formatName.c_str());
    }
}

void LoggerManager::Impl::setCustomLogLevels(
    const std::map<String, LogLevel> &levelMappings) {
    customLogLevels_ = levelMappings;
    SPDLOG_INFO("Updated custom log level mappings, total: {}",
                levelMappings.size());
}

void LoggerManager::Impl::addCategoryRules(
    const std::map<String, String> &categoryRules) {
    for (const auto &[pattern, category] : categoryRules) {
        categoryRules_[pattern] = category;
    }
    SPDLOG_INFO("Added {} category rules", categoryRules.size());
}

LogAnalysisResult LoggerManager::Impl::getLogStatistics() {
    return analyzeLogs();  // Reuse the existing analysis function
}

void LoggerManager::Impl::clearLogs() {
    std::lock_guard<std::mutex> lock(entriesMutex_);
    logEntries_.clear();
    SPDLOG_INFO("Cleared all log entries");
}

String LoggerManager::Impl::encryptFileContent(const String &content) {
    // Simple base64-like encoding for demonstration
    // In production, use proper encryption like AES
    std::ostringstream encoded;
    for (size_t i = 0; i < content.size(); ++i) {
        encoded << std::hex << static_cast<int>(content[i]);
    }
    return String(encoded.str());
}

}  // namespace lithium
