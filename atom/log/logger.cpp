/*
 * logger.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-08-19

Description: Optimized Custom Logger Manager Implementation

**************************************************/

#include "logger.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <string_view>
#include <thread>
// #include <vector> // Provided by high_performance.hpp
// #include <string> // Provided by high_performance.hpp

#include "atom/containers/high_performance.hpp"  // Include high performance containers
#include "atom/log/loguru.hpp"
#include "atom/web/curl.hpp"

// Use type aliases from high_performance.hpp
using atom::containers::String;
using atom::containers::Vector;

namespace lithium {

class LoggerManager::Impl {
public:
    // Use String and Vector according to logger.hpp
    void scanLogsFolder(const String &folderPath);
    auto searchLogs(std::string_view keyword) -> Vector<LogEntry>;
    void uploadFile(const String &filePath);
    void analyzeLogs();

private:
    // Use String and Vector internally where appropriate
    void parseLog(const String &filePath);
    auto extractErrorMessages() -> Vector<String>;
    auto encryptFileContent(const String &content) -> String;
    auto getErrorType(std::string_view errorMessage)
        -> String;  // Return String
    auto getMostCommonErrorMessage(const Vector<String> &errorMessages)
        -> String;  // Input Vector, Return String

    Vector<LogEntry> logEntries_;  // Use Vector<LogEntry>
    // Use String as key for map
    std::map<String, int> errorTypeCount_;
    std::map<String, int> errorMessageCount_;
};

LoggerManager::LoggerManager() : pImpl(std::make_unique<Impl>()) {}
LoggerManager::~LoggerManager() = default;

// Match the declaration in logger.hpp
void LoggerManager::scanLogsFolder(const String &folderPath) {
    pImpl->scanLogsFolder(folderPath);
}

// Match the declaration in logger.hpp
auto LoggerManager::searchLogs(std::string_view keyword) -> Vector<LogEntry> {
    return pImpl->searchLogs(keyword);
}

// Match the declaration in logger.hpp
void LoggerManager::uploadFile(const String &filePath) {
    pImpl->uploadFile(filePath);
}

void LoggerManager::analyzeLogs() { pImpl->analyzeLogs(); }

// Use String for folderPath
void LoggerManager::Impl::scanLogsFolder(const String &folderPath) {
    std::vector<std::jthread> threads;  // Keep std::vector for jthread for now
    // Use c_str() for filesystem path if String is not implicitly convertible
    for (const auto &entry :
         std::filesystem::directory_iterator(folderPath.c_str())) {
        if (entry.is_regular_file()) {
            // Convert path back to String if needed, or use string()
            threads.emplace_back(&Impl::parseLog, this,
                                 String(entry.path().string()));
        }
    }
    // Destructors of jthread will join
}

// Return Vector<LogEntry>
auto LoggerManager::Impl::searchLogs(std::string_view keyword)
    -> Vector<LogEntry> {
    Vector<LogEntry> searchResults;
    searchResults.reserve(logEntries_.size());  // Optional pre-allocation
    for (const auto &logEntry : logEntries_) {
        // Use String::npos
        if (logEntry.message.find(keyword) != String::npos) {
            searchResults.push_back(logEntry);
        }
    }
    return searchResults;
}

// Use String for filePath
void LoggerManager::Impl::parseLog(const String &filePath) {
    // Use c_str() for ifstream if String is not std::string
    std::ifstream logFile(filePath.c_str());
    if (logFile.is_open()) {
        std::string line;  // Use std::string for reading lines
        int lineNumber = 1;
        while (std::getline(logFile, line)) {
            // Construct LogEntry with String members
            logEntries_.push_back({filePath, lineNumber++, String(line)});
        }
    } else {
        LOG_F(ERROR, "Failed to open log file: {}", filePath.c_str());
    }
}

// Use String for filePath
void LoggerManager::Impl::uploadFile(const String &filePath) {
    // Use c_str() for ifstream
    std::ifstream file(filePath.c_str(), std::ios::binary);
    if (!file) {
        LOG_F(ERROR, "Failed to open file: {}", filePath.c_str());
        return;
    }

    // Read into std::string first, then convert to String if needed
    std::string content_std((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    String content(content_std);  // Convert to String
    String encryptedContent = encryptFileContent(content);

    atom::web::CurlWrapper curl;
    curl.setUrl("https://lightapt.com/upload");
    curl.setRequestMethod("POST");
    curl.addHeader("Content-Type", "application/octet-stream");
    // Assuming setRequestBody can handle String or requires std::string/char*
    // If it needs std::string:
    curl.setRequestBody(
        std::string(encryptedContent.data(), encryptedContent.size()));
    // If it needs char*:
    // curl.setRequestBody(encryptedContent.c_str(), encryptedContent.size());

    curl.setOnErrorCallback([](CURLcode error) {
        LOG_F(ERROR, "Failed to upload file: curl error code {}",
              static_cast<int>(error));
    });

    // Assuming callback provides std::string, log it directly
    curl.setOnResponseCallback([](const std::string &response) {
        DLOG_F(INFO, "File uploaded successfully. Server response: {}",
               response);
    });

    curl.perform();
}

// Return Vector<String>
auto LoggerManager::Impl::extractErrorMessages() -> Vector<String> {
    Vector<String> errorMessages;
    for (const auto &logEntry : logEntries_) {
        // Use String::npos
        if (logEntry.message.find("[ERROR]") != String::npos) {
            errorMessages.push_back(logEntry.message);
            // Use c_str() for logging
            DLOG_F(INFO, "{}", logEntry.message.c_str());
        }
    }
    return errorMessages;
}

void LoggerManager::Impl::analyzeLogs() {
    auto errorMessages = extractErrorMessages();

    if (errorMessages.empty()) {
        DLOG_F(INFO, "No errors found in the logs.");
        return;
    }
    DLOG_F(INFO, "Analyzing logs...");

    errorTypeCount_.clear();  // Use member map
    for (const auto &errorMessage : errorMessages) {
        String errorType =
            getErrorType(errorMessage);  // getErrorType returns String
        errorTypeCount_[errorType]++;
    }

    DLOG_F(INFO, "Error Type Count:");
    for (const auto &[errorType, count] : errorTypeCount_) {
        // Use c_str() for logging String keys
        DLOG_F(INFO, "{} : {}", errorType.c_str(), count);
    }

    String mostCommonErrorMessage =
        getMostCommonErrorMessage(errorMessages);  // Returns String
    // Use c_str() for logging
    DLOG_F(INFO, "Most Common Error Message: {}",
           mostCommonErrorMessage.c_str());
}

// Input and return String
String LoggerManager::Impl::encryptFileContent(const String &content) {
    // Simple encryption example, can be replaced with more complex algorithms
    String encryptedContent;
    encryptedContent.reserve(content.size());  // Pre-allocate
    for (char c : content) {  // Iterate over String (assuming it's char-based)
        encryptedContent += c ^ 0xFF;
    }
    return encryptedContent;
}

// Return String
String LoggerManager::Impl::getErrorType(std::string_view errorMessage) {
    // Use std::string_view::npos
    auto startPos = errorMessage.find('[');
    auto endPos = errorMessage.find(']');
    if (startPos != std::string_view::npos &&
        endPos != std::string_view::npos && endPos > startPos) {
        // Construct String from std::string_view substring
        return String(errorMessage.substr(startPos + 1, endPos - startPos - 1));
    }
    return String("Unknown");  // Return String literal
}

// Input Vector<String>, return String
String LoggerManager::Impl::getMostCommonErrorMessage(
    const Vector<String> &errorMessages) {
    errorMessageCount_.clear();  // Use member map
    for (const auto &errorMessage : errorMessages) {
        errorMessageCount_[errorMessage]++;
    }

    String mostCommonErrorMessage;
    int maxCount = 0;
    for (const auto &[errorMessage, count] : errorMessageCount_) {
        if (count > maxCount) {
            mostCommonErrorMessage = errorMessage;
            maxCount = count;
        }
    }
    return mostCommonErrorMessage;
}

}  // namespace lithium
