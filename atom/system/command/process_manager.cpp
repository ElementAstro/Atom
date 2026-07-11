/*
 * process_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "process_manager.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

#include "executor.hpp"

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>  // GetModuleBaseNameA
// clang-format on
#else
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
#include <csignal>
#include <dirent.h>
#endif

#include "atom/error/exception.hpp"

#ifdef _WIN32
#include "atom/utils/convert.hpp"
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

// Global monitoring state
namespace {
    std::atomic<bool> g_monitoringActive{false};
    std::thread g_monitoringThread;
    std::mutex g_monitoringMutex;
    std::unordered_map<std::string, std::shared_ptr<ProcessGroup>> g_processGroups;
    std::mutex g_processGroupsMutex;
}

// ProcessGroup implementation
ProcessGroup::ProcessGroup(const std::string &name) : name_(name) {
    spdlog::debug("Created process group: {}", name);
}

ProcessGroup::~ProcessGroup() {
    killAll(15); // SIGTERM
    spdlog::debug("Destroyed process group: {}", name_);
}

void ProcessGroup::addProcess(int pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    pids_.push_back(pid);
    spdlog::debug("Added process {} to group {}", pid, name_);
}

void ProcessGroup::removeProcess(int pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    pids_.erase(std::remove(pids_.begin(), pids_.end(), pid), pids_.end());
    spdlog::debug("Removed process {} from group {}", pid, name_);
}

auto ProcessGroup::getProcesses() const -> std::vector<int> {
    std::lock_guard<std::mutex> lock(mutex_);
    return pids_;
}

auto ProcessGroup::getName() const -> const std::string& {
    return name_;
}

void ProcessGroup::killAll(int signal) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (int pid : pids_) {
        try {
            killProcessByPID(pid, signal);
        } catch (const std::exception& e) {
            spdlog::warn("Failed to kill process {} in group {}: {}",
                        pid, name_, e.what());
        }
    }
    pids_.clear();
}

auto ProcessGroup::getTotalCpuUsage() const -> double {
    std::lock_guard<std::mutex> lock(mutex_);
    double total = 0.0;
    for (int pid : pids_) {
        auto info = getProcessInfo(pid);
        total += info.cpuUsage;
    }
    return total;
}

auto ProcessGroup::getTotalMemoryUsage() const -> size_t {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total = 0;
    for (int pid : pids_) {
        auto info = getProcessInfo(pid);
        total += info.memoryUsage;
    }
    return total;
}

auto getProcessInfo(int pid) -> ProcessInfo {
    ProcessInfo info;
    info.pid = pid;

#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) {
        spdlog::warn("Failed to open process {}", pid);
        return info;
    }

    // Get process name
    char processName[MAX_PATH];
    if (GetModuleBaseNameA(hProcess, NULL, processName, sizeof(processName))) {
        info.name = processName;
    }

    // Get memory usage
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        info.memoryUsage = pmc.WorkingSetSize;
    }

    CloseHandle(hProcess);
#else
    // Read from /proc/pid/stat
    std::ifstream statFile("/proc/" + std::to_string(pid) + "/stat");
    if (statFile.is_open()) {
        std::string line;
        std::getline(statFile, line);
        std::istringstream iss(line);

        std::string token;
        for (int i = 0; i < 23; ++i) {
            iss >> token;
            if (i == 1) {
                // Process name (remove parentheses)
                if (token.size() > 2) {
                    info.name = token.substr(1, token.size() - 2);
                }
            } else if (i == 3) {
                info.parentPid = std::stoi(token);
            } else if (i == 22) {
                // Virtual memory size in bytes
                info.memoryUsage = std::stoull(token);
            }
        }
    }

    // Read command line
    std::ifstream cmdlineFile("/proc/" + std::to_string(pid) + "/cmdline");
    if (cmdlineFile.is_open()) {
        std::getline(cmdlineFile, info.command);
        // Replace null characters with spaces
        std::replace(info.command.begin(), info.command.end(), '\0', ' ');
    }

    // Read status
    std::ifstream statusFile("/proc/" + std::to_string(pid) + "/status");
    if (statusFile.is_open()) {
        std::string line;
        while (std::getline(statusFile, line)) {
            if (line.substr(0, 6) == "State:") {
                info.status = line.substr(7);
                break;
            }
        }
    }
#endif

    info.startTime = std::chrono::system_clock::now(); // Simplified for now
    return info;
}

auto getProcessesInfo(const std::vector<int> &pids) -> std::vector<ProcessInfo> {
    std::vector<ProcessInfo> results;
    results.reserve(pids.size());

    for (int pid : pids) {
        results.push_back(getProcessInfo(pid));
    }

    return results;
}

auto isProcessRunning(int pid) -> bool {
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == NULL) {
        return false;
    }

    DWORD exitCode;
    bool running = GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(hProcess);
    return running;
#else
    return kill(pid, 0) == 0;
#endif
}

auto createProcessGroup(const std::string &name) -> std::shared_ptr<ProcessGroup> {
    std::lock_guard<std::mutex> lock(g_processGroupsMutex);

    auto group = std::make_shared<ProcessGroup>(name);
    g_processGroups[name] = group;

    spdlog::info("Created process group: {}", name);
    return group;
}

auto getSystemProcessStats() -> std::string {
    std::ostringstream stats;
    stats << "System Process Statistics:\n";

#ifdef _WIN32
    // Windows implementation would go here
    stats << "  Platform: Windows\n";
#else
    // Read from /proc/stat
    std::ifstream statFile("/proc/stat");
    if (statFile.is_open()) {
        std::string line;
        std::getline(statFile, line);
        stats << "  CPU Info: " << line << "\n";
    }

    // Read from /proc/meminfo
    std::ifstream meminfoFile("/proc/meminfo");
    if (meminfoFile.is_open()) {
        std::string line;
        int lineCount = 0;
        while (std::getline(meminfoFile, line) && lineCount < 3) {
            stats << "  " << line << "\n";
            lineCount++;
        }
    }
#endif

    return stats.str();
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

void killProcessByNameEnhanced(const std::string &processName, int signal,
                              bool killChildren) {
    spdlog::debug("Enhanced killing process by name: {}, signal: {}, killChildren: {}",
                  processName, signal, killChildren);

    auto processes = getProcessesBySubstring(processName);
    for (const auto &[pid, name] : processes) {
        if (killChildren) {
            // Get child processes first
            auto info = getProcessInfo(pid);
            for (int childPid : info.childPids) {
                killProcessByPID(childPid, signal);
            }
        }
        killProcessByPID(pid, signal);
    }
}

void killProcessByPIDEnhanced(int pid, int signal, bool killChildren) {
    spdlog::debug("Enhanced killing process by PID: {}, signal: {}, killChildren: {}",
                  pid, signal, killChildren);

    if (killChildren) {
        auto info = getProcessInfo(pid);
        for (int childPid : info.childPids) {
            killProcessByPID(childPid, signal);
        }
    }
    killProcessByPID(pid, signal);
}

auto startProcessEnhanced(
    const std::string &command,
    const std::string &workingDir,
    const std::vector<std::pair<std::string, std::string>> &envVars)
    -> ProcessInfo {

    spdlog::debug("Enhanced starting process: {}, workingDir: {}", command, workingDir);

    ProcessInfo info;
    info.command = command;
    info.startTime = std::chrono::system_clock::now();

    // Set environment variables
    for (const auto &[key, value] : envVars) {
#ifdef _WIN32
        SetEnvironmentVariableA(key.c_str(), value.c_str());
#else
        setenv(key.c_str(), value.c_str(), 1);
#endif
    }

    auto [pid, handle] = startProcess(command);
    info.pid = pid;

    // Restore environment variables
    for (const auto &[key, value] : envVars) {
#ifdef _WIN32
        SetEnvironmentVariableA(key.c_str(), nullptr);
#else
        unsetenv(key.c_str());
#endif
    }

    // Get additional process information
    if (pid > 0) {
        auto detailedInfo = getProcessInfo(pid);
        info.name = detailedInfo.name;
        info.status = detailedInfo.status;
        info.memoryUsage = detailedInfo.memoryUsage;
        info.cpuUsage = detailedInfo.cpuUsage;
    }

    spdlog::info("Enhanced process started with PID: {}", pid);
    return info;
}

auto getProcessesBySubstringEnhanced(
    const std::string &substring, bool includeDetails)
    -> std::vector<ProcessInfo> {

    spdlog::debug("Enhanced getting processes by substring: {}, includeDetails: {}",
                  substring, includeDetails);

    std::vector<ProcessInfo> results;
    auto basicResults = getProcessesBySubstring(substring);

    for (const auto &[pid, name] : basicResults) {
        if (includeDetails) {
            results.push_back(getProcessInfo(pid));
        } else {
            ProcessInfo info;
            info.pid = pid;
            info.name = name;
            results.push_back(info);
        }
    }

    return results;
}

auto monitorProcesses(const std::vector<int> &pids,
                     const ProcessMonitorConfig &config,
                     std::function<void(const std::vector<ProcessInfo>&)> callback)
    -> bool {

    if (g_monitoringActive.load()) {
        spdlog::warn("Process monitoring is already active");
        return false;
    }

    g_monitoringActive = true;

    g_monitoringThread = std::thread([pids, config, callback]() {
        spdlog::info("Started process monitoring for {} processes", pids.size());

        while (g_monitoringActive.load()) {
            auto processInfos = getProcessesInfo(pids);
            callback(processInfos);

            std::this_thread::sleep_for(config.updateInterval);
        }

        spdlog::info("Process monitoring stopped");
    });

    return true;
}

void stopProcessMonitoring() {
    if (g_monitoringActive.load()) {
        g_monitoringActive = false;
        if (g_monitoringThread.joinable()) {
            g_monitoringThread.join();
        }
        spdlog::info("Process monitoring stopped");
    }
}

}  // namespace atom::system
