#include "atom/log/logger.hpp"

#include <iostream>

int main() {
    // Create an instance of LoggerManager
    lithium::LoggerManager loggerManager;

    // Scan the logs folder
    // This function scans the specified folder for log files.
    // Replace "path/to/logs" with the actual path to your log files.
    loggerManager.scanLogsFolder("path/to/logs");

    // Search logs for a specific keyword
    // This function searches the logs for entries containing the specified
    // keyword. Replace "error" with the keyword you want to search for.
    std::vector<lithium::LogEntry> searchResults =
        loggerManager.searchLogs("error");

    // Print search results
    // Iterate through the search results and print each log entry.
    for (const auto& entry : searchResults) {
        std::cout << "File: " << entry.fileName
                  << ", Line: " << entry.lineNumber
                  << ", Message: " << entry.message << std::endl;
    }

    // Upload a log file
    // This function uploads the specified log file.
    // Replace "path/to/logfile.log" with the actual path to the log file you
    // want to upload.
    loggerManager.uploadFile("path/to/logfile.log");

    // Analyze logs
    // This function analyzes the collected log files.
    loggerManager.analyzeLogs();

    return 0;
}
