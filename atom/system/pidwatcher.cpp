/*
 * pidwatcher.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

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
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <tlhelp32.h>
// clang-format on
#ifdef _MSC_VER
#pragma comment(lib, "pdh.lib")
#endif
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
namespace fs = std::filesystem;
#endif

#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

namespace atom::system {

PidWatcher::PidWatcher()
    : rate_limit_start_time_(std::chrono::steady_clock::now()) {
    spdlog::info("PidWatcher initialized");
}

PidWatcher::PidWatcher(const MonitorConfig& config)
    : global_config_(config),
      monitor_interval_(config.update_interval),
      rate_limit_start_time_(std::chrono::steady_clock::now()) {
    spdlog::info("PidWatcher initialized with config (interval: {} ms)",
                 config.update_interval.count());
}

PidWatcher::~PidWatcher() {
    stop();
    spdlog::info("PidWatcher destroyed");
}

PidWatcher& PidWatcher::setExitCallback(ProcessCallback callback) {
    std::lock_guard lock(mutex_);
    exit_callback_ = std::move(callback);
    return *this;
}

PidWatcher& PidWatcher::setMonitorFunction(ProcessCallback callback,
                                           std::chrono::milliseconds interval) {
    std::lock_guard lock(mutex_);
    monitor_callback_ = std::move(callback);
    monitor_interval_ = interval;
    return *this;
}

PidWatcher& PidWatcher::setMultiProcessCallback(MultiProcessCallback callback) {
    std::lock_guard lock(mutex_);
    multi_process_callback_ = std::move(callback);
    return *this;
}

PidWatcher& PidWatcher::setErrorCallback(ErrorCallback callback) {
    std::lock_guard lock(mutex_);
    error_callback_ = std::move(callback);
    return *this;
}

PidWatcher& PidWatcher::setResourceLimitCallback(
    ResourceLimitCallback callback) {
    std::lock_guard lock(mutex_);
    resource_limit_callback_ = std::move(callback);
    return *this;
}

PidWatcher& PidWatcher::setProcessCreateCallback(
    ProcessCreateCallback callback) {
    std::lock_guard lock(mutex_);
    process_create_callback_ = std::move(callback);
    return *this;
}

PidWatcher& PidWatcher::setProcessFilter(ProcessFilter filter) {
    std::lock_guard lock(mutex_);
    process_filter_ = std::move(filter);
    return *this;
}

pid_t PidWatcher::getPidByName(const std::string& name) const {
#ifdef _WIN32
    DWORD pid_list[1024];
    DWORD cb_needed;
    if (!EnumProcesses(pid_list, sizeof(pid_list), &cb_needed)) {
        return 0;
    }

    for (unsigned int i = 0; i < cb_needed / sizeof(DWORD); i++) {
        HANDLE process_handle = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid_list[i]);
        if (process_handle != nullptr) {
            char filename[MAX_PATH];
            if (GetModuleFileNameEx(process_handle, nullptr, filename,
                                    MAX_PATH)) {
                std::string process_name =
                    std::filesystem::path(filename).filename().string();
                if (process_name == name) {
                    CloseHandle(process_handle);
                    return pid_list[i];
                }
            }
            CloseHandle(process_handle);
        }
    }
#else
    DIR* dir = opendir("/proc");
    if (!dir) {
        if (error_callback_) {
            error_callback_("Failed to open /proc directory", errno);
        }
        return 0;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR ||
            !std::all_of(entry->d_name, entry->d_name + strlen(entry->d_name),
                         ::isdigit)) {
            continue;
        }

        std::string proc_dir_path = "/proc/" + std::string(entry->d_name);
        std::ifstream cmdline_file(proc_dir_path + "/cmdline");
        if (!cmdline_file.is_open()) {
            continue;
        }

        std::string cmdline;
        std::getline(cmdline_file, cmdline);
        std::replace(cmdline.begin(), cmdline.end(), '\0', ' ');

        size_t pos = cmdline.find_last_of('/');
        std::string proc_name =
            (pos != std::string::npos) ? cmdline.substr(pos + 1) : cmdline;

        if (proc_name.find(name) == 0) {
            closedir(dir);
            return std::stoi(entry->d_name);
        }
    }
    closedir(dir);
#endif
    return 0;
}

std::vector<pid_t> PidWatcher::getPidsByName(const std::string& name) const {
    std::vector<pid_t> results;
    results.reserve(16);

#ifdef _WIN32
    DWORD pid_list[1024];
    DWORD cb_needed;
    if (!EnumProcesses(pid_list, sizeof(pid_list), &cb_needed)) {
        return results;
    }

    for (unsigned int i = 0; i < cb_needed / sizeof(DWORD); i++) {
        HANDLE process_handle = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid_list[i]);
        if (process_handle != nullptr) {
            char filename[MAX_PATH];
            if (GetModuleFileNameEx(process_handle, nullptr, filename,
                                    MAX_PATH)) {
                std::string process_name =
                    std::filesystem::path(filename).filename().string();
                if (process_name == name) {
                    results.push_back(pid_list[i]);
                }
            }
            CloseHandle(process_handle);
        }
    }
#else
    DIR* dir = opendir("/proc");
    if (!dir) {
        if (error_callback_) {
            error_callback_("Failed to open /proc directory", errno);
        }
        return results;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR ||
            !std::all_of(entry->d_name, entry->d_name + strlen(entry->d_name),
                         ::isdigit)) {
            continue;
        }

        std::string proc_dir_path = "/proc/" + std::string(entry->d_name);
        std::ifstream cmdline_file(proc_dir_path + "/cmdline");
        if (!cmdline_file.is_open()) {
            continue;
        }

        std::string cmdline;
        std::getline(cmdline_file, cmdline);
        std::replace(cmdline.begin(), cmdline.end(), '\0', ' ');

        size_t pos = cmdline.find_last_of('/');
        std::string proc_name =
            (pos != std::string::npos) ? cmdline.substr(pos + 1) : cmdline;

        if (proc_name.find(name) == 0) {
            results.push_back(std::stoi(entry->d_name));
        }
    }
    closedir(dir);
#endif

    return results;
}

std::optional<ProcessInfo> PidWatcher::getProcessInfo(pid_t pid) {
    std::lock_guard lock(mutex_);

    auto it = monitored_processes_.find(pid);
    if (it != monitored_processes_.end()) {
        return it->second;
    }

    ProcessInfo info;
    info.pid = pid;

#ifdef _WIN32
    HANDLE process_handle =
        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (process_handle == NULL) {
        return std::nullopt;
    }

    char process_name[MAX_PATH];
    if (GetModuleFileNameEx(process_handle, NULL, process_name, MAX_PATH) ==
        0) {
        CloseHandle(process_handle);
        return std::nullopt;
    }
    info.name = std::filesystem::path(process_name).filename().string();
    info.command_line = getProcessCommandLine(pid);

    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe{};
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

    h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (h != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te{};
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

    DWORD exit_code;
    GetExitCodeProcess(process_handle, &exit_code);
    info.running = (exit_code == STILL_ACTIVE);
    info.status = info.running ? ProcessStatus::RUNNING : ProcessStatus::DEAD;

    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(process_handle, (PROCESS_MEMORY_COUNTERS*)&pmc,
                             sizeof(pmc))) {
        info.memory_usage = pmc.WorkingSetSize / 1024;
        info.virtual_memory = pmc.PrivateUsage / 1024;
    }

    info.priority = GetPriorityClass(process_handle);
    CloseHandle(process_handle);

    info.io_stats = getProcessIOStats(pid);
    info.cpu_usage = getProcessCpuUsage(pid);

    FILETIME creation_time, exit_time, kernel_time, user_time;
    process_handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process_handle != NULL) {
        if (GetProcessTimes(process_handle, &creation_time, &exit_time,
                            &kernel_time, &user_time)) {
            ULARGE_INTEGER li;
            li.LowPart = creation_time.dwLowDateTime;
            li.HighPart = creation_time.dwHighDateTime;
            auto epoch_diff = 116444736000000000ULL;
            auto unix_time = (li.QuadPart - epoch_diff) / 10000000ULL;
            info.start_time = std::chrono::system_clock::from_time_t(unix_time);

            auto now = std::chrono::system_clock::now();
            info.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - info.start_time);
        }
        CloseHandle(process_handle);
    }

    info.child_processes = getChildProcesses(pid);

#else
    std::string proc_path = "/proc/" + std::to_string(pid);
    if (!fs::exists(proc_path)) {
        return std::nullopt;
    }

    info.running = true;

    std::ifstream cmdline_file(proc_path + "/cmdline");
    if (cmdline_file.is_open()) {
        std::string cmdline;
        std::getline(cmdline_file, cmdline);
        info.command_line = cmdline;

        std::string display_cmdline = cmdline;
        std::replace(display_cmdline.begin(), display_cmdline.end(), '\0', ' ');

        size_t pos = display_cmdline.find_last_of('/');
        info.name = (pos != std::string::npos) ? display_cmdline.substr(pos + 1)
                                               : display_cmdline;

        if (auto space_pos = info.name.find(' ');
            space_pos != std::string::npos) {
            info.name = info.name.substr(0, space_pos);
        }
    }

    std::ifstream status_file(proc_path + "/status");
    if (status_file.is_open()) {
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.starts_with("VmRSS:")) {
                std::istringstream iss(line.substr(6));
                iss >> info.memory_usage;
            } else if (line.starts_with("VmSize:")) {
                std::istringstream iss(line.substr(7));
                iss >> info.virtual_memory;
            } else if (line.starts_with("PPid:")) {
                std::istringstream iss(line.substr(5));
                iss >> info.parent_pid;
            } else if (line.starts_with("Threads:")) {
                std::istringstream iss(line.substr(8));
                iss >> info.thread_count;
            } else if (line.starts_with("State:")) {
                char state_char = line[7];
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

    info.username = getProcessUsername(pid);

    std::ifstream smaps_file(proc_path + "/smaps");
    if (smaps_file.is_open()) {
        std::string line;
        while (std::getline(smaps_file, line)) {
            if (line.starts_with("Shared_C")) {
                std::istringstream iss(
                    line.substr(line.find_first_of("0123456789")));
                size_t shared;
                iss >> shared;
                info.shared_memory += shared;
            }
        }
    }

    std::ifstream stat_file(proc_path + "/stat");
    if (stat_file.is_open()) {
        std::string stat_content;
        std::getline(stat_file, stat_content);

        std::istringstream iss(stat_content);
        std::string value;
        for (int i = 0; i < 18; i++) {
            iss >> value;
        }
        iss >> info.priority;
    }

    struct stat stat_buf;
    if (stat(proc_path.c_str(), &stat_buf) == 0) {
        info.start_time =
            std::chrono::system_clock::from_time_t(stat_buf.st_ctime);
        auto now = std::chrono::system_clock::now();
        info.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - info.start_time);
    }

    info.cpu_usage = getProcessCpuUsage(pid);
    info.io_stats = getProcessIOStats(pid);
    info.child_processes = getChildProcesses(pid);
#endif

    return info;
}

std::vector<ProcessInfo> PidWatcher::getAllProcesses() {
    std::vector<ProcessInfo> result;
    result.reserve(256);

#ifdef _WIN32
    DWORD process_ids[1024], bytes_needed, num_processes;
    if (!EnumProcesses(process_ids, sizeof(process_ids), &bytes_needed)) {
        spdlog::error("Failed to enumerate processes");
        return result;
    }

    num_processes = bytes_needed / sizeof(DWORD);
    for (unsigned int i = 0; i < num_processes; i++) {
        if (process_ids[i] != 0) {
            if (auto info = getProcessInfo(process_ids[i])) {
                result.push_back(*info);
            }
        }
    }
#else
    DIR* dir = opendir("/proc");
    if (!dir) {
        spdlog::error("Failed to open /proc directory");
        return result;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR || !std::isdigit(entry->d_name[0])) {
            continue;
        }

        pid_t pid = std::stoi(entry->d_name);
        if (auto info = getProcessInfo(pid)) {
            result.push_back(*info);
        }
    }

    closedir(dir);
#endif

    return result;
}

std::vector<pid_t> PidWatcher::getChildProcesses(pid_t pid) const {
    std::vector<pid_t> children;
    children.reserve(16);

#ifdef _WIN32
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe{};
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
                if (line.starts_with("PPid:")) {
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

bool PidWatcher::start(const std::string& name, const MonitorConfig* config) {
    std::lock_guard lock(mutex_);

    pid_t pid = getPidByName(name);
    if (pid == 0) {
        spdlog::error("Failed to get PID for {}", name);
        if (error_callback_) {
            error_callback_("Failed to get PID for " + name, ESRCH);
        }
        return false;
    }

    if (monitored_processes_.contains(pid)) {
        spdlog::info("Already monitoring process {} ({})", pid, name);
        return true;
    }

    ProcessInfo info;
    info.pid = pid;
    info.name = name;
    info.running = true;
    info.start_time = std::chrono::system_clock::now();

    if (auto detailed_info = getProcessInfo(pid)) {
        info = *detailed_info;
    }

    if (config) {
        process_configs_[pid] = *config;
    }

    monitored_processes_[pid] = info;

    if (monitored_processes_.size() == 1) {
        primary_pid_ = pid;
    }

    if (!running_) {
        running_ = true;
        monitoring_ = true;
        watchdog_healthy_ = true;

        monitor_thread_ = std::thread(&PidWatcher::monitorThread, this);
        exit_thread_ = std::thread(&PidWatcher::exitThread, this);
        multi_monitor_thread_ =
            std::thread(&PidWatcher::multiMonitorThread, this);
        resource_monitor_thread_ =
            std::thread(&PidWatcher::resourceMonitorThread, this);
        auto_restart_thread_ =
            std::thread(&PidWatcher::autoRestartThread, this);
        watchdog_thread_ = std::thread(&PidWatcher::watchdogThread, this);
    } else {
        monitor_cv_.notify_one();
        multi_monitor_cv_.notify_one();
        resource_monitor_cv_.notify_one();
    }

    spdlog::info("PidWatcher started for process: {}", name);
    return true;
}

bool PidWatcher::startByPid(pid_t pid, const MonitorConfig* config) {
    std::lock_guard lock(mutex_);

    if (!isProcessRunning(pid)) {
        spdlog::error("Process with PID {} does not exist", pid);
        if (error_callback_) {
            error_callback_(
                "Process with PID " + std::to_string(pid) + " does not exist",
                ESRCH);
        }
        return false;
    }

    if (monitored_processes_.contains(pid)) {
        spdlog::info("Already monitoring process {}", pid);
        return true;
    }

    auto info_opt = getProcessInfo(pid);
    if (!info_opt) {
        spdlog::error("Failed to get info for PID {}", pid);
        if (error_callback_) {
            error_callback_("Failed to get info for PID " + std::to_string(pid),
                            EINVAL);
        }
        return false;
    }

    if (config) {
        process_configs_[pid] = *config;
    }

    monitored_processes_[pid] = *info_opt;

    if (monitored_processes_.size() == 1) {
        primary_pid_ = pid;
    }

    if (!running_) {
        running_ = true;
        monitoring_ = true;
        watchdog_healthy_ = true;

        monitor_thread_ = std::thread(&PidWatcher::monitorThread, this);
        exit_thread_ = std::thread(&PidWatcher::exitThread, this);
        multi_monitor_thread_ =
            std::thread(&PidWatcher::multiMonitorThread, this);
        resource_monitor_thread_ =
            std::thread(&PidWatcher::resourceMonitorThread, this);
        auto_restart_thread_ =
            std::thread(&PidWatcher::autoRestartThread, this);
        watchdog_thread_ = std::thread(&PidWatcher::watchdogThread, this);
    } else {
        monitor_cv_.notify_one();
        multi_monitor_cv_.notify_one();
        resource_monitor_cv_.notify_one();
    }

    spdlog::info("PidWatcher started for PID: {}", pid);
    return true;
}

size_t PidWatcher::startMultiple(const std::vector<std::string>& process_names,
                                 const MonitorConfig* config) {
    std::lock_guard lock(mutex_);

    size_t success_count = 0;

    for (const auto& name : process_names) {
        pid_t pid = getPidByName(name);
        if (pid == 0) {
            spdlog::warn("Failed to get PID for {}", name);
            if (error_callback_) {
                error_callback_("Failed to get PID for " + name, ESRCH);
            }
            continue;
        }

        if (monitored_processes_.contains(pid)) {
            spdlog::info("Already monitoring process {} ({})", pid, name);
            success_count++;
            continue;
        }

        ProcessInfo info;
        info.pid = pid;
        info.name = name;
        info.running = true;
        info.start_time = std::chrono::system_clock::now();

        if (auto detailed_info = getProcessInfo(pid)) {
            info = *detailed_info;
        }

        if (config) {
            process_configs_[pid] = *config;
        }

        if (process_filter_ && !process_filter_(info)) {
            spdlog::info("Process {}/{} filtered out by custom filter", pid,
                         name);
            continue;
        }

        monitored_processes_[pid] = info;
        success_count++;

        if (monitored_processes_.size() == 1) {
            primary_pid_ = pid;
        }
    }

    if (success_count > 0 && !running_) {
        running_ = true;
        monitoring_ = true;
        watchdog_healthy_ = true;

        monitor_thread_ = std::thread(&PidWatcher::monitorThread, this);
        exit_thread_ = std::thread(&PidWatcher::exitThread, this);
        multi_monitor_thread_ =
            std::thread(&PidWatcher::multiMonitorThread, this);
        resource_monitor_thread_ =
            std::thread(&PidWatcher::resourceMonitorThread, this);
        auto_restart_thread_ =
            std::thread(&PidWatcher::autoRestartThread, this);
        watchdog_thread_ = std::thread(&PidWatcher::watchdogThread, this);
    } else if (success_count > 0) {
        monitor_cv_.notify_one();
        multi_monitor_cv_.notify_one();
        resource_monitor_cv_.notify_one();
    }

    spdlog::info("Started monitoring {} processes out of {}", success_count,
                 process_names.size());
    return success_count;
}

void PidWatcher::stop() {
    {
        std::lock_guard lock(mutex_);

        if (!running_) {
            return;
        }

        running_ = false;
        monitoring_ = false;
        watchdog_healthy_ = false;

        exit_cv_.notify_all();
        monitor_cv_.notify_all();
        multi_monitor_cv_.notify_all();
        resource_monitor_cv_.notify_all();
        auto_restart_cv_.notify_all();
        watchdog_cv_.notify_all();

        monitored_processes_.clear();
        process_configs_.clear();
    }

    if (monitor_thread_.joinable())
        monitor_thread_.join();
    if (exit_thread_.joinable())
        exit_thread_.join();
    if (multi_monitor_thread_.joinable())
        multi_monitor_thread_.join();
    if (resource_monitor_thread_.joinable())
        resource_monitor_thread_.join();
    if (auto_restart_thread_.joinable())
        auto_restart_thread_.join();
    if (watchdog_thread_.joinable())
        watchdog_thread_.join();

    spdlog::info("PidWatcher stopped");
}

bool PidWatcher::stopProcess(pid_t pid) {
    std::lock_guard lock(mutex_);

    auto it = monitored_processes_.find(pid);
    if (it == monitored_processes_.end()) {
        spdlog::warn("Process {} is not being monitored", pid);
        return false;
    }

    spdlog::info("Stopping monitoring for process {} ({})", pid,
                 it->second.name);

    process_configs_.erase(pid);
    monitored_processes_.erase(it);

    if (primary_pid_ == pid && !monitored_processes_.empty()) {
        primary_pid_ = monitored_processes_.begin()->first;
        spdlog::info("Updated primary PID to {}", primary_pid_.load());
    }

    if (monitored_processes_.empty()) {
        running_ = false;
        monitoring_ = false;

        exit_cv_.notify_all();
        monitor_cv_.notify_all();
        multi_monitor_cv_.notify_all();
        resource_monitor_cv_.notify_all();
        auto_restart_cv_.notify_all();
    }

    return true;
}

bool PidWatcher::switchToProcess(const std::string& name) {
    std::lock_guard lock(mutex_);

    if (!running_) {
        spdlog::error("Not running");
        return false;
    }

    pid_t new_pid = getPidByName(name);
    if (new_pid == 0) {
        spdlog::error("Failed to get PID for {}", name);
        if (error_callback_) {
            error_callback_("Failed to get PID for " + name, ESRCH);
        }
        return false;
    }

    if (monitored_processes_.contains(new_pid)) {
        spdlog::info("Already monitoring process {} ({}), making primary",
                     new_pid, name);
        primary_pid_ = new_pid;
        return true;
    }

    ProcessInfo info;
    info.pid = new_pid;
    info.name = name;
    info.running = true;
    info.start_time = std::chrono::system_clock::now();

    if (auto detailed_info = getProcessInfo(new_pid)) {
        info = *detailed_info;
    }

    primary_pid_ = new_pid;
    monitored_processes_[new_pid] = info;

    monitor_cv_.notify_one();
    multi_monitor_cv_.notify_one();
    resource_monitor_cv_.notify_one();

    spdlog::info("PidWatcher switched to process: {}", name);
    return true;
}

bool PidWatcher::switchToProcessById(pid_t pid) {
    std::lock_guard lock(mutex_);

    if (!running_) {
        spdlog::error("Not running");
        return false;
    }

    if (!isProcessRunning(pid)) {
        spdlog::error("Process {} is not running", pid);
        if (error_callback_) {
            error_callback_(
                "Process " + std::to_string(pid) + " is not running", ESRCH);
        }
        return false;
    }

    if (monitored_processes_.contains(pid)) {
        spdlog::info("Already monitoring process {}, making primary", pid);
        primary_pid_ = pid;
        return true;
    }

    auto info_opt = getProcessInfo(pid);
    if (!info_opt) {
        spdlog::error("Failed to get info for process {}", pid);
        if (error_callback_) {
            error_callback_(
                "Failed to get info for process " + std::to_string(pid),
                EINVAL);
        }
        return false;
    }

    primary_pid_ = pid;
    monitored_processes_[pid] = *info_opt;

    monitor_cv_.notify_one();
    multi_monitor_cv_.notify_one();
    resource_monitor_cv_.notify_one();

    spdlog::info("PidWatcher switched to PID: {}", pid);
    return true;
}

bool PidWatcher::isActive() const {
    std::lock_guard lock(mutex_);
    return running_ && !monitored_processes_.empty();
}

bool PidWatcher::isMonitoring(pid_t pid) const {
    std::lock_guard lock(mutex_);
    return monitored_processes_.contains(pid);
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
    return kill(pid, 0) == 0;
#endif
}

double PidWatcher::getProcessCpuUsage(pid_t pid) const {
    std::string proc_path = "/proc/" + std::to_string(pid);

    if (!fs::exists(proc_path)) {
        return -1.0;
    }

    std::lock_guard lock(mutex_);
    auto it = cpu_usage_data_.find(pid);
    bool first_call = (it == cpu_usage_data_.end());

    std::ifstream stat_file(proc_path + "/stat");
    if (!stat_file.is_open()) {
        return -1.0;
    }

    std::string line;
    std::getline(stat_file, line);
    std::istringstream iss(line);

    std::vector<std::string> stat_values;
    stat_values.reserve(20);
    std::string value;
    while (iss >> value && stat_values.size() < 17) {
        stat_values.push_back(value);
    }

    if (stat_values.size() < 15) {
        return -1.0;
    }

    uint64_t utime = std::stoull(stat_values[13]);
    uint64_t stime = std::stoull(stat_values[14]);
    uint64_t total_time = utime + stime;

    std::ifstream proc_stat("/proc/stat");
    if (!proc_stat.is_open()) {
        return -1.0;
    }

    std::getline(proc_stat, line);
    iss = std::istringstream(line);

    std::string cpu_label;
    uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
    iss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >>
        softirq >> steal;

    uint64_t total_cpu_time =
        user + nice + system + idle + iowait + irq + softirq + steal;

    auto now = std::chrono::steady_clock::now();

    if (first_call) {
        CPUUsageData data;
        data.last_total_user = user;
        data.last_total_user_low = nice;
        data.last_total_sys = system;
        data.last_total_idle = idle;
        data.last_update = now;
        cpu_usage_data_[pid] = data;
        return 0.0;
    } else {
        CPUUsageData& last = cpu_usage_data_[pid];

        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last.last_update)
                .count() < 200) {
            return 0.0;
        }

        double percent = 0.0;

        if (total_cpu_time > (last.last_total_user + last.last_total_user_low +
                              last.last_total_sys + last.last_total_idle)) {
            percent = (total_time * 100.0) /
                      (total_cpu_time -
                       (last.last_total_user + last.last_total_user_low +
                        last.last_total_sys + last.last_total_idle));
        }

        last.last_total_user = user;
        last.last_total_user_low = nice;
        last.last_total_sys = system;
        last.last_total_idle = idle;
        last.last_update = now;

        return percent;
    }
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
        return pmc.WorkingSetSize / 1024;
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
        if (line.starts_with("VmRSS:")) {
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
        if (line.starts_with("Threads:")) {
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
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process == NULL) {
        return stats;
    }

    IO_COUNTERS io_counters;
    if (GetProcessIoCounters(process, &io_counters)) {
        stats.read_bytes = io_counters.ReadTransferCount;
        stats.write_bytes = io_counters.WriteTransferCount;

        auto it = prev_io_stats_.find(pid);
        if (it != prev_io_stats_.end()) {
            auto now = std::chrono::steady_clock::now();
            auto last = it->second;
            auto time_diff = now - last.last_update;

            double seconds = std::chrono::duration<double>(time_diff).count();
            if (seconds > 0) {
                stats.read_rate =
                    (stats.read_bytes - last.read_bytes) / seconds;
                stats.write_rate =
                    (stats.write_bytes - last.write_bytes) / seconds;
            }
            stats.last_update = now;
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

    auto it = prev_io_stats_.find(pid);
    if (it != prev_io_stats_.end()) {
        prev_read = it->second.read_bytes;
        prev_write = it->second.write_bytes;
    }

    while (std::getline(io_file, line)) {
        if (line.starts_with("read_bytes")) {
            std::istringstream iss(
                line.substr(line.find_first_of("0123456789")));
            iss >> stats.read_bytes;
        } else if (line.starts_with("write_bytes")) {
            std::istringstream iss(
                line.substr(line.find_first_of("0123456789")));
            iss >> stats.write_bytes;
        }
    }

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
        if (line.starts_with("State:")) {
            char state_char = line[7];
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

    ULARGE_INTEGER create_time_li;
    create_time_li.LowPart = creation_time.dwLowDateTime;
    create_time_li.HighPart = creation_time.dwHighDateTime;

    FILETIME current_time;
    GetSystemTimeAsFileTime(&current_time);

    ULARGE_INTEGER current_time_li;
    current_time_li.LowPart = current_time.dwLowDateTime;
    current_time_li.HighPart = current_time.dwHighDateTime;

    uint64_t diff = current_time_li.QuadPart - create_time_li.QuadPart;

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

    std::ifstream uptime_file("/proc/uptime");
    if (!uptime_file.is_open()) {
        return std::chrono::milliseconds(0);
    }

    double system_uptime;
    uptime_file >> system_uptime;
    uptime_file.close();

    std::istringstream iss(content);
    std::string field;

    for (int i = 1; i < 22; i++) {
        iss >> field;
    }

    unsigned long long start_time;
    iss >> start_time;

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
    spdlog::info("Launching process: {}", command);

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

    if (!CreateProcess(NULL, const_cast<char*>(cmd_line.c_str()), NULL, NULL,
                       FALSE, 0, NULL, NULL, &si, &pi)) {
        spdlog::error("CreateProcess failed: {}", GetLastError());
        if (error_callback_) {
            error_callback_("Failed to launch process: " + command,
                            GetLastError());
        }
        return 0;
    }

    spdlog::info("Process launched with PID: {}", pi.dwProcessId);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    pid_t new_pid = static_cast<pid_t>(pi.dwProcessId);
#else
    pid_t new_pid = fork();

    if (new_pid < 0) {
        spdlog::error("Fork failed: {}", strerror(errno));
        if (error_callback_) {
            error_callback_("Fork failed when launching process: " + command,
                            errno);
        }
        return 0;
    }

    if (new_pid == 0) {
        std::vector<char*> c_args;
        c_args.push_back(const_cast<char*>(command.c_str()));

        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        execvp(command.c_str(), c_args.data());

        exit(EXIT_FAILURE);
    }

    spdlog::info("Process launched with PID: {}", new_pid);
#endif

    if (process_create_callback_) {
        process_create_callback_(new_pid, command);
    }

    if (auto_monitor) {
        std::thread([this, new_pid]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            this->startByPid(new_pid);
        }).detach();
    }

    return new_pid;
}

bool PidWatcher::terminateProcess(pid_t pid, bool force) {
    spdlog::info("Terminating process: {} (force: {})", pid, force);

#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (process == NULL) {
        spdlog::error("Failed to open process for termination: {}",
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
        spdlog::error("Failed to terminate process: {}", GetLastError());
        if (error_callback_) {
            error_callback_("Failed to terminate process", GetLastError());
        }
        return false;
    }
#else
    int signal_num = force ? SIGKILL : SIGTERM;
    if (kill(pid, signal_num) != 0) {
        spdlog::error("Failed to send signal to process: {}", strerror(errno));
        if (error_callback_) {
            error_callback_("Failed to send signal to process", errno);
        }
        return false;
    }
#endif

    spdlog::info("Signal sent to process {} successfully", pid);

    if (isMonitoring(pid)) {
        std::thread([this, pid]() {
            for (int i = 0; i < 50; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (!isProcessRunning(pid)) {
                    std::lock_guard lock(this->mutex_);
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
        process_configs_[pid] = global_config_;
    }

    process_configs_[pid].resource_limits = limits;

    spdlog::info("Set resource limits for process {}: CPU {}%, Memory {} KB",
                 pid, limits.max_cpu_percent, limits.max_memory_kb);

    return true;
}

bool PidWatcher::setProcessPriority(pid_t pid, int priority) {
    spdlog::info("Setting process {} priority to {}", pid, priority);

#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (process == NULL) {
        spdlog::error("Failed to open process for priority change: {}",
                      GetLastError());
        if (error_callback_) {
            error_callback_("Failed to open process for priority change",
                            GetLastError());
        }
        return false;
    }

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
        spdlog::error("Failed to set process priority: {}", GetLastError());
        if (error_callback_) {
            error_callback_("Failed to set process priority", GetLastError());
        }
        return false;
    }
#else
    if (setpriority(PRIO_PROCESS, pid, priority) != 0) {
        spdlog::error("Failed to set process priority: {}", strerror(errno));
        if (error_callback_) {
            error_callback_("Failed to set process priority", errno);
        }
        return false;
    }
#endif

    {
        std::lock_guard lock(mutex_);
        auto it = monitored_processes_.find(pid);
        if (it != monitored_processes_.end()) {
            it->second.priority = priority;
        }
    }

    spdlog::info("Successfully set process {} priority to {}", pid, priority);
    return true;
}

bool PidWatcher::configureAutoRestart(pid_t pid, bool enable,
                                      int max_attempts) {
    std::lock_guard lock(mutex_);

    auto it = process_configs_.find(pid);
    if (it == process_configs_.end()) {
        process_configs_[pid] = global_config_;
    }

    process_configs_[pid].auto_restart = enable;
    process_configs_[pid].max_restart_attempts = max_attempts;

    if (enable) {
        restart_attempts_[pid] = 0;
    } else {
        restart_attempts_.erase(pid);
    }

    spdlog::info("{} auto-restart for process {} (max attempts: {})",
                 enable ? "Enabled" : "Disabled", pid, max_attempts);

    auto_restart_cv_.notify_one();

    return true;
}

pid_t PidWatcher::restartProcess(pid_t pid) {
    spdlog::info("Restarting process: {}", pid);

    auto info_opt = getProcessInfo(pid);
    if (!info_opt) {
        spdlog::error("Failed to get process info for restart");
        if (error_callback_) {
            error_callback_("Failed to get process info for restart", EINVAL);
        }
        return 0;
    }

    std::string command = info_opt->command_line;

    if (!terminateProcess(pid, false)) {
        spdlog::warn("Failed to terminate process, trying with force");
        if (!terminateProcess(pid, true)) {
            spdlog::error("Failed to terminate process even with force");
            return 0;
        }
    }

    for (int i = 0; i < 50; i++) {
        if (!isProcessRunning(pid)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (isProcessRunning(pid)) {
        spdlog::error("Process {} did not terminate in time", pid);
        return 0;
    }

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
        spdlog::error("Failed to restart process");
        return 0;
    }

    if (!startByPid(new_pid, &config)) {
        spdlog::warn("Failed to start monitoring restarted process");
    }

    spdlog::info("Process restarted with new PID: {}", new_pid);
    return new_pid;
}

bool PidWatcher::dumpProcessInfo(pid_t pid, bool detailed,
                                 const std::string& output_file) {
    spdlog::info("Dumping process info for PID: {} (detailed: {})", pid,
                 detailed);

    auto info_opt = getProcessInfo(pid);
    if (!info_opt) {
        spdlog::error("Failed to get process info for dumping");
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
        spdlog::info("{}", dump);
    } else {
        std::ofstream file(output_file);
        if (!file.is_open()) {
            spdlog::error("Failed to open output file: {}", output_file);
            if (error_callback_) {
                error_callback_("Failed to open output file", errno);
            }
            return false;
        }
        file << dump;
        spdlog::info("Process information dumped to {}", output_file);
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
    spdlog::info("Set rate limiting to {} updates per second",
                 max_updates_per_second_.load());
    return *this;
}

bool PidWatcher::checkRateLimit() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - rate_limit_start_time_);

    if (elapsed.count() >= 1) {
        rate_limit_start_time_ = now;
        update_count_ = 1;
        return true;
    } else if (update_count_ < max_updates_per_second_) {
        update_count_++;
        return true;
    } else {
        return false;
    }
}

void PidWatcher::monitorChildProcesses(pid_t parent_pid) {
    std::vector<pid_t> children = getChildProcesses(parent_pid);

    for (pid_t child_pid : children) {
        if (isMonitoring(child_pid)) {
            continue;
        }

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

        spdlog::info("Auto-monitoring child process {} of parent {}", child_pid,
                     parent_pid);
        startByPid(child_pid, &child_config);

        if (child_config.monitor_children) {
            monitorChildProcesses(child_pid);
        }
    }
}

std::string PidWatcher::getProcessUsername([[maybe_unused]] pid_t pid) const {
#ifdef _WIN32
    return "";
#else
    std::string proc_path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream status_file(proc_path);
    if (!status_file.is_open()) {
        return "";
    }

    std::string line;
    while (std::getline(status_file, line)) {
        if (line.starts_with("Uid:")) {
            std::istringstream iss(line.substr(4));
            uid_t uid;
            iss >> uid;

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
        if (line.starts_with("read_bytes")) {
            std::istringstream iss(
                line.substr(line.find_first_of("0123456789")));
            iss >> stats.read_bytes;
        } else if (line.starts_with("write_bytes")) {
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
        return;
    }

    const auto& limits = config_it->second.resource_limits;
    bool limit_exceeded = false;

    if (limits.max_cpu_percent > 0.0 &&
        info.cpu_usage > limits.max_cpu_percent) {
        spdlog::warn("Process {} exceeded CPU limit: {:.2f}% > {:.2f}%", pid,
                     info.cpu_usage, limits.max_cpu_percent);
        limit_exceeded = true;
    }

    if (limits.max_memory_kb > 0 && info.memory_usage > limits.max_memory_kb) {
        spdlog::warn("Process {} exceeded memory limit: {} KB > {} KB", pid,
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

    auto it = monitored_processes_.find(pid);
    if (it != monitored_processes_.end()) {
        info = it->second;
    }

    info.running = isProcessRunning(pid);

    if (info.running) {
        auto detailed_info = getProcessInfo(pid);
        if (detailed_info) {
            info = *detailed_info;
        } else {
            info.cpu_usage = getProcessCpuUsage(pid);
            info.memory_usage = getProcessMemoryUsage(pid);
            info.thread_count = getProcessThreadCount(pid);
            info.status = getProcessStatus(pid);
            updateIOStats(pid, info.io_stats);
        }

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

    monitored_processes_[pid] = info;

    return info;
}

void PidWatcher::monitorThread() {
    spdlog::info("Monitor thread started");
    while (true) {
        std::unique_lock lock(mutex_);

        while (!monitoring_ && running_) {
            monitor_cv_.wait(lock);
        }

        if (!running_) {
            spdlog::info("Monitor thread exiting");
            break;
        }

        if (!checkRateLimit()) {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        ProcessInfo info = updateProcessInfo(primary_pid_);

        auto config_it = process_configs_.find(primary_pid_);
        bool monitor_children = false;
        if (config_it != process_configs_.end()) {
            monitor_children = config_it->second.monitor_children;
        } else {
            monitor_children = global_config_.monitor_children;
        }

        if (monitor_children && info.running) {
            lock.unlock();
            monitorChildProcesses(primary_pid_);
            lock.lock();
        }

        if (monitor_callback_ && info.running) {
            spdlog::info("Executing monitor callback for PID {}",
                         primary_pid_.load());
            lock.unlock();
            monitor_callback_(info);
            lock.lock();
        }

        if (!info.running) {
            spdlog::info("Process {} has exited", primary_pid_.load());

            if (exit_callback_) {
                lock.unlock();
                exit_callback_(info);
                lock.lock();
            }

            if (monitored_processes_.size() <= 1) {
                running_ = false;
                monitoring_ = false;
                break;
            } else {
                monitored_processes_.erase(primary_pid_);

                if (!monitored_processes_.empty()) {
                    auto first_proc = monitored_processes_.begin();
                    primary_pid_ = first_proc->first;
                    spdlog::info("Switching primary monitor to PID {}",
                                 primary_pid_.load());
                }
            }
        }

        watchdog_healthy_ = true;

        lock.unlock();

        if (monitor_interval_.count() > 0) {
            std::this_thread::sleep_for(monitor_interval_);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    spdlog::info("Monitor thread exited");
}

void PidWatcher::exitThread() {
    spdlog::info("Exit thread started");
    while (true) {
        std::unique_lock lock(mutex_);

        if (!running_) {
            spdlog::info("Exit thread exiting");
            break;
        }

        exit_cv_.wait_for(lock, std::chrono::seconds(1));

        if (!running_) {
            spdlog::info("Exit thread exiting");
            break;
        }

        std::vector<pid_t> exited_processes;

        for (auto& pair : monitored_processes_) {
            pid_t current_pid = pair.first;
            ProcessInfo& info = pair.second;

            if (info.running && !isProcessRunning(current_pid)) {
                spdlog::info("Process {} has exited", current_pid);
                info.running = false;
                info.status = ProcessStatus::DEAD;

                if (exit_callback_) {
                    lock.unlock();
                    exit_callback_(info);
                    lock.lock();
                }

                exited_processes.push_back(current_pid);
            }
        }

        for (pid_t pid : exited_processes) {
            auto it = monitored_processes_.find(pid);
            if (it != monitored_processes_.end()) {
                it->second.running = false;
                it->second.status = ProcessStatus::DEAD;

                if (primary_pid_ == pid && monitored_processes_.size() > 1) {
                    bool found = false;
                    for (auto& pair : monitored_processes_) {
                        if (pair.first != pid && pair.second.running) {
                            primary_pid_ = pair.first;
                            found = true;
                            spdlog::info("Switching primary monitor to PID {}",
                                         primary_pid_.load());
                            break;
                        }
                    }

                    if (!found) {
                        auto it = monitored_processes_.begin();
                        if (it->first != pid) {
                            primary_pid_ = it->first;
                        } else if (monitored_processes_.size() > 1) {
                            primary_pid_ = std::next(it)->first;
                        }
                        spdlog::info(
                            "No running processes, switching primary monitor "
                            "to PID {}",
                            primary_pid_.load());
                    }
                }
            }
        }

        auto_restart_cv_.notify_one();

        watchdog_healthy_ = true;
    }
    spdlog::info("Exit thread exited");
}

void PidWatcher::multiMonitorThread() {
    spdlog::info("Multi-monitor thread started");

    while (true) {
        std::unique_lock lock(mutex_);

        if (!running_ || monitored_processes_.empty()) {
            spdlog::info("Multi-monitor thread exiting");
            break;
        }

        multi_monitor_cv_.wait_for(lock, monitor_interval_);

        if (!running_ || monitored_processes_.empty()) {
            spdlog::info("Multi-monitor thread exiting");
            break;
        }

        if (!checkRateLimit()) {
            continue;
        }

        std::vector<ProcessInfo> process_infos;

        for (auto& pair : monitored_processes_) {
            pid_t current_pid = pair.first;

            if (!pair.second.running) {
                continue;
            }

            ProcessInfo info = updateProcessInfo(current_pid);

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

        if (multi_process_callback_ && !process_infos.empty()) {
            lock.unlock();
            multi_process_callback_(process_infos);
            lock.lock();
        }

        watchdog_healthy_ = true;
    }

    spdlog::info("Multi-monitor thread exited");
}

void PidWatcher::resourceMonitorThread() {
    spdlog::info("Resource monitor thread started");

    while (true) {
        std::unique_lock lock(mutex_);

        if (!running_ || monitored_processes_.empty()) {
            spdlog::info("Resource monitor thread exiting");
            break;
        }

        resource_monitor_cv_.wait_for(lock, std::chrono::seconds(1));

        if (!running_ || monitored_processes_.empty()) {
            spdlog::info("Resource monitor thread exiting");
            break;
        }

        for (auto& pair : monitored_processes_) {
            pid_t current_pid = pair.first;
            const ProcessInfo& info = pair.second;

            if (!info.running) {
                continue;
            }

            lock.unlock();
            checkResourceLimits(current_pid, info);
            lock.lock();
        }

        watchdog_healthy_ = true;
    }

    spdlog::info("Resource monitor thread exited");
}

void PidWatcher::autoRestartThread() {
    spdlog::info("Auto-restart thread started");

    while (true) {
        std::unique_lock lock(mutex_);

        if (!running_) {
            spdlog::info("Auto-restart thread exiting");
            break;
        }

        auto_restart_cv_.wait_for(lock, std::chrono::seconds(1));

        if (!running_) {
            spdlog::info("Auto-restart thread exiting");
            break;
        }

        std::vector<pid_t> to_restart;
        std::vector<pid_t> to_remove;

        for (auto& pair : monitored_processes_) {
            pid_t current_pid = pair.first;
            const ProcessInfo& info = pair.second;

            if (!info.running) {
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
                    int attempts = restart_attempts_[current_pid];
                    if (attempts < max_attempts) {
                        to_restart.push_back(current_pid);
                        restart_attempts_[current_pid]++;
                    } else {
                        to_remove.push_back(current_pid);
                    }
                } else {
                    to_remove.push_back(current_pid);
                }
            }
        }

        lock.unlock();

        for (pid_t pid : to_restart) {
            spdlog::info("Auto-restarting process {}", pid);
            pid_t new_pid = restartProcess(pid);

            if (new_pid == 0) {
                spdlog::error("Failed to auto-restart process {}", pid);
            } else {
                spdlog::info("Process {} restarted as PID {}", pid, new_pid);
            }
        }

        lock.lock();

        for (pid_t pid : to_remove) {
            monitored_processes_.erase(pid);
            process_configs_.erase(pid);
            restart_attempts_.erase(pid);

            if (primary_pid_ == pid && !monitored_processes_.empty()) {
                primary_pid_ = monitored_processes_.begin()->first;
                spdlog::info("Switching primary monitor to PID {}",
                             primary_pid_.load());
            }
        }

        if (monitored_processes_.empty()) {
            running_ = false;
            monitoring_ = false;
            break;
        }

        watchdog_healthy_ = true;
    }

    spdlog::info("Auto-restart thread exited");
}

void PidWatcher::watchdogThread() {
    spdlog::info("Watchdog thread started");

    int unhealthy_count = 0;
    const int max_unhealthy_count = 3;

    while (true) {
        std::unique_lock lock(mutex_);

        if (!running_) {
            spdlog::info("Watchdog thread exiting");
            break;
        }

        watchdog_cv_.wait_for(lock, std::chrono::seconds(5));

        if (!running_) {
            spdlog::info("Watchdog thread exiting");
            break;
        }

        if (!watchdog_healthy_) {
            unhealthy_count++;
            spdlog::warn("Watchdog detected unhealthy state ({}/{})",
                         unhealthy_count, max_unhealthy_count);

            if (unhealthy_count >= max_unhealthy_count) {
                spdlog::error(
                    "Watchdog detected system hung, attempting recovery");

                bool need_restart = false;

                running_ = false;

                exit_cv_.notify_all();
                monitor_cv_.notify_all();
                multi_monitor_cv_.notify_all();
                resource_monitor_cv_.notify_all();
                auto_restart_cv_.notify_all();

                lock.unlock();

                std::this_thread::sleep_for(std::chrono::seconds(1));

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
                    spdlog::info("Watchdog restarting monitoring system");
                    running_ = true;
                    monitoring_ = true;
                    watchdog_healthy_ = true;
                    unhealthy_count = 0;

                    monitor_thread_ =
                        std::thread(&PidWatcher::monitorThread, this);
                    exit_thread_ = std::thread(&PidWatcher::exitThread, this);
                    multi_monitor_thread_ =
                        std::thread(&PidWatcher::multiMonitorThread, this);
                    resource_monitor_thread_ =
                        std::thread(&PidWatcher::resourceMonitorThread, this);
                    auto_restart_thread_ =
                        std::thread(&PidWatcher::autoRestartThread, this);
                } else {
                    spdlog::error(
                        "Watchdog cannot recover, shutting down PidWatcher");
                    running_ = false;
                    monitoring_ = false;
                    monitored_processes_.clear();
                    break;
                }
            }
        } else {
            if (unhealthy_count > 0) {
                spdlog::info("Watchdog detected system recovered");
            }
            unhealthy_count = 0;
            watchdog_healthy_ = false;
        }
    }

    spdlog::info("Watchdog thread exited");
}

}  // namespace atom::system