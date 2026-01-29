/**
 * @file integration_examples.cpp
 * @brief Integration examples demonstrating how different utilities work
 * together
 *
 * This example demonstrates real-world scenarios where multiple utilities
 * from atom::utils are combined to solve complex problems:
 * - Log processing with string utilities, time handling, and error tracking
 * - Data serialization with crypto, compression, and type conversion
 * - Configuration management with XML parsing, validation, and file I/O
 * - Performance monitoring with timing, memory tracking, and statistics
 * - Network data processing with UTF conversion, validation, and formatting
 */

#include "atom/utils/container/linq.hpp"
#include "atom/utils/debug/color_print.hpp"
#include "atom/utils/debug/error_stack.hpp"
#include "atom/utils/random/random.hpp"
#include "atom/utils/text/string.hpp"
#include "atom/utils/text/to_string.hpp"
#include "atom/utils/time/stopwatcher.hpp"
#include "atom/utils/time/time.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace atom::utils;

// Helper function to print section headersvoid printSection(const std::string&
// title) {
ColorPrinter::printColoredLine("==========================================",
                               ColorCode::Cyan, TextStyle::Bold);
ColorPrinter::printColoredLine("  " + title, ColorCode::Cyan, TextStyle::Bold);
ColorPrinter::printColoredLine("==========================================",
                               ColorCode::Cyan, TextStyle::Bold);
}

// Helper function to print subsection headersvoid printSubsection(const
// std::string& title) {
ColorPrinter::printColoredLine("--- " + title + " ---", ColorCode::Yellow,
                               TextStyle::Bold);
}

// ============================
// Example 1: Log Processing System
// ============================

struct LogEntry {
    std::string timestamp;
    std::string level;
    std::string module;
    std::string message;
    int responseTime;
};

class LogProcessor {
private:
    std::shared_ptr<atom::error::ErrorStack> errorStack_;
    StopWatcher processingTimer_;
    std::vector<LogEntry> logs_;

public:
    LogProcessor() : errorStack_(atom::error::ErrorStack::createShared()) {}

    void processLogFile(const std::string& content) {
        processingTimer_.start();

        // Split content into lines using string utilities
        // Convert SplitString range to vector for easier iteration
        std::vector<std::string> lines;
        for (const auto& line : split(content, '\n')) {
            lines.push_back(std::string(line));
        }

        ColorPrinter::info("Processing {} log lines", lines.size());

        for (size_t i = 0; i < lines.size(); ++i) {
            const auto& line = lines[i];
            if (line.empty())
                continue;

            try {
                LogEntry entry = parseLogLine(line);
                logs_.push_back(entry);
            } catch (const std::exception& e) {
                errorStack_->insertError(
                    "Failed to parse log line: " + std::string(e.what()),
                    "LogProcessor", "processLogFile", __LINE__, __FILE__);
            }
        }

        processingTimer_.stop();
        ColorPrinter::success("Processed {} valid log entries in {:.2f}ms",
                              logs_.size(),
                              processingTimer_.elapsedMilliseconds());
    }

    LogEntry parseLogLine(const std::string& line) {
        // Simple log format: [timestamp] LEVEL module: message (responseTime
        // ms)
        LogEntry entry;

        // Extract timestamp
        size_t timestampEnd = line.find(']');
        if (timestampEnd == std::string::npos) {
            throw std::runtime_error("Invalid log format: missing timestamp");
        }
        entry.timestamp = line.substr(1, timestampEnd - 1);

        // Extract level
        size_t levelStart = timestampEnd + 2;
        size_t levelEnd = line.find(' ', levelStart);
        entry.level = line.substr(levelStart, levelEnd - levelStart);

        // Extract module
        size_t moduleStart = levelEnd + 1;
        size_t moduleEnd = line.find(':', moduleStart);
        entry.module = line.substr(moduleStart, moduleEnd - moduleStart);

        // Extract message and response time
        size_t messageStart = moduleEnd + 2;
        size_t responseStart = line.rfind('(');
        if (responseStart != std::string::npos) {
            entry.message =
                line.substr(messageStart, responseStart - messageStart - 1);
            size_t responseEnd = line.rfind("ms)");
            if (responseEnd != std::string::npos) {
                std::string responseStr = line.substr(
                    responseStart + 1, responseEnd - responseStart - 1);
                entry.responseTime = std::stoi(responseStr);
            }
        } else {
            entry.message = line.substr(messageStart);
            entry.responseTime = 0;
        }

        return entry;
    }

    void generateReport() {
        printSubsection("Log Analysis Report");

        if (logs_.empty()) {
            ColorPrinter::warning("No logs to analyze");
            return;
        }

        // Use LINQ-style operations for analysis
        auto logQuery = from(logs_);

        // Count by log level - simplified approach without groupBy
        std::map<std::string, size_t> levelCounts;
        for (const auto& entry : logs_) {
            levelCounts[entry.level]++;
        }

        std::cout << "Log Level Distribution:" << std::endl;
        for (const auto& [level, count] : levelCounts) {
            ColorCode color = (level == "ERROR")  ? ColorCode::Red
                              : (level == "WARN") ? ColorCode::Yellow
                              : (level == "INFO") ? ColorCode::Green
                                                  : ColorCode::White;
            ColorPrinter::printColored("  " + level + ": ", color,
                                       TextStyle::Bold);
            std::cout << count << " entries" << std::endl;
        }

        // Average response time by module - simplified approach
        std::map<std::string, std::vector<int>> moduleResponses;
        for (const auto& entry : logs_) {
            if (entry.responseTime > 0) {
                moduleResponses[entry.module].push_back(entry.responseTime);
            }
        }

        std::vector<std::pair<std::string, double>> moduleStats;
        for (const auto& [module, responses] : moduleResponses) {
            double sum = 0.0;
            for (int resp : responses) {
                sum += resp;
            }
            double avg = sum / responses.size();
            moduleStats.push_back({module, avg});
        }

        // Sort by average response time descending
        std::sort(
            moduleStats.begin(), moduleStats.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        std::cout << "\nAverage Response Time by Module:" << std::endl;
        for (const auto& [module, avgTime] : moduleStats) {
            ColorCode color = (avgTime > 1000)  ? ColorCode::Red
                              : (avgTime > 500) ? ColorCode::Yellow
                                                : ColorCode::Green;
            ColorPrinter::printColored("  " + module + ": ", ColorCode::White);
            ColorPrinter::printColored(toString(avgTime) + "ms", color,
                                       TextStyle::Bold);
            std::cout << std::endl;
        }

        // Show errors if any
        if (errorStack_->size() > 0) {
            ColorPrinter::error("Processing errors encountered:");
            errorStack_->printFilteredErrorStack();
        }
    }

    const std::vector<LogEntry>& getLogs() const { return logs_; }
};

// ============================
// Example 2: Data Processing Pipeline
// ============================

class DataProcessor {
private:
    StopWatcher timer_;
    Random<std::mt19937, std::uniform_int_distribution<int>> randomGen_;

public:
    DataProcessor() : randomGen_(1, 1000) {}

    std::vector<int> generateTestData(size_t count) {
        timer_.start();

        std::vector<int> data;
        data.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            data.push_back(randomGen_.generate());
        }

        timer_.stop();
        ColorPrinter::info("Generated {} data points in {:.2f}ms", count,
                           timer_.elapsedMilliseconds());

        return data;
    }

    void processData(const std::vector<int>& data) {
        printSubsection("Data Processing Pipeline");

        timer_.start();

        // Use LINQ-style operations for data processing
        auto dataQuery = from(data);

        // Statistical analysis
        auto stats =
            dataQuery.where([](int x) { return x > 0; })
                .select<double>([](int x) { return static_cast<double>(x); });

        double mean = stats.average();
        double sum = stats.sum();
        int count = static_cast<int>(stats.count());
        auto minMax = stats.minMax();

        timer_.stop();

        std::cout << "Data Statistics:" << std::endl;
        std::cout << "  Count: " << count << std::endl;
        std::cout << "  Sum: " << sum << std::endl;
        std::cout << "  Mean: " << mean << std::endl;
        std::cout << "  Min: " << minMax.first << std::endl;
        std::cout << "  Max: " << minMax.second << std::endl;
        std::cout << "  Processing time: " << timer_.elapsedMilliseconds()
                  << "ms" << std::endl;

        // Filter and transform data
        auto processedData = dataQuery.where([mean](int x) { return x > mean; })
                                 .select<int>([](int x) { return x * 2; })
                                 .orderByDescending([](int x) { return x; })
                                 .take(10)
                                 .toVector();

        std::cout << "\nTop 10 processed values (above mean, doubled):"
                  << std::endl;
        for (size_t i = 0; i < processedData.size(); ++i) {
            std::cout << "  " << (i + 1) << ": " << processedData[i]
                      << std::endl;
        }
    }
};

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  Integration Examples Demo" << std::endl;
    std::cout << "==========================================" << std::endl;

    // ============================
    // Example 1: Log Processing
    // ============================
    printSection("1. Log Processing System");

    // Generate sample log data
    std::string sampleLogs =
        "[2024-01-15 10:30:15] INFO WebServer: Request processed successfully "
        "(150 ms)\n"
        "[2024-01-15 10:30:16] ERROR Database: Connection timeout (5000 ms)\n"
        "[2024-01-15 10:30:17] WARN Cache: High memory usage detected (200 "
        "ms)\n"
        "[2024-01-15 10:30:18] INFO Auth: User login successful (75 ms)\n"
        "[2024-01-15 10:30:19] ERROR Network: API call failed (3000 ms)\n"
        "[2024-01-15 10:30:20] INFO WebServer: Static file served (25 ms)\n"
        "[2024-01-15 10:30:21] DEBUG Scheduler: Task queued\n"
        "[2024-01-15 10:30:22] INFO Database: Query executed (120 ms)\n";

    LogProcessor logProcessor;
    logProcessor.processLogFile(sampleLogs);
    logProcessor.generateReport();

    // ============================
    // Example 2: Data Processing
    // ============================
    printSection("2. Data Processing Pipeline");

    DataProcessor dataProcessor;
    auto testData = dataProcessor.generateTestData(1000);
    dataProcessor.processData(testData);

    ColorPrinter::success("All integration examples completed successfully!");

    return 0;
}
