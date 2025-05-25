/*
 * command.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "command.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "env.hpp"

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
#include "atom/meta/global_ptr.hpp"
#include "atom/system/process.hpp"

#ifdef _WIN32
#include "atom/utils/convert.hpp"
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

std::mutex envMutex;

auto executeCommandInternal(
    const std::string &command, bool openTerminal,
    const std::function<void(const std::string &)> &processLine, int &status,
    const std::string &input = "", const std::string &username = "",
    const std::string &domain = "", const std::string &password = "")
    -> std::string {
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
    pipe.reset(_popen(command.c_str(), "w"));
#else
    pipe.reset(popen(command.c_str(), "w"));
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

auto executeCommandSimple(const std::string &command) -> bool {
    spdlog::debug("Executing simple command: {}", command);
    auto result = executeCommandWithStatus(command).second == 0;
    spdlog::debug("Simple command completed with result: {}", result);
    return result;
}

void killProcessByName(const std::string &processName, int signal) {
    spdlog::debug("Killing process by name: {}, signal: {}", processName,
                  signal);
#ifdef _WIN32
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        spdlog::error("Unable to create toolhelp snapshot");
        THROW_SYSTEM_COLLAPSE("Unable to create toolhelp snapshot");
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(snap, &entry)) {
        CloseHandle(snap);
        spdlog::error("Unable to get the first process");
        THROW_SYSTEM_COLLAPSE("Unable to get the first process");
    }

    do {
        std::string currentProcess =
            atom::utils::WCharArrayToString(entry.szExeFile);
        if (currentProcess == processName) {
            HANDLE hProcess =
                OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
            if (hProcess) {
                if (!TerminateProcess(hProcess, 0)) {
                    spdlog::error("Failed to terminate process '{}'",
                                  processName);
                    CloseHandle(hProcess);
                    THROW_SYSTEM_COLLAPSE("Failed to terminate process");
                }
                CloseHandle(hProcess);
                spdlog::info("Process '{}' terminated", processName);
            }
        }
    } while (Process32NextW(snap, &entry));

    CloseHandle(snap);
#else
    std::string cmd = "pkill -" + std::to_string(signal) + " -f " + processName;
    auto [output, status] = executeCommandWithStatus(cmd);
    if (status != 0) {
        spdlog::error("Failed to kill process with name '{}'", processName);
        THROW_SYSTEM_COLLAPSE("Failed to kill process by name");
    }
    spdlog::info("Process '{}' terminated with signal {}", processName, signal);
#endif
}

void killProcessByPID(int pid, int signal) {
    spdlog::debug("Killing process by PID: {}, signal: {}", pid, signal);
#ifdef _WIN32
    HANDLE hProcess =
        OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!hProcess) {
        spdlog::error("Unable to open process with PID {}", pid);
        THROW_SYSTEM_COLLAPSE("Unable to open process");
    }
    if (!TerminateProcess(hProcess, 0)) {
        spdlog::error("Failed to terminate process with PID {}", pid);
        CloseHandle(hProcess);
        THROW_SYSTEM_COLLAPSE("Failed to terminate process by PID");
    }
    CloseHandle(hProcess);
    spdlog::info("Process with PID {} terminated", pid);
#else
    if (kill(pid, signal) == -1) {
        spdlog::error("Failed to kill process with PID {}", pid);
        THROW_SYSTEM_COLLAPSE("Failed to kill process by PID");
    }
    int status;
    waitpid(pid, &status, 0);
    spdlog::info("Process with PID {} terminated with signal {}", pid, signal);
#endif
}

auto startProcess(const std::string &command) -> std::pair<int, void *> {
    spdlog::debug("Starting process: {}", command);
#ifdef _WIN32
    STARTUPINFOW startupInfo{};
    PROCESS_INFORMATION processInfo{};
    startupInfo.cb = sizeof(startupInfo);

    std::wstring commandW = atom::utils::StringToLPWSTR(command);
    if (CreateProcessW(nullptr, const_cast<LPWSTR>(commandW.c_str()), nullptr,
                       nullptr, FALSE, 0, nullptr, nullptr, &startupInfo,
                       &processInfo)) {
        CloseHandle(processInfo.hThread);
        spdlog::info("Process '{}' started with PID: {}", command,
                     processInfo.dwProcessId);
        return {processInfo.dwProcessId, processInfo.hProcess};
    } else {
        spdlog::error("Failed to start process '{}'", command);
        THROW_FAIL_TO_CREATE_PROCESS("Failed to start process");
    }
#else
    pid_t pid = fork();
    if (pid == -1) {
        spdlog::error("Failed to fork process for command '{}'", command);
        THROW_FAIL_TO_CREATE_PROCESS("Failed to fork process");
    }
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", command.c_str(), (char *)nullptr);
        _exit(EXIT_FAILURE);
    } else {
        spdlog::info("Process '{}' started with PID: {}", command, pid);
        return {pid, nullptr};
    }
#endif
}

auto isCommandAvailable(const std::string &command) -> bool {
    std::string checkCommand;
#ifdef _WIN32
    checkCommand = "where " + command + " > nul 2>&1";
#else
    checkCommand = "command -v " + command + " > /dev/null 2>&1";
#endif
    return atom::system::executeCommandSimple(checkCommand);
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

auto getProcessesBySubstring(const std::string &substring)
    -> std::vector<std::pair<int, std::string>> {
    spdlog::debug("Getting processes by substring: {}", substring);

    std::vector<std::pair<int, std::string>> processes;

#ifdef _WIN32
    std::string command = "tasklist /FO CSV /NH";
    auto output = executeCommand(command);

    std::istringstream ss(output);
    std::string line;
    std::regex pattern("\"([^\"]+)\",\"(\\d+)\"");

    while (std::getline(ss, line)) {
        std::smatch matches;
        if (std::regex_search(line, matches, pattern) && matches.size() > 2) {
            std::string processName = matches[1].str();
            int pid = std::stoi(matches[2].str());

            if (processName.find(substring) != std::string::npos) {
                processes.emplace_back(pid, processName);
            }
        }
    }
#else
    std::string command = "ps -eo pid,comm | grep " + substring;
    auto output = executeCommand(command);

    std::istringstream ss(output);
    std::string line;

    while (std::getline(ss, line)) {
        std::istringstream lineStream(line);
        int pid;
        std::string processName;

        if (lineStream >> pid >> processName) {
            if (processName != "grep") {
                processes.emplace_back(pid, processName);
            }
        }
    }
#endif

    spdlog::debug("Found {} processes matching '{}'", processes.size(),
                  substring);
    return processes;
}

auto executeCommandGetLines(const std::string &command)
    -> std::vector<std::string> {
    spdlog::debug("Executing command and getting lines: {}", command);

    std::vector<std::string> lines;
    auto output = executeCommand(command);

    std::istringstream ss(output);
    std::string line;

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }

    spdlog::debug("Command returned {} lines", lines.size());
    return lines;
}

auto pipeCommands(const std::string &firstCommand,
                  const std::string &secondCommand) -> std::string {
    spdlog::debug("Piping commands: '{}' | '{}'", firstCommand, secondCommand);

#ifdef _WIN32
    std::string tempFile = std::tmpnam(nullptr);
    std::string combinedCommand = firstCommand + " > " + tempFile + " && " +
                                  secondCommand + " < " + tempFile +
                                  " && del " + tempFile;
#else
    std::string combinedCommand = firstCommand + " | " + secondCommand;
#endif

    auto result = executeCommand(combinedCommand);
    spdlog::debug("Pipe commands completed");
    return result;
}

class CommandHistory::Impl {
public:
    explicit Impl(size_t maxSize) : _maxSize(maxSize) {}

    void addCommand(const std::string &command, int exitStatus) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (_history.size() >= _maxSize) {
            _history.pop_front();
        }

        _history.emplace_back(command, exitStatus);
    }

    auto getLastCommands(size_t count) const
        -> std::vector<std::pair<std::string, int>> {
        std::lock_guard<std::mutex> lock(_mutex);

        count = std::min(count, _history.size());
        std::vector<std::pair<std::string, int>> result;
        result.reserve(count);

        auto it = _history.rbegin();
        for (size_t i = 0; i < count; ++i, ++it) {
            result.push_back(*it);
        }

        return result;
    }

    auto searchCommands(const std::string &substring) const
        -> std::vector<std::pair<std::string, int>> {
        std::lock_guard<std::mutex> lock(_mutex);

        std::vector<std::pair<std::string, int>> result;

        for (const auto &entry : _history) {
            if (entry.first.find(substring) != std::string::npos) {
                result.push_back(entry);
            }
        }

        return result;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _history.clear();
    }

    auto size() const -> size_t {
        std::lock_guard<std::mutex> lock(_mutex);
        return _history.size();
    }

private:
    mutable std::mutex _mutex;
    std::list<std::pair<std::string, int>> _history;
    size_t _maxSize;
};

CommandHistory::CommandHistory(size_t maxSize)
    : pImpl(std::make_unique<Impl>(maxSize)) {}

CommandHistory::~CommandHistory() = default;

void CommandHistory::addCommand(const std::string &command, int exitStatus) {
    pImpl->addCommand(command, exitStatus);
}

auto CommandHistory::getLastCommands(size_t count) const
    -> std::vector<std::pair<std::string, int>> {
    return pImpl->getLastCommands(count);
}

auto CommandHistory::searchCommands(const std::string &substring) const
    -> std::vector<std::pair<std::string, int>> {
    return pImpl->searchCommands(substring);
}

void CommandHistory::clear() { pImpl->clear(); }

auto CommandHistory::size() const -> size_t { return pImpl->size(); }

auto createCommandHistory(size_t maxHistorySize)
    -> std::unique_ptr<CommandHistory> {
    spdlog::debug("Creating command history with max size: {}", maxHistorySize);
    return std::make_unique<CommandHistory>(maxHistorySize);
}

}  // namespace atom::system
