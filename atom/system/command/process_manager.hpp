/*
 * process_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_PROCESS_MANAGER_HPP
#define ATOM_SYSTEM_COMMAND_PROCESS_MANAGER_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Process information structure
 */
struct ProcessInfo {
    int pid = -1;
    std::string name;
    std::string command;
    std::chrono::system_clock::time_point startTime;
    double cpuUsage = 0.0;
    size_t memoryUsage = 0;  // in bytes
    std::string status;
    int parentPid = -1;
    std::vector<int> childPids;
};

/**
 * @brief Process monitoring configuration
 */
struct ProcessMonitorConfig {
    std::chrono::milliseconds updateInterval{1000};
    bool trackCpuUsage = true;
    bool trackMemoryUsage = true;
    bool trackChildProcesses = true;
    size_t maxHistoryEntries = 100;
};

/**
 * @brief Process group management
 */
class ProcessGroup {
public:
    explicit ProcessGroup(const std::string &name);
    ~ProcessGroup();

    void addProcess(int pid);
    void removeProcess(int pid);
    auto getProcesses() const -> std::vector<int>;
    auto getName() const -> const std::string&;
    void killAll(int signal = 15);
    auto getTotalCpuUsage() const -> double;
    auto getTotalMemoryUsage() const -> size_t;

private:
    std::string name_;
    std::vector<int> pids_;
    mutable std::mutex mutex_;
};

/**
 * @brief Get detailed information about a process
 *
 * @param pid The process ID
 * @return ProcessInfo structure with detailed process information
 */
ATOM_NODISCARD auto getProcessInfo(int pid) -> ProcessInfo;

/**
 * @brief Get detailed information about multiple processes
 *
 * @param pids Vector of process IDs
 * @return Vector of ProcessInfo structures
 */
ATOM_NODISCARD auto getProcessesInfo(const std::vector<int> &pids)
    -> std::vector<ProcessInfo>;

/**
 * @brief Monitor processes with real-time updates
 *
 * @param pids Vector of process IDs to monitor
 * @param config Monitoring configuration
 * @param callback Function called with updated process information
 * @return true if monitoring started successfully
 */
auto monitorProcesses(const std::vector<int> &pids,
                     const ProcessMonitorConfig &config,
                     std::function<void(const std::vector<ProcessInfo>&)> callback)
    -> bool;

/**
 * @brief Stop process monitoring
 */
void stopProcessMonitoring();

/**
 * @brief Kill a process by its name with enhanced options.
 *
 * @param processName The name of the process to kill.
 * @param signal The signal to send to the process.
 * @param killChildren Whether to kill child processes as well.
 */
void killProcessByNameEnhanced(const std::string &processName, int signal,
                              bool killChildren = false);

/**
 * @brief Kill a process by its PID with enhanced options.
 *
 * @param pid The PID of the process to kill.
 * @param signal The signal to send to the process.
 * @param killChildren Whether to kill child processes as well.
 */
void killProcessByPIDEnhanced(int pid, int signal, bool killChildren = false);

/**
 * @brief Kill a process by its name.
 *
 * @param processName The name of the process to kill.
 * @param signal The signal to send to the process.
 */
void killProcessByName(const std::string &processName, int signal);

/**
 * @brief Kill a process by its PID.
 *
 * @param pid The PID of the process to kill.
 * @param signal The signal to send to the process.
 */
void killProcessByPID(int pid, int signal);

/**
 * @brief Start a process with enhanced configuration and return detailed info.
 *
 * @param command The command to execute.
 * @param workingDir Working directory for the process.
 * @param envVars Environment variables to set.
 * @return ProcessInfo structure with process details.
 */
ATOM_NODISCARD auto startProcessEnhanced(
    const std::string &command,
    const std::string &workingDir = "",
    const std::vector<std::pair<std::string, std::string>> &envVars = {})
    -> ProcessInfo;

/**
 * @brief Start a process and return the process ID and handle.
 *
 * @param command The command to execute.
 * @return A pair containing the process ID as an integer and the process handle
 * as a void pointer.
 */
auto startProcess(const std::string &command) -> std::pair<int, void *>;

/**
 * @brief Get a list of running processes containing the specified substring.
 *
 * @param substring The substring to search for in process names.
 * @param includeDetails Whether to include detailed process information.
 * @return A vector of ProcessInfo or pairs containing PIDs and process names.
 */
ATOM_NODISCARD auto getProcessesBySubstringEnhanced(
    const std::string &substring, bool includeDetails = false)
    -> std::vector<ProcessInfo>;

/**
 * @brief Get a list of running processes containing the specified substring.
 *
 * @param substring The substring to search for in process names.
 * @return A vector of pairs containing PIDs and process names.
 */
auto getProcessesBySubstring(const std::string &substring)
    -> std::vector<std::pair<int, std::string>>;

/**
 * @brief Check if a process is running
 *
 * @param pid The process ID to check
 * @return true if the process is running
 */
ATOM_NODISCARD auto isProcessRunning(int pid) -> bool;

/**
 * @brief Get system-wide process statistics
 *
 * @return String containing formatted process statistics
 */
ATOM_NODISCARD auto getSystemProcessStats() -> std::string;

/**
 * @brief Create a new process group
 *
 * @param name Name for the process group
 * @return Shared pointer to the ProcessGroup
 */
ATOM_NODISCARD auto createProcessGroup(const std::string &name)
    -> std::shared_ptr<ProcessGroup>;

}  // namespace atom::system

#endif
