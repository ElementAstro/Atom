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
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <condition_variable>
#include <queue>

#include "statistics.hpp"
#include "validation.hpp"
#include "history.hpp"
#include "atom/meta/global_ptr.hpp"
#include "../env.hpp"

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
    incrementTotalExecutions();

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
        incrementFailedExecutions();
        return result;
    }

    // Validate command if enabled
    if (config.validateCommand && !validateCommand(command)) {
        result.exitCode = -1;
        result.error = "Command validation failed";
        if (config.enableLogging) {
            spdlog::error("Command validation failed for: {}", command);
        }
        incrementFailedExecutions();
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
            incrementFailedExecutions();
            return result;
        }
        result.exitCode = 0;
        if (config.enableLogging) {
            spdlog::info("Command '{}' executed as user '{}\\{}'",
                        command, domain, username);
        }
        incrementSuccessfulExecutions();
        auto endTime = std::chrono::steady_clock::now();
        result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        addExecutionTime(result.executionTime.count());
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
        incrementFailedExecutions();
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
            incrementFailedExecutions();
            return result;
        }
        if (fflush(pipe.get()) != 0) {
            result.exitCode = -1;
            result.error = "Failed to flush pipe";
            if (config.enableLogging) {
                spdlog::error("Failed to flush pipe for command '{}'", command);
            }
            incrementFailedExecutions();
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
    addExecutionTime(result.executionTime.count());
    if (timedOut) {
        incrementTimedOutExecutions();
    } else if (result.exitCode == 0) {
        incrementSuccessfulExecutions();
    } else {
        incrementFailedExecutions();
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


// ---- merged from the former advanced_executor.cpp ----

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

auto executeCommandWithPolicy(
    const std::string &command,
    const ExecutionPolicy &config,
    const std::function<void(const std::string &)> &processLine)
    -> ExecutionResult {

    spdlog::debug("Executing command with policy: {}", command);

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

auto executeCommandsWithPolicy(
    const std::vector<std::string> &commands,
    const ExecutionPolicy &config,
    bool parallel,
    bool stopOnError) -> std::vector<ExecutionResult> {

    spdlog::debug("Executing {} commands with policy, parallel: {}", commands.size(), parallel);

    std::vector<ExecutionResult> results;
    results.reserve(commands.size());

    if (parallel) {
        // Parallel execution with resource management
        std::vector<std::future<ExecutionResult>> futures;
        futures.reserve(commands.size());

        for (const auto &command : commands) {
            futures.emplace_back(std::async(std::launch::async, [&command, &config]() {
                return executeCommandWithPolicy(command, config, nullptr);
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
            auto result = executeCommandWithPolicy(command, config, nullptr);
            results.push_back(result);

            if (stopOnError && result.exitCode != 0) {
                spdlog::warn("Command '{}' failed with exit code {}. Stopping sequence",
                           command, result.exitCode);
                break;
            }
        }
    }

    spdlog::debug("Commands with policy completed with {} results", results.size());
    return results;
}

auto executeCommandAsyncWithPolicy(
    const std::string &command,
    const ExecutionPolicy &config,
    const std::function<void(const std::string &)> &processLine)
    -> std::future<ExecutionResult> {

    spdlog::debug("Executing async command with policy: {}", command);

    return std::async(std::launch::async, [command, config, processLine]() {
        return executeCommandWithPolicy(command, config, processLine);
    });
}

auto executeCommandWithTimeoutCancellable(
    const std::string &command,
    const std::chrono::milliseconds &timeout,
    std::shared_ptr<CancellationToken> cancellationToken,
    const ExecutionConfig &config,
    const std::function<void(const std::string &)> &processLine)
    -> std::optional<ExecutionResult> {

    spdlog::debug("Executing command with cancellable timeout: {}, timeout: {}ms",
                  command, timeout.count());

    // Create a local cancellation token if none provided
    auto localToken = cancellationToken ? cancellationToken : std::make_shared<CancellationToken>();

    ExecutionPolicy policy;
    policy.baseConfig = config;
    policy.baseConfig.timeout = timeout;
    policy.cancellationToken = localToken;

    auto future = executeCommandAsyncWithPolicy(command, policy, processLine);
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
        spdlog::debug("Command with cancellable timeout completed successfully");
        return result;
    } catch (const std::exception &e) {
        spdlog::error("Command with cancellable timeout failed: {}", e.what());
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
