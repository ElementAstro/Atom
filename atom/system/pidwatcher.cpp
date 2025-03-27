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
#include <istream>
#include <sstream>
#include <string>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#pragma comment(lib, "pdh.lib")
// clang-format on
#else
#include <dirent.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

#include "atom/log/loguru.hpp"

namespace atom::system {

PidWatcher::PidWatcher()
    : running_(false),
      monitoring_(false),
      monitor_interval_(std::chrono::milliseconds(1000)) {
    LOG_F(INFO, "PidWatcher constructor called");
}

PidWatcher::~PidWatcher() {
    LOG_F(INFO, "PidWatcher destructor called");
    stop();
}

void PidWatcher::setExitCallback(ProcessCallback callback) {
    LOG_F(INFO, "Setting exit callback");
    std::lock_guard lock(mutex_);
    exit_callback_ = std::move(callback);
}

void PidWatcher::setMonitorFunction(ProcessCallback callback,
                                    std::chrono::milliseconds interval) {
    LOG_F(INFO, "Setting monitor function with interval: {} ms",
          interval.count());
    std::lock_guard lock(mutex_);
    monitor_callback_ = std::move(callback);
    monitor_interval_ = interval;
}

void PidWatcher::setMultiProcessCallback(MultiProcessCallback callback) {
    LOG_F(INFO, "Setting multi-process callback");
    std::lock_guard lock(mutex_);
    multi_process_callback_ = std::move(callback);
}

void PidWatcher::setErrorCallback(ErrorCallback callback) {
    LOG_F(INFO, "Setting error callback");
    std::lock_guard lock(mutex_);
    error_callback_ = std::move(callback);
}

auto PidWatcher::getPidByName(const std::string &name) const -> pid_t {
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
    DIR *dir = opendir("/proc");
    if (!dir) {
        LOG_F(ERROR, "Failed to open /proc directory");
        if (error_callback_) {
            error_callback_("Failed to open /proc directory", errno);
        }
        return 0;
    }

    struct dirent *entry;
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
        proc_name.erase(0, proc_name.find_first_not_of(" \t"));
        proc_name.erase(proc_name.find_last_not_of(" \t") + 1);

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

auto PidWatcher::getProcessInfo(pid_t pid) const -> std::optional<ProcessInfo> {
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

    // Check if process is running
    DWORD exit_code;
    GetExitCodeProcess(process_handle, &exit_code);
    info.running = (exit_code == STILL_ACTIVE);

    // Get memory usage
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(process_handle, &pmc, sizeof(pmc))) {
        info.memory_usage = pmc.WorkingSetSize / 1024;  // Convert to KB
    }

    CloseHandle(process_handle);
#else
    // Check if process exists
    std::string proc_path = "/proc/" + std::to_string(pid);
    if (!fs::exists(proc_path)) {
        return std::nullopt;
    }

    info.running = true;

    // Get process name
    std::ifstream cmdline_file(proc_path + "/cmdline");
    if (cmdline_file.is_open()) {
        std::string cmdline;
        std::getline(cmdline_file, cmdline);

        // Extract executable name from cmdline
        size_t pos = cmdline.find_last_of('/');
        info.name =
            (pos != std::string::npos) ? cmdline.substr(pos + 1) : cmdline;
        info.name.erase(0, info.name.find_first_not_of(" \t\0"));
        info.name.erase(info.name.find_last_not_of(" \t\0") + 1);
    }

    // Get memory usage
    std::ifstream status_file(proc_path + "/status");
    if (status_file.is_open()) {
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.compare(0, 6, "VmRSS:") == 0) {
                std::istringstream iss(line.substr(6));
                iss >> info.memory_usage;
                break;
            }
        }
    }

    // Get process start time
    struct stat stat_buf;
    if (stat(proc_path.c_str(), &stat_buf) == 0) {
        info.start_time =
            std::chrono::system_clock::from_time_t(stat_buf.st_ctime);
    }

    // Get CPU usage
    info.cpu_usage = getProcessCpuUsage(pid);
#endif

    return info;
}

auto PidWatcher::start(const std::string &name) -> bool {
    LOG_F(INFO, "Starting PidWatcher for process name: {}", name);
    std::lock_guard lock(mutex_);

    if (running_) {
        LOG_F(ERROR, "Already running.");
        return false;
    }

    pid_ = getPidByName(name);
    if (pid_ == 0) {
        LOG_F(ERROR, "Failed to get PID for {}", name);
        if (error_callback_) {
            error_callback_("Failed to get PID for " + name, ESRCH);
        }
        return false;
    }

    // Initialize process info
    ProcessInfo info;
    info.pid = pid_;
    info.name = name;
    info.running = true;
    info.start_time = std::chrono::system_clock::now();

    monitored_processes_[pid_] = info;

    running_ = true;
    monitoring_ = true;

#if __cplusplus >= 202002L
    monitor_thread_ = std::jthread(&PidWatcher::monitorThread, this);
    exit_thread_ = std::jthread(&PidWatcher::exitThread, this);
#else
    monitor_thread_ = std::thread(&PidWatcher::monitorThread, this);
    exit_thread_ = std::thread(&PidWatcher::exitThread, this);
#endif

    LOG_F(INFO, "PidWatcher started for process name: {}", name);
    return true;
}

auto PidWatcher::startMultiple(const std::vector<std::string> &process_names)
    -> size_t {
    LOG_F(INFO, "Starting PidWatcher for multiple processes");
    std::lock_guard lock(mutex_);

    size_t success_count = 0;

    for (const auto &name : process_names) {
        pid_t pid = getPidByName(name);
        if (pid == 0) {
            LOG_F(WARNING, "Failed to get PID for {}", name);
            if (error_callback_) {
                error_callback_("Failed to get PID for " + name, ESRCH);
            }
            continue;
        }

        // Initialize process info
        ProcessInfo info;
        info.pid = pid;
        info.name = name;
        info.running = true;
        info.start_time = std::chrono::system_clock::now();

        monitored_processes_[pid] = info;
        success_count++;
    }

    if (success_count > 0 && !running_) {
        running_ = true;
        monitoring_ = true;

#if __cplusplus >= 202002L
        multi_monitor_thread_ =
            std::jthread(&PidWatcher::multiMonitorThread, this);
#else
        multi_monitor_thread_ =
            std::thread(&PidWatcher::multiMonitorThread, this);
#endif
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

        exit_cv_.notify_one();
        monitor_cv_.notify_one();
        multi_monitor_cv_.notify_one();

        monitored_processes_.clear();
    }

    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    if (exit_thread_.joinable()) {
        exit_thread_.join();
    }
    if (multi_monitor_thread_.joinable()) {
        multi_monitor_thread_.join();
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
    monitored_processes_.erase(it);

    // If this was the last process, stop all threads
    if (monitored_processes_.empty()) {
        running_ = false;
        monitoring_ = false;

        exit_cv_.notify_one();
        monitor_cv_.notify_one();
        multi_monitor_cv_.notify_one();
    }

    return true;
}

auto PidWatcher::Switch(const std::string &name) -> bool {
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

    // If already monitoring this process in multi-mode, don't switch
    if (monitored_processes_.find(new_pid) != monitored_processes_.end()) {
        LOG_F(INFO, "Already monitoring process {} ({})", new_pid, name);
        return true;
    }

    // Update primary PID
    pid_ = new_pid;

    // Initialize or update process info
    ProcessInfo info;
    info.pid = pid_;
    info.name = name;
    info.running = true;
    info.start_time = std::chrono::system_clock::now();

    monitored_processes_[pid_] = info;

    monitor_cv_.notify_one();

    LOG_F(INFO, "PidWatcher switched to process name: {}", name);
    return true;
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
        PdhOpenQuery(NULL, NULL, &cpuQuery);
        std::string counterPath =
            "\\Process(" + std::to_string(pid) + ")\\% Processor Time";
        PdhAddEnglishCounter(cpuQuery, counterPath.c_str(), NULL, &cpuTotal);
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

    if (first_call) {
        // First call, just store values
        CPUUsageData data;
        data.lastTotalUser = user;
        data.lastTotalUserLow = nice;
        data.lastTotalSys = system;
        data.lastTotalIdle = idle;
        cpu_usage_data_[pid] = data;
        return 0.0;
    } else {
        // Calculate CPU usage
        CPUUsageData &last = cpu_usage_data_[pid];

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
        // Update resource usage
        info.cpu_usage = getProcessCpuUsage(pid);
        info.memory_usage = getProcessMemoryUsage(pid);
    } else {
        info.cpu_usage = 0.0;
        info.memory_usage = 0;
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

        // Update process info
        ProcessInfo info = updateProcessInfo(pid_);

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

        lock.unlock();

        if (monitor_interval_.count() > 0) {
            std::this_thread::sleep_for(monitor_interval_);
        } else {
            break;
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

        // Check if primary process has exited
        bool is_running = isProcessRunning(pid_);

        if (!is_running) {
            LOG_F(INFO, "Primary process {} has exited", pid_);

            ProcessInfo info = monitored_processes_[pid_];
            info.running = false;

            if (exit_callback_) {
                lock.unlock();
                exit_callback_(info);
                lock.lock();
            }

            // Process has exited
            monitored_processes_.erase(pid_);

            if (monitored_processes_.empty()) {
                // No more processes to monitor
                running_ = false;
                monitoring_ = false;
                break;
            } else {
                // Switch to another process if available
                auto first_proc = monitored_processes_.begin();
                pid_ = first_proc->first;
                LOG_F(INFO, "Switching primary monitor to PID {}", pid_);
            }
        }
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

        // Update all processes and create vector of info
        std::vector<ProcessInfo> process_infos;
        std::vector<pid_t> exited_processes;

        for (auto &pair : monitored_processes_) {
            pid_t current_pid = pair.first;

            ProcessInfo info = updateProcessInfo(current_pid);

            if (!info.running) {
                LOG_F(INFO, "Process {} has exited", current_pid);
                exited_processes.push_back(current_pid);

                // Call individual exit callback if set
                if (exit_callback_) {
                    lock.unlock();
                    exit_callback_(info);
                    lock.lock();
                }
            }

            process_infos.push_back(info);
        }

        // Call multi-process callback if set
        if (multi_process_callback_ && !process_infos.empty()) {
            lock.unlock();
            multi_process_callback_(process_infos);
            lock.lock();
        }

        // Remove exited processes
        for (pid_t pid : exited_processes) {
            monitored_processes_.erase(pid);
        }

        // Check if we should exit
        if (monitored_processes_.empty()) {
            running_ = false;
            monitoring_ = false;
            break;
        }

        lock.unlock();

        // Sleep for the monitoring interval
        std::this_thread::sleep_for(monitor_interval_);
    }

    LOG_F(INFO, "Multi-monitor thread exited");
}

}  // namespace atom::system
