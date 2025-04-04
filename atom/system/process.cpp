#include "process.hpp"

#include "command.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <regex>
#include <thread>
#include <unordered_map>

#if defined(_WIN32)
// clang-format off
#include <windows.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <tchar.h>
#include <psapi.h>
#include <pdh.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "pdh.lib")
// clang-format on
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
#include <sys/time.h>
#include <sys/times.h>
#elif defined(__APPLE__)
#include <libproc.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/task.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#else
#error "Unknown platform"
#endif

#include "atom/log/loguru.hpp"
#include "atom/utils/convert.hpp"

namespace atom::system {

constexpr int BUFFER_SIZE = 1024;
namespace {
class ProcessMonitorManager {
public:
    static ProcessMonitorManager &getInstance() {
        static ProcessMonitorManager instance;
        return instance;
    }

    int startMonitoring(int pid,
                        std::function<void(int, const std::string &)> callback,
                        unsigned int intervalMs) {
        std::lock_guard<std::mutex> lock(mutex_);
        int monitorId = nextMonitorId_++;

        auto task = std::async(
            std::launch::async, [this, pid, callback, intervalMs, monitorId]() {
                std::string lastStatus;
                while (!shouldStop(monitorId)) {
                    try {
                        Process info = getProcessInfoByPid(pid);
                        if (info.status != lastStatus) {
                            lastStatus = info.status;
                            callback(pid, lastStatus);
                        }

                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(intervalMs));
                    } catch (const std::exception &e) {
                        // 进程可能已经结束
                        callback(pid, "Terminated");
                        break;
                    }
                }

                std::lock_guard<std::mutex> taskLock(mutex_);
                tasks_.erase(monitorId);
            });

        tasks_[monitorId] = std::move(task);
        return monitorId;
    }

    bool stopMonitoring(int monitorId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tasks_.find(monitorId);
        if (it != tasks_.end()) {
            stopFlags_[monitorId] = true;
            return true;
        }
        return false;
    }

private:
    ProcessMonitorManager() : nextMonitorId_(1) {}
    ~ProcessMonitorManager() {
        for (auto &id : tasks_) {
            stopFlags_[id.first] = true;
        }

        for (auto &task : tasks_) {
            if (task.second.valid()) {
                task.second.wait();
            }
        }
    }

    bool shouldStop(int monitorId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = stopFlags_.find(monitorId);
        if (it != stopFlags_.end()) {
            return it->second;
        }
        return false;
    }

    std::mutex mutex_;
    std::atomic<int> nextMonitorId_;
    std::unordered_map<int, std::future<void>> tasks_;
    std::unordered_map<int, bool> stopFlags_;
};

// 资源监控管理器
class ResourceMonitorManager {
public:
    static ResourceMonitorManager &getInstance() {
        static ResourceMonitorManager instance;
        return instance;
    }

    int startMonitoring(
        int pid, const std::string &resourceType, double threshold,
        std::function<void(int, const std::string &, double)> callback,
        unsigned int intervalMs) {
        std::lock_guard<std::mutex> lock(mutex_);
        int monitorId = nextMonitorId_++;

        auto task = std::async(std::launch::async, [this, pid, resourceType,
                                                    threshold, callback,
                                                    intervalMs, monitorId]() {
            while (!shouldStop(monitorId)) {
                try {
                    double currentValue = -1;
                    if (resourceType == "cpu") {
                        currentValue = getProcessCpuUsage(pid);
                    } else if (resourceType == "memory") {
                        currentValue =
                            static_cast<double>(getProcessMemoryUsage(pid));
                    }
                    // 可以扩展其他资源类型监控

                    if (currentValue >= threshold) {
                        callback(pid, resourceType, currentValue);
                    }

                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(intervalMs));
                } catch (const std::exception &e) {
                    LOG_F(ERROR, "Exception in resource monitor: {}", e.what());
                    break;
                }
            }

            std::lock_guard<std::mutex> taskLock(mutex_);
            resourceTasks_.erase(monitorId);
        });

        resourceTasks_[monitorId] = std::move(task);
        return monitorId;
    }

    bool stopMonitoring(int monitorId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = resourceTasks_.find(monitorId);
        if (it != resourceTasks_.end()) {
            stopFlags_[monitorId] = true;
            return true;
        }
        return false;
    }

private:
    ResourceMonitorManager()
        : nextMonitorId_(1000) {}  // 从1000开始，区别于普通监控

    bool shouldStop(int monitorId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = stopFlags_.find(monitorId);
        if (it != stopFlags_.end()) {
            return it->second;
        }
        return false;
    }

    std::mutex mutex_;
    std::atomic<int> nextMonitorId_;
    std::unordered_map<int, std::future<void>> resourceTasks_;
    std::unordered_map<int, bool> stopFlags_;
};

// 性能历史记录管理器
class PerformanceHistoryManager {
public:
    static PerformanceHistoryManager &getInstance() {
        static PerformanceHistoryManager instance;
        return instance;
    }

    PerformanceHistory collectHistory(int pid, std::chrono::seconds duration,
                                      unsigned int intervalMs) {
        PerformanceHistory history;
        history.pid = pid;

        auto endTime = std::chrono::system_clock::now() + duration;

        while (std::chrono::system_clock::now() < endTime) {
            try {
                PerformanceDataPoint point;
                point.timestamp = std::chrono::system_clock::now();
                point.cpuUsage = getProcessCpuUsage(pid);
                point.memoryUsage = getProcessMemoryUsage(pid);

                // 这里可以添加获取IO统计信息的代码
                auto resources = getProcessResources(pid);
                point.ioReadBytes = resources.ioRead;
                point.ioWriteBytes = resources.ioWrite;

                history.dataPoints.push_back(point);

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(intervalMs));
            } catch (const std::exception &e) {
                LOG_F(ERROR, "Exception collecting performance history: {}",
                      e.what());
                break;
            }
        }

        return history;
    }
};
}  // namespace

// 保存性能计数器信息的结构体
struct CpuUsageTracker {
#ifdef _WIN32
    struct ProcessCpuInfo {
        ULARGE_INTEGER lastCPU;
        ULARGE_INTEGER lastSysCPU;
        ULARGE_INTEGER lastUserCPU;
        DWORD numProcessors;
        HANDLE hProcess;
    };

    static std::map<int, ProcessCpuInfo> &getTrackers() {
        static std::map<int, ProcessCpuInfo> trackers;
        return trackers;
    }

    static std::mutex &getMutex() {
        static std::mutex mutex;
        return mutex;
    }
#elif defined(__linux__)
    struct ProcessCpuInfo {
        clock_t lastCPU;
        clock_t lastSysCPU;
        clock_t lastUserCPU;
        int numProcessors;
        unsigned long lastTotalUser;
        unsigned long lastTotalUserLow;
        unsigned long lastTotalSys;
        unsigned long lastTotalIdle;
    };

    static std::map<int, ProcessCpuInfo> &getTrackers() {
        static std::map<int, ProcessCpuInfo> trackers;
        return trackers;
    }

    static std::mutex &getMutex() {
        static std::mutex mutex;
        return mutex;
    }
#endif
};

#ifdef _WIN32

auto getAllProcesses() -> std::vector<std::pair<int, std::string>> {
    std::vector<std::pair<int, std::string>> processes;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        LOG_F(ERROR, "Failed to create process snapshot");
        return processes;
    }

    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(processEntry);

    if (Process32First(snapshot, &processEntry) != 0) {
        do {
            int pid = processEntry.th32ProcessID;
            processes.emplace_back(pid, std::string(processEntry.szExeFile));
        } while (Process32Next(snapshot, &processEntry) != 0);
    }

    CloseHandle(snapshot);
    return processes;
}

#elif defined(__linux__)
auto getProcessName(int pid) -> std::optional<std::string> {
    std::string path = "/proc/" + std::to_string(pid) + "/comm";
    std::ifstream commFile(path);
    if (commFile) {
        std::string name;
        std::getline(commFile, name);
        return name;
    }
    return std::nullopt;
}

auto getAllProcesses() -> std::vector<std::pair<int, std::string>> {
    std::vector<std::pair<int, std::string>> processes;

    DIR *procDir = opendir("/proc");
    if (procDir == nullptr) {
        LOG_F(ERROR, "Failed to open /proc directory");
        return processes;
    }

    struct dirent *entry;
    while ((entry = readdir(procDir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            char *end;
            long pid = strtol(entry->d_name, &end, 10);
            if (*end == '\0') {
                if (auto name = getProcessName(pid)) {
                    processes.emplace_back(pid, *name);
                }
            }
        }
    }

    closedir(procDir);
    return processes;
}

#elif defined(__APPLE__)
std::optional<std::string> getProcessName(int pid) {
    char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, pathbuf, sizeof(pathbuf)) > 0) {
        std::string path(pathbuf);
        size_t slashPos = path.rfind('/');
        if (slashPos != std::string::npos) {
            return path.substr(slashPos + 1);
        }
        return path;
    }
    return std::nullopt;
}

std::vector<std::pair<int, std::string>> getAllProcesses() {
    std::vector<std::pair<int, std::string>> processes;

    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t length = 0;

    if (sysctl(mib, 4, nullptr, &length, nullptr, 0) == -1) {
        LOG_F(ERROR, "Failed to get process info length");
        return processes;
    }

    auto procBuf = std::make_unique<kinfo_proc[]>(length / sizeof(kinfo_proc));
    if (sysctl(mib, 4, procBuf.get(), &length, nullptr, 0) == -1) {
        LOG_F(ERROR, "Failed to get process info");
        return processes;
    }

    int procCount = length / sizeof(kinfo_proc);
    for (int i = 0; i < procCount; ++i) {
        int pid = procBuf[i].kp_proc.p_pid;
        if (auto name = getProcessName(pid)) {
            processes.emplace_back(pid, *name);
        }
    }

    return processes;
}
#else
#error "Unsupported operating system"
#endif

auto getLatestLogFile(const std::string &folderPath) -> std::string {
    std::vector<fs::path> logFiles;

    try {
        for (const auto &entry : fs::directory_iterator(folderPath)) {
            const auto &path = entry.path();
            if (path.extension() == ".log") {
                logFiles.push_back(path);
            }
        }
    } catch (const fs::filesystem_error &e) {
        LOG_F(ERROR, "Error accessing directory {}: {}", folderPath, e.what());
        return "";
    }

    if (logFiles.empty()) {
        LOG_F(WARNING, "No log files found in directory {}", folderPath);
        return "";
    }

    auto latestFile = std::max_element(
        logFiles.begin(), logFiles.end(),
        [](const fs::path &a, const fs::path &b) {
            try {
                return fs::last_write_time(a) < fs::last_write_time(b);
            } catch (const fs::filesystem_error &e) {
                LOG_F(ERROR, "Error comparing file times: {}", e.what());
                return false;
            }
        });

    if (latestFile == logFiles.end()) {
        LOG_F(ERROR, "Failed to determine the latest log file in directory {}",
              folderPath);
        return "";
    }

    LOG_F(INFO, "Latest log file found: {}", latestFile->string());
    return latestFile->string();
}

auto getProcessInfo(int pid) -> Process {
    Process info;
    info.pid = pid;

    // 获取进程位置
#ifdef _WIN32
    HANDLE hProcess =
        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess != nullptr) {
        wchar_t path[MAX_PATH];
        if (GetModuleFileNameExW(hProcess, nullptr, path, MAX_PATH) != 0) {
            info.path = fs::path(path).string();
        }
        CloseHandle(hProcess);
    }
#else
    char path[PATH_MAX];
    std::string procPath = "/proc/" + std::to_string(pid) + "/exe";
    ssize_t count = readlink(procPath.c_str(), path, PATH_MAX);
    if (count != -1) {
        path[count] = '\0';
        info.path = path;
    }
#endif

    // 获取进程名称
    info.name = fs::path(info.path).filename().string();

    // 获取进程状态
    info.status = "Unknown";
#ifdef _WIN32
    if (hProcess != nullptr) {
        DWORD exitCode;
        if ((GetExitCodeProcess(hProcess, &exitCode) != 0) &&
            exitCode == STILL_ACTIVE) {
            info.status = "Running";
        }
        CloseHandle(hProcess);
    }
#else
    if (fs::exists(info.path)) {
        info.status = "Running";
    }
#endif

    auto outputPath = getLatestLogFile("./log");
    if (!outputPath.empty()) {
        std::ifstream outputFile(outputPath);
        info.output = std::string(std::istreambuf_iterator<char>(outputFile),
                                  std::istreambuf_iterator<char>());
    }

    return info;
}

auto getSelfProcessInfo() -> Process {
#ifdef _WIN32
    return getProcessInfo(GetCurrentProcessId());
#else
    return getProcessInfo(getpid());
#endif
}

auto getProcessInfoByPid(int pid) -> Process { return getProcessInfo(pid); }

auto getProcessInfoByName(const std::string &processName)
    -> std::vector<Process> {
    std::vector<Process> processes;

#ifdef _WIN32
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        LOG_F(ERROR, "Unable to create toolhelp snapshot.");
        return processes;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(snap, &entry)) {
        CloseHandle(snap);
        LOG_F(ERROR, "Unable to get the first process.");
        return processes;
    }

    do {
        std::string currentProcess =
            atom::utils::WCharArrayToString(entry.szExeFile);
        if (currentProcess == processName) {
            processes.push_back(getProcessInfo(entry.th32ProcessID));
        }
    } while (Process32NextW(snap, &entry));

    CloseHandle(snap);
#else
    std::string cmd = "pgrep -fl " + processName;
    auto [output, status] = executeCommandWithStatus(cmd);
    if (status != 0) {
        LOG_F(ERROR, "Failed to find process with name '{}'.", processName);
        return processes;
    }

    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        std::istringstream lineStream(line);
        int pid;
        std::string name;
        if (lineStream >> pid >> name) {
            if (name == processName) {
                processes.push_back(getProcessInfo(pid));
            }
        }
    }
#endif

    return processes;
}

auto isProcessRunning(const std::string &processName) -> bool {
#ifdef _WIN32
    // Windows-specific: Use Toolhelp32 API to iterate over running processes.
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hSnapshot, &pe32) == 0) {
        CloseHandle(hSnapshot);
        return false;
    }

    bool isRunning = false;
    do {
        if (processName == std::string(pe32.szExeFile)) {
            isRunning = true;
            break;
        }
    } while (Process32Next(hSnapshot, &pe32) != 0);

    CloseHandle(hSnapshot);
    return isRunning;

#elif __APPLE__
    // macOS-specific: Use `pgrep` to search for the process.
    std::string command = "pgrep -x " + processName + " > /dev/null 2>&1";
    return system(command.c_str()) == 0;

#elif __linux__ || __ANDROID__
    // Linux/Android: Iterate over `/proc` to find the process by name.
    std::filesystem::path procDir("/proc");
    if (!std::filesystem::exists(procDir) ||
        !std::filesystem::is_directory(procDir))
        return false;

    for (const auto &p : std::filesystem::directory_iterator(procDir)) {
        if (p.is_directory()) {
            std::string pid = p.path().filename().string();
            if (std::all_of(pid.begin(), pid.end(), isdigit)) {
                std::ifstream cmdline(p.path() / "cmdline");
                std::string cmd;
                std::getline(cmdline, cmd);
                if (cmd.find(processName) != std::string::npos) {
                    return true;
                }
            }
        }
    }
    return false;
#endif
}

auto getParentProcessId(int processId) -> int {
#ifdef _WIN32
    DWORD parentProcessId = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hSnapshot, &processEntry) != 0) {
            do {
                if (static_cast<int>(processEntry.th32ProcessID) == processId) {
                    parentProcessId = processEntry.th32ParentProcessID;
                    break;
                }
            } while (Process32Next(hSnapshot, &processEntry) != 0);
        }

        CloseHandle(hSnapshot);
    }
    return static_cast<int>(parentProcessId);
#else
    std::string path = "/proc/" + std::to_string(processId) + "/stat";
    std::ifstream file(path);
    std::string line;
    if (std::getline(file, line)) {
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
        if (tokens.size() > 3) {
            return std::stoul(tokens[3]);  // PPID is at the 4th position
        }
    }
    return 0;
#endif
}

auto createProcessAsUser(const std::string &command, const std::string &user,
                         [[maybe_unused]] const std::string &domain,
                         [[maybe_unused]] const std::string &password) -> bool {
#ifdef _WIN32
    HANDLE tokenHandle = nullptr;
    HANDLE newTokenHandle = nullptr;
    STARTUPINFOW startupInfo;
    PROCESS_INFORMATION processInfo;
    bool result = false;
    ZeroMemory(&startupInfo, sizeof(STARTUPINFOW));
    startupInfo.cb = sizeof(STARTUPINFOW);
    ZeroMemory(&processInfo, sizeof(PROCESS_INFORMATION));

    struct Cleanup {
        HANDLE &tokenHandle;
        HANDLE &newTokenHandle;
        PROCESS_INFORMATION &processInfo;

        ~Cleanup() {
            if (tokenHandle != nullptr) {
                CloseHandle(tokenHandle);
            }
            if (newTokenHandle != nullptr) {
                CloseHandle(newTokenHandle);
            }
            if (processInfo.hProcess != nullptr) {
                CloseHandle(processInfo.hProcess);
            }
            if (processInfo.hThread != nullptr) {
                CloseHandle(processInfo.hThread);
            }
        }
    } cleanup{tokenHandle, newTokenHandle, processInfo};

    if (LogonUserA(atom::utils::StringToLPSTR(user),
                   atom::utils::StringToLPSTR(domain),
                   atom::utils::StringToLPSTR(password),
                   LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT,
                   &tokenHandle) == 0) {
        LOG_F(ERROR, "LogonUser failed with error: {}", GetLastError());
        return false;
    }

    if (DuplicateTokenEx(tokenHandle, TOKEN_ALL_ACCESS, nullptr,
                         SecurityImpersonation, TokenPrimary,
                         &newTokenHandle) == 0) {
        LOG_F(ERROR, "DuplicateTokenEx failed with error: {}", GetLastError());
        return false;
    }

    if (CreateProcessAsUserW(newTokenHandle, nullptr,
                             atom::utils::StringToLPWSTR(command), nullptr,
                             nullptr, FALSE, 0, nullptr, nullptr, &startupInfo,
                             &processInfo) == 0) {
        LOG_F(ERROR, "CreateProcessAsUser failed with error: {}",
              GetLastError());
        return false;
    }
    LOG_F(INFO, "Process created successfully!");
    result = true;
    WaitForSingleObject(processInfo.hProcess, INFINITE);

    return result;
#else
    pid_t pid = fork();
    if (pid < 0) {
        LOG_F(ERROR, "Fork failed");
        return false;
    } else if (pid == 0) {
        struct passwd *pw = getpwnam(user.c_str());
        if (pw == nullptr) {
            LOG_F(ERROR, "Failed to get user information for {}", user);
            exit(EXIT_FAILURE);
        }

        if (setgid(pw->pw_gid) != 0) {
            LOG_F(ERROR, "Failed to set group ID");
            exit(EXIT_FAILURE);
        }
        if (setuid(pw->pw_uid) != 0) {
            LOG_F(ERROR, "Failed to set user ID");
            exit(EXIT_FAILURE);
        }
        execl("/bin/sh", "sh", "-c", command.c_str(), (char *)nullptr);
        LOG_F(ERROR, "Failed to execute command");
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            LOG_F(INFO, "Process exited with status {}",
                  std::to_string(WEXITSTATUS(status)));
            return WEXITSTATUS(status) == 0;
        }
        LOG_F(ERROR, "Process did not exit normally");
        return false;
    }
#endif
}

auto getProcessIdByName(const std::string &processName) -> std::vector<int> {
    std::vector<int> pids;
#ifdef _WIN32
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        LOG_F(ERROR, "Failed to create snapshot!");
        return pids;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32) != 0) {
        do {
            if (_stricmp(pe32.szExeFile, processName.c_str()) == 0) {
                pids.push_back(static_cast<int>(pe32.th32ProcessID));
            }
        } while (Process32Next(hSnapshot, &pe32) != 0);
    }

    CloseHandle(hSnapshot);
#elif defined(__linux__)
    try {
        for (const auto &entry : fs::directory_iterator("/proc")) {
            if (entry.is_directory()) {
                const std::string DIR_NAME = entry.path().filename().string();
                if (std::all_of(DIR_NAME.begin(), DIR_NAME.end(), ::isdigit)) {
                    std::ifstream cmdFile(entry.path() / "comm");
                    if (cmdFile) {
                        std::string cmdName;
                        std::getline(cmdFile, cmdName);
                        if (cmdName == processName) {
                            pids.push_back(
                                static_cast<pid_t>(std::stoi(DIR_NAME)));
                        }
                    }
                }
            }
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Error reading /proc directory: {}", e.what());
    }
#elif defined(__APPLE__)
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    struct kinfo_proc *processList = nullptr;
    size_t size = 0;

    if (sysctl(mib, 4, nullptr, &size, nullptr, 0) == -1) {
        LOG_F(ERROR, "Failed to get process size.");
        return pids;
    }

    processList = new kinfo_proc[size / sizeof(struct kinfo_proc)];
    if (sysctl(mib, 4, processList, &size, nullptr, 0) == -1) {
        LOG_F(ERROR, "Failed to get process list.");
        delete[] processList;
        return pids;
    }

    for (size_t i = 0; i < size / sizeof(struct kinfo_proc); ++i) {
        char processPath[PROC_PIDPATHINFO_MAXSIZE];
        proc_pidpath(processList[i].kp_proc.p_pid, processPath,
                     sizeof(processPath));

        std::string proc_name = processPath;
        if (proc_name.find(processName) != std::string::npos) {
            pids.push_back(processList[i].kp_proc.p_pid);
        }
    }
    delete[] processList;
#else
#error "Unsupported operating system"
#endif
    return pids;
}

#ifdef _WIN32
// Get current user privileges on Windows systems
auto getWindowsPrivileges(int pid) -> PrivilegesInfo {
    PrivilegesInfo info;
    HANDLE tokenHandle = nullptr;
    DWORD tokenInfoLength = 0;
    PTOKEN_PRIVILEGES privileges = nullptr;
    std::array<char, BUFFER_SIZE> username;
    auto usernameLen = static_cast<DWORD>(username.size());

    // Get the username
    if (GetUserNameA(username.data(), &usernameLen) != 0) {
        info.username = username.data();
        LOG_F(INFO, "Current User: {}", info.username);
    } else {
        LOG_F(ERROR, "Failed to get username. Error: {}", GetLastError());
    }

    // Open the access token of the specified process
    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (processHandle != nullptr &&
        OpenProcessToken(processHandle, TOKEN_QUERY, &tokenHandle) != 0) {
        CloseHandle(processHandle);
        // Get the length of the token information
        GetTokenInformation(tokenHandle, TokenPrivileges, nullptr, 0,
                            &tokenInfoLength);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            privileges =
                static_cast<PTOKEN_PRIVILEGES>(malloc(tokenInfoLength));
            if (privileges != nullptr &&
                GetTokenInformation(tokenHandle, TokenPrivileges, privileges,
                                    tokenInfoLength, &tokenInfoLength) != 0) {
                LOG_F(INFO, "Privileges:");
                for (DWORD i = 0; i < privileges->PrivilegeCount; ++i) {
                    LUID_AND_ATTRIBUTES laa = privileges->Privileges[i];
                    std::array<char, BUFFER_SIZE> privilegeName;
                    auto nameSize = static_cast<DWORD>(privilegeName.size());

                    // Get the privilege name
                    if (LookupPrivilegeNameA(nullptr, &laa.Luid,
                                             privilegeName.data(),
                                             &nameSize) != 0) {
                        std::string privilege = privilegeName.data();
                        privilege +=
                            (laa.Attributes & SE_PRIVILEGE_ENABLED) != 0
                                ? " - Enabled"
                                : " - Disabled";
                        info.privileges.push_back(privilege);
                        LOG_F(INFO, "  {}", privilege);
                    } else {
                        LOG_F(ERROR,
                              "Failed to lookup privilege name. Error: {}",
                              GetLastError());
                    }
                }
            } else {
                LOG_F(ERROR, "Failed to get token information. Error: {}",
                      GetLastError());
            }
            free(privileges);
        } else {
            LOG_F(ERROR, "Failed to get token information length. Error: {}",
                  GetLastError());
        }
        CloseHandle(tokenHandle);
    } else {
        LOG_F(ERROR, "Failed to open process token. Error: {}", GetLastError());
    }

    // Check if the user is an administrator
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID administratorsGroup;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                 &administratorsGroup) != 0) {
        CheckTokenMembership(nullptr, administratorsGroup, &isAdmin);
        FreeSid(administratorsGroup);
    } else {
        LOG_F(ERROR, "Failed to allocate and initialize SID. Error: {}",
              GetLastError());
    }
    info.isAdmin = isAdmin != 0;
    LOG_F(INFO, "User has {}Administrator privileges.",
          info.isAdmin ? "" : "no ");

    return info;
}

#else
// Get current user and group privileges on POSIX systems
auto getPosixPrivileges(pid_t pid) -> PrivilegesInfo {
    PrivilegesInfo info;
    std::string procPath = "/proc/" + std::to_string(pid);

    // Read UID and GID from /proc/[pid]/status
    std::ifstream statusFile(procPath + "/status");
    if (!statusFile) {
        LOG_F(ERROR, "Failed to open /proc/{}/status", pid);
        return info;
    }

    std::string line;
    uid_t uid = -1;
    uid_t euid = -1;
    gid_t gid = -1;
    gid_t egid = -1;

    std::regex uidRegex(R"(Uid:\s+(\d+)\s+(\d+))");
    std::regex gidRegex(R"(Gid:\s+(\d+)\s+(\d+))");
    std::smatch match;

    while (std::getline(statusFile, line)) {
        if (std::regex_search(line, match, uidRegex)) {
            uid = std::stoi(match[1]);
            euid = std::stoi(match[2]);
        } else if (std::regex_search(line, match, gidRegex)) {
            gid = std::stoi(match[1]);
            egid = std::stoi(match[2]);
        }
    }

    struct passwd *pw = getpwuid(uid);
    struct group *gr = getgrgid(gid);

    if (pw != nullptr) {
        info.username = pw->pw_name;
        LOG_F(INFO, "User: {} (UID: {})", info.username, uid);
    } else {
        LOG_F(ERROR, "Failed to get user information for UID: {}", uid);
    }
    if (gr != nullptr) {
        info.groupname = gr->gr_name;
        LOG_F(INFO, "Group: {} (GID: {})", info.groupname, gid);
    } else {
        LOG_F(ERROR, "Failed to get group information for GID: {}", gid);
    }

    // Display effective user and group IDs
    if (uid != euid) {
        struct passwd *epw = getpwuid(euid);
        if (epw != nullptr) {
            LOG_F(INFO, "Effective User: {} (EUID: {})", epw->pw_name, euid);
        } else {
            LOG_F(ERROR,
                  "Failed to get effective user information for EUID: {}",
                  euid);
        }
    }

    if (gid != egid) {
        struct group *egr = getgrgid(egid);
        if (egr != nullptr) {
            LOG_F(INFO, "Effective Group: {} (EGID: {})", egr->gr_name, egid);
        } else {
            LOG_F(ERROR,
                  "Failed to get effective group information for EGID: {}",
                  egid);
        }
    }

#if defined(__linux__) && __has_include(<sys/capability.h>)
    // Check process capabilities on Linux systems
    std::ifstream capFile(procPath + "/status");
    if (capFile) {
        std::string capLine;
        while (std::getline(capFile, capLine)) {
            if (capLine.find("CapEff:") == 0) {
                info.privileges.push_back(capLine);
                LOG_F(INFO, "Capabilities: {}", capLine);
            }
        }
    } else {
        LOG_F(ERROR, "Failed to open /proc/{}/status", pid);
    }
#endif

    return info;
}
#endif

auto getProcessCpuUsage(int pid) -> double {
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(CpuUsageTracker::getMutex());
    auto &trackers = CpuUsageTracker::getTrackers();

    // 初始化或获取进程追踪器
    if (trackers.find(pid) == trackers.end()) {
        SYSTEM_INFO sysInfo;
        FILETIME ftime, fsys, fuser;

        GetSystemInfo(&sysInfo);

        CpuUsageTracker::ProcessCpuInfo info;
        info.numProcessors = sysInfo.dwNumberOfProcessors;

        info.hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                    FALSE, pid);
        if (info.hProcess == NULL) {
            return -1.0;
        }

        GetSystemTimeAsFileTime(&ftime);
        memcpy(&info.lastCPU, &ftime, sizeof(FILETIME));

        GetProcessTimes(info.hProcess, &ftime, &ftime, &fsys, &fuser);
        memcpy(&info.lastSysCPU, &fsys, sizeof(FILETIME));
        memcpy(&info.lastUserCPU, &fuser, sizeof(FILETIME));

        trackers[pid] = info;
        return 0.0;  // 第一次调用返回0
    }

    auto &info = trackers[pid];
    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;
    double percent;

    GetSystemTimeAsFileTime(&ftime);
    memcpy(&now, &ftime, sizeof(FILETIME));

    // 确保进程句柄有效
    if (info.hProcess == NULL) {
        info.hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                    FALSE, pid);
        if (info.hProcess == NULL) {
            return -1.0;
        }
    }

    if (!GetProcessTimes(info.hProcess, &ftime, &ftime, &fsys, &fuser)) {
        CloseHandle(info.hProcess);
        info.hProcess = NULL;
        trackers.erase(pid);  // 清理无效追踪器
        return -1.0;
    }

    memcpy(&sys, &fsys, sizeof(FILETIME));
    memcpy(&user, &fuser, sizeof(FILETIME));

    percent = (sys.QuadPart - info.lastSysCPU.QuadPart) +
              (user.QuadPart - info.lastUserCPU.QuadPart);
    percent /= (now.QuadPart - info.lastCPU.QuadPart);
    percent /= info.numProcessors;
    percent *= 100;

    info.lastCPU = now;
    info.lastUserCPU = user;
    info.lastSysCPU = sys;

    return percent;
#elif defined(__linux__)
    std::lock_guard<std::mutex> lock(CpuUsageTracker::getMutex());
    auto &trackers = CpuUsageTracker::getTrackers();

    // 初始化或获取进程追踪器
    if (trackers.find(pid) == trackers.end()) {
        FILE *file;

        CpuUsageTracker::ProcessCpuInfo info;
        struct tms timeSample;

        info.lastCPU = times(&timeSample);
        info.lastSysCPU = timeSample.tms_stime;
        info.lastUserCPU = timeSample.tms_utime;

        file = fopen("/proc/stat", "r");
        if (file == nullptr) {
            return -1.0;
        }

        unsigned long user, nice, sys, idle;
        if (fscanf(file, "cpu %lu %lu %lu %lu", &user, &nice, &sys, &idle) !=
            4) {
            fclose(file);
            return -1.0;
        }
        fclose(file);

        info.lastTotalUser = user + nice;
        info.lastTotalUserLow = nice;
        info.lastTotalSys = sys;
        info.lastTotalIdle = idle;

        info.numProcessors = sysconf(_SC_NPROCESSORS_ONLN);

        trackers[pid] = info;
        return 0.0;  // 第一次调用返回0
    }

    auto &info = trackers[pid];
    FILE *file;
    double percent;
    struct tms timeSample;
    clock_t now;

    // 检查进程是否存在
    std::string procPath = "/proc/" + std::to_string(pid) + "/stat";
    file = fopen(procPath.c_str(), "r");
    if (file == nullptr) {
        trackers.erase(pid);  // 清理无效追踪器
        return -1.0;
    }
    fclose(file);

    now = times(&timeSample);
    if (now <= info.lastCPU || timeSample.tms_stime < info.lastSysCPU ||
        timeSample.tms_utime < info.lastUserCPU) {
        // 溢出检测
        percent = 0.0;
    } else {
        percent = (timeSample.tms_stime - info.lastSysCPU) +
                  (timeSample.tms_utime - info.lastUserCPU);
        percent /= (now - info.lastCPU);
        percent /= info.numProcessors;
        percent *= 100;
    }

    info.lastCPU = now;
    info.lastSysCPU = timeSample.tms_stime;
    info.lastUserCPU = timeSample.tms_utime;

    return percent;
#elif defined(__APPLE__)
    // macOS实现
    task_t task;
    kern_return_t kr;
    mach_msg_type_number_t count;
    thread_array_t thread_list;
    thread_info_data_t thinfo;
    thread_basic_info_t basic_info_th;
    mach_msg_type_number_t thread_count;

    if (task_for_pid(mach_task_self(), pid, &task) != KERN_SUCCESS) {
        return -1.0;
    }

    count = TASK_BASIC_INFO_COUNT;
    task_info_t tinfo;
    task_basic_info_t basic_info;
    kr = task_info(task, TASK_BASIC_INFO, (task_info_t)tinfo, &count);
    if (kr != KERN_SUCCESS) {
        return -1.0;
    }

    basic_info = (task_basic_info_t)tinfo;

    // 获取线程信息
    kr = task_threads(task, &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
        return -1.0;
    }

    double cpu_usage = 0;
    for (unsigned int i = 0; i < thread_count; i++) {
        count = THREAD_BASIC_INFO_COUNT;
        kr = thread_info(thread_list[i], THREAD_BASIC_INFO,
                         (thread_info_t)thinfo, &count);
        if (kr != KERN_SUCCESS) {
            continue;
        }

        basic_info_th = (thread_basic_info_t)thinfo;
        if (!(basic_info_th->flags & TH_FLAGS_IDLE)) {
            cpu_usage += basic_info_th->cpu_usage / (float)TH_USAGE_SCALE;
        }
    }

    // 释放线程列表
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                  thread_count * sizeof(thread_act_t));

    return cpu_usage * 100.0;
#else
    return -1.0;
#endif
}

auto getProcessMemoryUsage(int pid) -> std::size_t {
#ifdef _WIN32
    HANDLE hProcess =
        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) {
        return 0;
    }

    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS *)&pmc,
                             sizeof(pmc))) {
        CloseHandle(hProcess);
        return pmc.WorkingSetSize;  // 返回实际物理内存使用量
    }

    CloseHandle(hProcess);
    return 0;
#elif defined(__linux__)
    std::string statmPath = "/proc/" + std::to_string(pid) + "/statm";
    std::ifstream statmFile(statmPath);
    if (!statmFile) {
        return 0;
    }

    std::size_t size, resident, shared, text, lib, data, dt;
    statmFile >> size >> resident >> shared >> text >> lib >> data >> dt;

    // 计算实际物理内存（RSS）
    long pageSize = sysconf(_SC_PAGESIZE);
    return resident * pageSize;
#elif defined(__APPLE__)
    task_t task;
    if (task_for_pid(mach_task_self(), pid, &task) != KERN_SUCCESS) {
        return 0;
    }

    struct task_basic_info info;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    if (task_info(task, TASK_BASIC_INFO, (task_info_t)&info, &count) !=
        KERN_SUCCESS) {
        return 0;
    }

    return info.resident_size;
#else
    return 0;
#endif
}

auto setProcessPriority(int pid, ProcessPriority priority) -> bool {
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (hProcess == NULL) {
        LOG_F(ERROR, "无法打开进程: PID={}", pid);
        return false;
    }

    DWORD priorityClass;
    switch (priority) {
        case ProcessPriority::IDLE:
            priorityClass = IDLE_PRIORITY_CLASS;
            break;
        case ProcessPriority::LOW:
            priorityClass = BELOW_NORMAL_PRIORITY_CLASS;
            break;
        case ProcessPriority::NORMAL:
            priorityClass = NORMAL_PRIORITY_CLASS;
            break;
        case ProcessPriority::HIGH:
            priorityClass = HIGH_PRIORITY_CLASS;
            break;
        case ProcessPriority::REALTIME:
            priorityClass = REALTIME_PRIORITY_CLASS;
            break;
        default:
            priorityClass = NORMAL_PRIORITY_CLASS;
    }

    BOOL result = SetPriorityClass(hProcess, priorityClass);
    CloseHandle(hProcess);

    if (!result) {
        LOG_F(ERROR, "设置进程优先级失败: PID={}, 错误码={}", pid,
              GetLastError());
        return false;
    }

    return true;
#elif defined(__linux__) || defined(__APPLE__)
    int which = PRIO_PROCESS;
    int prio;

    switch (priority) {
        case ProcessPriority::IDLE:
            prio = 19;
            break;
        case ProcessPriority::LOW:
            prio = 10;
            break;
        case ProcessPriority::NORMAL:
            prio = 0;
            break;
        case ProcessPriority::HIGH:
            prio = -10;
            break;
        case ProcessPriority::REALTIME:
            prio = -20;
            break;
        default:
            prio = 0;
    }

    int result = setpriority(which, pid, prio);
    if (result == -1) {
        LOG_F(ERROR, "设置进程优先级失败: PID={}, 错误={}", pid,
              strerror(errno));
        return false;
    }

    return true;
#else
    return false;
#endif
}

auto getProcessPriority(int pid) -> std::optional<ProcessPriority> {
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == NULL) {
        LOG_F(ERROR, "无法打开进程: PID={}", pid);
        return std::nullopt;
    }

    DWORD priorityClass = GetPriorityClass(hProcess);
    CloseHandle(hProcess);

    if (priorityClass == 0) {
        LOG_F(ERROR, "获取进程优先级失败: PID={}, 错误码={}", pid,
              GetLastError());
        return std::nullopt;
    }

    switch (priorityClass) {
        case IDLE_PRIORITY_CLASS:
            return ProcessPriority::IDLE;
        case BELOW_NORMAL_PRIORITY_CLASS:
            return ProcessPriority::LOW;
        case NORMAL_PRIORITY_CLASS:
            return ProcessPriority::NORMAL;
        case HIGH_PRIORITY_CLASS:
            return ProcessPriority::HIGH;
        case REALTIME_PRIORITY_CLASS:
            return ProcessPriority::REALTIME;
        default:
            return ProcessPriority::NORMAL;
    }
#elif defined(__linux__) || defined(__APPLE__)
    int which = PRIO_PROCESS;
    errno = 0;
    int prio = getpriority(which, pid);

    if (errno != 0) {
        LOG_F(ERROR, "获取进程优先级失败: PID={}, 错误={}", pid,
              strerror(errno));
        return std::nullopt;
    }

    if (prio >= 10)
        return ProcessPriority::IDLE;
    else if (prio >= 1)
        return ProcessPriority::LOW;
    else if (prio >= -9)
        return ProcessPriority::NORMAL;
    else if (prio >= -19)
        return ProcessPriority::HIGH;
    else
        return ProcessPriority::REALTIME;
#else
    return std::nullopt;
#endif
}

auto getChildProcesses(int pid) -> std::vector<int> {
    std::vector<int> children;

#ifdef _WIN32
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        LOG_F(ERROR, "无法创建进程快照: 错误码={}", GetLastError());
        return children;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hSnap, &pe32)) {
        LOG_F(ERROR, "无法获取第一个进程: 错误码={}", GetLastError());
        CloseHandle(hSnap);
        return children;
    }

    do {
        if (pe32.th32ParentProcessID == static_cast<DWORD>(pid)) {
            children.push_back(static_cast<int>(pe32.th32ProcessID));
        }
    } while (Process32Next(hSnap, &pe32));

    CloseHandle(hSnap);
#elif defined(__linux__)
    // Linux下通过/proc文件系统获取子进程
    DIR *procDir = opendir("/proc");
    if (procDir == nullptr) {
        LOG_F(ERROR, "无法打开/proc目录");
        return children;
    }

    struct dirent *entry;
    while ((entry = readdir(procDir)) != nullptr) {
        // 检查目录名是否为纯数字(PID)
        if (entry->d_type == DT_DIR) {
            bool isPid = true;
            for (const char *p = entry->d_name; *p; ++p) {
                if (!isdigit(*p)) {
                    isPid = false;
                    break;
                }
            }

            if (isPid) {
                int childPid = std::stoi(entry->d_name);
                std::string statPath =
                    "/proc/" + std::string(entry->d_name) + "/stat";
                std::ifstream statFile(statPath);

                if (statFile) {
                    std::string line;
                    std::getline(statFile, line);

                    // 解析stat文件，PPID是第4个字段
                    std::istringstream iss(line);
                    std::string field;
                    int fieldCount = 0;
                    int ppid = -1;

                    while (iss >> field && fieldCount < 4) {
                        fieldCount++;
                        if (fieldCount == 4) {
                            ppid = std::stoi(field);
                        }
                    }

                    if (ppid == pid) {
                        children.push_back(childPid);
                    }
                }
            }
        }
    }

    closedir(procDir);
#elif defined(__APPLE__)
    // macOS上使用sysctl获取进程树
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    std::size_t size;

    if (sysctl(mib, 4, nullptr, &size, nullptr, 0) < 0) {
        LOG_F(ERROR, "获取进程列表大小失败: 错误={}", strerror(errno));
        return children;
    }

    std::vector<struct kinfo_proc> processList(size /
                                               sizeof(struct kinfo_proc));

    if (sysctl(mib, 4, processList.data(), &size, nullptr, 0) < 0) {
        LOG_F(ERROR, "获取进程列表失败: 错误={}", strerror(errno));
        return children;
    }

    for (const auto &proc : processList) {
        if (proc.kp_eproc.e_ppid == pid) {
            children.push_back(proc.kp_proc.p_pid);
        }
    }
#endif

    return children;
}

auto getProcessStartTime(int pid)
    -> std::optional<std::chrono::system_clock::time_point> {
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == NULL) {
        LOG_F(ERROR, "无法打开进程: PID={}", pid);
        return std::nullopt;
    }

    FILETIME ftCreate, ftExit, ftKernel, ftUser;
    if (!GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
        LOG_F(ERROR, "获取进程时间失败: PID={}, 错误码={}", pid,
              GetLastError());
        CloseHandle(hProcess);
        return std::nullopt;
    }

    CloseHandle(hProcess);

    // 将FILETIME转换为system_clock::time_point
    ULARGE_INTEGER uli;
    uli.LowPart = ftCreate.dwLowDateTime;
    uli.HighPart = ftCreate.dwHighDateTime;

    // Windows FILETIME是从1601年1月1日开始的100纳秒间隔
    // 需要转换为Unix时间戳(从1970年1月1日开始的秒数)
    const ULONGLONG WINDOWS_TICK = 10000000ULL;
    const ULONGLONG SEC_TO_UNIX_EPOCH = 11644473600ULL;
    ULONGLONG unixTime = uli.QuadPart / WINDOWS_TICK - SEC_TO_UNIX_EPOCH;

    auto startTime =
        std::chrono::system_clock::from_time_t(static_cast<time_t>(unixTime));
    return startTime;
#elif defined(__linux__)
    std::string statPath = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream statFile(statPath);
    if (!statFile) {
        LOG_F(ERROR, "无法打开进程状态文件: {}", statPath);
        return std::nullopt;
    }

    std::string line;
    std::getline(statFile, line);
    std::istringstream iss(line);

    // stat文件的第22个字段是进程启动时间(以jiffies为单位, 从系统启动开始)
    std::string field;
    for (int i = 0; i < 21; ++i) {
        iss >> field;
    }

    unsigned long long startTimeJiffies;
    iss >> startTimeJiffies;

    // 获取系统启动时间
    std::ifstream uptimeFile("/proc/uptime");
    if (!uptimeFile) {
        LOG_F(ERROR, "无法打开系统运行时间文件");
        return std::nullopt;
    }

    double uptime;
    uptimeFile >> uptime;

    // 计算进程启动的绝对时间
    time_t now = std::time(nullptr);
    time_t bootTime = now - static_cast<time_t>(uptime);
    long clkTck = sysconf(_SC_CLK_TCK);
    time_t startTime = bootTime + (startTimeJiffies / clkTck);

    return std::chrono::system_clock::from_time_t(startTime);
#elif defined(__APPLE__)
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);

    if (sysctl(mib, 4, &kp, &len, nullptr, 0) < 0) {
        LOG_F(ERROR, "sysctl失败: 错误={}", strerror(errno));
        return std::nullopt;
    }

    // macOS上, p_starttime是一个timeval结构
    time_t startTime = kp.kp_proc.p_starttime.tv_sec;
    return std::chrono::system_clock::from_time_t(startTime);
#else
    return std::nullopt;
#endif
}

auto getProcessRunningTime(int pid) -> long {
    auto startTime = getProcessStartTime(pid);
    if (!startTime) {
        return -1;
    }

    auto now = std::chrono::system_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::seconds>(now - *startTime);
    return duration.count();
}

auto monitorProcess(int pid,
                    std::function<void(int, const std::string &)> callback,
                    unsigned int intervalMs) -> int {
    return ProcessMonitorManager::getInstance().startMonitoring(pid, callback,
                                                                intervalMs);
}

auto stopMonitoring(int monitorId) -> bool {
    // 根据monitorId停止不同类型的监控
    if (monitorId >= 1000) {
        // 资源监控任务
        return ResourceMonitorManager::getInstance().stopMonitoring(monitorId);
    } else {
        // 普通进程监控任务
        return ProcessMonitorManager::getInstance().stopMonitoring(monitorId);
    }
}

auto getProcessCommandLine(int pid) -> std::vector<std::string> {
    std::vector<std::string> cmdline;

#ifdef _WIN32
    HANDLE hProcess =
        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == nullptr) {
        LOG_F(ERROR, "无法打开进程: PID={}", pid);
        return cmdline;
    }

    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameExW(hProcess, nullptr, exePath, MAX_PATH) != 0) {
        cmdline.push_back(atom::utils::WCharArrayToString(exePath));
    }

    CloseHandle(hProcess);

    // Windows不易直接获取命令行参数，这只返回可执行文件路径
#elif defined(__linux__)
    std::string cmdlinePath = "/proc/" + std::to_string(pid) + "/cmdline";
    std::ifstream cmdlineFile(cmdlinePath);
    if (cmdlineFile) {
        std::string arg;
        std::string content((std::istreambuf_iterator<char>(cmdlineFile)),
                            std::istreambuf_iterator<char>());

        // cmdline文件以null字符分隔参数
        size_t pos = 0;
        while (pos < content.size()) {
            std::string token;
            while (pos < content.size() && content[pos] != '\0') {
                token += content[pos++];
            }
            if (!token.empty()) {
                cmdline.push_back(token);
            }
            pos++;  // 跳过null字符
        }
    }
#elif defined(__APPLE__)
    int mib[3] = {CTL_KERN, KERN_PROCARGS2, pid};
    size_t size = 0;

    // 获取所需大小
    if (sysctl(mib, 3, nullptr, &size, nullptr, 0) < 0) {
        LOG_F(ERROR, "sysctl获取大小失败: {}", strerror(errno));
        return cmdline;
    }

    std::vector<char> procargs(size);
    if (sysctl(mib, 3, procargs.data(), &size, nullptr, 0) < 0) {
        LOG_F(ERROR, "sysctl获取进程参数失败: {}", strerror(errno));
        return cmdline;
    }

    // 解析参数
    int argc = *reinterpret_cast<int *>(procargs.data());
    char *cp = procargs.data() + sizeof(int);

    // 跳过exec_path
    while (*cp != '\0') {
        cp++;
    }
    cp++;

    // 跳过padding
    while (*cp == '\0') {
        cp++;
    }

    // 现在cp指向参数
    for (int i = 0; i < argc && cp < procargs.data() + size; i++) {
        std::string arg = cp;
        cmdline.push_back(arg);
        cp += arg.size() + 1;
    }
#endif

    return cmdline;
}

auto getProcessEnvironment(int pid)
    -> std::unordered_map<std::string, std::string> {
    std::unordered_map<std::string, std::string> env;

#ifdef _WIN32
    // Windows不容易获取其他进程的环境变量
    LOG_F(WARNING, "Windows不支持获取其他进程的环境变量");
#elif defined(__linux__)
    std::string envPath = "/proc/" + std::to_string(pid) + "/environ";
    std::ifstream envFile(envPath);
    if (!envFile) {
        LOG_F(ERROR, "无法打开环境变量文件: {}", envPath);
        return env;
    }

    std::string content((std::istreambuf_iterator<char>(envFile)),
                        std::istreambuf_iterator<char>());

    // 环境变量以null字符分隔
    size_t pos = 0;
    while (pos < content.size()) {
        std::string entry;
        while (pos < content.size() && content[pos] != '\0') {
            entry += content[pos++];
        }

        // 解析KEY=VALUE格式
        size_t equalsPos = entry.find('=');
        if (equalsPos != std::string::npos) {
            std::string key = entry.substr(0, equalsPos);
            std::string value = entry.substr(equalsPos + 1);
            env[key] = value;
        }

        pos++;  // 跳过null字符
    }
#elif defined(__APPLE__)
    // macOS不支持直接获取其他进程的环境变量
    LOG_F(WARNING, "macOS不支持获取其他进程的环境变量");
#endif

    return env;
}

auto suspendProcess(int pid) -> bool {
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (hProcess == nullptr) {
        LOG_F(ERROR, "无法打开进程: PID={}", pid);
        return false;
    }

    typedef LONG(NTAPI * NtSuspendProcess)(HANDLE ProcessHandle);
    NtSuspendProcess pfnNtSuspendProcess = reinterpret_cast<NtSuspendProcess>(
        GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSuspendProcess"));

    if (!pfnNtSuspendProcess) {
        LOG_F(ERROR, "无法获取NtSuspendProcess函数");
        CloseHandle(hProcess);
        return false;
    }

    LONG status = pfnNtSuspendProcess(hProcess);
    CloseHandle(hProcess);

    if (status != 0) {
        LOG_F(ERROR, "挂起进程失败: PID={}, 状态={}", pid, status);
        return false;
    }

    return true;
#elif defined(__linux__)
    // 在Linux上使用SIGSTOP信号挂起进程
    if (kill(pid, SIGSTOP) != 0) {
        LOG_F(ERROR, "挂起进程失败: PID={}, 错误={}", pid, strerror(errno));
        return false;
    }
    return true;
#elif defined(__APPLE__)
    // macOS与Linux类似，使用SIGSTOP信号
    if (kill(pid, SIGSTOP) != 0) {
        LOG_F(ERROR, "挂起进程失败: PID={}, 错误={}", pid, strerror(errno));
        return false;
    }
    return true;
#else
    return false;
#endif
}

auto resumeProcess(int pid) -> bool {
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (hProcess == nullptr) {
        LOG_F(ERROR, "无法打开进程: PID={}", pid);
        return false;
    }

    typedef LONG(NTAPI * NtResumeProcess)(HANDLE ProcessHandle);
    NtResumeProcess pfnNtResumeProcess = reinterpret_cast<NtResumeProcess>(
        GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtResumeProcess"));

    if (!pfnNtResumeProcess) {
        LOG_F(ERROR, "无法获取NtResumeProcess函数");
        CloseHandle(hProcess);
        return false;
    }

    LONG status = pfnNtResumeProcess(hProcess);
    CloseHandle(hProcess);

    if (status != 0) {
        LOG_F(ERROR, "恢复进程失败: PID={}, 状态={}", pid, status);
        return false;
    }

    return true;
#elif defined(__linux__) || defined(__APPLE__)
    // 在Linux/macOS上使用SIGCONT信号继续运行进程
    if (kill(pid, SIGCONT) != 0) {
        LOG_F(ERROR, "恢复进程失败: PID={}, 错误={}", pid, strerror(errno));
        return false;
    }
    return true;
#else
    return false;
#endif
}

auto setProcessAffinity(int pid, const std::vector<int> &cpuIndices) -> bool {
    if (cpuIndices.empty()) {
        LOG_F(ERROR, "CPU核心索引列表为空");
        return false;
    }

#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (hProcess == nullptr) {
        LOG_F(ERROR, "无法打开进程: PID={}", pid);
        return false;
    }

    DWORD_PTR affinityMask = 0;
    for (int index : cpuIndices) {
        if (index >= 0 && index < 64) {  // Windows支持最多64个处理器
            affinityMask |= (static_cast<DWORD_PTR>(1) << index);
        }
    }

    BOOL result = SetProcessAffinityMask(hProcess, affinityMask);
    CloseHandle(hProcess);

    if (!result) {
        LOG_F(ERROR, "设置进程CPU亲和性失败: PID={}, 错误码={}", pid,
              GetLastError());
        return false;
    }
    return true;
#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    for (int index : cpuIndices) {
        if (index >= 0 && index < CPU_SETSIZE) {
            CPU_SET(index, &cpuset);
        }
    }

    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) != 0) {
        LOG_F(ERROR, "设置进程CPU亲和性失败: PID={}, 错误={}", pid,
              strerror(errno));
        return false;
    }
    return true;
#elif defined(__APPLE__)
    // macOS不支持设置CPU亲和性
    LOG_F(WARNING, "macOS不支持设置CPU亲和性");
    return false;
#else
    return false;
#endif
}

auto getProcessAffinity(int pid) -> std::vector<int> {
    std::vector<int> cpuIndices;

#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == nullptr) {
        LOG_F(ERROR, "无法打开进程: PID={}", pid);
        return cpuIndices;
    }

    DWORD_PTR processAffinity, systemAffinity;
    if (GetProcessAffinityMask(hProcess, &processAffinity, &systemAffinity)) {
        for (int i = 0; i < 64; i++) {  // 最多64个处理器
            if (processAffinity & (static_cast<DWORD_PTR>(1) << i)) {
                cpuIndices.push_back(i);
            }
        }
    } else {
        LOG_F(ERROR, "获取进程CPU亲和性失败: PID={}, 错误码={}", pid,
              GetLastError());
    }

    CloseHandle(hProcess);
#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    if (sched_getaffinity(pid, sizeof(cpu_set_t), &cpuset) == 0) {
        for (int i = 0; i < CPU_SETSIZE; i++) {
            if (CPU_ISSET(i, &cpuset)) {
                cpuIndices.push_back(i);
            }
        }
    } else {
        LOG_F(ERROR, "获取进程CPU亲和性失败: PID={}, 错误={}", pid,
              strerror(errno));
    }
#elif defined(__APPLE__)
    // macOS不支持获取CPU亲和性
    LOG_F(WARNING, "macOS不支持获取CPU亲和性");
#endif

    return cpuIndices;
}

auto setProcessMemoryLimit(int pid, std::size_t limitBytes) -> bool {
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_SET_QUOTA, FALSE, pid);
    if (hProcess == nullptr) {
        LOG_F(ERROR, "无法打开进程: PID={}", pid);
        return false;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {};
    jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    jobInfo.ProcessMemoryLimit = limitBytes;

    HANDLE hJob = CreateJobObject(nullptr, nullptr);
    if (hJob == nullptr) {
        LOG_F(ERROR, "创建作业对象失败: 错误码={}", GetLastError());
        CloseHandle(hProcess);
        return false;
    }

    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation,
                                 &jobInfo, sizeof(jobInfo))) {
        LOG_F(ERROR, "设置作业对象信息失败: 错误码={}", GetLastError());
        CloseHandle(hJob);
        CloseHandle(hProcess);
        return false;
    }

    if (!AssignProcessToJobObject(hJob, hProcess)) {
        LOG_F(ERROR, "将进程分配到作业对象失败: 错误码={}", GetLastError());
        CloseHandle(hJob);
        CloseHandle(hProcess);
        return false;
    }

    CloseHandle(hProcess);
    // 注意：不要关闭作业对象句柄，否则限制将被解除
    // CloseHandle(hJob);

    return true;
#elif defined(__linux__)
    // 在Linux上使用cgroups设置内存限制
    std::string cgroupPath = "/sys/fs/cgroup/memory/";
    std::string processGroup = "process_" + std::to_string(pid);
    std::string fullPath = cgroupPath + processGroup;

    // 创建cgroup
    if (mkdir(fullPath.c_str(), 0755) != 0 && errno != EEXIST) {
        LOG_F(ERROR, "创建cgroup失败: {}", strerror(errno));
        return false;
    }

    // 设置内存限制
    std::ofstream limitFile(fullPath + "/memory.limit_in_bytes");
    if (!limitFile) {
        LOG_F(ERROR, "打开内存限制文件失败");
        return false;
    }
    limitFile << limitBytes;
    limitFile.close();

    // 添加进程到cgroup
    std::ofstream tasksFile(fullPath + "/tasks");
    if (!tasksFile) {
        LOG_F(ERROR, "打开任务文件失败");
        return false;
    }
    tasksFile << pid;
    tasksFile.close();

    return true;
#else
    LOG_F(WARNING, "当前平台不支持设置进程内存限制");
    return false;
#endif
}

auto getProcessPath(int pid) -> std::string {
#ifdef _WIN32
    HANDLE hProcess =
        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == nullptr) {
        LOG_F(ERROR, "无法打开进程: PID={}", pid);
        return "";
    }

    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameExW(hProcess, nullptr, path, MAX_PATH);
    CloseHandle(hProcess);

    if (length == 0) {
        LOG_F(ERROR, "获取进程路径失败: PID={}, 错误码={}", pid,
              GetLastError());
        return "";
    }

    return atom::utils::WCharArrayToString(path);
#elif defined(__linux__)
    std::string procPath = "/proc/" + std::to_string(pid) + "/exe";
    char path[PATH_MAX];
    ssize_t length = readlink(procPath.c_str(), path, PATH_MAX - 1);

    if (length == -1) {
        LOG_F(ERROR, "获取进程路径失败: PID={}, 错误={}", pid, strerror(errno));
        return "";
    }

    path[length] = '\0';
    return std::string(path);
#elif defined(__APPLE__)
    char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
    int ret = proc_pidpath(pid, pathbuf, sizeof(pathbuf));

    if (ret <= 0) {
        LOG_F(ERROR, "获取进程路径失败: PID={}, 错误={}", pid, strerror(errno));
        return "";
    }

    return std::string(pathbuf);
#else
    return "";
#endif
}

auto monitorProcessResource(
    int pid, const std::string &resourceType, double threshold,
    std::function<void(int, const std::string &, double)> callback,
    unsigned int intervalMs) -> int {
    return ResourceMonitorManager::getInstance().startMonitoring(
        pid, resourceType, threshold, callback, intervalMs);
}

auto getProcessSyscalls(int pid)
    -> std::unordered_map<std::string, unsigned long> {
    std::unordered_map<std::string, unsigned long> syscalls;

#ifdef __linux__
    // Linux上可以通过/proc/[pid]/syscall查看当前系统调用
    // 或者通过strace工具统计系统调用
    std::string cmd = "strace -c -p " + std::to_string(pid) + " 2>&1";
    auto [output, status] = executeCommandWithStatus(cmd);

    if (status != 0) {
        LOG_F(ERROR, "执行strace命令失败: {}", output);
        return syscalls;
    }

    // 解析strace输出
    std::istringstream iss(output);
    std::string line;
    bool foundHeader = false;

    while (std::getline(iss, line)) {
        if (line.find("calls") != std::string::npos &&
            line.find("errors") != std::string::npos) {
            foundHeader = true;
            continue;
        }

        if (foundHeader) {
            std::istringstream lineStream(line);
            std::string syscallName;
            unsigned long callCount;

            // 跳过百分比和其他字段
            std::string dummy;
            lineStream >> dummy;      // 百分比
            lineStream >> callCount;  // 调用次数

            // 跳过多个字段，直到系统调用名称
            for (int i = 0; i < 4; i++) {
                lineStream >> dummy;
            }

            lineStream >> syscallName;
            if (!syscallName.empty()) {
                syscalls[syscallName] = callCount;
            }
        }
    }
#else
    LOG_F(WARNING, "当前平台不支持获取系统调用统计");
#endif

    return syscalls;
}

auto getProcessNetworkConnections(int pid) -> std::vector<NetworkConnection> {
    std::vector<NetworkConnection> connections;

#ifdef _WIN32
    // 使用Windows API获取网络连接
    MIB_TCPTABLE_OWNER_PID *pTcpTable = nullptr;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    // 获取TCP表所需大小
    dwRetVal = GetExtendedTcpTable(nullptr, &dwSize, TRUE, AF_INET,
                                   TCP_TABLE_OWNER_PID_ALL, 0);
    if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) {
        pTcpTable = (MIB_TCPTABLE_OWNER_PID *)malloc(dwSize);
        if (pTcpTable == nullptr) {
            LOG_F(ERROR, "内存分配失败");
            return connections;
        }

        dwRetVal = GetExtendedTcpTable(pTcpTable, &dwSize, TRUE, AF_INET,
                                       TCP_TABLE_OWNER_PID_ALL, 0);
        if (dwRetVal == NO_ERROR) {
            for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++) {
                if (pTcpTable->table[i].dwOwningPid ==
                    static_cast<DWORD>(pid)) {
                    NetworkConnection conn;
                    conn.protocol = "TCP";

                    // 获取本地地址
                    struct in_addr localAddr;
                    localAddr.s_addr = pTcpTable->table[i].dwLocalAddr;
                    char localAddrStr[16];  // Buffer size sufficient for IPv4
                                            // (xxx.xxx.xxx.xxx)
                    InetNtopA(AF_INET, &localAddr, localAddrStr,
                              sizeof(localAddrStr));
                    conn.localAddress = localAddrStr;
                    conn.localPort =
                        ntohs((u_short)pTcpTable->table[i].dwLocalPort);

                    // 获取远程地址
                    struct in_addr remoteAddr;
                    remoteAddr.s_addr = pTcpTable->table[i].dwRemoteAddr;
                    char remoteAddrStr[16];  // Buffer size sufficient for IPv4
                                             // (xxx.xxx.xxx.xxx)
                    InetNtopA(AF_INET, &remoteAddr, remoteAddrStr,
                              sizeof(remoteAddrStr));
                    conn.remoteAddress = remoteAddrStr;
                    conn.remotePort =
                        ntohs((u_short)pTcpTable->table[i].dwRemotePort);
                    // 获取连接状态
                    switch (pTcpTable->table[i].dwState) {
                        case MIB_TCP_STATE_CLOSED:
                            conn.status = "CLOSED";
                            break;
                        case MIB_TCP_STATE_LISTEN:
                            conn.status = "LISTEN";
                            break;
                        case MIB_TCP_STATE_ESTAB:
                            conn.status = "ESTABLISHED";
                            break;
                        default:
                            conn.status = "OTHER";
                    }

                    connections.push_back(conn);
                }
            }
        }
        free(pTcpTable);
    }
#elif defined(__linux__)
    // 读取/proc/[pid]/net/tcp和/proc/[pid]/net/udp文件
    std::vector<std::string> protocols = {"tcp", "udp"};

    for (const auto &proto : protocols) {
        std::string netPath = "/proc/" + std::to_string(pid) + "/net/" + proto;
        std::ifstream netFile(netPath);

        if (!netFile) {
            continue;
        }

        std::string line;
        // 跳过标题行
        std::getline(netFile, line);

        while (std::getline(netFile, line)) {
            NetworkConnection conn;
            conn.protocol = proto;

            std::istringstream iss(line);
            std::string sl, localAddr, remoteAddr, status, other;

            iss >> sl >> localAddr >> remoteAddr >> status >> other;

            // 解析本地地址和端口
            size_t colonPos = localAddr.find(':');
            if (colonPos != std::string::npos) {
                std::string addrHex = localAddr.substr(0, colonPos);
                std::string portHex = localAddr.substr(colonPos + 1);

                // 转换十六进制地址为IP地址
                unsigned int addr;
                std::stringstream ss;
                ss << std::hex << addrHex;
                ss >> addr;

                unsigned char bytes[4];
                bytes[0] = addr & 0xFF;
                bytes[1] = (addr >> 8) & 0xFF;
                bytes[2] = (addr >> 16) & 0xFF;
                bytes[3] = (addr >> 24) & 0xFF;

                std::stringstream addrStream;
                addrStream << static_cast<int>(bytes[3]) << "."
                           << static_cast<int>(bytes[2]) << "."
                           << static_cast<int>(bytes[1]) << "."
                           << static_cast<int>(bytes[0]);
                conn.localAddress = addrStream.str();

                // 转换十六进制端口
                std::stringstream portStream;
                portStream << std::hex << portHex;
                portStream >> conn.localPort;
            }

            // 解析远程地址和端口
            colonPos = remoteAddr.find(':');
            if (colonPos != std::string::npos) {
                std::string addrHex = remoteAddr.substr(0, colonPos);
                std::string portHex = remoteAddr.substr(colonPos + 1);

                // 转换十六进制地址为IP地址
                unsigned int addr;
                std::stringstream ss;
                ss << std::hex << addrHex;
                ss >> addr;

                unsigned char bytes[4];
                bytes[0] = addr & 0xFF;
                bytes[1] = (addr >> 8) & 0xFF;
                bytes[2] = (addr >> 16) & 0xFF;
                bytes[3] = (addr >> 24) & 0xFF;

                std::stringstream addrStream;
                addrStream << static_cast<int>(bytes[3]) << "."
                           << static_cast<int>(bytes[2]) << "."
                           << static_cast<int>(bytes[1]) << "."
                           << static_cast<int>(bytes[0]);
                conn.remoteAddress = addrStream.str();

                // 转换十六进制端口
                std::stringstream portStream;
                portStream << std::hex << portHex;
                portStream >> conn.remotePort;
            }

            // 转换状态码
            if (proto == "tcp") {
                // 将状态代码转换为可读形式
                switch (std::stoi(status, nullptr, 16)) {
                    case 1:
                        conn.status = "ESTABLISHED";
                        break;
                    case 2:
                        conn.status = "SYN_SENT";
                        break;
                    case 3:
                        conn.status = "SYN_RECV";
                        break;
                    case 4:
                        conn.status = "FIN_WAIT1";
                        break;
                    case 5:
                        conn.status = "FIN_WAIT2";
                        break;
                    case 6:
                        conn.status = "TIME_WAIT";
                        break;
                    case 7:
                        conn.status = "CLOSE";
                        break;
                    case 8:
                        conn.status = "CLOSE_WAIT";
                        break;
                    case 9:
                        conn.status = "LAST_ACK";
                        break;
                    case 10:
                        conn.status = "LISTEN";
                        break;
                    default:
                        conn.status = "UNKNOWN";
                }
            } else {
                // UDP没有状态，设为空
                conn.status = "";
            }

            connections.push_back(conn);
        }
    }
#elif defined(__APPLE__)
    // 使用lsof命令获取网络连接信息
    std::string cmd = "lsof -i -n -P -p " + std::to_string(pid);
    auto [output, status] = executeCommandWithStatus(cmd);

    if (status != 0) {
        LOG_F(ERROR, "执行lsof命令失败: {}", output);
        return connections;
    }

    std::istringstream iss(output);
    std::string line;
    bool skipHeader = true;

    while (std::getline(iss, line)) {
        if (skipHeader) {
            skipHeader = false;
            continue;
        }

        std::istringstream lineStream(line);
        std::string command, pid, user, fd, type, device, sizeOff, node, name;

        lineStream >> command >> pid >> user >> fd >> type >> device >>
            sizeOff >> node;
        std::getline(lineStream, name);  // 剩余部分为name

        // 只处理网络连接
        if (type != "IPv4" && type != "IPv6") {
            continue;
        }

        NetworkConnection conn;
        conn.protocol = (name.find("UDP") != std::string::npos) ? "UDP" : "TCP";

        // 解析地址和端口信息，格式如： TCP 127.0.0.1:8080->127.0.0.1:12345
        // (ESTABLISHED)
        auto addrInfo = name;
        addrInfo =
            addrInfo.substr(addrInfo.find_first_not_of(' '));  // 去除前导空格

        // 提取状态信息
        size_t statusPos = addrInfo.find('(');
        if (statusPos != std::string::npos) {
            conn.status = addrInfo.substr(statusPos + 1);
            conn.status = conn.status.substr(0, conn.status.find(')'));
            addrInfo = addrInfo.substr(0, statusPos);
        }

        // 解析本地和远程地址
        size_t arrowPos = addrInfo.find("->");
        if (arrowPos != std::string::npos) {
            // 有远程连接
            std::string localPart = addrInfo.substr(0, arrowPos);
            std::string remotePart = addrInfo.substr(arrowPos + 2);

            size_t colonPos = localPart.find(':');
            if (colonPos != std::string::npos) {
                conn.localAddress = localPart.substr(0, colonPos);
                conn.localPort = std::stoi(localPart.substr(colonPos + 1));
            }

            colonPos = remotePart.find(':');
            if (colonPos != std::string::npos) {
                conn.remoteAddress = remotePart.substr(0, colonPos);
                conn.remotePort = std::stoi(remotePart.substr(colonPos + 1));
            }
        } else {
            // 只有本地监听
            size_t colonPos = addrInfo.find(':');
            if (colonPos != std::string::npos) {
                conn.localAddress = addrInfo.substr(0, colonPos);
                conn.localPort = std::stoi(addrInfo.substr(colonPos + 1));
                conn.remoteAddress = "*";
                conn.remotePort = 0;
            }
        }

        connections.push_back(conn);
    }
#endif

    return connections;
}

auto getProcessFileDescriptors(int pid) -> std::vector<FileDescriptor> {
    std::vector<FileDescriptor> fds;

#ifdef _WIN32
    // Windows上获取打开的句柄信息比较复杂，这里简化处理
    LOG_F(WARNING, "Windows平台上获取文件句柄列表功能尚未实现");
#elif defined(__linux__)
    std::string fdPath = "/proc/" + std::to_string(pid) + "/fd";
    DIR *dir = opendir(fdPath.c_str());
    if (dir != nullptr) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            // 跳过.和..
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            int fd = std::stoi(entry->d_name);
            std::string linkPath = fdPath + "/" + entry->d_name;
            char target[PATH_MAX];
            ssize_t len = readlink(linkPath.c_str(), target, PATH_MAX - 1);

            if (len != -1) {
                target[len] = '\0';
                FileDescriptor fdInfo;
                fdInfo.fd = fd;
                fdInfo.path = target;

                // 确定文件类型
                if (strncmp(target, "socket:", 7) == 0) {
                    fdInfo.type = "socket";
                } else if (strncmp(target, "pipe:", 5) == 0) {
                    fdInfo.type = "pipe";
                } else if (strncmp(target, "/dev/", 5) == 0) {
                    fdInfo.type = "device";
                } else {
                    fdInfo.type = "regular";
                }

                // 获取访问模式（这是个近似值，精确值需要更复杂的处理）
                std::string fdInfoPath =
                    "/proc/" + std::to_string(pid) + "/fdinfo/" + entry->d_name;
                std::ifstream fdInfoFile(fdInfoPath);
                if (fdInfoFile) {
                    std::string line;
                    while (std::getline(fdInfoFile, line)) {
                        if (line.find("flags:") == 0) {
                            int flags = std::stoi(
                                line.substr(line.find_first_of("0123456789")));
                            if ((flags & 0x3) == 0x0) {
                                fdInfo.mode = "r";
                            } else if ((flags & 0x3) == 0x1) {
                                fdInfo.mode = "w";
                            } else if ((flags & 0x3) == 0x2) {
                                fdInfo.mode = "rw";
                            } else {
                                fdInfo.mode = "unknown";
                            }
                            break;
                        }
                    }
                }

                fds.push_back(fdInfo);
            }
        }
        closedir(dir);
    }
#elif defined(__APPLE__)
    // 在macOS上使用lsof命令获取文件描述符信息
    std::string cmd = "lsof -p " + std::to_string(pid);
    auto [output, status] = executeCommandWithStatus(cmd);

    if (status != 0) {
        LOG_F(ERROR, "执行lsof命令失败: {}", output);
        return fds;
    }

    std::istringstream iss(output);
    std::string line;
    bool skipHeader = true;

    while (std::getline(iss, line)) {
        if (skipHeader) {
            skipHeader = false;
            continue;
        }

        std::istringstream lineStream(line);
        std::string command, pid, user, fd, type, device, sizeOff, node, name;

        lineStream >> command >> pid >> user >> fd;

        // 提取文件描述符编号
        int fdNum = -1;
        if (fd.find('r') != std::string::npos ||
            fd.find('w') != std::string::npos) {
            std::string fdNumStr = fd;
            fdNumStr.erase(
                std::remove_if(fdNumStr.begin(), fdNumStr.end(),
                               [](char c) { return !std::isdigit(c); }),
                fdNumStr.end());
            if (!fdNumStr.empty()) {
                fdNum = std::stoi(fdNumStr);
            }
        } else {
            try {
                fdNum = std::stoi(fd);
            } catch (...) {
                continue;  // 跳过无法解析的行
            }
        }

        if (fdNum == -1) {
            continue;
        }

        lineStream >> type >> device >> sizeOff >> node;
        std::getline(lineStream, name);  // 剩余部分为路径

        FileDescriptor fdInfo;
        fdInfo.fd = fdNum;
        fdInfo.path = name;
        fdInfo.type = type;

        // 确定访问模式
        if (fd.find('r') != std::string::npos &&
            fd.find('w') != std::string::npos) {
            fdInfo.mode = "rw";
        } else if (fd.find('r') != std::string::npos) {
            fdInfo.mode = "r";
        } else if (fd.find('w') != std::string::npos) {
            fdInfo.mode = "w";
        } else {
            fdInfo.mode = "unknown";
        }

        fds.push_back(fdInfo);
    }
#endif

    return fds;
}

auto getProcessPerformanceHistory(int pid, std::chrono::seconds duration,
                                  unsigned int intervalMs)
    -> PerformanceHistory {
    return PerformanceHistoryManager::getInstance().collectHistory(
        pid, duration, intervalMs);
}

auto setProcessIOPriority(int pid, int priority) -> bool {
    if (priority < 0 || priority > 7) {
        LOG_F(ERROR, "IO优先级必须在0-7范围内");
        return false;
    }

#ifdef __linux__
    // Linux使用ioprio_set系统调用
    // 由于这是一个不常用的系统调用，需要直接使用syscall
    long ioprio = (4 << 13) | priority;  // 4是IOPRIO_CLASS_BE (best effort)
    int ret = syscall(SYS_ioprio_set, 1, pid, ioprio);  // 1是IOPRIO_WHO_PROCESS

    if (ret == -1) {
        LOG_F(ERROR, "设置IO优先级失败: PID={}, 错误={}", pid, strerror(errno));
        return false;
    }
    return true;
#else
    LOG_F(WARNING, "当前平台不支持设置IO优先级");
    return false;
#endif
}

auto getProcessIOPriority(int pid) -> int {
#ifdef __linux__
    // Linux使用ioprio_get系统调用
    long ioprio = syscall(SYS_ioprio_get, 1, pid);  // 1是IOPRIO_WHO_PROCESS

    if (ioprio == -1) {
        LOG_F(ERROR, "获取IO优先级失败: PID={}, 错误={}", pid, strerror(errno));
        return -1;
    }

    // 提取优先级值
    return ioprio & 0xFF;
#else
    LOG_F(WARNING, "当前平台不支持获取IO优先级");
    return -1;
#endif
}

auto sendSignalToProcess(int pid, int signal) -> bool {
#ifdef _WIN32
    if (signal == SIGTERM) {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess == nullptr) {
            LOG_F(ERROR, "无法打开进程: PID={}", pid);
            return false;
        }

        BOOL result = TerminateProcess(hProcess, 1);
        CloseHandle(hProcess);

        if (!result) {
            LOG_F(ERROR, "终止进程失败: PID={}, 错误码={}", pid,
                  GetLastError());
            return false;
        }
        return true;
    } else if (signal == SIGINT) {
        // 发送Ctrl+C事件
        if (!GenerateConsoleCtrlEvent(CTRL_C_EVENT, pid)) {
            LOG_F(ERROR, "发送Ctrl+C事件失败: PID={}, 错误码={}", pid,
                  GetLastError());
            return false;
        }
        return true;
    } else {
        LOG_F(ERROR, "Windows平台不支持信号: {}", signal);
        return false;
    }
#else
    // POSIX系统使用kill发送信号
    if (kill(pid, signal) != 0) {
        LOG_F(ERROR, "发送信号失败: PID={}, 信号={}, 错误={}", pid, signal,
              strerror(errno));
        return false;
    }
    return true;
#endif
}

auto findProcesses(std::function<bool(const Process &)> predicate)
    -> std::vector<int> {
    std::vector<int> matchingPids;

    auto allProcesses = getAllProcesses();
    for (const auto &[pid, name] : allProcesses) {
        try {
            Process process = getProcessInfoByPid(pid);
            if (predicate(process)) {
                matchingPids.push_back(pid);
            }
        } catch (const std::exception &e) {
            LOG_F(WARNING, "获取进程{}信息时出错: {}", pid, e.what());
        }
    }

    return matchingPids;
}

auto ctermid() -> std::string {
#if defined(_WIN32)
    return "CON";  // Windows控制台
#else
    char term[L_ctermid];
    return ::ctermid(term);
#endif
}

}  // namespace atom::system
