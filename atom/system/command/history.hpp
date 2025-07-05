/*
 * history.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_HISTORY_HPP
#define ATOM_SYSTEM_COMMAND_HISTORY_HPP

#include <memory>
#include <string>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Command history class to track executed commands.
 */
class CommandHistory {
public:
    /**
     * @brief Construct a new Command History object.
     *
     * @param maxSize The maximum number of commands to keep in history.
     */
    CommandHistory(size_t maxSize);

    /**
     * @brief Destroy the Command History object.
     */
    ~CommandHistory();

    /**
     * @brief Add a command to the history.
     *
     * @param command The command to add.
     * @param exitStatus The exit status of the command.
     */
    void addCommand(const std::string &command, int exitStatus);

    /**
     * @brief Get the last commands from history.
     *
     * @param count The number of commands to retrieve.
     * @return A vector of pairs containing commands and their exit status.
     */
    ATOM_NODISCARD auto getLastCommands(size_t count) const
        -> std::vector<std::pair<std::string, int>>;

    /**
     * @brief Search commands in history by substring.
     *
     * @param substring The substring to search for.
     * @return A vector of pairs containing matching commands and their exit
     * status.
     */
    ATOM_NODISCARD auto searchCommands(const std::string &substring) const
        -> std::vector<std::pair<std::string, int>>;

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

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Creates a command history tracker to keep track of executed commands.
 *
 * @param maxHistorySize The maximum number of commands to keep in history.
 * @return A unique pointer to the command history tracker.
 */
auto createCommandHistory(size_t maxHistorySize = 100)
    -> std::unique_ptr<CommandHistory>;

}  // namespace atom::system

#endif
