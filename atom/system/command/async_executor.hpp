/*
 * async_executor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_ASYNC_EXECUTOR_HPP
#define ATOM_SYSTEM_COMMAND_ASYNC_EXECUTOR_HPP

#include <future>
#include <string>
#include <vector>

#include "atom/macro.hpp"
#include "types.hpp"

namespace atom::system {

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

}  // namespace atom::system

#endif  // ATOM_SYSTEM_COMMAND_ASYNC_EXECUTOR_HPP
