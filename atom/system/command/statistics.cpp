/*
 * statistics.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "statistics.hpp"

#include <atomic>
#include <iomanip>
#include <mutex>
#include <sstream>

#include <spdlog/spdlog.h>

namespace atom::system {

// Global statistics tracking
namespace {
    std::mutex g_statsMutex;
    std::atomic<size_t> g_totalExecutions{0};
    std::atomic<size_t> g_successfulExecutions{0};
    std::atomic<size_t> g_failedExecutions{0};
    std::atomic<size_t> g_timedOutExecutions{0};
    std::atomic<std::chrono::milliseconds::rep> g_totalExecutionTime{0};
}

void incrementTotalExecutions() {
    g_totalExecutions++;
}

void incrementSuccessfulExecutions() {
    g_successfulExecutions++;
}

void incrementFailedExecutions() {
    g_failedExecutions++;
}

void incrementTimedOutExecutions() {
    g_timedOutExecutions++;
}

void addExecutionTime(long long milliseconds) {
    g_totalExecutionTime += milliseconds;
}

auto getExecutionStatistics() -> std::string {
    std::lock_guard<std::mutex> lock(g_statsMutex);

    std::ostringstream stats;
    stats << "Command Execution Statistics:\n";
    stats << "  Total Executions: " << g_totalExecutions.load() << "\n";
    stats << "  Successful: " << g_successfulExecutions.load() << "\n";
    stats << "  Failed: " << g_failedExecutions.load() << "\n";
    stats << "  Timed Out: " << g_timedOutExecutions.load() << "\n";

    auto totalTime = g_totalExecutionTime.load();
    stats << "  Total Execution Time: " << totalTime << "ms\n";

    if (g_totalExecutions.load() > 0) {
        auto avgTime = totalTime / g_totalExecutions.load();
        stats << "  Average Execution Time: " << avgTime << "ms\n";

        auto successRate = (g_successfulExecutions.load() * 100.0) / g_totalExecutions.load();
        stats << "  Success Rate: " << std::fixed << std::setprecision(2) << successRate << "%\n";
    }

    return stats.str();
}

void clearExecutionStatistics() {
    std::lock_guard<std::mutex> lock(g_statsMutex);

    g_totalExecutions = 0;
    g_successfulExecutions = 0;
    g_failedExecutions = 0;
    g_timedOutExecutions = 0;
    g_totalExecutionTime = 0;

    spdlog::info("Execution statistics cleared");
}

auto getExecutionMetrics() -> const CommandSystemMetrics& {
    return COMMAND_METRICS();
}

void resetExecutionStatistics() {
    const_cast<CommandSystemMetrics&>(COMMAND_METRICS()).reset();
    clearExecutionStatistics(); // Also clear legacy statistics
}

}  // namespace atom::system
