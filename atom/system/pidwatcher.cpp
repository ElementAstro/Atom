/*
 * pidwatcher.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-17

Description: PID Watcher

**************************************************/

#include "pidwatcher.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <tlhelp32.h>
#ifdef _MSC_VER
#pragma comment(lib, "pdh.lib")
#endif
// clang-format on
#else
#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "atom/log/loguru.hpp"
#include "atom/utils/string.hpp"

namespace fs = std::filesystem;

namespace atom::system {
PidWatcher::PidWatcher()
    : running_(false),
      monitoring_(false),
      watchdog_healthy_(true),
      monitor_interval_(std::chrono::milliseconds(1000)),
      max_updates_per_second_(10),
      update_count_(0) {
    LOG_F(INFO, "PidWatcher constructor called");
    rate_limit_start_time_ = std::chrono::steady_clock::now();
}

PidWatcher::PidWatcher(const MonitorConfig& config)
    : running_(false),
      monitoring_(false),
      watchdog_healthy_(true),
      global_config_(config),
      monitor_interval_(config.update_interval),
      max_updates_per_second_(10),
      update_count_(0) {
    LOG_F(INFO, "PidWatcher constructor called with config (interval: {} ms)",
          config.update_interval.count());
    rate_limit_start_time_ = std::chrono::steady_clock::now();
}

PidWatcher::~PidWatcher() {
    LOG_F(INFO, "PidWatcher destructor called");
    stop();
}

PidWatcher& PidWatcher::setExitCallback(ProcessCallback callback) {
    LOG_F(INFO, "Setting exit callback");
    std::lock_guard lock(mutex_);
    exit_callback_ = std::move(callback);
    return *this;
}

PidWatcher& PidWatcher::setMonitorFunction(ProcessCallback callback,
                                           std::chrono::milliseconds interval) {
    LOG_F(INFO, "Setting monitor function with interval: {} ms",
          interval.count());
    std::lock_guard lock(mutex_);
    monitor_callback_ = std::move(callback);
    monitor_interval_ = interval;
    return *this;
}

PidWatcher& PidWatcher::setMultiProcessCallback(MultiProcessCallback callback) {
    LOG_F(INFO, "Setting multi-process callback");
    std::lock_guard lock(mutex_);
    multi_process_callback_ = std::move(callback);
    return *this;
}

PidWatcher& PidWatcher::setErrorCallback(ErrorCallback callback) {
    LOG_F(INFO, "Setting error callback");
    std::lock_guard lock(mutex_);
    error_callback_ = std::move(callback);
    return *this;
}

PidWatcher& PidWatcher::setResourceLimitCallback(
    ResourceLimitCallback callback) {
    LOG_F(INFO, "Setting resource limit callback");
    std::lock_guard lock(mutex_);
    resource_limit_callback_ = std::move(callback);
    return *this;
}

PidWatcher& PidWatcher::setProcessCreateCallback(
    ProcessCreateCallback callback) {
    LOG_F(INFO, "Setting process create callback");
    std::lock_guard lock(mutex_);
    process_create_callback_ = std::move(callback);
    return *this;
}

PidWatcher& PidWatcher::setProcessFilter(ProcessFilter filter) {
    LOG_F(INFO, "Setting process filter");
    std::lock_guard lock(mutex_);
    process_filter_ = std::move(filter);
    return *this;
}

auto PidWatcher::getPidByName(const std::string& name) const -> pid_t {
    LOG_F(INFO, "Getting PID by name: {}", name);
#ifdef _WIN32
    DWORD pidList[1024];
    DWORD cbNeeded;
    if (EnumProcesses(pidList, sizeof(pidList), &cbNeeded)) {
        for (unsigned int i = 0; i < cbNeeded / sizeof(DWORD); i++) {
            HANDLE processHandle = OpenProcess(
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pidList[i]);
            if (processHandle != nullptr) {
                char filename[MAX_PATH];
                if (GetModuleFileNameEx(processHandle, nullptr, filename,
                                        MAX_PATH)) {
                    std::string processName = strrchr(filename, '\\') + 1;
                    if (processName == name) {
                        CloseHandle(processHandle);
                        LOG_F(INFO, "Found PID: {} for name: {}", pidList[i],
                              name);
                        return pidList[i];
                    }
                }
                CloseHandle(processHandle);
            }
        }
    }
#else
    DIR* dir = opendir("/proc");
    if (!dir) {
        LOG_F(ERROR, "Failed to open /proc directory");
        if (error_callback_) {
            error_callback_("Failed to open /proc directory", errno);
        }
        return 0;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR) {
            continue;
        }

        // Check if directory name is a number (PID)
        if (!std::all_of(entry->d_name, entry->d_name + strlen(entry->d_name),
                         [](char c) { return std::isdigit(c); })) {
            continue;
        }

        std::string proc_dir_path = std::string("/proc/") + entry->d_name;

        // Check command line
        std::ifstream cmdline_file(proc_dir_path + "/cmdline");
        if (!cmdline_file.is_open()) {
            continue;
        }

        std::string cmdline;
        std::getline(cmdline_file, cmdline);

        // Handle null bytes in cmdline
        std::replace(cmdline.begin(), cmdline.end(), '\0', ' ');

        // Extract executable name from path
        size_t pos = cmdline.find_last_of('/');
        std::string proc_name =
            (pos != std::string::npos) ? cmdline.substr(pos + 1) : cmdline;
        // Trim whitespace
        proc_name = atom::utils::trim(proc_name);

        if (proc_name == name) {
            closedir(dir);
            pid_t found_pid = std::stoi(entry->d_name);
            LOG_F(INFO, "Found PID: {} for name: {}", found_pid, name);
            return found_pid;
        }
    }
    closedir(dir);
#endif
    LOG_F(WARNING, "PID not found for name: {}", name);
    return 0;
}

auto PidWatcher::getPidsByName(const std::string& name) const
    -> std::vector<pid_t> {
    LOG_F(INFO, "Getting all PIDs by name: {}", name);
    std::vector<pid_t> results;

#ifdef _WIN32
    DWORD pidList[1024];
    DWORD cbNeeded;
    if (EnumProcesses(pidList, sizeof(pidList), &cbNeeded)) {
        for (unsigned int i = 0; i < cbNeeded / sizeof(DWORD); i++) {
            HANDLE processHandle = OpenProcess(
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pidList[i]);
            if (processHandle != nullptr) {
                char filename[MAX_PATH];
                if (GetModuleFileNameEx(processHandle, nullptr, filename,
                                        MAX_PATH)) {
                    std::string processName = strrchr(filename, '\\') + 1;
                    if (processName == name) {
                        results.push_back(pidList[i]);
                        LOG_F(INFO, "Found PID: {} for name: {}", pidList[i],
                              name);
                    }
                }
                CloseHandle(processHandle);
            }
        }
    }
#else
    DIR* dir = opendir("/proc");
    if (!dir) {
        LOG_F(ERROR, "Failed to open /proc directory");
        if (error_callback_) {
            error_callback_("Failed to open /proc directory", errno);
        }
        return results;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR) {
            continue;
        }

        // Check if directory name is a number (PID)
        if (!std::all_of(entry->d_name, entry->d_name + strlen(entry->d_name),
                         [](char c) { return std::isdigit(c); })) {
            continue;
        }

        std::string proc_dir_path = std::string("/proc/") + entry->d_name;

        // Check command line
        std::ifstream cmdline_file(proc_dir_path + "/cmdline");
        if (!cmdline_file.is_open()) {
            continue;
        }

        std::string cmdline;
        std::getline(cmdline_file, cmdline);

        // Handle null bytes in cmdline
        std::replace(cmdline.begin(), cmdline.end(), '\0', ' ');

        // Extract executable name from path
        size_t pos = cmdline.find_last_of('/');
        std::string proc_name =
            (pos != std::string::npos) ? cmdline.substr(pos + 1) : cmdline;
        // Trim whitespace
        proc_name = atom::utils::trim(proc_name);

        if (proc_name == name) {
            pid_t found_pid = std::stoi(entry->d_name);
            results.push_back(found_pid);
            LOG_F(INFO, "Found PID: {} for name: {}", found_pid, name);
        }
    }
    closedir(dir);
#endif

    LOG_F(INFO, "Found {} processes with name: {}", results.size(), name);
    return results;
}

auto PidWatcher::getProcessInfo(pid_t pid) -> std::optional<ProcessInfo> {
    std::lock_guard lock(mutex_);

    auto it = monitored_processes_.find(pid);
    if (it != monitored_processes_.end()) {
        return it->second;
    }

    // Process not in our monitored list, try to get info directly
    ProcessInfo info;
    info.pid = pid;

#ifdef _WIN32
    HANDLE process_handle =
        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (process_handle == NULL) {
        LOG_F(WARNING, "Failed to open process {}", pid);
        return std::nullopt;
    }

    // Get process name
    char process_name[MAX_PATH];
    if (GetModuleFileNameEx(process_handle, NULL, process_name, MAX_PATH) ==
        0) {
        LOG_F(WARNING, "Failed to get process name for {}", pid);
        CloseHandle(process_handle);
        return std::nullopt;
    }
    info.name = strrchr(process_name, '\\') + 1;

    // Get command line
    info.command_line = getProcessCommandLine(pid);

    // Get parent PID using toolhelp snapshot
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe = {};
        pe.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(h, &pe)) {
            do {
                if (pe.th32ProcessID == pid) {
                    info.parent_pid = pe.th32ParentProcessID;
                    break;
                }
            } while (Process32Next(h, &pe));
        }
        CloseHandle(h);
    }

    // Get thread count
    h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (h != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te = {};
        te.dwSize = sizeof(THREADENTRY32);
        info.thread_count = 0;
        if (Thread32First(h, &te)) {
            do {
                if (te.th32OwnerProcessID == pid) {
                    info.thread_count++;
                }
            } while (Thread32Next(h, &te));
        }
        CloseHandle(h);
    }

    // Check if process is running
    DWORD exit_code;
    GetExitCodeProcess(process_handle, &exit_code);
    info.running = (exit_code == STILL_ACTIVE);
    info.status = info.running ? ProcessStatus::RUNNING : ProcessStatus::DEAD;

    // Get memory usage
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(process_handle, (PROCESS_MEMORY_COUNTERS*)&pmc,
                             sizeof(pmc))) {
        info.memory_usage = pmc.WorkingSetSize / 1024;  // Convert to KB
        info.virtual_memory = pmc.PrivateUsage / 1024;  // Convert to KB
    }

    // Get process priority
    info.priority = GetPriorityClass(process_handle);

    CloseHandle(process_handle);

    // Get I/O stats
    info.io_stats = getProcessIOStats(pid);

    // Get CPU usage
    info.cpu_usage = getProcessCpuUsage(pid);

    // Get process start time
    FILETIME creation_time, exit_time, kernel_time, user_time;
    process_handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process_handle != NULL) {
        if (GetProcessTimes(process_handle, &creation_time, &exit_time,
                            &kernel_time, &user_time)) {
            ULARGE_INTEGER li;
            li.LowPart = creation_time.dwLowDateTime;
            li.HighPart = creation_time.dwHighDateTime;
            // Convert Windows FILETIME to Unix epoch time
            auto epoch_diff =
                116444736000000000ULL;  // Difference between Windows epoch and
                                        // Unix epoch
            auto unix_time =
                (li.QuadPart - epoch_diff) / 10000000ULL;  // Convert to seconds
            info.start_time = std::chrono::system_clock::from_time_t(unix_time);

            // Calculate uptime
            auto now = std::chrono::system_clock::now();
            info.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - info.start_time);
        }
        CloseHandle(process_handle);
    }

    // Get child processes
    info.child_processes = getChildProcesses(pid);

#else
    // Check if process exists
    std::string proc_path = "/proc/" + std::to_string(pid);
    if (!fs::exists(proc_path)) {
        return std::nullopt;
    }

    info.running = true;

    // Get process name from command line
    std::ifstream cmdline_file(proc_path + "/cmdline");
    if (cmdline_file.is_open()) {
        std::string cmdline;
        std::getline(cmdline_file, cmdline);

        // Store full command line
        info.command_line = cmdline;

        // Replace null bytes with spaces for display
        std::string display_cmdline = cmdline;
        std::replace(display_cmdline.begin(), display_cmdline.end(), '\0', ' ');

        // Extract executable name from cmdline
        size_t pos = display_cmdline.find_last_of('/');
        info.name = (pos != std::string::npos) ? display_cmdline.substr(pos + 1)
                                               : display_cmdline;
        info.name = atom::utils::trim(info.name);
    }

    // Get process status, parent PID and other details
    std::ifstream status_file(proc_path + "/status");
    if (status_file.is_open()) {
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.compare(0, 6, "VmRSS:") == 0) {
                std::istringstream iss(line.substr(6));
                iss >> info.memory_usage;
            } else if (line.compare(0, 7, "VmSize:") == 0) {
                std::istringstream iss(line.substr(7));
                iss >> info.virtual_memory;
            } else if (line.compare(0, 5, "PPid:") == 0) {
                std::istringstream iss(line.substr(5));
                iss >> info.parent_pid;
            } else if (line.compare(0, 8, "Threads:") == 0) {
                std::istringstream iss(line.substr(8));
                iss >> info.thread_count;
            } else if (line.compare(0, 6, "State:") == 0) {
                char state_char = line.substr(7, 1)[0];
                switch (state_char) {
                    case 'R':
                        info.status = ProcessStatus::RUNNING;
                        break;
                    case 'S':
                        info.status = ProcessStatus::SLEEPING;
                        break;
                    case 'D':
                        info.status = ProcessStatus::WAITING;
                        break;
                    case 'Z':
                        info.status = ProcessStatus::ZOMBIE;
                        break;
                    case 'T':
                        info.status = ProcessStatus::STOPPED;
                        break;
                    case 'X':
                        info.status = ProcessStatus::DEAD;
                        break;
                    default:
                        info.status = ProcessStatus::UNKNOWN;
                }
            }
        }
    }

    // Get username
    info.username = getProcessUsername(pid);

    // Get shared memory
    std::ifstream smaps_file(proc_path + "/smaps");
    if (smaps_file.is_open()) {
        std::string line;
        while (std::getline(smaps_file, line)) {
            if (line.compare(0, 8, "Shared_C") == 0) {
                std::istringstream iss(
                    line.substr(line.find_first_of("0123456789")));
                size_t shared;
                iss >> shared;
                info.shared_memory += shared;
            }
        }
    }

    // Get process priority (nice value)
    std::ifstream stat_file(proc_path + "/stat");
    if (stat_file.is_open()) {
        std::string stat_content;
        std::getline(stat_file, stat_content);

        // Parse stat file - nice value is the 19th field
        std::istringstream iss(stat_content);
        std::string value;
        for (int i = 0; i < 18; i++) {
            iss >> value;
        }
        iss >> info.priority;  // nice value
    }

    // Get process start time
    struct stat stat_buf;
    if (stat(proc_path.c_str(), &stat_buf) == 0) {
        info.start_time =
            std::chrono::system_clock::from_time_t(stat_buf.st_ctime);

        // Calculate uptime
        auto now = std::chrono::system_clock::now();
        info.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - info.start_time);
    }

    // Get CPU usage
    info.cpu_usage = getProcessCpuUsage(pid);

    // Get I/O stats
    info.io_stats = getProcessIOStats(pid);

    // Get child processes
    info.child_processes = getChildProcesses(pid);
#endif

    return info;
}

auto PidWatcher::getAllProcesses() -> std::vector<ProcessInfo> {
    std::vector<ProcessInfo> result;

#ifdef _WIN32
    DWORD process_ids[1024], bytes_needed, num_processes;
    if (!EnumProcesses(process_ids, sizeof(process_ids), &bytes_needed)) {
        LOG_F(ERROR, "Failed to enumerate processes");
        return result;
    }

    num_processes = bytes_needed / sizeof(DWORD);
    for (unsigned int i = 0; i < num_processes; i++) {
        if (process_ids[i] != 0) {
            auto info = getProcessInfo(process_ids[i]);
            if (info) {
                result.push_back(*info);
            }
        }
    }
#else
    DIR* dir = opendir("/proc");
    if (!dir) {
        LOG_F(ERROR, "Failed to open /proc directory");
        return result;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip non-numeric directories (not processes)
        if (entry->d_type != DT_DIR || !std::isdigit(entry->d_name[0])) {
            continue;
        }

        pid_t pid = std::stoi(entry->d_name);
        auto info = getProcessInfo(pid);
        if (info) {
            result.push_back(*info);
        }
    }

    closedir(dir);
#endif

    return result;
}

auto PidWatcher::getChildProcesses(pid_t pid) const -> std::vector<pid_t> {
    std::vector<pid_t> children;

#ifdef _WIN32
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe = {};
        pe.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(h, &pe)) {
            do {
                if (pe.th32ParentProcessID == pid) {
                    children.push_back(pe.th32ProcessID);
                }
            } while (Process32Next(h, &pe));
        }
        CloseHandle(h);
    }
#else
    DIR* dir = opendir("/proc");
    if (!dir) {
        return children;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip non-numeric directories (not processes)
        if (entry->d_type != DT_DIR || !std::isdigit(entry->d_name[0])) {
            continue;
        }

        pid_t current_pid = std::stoi(entry->d_name);
        std::string status_path =
            "/proc/" + std::string(entry->d_name) + "/status";

        std::ifstream status_file(status_path);
        if (status_file.is_open()) {
            std::string line;
            while (std::getline(status_file, line)) {
                if (line.compare(0, 5, "PPid:") == 0) {
                    std::istringstream iss(line.substr(5));
                    pid_t ppid;
                    iss >> ppid;

                    if (ppid == pid) {
                        children.push_back(current_pid);
                    }
                    break;
                }
            }
        }
    }

    closedir(dir);
#endif

    return children;
}

auto PidWatcher::start(const std::string& name, const MonitorConfig* config)
    -> bool {
    LOG_F(INFO, "Starting PidWatcher for process name: {}", name);
    std::lock_guard lock(mutex_);

    if (running_ && !monitored_processes_.empty()) {
        LOG_F(INFO, "Watcher already running with {} processes",
              monitored_processes_.size());
    }

    pid_t pid = getPidByName(name);
    if (pid == 0) {
        LOG_F(ERROR, "Failed to get PID for {}", name);
        if (error_callback_) {
            error_callback_("Failed to get PID for " + name, ESRCH);
        }
        return false;
    }

    // Check if we're already monitoring this process
    if (monitored_processes_.find(pid) != monitored_processes_.end()) {
        LOG_F(INFO, "Already monitoring process {} ({})", pid, name);
        return true;
    }

    // Initialize process info
    ProcessInfo info;
    info.pid = pid;
    info.name = name;
    info.running = true;
    info.start_time = std::chrono::system_clock::now();

    // Get more detailed info right away
    auto detailed_info = getProcessInfo(pid);
    if (detailed_info) {
        info = *detailed_info;
    }

    // Apply process-specific configuration if provided
    if (config) {
        process_configs_[pid] = *config;
    }

    monitored_processes_[pid] = info;

    // Set primary PID if this is the first process
    if (monitored_processes_.size() == 1) {
        pid_ = pid;
    }

    if (!running_) {
        running_ = true;
        monitoring_ = true;
        watchdog_healthy_ = true;

        // Start all monitoring threads
#if __cplusplus >= 202002L
        monitor_thread_ = std::jthread(&PidWatcher::monitorThread, this);
        exit_thread_ = std::jthread(&PidWatcher::exitThread, this);
        multi_monitor_thread_ =
            std::jthread(&PidWatcher::multiMonitorThread, this);
        resource_monitor_thread_ =
            std::jthread(&PidWatcher::resourceMonitorThread, this);
        auto_restart_thread_ =
            std::jthread(&PidWatcher::autoRestartThread, this);
        watchdog_thread_ = std::jthread(&PidWatcher::watchdogThread, this);
#else
        monitor_thread_ = std::thread(&PidWatcher::monitorThread, this);
        exit_thread_ = std::thread(&PidWatcher::exitThread, this);
        multi_monitor_thread_ =
            std::thread(&PidWatcher::multiMonitorThread, this);
        resource_monitor_thread_ =
            std::thread(&PidWatcher::resourceMonitorThread, this);
        auto_restart_thread_ =
            std::thread(&PidWatcher::autoRestartThread, this);
        watchdog_thread_ = std::thread(&PidWatcher::watchdogThread, this);
#endif
    } else {
        // Notify threads about new process
        monitor_cv_.notify_one();
        multi_monitor_cv_.notify_one();
        resource_monitor_cv_.notify_one();
    }

    LOG_F(INFO, "PidWatcher started for process name: {}", name);
    return true;
}

auto PidWatcher::startByPid(pid_t pid, const MonitorConfig* config) -> bool {
    LOG_F(INFO, "Starting PidWatcher for PID: {}", pid);
    std::lock_guard lock(mutex_);

    if (running_ && !monitored_processes_.empty()) {
        LOG_F(INFO, "Watcher already running with {} processes",
              monitored_processes_.size());
    }

    // Check if process exists
    if (!isProcessRunning(pid)) {
        LOG_F(ERROR, "Process with PID {} does not exist", pid);
        if (error_callback_) {
            error_callback_(
                "Process with PID " + std::to_string(pid) + " does not exist",
                ESRCH);
        }
        return false;
    }

    // Check if we're already monitoring this process
    if (monitored_processes_.find(pid) != monitored_processes_.end()) {
        LOG_F(INFO, "Already monitoring process {}", pid);
        return true;
    }

    // Initialize process info
    auto info_opt = getProcessInfo(pid);
    if (!info_opt) {
        LOG_F(ERROR, "Failed to get info for PID {}", pid);
        if (error_callback_) {
            error_callback_("Failed to get info for PID " + std::to_string(pid),
                            EINVAL);
        }
        return false;
    }

    ProcessInfo info = *info_opt;

    // Apply process-specific configuration if provided
    if (config) {
        process_configs_[pid] = *config;
    }

    monitored_processes_[pid] = info;

    // Set primary PID if this is the first process
    if (monitored_processes_.size() == 1) {
        pid_ = pid;
    }

    if (!running_) {
        running_ = true;
        monitoring_ = true;
        watchdog_healthy_ = true;

        // Start all monitoring threads
#if __cplusplus >= 202002L
        monitor_thread_ = std::jthread(&PidWatcher::monitorThread, this);
        exit_thread_ = std::jthread(&PidWatcher::exitThread, this);
        multi_monitor_thread_ =
            std::jthread(&PidWatcher::multiMonitorThread, this);
        resource_monitor_thread_ =
            std::jthread(&PidWatcher::resourceMonitorThread, this);
        auto_restart_thread_ =
            std::jthread(&PidWatcher::autoRestartThread, this);
        watchdog_thread_ = std::jthread(&PidWatcher::watchdogThread, this);
#else
        monitor_thread_ = std::thread(&PidWatcher::monitorThread, this);
        exit_thread_ = std::thread(&PidWatcher::exitThread, this);
        multi_monitor_thread_ =
            std::thread(&PidWatcher::multiMonitorThread, this);
        resource_monitor_thread_ =
            std::thread(&PidWatcher::resourceMonitorThread, this);
        auto_restart_thread_ =
            std::thread(&PidWatcher::autoRestartThread, this);
        watchdog_thread_ = std::thread(&PidWatcher::watchdogThread, this);
#endif
    } else {
        // Notify threads about new process
        monitor_cv_.notify_one();
        multi_monitor_cv_.notify_one();
        resource_monitor_cv_.notify_one();
    }

    LOG_F(INFO, "PidWatcher started for PID: {}", pid);
    return true;
}

auto PidWatcher::startMultiple(const std::vector<std::string>& process_names,
                               const MonitorConfig* config) -> size_t {
    LOG_F(INFO, "Starting PidWatcher for multiple processes (count: {})",
          process_names.size());
    std::lock_guard lock(mutex_);

    size_t success_count = 0;

    for (const auto& name : process_names) {
        pid_t pid = getPidByName(name);
        if (pid == 0) {
            LOG_F(WARNING, "Failed to get PID for {}", name);
            if (error_callback_) {
                error_callback_("Failed to get PID for " + name, ESRCH);
            }
            continue;
        }

        // Skip already monitored processes
        if (monitored_processes_.find(pid) != monitored_processes_.end()) {
            LOG_F(INFO, "Already monitoring process {} ({})", pid, name);
            success_count++;
            continue;
        }

        // Initialize process info
        ProcessInfo info;
        info.pid = pid;
        info.name = name;
        info.running = true;
        info.start_time = std::chrono::system_clock::now();

        // Get more detailed info right away
        auto detailed_info = getProcessInfo(pid);
        if (detailed_info) {
            info = *detailed_info;
        }

        // Apply process-specific configuration if provided
        if (config) {
            process_configs_[pid] = *config;
        }

        // Apply filter if set
        if (process_filter_ && !process_filter_(info)) {
            LOG_F(INFO, "Process {}/{} filtered out by custom filter", pid,
                  name);
            continue;
        }

        monitored_processes_[pid] = info;
        success_count++;

        // Set primary PID if this is the first process
        if (monitored_processes_.size() == 1) {
            pid_ = pid;
        }
    }

    if (success_count > 0 && !running_) {
        running_ = true;
        monitoring_ = true;
        watchdog_healthy_ = true;

        // Start all monitoring threads
#if __cplusplus >= 202002L
        monitor_thread_ = std::jthread(&PidWatcher::monitorThread, this);
        exit_thread_ = std::jthread(&PidWatcher::exitThread, this);
        multi_monitor_thread_ =
            std::jthread(&PidWatcher::multiMonitorThread, this);
        resource_monitor_thread_ =
            std::jthread(&PidWatcher::resourceMonitorThread, this);
        auto_restart_thread_ =
            std::jthread(&PidWatcher::autoRestartThread, this);
        watchdog_thread_ = std::jthread(&PidWatcher::watchdogThread, this);
#else
        monitor_thread_ = std::thread(&PidWatcher::monitorThread, this);
        exit_thread_ = std::thread(&PidWatcher::exitThread, this);
        multi_monitor_thread_ =
            std::thread(&PidWatcher::multiMonitorThread, this);
        resource_monitor_thread_ =
            std::thread(&PidWatcher::resourceMonitorThread, this);
        auto_restart_thread_ =
            std::thread(&PidWatcher::autoRestartThread, this);
        watchdog_thread_ = std::thread(&PidWatcher::watchdogThread, this);
#endif
    } else if (success_count > 0) {
        // Notify threads about new processes
        monitor_cv_.notify_one();
        multi_monitor_cv_.notify_one();
        resource_monitor_cv_.notify_one();
    }

    LOG_F(INFO, "Started monitoring {} processes out of {}", success_count,
          process_names.size());

    return success_count;
}

void PidWatcher::stop() {
    LOG_F(INFO, "Stopping PidWatcher");

    {
        std::lock_guard lock(mutex_);

        if (!running_) {
            LOG_F(INFO, "PidWatcher is not running");
            return;
        }

        running_ = false;
        monitoring_ = false;
        watchdog_healthy_ = false;

        // Notify all threads to exit
        exit_cv_.notify_all();
        monitor_cv_.notify_all();
        multi_monitor_cv_.notify_all();
        resource_monitor_cv_.notify_all();
        auto_restart_cv_.notify_all();
        watchdog_cv_.notify_all();

        monitored_processes_.clear();
        process_configs_.clear();
    }

    // Join all threads
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    if (exit_thread_.joinable()) {
        exit_thread_.join();
    }
    if (multi_monitor_thread_.joinable()) {
        multi_monitor_thread_.join();
    }
    if (resource_monitor_thread_.joinable()) {
        resource_monitor_thread_.join();
    }
    if (auto_restart_thread_.joinable()) {
        auto_restart_thread_.join();
    }
    if (watchdog_thread_.joinable()) {
        watchdog_thread_.join();
    }

    LOG_F(INFO, "PidWatcher stopped");
}

bool PidWatcher::stopProcess(pid_t pid) {
    std::lock_guard lock(mutex_);

    auto it = monitored_processes_.find(pid);
    if (it == monitored_processes_.end()) {
        LOG_F(WARNING, "Process {} is not being monitored", pid);
        return false;
    }

    LOG_F(INFO, "Stopping monitoring for process {} ({})", pid,
          it->second.name);

    // Remove from configurations
    process_configs_.erase(pid);

    // Remove from monitored processes
    monitored_processes_.erase(it);

    // If this was the primary process, update primary PID
    if (pid_ == pid && !monitored_processes_.empty()) {
        pid_ = monitored_processes_.begin()->first;
        LOG_F(INFO, "Updated primary PID to {}", pid_);
    }

    // If this was the last process, stop all threads
    if (monitored_processes_.empty()) {
        running_ = false;
        monitoring_ = false;

        // Notify all threads to exit
        exit_cv_.notify_all();
        monitor_cv_.notify_all();
        multi_monitor_cv_.notify_all();
        resource_monitor_cv_.notify_all();
        auto_restart_cv_.notify_all();
    }

    return true;
}

auto PidWatcher::switchToProcess(const std::string& name) -> bool {
    std::lock_guard lock(mutex_);

    if (!running_) {
        LOG_F(ERROR, "Not running.");
        return false;
    }

    pid_t new_pid = getPidByName(name);
    if (new_pid == 0) {
        LOG_F(ERROR, "Failed to get PID for {}", name);
        if (error_callback_) {
            error_callback_("Failed to get PID for " + name, ESRCH);
        }
        return false;
    }

    // If already monitoring this process in multi-mode, make it primary
    if (monitored_processes_.find(new_pid) != monitored_processes_.end()) {
        LOG_F(INFO, "Already monitoring process {} ({}), making primary",
              new_pid, name);
        pid_ = new_pid;
        return true;
    }

    // Initialize process info
    ProcessInfo info;
    info.pid = new_pid;
    info.name = name;
    info.running = true;
    info.start_time = std::chrono::system_clock::now();

    // Get more detailed info right away
    auto detailed_info = getProcessInfo(new_pid);
    if (detailed_info) {
        info = *detailed_info;
    }

    // Update primary PID
    pid_ = new_pid;

    // Add to monitored processes
    monitored_processes_[new_pid] = info;

    // Notify threads
    monitor_cv_.notify_one();
    multi_monitor_cv_.notify_one();
    resource_monitor_cv_.notify_one();

    LOG_F(INFO, "PidWatcher switched to process name: {}", name);
    return true;
}

auto PidWatcher::switchToProcessById(pid_t pid) -> bool {
    std::lock_guard lock(mutex_);

    if (!running_) {
        LOG_F(ERROR, "Not running.");
        return false;
    }

    if (!isProcessRunning(pid)) {
        LOG_F(ERROR, "Process {} is not running", pid);
        if (error_callback_) {
            error_callback_(
                "Process " + std::to_string(pid) + " is not running", ESRCH);
        }
        return false;
    }

    // If already monitoring this process, make it primary
    if (monitored_processes_.find(pid) != monitored_processes_.end()) {
        LOG_F(INFO, "Already monitoring process {}, making primary", pid);
        pid_ = pid;
        return true;
    }

    // Get process info
    auto info_opt = getProcessInfo(pid);
    if (!info_opt) {
        LOG_F(ERROR, "Failed to get info for process {}", pid);
        if (error_callback_) {
            error_callback_(
                "Failed to get info for process " + std::to_string(pid),
                EINVAL);
        }
        return false;
    }

    // Update primary PID
    pid_ = pid;

    // Add to monitored processes
    monitored_processes_[pid] = *info_opt;

    // Notify threads
    monitor_cv_.notify_one();
    multi_monitor_cv_.notify_one();
    resource_monitor_cv_.notify_one();

    LOG_F(INFO, "PidWatcher switched to process ID: {}", pid);
    return true;
}

bool PidWatcher::isActive() const {
    std::lock_guard lock(mutex_);
    return running_ && !monitored_processes_.empty();
}

bool PidWatcher::isMonitoring(pid_t pid) const {
    std::lock_guard lock(mutex_);
    return monitored_processes_.find(pid) != monitored_processes_.end();
}

bool PidWatcher::isProcessRunning(pid_t pid) const {
#ifdef _WIN32
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (process == NULL) {
        return false;
    }

    DWORD status = WaitForSingleObject(process, 0);
    CloseHandle(process);
    return status == WAIT_TIMEOUT;
#else
    // Send signal 0 to check if process exists
    return kill(pid, 0) == 0;
#endif
}

double PidWatcher::getProcessCpuUsage(pid_t pid) const {
#ifdef _WIN32
    // Windows implementation using PDH
    static PDH_HQUERY cpuQuery = NULL;
    static PDH_HCOUNTER cpuTotal = NULL;

    if (cpuQuery == NULL) {
        PdhOpenQuery(NULL, 0, &cpuQuery);
        std::string counterPath =
            "\\Process(" + std::to_string(pid) + ")\\% Processor Time";
        PdhAddEnglishCounter(cpuQuery, counterPath.c_str(), 0, &cpuTotal);
        PdhCollectQueryData(cpuQuery);

        // Need to wait for second call to get an actual value
        return 0.0;
    }

    PDH_FMT_COUNTERVALUE counterVal;
    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);

    return counterVal.doubleValue;
#else
    std::string proc_path = "/proc/" + std::to_string(pid);

    // Check if process exists
    if (!fs::exists(proc_path)) {
        return -1.0;
    }

    std::lock_guard lock(mutex_);
    auto it = cpu_usage_data_.find(pid);
    bool first_call = (it == cpu_usage_data_.end());

    // Read process stats
    std::ifstream stat_file(proc_path + "/stat");
    if (!stat_file.is_open()) {
        return -1.0;
    }

    std::string line;
    std::getline(stat_file, line);
    std::istringstream iss(line);

    std::vector<std::string> stat_values;
    std::string value;
    while (iss >> value) {
        stat_values.push_back(value);
    }

    if (stat_values.size() < 17) {
        return -1.0;
    }

    // Get user and system time
    unsigned long long utime = std::stoull(stat_values[13]);
    unsigned long long stime = std::stoull(stat_values[14]);
    unsigned long long total_time = utime + stime;

    // Read system stats
    std::ifstream proc_stat("/proc/stat");
    if (!proc_stat.is_open()) {
        return -1.0;
    }

    std::getline(proc_stat, line);
    iss = std::istringstream(line);

    std::string cpu_label;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    iss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >>
        softirq >> steal;

    unsigned long long total_cpu_time =
        user + nice + system + idle + iowait + irq + softirq + steal;

    auto now = std::chrono::steady_clock::now();

    if (first_call) {
        // First call, just store values
        CPUUsageData data;
        data.lastTotalUser = user;
        data.lastTotalUserLow = nice;
        data.lastTotalSys = system;
        data.lastTotalIdle = idle;
        data.last_update = now;
        cpu_usage_data_[pid] = data;
        return 0.0;
    } else {
        // Calculate CPU usage
        CPUUsageData& last = cpu_usage_data_[pid];

        // If update was too recent, return last value
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last.last_update)
                .count() < 200) {
            return 0.0;
        }

        double percent = 0.0;

        if (total_cpu_time > (last.lastTotalUser + last.lastTotalUserLow +
                              last.lastTotalSys + last.lastTotalIdle)) {
            percent =
                (total_time * 100.0) /
                (total_cpu_time - (last.lastTotalUser + last.lastTotalUserLow +
                                   last.lastTotalSys + last.lastTotalIdle));
        }

        // Update stored values
        last.lastTotalUser = user;
        last.lastTotalUserLow = nice;
        last.lastTotalSys = system;
        last.lastTotalIdle = idle;
        last.last_update = now;

        return percent;
    }
#endif
}

size_t PidWatcher::getProcessMemoryUsage(pid_t pid) const {
#ifdef _WIN32
    HANDLE process =
        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (process == NULL) {
        return 0;
    }

    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(process, &pmc, sizeof(pmc))) {
        CloseHandle(process);
        return pmc.WorkingSetSize / 1024;  // Convert to KB
    }

    CloseHandle(process);
    return 0;
#else
    std::string proc_path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream status_file(proc_path);
    if (!status_file.is_open()) {
        return 0;
    }

    std::string line;
    while (std::getline(status_file, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            std::istringstream iss(line.substr(6));
            size_t memory_kb;
            iss >> memory_kb;
            return memory_kb;
        }
    }

    return 0;
#endif
}

unsigned int PidWatcher::getProcessThreadCount(pid_t pid) const {
#ifdef _WIN32
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }

    unsigned int thread_count = 0;
    THREADENTRY32 te = {};
    te.dwSize = sizeof(THREADENTRY32);

    if (Thread32First(h, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                thread_count++;
            }
        } while (Thread32Next(h, &te));
    }

    CloseHandle(h);
    return thread_count;
#else
    std::string proc_path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream status_file(proc_path);
    if (!status_file.is_open()) {
        return 0;
    }

    std::string line;
    while (std::getline(status_file, line)) {
        if (line.compare(0, 8, "Threads:") == 0) {
            std::istringstream iss(line.substr(8));
            unsigned int thread_count;
            iss >> thread_count;
            return thread_count;
        }
    }

    return 0;
#endif
}

ProcessIOStats PidWatcher::getProcessIOStats(pid_t pid) {
    ProcessIOStats stats;
    std::lock_guard lock(mutex_);

#ifdef _WIN32
    // Windows doesn't have a straightforward API for per-process I/O stats
    // We can use IO_COUNTERS from GetProcessIoCounters but it may not work on
    // all Windows versions
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process == NULL) {
        return stats;
    }

    IO_COUNTERS io_counters;
    if (GetProcessIoCounters(process, &io_counters)) {
        stats.read_bytes = io_counters.ReadTransferCount;
        stats.write_bytes = io_counters.WriteTransferCount;

        // Calculate rates by comparing with previous measurements
        auto it = prev_io_stats_.find(pid);
        if (it != prev_io_stats_.end()) {
            auto now = std::chrono::steady_clock::now();
            auto last = it->second;
            auto time_diff =
                now -
                std::chrono::steady_clock::now();  // Need to store timestamps

            double seconds = std::chrono::duration<double>(time_diff).count();
            if (seconds > 0) {
                stats.read_rate =
                    (stats.read_bytes - last.read_bytes) / seconds;
                stats.write_rate =
                    (stats.write_bytes - last.write_bytes) / seconds;
            }
        }

        prev_io_stats_[pid] = stats;
    }

    CloseHandle(process);
#else
    std::string io_path = "/proc/" + std::to_string(pid) + "/io";
    std::ifstream io_file(io_path);
    if (!io_file.is_open()) {
        return stats;
    }

    std::string line;
    uint64_t prev_read = 0, prev_write = 0;

    // Get previous values if they exist
    auto it = prev_io_stats_.find(pid);
    if (it != prev_io_stats_.end()) {
        prev_read = it->second.read_bytes;
        prev_write = it->second.write_bytes;
    }

    // Parse current IO values
    while (std::getline(io_file, line)) {
        if (line.compare(0, 10, "read_bytes") == 0) {
            std::istringstream iss(
                line.substr(line.find_first_of("0123456789")));
            iss >> stats.read_bytes;
        } else if (line.compare(0, 11, "write_bytes") == 0) {
            std::istringstream iss(
                line.substr(line.find_first_of("0123456789")));
            iss >> stats.write_bytes;
        }
    }

    // Calculate rates
    auto now = std::chrono::steady_clock::now();
    auto last_it = last_update_time_.find(pid);
    if (last_it != last_update_time_.end()) {
        auto time_diff = now - last_it->second;
        double seconds = std::chrono::duration<double>(time_diff).count();

        if (seconds > 0 && prev_read <= stats.read_bytes &&
            prev_write <= stats.write_bytes) {
            stats.read_rate = (stats.read_bytes - prev_read) / seconds;
            stats.write_rate = (stats.write_bytes - prev_write) / seconds;
        }
    }

    // Store for next calculation
    prev_io_stats_[pid] = stats;
    last_update_time_[pid] = now;
#endif

    return stats;
}

ProcessStatus PidWatcher::getProcessStatus(pid_t pid) const {
#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process == NULL) {
        return ProcessStatus::UNKNOWN;
    }

    DWORD exit_code;
    if (!GetExitCodeProcess(process, &exit_code)) {
        CloseHandle(process);
        return ProcessStatus::UNKNOWN;
    }

    CloseHandle(process);
    return (exit_code == STILL_ACTIVE) ? ProcessStatus::RUNNING
                                       : ProcessStatus::DEAD;
#else
    std::string status_path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream status_file(status_path);
    if (!status_file.is_open()) {
        return ProcessStatus::UNKNOWN;
    }

    std::string line;
    while (std::getline(status_file, line)) {
        if (line.compare(0, 6, "State:") == 0) {
            char state_char = line.substr(7, 1)[0];
            switch (state_char) {
                case 'R':
                    return ProcessStatus::RUNNING;
                case 'S':
                    return ProcessStatus::SLEEPING;
                case 'D':
                    return ProcessStatus::WAITING;
                case 'Z':
                    return ProcessStatus::ZOMBIE;
                case 'T':
                    return ProcessStatus::STOPPED;
                case 'X':
                    return ProcessStatus::DEAD;
                default:
                    return ProcessStatus::UNKNOWN;
            }
        }
    }

    return ProcessStatus::UNKNOWN;
#endif
}

std::chrono::milliseconds PidWatcher::getProcessUptime(pid_t pid) const {
#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process == NULL) {
        return std::chrono::milliseconds(0);
    }

    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (!GetProcessTimes(process, &creation_time, &exit_time, &kernel_time,
                         &user_time)) {
        CloseHandle(process);
        return std::chrono::milliseconds(0);
    }

    CloseHandle(process);

    // Convert FILETIME to system time
    ULARGE_INTEGER create_time_li;
    create_time_li.LowPart = creation_time.dwLowDateTime;
    create_time_li.HighPart = creation_time.dwHighDateTime;

    // Get current system time
    FILETIME current_time;
    GetSystemTimeAsFileTime(&current_time);

    ULARGE_INTEGER current_time_li;
    current_time_li.LowPart = current_time.dwLowDateTime;
    current_time_li.HighPart = current_time.dwHighDateTime;

    // Calculate difference in 100-nanosecond intervals
    uint64_t diff = current_time_li.QuadPart - create_time_li.QuadPart;

    // Convert to milliseconds (1 unit = 100ns, so divide by 10000 for
    // milliseconds)
    return std::chrono::milliseconds(diff / 10000);
#else
    std::string stat_path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream stat_file(stat_path);
    if (!stat_file.is_open()) {
        return std::chrono::milliseconds(0);
    }

    std::string content;
    std::getline(stat_file, content);
    stat_file.close();

    // Get system uptime
    std::ifstream uptime_file("/proc/uptime");
    if (!uptime_file.is_open()) {
        return std::chrono::milliseconds(0);
    }

    double system_uptime;
    uptime_file >> system_uptime;
    uptime_file.close();

    // Parse stat file to get start time (field 22)
    std::istringstream iss(content);
    std::string field;

    for (int i = 1; i < 22; i++) {
        iss >> field;
    }

    unsigned long long start_time;
    iss >> start_time;

    // Convert clock ticks to seconds
    long clock_ticks = sysconf(_SC_CLK_TCK);
    if (clock_ticks <= 0) {
        return std::chrono::milliseconds(0);
    }

    double start_seconds = static_cast<double>(start_time) / clock_ticks;
    double uptime_seconds = system_uptime - start_seconds;

    return std::chrono::milliseconds(
        static_cast<long long>(uptime_seconds * 1000));
#endif
}

pid_t PidWatcher::launchProcess(const std::string& command,
                                const std::vector<std::string>& args,
                                bool auto_monitor) {
    LOG_F(INFO, "Launching process: {}", command);

#ifdef _WIN32
    std::string cmd_line = command;
    for (const auto& arg : args) {
        cmd_line += " " + arg;
    }

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Create the process
    if (!CreateProcess(NULL, const_cast<char*>(cmd_line.c_str()), NULL, NULL,
                       FALSE, 0, NULL, NULL, &si, &pi)) {
        LOG_F(ERROR, "CreateProcess failed: {}", GetLastError());
        if (error_callback_) {
            error_callback_("Failed to launch process: " + command,
                            GetLastError());
        }
        return 0;
    }

    LOG_F(INFO, "Process launched with PID: {}", pi.dwProcessId);

    // Close handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    pid_t new_pid = static_cast<pid_t>(pi.dwProcessId);
#else
    pid_t new_pid = fork();

    if (new_pid < 0) {
        // Fork failed
        LOG_F(ERROR, "Fork failed: {}", strerror(errno));
        if (error_callback_) {
            error_callback_("Fork failed when launching process: " + command,
                            errno);
        }
        return 0;
    }

    if (new_pid == 0) {
        // Child process
        // Prepare arguments for execv
        std::vector<char*> c_args;
        c_args.push_back(const_cast<char*>(command.c_str()));

        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        // Execute the command
        execvp(command.c_str(), c_args.data());

        // If execvp returns, it failed
        exit(EXIT_FAILURE);
    }

    // Parent process
    LOG_F(INFO, "Process launched with PID: {}", new_pid);
#endif

    // Notify about process creation
    if (process_create_callback_) {
        process_create_callback_(new_pid, command);
    }

    // Start monitoring if requested
    if (auto_monitor) {
        std::thread([this, new_pid]() {
            // Wait a moment for the process to initialize
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            this->startByPid(new_pid);
        }).detach();
    }

    return new_pid;
}

bool PidWatcher::terminateProcess(pid_t pid, bool force) {
    LOG_F(INFO, "Terminating process: {} (force: {})", pid, force);

#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (process == NULL) {
        LOG_F(ERROR, "Failed to open process for termination: {}",
              GetLastError());
        if (error_callback_) {
            error_callback_("Failed to open process for termination",
                            GetLastError());
        }
        return false;
    }

    BOOL result = TerminateProcess(process, force ? 9 : 1);
    CloseHandle(process);

    if (!result) {
        LOG_F(ERROR, "Failed to terminate process: {}", GetLastError());
        if (error_callback_) {
            error_callback_("Failed to terminate process", GetLastError());
        }
        return false;
    }
#else
    int signal_num = force ? SIGKILL : SIGTERM;
    if (kill(pid, signal_num) != 0) {
        LOG_F(ERROR, "Failed to send signal to process: {}", strerror(errno));
        if (error_callback_) {
            error_callback_("Failed to send signal to process", errno);
        }
        return false;
    }
#endif

    LOG_F(INFO, "Signal sent to process {} successfully", pid);

    // If we're monitoring this process, keep monitoring until it exits
    if (isMonitoring(pid)) {
        std::thread([this, pid]() {
            // Check if process has exited every 100ms for up to 5 seconds
            for (int i = 0; i < 50; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (!isProcessRunning(pid)) {
                    // Process has exited, update our records
                    std::lock_guard lock(mutex_);
                    auto it = monitored_processes_.find(pid);
                    if (it != monitored_processes_.end()) {
                        it->second.running = false;
                        it->second.status = ProcessStatus::DEAD;
                    }
                    break;
                }
            }
        }).detach();
    }

    return true;
}

bool PidWatcher::setResourceLimits(pid_t pid, const ResourceLimits& limits) {
    std::lock_guard lock(mutex_);

    auto it = process_configs_.find(pid);
    if (it == process_configs_.end()) {
        // Create a default config if none exists
        process_configs_[pid] = global_config_;
    }

    // Update resource limits
    process_configs_[pid].resource_limits = limits;

    LOG_F(INFO, "Set resource limits for process {}: CPU {}%, Memory {} KB",
          pid, limits.max_cpu_percent, limits.max_memory_kb);

    return true;
}

bool PidWatcher::setProcessPriority(pid_t pid, int priority) {
    LOG_F(INFO, "Setting process {} priority to {}", pid, priority);

#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (process == NULL) {
        LOG_F(ERROR, "Failed to open process for priority change: {}",
              GetLastError());
        if (error_callback_) {
            error_callback_("Failed to open process for priority change",
                            GetLastError());
        }
        return false;
    }

    // Map nice value to Windows priority class
    DWORD priority_class;
    if (priority <= -15) {
        priority_class = REALTIME_PRIORITY_CLASS;
    } else if (priority <= -10) {
        priority_class = HIGH_PRIORITY_CLASS;
    } else if (priority <= 0) {
        priority_class = ABOVE_NORMAL_PRIORITY_CLASS;
    } else if (priority <= 10) {
        priority_class = NORMAL_PRIORITY_CLASS;
    } else if (priority <= 15) {
        priority_class = BELOW_NORMAL_PRIORITY_CLASS;
    } else {
        priority_class = IDLE_PRIORITY_CLASS;
    }

    BOOL result = SetPriorityClass(process, priority_class);
    CloseHandle(process);

    if (!result) {
        LOG_F(ERROR, "Failed to set process priority: {}", GetLastError());
        if (error_callback_) {
            error_callback_("Failed to set process priority", GetLastError());
        }
        return false;
    }
#else
    if (setpriority(PRIO_PROCESS, pid, priority) != 0) {
        LOG_F(ERROR, "Failed to set process priority: {}", strerror(errno));
        if (error_callback_) {
            error_callback_("Failed to set process priority", errno);
        }
        return false;
    }
#endif

    // Update process info if we're monitoring this process
    {
        std::lock_guard lock(mutex_);
        auto it = monitored_processes_.find(pid);
        if (it != monitored_processes_.end()) {
            it->second.priority = priority;
        }
    }

    LOG_F(INFO, "Successfully set process {} priority to {}", pid, priority);
    return true;
}

bool PidWatcher::configureAutoRestart(pid_t pid, bool enable,
                                      int max_attempts) {
    std::lock_guard lock(mutex_);

    auto it = process_configs_.find(pid);
    if (it == process_configs_.end()) {
        // Create a default config if none exists
        process_configs_[pid] = global_config_;
    }

    // Update auto-restart configuration
    process_configs_[pid].auto_restart = enable;
    process_configs_[pid].max_restart_attempts = max_attempts;

    // Reset restart attempts counter if enabling
    if (enable) {
        restart_attempts_[pid] = 0;
    } else {
        restart_attempts_.erase(pid);
    }

    LOG_F(INFO, "{} auto-restart for process {} (max attempts: {})",
          enable ? "Enabled" : "Disabled", pid, max_attempts);

    // Ensure auto-restart thread is running
    auto_restart_cv_.notify_one();

    return true;
}

pid_t PidWatcher::restartProcess(pid_t pid) {
    LOG_F(INFO, "Restarting process: {}", pid);

    // Get process info to save command line before terminating
    auto info_opt = getProcessInfo(pid);
    if (!info_opt) {
        LOG_F(ERROR, "Failed to get process info for restart");
        if (error_callback_) {
            error_callback_("Failed to get process info for restart", EINVAL);
        }
        return 0;
    }

    std::string command = info_opt->command_line;

    // Terminate the process
    if (!terminateProcess(pid, false)) {
        LOG_F(WARNING, "Failed to terminate process, trying with force");
        if (!terminateProcess(pid, true)) {
            LOG_F(ERROR, "Failed to terminate process even with force");
            return 0;
        }
    }

    // Wait for process to fully terminate
    for (int i = 0; i < 50; i++) {
        if (!isProcessRunning(pid)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (isProcessRunning(pid)) {
        LOG_F(ERROR, "Process {} did not terminate in time", pid);
        return 0;
    }

    // Launch new process with same command line
    // Parse command line into command and args
    std::vector<std::string> args;
    std::stringstream ss(command);
    std::string item;
    bool first = true;
    std::string cmd;

    while (std::getline(ss, item, ' ')) {
        if (first) {
            cmd = item;
            first = false;
        } else {
            args.push_back(item);
        }
    }

    // Launch with same monitoring settings
    MonitorConfig config;
    {
        std::lock_guard lock(mutex_);
        auto it = process_configs_.find(pid);
        if (it != process_configs_.end()) {
            config = it->second;
        } else {
            config = global_config_;
        }
    }

    pid_t new_pid = launchProcess(cmd, args, false);
    if (new_pid == 0) {
        LOG_F(ERROR, "Failed to restart process");
        return 0;
    }

    // Start monitoring with same configuration
    if (!startByPid(new_pid, &config)) {
        LOG_F(WARNING, "Failed to start monitoring restarted process");
    }

    LOG_F(INFO, "Process restarted with new PID: {}", new_pid);
    return new_pid;
}

bool PidWatcher::dumpProcessInfo(pid_t pid, bool detailed,
                                 const std::string& output_file) {
    LOG_F(INFO, "Dumping process info for PID: {} (detailed: {})", pid,
          detailed);

    auto info_opt = getProcessInfo(pid);
    if (!info_opt) {
        LOG_F(ERROR, "Failed to get process info for dumping");
        if (error_callback_) {
            error_callback_("Failed to get process info for dumping", EINVAL);
        }
        return false;
    }

    const ProcessInfo& info = *info_opt;

    std::ostringstream oss;
    oss << "=== Process Information for PID " << pid << " ===\n";
    oss << "Name: " << info.name << "\n";
    oss << "Running: " << (info.running ? "Yes" : "No") << "\n";
    oss << "Status: ";

    switch (info.status) {
        case ProcessStatus::RUNNING:
            oss << "Running";
            break;
        case ProcessStatus::SLEEPING:
            oss << "Sleeping";
            break;
        case ProcessStatus::WAITING:
            oss << "Waiting (uninterruptible)";
            break;
        case ProcessStatus::STOPPED:
            oss << "Stopped";
            break;
        case ProcessStatus::ZOMBIE:
            oss << "Zombie";
            break;
        case ProcessStatus::DEAD:
            oss << "Dead";
            break;
        default:
            oss << "Unknown";
    }
    oss << "\n";

    oss << "CPU Usage: " << info.cpu_usage << "%\n";
    oss << "Memory Usage: " << info.memory_usage << " KB\n";
    oss << "Thread Count: " << info.thread_count << "\n";
    oss << "Parent PID: " << info.parent_pid << "\n";

    auto start_time_t = std::chrono::system_clock::to_time_t(info.start_time);
    oss << "Start Time: " << std::ctime(&start_time_t);
    oss << "Uptime: " << info.uptime.count() / 1000 << " seconds\n";

    if (detailed) {
        oss << "\n=== Detailed Information ===\n";
        oss << "Command Line: " << info.command_line << "\n";
        oss << "Username: " << info.username << "\n";
        oss << "Virtual Memory: " << info.virtual_memory << " KB\n";
        oss << "Shared Memory: " << info.shared_memory << " KB\n";
        oss << "Priority: " << info.priority << "\n";

        oss << "I/O Statistics:\n";
        oss << "  Read Bytes: " << info.io_stats.read_bytes << "\n";
        oss << "  Write Bytes: " << info.io_stats.write_bytes << "\n";
        oss << "  Read Rate: " << info.io_stats.read_rate << " bytes/sec\n";
        oss << "  Write Rate: " << info.io_stats.write_rate << " bytes/sec\n";

        oss << "Child Processes: ";
        if (info.child_processes.empty()) {
            oss << "None\n";
        } else {
            oss << "\n";
            for (pid_t child_pid : info.child_processes) {
                oss << "  - PID " << child_pid;
                auto child_info = getProcessInfo(child_pid);
                if (child_info) {
                    oss << " (" << child_info->name << ")";
                }
                oss << "\n";
            }
        }

        // Include monitoring configuration if available
        std::lock_guard lock(mutex_);
        auto config_it = process_configs_.find(pid);
        if (config_it != process_configs_.end()) {
            const auto& config = config_it->second;
            oss << "\nMonitoring Configuration:\n";
            oss << "  Update Interval: " << config.update_interval.count()
                << " ms\n";
            oss << "  Monitor Children: "
                << (config.monitor_children ? "Yes" : "No") << "\n";
            oss << "  Auto Restart: " << (config.auto_restart ? "Yes" : "No")
                << "\n";
            oss << "  Max Restart Attempts: " << config.max_restart_attempts
                << "\n";
            oss << "  Resource Limits:\n";
            oss << "    Max CPU: " << config.resource_limits.max_cpu_percent
                << "%\n";
            oss << "    Max Memory: " << config.resource_limits.max_memory_kb
                << " KB\n";
        }
    }

    std::string dump = oss.str();

    if (output_file.empty()) {
        // Log the information
        LOG_F(INFO, "%s", dump.c_str());
    } else {
        // Write to file
        std::ofstream file(output_file);
        if (!file.is_open()) {
            LOG_F(ERROR, "Failed to open output file: {}", output_file);
            if (error_callback_) {
                error_callback_("Failed to open output file", errno);
            }
            return false;
        }
        file << dump;
        LOG_F(INFO, "Process information dumped to {}", output_file);
    }

    return true;
}

std::unordered_map<pid_t, std::map<std::string, double>>
PidWatcher::getMonitoringStats() const {
    std::lock_guard lock(mutex_);
    return monitoring_stats_;
}

PidWatcher& PidWatcher::setRateLimiting(unsigned int max_updates_per_second) {
    std::lock_guard lock(mutex_);
    max_updates_per_second_ = std::max(1u, max_updates_per_second);
    LOG_F(INFO, "Set rate limiting to {} updates per second",
          max_updates_per_second_);
    return *this;
}

bool PidWatcher::checkRateLimit() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - rate_limit_start_time_);

    if (elapsed.count() >= 1) {
        // Reset counter for new period
        rate_limit_start_time_ = now;
        update_count_ = 1;
        return true;
    } else if (update_count_ < max_updates_per_second_) {
        // Within rate limit
        update_count_++;
        return true;
    } else {
        // Rate limit exceeded
        return false;
    }
}

void PidWatcher::monitorChildProcesses(pid_t parent_pid) {
    // Get child processes
    std::vector<pid_t> children = getChildProcesses(parent_pid);

    for (pid_t child_pid : children) {
        // Skip if already monitoring this child
        if (isMonitoring(child_pid)) {
            continue;
        }

        // Get config for parent process
        MonitorConfig child_config;
        {
            std::lock_guard lock(mutex_);
            auto parent_it = process_configs_.find(parent_pid);
            if (parent_it != process_configs_.end()) {
                child_config = parent_it->second;
            } else {
                child_config = global_config_;
            }
        }

        // Start monitoring child
        LOG_F(INFO, "Auto-monitoring child process {} of parent {}", child_pid,
              parent_pid);
        startByPid(child_pid, &child_config);

        // Recursively monitor grandchildren if configured
        if (child_config.monitor_children) {
            monitorChildProcesses(child_pid);
        }
    }
}

std::string PidWatcher::getProcessUsername([[maybe_unused]] pid_t pid) const {
#ifdef _WIN32
    // Windows doesn't have a straightforward API for this in C++
    return "";
#else
    std::string proc_path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream status_file(proc_path);
    if (!status_file.is_open()) {
        return "";
    }

    std::string line;
    while (std::getline(status_file, line)) {
        if (line.compare(0, 4, "Uid:") == 0) {
            std::istringstream iss(line.substr(4));
            uid_t uid;
            iss >> uid;  // Read real UID

            // Convert UID to username
            struct passwd* pw = getpwuid(uid);
            if (pw) {
                return pw->pw_name;
            }
            break;
        }
    }

    return "";
#endif
}

std::string PidWatcher::getProcessCommandLine(pid_t pid) const {
#ifdef _WIN32
    HANDLE process =
        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (process == NULL) {
        return "";
    }

    // Windows doesn't provide easy access to command line args
    // GetCommandLine only returns the command line of the current process

    // We can get the executable path
    char filename[MAX_PATH];
    if (GetModuleFileNameEx(process, NULL, filename, MAX_PATH)) {
        CloseHandle(process);
        return filename;
    }

    CloseHandle(process);
    return "";
#else
    std::string proc_path = "/proc/" + std::to_string(pid) + "/cmdline";
    std::ifstream cmdline_file(proc_path);
    if (!cmdline_file.is_open()) {
        return "";
    }

    std::string cmdline;
    std::getline(cmdline_file, cmdline);

    // Replace null bytes with spaces for display
    for (char& c : cmdline) {
        if (c == '\0')
            c = ' ';
    }

    return cmdline;
#endif
}

void PidWatcher::updateIOStats(pid_t pid, ProcessIOStats& stats) const {
#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process == NULL) {
        return;
    }

    IO_COUNTERS io_counters;
    if (GetProcessIoCounters(process, &io_counters)) {
        stats.read_bytes = io_counters.ReadTransferCount;
        stats.write_bytes = io_counters.WriteTransferCount;
    }

    CloseHandle(process);
#else
    std::string io_path = "/proc/" + std::to_string(pid) + "/io";
    std::ifstream io_file(io_path);
    if (!io_file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(io_file, line)) {
        if (line.compare(0, 10, "read_bytes") == 0) {
            std::istringstream iss(
                line.substr(line.find_first_of("0123456789")));
            iss >> stats.read_bytes;
        } else if (line.compare(0, 11, "write_bytes") == 0) {
            std::istringstream iss(
                line.substr(line.find_first_of("0123456789")));
            iss >> stats.write_bytes;
        }
    }
#endif
}

void PidWatcher::checkResourceLimits(pid_t pid, const ProcessInfo& info) {
    std::lock_guard lock(mutex_);

    auto config_it = process_configs_.find(pid);
    if (config_it == process_configs_.end()) {
        return;  // No specific config for this process
    }

    const auto& limits = config_it->second.resource_limits;
    bool limit_exceeded = false;

    // Check CPU limit
    if (limits.max_cpu_percent > 0.0 &&
        info.cpu_usage > limits.max_cpu_percent) {
        LOG_F(WARNING, "Process {} exceeded CPU limit: {:.2f}% > {:.2f}%", pid,
              info.cpu_usage, limits.max_cpu_percent);
        limit_exceeded = true;
    }

    // Check memory limit
    if (limits.max_memory_kb > 0 && info.memory_usage > limits.max_memory_kb) {
        LOG_F(WARNING, "Process {} exceeded memory limit: {} KB > {} KB", pid,
              info.memory_usage, limits.max_memory_kb);
        limit_exceeded = true;
    }

    if (limit_exceeded && resource_limit_callback_) {
        resource_limit_callback_(info, limits);
    }
}

ProcessInfo PidWatcher::updateProcessInfo(pid_t pid) {
    ProcessInfo info;
    info.pid = pid;

    // Find existing info if available
    auto it = monitored_processes_.find(pid);
    if (it != monitored_processes_.end()) {
        info = it->second;
    }

    // Check if process is running
    info.running = isProcessRunning(pid);

    if (info.running) {
        // Get detailed process info
        auto detailed_info = getProcessInfo(pid);
        if (detailed_info) {
            info = *detailed_info;
        } else {
            // Basic update if detailed info failed
            info.cpu_usage = getProcessCpuUsage(pid);
            info.memory_usage = getProcessMemoryUsage(pid);
            info.thread_count = getProcessThreadCount(pid);
            info.status = getProcessStatus(pid);
            updateIOStats(pid, info.io_stats);
        }

        // Update monitoring statistics
        monitoring_stats_[pid]["cpu_usage"] = info.cpu_usage;
        monitoring_stats_[pid]["memory_kb"] =
            static_cast<double>(info.memory_usage);
        monitoring_stats_[pid]["threads"] =
            static_cast<double>(info.thread_count);
        monitoring_stats_[pid]["io_read_rate"] = info.io_stats.read_rate;
        monitoring_stats_[pid]["io_write_rate"] = info.io_stats.write_rate;
    } else {
        info.cpu_usage = 0.0;
        info.memory_usage = 0;
        info.status = ProcessStatus::DEAD;
    }

    // Update in the map
    monitored_processes_[pid] = info;

    return info;
}

void PidWatcher::monitorThread() {
    LOG_F(INFO, "Monitor thread started");
    while (true) {
        std::unique_lock lock(mutex_);

        while (!monitoring_ && running_) {
            monitor_cv_.wait(lock);
        }

        if (!running_) {
            LOG_F(INFO, "Monitor thread exiting");
            break;
        }

        // Check rate limit
        if (!checkRateLimit()) {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // Update process info
        ProcessInfo info = updateProcessInfo(pid_);

        // Check if we should monitor child processes
        auto config_it = process_configs_.find(pid_);
        bool monitor_children = false;
        if (config_it != process_configs_.end()) {
            monitor_children = config_it->second.monitor_children;
        } else {
            monitor_children = global_config_.monitor_children;
        }

        if (monitor_children && info.running) {
            lock.unlock();
            monitorChildProcesses(pid_);
            lock.lock();
        }

        if (monitor_callback_ && info.running) {
            LOG_F(INFO, "Executing monitor callback for PID {}", pid_);
            lock.unlock();
            monitor_callback_(info);
            lock.lock();
        }

        if (!info.running) {
            LOG_F(INFO, "Process {} has exited", pid_);

            if (exit_callback_) {
                lock.unlock();
                exit_callback_(info);
                lock.lock();
            }

            // Process has exited
            if (monitored_processes_.size() <= 1) {
                // No more processes to monitor
                running_ = false;
                monitoring_ = false;
                break;
            } else {
                // Remove this process and continue monitoring others
                monitored_processes_.erase(pid_);

                // Switch to another process if available
                if (!monitored_processes_.empty()) {
                    auto first_proc = monitored_processes_.begin();
                    pid_ = first_proc->first;
                    LOG_F(INFO, "Switching primary monitor to PID {}", pid_);
                }
            }
        }

        // Signal watchdog that we're healthy
        watchdog_healthy_ = true;

        lock.unlock();

        if (monitor_interval_.count() > 0) {
            std::this_thread::sleep_for(monitor_interval_);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    LOG_F(INFO, "Monitor thread exited");
}

void PidWatcher::exitThread() {
    LOG_F(INFO, "Exit thread started");
    while (true) {
        std::unique_lock lock(mutex_);

        if (!running_) {
            LOG_F(INFO, "Exit thread exiting");
            break;
        }

        exit_cv_.wait_for(lock, std::chrono::seconds(1));

        if (!running_) {
            LOG_F(INFO, "Exit thread exiting");
            break;
        }

        // Check all monitored processes for exits
        std::vector<pid_t> exited_processes;

        for (auto& pair : monitored_processes_) {
            pid_t current_pid = pair.first;
            ProcessInfo& info = pair.second;

            if (info.running && !isProcessRunning(current_pid)) {
                LOG_F(INFO, "Process {} has exited", current_pid);
                info.running = false;
                info.status = ProcessStatus::DEAD;

                // Call exit callback
                if (exit_callback_) {
                    lock.unlock();
                    exit_callback_(info);
                    lock.lock();
                }

                // Mark for removal
                exited_processes.push_back(current_pid);
            }
        }

        // Remove exited processes
        for (pid_t pid : exited_processes) {
            // Don't actually remove here - let auto-restart thread handle it
            // Just mark as not running for now
            auto it = monitored_processes_.find(pid);
            if (it != monitored_processes_.end()) {
                it->second.running = false;
                it->second.status = ProcessStatus::DEAD;

                // If this was the primary process, update primary PID
                if (pid_ == pid && monitored_processes_.size() > 1) {
                    // Find first running process
                    bool found = false;
                    for (auto& pair : monitored_processes_) {
                        if (pair.first != pid && pair.second.running) {
                            pid_ = pair.first;
                            found = true;
                            LOG_F(INFO, "Switching primary monitor to PID {}",
                                  pid_);
                            break;
                        }
                    }

                    if (!found) {
                        // No running processes, use first in map
                        auto it = monitored_processes_.begin();
                        if (it->first != pid) {
                            pid_ = it->first;
                        } else if (monitored_processes_.size() > 1) {
                            pid_ = std::next(it)->first;
                        }
                        LOG_F(INFO,
                              "No running processes, switching primary monitor "
                              "to PID {}",
                              pid_);
                    }
                }
            }
        }

        // Notify auto-restart thread
        if (!exited_processes.empty()) {
            auto_restart_cv_.notify_one();
        }

        // Signal watchdog that we're healthy
        watchdog_healthy_ = true;
    }
    LOG_F(INFO, "Exit thread exited");
}

void PidWatcher::multiMonitorThread() {
    LOG_F(INFO, "Multi-monitor thread started");

    while (true) {
        std::unique_lock lock(mutex_);

        if (!running_ || monitored_processes_.empty()) {
            LOG_F(INFO, "Multi-monitor thread exiting");
            break;
        }

        multi_monitor_cv_.wait_for(lock, monitor_interval_);

        if (!running_ || monitored_processes_.empty()) {
            LOG_F(INFO, "Multi-monitor thread exiting");
            break;
        }

        // Check rate limit
        if (!checkRateLimit()) {
            continue;
        }

        // Update all processes and create vector of info
        std::vector<ProcessInfo> process_infos;

        for (auto& pair : monitored_processes_) {
            pid_t current_pid = pair.first;

            // Skip processes that are known to be not running
            if (!pair.second.running) {
                continue;
            }

            ProcessInfo info = updateProcessInfo(current_pid);

            // Check if we should monitor child processes
            auto config_it = process_configs_.find(current_pid);
            bool monitor_children = false;
            if (config_it != process_configs_.end()) {
                monitor_children = config_it->second.monitor_children;
            } else {
                monitor_children = global_config_.monitor_children;
            }

            if (monitor_children && info.running) {
                lock.unlock();
                monitorChildProcesses(current_pid);
                lock.lock();
            }

            process_infos.push_back(info);
        }

        // Call multi-process callback if set
        if (multi_process_callback_ && !process_infos.empty()) {
            lock.unlock();
            multi_process_callback_(process_infos);
            lock.lock();
        }

        // Signal watchdog that we're healthy
        watchdog_healthy_ = true;
    }

    LOG_F(INFO, "Multi-monitor thread exited");
}

void PidWatcher::resourceMonitorThread() {
    LOG_F(INFO, "Resource monitor thread started");

    while (true) {
        std::unique_lock lock(mutex_);

        if (!running_ || monitored_processes_.empty()) {
            LOG_F(INFO, "Resource monitor thread exiting");
            break;
        }

        resource_monitor_cv_.wait_for(lock, std::chrono::seconds(1));

        if (!running_ || monitored_processes_.empty()) {
            LOG_F(INFO, "Resource monitor thread exiting");
            break;
        }

        // Check each process against its resource limits
        for (auto& pair : monitored_processes_) {
            pid_t current_pid = pair.first;
            const ProcessInfo& info = pair.second;

            // Skip processes that are not running
            if (!info.running) {
                continue;
            }

            // Check resource limits
            lock.unlock();
            checkResourceLimits(current_pid, info);
            lock.lock();
        }

        // Signal watchdog that we're healthy
        watchdog_healthy_ = true;
    }

    LOG_F(INFO, "Resource monitor thread exited");
}

void PidWatcher::autoRestartThread() {
    LOG_F(INFO, "Auto-restart thread started");

    while (true) {
        std::unique_lock lock(mutex_);

        if (!running_) {
            LOG_F(INFO, "Auto-restart thread exiting");
            break;
        }

        auto_restart_cv_.wait_for(lock, std::chrono::seconds(1));

        if (!running_) {
            LOG_F(INFO, "Auto-restart thread exiting");
            break;
        }

        // Check for processes that need restarting
        std::vector<pid_t> to_restart;
        std::vector<pid_t> to_remove;

        for (auto& pair : monitored_processes_) {
            pid_t current_pid = pair.first;
            const ProcessInfo& info = pair.second;

            if (!info.running) {
                // Check if auto-restart is enabled for this process
                auto config_it = process_configs_.find(current_pid);
                bool auto_restart = false;
                int max_attempts = 3;

                if (config_it != process_configs_.end()) {
                    auto_restart = config_it->second.auto_restart;
                    max_attempts = config_it->second.max_restart_attempts;
                } else {
                    auto_restart = global_config_.auto_restart;
                    max_attempts = global_config_.max_restart_attempts;
                }

                if (auto_restart) {
                    // Check restart attempts
                    int attempts = restart_attempts_[current_pid];
                    if (attempts < max_attempts) {
                        // Schedule for restart
                        to_restart.push_back(current_pid);
                        restart_attempts_[current_pid]++;
                    } else {
                        // Max attempts reached, remove from monitoring
                        LOG_F(WARNING,
                              "Process {} reached max restart attempts ({}), "
                              "removing from monitoring",
                              current_pid, max_attempts);
                        to_remove.push_back(current_pid);
                    }
                } else {
                    // No auto-restart, remove from monitoring
                    to_remove.push_back(current_pid);
                }
            }
        }

        // Release lock before performing restarts
        lock.unlock();

        // Restart processes
        for (pid_t pid : to_restart) {
            LOG_F(INFO, "Auto-restarting process {}", pid);
            pid_t new_pid = restartProcess(pid);

            if (new_pid == 0) {
                LOG_F(ERROR, "Failed to auto-restart process {}", pid);
            } else {
                LOG_F(INFO, "Process {} restarted as PID {}", pid, new_pid);
            }
        }

        lock.lock();

        // Remove processes that shouldn't be monitored anymore
        for (pid_t pid : to_remove) {
            monitored_processes_.erase(pid);
            process_configs_.erase(pid);
            restart_attempts_.erase(pid);

            // If this was the primary process, update primary PID
            if (pid_ == pid && !monitored_processes_.empty()) {
                pid_ = monitored_processes_.begin()->first;
                LOG_F(INFO, "Switching primary monitor to PID {}", pid_);
            }
        }

        // Check if we should exit
        if (monitored_processes_.empty()) {
            running_ = false;
            monitoring_ = false;
            break;
        }

        // Signal watchdog that we're healthy
        watchdog_healthy_ = true;
    }

    LOG_F(INFO, "Auto-restart thread exited");
}

void PidWatcher::watchdogThread() {
    LOG_F(INFO, "Watchdog thread started");

    int unhealthy_count = 0;
    const int max_unhealthy_count = 3;  // How many times watchdog can find
                                        // system unhealthy before taking action

    while (true) {
        std::unique_lock lock(mutex_);

        if (!running_) {
            LOG_F(INFO, "Watchdog thread exiting");
            break;
        }

        // Wait for next check interval
        watchdog_cv_.wait_for(lock, std::chrono::seconds(5));

        if (!running_) {
            LOG_F(INFO, "Watchdog thread exiting");
            break;
        }

        if (!watchdog_healthy_) {
            unhealthy_count++;
            LOG_F(WARNING, "Watchdog detected unhealthy state ({}/{})",
                  unhealthy_count, max_unhealthy_count);

            if (unhealthy_count >= max_unhealthy_count) {
                LOG_F(ERROR,
                      "Watchdog detected system hung, attempting recovery");

                // Try to recover by restarting threads
                bool need_restart = false;

                // Stop threads
                running_ = false;

                // Notify all threads to exit
                exit_cv_.notify_all();
                monitor_cv_.notify_all();
                multi_monitor_cv_.notify_all();
                resource_monitor_cv_.notify_all();
                auto_restart_cv_.notify_all();

                lock.unlock();

                // Wait for threads to exit
                std::this_thread::sleep_for(std::chrono::seconds(1));

                // Join any joinable threads
                if (monitor_thread_.joinable()) {
                    try {
                        monitor_thread_.join();
                        need_restart = true;
                    } catch (...) {
                    }
                }

                if (exit_thread_.joinable()) {
                    try {
                        exit_thread_.join();
                        need_restart = true;
                    } catch (...) {
                    }
                }

                if (multi_monitor_thread_.joinable()) {
                    try {
                        multi_monitor_thread_.join();
                        need_restart = true;
                    } catch (...) {
                    }
                }

                if (resource_monitor_thread_.joinable()) {
                    try {
                        resource_monitor_thread_.join();
                        need_restart = true;
                    } catch (...) {
                    }
                }

                if (auto_restart_thread_.joinable()) {
                    try {
                        auto_restart_thread_.join();
                        need_restart = true;
                    } catch (...) {
                    }
                }

                lock.lock();

                if (need_restart && !monitored_processes_.empty()) {
                    // Restart monitoring
                    LOG_F(INFO, "Watchdog restarting monitoring system");
                    running_ = true;
                    monitoring_ = true;
                    watchdog_healthy_ = true;
                    unhealthy_count = 0;

                    // Start all monitoring threads
#if __cplusplus >= 202002L
                    monitor_thread_ =
                        std::jthread(&PidWatcher::monitorThread, this);
                    exit_thread_ = std::jthread(&PidWatcher::exitThread, this);
                    multi_monitor_thread_ =
                        std::jthread(&PidWatcher::multiMonitorThread, this);
                    resource_monitor_thread_ =
                        std::jthread(&PidWatcher::resourceMonitorThread, this);
                    auto_restart_thread_ =
                        std::jthread(&PidWatcher::autoRestartThread, this);
#else
                    monitor_thread_ =
                        std::thread(&PidWatcher::monitorThread, this);
                    exit_thread_ = std::thread(&PidWatcher::exitThread, this);
                    multi_monitor_thread_ =
                        std::thread(&PidWatcher::multiMonitorThread, this);
                    resource_monitor_thread_ =
                        std::thread(&PidWatcher::resourceMonitorThread, this);
                    auto_restart_thread_ =
                        std::thread(&PidWatcher::autoRestartThread, this);
#endif
                } else {
                    // Something is very wrong, shutdown
                    LOG_F(ERROR,
                          "Watchdog cannot recover, shutting down PidWatcher");
                    running_ = false;
                    monitoring_ = false;
                    monitored_processes_.clear();
                    break;
                }
            }
        } else {
            // System is healthy, reset counter
            if (unhealthy_count > 0) {
                LOG_F(INFO, "Watchdog detected system recovered");
            }
            unhealthy_count = 0;
            watchdog_healthy_ = false;  // Reset for next interval
        }
    }

    LOG_F(INFO, "Watchdog thread exited");
}

}  // namespace atom::system