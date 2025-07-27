/*
 * executor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "executor.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <future>
#include <iomanip>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>
#include <unordered_set>

#include "cache.hpp"
#include "config.hpp"
#include "thread_pool.hpp"

#ifdef _WIN32
#define SETENV(name, value) SetEnvironmentVariableA(name, value)
#define UNSETENV(name) SetEnvironmentVariableA(name, nullptr)
// clang-format off
#include <windows.h>
#include <conio.h>
#include <tlhelp32.h>
// clang-format on
#else
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#define SETENV(name, value) setenv(name, value, 1)
#define UNSETENV(name) unsetenv(name)
#endif

#include "atom/error/exception.hpp"
#include "atom/system/process.hpp"

#ifdef _WIN32
#include "atom/utils/convert.hpp"
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

// Global statistics tracking
namespace {
    std::mutex g_statsMutex;
    std::atomic<size_t> g_totalExecutions{0};
    std::atomic<size_t> g_successfulExecutions{0};
    std::atomic<size_t> g_failedExecutions{0};
    std::atomic<size_t> g_timedOutExecutions{0};
    std::atomic<std::chrono::milliseconds::rep> g_totalExecutionTime{0};

    // Command validation patterns
    const std::unordered_set<std::string> DANGEROUS_COMMANDS = {
        "rm", "del", "format", "fdisk", "mkfs", "dd", "shutdown", "reboot",
        "halt", "poweroff", "init", "kill", "killall", "pkill"
    };

    const std::regex COMMAND_INJECTION_PATTERN(R"([;&|`$(){}[\]<>])");
}

auto validateCommand(const std::string &command) -> bool {
    if (command.empty()) {
        spdlog::warn("Empty command provided for validation");
        return false;
    }

    // Check for command injection patterns
    if (std::regex_search(command, COMMAND_INJECTION_PATTERN)) {
        spdlog::warn("Command contains potentially dangerous characters: {}", command);
        return false;
    }

    // Extract the base command (first word)
    std::istringstream iss(command);
    std::string baseCommand;
    iss >> baseCommand;

    // Remove path if present
    size_t lastSlash = baseCommand.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        baseCommand = baseCommand.substr(lastSlash + 1);
    }

    // Check against dangerous commands list
    if (DANGEROUS_COMMANDS.find(baseCommand) != DANGEROUS_COMMANDS.end()) {
        spdlog::warn("Command '{}' is in the dangerous commands list", baseCommand);
        return false;
    }

    spdlog::debug("Command validation passed for: {}", command);
    return true;
}

auto executeCommandInternalEnhanced(
    const std::string &command,
    const ExecutionConfig &config,
    const std::function<void(const std::string &)> &processLine,
    const std::string &input,
    const std::string &username,
    const std::string &domain,
    const std::string &password) -> ExecutionResult {

    auto startTime = std::chrono::steady_clock::now();
    ExecutionResult result;

    // Update statistics
    g_totalExecutions++;

    if (config.enableLogging) {
        spdlog::debug("Executing enhanced command: {}, openTerminal: {}",
                     command, config.openTerminal);
    }

    if (command.empty()) {
        result.exitCode = -1;
        result.error = "Command is empty";
        if (config.enableLogging) {
            spdlog::error("Command is empty");
        }
        g_failedExecutions++;
        return result;
    }

    // Validate command if enabled
    if (config.validateCommand && !validateCommand(command)) {
        result.exitCode = -1;
        result.error = "Command validation failed";
        if (config.enableLogging) {
            spdlog::error("Command validation failed for: {}", command);
        }
        g_failedExecutions++;
        return result;
    }

    auto pipeDeleter = [](FILE *pipe) {
        if (pipe != nullptr) {
#ifdef _MSC_VER
            _pclose(pipe);
#else
            pclose(pipe);
#endif
        }
    };

    std::unique_ptr<FILE, decltype(pipeDeleter)> pipe(nullptr, pipeDeleter);

    // Handle user authentication if provided
    if (!username.empty() && !domain.empty() && !password.empty()) {
        if (!createProcessAsUser(command, username, domain, password)) {
            result.exitCode = -1;
            result.error = "Failed to run command as user";
            if (config.enableLogging) {
                spdlog::error("Failed to run command '{}' as user '{}\\{}'",
                              command, domain, username);
            }
            g_failedExecutions++;
            return result;
        }
        result.exitCode = 0;
        if (config.enableLogging) {
            spdlog::info("Command '{}' executed as user '{}\\{}'",
                        command, domain, username);
        }
        g_successfulExecutions++;
        auto endTime = std::chrono::steady_clock::now();
        result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        g_totalExecutionTime += result.executionTime.count();
        return result;
    }

    // Prepare command for execution
    std::string fullCommand = command;
#ifdef _WIN32
    if (config.openTerminal) {
        fullCommand = "start cmd /c \"" + command + "\"";
    }
    pipe.reset(_popen(fullCommand.c_str(), "r"));
#else
    if (config.openTerminal) {
        fullCommand = "gnome-terminal -e 'bash -c \"" + command + "; read\"' &";
    }
    pipe.reset(popen(fullCommand.c_str(), "r"));
#endif

    if (!pipe) {
        result.exitCode = -1;
        result.error = "Failed to run command";
        if (config.enableLogging) {
            spdlog::error("Failed to run command '{}'", command);
        }
        g_failedExecutions++;
        return result;
    }

    // Handle input if provided
    if (!input.empty()) {
        if (fwrite(input.c_str(), sizeof(char), input.size(), pipe.get()) !=
            input.size()) {
            result.exitCode = -1;
            result.error = "Failed to write input to pipe";
            if (config.enableLogging) {
                spdlog::error("Failed to write input to pipe for command '{}'", command);
            }
            g_failedExecutions++;
            return result;
        }
        if (fflush(pipe.get()) != 0) {
            result.exitCode = -1;
            result.error = "Failed to flush pipe";
            if (config.enableLogging) {
                spdlog::error("Failed to flush pipe for command '{}'", command);
            }
            g_failedExecutions++;
            return result;
        }
    }

    // Read output with enhanced buffering
    std::vector<char> buffer(config.bufferSize);
    std::ostringstream output;
    std::ostringstream errorOutput;
    size_t totalOutputSize = 0;
    bool interrupted = false;
    bool timedOut = false;

    auto timeoutTime = startTime + config.timeout;
    bool hasTimeout = config.timeout.count() > 0;

#ifdef _WIN32
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr && !interrupted) {
        if (hasTimeout && std::chrono::steady_clock::now() > timeoutTime) {
            timedOut = true;
            break;
        }

        std::string line(buffer.data());
        totalOutputSize += line.size();

        if (totalOutputSize > config.maxOutputSize) {
            result.error = "Output size limit exceeded";
            break;
        }

        output << line;

        if (_kbhit()) {
            int key = _getch();
            if (key == 3) { // Ctrl+C
                interrupted = true;
            }
        }

        if (processLine) {
            processLine(line);
        }
    }
#else
    while (!interrupted && !timedOut &&
           fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        if (hasTimeout && std::chrono::steady_clock::now() > timeoutTime) {
            timedOut = true;
            break;
        }

        std::string line(buffer.data());
        totalOutputSize += line.size();

        if (totalOutputSize > config.maxOutputSize) {
            result.error = "Output size limit exceeded";
            break;
        }

        output << line;

        if (processLine) {
            processLine(line);
        }
    }
#endif

    // Get exit status
#ifdef _WIN32
    result.exitCode = _pclose(pipe.release());
#else
    result.exitCode = WEXITSTATUS(pclose(pipe.release()));
#endif

    result.output = output.str();
    result.timedOut = timedOut;
    result.wasKilled = interrupted;

    auto endTime = std::chrono::steady_clock::now();
    result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    // Update statistics
    g_totalExecutionTime += result.executionTime.count();
    if (timedOut) {
        g_timedOutExecutions++;
    } else if (result.exitCode == 0) {
        g_successfulExecutions++;
    } else {
        g_failedExecutions++;
    }

    if (config.enableLogging) {
        spdlog::debug("Enhanced command '{}' executed with status: {}, time: {}ms",
                     command, result.exitCode, result.executionTime.count());
    }

    return result;
}

auto executeCommandInternal(
    const std::string &command, bool openTerminal,
    const std::function<void(const std::string &)> &processLine, int &status,
    const std::string &input, const std::string &username,
    const std::string &domain, const std::string &password) -> std::string {
    spdlog::debug("Executing command: {}, openTerminal: {}", command,
                  openTerminal);

    if (command.empty()) {
        status = -1;
        spdlog::error("Command is empty");
        return "";
    }

    auto pipeDeleter = [](FILE *pipe) {
        if (pipe != nullptr) {
#ifdef _MSC_VER
            _pclose(pipe);
#else
            pclose(pipe);
#endif
        }
    };

    std::unique_ptr<FILE, decltype(pipeDeleter)> pipe(nullptr, pipeDeleter);

    if (!username.empty() && !domain.empty() && !password.empty()) {
        if (!createProcessAsUser(command, username, domain, password)) {
            spdlog::error("Failed to run command '{}' as user '{}\\{}'",
                          command, domain, username);
            THROW_RUNTIME_ERROR("Failed to run command as user");
        }
        status = 0;
        spdlog::info("Command '{}' executed as user '{}\\{}'", command, domain,
                     username);
        return "";
    }

#ifdef _WIN32
    if (openTerminal) {
        STARTUPINFOW startupInfo{};
        PROCESS_INFORMATION processInfo{};
        startupInfo.cb = sizeof(startupInfo);

        std::wstring commandW = atom::utils::StringToLPWSTR(command);
        if (CreateProcessW(nullptr, &commandW[0], nullptr, nullptr, FALSE, 0,
                           nullptr, nullptr, &startupInfo, &processInfo)) {
            WaitForSingleObject(processInfo.hProcess, INFINITE);
            CloseHandle(processInfo.hProcess);
            CloseHandle(processInfo.hThread);
            status = 0;
            spdlog::info("Command '{}' executed in terminal", command);
            return "";
        }
        spdlog::error("Failed to run command '{}' in terminal", command);
        THROW_FAIL_TO_CREATE_PROCESS("Failed to run command in terminal");
    }
    pipe.reset(_popen(command.c_str(), "r"));
#else
    pipe.reset(popen(command.c_str(), "r"));
#endif

    if (!pipe) {
        spdlog::error("Failed to run command '{}'", command);
        THROW_FAIL_TO_CREATE_PROCESS("Failed to run command");
    }

    if (!input.empty()) {
        if (fwrite(input.c_str(), sizeof(char), input.size(), pipe.get()) !=
            input.size()) {
            spdlog::error("Failed to write input to pipe for command '{}'",
                          command);
            THROW_RUNTIME_ERROR("Failed to write input to pipe");
        }
        if (fflush(pipe.get()) != 0) {
            spdlog::error("Failed to flush pipe for command '{}'", command);
            THROW_RUNTIME_ERROR("Failed to flush pipe");
        }
    }

    constexpr std::size_t BUFFER_SIZE = 4096;
    std::array<char, BUFFER_SIZE> buffer{};
    std::ostringstream output;

    bool interrupted = false;

#ifdef _WIN32
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr &&
           !interrupted) {
        std::string line(buffer.data());
        output << line;

        if (_kbhit()) {
            int key = _getch();
            if (key == 3) {
                interrupted = true;
            }
        }

        if (processLine) {
            processLine(line);
        }
    }
#else
    while (!interrupted &&
           fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line(buffer.data());
        output << line;

        if (processLine) {
            processLine(line);
        }
    }
#endif

#ifdef _WIN32
    status = _pclose(pipe.release());
#else
    status = WEXITSTATUS(pclose(pipe.release()));
#endif
    spdlog::debug("Command '{}' executed with status: {}", command, status);
    return output.str();
}

auto executeCommandStream(
    const std::string &command, bool openTerminal,
    const std::function<void(const std::string &)> &processLine, int &status,
    const std::function<bool()> &terminateCondition) -> std::string {
    spdlog::debug("Executing command stream: {}, openTerminal: {}", command,
                  openTerminal);

    if (command.empty()) {
        status = -1;
        spdlog::error("Command is empty");
        return "";
    }

    auto pipeDeleter = [](FILE *pipe) {
        if (pipe != nullptr) {
#ifdef _MSC_VER
            _pclose(pipe);
#else
            pclose(pipe);
#endif
        }
    };

    std::unique_ptr<FILE, decltype(pipeDeleter)> pipe(nullptr, pipeDeleter);

#ifdef _WIN32
    if (openTerminal) {
        STARTUPINFOW startupInfo{};
        PROCESS_INFORMATION processInfo{};
        startupInfo.cb = sizeof(startupInfo);

        std::wstring commandW = atom::utils::StringToLPWSTR(command);
        if (CreateProcessW(nullptr, &commandW[0], nullptr, nullptr, FALSE,
                           CREATE_NEW_CONSOLE, nullptr, nullptr, &startupInfo,
                           &processInfo)) {
            WaitForSingleObject(processInfo.hProcess, INFINITE);
            CloseHandle(processInfo.hProcess);
            CloseHandle(processInfo.hThread);
            status = 0;
            spdlog::info("Command '{}' executed in terminal", command);
            return "";
        }
        spdlog::error("Failed to run command '{}' in terminal", command);
        THROW_FAIL_TO_CREATE_PROCESS("Failed to run command in terminal");
    }
    pipe.reset(_popen(command.c_str(), "r"));
#else
    pipe.reset(popen(command.c_str(), "r"));
#endif

    if (!pipe) {
        spdlog::error("Failed to run command '{}'", command);
        THROW_FAIL_TO_CREATE_PROCESS("Failed to run command");
    }

    constexpr std::size_t BUFFER_SIZE = 4096;
    std::array<char, BUFFER_SIZE> buffer{};
    std::ostringstream output;

    std::promise<void> exitSignal;
    std::future<void> futureObj = exitSignal.get_future();
    std::atomic<bool> stopReading{false};

    std::thread readerThread(
        [&pipe, &buffer, &output, &processLine, &futureObj, &stopReading]() {
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                if (stopReading) {
                    break;
                }

                std::string line(buffer.data());
                output << line;
                if (processLine) {
                    processLine(line);
                }

                if (futureObj.wait_for(std::chrono::milliseconds(1)) !=
                    std::future_status::timeout) {
                    break;
                }
            }
        });

    while (!terminateCondition()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    stopReading = true;
    exitSignal.set_value();

    if (readerThread.joinable()) {
        readerThread.join();
    }

#ifdef _WIN32
    status = _pclose(pipe.release());
#else
    status = WEXITSTATUS(pclose(pipe.release()));
#endif

    spdlog::debug("Command '{}' executed with status: {}", command, status);
    return output.str();
}

auto executeCommand(const std::string &command, bool openTerminal,
                    const std::function<void(const std::string &)> &processLine)
    -> std::string {
    spdlog::debug("Executing command: {}, openTerminal: {}", command,
                  openTerminal);
    int status = 0;
    auto result =
        executeCommandInternal(command, openTerminal, processLine, status);
    spdlog::debug("Command completed with status: {}", status);
    return result;
}

auto executeCommandWithStatus(const std::string &command)
    -> std::pair<std::string, int> {
    spdlog::debug("Executing command with status: {}", command);
    int status = 0;
    std::string output =
        executeCommandInternal(command, false, nullptr, status);
    spdlog::debug("Command completed with status: {}", status);
    return {output, status};
}

auto executeCommandWithInput(
    const std::string &command, const std::string &input,
    const std::function<void(const std::string &)> &processLine)
    -> std::string {
    spdlog::debug("Executing command with input: {}", command);
    int status = 0;
    auto result =
        executeCommandInternal(command, false, processLine, status, input);
    spdlog::debug("Command with input completed with status: {}", status);
    return result;
}

void executeCommands(const std::vector<std::string> &commands) {
    spdlog::debug("Executing {} commands", commands.size());
    std::vector<std::thread> threads;
    std::vector<std::string> errors;
    std::mutex errorMutex;

    threads.reserve(commands.size());
    for (const auto &command : commands) {
        threads.emplace_back([&command, &errors, &errorMutex]() {
            try {
                int status = 0;
                [[maybe_unused]] auto res =
                    executeCommand(command, false, nullptr);
                if (status != 0) {
                    THROW_RUNTIME_ERROR("Error executing command: " + command);
                }
            } catch (const std::runtime_error &e) {
                std::lock_guard lock(errorMutex);
                errors.emplace_back(e.what());
            }
        });
    }

    for (auto &thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    if (!errors.empty()) {
        std::ostringstream oss;
        for (const auto &err : errors) {
            oss << err << "\n";
        }
        THROW_INVALID_ARGUMENT("One or more commands failed:\n" + oss.str());
    }
    spdlog::debug("All commands executed successfully");
}

auto executeCommandSimple(const std::string &command) -> bool {
    spdlog::debug("Executing simple command: {}", command);
    auto result = executeCommandWithStatus(command).second == 0;
    spdlog::debug("Simple command completed with result: {}", result);
    return result;
}

auto executeCommandEnhanced(
    const std::string &command,
    const ExecutionConfig &config,
    const std::function<void(const std::string &)> &processLine)
    -> ExecutionResult {
    return executeCommandInternalEnhanced(command, config, processLine);
}

auto executeCommandsEnhanced(
    const std::vector<std::string> &commands,
    const ExecutionConfig &config,
    bool parallel,
    bool stopOnError) -> std::vector<ExecutionResult> {

    spdlog::debug("Executing {} enhanced commands, parallel: {}, stopOnError: {}",
                  commands.size(), parallel, stopOnError);

    std::vector<ExecutionResult> results;
    results.reserve(commands.size());

    if (parallel) {
        // Parallel execution
        std::vector<std::future<ExecutionResult>> futures;
        futures.reserve(commands.size());

        for (const auto &command : commands) {
            futures.emplace_back(std::async(std::launch::async, [&command, &config]() {
                return executeCommandInternalEnhanced(command, config, nullptr);
            }));
        }

        for (auto &future : futures) {
            auto result = future.get();
            results.push_back(result);

            if (stopOnError && result.exitCode != 0) {
                spdlog::warn("Command failed with exit code {}. Stopping parallel execution",
                           result.exitCode);
                break;
            }
        }
    } else {
        // Sequential execution
        for (const auto &command : commands) {
            auto result = executeCommandInternalEnhanced(command, config, nullptr);
            results.push_back(result);

            if (stopOnError && result.exitCode != 0) {
                spdlog::warn("Command '{}' failed with exit code {}. Stopping sequence",
                           command, result.exitCode);
                break;
            }
        }
    }

    spdlog::debug("Enhanced commands completed with {} results", results.size());
    return results;
}

auto getExecutionStatistics() -> std::string {
    std::lock_guard<std::mutex> lock(g_statsMutex);

    std::ostringstream stats;
    stats << "Command Execution Statistics:\n";
    stats << "  Total Executions: " << g_totalExecutions.load() << "\n";
    stats << "  Successful: " << g_successfulExecutions.load() << "\n";
    stats << "  Failed: " << g_failedExecutions.load() << "\n";
    stats << "  Timed Out: " << g_timedOutExecutions.load() << "\n";

    auto totalTime = g_totalExecutionTime.load();
    stats << "  Total Execution Time: " << totalTime << "ms\n";

    if (g_totalExecutions.load() > 0) {
        auto avgTime = totalTime / g_totalExecutions.load();
        stats << "  Average Execution Time: " << avgTime << "ms\n";

        auto successRate = (g_successfulExecutions.load() * 100.0) / g_totalExecutions.load();
        stats << "  Success Rate: " << std::fixed << std::setprecision(2) << successRate << "%\n";
    }

    return stats.str();
}

void clearExecutionStatistics() {
    std::lock_guard<std::mutex> lock(g_statsMutex);

    g_totalExecutions = 0;
    g_successfulExecutions = 0;
    g_failedExecutions = 0;
    g_timedOutExecutions = 0;
    g_totalExecutionTime = 0;

    spdlog::info("Execution statistics cleared");
}

// ============================================================================
// Optimized Execution Functions Implementation
// ============================================================================

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

auto getExecutionMetrics() -> const CommandSystemMetrics& {
    return COMMAND_METRICS();
}

void resetExecutionStatistics() {
    const_cast<CommandSystemMetrics&>(COMMAND_METRICS()).reset();
    clearExecutionStatistics(); // Also clear legacy statistics
}

}  // namespace atom::system
