/*
 * advanced_executor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "advanced_executor.hpp"

#include <chrono>
#include <mutex>

#include "executor.hpp"

#include "atom/error/exception.hpp"
#include "atom/meta/global_ptr.hpp"
#include "../env.hpp"

#include <spdlog/spdlog.h>

namespace atom::system {

// Global mutex for environment operations (declared in command.cpp)
extern std::mutex envMutex;

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
