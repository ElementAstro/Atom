/*
 * history.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_HISTORY_HPP
#define ATOM_SYSTEM_COMMAND_HISTORY_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Enhanced command entry with detailed information
 */
struct CommandEntry {
    std::string command;
    int exitStatus = 0;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::milliseconds executionTime{0};
    std::string workingDirectory;
    std::string user;
    size_t outputSize = 0;
    std::unordered_map<std::string, std::string> metadata;

    // Comparison operators for sorting
    bool operator<(const CommandEntry& other) const {
        return timestamp < other.timestamp;
    }

    bool operator==(const CommandEntry& other) const {
        return command == other.command && timestamp == other.timestamp;
    }
};

/**
 * @brief History search criteria
 */
struct HistorySearchCriteria {
    std::string commandPattern;
    std::string userFilter;
    std::string workingDirectoryFilter;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::vector<int> exitStatusFilter;
    bool useRegex = false;
    bool caseSensitive = false;
    size_t maxResults = 100;
};

/**
 * @brief History statistics
 */
struct HistoryStatistics {
    size_t totalCommands = 0;
    size_t successfulCommands = 0;
    size_t failedCommands = 0;
    std::chrono::milliseconds totalExecutionTime{0};
    std::chrono::milliseconds averageExecutionTime{0};
    std::unordered_map<std::string, size_t> commandFrequency;
    std::unordered_map<int, size_t> exitStatusFrequency;
    std::string mostUsedCommand;
    std::string longestRunningCommand;
};

/**
 * @brief Enhanced command history class with advanced features.
 */
class CommandHistory {
public:
    /**
     * @brief Construct a new Command History object.
     *
     * @param maxSize The maximum number of commands to keep in history.
     * @param enablePersistence Whether to enable persistent storage.
     * @param persistenceFile File path for persistent storage.
     */
    CommandHistory(size_t maxSize, bool enablePersistence = false,
                   const std::string& persistenceFile = "");

    /**
     * @brief Destroy the Command History object.
     */
    ~CommandHistory();

    /**
     * @brief Add a command entry to the history with detailed information.
     *
     * @param entry The command entry to add.
     */
    void addCommandEntry(const CommandEntry& entry);

    /**
     * @brief Add a command to the history (legacy interface).
     *
     * @param command The command to add.
     * @param exitStatus The exit status of the command.
     */
    void addCommand(const std::string &command, int exitStatus);

    /**
     * @brief Add a command with additional metadata.
     *
     * @param command The command to add.
     * @param exitStatus The exit status of the command.
     * @param executionTime Time taken to execute the command.
     * @param workingDirectory Working directory when command was executed.
     * @param user User who executed the command.
     * @param outputSize Size of command output in bytes.
     */
    void addCommandDetailed(const std::string& command, int exitStatus,
                           std::chrono::milliseconds executionTime,
                           const std::string& workingDirectory = "",
                           const std::string& user = "",
                           size_t outputSize = 0);

    /**
     * @brief Search commands with advanced criteria.
     *
     * @param criteria Search criteria for filtering commands.
     * @return Vector of matching command entries.
     */
    ATOM_NODISCARD auto searchCommandsAdvanced(const HistorySearchCriteria& criteria) const
        -> std::vector<CommandEntry>;

    /**
     * @brief Get the last command entries from history.
     *
     * @param count The number of commands to retrieve.
     * @return A vector of command entries.
     */
    ATOM_NODISCARD auto getLastCommandEntries(size_t count) const
        -> std::vector<CommandEntry>;

    /**
     * @brief Get the last commands from history (legacy interface).
     *
     * @param count The number of commands to retrieve.
     * @return A vector of pairs containing commands and their exit status.
     */
    ATOM_NODISCARD auto getLastCommands(size_t count) const
        -> std::vector<std::pair<std::string, int>>;

    /**
     * @brief Search commands in history by substring (legacy interface).
     *
     * @param substring The substring to search for.
     * @return A vector of pairs containing matching commands and their exit
     * status.
     */
    ATOM_NODISCARD auto searchCommands(const std::string &substring) const
        -> std::vector<std::pair<std::string, int>>;

    /**
     * @brief Get command frequency statistics.
     *
     * @param topN Number of top commands to include.
     * @return Map of commands to their frequency count.
     */
    ATOM_NODISCARD auto getCommandFrequency(size_t topN = 10) const
        -> std::unordered_map<std::string, size_t>;

    /**
     * @brief Get comprehensive history statistics.
     *
     * @return HistoryStatistics structure with detailed metrics.
     */
    ATOM_NODISCARD auto getStatistics() const -> HistoryStatistics;

    /**
     * @brief Export history to a file.
     *
     * @param filename File path to export to.
     * @param format Export format ("json", "csv", "txt").
     * @return true if export was successful.
     */
    auto exportHistory(const std::string& filename,
                      const std::string& format = "json") const -> bool;

    /**
     * @brief Import history from a file.
     *
     * @param filename File path to import from.
     * @param format Import format ("json", "csv", "txt").
     * @return true if import was successful.
     */
    auto importHistory(const std::string& filename,
                      const std::string& format = "json") -> bool;

    /**
     * @brief Save history to persistent storage.
     *
     * @return true if save was successful.
     */
    auto saveToFile() const -> bool;

    /**
     * @brief Load history from persistent storage.
     *
     * @return true if load was successful.
     */
    auto loadFromFile() -> bool;

    /**
     * @brief Clear all commands from history.
     */
    void clear();

    /**
     * @brief Get the number of commands in history.
     *
     * @return The size of the command history.
     */
    ATOM_NODISCARD auto size() const -> size_t;

    /**
     * @brief Set the maximum size of the history.
     *
     * @param maxSize New maximum size.
     */
    void setMaxSize(size_t maxSize);

    /**
     * @brief Get the maximum size of the history.
     *
     * @return Maximum size of the history.
     */
    ATOM_NODISCARD auto getMaxSize() const -> size_t;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Creates an enhanced command history tracker with persistence support.
 *
 * @param maxHistorySize The maximum number of commands to keep in history.
 * @param enablePersistence Whether to enable persistent storage.
 * @param persistenceFile File path for persistent storage.
 * @return A unique pointer to the command history tracker.
 */
auto createCommandHistoryEnhanced(size_t maxHistorySize = 100,
                                 bool enablePersistence = false,
                                 const std::string& persistenceFile = "")
    -> std::unique_ptr<CommandHistory>;

/**
 * @brief Creates a command history tracker to keep track of executed commands.
 *
 * @param maxHistorySize The maximum number of commands to keep in history.
 * @return A unique pointer to the command history tracker.
 */
auto createCommandHistory(size_t maxHistorySize = 100)
    -> std::unique_ptr<CommandHistory>;

/**
 * @brief Create a shared command history instance for global use.
 *
 * @param maxHistorySize The maximum number of commands to keep in history.
 * @param enablePersistence Whether to enable persistent storage.
 * @return A shared pointer to the command history tracker.
 */
auto getGlobalCommandHistory(size_t maxHistorySize = 1000,
                            bool enablePersistence = true)
    -> std::shared_ptr<CommandHistory>;

}  // namespace atom::system

#endif
