/*
 * utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_UTILS_HPP
#define ATOM_SYSTEM_COMMAND_UTILS_HPP

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/macro.hpp"
#include "executor.hpp"

namespace atom::system {

/**
 * @brief Command validation result
 */
struct ValidationResult {
    bool isValid = false;
    std::string errorMessage;
    std::vector<std::string> warnings;
    double securityScore = 0.0; // 0.0 = very dangerous, 1.0 = safe
};

/**
 * @brief Command performance metrics
 */
struct CommandMetrics {
    std::chrono::milliseconds executionTime{0};
    size_t outputSize = 0;
    size_t memoryUsage = 0;
    double cpuUsage = 0.0;
    bool wasSuccessful = false;
};

/**
 * @brief Pipe configuration for command chaining
 */
struct PipeConfig {
    bool captureStderr = true;
    bool validateCommands = true;
    std::chrono::milliseconds timeout{30000}; // 30 seconds default
    size_t maxOutputSize = 1024 * 1024; // 1MB default
};

/**
 * @brief Validate a command with detailed security analysis
 *
 * @param command The command to validate
 * @return ValidationResult with detailed validation information
 */
ATOM_NODISCARD auto validateCommandDetailed(const std::string &command)
    -> ValidationResult;

/**
 * @brief Sanitize a command by removing or escaping dangerous elements
 *
 * @param command The command to sanitize
 * @return Sanitized command string
 */
ATOM_NODISCARD auto sanitizeCommand(const std::string &command) -> std::string;

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

/**
 * @brief Check if a command requires elevated privileges
 *
 * @param command The command to check
 * @return true if the command typically requires elevated privileges
 */
ATOM_NODISCARD auto requiresElevatedPrivileges(const std::string &command) -> bool;

}  // namespace atom::system

#endif
