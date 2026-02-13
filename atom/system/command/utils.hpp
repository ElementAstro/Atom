/*
 * utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_UTILS_HPP
#define ATOM_SYSTEM_COMMAND_UTILS_HPP

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/macro.hpp"
#include "types.hpp"

namespace atom::system {

/**
 * @brief Parse command line arguments from a command string
 *
 * @param command The command string to parse
 * @return Vector of parsed arguments
 */
ATOM_NODISCARD auto parseCommandArguments(const std::string &command)
    -> std::vector<std::string>;

/**
 * @brief Build a command string from arguments with proper escaping
 *
 * @param args Vector of command arguments
 * @return Properly escaped command string
 */
ATOM_NODISCARD auto buildCommandString(const std::vector<std::string> &args)
    -> std::string;

/**
 * @brief Get command performance metrics
 *
 * @param command The command to analyze
 * @return CommandMetrics with performance information
 */
ATOM_NODISCARD auto getCommandMetrics(const std::string &command)
    -> CommandMetrics;

/**
 * @brief Check if a command is available in the system.
 *
 * @param command The command to check.
 * @return A boolean indicating whether the command is available.
 */
auto isCommandAvailable(const std::string &command) -> bool;

/**
 * @brief Execute a command and return its output as a list of lines with enhanced options.
 *
 * @param command The command to execute.
 * @param trimWhitespace Whether to trim whitespace from each line.
 * @param skipEmptyLines Whether to skip empty lines.
 * @param maxLines Maximum number of lines to return (0 = no limit).
 * @return A vector of strings, each representing a line of output.
 */
ATOM_NODISCARD auto executeCommandGetLinesEnhanced(
    const std::string &command,
    bool trimWhitespace = true,
    bool skipEmptyLines = true,
    size_t maxLines = 0) -> std::vector<std::string>;

/**
 * @brief Execute a command and return its output as a list of lines.
 *
 * @param command The command to execute.
 * @return A vector of strings, each representing a line of output.
 */
ATOM_NODISCARD auto executeCommandGetLines(const std::string &command)
    -> std::vector<std::string>;

/**
 * @brief Pipe multiple commands together with enhanced configuration.
 *
 * @param commands Vector of commands to pipe together.
 * @param config Pipe configuration options.
 * @return ExecutionResult containing the final output and metrics.
 */
ATOM_NODISCARD auto pipeCommandsEnhanced(
    const std::vector<std::string> &commands,
    const PipeConfig &config = {}) -> ExecutionResult;

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
 * @brief Find the full path of a command in the system PATH
 *
 * @param command The command name to find
 * @return Full path to the command, or empty string if not found
 */
ATOM_NODISCARD auto findCommandPath(const std::string &command) -> std::string;

/**
 * @brief Get system command aliases and their expansions
 *
 * @return Map of alias names to their command expansions
 */
ATOM_NODISCARD auto getSystemAliases() -> std::unordered_map<std::string, std::string>;

/**
 * @brief Expand command aliases in a command string
 *
 * @param command The command string that may contain aliases
 * @return Command string with aliases expanded
 */
ATOM_NODISCARD auto expandCommandAliases(const std::string &command) -> std::string;

/**
 * @brief Get command completion suggestions
 *
 * @param partialCommand Partial command to complete
 * @param maxSuggestions Maximum number of suggestions to return
 * @return Vector of completion suggestions
 */
ATOM_NODISCARD auto getCommandCompletions(
    const std::string &partialCommand,
    size_t maxSuggestions = 10) -> std::vector<std::string>;

}  // namespace atom::system

#endif
