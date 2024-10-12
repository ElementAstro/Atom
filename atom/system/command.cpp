/*
 * command.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-24

Description: Simple wrapper for executing commands.

**************************************************/

#include "command.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <future>
#include <memory>
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

#include "macro.hpp"

#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/system/process.hpp"
#include "atom/utils/to_string.hpp"

#ifdef _WIN32
#include "atom/utils/convert.hpp"
#endif

namespace atom::system {

std::mutex envMutex;

auto executeCommandInternal(
    const std::string &command, [[maybe_unused]] bool openTerminal,
    const std::function<void(const std::string &)> &processLine, int &status,
    const std::string &input = "",  // 新增input参数
    const std::string &username = "", const std::string &domain = "",
    const std::string &password = "") -> std::string {
    if (command.empty()) {
        status = -1;
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
        if (!_CreateProcessAsUser(command, username, domain, password)) {
            THROW_RUNTIME_ERROR("Error: failed to run command '" + command +
                                "' as user.");
        }
        status = 0;
        return "";
    }

#ifdef _WIN32
    if (openTerminal) {
        STARTUPINFOW startupInfo{};
        PROCESS_INFORMATION processInfo{};
        if (CreateProcessW(nullptr, atom::utils::StringToLPWSTR(command),
                           nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                           &startupInfo, &processInfo) != 0) {
            WaitForSingleObject(processInfo.hProcess, INFINITE);
            CloseHandle(processInfo.hProcess);
            CloseHandle(processInfo.hThread);
            status = 0;
            return "";  // 因为终端界面会在新进程中执行，无法获得输出，所以这里返回空字符串。
        }
        THROW_FAIL_TO_CREATE_PROCESS("Error: failed to run command '" +
                                     command + "'.");
    }
    pipe.reset(_popen(command.c_str(), "w"));
#else  // 非Windows平台
    pipe.reset(popen(command.c_str(), "w"));
#endif

    if (!pipe) {
        THROW_FAIL_TO_CREATE_PROCESS("Error: failed to run command '" +
                                     command + "'.");
    }

    // 写入输入
    if (!input.empty()) {
        if (fwrite(input.c_str(), sizeof(char), input.size(), pipe.get()) !=
            input.size()) {
            THROW_RUNTIME_ERROR("Error: failed to write input to pipe.");
        }
        if (fflush(pipe.get()) != 0) {
            THROW_RUNTIME_ERROR("Error: failed to flush pipe.");
        }
    }

    constexpr std::size_t BUFFER_SIZE = 4096;
    std::array<char, BUFFER_SIZE> buffer{};
    std::ostringstream output;

    bool interrupted = false;

#ifdef _WIN32
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr &&
           !interrupted) {
        output << buffer.data();

        if (_kbhit() != 0) {
            int key = _getch();
            if (key == 3) {
                interrupted = true;
            }
        }

        if (processLine) {
            processLine(buffer.data());
        }
    }
#else
    while (!interrupted) {
#pragma unroll
        for (int i = 0; i < 4; ++i) {
            if (fgets(buffer.data(), buffer.size(), pipe.get()) == nullptr) {
                break;
            }
            std::string line = buffer.data();
            output << line;
            if (processLine) {
                processLine(line);
            }
        }
    }
#endif

#ifdef _WIN32
    status = 0;
#else
    status = WEXITSTATUS(pclose(pipe.get()));
#endif

    return output.str();
}

auto executeCommandStream(
    const std::string &command, [[maybe_unused]] bool openTerminal,
    const std::function<void(const std::string &)> &processLine, int &status,
    const std::function<bool()> &terminateCondition) -> std::string {
    if (command.empty()) {
        status = -1;
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
        STARTUPINFO startupInfo{};
        PROCESS_INFORMATION processInfo{};
        ZeroMemory(&startupInfo, sizeof(startupInfo));
        startupInfo.cb = sizeof(startupInfo);
        ZeroMemory(&processInfo, sizeof(processInfo));

        if (CreateProcess(nullptr, const_cast<LPSTR>(command.c_str()), nullptr,
                          nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr,
                          &startupInfo, &processInfo)) {
            WaitForSingleObject(processInfo.hProcess, INFINITE);
            CloseHandle(processInfo.hProcess);
            CloseHandle(processInfo.hThread);
            status = 0;
            return "";  // Since terminal window will execute in new process, we
                        // can't get output here.
        }
        THROW_FAIL_TO_CREATE_PROCESS("Error: failed to run command '" +
                                     command + "'.");
    }
    pipe.reset(_popen(command.c_str(), "r"));
#else
    pipe.reset(popen(command.c_str(), "r"));
#endif

    if (!pipe) {
        THROW_FAIL_TO_CREATE_PROCESS("Error: failed to run command '" +
                                     command + "'.");
    }

    constexpr std::size_t BUFFER_SIZE = 4096;
    std::array<char, BUFFER_SIZE> buffer{};
    std::ostringstream output;

    std::promise<void> exitSignal;
    std::future<void> futureObj = exitSignal.get_future();
    std::atomic<bool> stopReading{false};

    std::thread readerThread(
        [&pipe, &buffer, &output, &processLine, &futureObj, &stopReading]() {
#pragma GCC unroll 4
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                if (stopReading) {
                    break;
                }

                std::string line = buffer.data();
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

    // Monitor for termination condition
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
    status =
        WEXITSTATUS(pclose(pipe.release()));  // Release ownership to ensure the
                                              // pipe is closed correctly
#endif

    return output.str();
}

auto executeCommand(const std::string &command, bool openTerminal,
                    const std::function<void(const std::string &)> &processLine)
    -> std::string {
    int status = 0;
    return executeCommandInternal(command, openTerminal, processLine, status);
}

auto executeCommandWithStatus(const std::string &command)
    -> std::pair<std::string, int> {
    int status = 0;
    std::string output =
        executeCommandInternal(command, false, nullptr, status);
    return {output, status};
}

auto executeCommandWithInput(const std::string &command,
                             const std::string &input,
                             const std::function<void(const std::string &)>
                                 &processLine) -> std::string {
    int status = 0;
    return executeCommandInternal(command, /*openTerminal=*/false, processLine,
                                  status, input);
}

void executeCommands(const std::vector<std::string> &commands) {
    std::vector<std::thread> threads;
    std::vector<std::string> errors;

    threads.reserve(commands.size());
    for (const auto &command : commands) {
        threads.emplace_back([&command, &errors]() {
            try {
                int status = 0;
                std::string output = executeCommand(command, false, nullptr);
                if (status != 0) {
                    THROW_RUNTIME_ERROR("Error executing command: " + command);
                }
            } catch (const std::runtime_error &e) {
                errors.emplace_back(e.what());
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    if (!errors.empty()) {
        THROW_INVALID_ARGUMENT("One or more commands failed." +
                               atom::utils::toString(errors));
    }
}

auto executeCommandWithEnv(const std::string &command,
                           const std::unordered_map<std::string, std::string>
                               &envVars) -> std::string {
    if (command.empty()) {
        return "";
    }

    std::unordered_map<std::string, std::string> oldEnvVars;

    {
        // Lock the mutex to ensure thread safety
        std::lock_guard lock(envMutex);

        for (const auto &var : envVars) {
            std::lock_guard lock(envMutex);
            std::shared_ptr<utils::Env> env;
            GET_OR_CREATE_PTR(env, utils::Env, "LITHIUM.ENV");
            auto oldValue = env->getEnv(var.first);
            if (!oldValue.empty()) {
                oldEnvVars[var.first] = std::string(oldValue);
            }
            SETENV(var.first.c_str(), var.second.c_str());
        }
    }

    auto result = executeCommand(command, false);

    {
        // Lock the mutex to ensure thread safety
        std::lock_guard lock(envMutex);

        for (const auto &var : envVars) {
            if (oldEnvVars.find(var.first) != oldEnvVars.end()) {
                SETENV(var.first.c_str(), oldEnvVars[var.first].c_str());
            } else {
                UNSETENV(var.first.c_str());
            }
        }
    }

    return result;
}

auto executeCommandSimple(const std::string &command) -> bool {
    return executeCommandWithStatus(command).second == 0;
}

void killProcessByName(const std::string &processName, ATOM_UNUSED int signal) {
#ifdef _WIN32
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        THROW_SYSTEM_COLLAPSE("Error: unable to create toolhelp snapshot.");
    }

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32FirstW(snap, &entry) == 0) {
        CloseHandle(snap);
        THROW_SYSTEM_COLLAPSE("Error: unable to get the first process.");
    }

    do {
        if (strcmp(atom::utils::WCharArrayToString(entry.szExeFile).c_str(),
                   processName.c_str()) == 0) {
            HANDLE hProcess =
                OpenProcess(PROCESS_TERMINATE, 0, entry.th32ProcessID);
            if (hProcess != nullptr) {
                TerminateProcess(hProcess, 0);
                CloseHandle(hProcess);
            }
        }
    } while (Process32NextW(snap, &entry) != 0);

    CloseHandle(snap);
#else
    std::string command =
        "pkill -" + std::to_string(signal) + " -f " + processName;
    int result = std::system(command.c_str());
    if (result != 0) {
        THROW_SYSTEM_COLLAPSE("Error: failed to kill process with name " +
                              processName);
    }
#endif
}

void killProcessByPID(int pid, ATOM_UNUSED int signal) {
#ifdef _WIN32
    HANDLE hProcess =
        OpenProcess(PROCESS_TERMINATE, 0, static_cast<DWORD>(pid));
    if (hProcess == nullptr) {
        THROW_SYSTEM_COLLAPSE("Error: unable to open process with PID " +
                              std::to_string(pid));
    }
    TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);
#else
    if (kill(pid, signal) == -1) {
        THROW_SYSTEM_COLLAPSE("Error: failed to kill process with PID " +
                              std::to_string(pid));
    }
    int status;
    waitpid(pid, &status, 0);

#endif
}

}  // namespace atom::system
