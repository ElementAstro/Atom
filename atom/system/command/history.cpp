/*
 * history.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "history.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <list>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

// Global shared history instance
namespace {
    std::shared_ptr<CommandHistory> g_globalHistory;
    std::mutex g_globalHistoryMutex;
}

class CommandHistory::Impl {
public:
    explicit Impl(size_t maxSize, bool enablePersistence = false,
                  const std::string& persistenceFile = "")
        : maxSize_(maxSize), enablePersistence_(enablePersistence),
          persistenceFile_(persistenceFile) {

        if (persistenceFile_.empty() && enablePersistence_) {
            persistenceFile_ = "command_history.json";
        }

        if (enablePersistence_ && !persistenceFile_.empty()) {
            loadFromFile();
        }
    }

    ~Impl() {
        if (enablePersistence_ && !persistenceFile_.empty()) {
            saveToFile();
        }
    }

    void addCommandEntry(const CommandEntry& entry) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (history_.size() >= maxSize_) {
            history_.pop_front();
        }

        history_.push_back(entry);
        updateStatistics(entry);

        if (enablePersistence_ && history_.size() % 10 == 0) {
            // Periodically save to file
            saveToFile();
        }
    }

    void addCommand(const std::string &command, int exitStatus) {
        CommandEntry entry;
        entry.command = command;
        entry.exitStatus = exitStatus;
        entry.timestamp = std::chrono::system_clock::now();
        entry.workingDirectory = getCurrentWorkingDirectory();
        entry.user = getCurrentUser();

        addCommandEntry(entry);
    }

    void addCommandDetailed(const std::string& command, int exitStatus,
                           std::chrono::milliseconds executionTime,
                           const std::string& workingDirectory,
                           const std::string& user,
                           size_t outputSize) {
        CommandEntry entry;
        entry.command = command;
        entry.exitStatus = exitStatus;
        entry.timestamp = std::chrono::system_clock::now();
        entry.executionTime = executionTime;
        entry.workingDirectory = workingDirectory.empty() ? getCurrentWorkingDirectory() : workingDirectory;
        entry.user = user.empty() ? getCurrentUser() : user;
        entry.outputSize = outputSize;

        addCommandEntry(entry);
    }

    auto searchCommandsByCriteria(const HistorySearchCriteria& criteria) const
        -> std::vector<CommandEntry> {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<CommandEntry> result;
        std::regex pattern;
        bool useRegex = criteria.useRegex && !criteria.commandPattern.empty();

        if (useRegex) {
            try {
                std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
                if (!criteria.caseSensitive) {
                    flags |= std::regex_constants::icase;
                }
                pattern = std::regex(criteria.commandPattern, flags);
            } catch (const std::regex_error&) {
                useRegex = false; // Fall back to substring search
            }
        }

        for (const auto &entry : history_) {
            // Check command pattern
            if (!criteria.commandPattern.empty()) {
                bool matches = false;
                if (useRegex) {
                    matches = std::regex_search(entry.command, pattern);
                } else {
                    std::string command = entry.command;
                    std::string searchPattern = criteria.commandPattern;

                    if (!criteria.caseSensitive) {
                        std::transform(command.begin(), command.end(), command.begin(), ::tolower);
                        std::transform(searchPattern.begin(), searchPattern.end(), searchPattern.begin(), ::tolower);
                    }

                    matches = command.find(searchPattern) != std::string::npos;
                }

                if (!matches) continue;
            }

            // Check user filter
            if (!criteria.userFilter.empty() && entry.user != criteria.userFilter) {
                continue;
            }

            // Check working directory filter
            if (!criteria.workingDirectoryFilter.empty() &&
                entry.workingDirectory != criteria.workingDirectoryFilter) {
                continue;
            }

            // Check time range
            if (criteria.startTime != std::chrono::system_clock::time_point{} &&
                entry.timestamp < criteria.startTime) {
                continue;
            }

            if (criteria.endTime != std::chrono::system_clock::time_point{} &&
                entry.timestamp > criteria.endTime) {
                continue;
            }

            // Check exit status filter
            if (!criteria.exitStatusFilter.empty()) {
                bool statusMatches = std::find(criteria.exitStatusFilter.begin(),
                                             criteria.exitStatusFilter.end(),
                                             entry.exitStatus) != criteria.exitStatusFilter.end();
                if (!statusMatches) continue;
            }

            result.push_back(entry);

            if (result.size() >= criteria.maxResults) {
                break;
            }
        }

        return result;
    }

    auto getLastCommandEntries(size_t count) const -> std::vector<CommandEntry> {
        std::lock_guard<std::mutex> lock(mutex_);

        count = std::min(count, history_.size());
        std::vector<CommandEntry> result;
        result.reserve(count);

        auto it = history_.rbegin();
        for (size_t i = 0; i < count; ++i, ++it) {
            result.push_back(*it);
        }

        return result;
    }

    auto getLastCommands(size_t count) const
        -> std::vector<std::pair<std::string, int>> {
        auto entries = getLastCommandEntries(count);
        std::vector<std::pair<std::string, int>> result;
        result.reserve(entries.size());

        for (const auto& entry : entries) {
            result.emplace_back(entry.command, entry.exitStatus);
        }

        return result;
    }

    auto searchCommands(const std::string &substring) const
        -> std::vector<std::pair<std::string, int>> {
        HistorySearchCriteria criteria;
        criteria.commandPattern = substring;
        criteria.maxResults = 100;

        auto entries = searchCommandsByCriteria(criteria);
        std::vector<std::pair<std::string, int>> result;
        result.reserve(entries.size());

        for (const auto& entry : entries) {
            result.emplace_back(entry.command, entry.exitStatus);
        }

        return result;
    }

private:
    std::string getCurrentWorkingDirectory() const {
        char buffer[1024];
        if (getcwd(buffer, sizeof(buffer)) != nullptr) {
            return std::string(buffer);
        }
        return "";
    }

    std::string getCurrentUser() const {
#ifdef _WIN32
        char buffer[256];
        DWORD size = sizeof(buffer);
        if (GetUserNameA(buffer, &size)) {
            return std::string(buffer);
        }
#else
        const char* user = getenv("USER");
        if (user) {
            return std::string(user);
        }
#endif
        return "unknown";
    }

    void updateStatistics(const CommandEntry& entry) {
        // Update command frequency
        commandFrequency_[entry.command]++;

        // Update exit status frequency
        exitStatusFrequency_[entry.exitStatus]++;

        // Update total execution time
        totalExecutionTime_ += entry.executionTime;

        // Update most used command
        if (commandFrequency_[entry.command] >
            (mostUsedCommand_.empty() ? 0 : commandFrequency_[mostUsedCommand_])) {
            mostUsedCommand_ = entry.command;
        }

        // Update longest running command
        if (entry.executionTime > longestExecutionTime_) {
            longestExecutionTime_ = entry.executionTime;
            longestRunningCommand_ = entry.command;
        }
    }

public:
    auto getCommandFrequency(size_t topN) const -> std::unordered_map<std::string, size_t> {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::pair<std::string, size_t>> freqVec(
            commandFrequency_.begin(), commandFrequency_.end());

        std::sort(freqVec.begin(), freqVec.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        std::unordered_map<std::string, size_t> result;
        size_t count = std::min(topN, freqVec.size());

        for (size_t i = 0; i < count; ++i) {
            result[freqVec[i].first] = freqVec[i].second;
        }

        return result;
    }

    auto getStatistics() const -> HistoryStatistics {
        std::lock_guard<std::mutex> lock(mutex_);

        HistoryStatistics stats;
        stats.totalCommands = history_.size();
        stats.commandFrequency = commandFrequency_;
        stats.exitStatusFrequency = exitStatusFrequency_;
        stats.mostUsedCommand = mostUsedCommand_;
        stats.longestRunningCommand = longestRunningCommand_;
        stats.totalExecutionTime = totalExecutionTime_;

        // Calculate success/failure counts
        auto it = exitStatusFrequency_.find(0);
        if (it != exitStatusFrequency_.end()) {
            stats.successfulCommands = it->second;
        }
        stats.failedCommands = stats.totalCommands - stats.successfulCommands;

        // Calculate average execution time
        if (stats.totalCommands > 0) {
            stats.averageExecutionTime = std::chrono::milliseconds(
                totalExecutionTime_.count() / stats.totalCommands);
        }

        return stats;
    }

    auto saveToFile() const -> bool {
        if (persistenceFile_.empty()) return false;

        std::lock_guard<std::mutex> lock(mutex_);

        try {
            std::ofstream file(persistenceFile_);
            if (!file.is_open()) {
                spdlog::error("Failed to open history file for writing: {}", persistenceFile_);
                return false;
            }

            file << "{\n  \"history\": [\n";

            bool first = true;
            for (const auto& entry : history_) {
                if (!first) file << ",\n";
                first = false;

                file << "    {\n";
                file << "      \"command\": \"" << escapeJson(entry.command) << "\",\n";
                file << "      \"exitStatus\": " << entry.exitStatus << ",\n";
                file << "      \"timestamp\": " << std::chrono::duration_cast<std::chrono::seconds>(
                    entry.timestamp.time_since_epoch()).count() << ",\n";
                file << "      \"executionTime\": " << entry.executionTime.count() << ",\n";
                file << "      \"workingDirectory\": \"" << escapeJson(entry.workingDirectory) << "\",\n";
                file << "      \"user\": \"" << escapeJson(entry.user) << "\",\n";
                file << "      \"outputSize\": " << entry.outputSize << "\n";
                file << "    }";
            }

            file << "\n  ]\n}\n";
            file.close();

            spdlog::debug("Saved {} commands to history file: {}", history_.size(), persistenceFile_);
            return true;

        } catch (const std::exception& e) {
            spdlog::error("Failed to save history file: {}", e.what());
            return false;
        }
    }

    auto loadFromFile() -> bool {
        if (persistenceFile_.empty()) return false;

        std::lock_guard<std::mutex> lock(mutex_);

        try {
            std::ifstream file(persistenceFile_);
            if (!file.is_open()) {
                spdlog::debug("History file not found, starting with empty history: {}", persistenceFile_);
                return true; // Not an error for new installations
            }

            // Simple JSON parsing for command history
            // In a real implementation, you'd use a proper JSON library
            std::string line;
            CommandEntry entry;
            bool inEntry = false;

            while (std::getline(file, line)) {
                if (line.find("\"command\":") != std::string::npos) {
                    size_t start = line.find("\"", line.find(":") + 1) + 1;
                    size_t end = line.find("\"", start);
                    if (start != std::string::npos && end != std::string::npos) {
                        entry.command = line.substr(start, end - start);
                        inEntry = true;
                    }
                } else if (line.find("\"exitStatus\":") != std::string::npos && inEntry) {
                    size_t start = line.find(":") + 1;
                    entry.exitStatus = std::stoi(line.substr(start));
                } else if (line.find("\"timestamp\":") != std::string::npos && inEntry) {
                    size_t start = line.find(":") + 1;
                    auto timestamp = std::stoll(line.substr(start));
                    entry.timestamp = std::chrono::system_clock::from_time_t(timestamp);
                } else if (line.find("}") != std::string::npos && inEntry) {
                    history_.push_back(entry);
                    updateStatistics(entry);
                    entry = CommandEntry{}; // Reset
                    inEntry = false;
                }
            }

            file.close();
            spdlog::info("Loaded {} commands from history file: {}", history_.size(), persistenceFile_);
            return true;

        } catch (const std::exception& e) {
            spdlog::error("Failed to load history file: {}", e.what());
            return false;
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        history_.clear();
        commandFrequency_.clear();
        exitStatusFrequency_.clear();
        totalExecutionTime_ = std::chrono::milliseconds{0};
        mostUsedCommand_.clear();
        longestRunningCommand_.clear();
        longestExecutionTime_ = std::chrono::milliseconds{0};
    }

    auto size() const -> size_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return history_.size();
    }

    void setMaxSize(size_t maxSize) {
        std::lock_guard<std::mutex> lock(mutex_);
        maxSize_ = maxSize;

        // Trim history if necessary
        while (history_.size() > maxSize_) {
            history_.pop_front();
        }
    }

    auto getMaxSize() const -> size_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return maxSize_;
    }

private:
    std::string escapeJson(const std::string& str) const {
        std::string escaped;
        for (char c : str) {
            switch (c) {
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped += c; break;
            }
        }
        return escaped;
    }

    mutable std::mutex mutex_;
    std::list<CommandEntry> history_;
    size_t maxSize_;
    bool enablePersistence_;
    std::string persistenceFile_;

    // Statistics tracking
    std::unordered_map<std::string, size_t> commandFrequency_;
    std::unordered_map<int, size_t> exitStatusFrequency_;
    std::chrono::milliseconds totalExecutionTime_{0};
    std::string mostUsedCommand_;
    std::string longestRunningCommand_;
    std::chrono::milliseconds longestExecutionTime_{0};
};

CommandHistory::CommandHistory(size_t maxSize, bool enablePersistence,
                               const std::string& persistenceFile)
    : pImpl(std::make_unique<Impl>(maxSize, enablePersistence, persistenceFile)) {}

CommandHistory::~CommandHistory() = default;

void CommandHistory::addCommandEntry(const CommandEntry& entry) {
    pImpl->addCommandEntry(entry);
}

void CommandHistory::addCommand(const std::string &command, int exitStatus) {
    pImpl->addCommand(command, exitStatus);
}

void CommandHistory::addCommandDetailed(const std::string& command, int exitStatus,
                                       std::chrono::milliseconds executionTime,
                                       const std::string& workingDirectory,
                                       const std::string& user,
                                       size_t outputSize) {
    pImpl->addCommandDetailed(command, exitStatus, executionTime,
                             workingDirectory, user, outputSize);
}

auto CommandHistory::searchCommandsByCriteria(const HistorySearchCriteria& criteria) const
    -> std::vector<CommandEntry> {
    return pImpl->searchCommandsByCriteria(criteria);
}

auto CommandHistory::getLastCommandEntries(size_t count) const
    -> std::vector<CommandEntry> {
    return pImpl->getLastCommandEntries(count);
}

auto CommandHistory::getLastCommands(size_t count) const
    -> std::vector<std::pair<std::string, int>> {
    return pImpl->getLastCommands(count);
}

auto CommandHistory::searchCommands(const std::string &substring) const
    -> std::vector<std::pair<std::string, int>> {
    return pImpl->searchCommands(substring);
}

auto CommandHistory::getCommandFrequency(size_t topN) const
    -> std::unordered_map<std::string, size_t> {
    return pImpl->getCommandFrequency(topN);
}

auto CommandHistory::getStatistics() const -> HistoryStatistics {
    return pImpl->getStatistics();
}

auto CommandHistory::exportHistory(const std::string& filename,
                                  const std::string& format) const -> bool {
    // Implementation would depend on the format
    spdlog::info("Exporting history to {} in {} format", filename, format);
    return pImpl->saveToFile(); // Simplified for now
}

auto CommandHistory::importHistory(const std::string& filename,
                                  const std::string& format) -> bool {
    // Implementation would depend on the format
    spdlog::info("Importing history from {} in {} format", filename, format);
    return pImpl->loadFromFile(); // Simplified for now
}

auto CommandHistory::saveToFile() const -> bool {
    return pImpl->saveToFile();
}

auto CommandHistory::loadFromFile() -> bool {
    return pImpl->loadFromFile();
}

void CommandHistory::clear() {
    pImpl->clear();
}

auto CommandHistory::size() const -> size_t {
    return pImpl->size();
}

void CommandHistory::setMaxSize(size_t maxSize) {
    pImpl->setMaxSize(maxSize);
}

auto CommandHistory::getMaxSize() const -> size_t {
    return pImpl->getMaxSize();
}

auto createCommandHistoryEnhanced(size_t maxHistorySize,
                                 bool enablePersistence,
                                 const std::string& persistenceFile)
    -> std::unique_ptr<CommandHistory> {
    spdlog::debug("Creating enhanced command history with max size: {}, persistence: {}",
                  maxHistorySize, enablePersistence);
    return std::make_unique<CommandHistory>(maxHistorySize, enablePersistence, persistenceFile);
}

auto createCommandHistory(size_t maxHistorySize)
    -> std::unique_ptr<CommandHistory> {
    spdlog::debug("Creating command history with max size: {}", maxHistorySize);
    return std::make_unique<CommandHistory>(maxHistorySize);
}

auto getGlobalCommandHistory(size_t maxHistorySize, bool enablePersistence)
    -> std::shared_ptr<CommandHistory> {
    std::lock_guard<std::mutex> lock(g_globalHistoryMutex);

    if (!g_globalHistory) {
        g_globalHistory = std::make_shared<CommandHistory>(
            maxHistorySize, enablePersistence, "global_command_history.json");
        spdlog::info("Created global command history with max size: {}", maxHistorySize);
    }

    return g_globalHistory;
}

}  // namespace atom::system
