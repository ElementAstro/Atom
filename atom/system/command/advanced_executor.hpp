/*
 * advanced_executor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_ADVANCED_EXECUTOR_HPP
#define ATOM_SYSTEM_COMMAND_ADVANCED_EXECUTOR_HPP

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

}  // namespace atom::system

#endif
