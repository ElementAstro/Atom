/*
 * command.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-24

Description: Simple wrapper for executing commands.

**************************************************/

#ifndef ATOM_SYSTEM_COMMAND_HPP
#define ATOM_SYSTEM_COMMAND_HPP

#include <chrono>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
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
 * @brief Kill a process by its name.
 *
 * @param processName The name of the process to kill.
 * @param signal The signal to send to the process.
 */
void killProcessByName(const std::string &processName, int signal);

/**
 * @brief Kill a process by its PID.
 *
 * @param pid The PID of the process to kill.
 * @param signal The signal to send to the process.
 */
void killProcessByPID(int pid, int signal);

/**
 * @brief Execute a command with environment variables and return the command
 * output as a string.
 *
 * @param command The command to execute.
 * @param envVars The environment variables as a map of variable name to value.
 * @return The output of the command as a string.
 *
 * @note The function throws a std::runtime_error if the command fails to
 * execute.
 */
ATOM_NODISCARD auto executeCommandWithEnv(
    const std::string &command,
    const std::unordered_map<std::string, std::string> &envVars) -> std::string;

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

/**
 * @brief Start a process and return the process ID and handle.
 *
 * @param command The command to execute.
 * @return A pair containing the process ID as an integer and the process handle
 * as a void pointer.
 */
auto startProcess(const std::string &command) -> std::pair<int, void *>;

/**
 * @brief Check if a command is available in the system.
 *
 * @param command The command to check.
 * @return A boolean indicating whether the command is available.
 */
auto isCommandAvailable(const std::string &command) -> bool;

/**
 * @brief Execute a command asynchronously and return a future to the result.
 *
 * @param command The command to execute.
 * @param openTerminal Whether to open a terminal window for the command.
 * @param processLine A callback function to process each line of output.
 * @return A future to the output of the command.
 */
ATOM_NODISCARD auto executeCommandAsync(
    const std::string &command, bool openTerminal = false,
    const std::function<void(const std::string &)> &processLine = nullptr)
    -> std::future<std::string>;

/**
 * @brief Execute a command with a timeout.
 *
 * @param command The command to execute.
 * @param timeout The maximum time to wait for the command to complete.
 * @param openTerminal Whether to open a terminal window for the command.
 * @param processLine A callback function to process each line of output.
 * @return The output of the command or empty string if timed out.
 */
ATOM_NODISCARD auto executeCommandWithTimeout(
    const std::string &command, const std::chrono::milliseconds &timeout,
    bool openTerminal = false,
    const std::function<void(const std::string &)> &processLine = nullptr)
    -> std::optional<std::string>;

/**
 * @brief Execute multiple commands sequentially with a common environment.
 *
 * @param commands The list of commands to execute.
 * @param envVars The environment variables to set for all commands.
 * @param stopOnError Whether to stop execution if a command fails.
 * @return A vector of pairs containing each command's output and status.
 */
ATOM_NODISCARD auto executeCommandsWithCommonEnv(
    const std::vector<std::string> &commands,
    const std::unordered_map<std::string, std::string> &envVars,
    bool stopOnError = true) -> std::vector<std::pair<std::string, int>>;

/**
 * @brief Get a list of running processes containing the specified substring.
 *
 * @param substring The substring to search for in process names.
 * @return A vector of pairs containing PIDs and process names.
 */
ATOM_NODISCARD auto getProcessesBySubstring(const std::string &substring)
    -> std::vector<std::pair<int, std::string>>;

/**
 * @brief Execute a command and return its output as a list of lines.
 *
 * @param command The command to execute.
 * @return A vector of strings, each representing a line of output.
 */
ATOM_NODISCARD auto executeCommandGetLines(const std::string &command)
    -> std::vector<std::string>;

/**
 * @brief Pipe the output of one command to another command.
 *
 * @param firstCommand The first command to execute.
 * @param secondCommand The second command that receives the output of the
 * first.
 * @return The output of the second command.
 */
ATOM_NODISCARD auto pipeCommands(const std::string &firstCommand,
                                 const std::string &secondCommand)
    -> std::string;

/**
 * @brief Creates a command history tracker to keep track of executed commands.
 *
 * @param maxHistorySize The maximum number of commands to keep in history.
 * @return A unique pointer to the command history tracker.
 */
auto createCommandHistory(size_t maxHistorySize = 100)
    -> std::unique_ptr<class CommandHistory>;

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

}  // namespace atom::system

#endif
