/*
 * executor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "executor.hpp"

#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

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

}  // namespace atom::system
