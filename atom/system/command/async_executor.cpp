/*
 * async_executor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "async_executor.hpp"

#include <algorithm>
#include <future>

#include "cache.hpp"
#include "config.hpp"
#include "executor.hpp"
#include "statistics.hpp"
#include "thread_pool.hpp"

#include <spdlog/spdlog.h>

namespace atom::system {

auto executeCommandAsync(const std::string& command,
                         const ExecutionConfig& config,
                         int priority) -> std::future<ExecutionResult> {

    // Check rate limiting
    if (COMMAND_CONFIG().enableRateLimit) {
        if (!COMMAND_RATE_LIMITER().allowRequest()) {
            ExecutionResult result;
            result.exitCode = -1;
            result.error = "Rate limit exceeded";
            return std::async(std::launch::deferred, [result]() { return result; });
        }
    }

    // Convert priority
    TaskPriority taskPriority = static_cast<TaskPriority>(
        std::clamp(priority, 0, 3));

    return CommandThreadPool::getInstance().submit(
        taskPriority,
        [command, config]() -> ExecutionResult {
            return executeCommandEnhanced(command, config);
        }
    );
}

auto executeCommandCached(const std::string& command,
                         const ExecutionConfig& config) -> ExecutionResult {

    // Check cache first for validation
    auto& cacheManager = CommandCacheManager::getInstance();
    auto cachedValidation = cacheManager.getValidationResult(command);

    if (cachedValidation && !cachedValidation->isValid) {
        ExecutionResult result;
        result.exitCode = -1;
        result.error = "Command validation failed: " + cachedValidation->errorMessage;
        return result;
    }

    // Execute command
    auto result = executeCommandEnhanced(command, config);

    // Cache metrics if successful
    if (result.exitCode == 0) {
        CommandMetrics metrics;
        metrics.executionTime = result.executionTime;
        metrics.outputSize = result.output.size();
        metrics.wasSuccessful = true;
        cacheManager.cacheCommandMetrics(command, metrics);
    }

    return result;
}

auto executeCommandsBatch(const std::vector<std::string>& commands,
                         const ExecutionConfig& config,
                         size_t maxConcurrency) -> std::vector<ExecutionResult> {

    if (maxConcurrency == 0) {
        maxConcurrency = COMMAND_CONFIG().maxConcurrentCommands;
    }

    std::vector<std::future<ExecutionResult>> futures;
    std::vector<ExecutionResult> results;

    futures.reserve(commands.size());
    results.reserve(commands.size());

    // Submit all commands
    for (const auto& command : commands) {
        futures.push_back(executeCommandAsync(command, config, 1)); // Normal priority
    }

    // Collect results
    for (auto& future : futures) {
        results.push_back(future.get());
    }

    return results;
}

auto executeCommandRateLimited(const std::string& command,
                              const ExecutionConfig& config,
                              const std::string& identifier) -> ExecutionResult {

    if (COMMAND_CONFIG().enableRateLimit) {
        if (!COMMAND_RATE_LIMITER().allowRequest(identifier)) {
            ExecutionResult result;
            result.exitCode = -1;
            result.error = "Rate limit exceeded for identifier: " + identifier;
            const_cast<CommandSystemMetrics&>(COMMAND_METRICS()).blockedCommands++;
            return result;
        }
    }

    return executeCommandEnhanced(command, config);
}

}  // namespace atom::system
