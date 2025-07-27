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
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/macro.hpp"
#include "executor.hpp"

namespace atom::system {

/**
 * @brief Cancellation token for async operations
 */
class CancellationToken {
public:
    CancellationToken() = default;

    void cancel();
    auto isCancelled() const -> bool;
    void reset();

private:
    std::atomic<bool> cancelled_{false};
};

/**
 * @brief Resource pool for managing execution resources
 */
class ExecutionResourcePool {
public:
    explicit ExecutionResourcePool(size_t maxConcurrentExecutions = 10);
    ~ExecutionResourcePool();

    auto acquireResource() -> std::shared_ptr<void>;
    void releaseResource(std::shared_ptr<void> resource);
    auto getAvailableResources() const -> size_t;
    auto getTotalResources() const -> size_t;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/**
 * @brief Advanced execution configuration
 */
struct AdvancedExecutionConfig {
    ExecutionConfig baseConfig;
    std::shared_ptr<CancellationToken> cancellationToken;
    std::shared_ptr<ExecutionResourcePool> resourcePool;
    bool retryOnFailure = false;
    size_t maxRetries = 3;
    std::chrono::milliseconds retryDelay{1000};
    std::function<bool(const ExecutionResult&)> shouldRetry;
};

/**
 * @brief Execute a command with advanced configuration and cancellation support
 *
 * @param command The command to execute
 * @param config Advanced execution configuration
 * @param processLine Optional callback function to process each line of output
 * @return ExecutionResult containing output, error, status, and timing info
 */
ATOM_NODISCARD auto executeCommandAdvanced(
    const std::string &command,
    const AdvancedExecutionConfig &config,
    const std::function<void(const std::string &)> &processLine = nullptr)
    -> ExecutionResult;

/**
 * @brief Execute multiple commands with advanced configuration
 *
 * @param commands Vector of commands to execute
 * @param config Advanced execution configuration
 * @param parallel Whether to execute commands in parallel
 * @param stopOnError Whether to stop execution if a command fails
 * @return Vector of ExecutionResult for each command
 */
ATOM_NODISCARD auto executeCommandsAdvanced(
    const std::vector<std::string> &commands,
    const AdvancedExecutionConfig &config,
    bool parallel = false,
    bool stopOnError = true) -> std::vector<ExecutionResult>;

/**
 * @brief Create a shared resource pool for execution management
 *
 * @param maxConcurrentExecutions Maximum number of concurrent executions
 * @return Shared pointer to ExecutionResourcePool
 */
ATOM_NODISCARD auto createExecutionResourcePool(size_t maxConcurrentExecutions = 10)
    -> std::shared_ptr<ExecutionResourcePool>;

/**
 * @brief Create a cancellation token
 *
 * @return Shared pointer to CancellationToken
 */
ATOM_NODISCARD auto createCancellationToken() -> std::shared_ptr<CancellationToken>;

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
 * @brief Execute a command asynchronously with advanced configuration
 *
 * @param command The command to execute
 * @param config Advanced execution configuration
 * @param processLine Optional callback function to process each line of output
 * @return Future to ExecutionResult
 */
ATOM_NODISCARD auto executeCommandAsyncAdvanced(
    const std::string &command,
    const AdvancedExecutionConfig &config,
    const std::function<void(const std::string &)> &processLine = nullptr)
    -> std::future<ExecutionResult>;

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
 * @brief Execute a command with enhanced timeout and cancellation support
 *
 * @param command The command to execute
 * @param timeout The maximum time to wait for the command to complete
 * @param cancellationToken Optional cancellation token
 * @param config Optional execution configuration
 * @param processLine Optional callback function to process each line of output
 * @return ExecutionResult or nullopt if timed out/cancelled
 */
ATOM_NODISCARD auto executeCommandWithTimeoutAdvanced(
    const std::string &command,
    const std::chrono::milliseconds &timeout,
    std::shared_ptr<CancellationToken> cancellationToken = nullptr,
    const ExecutionConfig &config = {},
    const std::function<void(const std::string &)> &processLine = nullptr)
    -> std::optional<ExecutionResult>;

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
