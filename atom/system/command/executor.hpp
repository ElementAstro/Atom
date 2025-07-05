/*
 * executor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_EXECUTOR_HPP
#define ATOM_SYSTEM_COMMAND_EXECUTOR_HPP

#include <functional>
#include <string>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Execute a command and return the command output as a string.
 *
 * @param command The command to execute.
 * @param openTerminal Whether to open a terminal window for the command.
 * @param processLine A callback function to process each line of output.
 * @return The output of the command as a string.
 *
 * @note The function throws a std::runtime_error if the command fails to
 * execute.
 */
ATOM_NODISCARD auto executeCommand(
    const std::string &command, bool openTerminal = false,
    const std::function<void(const std::string &)> &processLine =
        [](const std::string &) {}) -> std::string;

/**
 * @brief Execute a command with input and return the command output as a
 * string.
 *
 * @param command The command to execute.
 * @param input The input to provide to the command.
 * @param processLine A callback function to process each line of output.
 * @return The output of the command as a string.
 *
 * @note The function throws a std::runtime_error if the command fails to
 * execute.
 */
ATOM_NODISCARD auto executeCommandWithInput(
    const std::string &command, const std::string &input,
    const std::function<void(const std::string &)> &processLine = nullptr)
    -> std::string;

/**
 * @brief Execute a command and return the command output as a string.
 *
 * @param command The command to execute.
 * @param openTerminal Whether to open a terminal window for the command.
 * @param processLine A callback function to process each line of output.
 * @param status The exit status of the command.
 * @param terminateCondition A callback function to determine whether to
 * terminate the command execution.
 * @return The output of the command as a string.
 *
 * @note The function throws a std::runtime_error if the command fails to
 * execute.
 */
auto executeCommandStream(
    const std::string &command, bool openTerminal,
    const std::function<void(const std::string &)> &processLine, int &status,
    const std::function<bool()> &terminateCondition = [] { return false; })
    -> std::string;

/**
 * @brief Execute a list of commands.
 *
 * @param commands The list of commands to execute.
 *
 * @note The function throws a std::runtime_error if any of the commands fail to
 * execute.
 */
void executeCommands(const std::vector<std::string> &commands);

/**
 * @brief Execute a command and return the command output along with the exit
 * status.
 *
 * @param command The command to execute.
 * @return A pair containing the output of the command as a string and the exit
 * status as an integer.
 *
 * @note The function throws a std::runtime_error if the command fails to
 * execute.
 */
ATOM_NODISCARD auto executeCommandWithStatus(const std::string &command)
    -> std::pair<std::string, int>;

/**
 * @brief Execute a command and return a boolean indicating whether the command
 * was successful.
 *
 * @param command The command to execute.
 * @return A boolean indicating whether the command was successful.
 *
 * @note The function throws a std::runtime_error if the command fails to
 * execute.
 */
ATOM_NODISCARD auto executeCommandSimple(const std::string &command) -> bool;

// Internal implementation function (used by other modules)
auto executeCommandInternal(
    const std::string &command, bool openTerminal,
    const std::function<void(const std::string &)> &processLine, int &status,
    const std::string &input = "", const std::string &username = "",
    const std::string &domain = "", const std::string &password = "")
    -> std::string;

}  // namespace atom::system

#endif
