/*
 * executor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_EXECUTOR_HPP
#define ATOM_SYSTEM_COMMAND_EXECUTOR_HPP

#include <chrono>
#include <functional>
#include <future>
#include <string>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system {

// Forward declarations
enum class TaskPriority;
struct CommandSystemMetrics;

/**
 * @brief Configuration for command execution
 */
struct ExecutionConfig {
    bool openTerminal = false;
    bool validateCommand = true;
    bool enableLogging = true;
    size_t bufferSize = 8192;  // Increased default buffer size
    std::chrono::milliseconds timeout = std::chrono::milliseconds::zero();
    size_t maxOutputSize = 1024 * 1024;  // 1MB default limit
    bool streamOutput = false;
    bool captureStderr = true;
};

/**
 * @brief Result of command execution
 */
struct ExecutionResult {
    std::string output;
    std::string error;
    int exitCode = 0;
    std::chrono::milliseconds executionTime{0};
    bool timedOut = false;
    bool wasKilled = false;
};

/**
 * @brief Validate a command for security and safety
 *
 * @param command The command to validate
 * @return true if command is safe to execute
 */
ATOM_NODISCARD auto validateCommand(const std::string &command) -> bool;

/**
 * @brief Execute a command with enhanced configuration and return detailed result
 *
 * @param command The command to execute
 * @param config Execution configuration
 * @param processLine Optional callback function to process each line of output
 * @return ExecutionResult containing output, error, status, and timing info
 */
ATOM_NODISCARD auto executeCommandEnhanced(
    const std::string &command,
    const ExecutionConfig &config = {},
    const std::function<void(const std::string &)> &processLine = nullptr)
    -> ExecutionResult;

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
 * @brief Execute a list of commands with enhanced configuration.
 *
 * @param commands The list of commands to execute.
 * @param config Execution configuration applied to all commands.
 * @param parallel Whether to execute commands in parallel.
 * @param stopOnError Whether to stop execution if a command fails.
 * @return Vector of ExecutionResult for each command.
 */
ATOM_NODISCARD auto executeCommandsEnhanced(
    const std::vector<std::string> &commands,
    const ExecutionConfig &config = {},
    bool parallel = false,
    bool stopOnError = true) -> std::vector<ExecutionResult>;

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

/**
 * @brief Get execution statistics and performance metrics
 *
 * @return String containing formatted statistics
 */
ATOM_NODISCARD auto getExecutionStatistics() -> std::string;

/**
 * @brief Clear execution statistics
 */
void clearExecutionStatistics();

// Internal implementation function (used by other modules)
auto executeCommandInternal(
    const std::string &command, bool openTerminal,
    const std::function<void(const std::string &)> &processLine, int &status,
    const std::string &input = "", const std::string &username = "",
    const std::string &domain = "", const std::string &password = "")
    -> std::string;

// Enhanced internal implementation with configuration
auto executeCommandInternalEnhanced(
    const std::string &command,
    const ExecutionConfig &config,
    const std::function<void(const std::string &)> &processLine,
    const std::string &input = "",
    const std::string &username = "",
    const std::string &domain = "",
    const std::string &password = "")
    -> ExecutionResult;

// ============================================================================
// Optimized Execution Functions
// ============================================================================

/**
 * @brief Execute command asynchronously with optimizations
 * @param command The command to execute
 * @param config Execution configuration
 * @param priority Task priority (0=low, 1=normal, 2=high, 3=critical)
 * @return Future containing the execution result
 */
ATOM_NODISCARD auto executeCommandAsync(
    const std::string& command,
    const ExecutionConfig& config = {},
    int priority = 1) -> std::future<ExecutionResult>;

/**
 * @brief Execute command with intelligent caching
 * @param command The command to execute
 * @param config Execution configuration
 * @return Execution result (may be cached)
 */
ATOM_NODISCARD auto executeCommandCached(
    const std::string& command,
    const ExecutionConfig& config = {}) -> ExecutionResult;

/**
 * @brief Execute multiple commands with optimized thread pool
 * @param commands Vector of commands to execute
 * @param config Execution configuration
 * @param maxConcurrency Maximum concurrent executions
 * @return Vector of execution results
 */
ATOM_NODISCARD auto executeCommandsBatch(
    const std::vector<std::string>& commands,
    const ExecutionConfig& config = {},
    size_t maxConcurrency = 0) -> std::vector<ExecutionResult>;

/**
 * @brief Execute command with rate limiting
 * @param command The command to execute
 * @param config Execution configuration
 * @param identifier Rate limit identifier
 * @return Execution result or error if rate limited
 */
ATOM_NODISCARD auto executeCommandRateLimited(
    const std::string& command,
    const ExecutionConfig& config = {},
    const std::string& identifier = "") -> ExecutionResult;

/**
 * @brief Get execution metrics
 * @return Current execution metrics
 */
ATOM_NODISCARD auto getExecutionMetrics() -> const CommandSystemMetrics&;

/**
 * @brief Reset execution statistics
 */
void resetExecutionStatistics();

}  // namespace atom::system

#endif
