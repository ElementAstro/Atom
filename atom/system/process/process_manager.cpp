/*
 * process_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "process_manager.hpp"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <sstream>

#if defined(_WIN32)
#include <iphlpapi.h>
#include <psapi.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <windows.h>
#elif defined(__linux__) || defined(__ANDROID__)
#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#if __has_include(<sys/capability.h>)
#include <sys/capability.h>
#endif
#elif defined(__APPLE__)
#include <libproc.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#else
#error "Unknown platform"
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

constexpr size_t BUFFER_SIZE = 256;

class ProcessManager::ProcessManagerImpl {
public:
    explicit ProcessManagerImpl(int maxProcess) : m_maxProcesses(maxProcess) {}

    ~ProcessManagerImpl() { waitForCompletion(); }

    ProcessManagerImpl(const ProcessManagerImpl &) = delete;
    ProcessManagerImpl &operator=(const ProcessManagerImpl &) = delete;
    ProcessManagerImpl(ProcessManagerImpl &&) = delete;
    ProcessManagerImpl &operator=(ProcessManagerImpl &&) = delete;

    auto createProcess(const std::string &command,
                       const std::string &identifier, bool isBackground)
        -> bool {
        if (processes.size() >= static_cast<size_t>(m_maxProcesses)) {
            spdlog::error("Maximum number of managed processes reached: {}",
                          m_maxProcesses);
            THROW_PROCESS_ERROR("Maximum number of managed processes reached.");
        }

        pid_t pid;
#ifdef _WIN32
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        BOOL success = CreateProcessA(
            NULL, const_cast<char *>(command.c_str()), NULL, NULL, FALSE,
            isBackground ? CREATE_NO_WINDOW : 0, NULL, NULL, &si, &pi);

        if (!success) {
            DWORD error = GetLastError();
            spdlog::error(
                "CreateProcess failed with error code: {} for command: {}",
                error, command);
            THROW_PROCESS_ERROR("Failed to create process.");
        }

        pid = pi.dwProcessId;
#else
        pid = fork();
        if (pid == 0) {
            if (isBackground) {
                if (setsid() < 0) {
                    _exit(EXIT_FAILURE);
                }
            }
            execlp(command.c_str(), command.c_str(), nullptr);
            spdlog::error("execlp failed for command: {}", command);
            _exit(EXIT_FAILURE);
        } else if (pid < 0) {
            spdlog::error("Failed to fork process for command: {}", command);
            THROW_PROCESS_ERROR("Failed to fork process.");
        }
#endif
        std::unique_lock lock(mtx);
        Process process;
        process.pid = pid;
        process.name = identifier;
        process.command = command;
        process.isBackground = isBackground;
#ifdef _WIN32
        process.handle = pi.hProcess;
#endif
        processes.emplace_back(process);
        spdlog::info(
            "Process created successfully: PID={}, identifier={}, command={}",
            pid, identifier, command);
        return true;
    }

    auto terminateProcess(int pid, int signal) -> bool {
        std::unique_lock lock(mtx);
        auto processIt = std::find_if(
            processes.begin(), processes.end(),
            [pid](const Process &process) { return process.pid == pid; });

        if (processIt != processes.end()) {
#ifdef _WIN32
            if (!TerminateProcess(processIt->handle, 1)) {
                DWORD error = GetLastError();
                spdlog::error(
                    "TerminateProcess failed with error code: {} for PID: {}",
                    error, pid);
                THROW_PROCESS_ERROR("Failed to terminate process.");
            }
            CloseHandle(processIt->handle);
#else
            if (kill(pid, signal) != 0) {
                spdlog::error("Failed to send signal {} to PID {}: {}", signal,
                              pid, strerror(errno));
                THROW_PROCESS_ERROR("Failed to terminate process.");
            }
#endif
            spdlog::info("Process terminated successfully: PID={}, signal={}",
                         pid, signal);
            processes.erase(processIt);
            cv.notify_all();
            return true;
        }
        spdlog::warn("Attempted to terminate non-existent PID: {}", pid);
        return false;
    }

    auto terminateProcessByName(const std::string &name, int signal) -> bool {
        std::unique_lock lock(mtx);
        bool success = false;
        for (auto processIt = processes.begin();
             processIt != processes.end();) {
            if (processIt->name == name) {
                try {
                    if (terminateProcess(processIt->pid, signal)) {
                        success = true;
                    }
                } catch (const ProcessException &e) {
                    spdlog::error("Failed to terminate process {}: {}", name,
                                  e.what());
                }
                processIt = processes.erase(processIt);
            } else {
                ++processIt;
            }
        }
        if (success) {
            spdlog::info("Successfully terminated processes with name: {}",
                         name);
        } else {
            spdlog::warn("No processes found with name: {}", name);
        }
        return success;
    }

    void waitForCompletion() {
        std::unique_lock lock(mtx);
        spdlog::info(
            "Waiting for all managed processes to complete. Current count: {}",
            processes.size());
    }

    auto runScript(const std::string &script, const std::string &identifier,
                   bool isBackground) -> bool {
        return createProcess(script, identifier, isBackground);
    }

    auto monitorProcesses() -> bool {
#ifdef _WIN32
        spdlog::warn("Process monitoring not implemented for Windows platform");
        return false;
#elif defined(__linux__) || defined(__APPLE__)
        std::unique_lock lock(mtx);
        size_t initialCount = processes.size();
        for (auto processIt = processes.begin();
             processIt != processes.end();) {
            int status;
            pid_t result = waitpid(processIt->pid, &status, WNOHANG);
            if (result == 0) {
                ++processIt;
            } else if (result == -1) {
                spdlog::error("Error monitoring PID {}: {}", processIt->pid,
                              strerror(errno));
                processIt = processes.erase(processIt);
            } else {
                spdlog::info("Process terminated naturally: PID={}, status={}",
                             processIt->pid, status);
                processIt = processes.erase(processIt);
                cv.notify_all();
            }
        }
        if (processes.size() != initialCount) {
            spdlog::debug("Process monitoring completed. Active processes: {}",
                          processes.size());
        }
        return true;
#else
        spdlog::warn("Process monitoring not implemented for this platform");
        return false;
#endif
    }

    auto getProcessInfo(int pid) -> Process {
        std::shared_lock lock(mtx);
        auto processIt = std::find_if(
            processes.begin(), processes.end(),
            [pid](const Process &process) { return process.pid == pid; });
        if (processIt != processes.end()) {
            return *processIt;
        }
        spdlog::error("Process with PID {} not found in managed processes",
                      pid);
        THROW_PROCESS_ERROR("Process not found.");
    }

#ifdef _WIN32
    auto getProcessHandle(int pid) const -> void * {
        std::shared_lock lock(mtx);
        auto processIt = std::find_if(
            processes.begin(), processes.end(),
            [pid](const Process &process) { return process.pid == pid; });
        if (processIt != processes.end()) {
            return processIt->handle;
        }
        spdlog::error("Process handle for PID {} not found", pid);
        THROW_PROCESS_ERROR("Process handle not found.");
    }
#else
    static auto getProcFilePath(int pid, const std::string &file)
        -> std::string {
        std::string path = "/proc/" + std::to_string(pid) + "/" + file;
        if (access(path.c_str(), F_OK) != 0) {
            spdlog::error("Process file {} not found for PID {}", file, pid);
            THROW_PROCESS_ERROR("Process file path not found.");
        }
        return path;
    }
#endif

    auto getRunningProcesses() const -> std::vector<Process> {
        std::shared_lock lock(mtx);
        return processes;
    }

    int m_maxProcesses;
    std::condition_variable cv;
    std::vector<Process> processes;
    mutable std::shared_timed_mutex mtx;
};

ProcessManager::ProcessManager(int maxProcess)
    : impl(std::make_unique<ProcessManagerImpl>(maxProcess)) {}

ProcessManager::~ProcessManager() = default;

auto ProcessManager::createShared(int maxProcess)
    -> std::shared_ptr<ProcessManager> {
    return std::make_shared<ProcessManager>(maxProcess);
}

auto ProcessManager::createProcess(const std::string &command,
                                   const std::string &identifier,
                                   bool isBackground) -> bool {
    try {
        return impl->createProcess(command, identifier, isBackground);
    } catch (const ProcessException &e) {
        spdlog::error("Failed to create process {}: {}", identifier, e.what());
        throw;
    }
}

auto ProcessManager::terminateProcess(int pid, int signal) -> bool {
    try {
        return impl->terminateProcess(pid, signal);
    } catch (const ProcessException &e) {
        spdlog::error("Failed to terminate PID {}: {}", pid, e.what());
        return false;
    }
}

auto ProcessManager::terminateProcessByName(const std::string &name, int signal)
    -> bool {
    try {
        return impl->terminateProcessByName(name, signal);
    } catch (const ProcessException &e) {
        spdlog::error("Failed to terminate process {}: {}", name, e.what());
        return false;
    }
}

auto ProcessManager::hasProcess(const std::string &identifier) -> bool {
    std::shared_lock lock(impl->mtx);
    return std::any_of(impl->processes.begin(), impl->processes.end(),
                       [&identifier](const Process &process) {
                           return process.name == identifier;
                       });
}

void ProcessManager::waitForCompletion() { impl->waitForCompletion(); }

auto ProcessManager::getRunningProcesses() const -> std::vector<Process> {
    return impl->getRunningProcesses();
}

auto ProcessManager::getProcessOutput(const std::string &identifier)
    -> std::vector<std::string> {
    std::shared_lock lock(impl->mtx);
    auto processIt =
        std::find_if(impl->processes.begin(), impl->processes.end(),
                     [&identifier](const Process &process) {
                         return process.name == identifier;
                     });

    if (processIt != impl->processes.end()) {
        std::vector<std::string> outputLines;
        std::stringstream sss(processIt->output);
        std::string line;

        while (std::getline(sss, line)) {
            outputLines.emplace_back(line);
        }
        spdlog::debug("Retrieved {} lines of output for process: {}",
                      outputLines.size(), identifier);
        return outputLines;
    }
    spdlog::warn("No output found for process identifier: {}", identifier);
    return {};
}

auto ProcessManager::runScript(const std::string &script,
                               const std::string &identifier, bool isBackground)
    -> bool {
    try {
        return impl->runScript(script, identifier, isBackground);
    } catch (const ProcessException &e) {
        spdlog::error("Failed to run script {}: {}", identifier, e.what());
        return false;
    }
}

auto ProcessManager::monitorProcesses() -> bool {
    return impl->monitorProcesses();
}

auto ProcessManager::getProcessInfo(int pid) -> Process {
    try {
        return impl->getProcessInfo(pid);
    } catch (const ProcessException &e) {
        spdlog::error("Failed to get info for PID {}: {}", pid, e.what());
        throw;
    }
}

#ifdef _WIN32
auto ProcessManager::getProcessHandle(int pid) const -> void * {
    try {
        return impl->getProcessHandle(pid);
    } catch (const ProcessException &e) {
        spdlog::error("Failed to get handle for PID {}: {}", pid, e.what());
        throw;
    }
}
#else
auto ProcessManager::getProcFilePath(int pid, const std::string &file)
    -> std::string {
    try {
        return ProcessManagerImpl::getProcFilePath(pid, file);
    } catch (const ProcessException &e) {
        spdlog::error("Failed to get file path for PID {}: {}", pid, e.what());
        throw;
    }
}
#endif

}  // namespace atom::system
