/*
 * advanced_executor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "advanced_executor.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "executor.hpp"

#include "atom/meta/global_ptr.hpp"
#include "../env.hpp"

#include <spdlog/spdlog.h>

namespace atom::system {

// Global mutex for environment operations (declared in command.cpp)
extern std::mutex envMutex;

// CancellationToken implementation
void CancellationToken::cancel() {
    cancelled_.store(true);
    spdlog::debug("Cancellation token cancelled");
}

auto CancellationToken::isCancelled() const -> bool {
    return cancelled_.load();
}

void CancellationToken::reset() {
    cancelled_.store(false);
    spdlog::debug("Cancellation token reset");
}

// ExecutionResourcePool implementation
class ExecutionResourcePool::Impl {
public:
    explicit Impl(size_t maxResources) : maxResources_(maxResources) {
        for (size_t i = 0; i < maxResources; ++i) {
            availableResources_.push(std::make_shared<int>(static_cast<int>(i)));
        }
    }

    auto acquireResource() -> std::shared_ptr<void> {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !availableResources_.empty(); });

        auto resource = availableResources_.front();
        availableResources_.pop();
        return resource;
    }

    void releaseResource(std::shared_ptr<void> resource) {
        std::lock_guard<std::mutex> lock(mutex_);
        availableResources_.push(std::static_pointer_cast<int>(resource));
        cv_.notify_one();
    }

    auto getAvailableResources() const -> size_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return availableResources_.size();
    }

    auto getTotalResources() const -> size_t {
        return maxResources_;
    }

private:
    size_t maxResources_;
    std::queue<std::shared_ptr<int>> availableResources_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

ExecutionResourcePool::ExecutionResourcePool(size_t maxConcurrentExecutions)
    : pImpl_(std::make_unique<Impl>(maxConcurrentExecutions)) {
    spdlog::debug("Created execution resource pool with {} resources", maxConcurrentExecutions);
}

ExecutionResourcePool::~ExecutionResourcePool() = default;

auto ExecutionResourcePool::acquireResource() -> std::shared_ptr<void> {
    return pImpl_->acquireResource();
}

void ExecutionResourcePool::releaseResource(std::shared_ptr<void> resource) {
    pImpl_->releaseResource(resource);
}

auto ExecutionResourcePool::getAvailableResources() const -> size_t {
    return pImpl_->getAvailableResources();
}

auto ExecutionResourcePool::getTotalResources() const -> size_t {
    return pImpl_->getTotalResources();
}

auto executeCommandAdvanced(
    const std::string &command,
    const AdvancedExecutionConfig &config,
    const std::function<void(const std::string &)> &processLine)
    -> ExecutionResult {

    spdlog::debug("Executing advanced command: {}", command);

    // Check cancellation before starting
    if (config.cancellationToken && config.cancellationToken->isCancelled()) {
        ExecutionResult result;
        result.exitCode = -1;
        result.error = "Operation was cancelled before execution";
        result.wasKilled = true;
        return result;
    }

    // Acquire resource if pool is provided
    std::shared_ptr<void> resource;
    if (config.resourcePool) {
        resource = config.resourcePool->acquireResource();
        spdlog::debug("Acquired execution resource");
    }

    // RAII resource management
    auto resourceGuard = [&config, resource]() {
        if (config.resourcePool && resource) {
            config.resourcePool->releaseResource(resource);
            spdlog::debug("Released execution resource");
        }
    };

    ExecutionResult result;
    size_t attempts = 0;
    const size_t maxAttempts = config.retryOnFailure ? config.maxRetries + 1 : 1;

    while (attempts < maxAttempts) {
        // Check cancellation before each attempt
        if (config.cancellationToken && config.cancellationToken->isCancelled()) {
            result.exitCode = -1;
            result.error = "Operation was cancelled during execution";
            result.wasKilled = true;
            break;
        }

        attempts++;
        spdlog::debug("Executing command attempt {} of {}", attempts, maxAttempts);

        // Execute with enhanced configuration
        result = executeCommandInternalEnhanced(command, config.baseConfig, processLine);

        // Check if we should retry
        if (attempts < maxAttempts &&
            ((config.shouldRetry && config.shouldRetry(result)) ||
             (!config.shouldRetry && result.exitCode != 0))) {

            spdlog::warn("Command failed (exit code: {}), retrying in {}ms",
                        result.exitCode, config.retryDelay.count());
            std::this_thread::sleep_for(config.retryDelay);
            continue;
        }

        break;
    }

    resourceGuard();

    if (attempts > 1) {
        spdlog::info("Command completed after {} attempts", attempts);
    }

    return result;
}

auto executeCommandsAdvanced(
    const std::vector<std::string> &commands,
    const AdvancedExecutionConfig &config,
    bool parallel,
    bool stopOnError) -> std::vector<ExecutionResult> {

    spdlog::debug("Executing {} advanced commands, parallel: {}", commands.size(), parallel);

    std::vector<ExecutionResult> results;
    results.reserve(commands.size());

    if (parallel) {
        // Parallel execution with resource management
        std::vector<std::future<ExecutionResult>> futures;
        futures.reserve(commands.size());

        for (const auto &command : commands) {
            futures.emplace_back(std::async(std::launch::async, [&command, &config]() {
                return executeCommandAdvanced(command, config, nullptr);
            }));
        }

        for (auto &future : futures) {
            auto result = future.get();
            results.push_back(result);

            if (stopOnError && result.exitCode != 0) {
                spdlog::warn("Command failed with exit code {}. Stopping parallel execution",
                           result.exitCode);

                // Cancel remaining operations if cancellation token is available
                if (config.cancellationToken) {
                    config.cancellationToken->cancel();
                }
                break;
            }
        }
    } else {
        // Sequential execution
        for (const auto &command : commands) {
            auto result = executeCommandAdvanced(command, config, nullptr);
            results.push_back(result);

            if (stopOnError && result.exitCode != 0) {
                spdlog::warn("Command '{}' failed with exit code {}. Stopping sequence",
                           command, result.exitCode);
                break;
            }
        }
    }

    spdlog::debug("Advanced commands completed with {} results", results.size());
    return results;
}

auto executeCommandAsyncAdvanced(
    const std::string &command,
    const AdvancedExecutionConfig &config,
    const std::function<void(const std::string &)> &processLine)
    -> std::future<ExecutionResult> {

    spdlog::debug("Executing async advanced command: {}", command);

    return std::async(std::launch::async, [command, config, processLine]() {
        return executeCommandAdvanced(command, config, processLine);
    });
}

auto executeCommandWithTimeoutAdvanced(
    const std::string &command,
    const std::chrono::milliseconds &timeout,
    std::shared_ptr<CancellationToken> cancellationToken,
    const ExecutionConfig &config,
    const std::function<void(const std::string &)> &processLine)
    -> std::optional<ExecutionResult> {

    spdlog::debug("Executing command with advanced timeout: {}, timeout: {}ms",
                  command, timeout.count());

    // Create a local cancellation token if none provided
    auto localToken = cancellationToken ? cancellationToken : std::make_shared<CancellationToken>();

    AdvancedExecutionConfig advancedConfig;
    advancedConfig.baseConfig = config;
    advancedConfig.baseConfig.timeout = timeout;
    advancedConfig.cancellationToken = localToken;

    auto future = executeCommandAsyncAdvanced(command, advancedConfig, processLine);
    auto status = future.wait_for(timeout);

    if (status == std::future_status::timeout) {
        spdlog::warn("Command '{}' timed out after {}ms", command, timeout.count());
        localToken->cancel();

        // Try to get the result with a short wait to see if cancellation worked
        if (future.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready) {
            auto result = future.get();
            result.timedOut = true;
            return result;
        }

        return std::nullopt;
    }

    try {
        auto result = future.get();
        spdlog::debug("Command with advanced timeout completed successfully");
        return result;
    } catch (const std::exception &e) {
        spdlog::error("Command with advanced timeout failed: {}", e.what());
        return std::nullopt;
    }
}

auto createExecutionResourcePool(size_t maxConcurrentExecutions)
    -> std::shared_ptr<ExecutionResourcePool> {
    return std::make_shared<ExecutionResourcePool>(maxConcurrentExecutions);
}

auto createCancellationToken() -> std::shared_ptr<CancellationToken> {
    return std::make_shared<CancellationToken>();
}

auto executeCommandWithEnv(
    const std::string &command,
    const std::unordered_map<std::string, std::string> &envVars)
    -> std::string {
    spdlog::debug("Executing command with environment: {}", command);
    if (command.empty()) {
        spdlog::warn("Command is empty");
        return "";
    }

    std::unordered_map<std::string, std::string> oldEnvVars;
    std::shared_ptr<utils::Env> env;
    GET_OR_CREATE_PTR(env, utils::Env, "LITHIUM.ENV");
    {
        std::lock_guard lock(envMutex);
        for (const auto &var : envVars) {
            auto oldValue = env->getEnv(var.first);
            if (!oldValue.empty()) {
                oldEnvVars[var.first] = oldValue;
            }
            env->setEnv(var.first, var.second);
        }
    }

    auto result = executeCommand(command, false, nullptr);

    {
        std::lock_guard lock(envMutex);
        for (const auto &var : envVars) {
            if (oldEnvVars.find(var.first) != oldEnvVars.end()) {
                env->setEnv(var.first, oldEnvVars[var.first]);
            } else {
                env->unsetEnv(var.first);
            }
        }
    }

    spdlog::debug("Command with environment completed");
    return result;
}

auto executeCommandAsync(
    const std::string &command, bool openTerminal,
    const std::function<void(const std::string &)> &processLine)
    -> std::future<std::string> {
    spdlog::debug("Executing async command: {}, openTerminal: {}", command,
                  openTerminal);

    return std::async(
        std::launch::async, [command, openTerminal, processLine]() {
            int status = 0;
            auto result = executeCommandInternal(command, openTerminal,
                                                 processLine, status);
            spdlog::debug("Async command '{}' completed with status: {}",
                          command, status);
            return result;
        });
}

auto executeCommandWithTimeout(
    const std::string &command, const std::chrono::milliseconds &timeout,
    bool openTerminal,
    const std::function<void(const std::string &)> &processLine)
    -> std::optional<std::string> {
    spdlog::debug("Executing command with timeout: {}, timeout: {}ms", command,
                  timeout.count());

    auto future = executeCommandAsync(command, openTerminal, processLine);
    auto status = future.wait_for(timeout);

    if (status == std::future_status::timeout) {
        spdlog::warn("Command '{}' timed out after {}ms", command,
                     timeout.count());

#ifdef _WIN32
        std::string killCmd =
            "taskkill /F /IM " + command.substr(0, command.find(' ')) + ".exe";
#else
        std::string killCmd = "pkill -f \"" + command + "\"";
#endif
        auto result = executeCommandSimple(killCmd);
        if (!result) {
            spdlog::error("Failed to kill process for command '{}'", command);
        } else {
            spdlog::info("Process for command '{}' killed successfully",
                         command);
        }
        return std::nullopt;
    }

    try {
        auto result = future.get();
        spdlog::debug("Command with timeout completed successfully");
        return result;
    } catch (const std::exception &e) {
        spdlog::error("Command with timeout failed: {}", e.what());
        return std::nullopt;
    }
}

auto executeCommandsWithCommonEnv(
    const std::vector<std::string> &commands,
    const std::unordered_map<std::string, std::string> &envVars,
    bool stopOnError) -> std::vector<std::pair<std::string, int>> {
    spdlog::debug("Executing {} commands with common environment",
                  commands.size());

    std::vector<std::pair<std::string, int>> results;
    results.reserve(commands.size());

    std::unordered_map<std::string, std::string> oldEnvVars;
    std::shared_ptr<utils::Env> env;
    GET_OR_CREATE_PTR(env, utils::Env, "LITHIUM.ENV");

    {
        std::lock_guard lock(envMutex);
        for (const auto &var : envVars) {
            auto oldValue = env->getEnv(var.first);
            if (!oldValue.empty()) {
                oldEnvVars[var.first] = oldValue;
            }
            env->setEnv(var.first, var.second);
        }
    }

    for (const auto &command : commands) {
        auto [output, status] = executeCommandWithStatus(command);
        results.emplace_back(output, status);

        if (stopOnError && status != 0) {
            spdlog::warn(
                "Command '{}' failed with status {}. Stopping sequence",
                command, status);
            break;
        }
    }

    {
        std::lock_guard lock(envMutex);
        for (const auto &var : envVars) {
            if (oldEnvVars.find(var.first) != oldEnvVars.end()) {
                env->setEnv(var.first, oldEnvVars[var.first]);
            } else {
                env->unsetEnv(var.first);
            }
        }
    }

    spdlog::debug("Commands with common environment completed with {} results",
                  results.size());
    return results;
}

}  // namespace atom::system
