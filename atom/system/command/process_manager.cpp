/*
 * process_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "process_manager.hpp"

#include <sstream>
#include <string>

#include "executor.hpp"

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <tlhelp32.h>
// clang-format on
#else
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#endif

#include "atom/error/exception.hpp"

#ifdef _WIN32
#include "atom/utils/convert.hpp"
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

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

}  // namespace atom::system
