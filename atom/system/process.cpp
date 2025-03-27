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

namespace atom::system {

// 添加一个进程监控管理器
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
    return ProcessMonitorManager::getInstance().stopMonitoring(monitorId);
}

auto getProcessCommandLine(int pid) -> std::vector<std::string> {
    std::vector<std::string> cmdline;

#ifdef _WIN32
    HANDLE hProcess =
        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) {
        LOG_F(ERROR, "无法打开进程: PID={}", pid);
        return cmdline;
    }

    // Windows上没有直接获取命令行参数的API, 使用模块名称作为命令
    char moduleName[MAX_PATH];
    if (GetModuleFileNameExA(hProcess, NULL, moduleName, MAX_PATH) > 0) {
        cmdline.push_back(moduleName);
    }

    CloseHandle(hProcess);
#elif defined(__linux__)
    std::string cmdlinePath = "/proc/" + std::to_string(pid) + "/cmdline";
    std::ifstream cmdlineFile(cmdlinePath);
    if (!cmdlineFile) {
        LOG_F(ERROR, "无法打开命令行文件: {}", cmdlinePath);
        return cmdline;
    }

    std::string line;
    std::getline(cmdlineFile, line);

    // cmdline文件中参数用\0分隔
    std::size_t pos = 0;
    std::size_t lastPos = 0;
    while ((pos = line.find('\0', lastPos)) != std::string::npos) {
        if (pos > lastPos) {
            cmdline.push_back(line.substr(lastPos, pos - lastPos));
        }
        lastPos = pos + 1;

        // 处理文件末尾
        if (lastPos >= line.size()) {
            break;
        }
    }

    // 处理最后一个参数
    if (lastPos < line.size()) {
        cmdline.push_back(line.substr(lastPos));
    }
#elif defined(__APPLE__)
    char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, pathbuf, sizeof(pathbuf)) > 0) {
        cmdline.push_back(pathbuf);
    }

    int mib[3] = {CTL_KERN, KERN_PROCARGS2, pid};
    char buffer[4096];
    size_t bufsize = sizeof(buffer);

    if (sysctl(mib, 3, buffer, &bufsize, nullptr, 0) == 0) {
        // 解析参数
        int nargs;
        memcpy(&nargs, buffer, sizeof(nargs));

        char *cp = buffer + sizeof(nargs);

        // 第一个部分是可执行文件路径，已经添加，所以跳过
        while (cp < buffer + bufsize && *cp != '\0') {
            cp++;
        }
        cp++;

        // 收集参数
        while (cp < buffer + bufsize && *cp != '\0') {
            cmdline.push_back(cp);

            // 移动到下一个参数
            while (cp < buffer + bufsize && *cp != '\0') {
                cp++;
            }
            cp++;
        }
    }
#endif

    return cmdline;
}

auto getProcessEnvironment(int pid)
    -> std::unordered_map<std::string, std::string> {
    std::unordered_map<std::string, std::string> env;

#ifdef _WIN32
    // Windows无法直接获取其他进程的环境变量
    LOG_F(WARNING, "Windows不支持获取其他进程的环境变量");
#elif defined(__linux__)
    std::string envPath = "/proc/" + std::to_string(pid) + "/environ";
    std::ifstream envFile(envPath);
    if (!envFile) {
        LOG_F(ERROR, "无法打开环境变量文件: {}", envPath);
        return env;
    }

    std::string line;
    std::getline(envFile, line, '\0');

    // environ文件中变量用\0分隔
    std::size_t pos = 0;
    while (pos < line.size()) {
        std::string entry = line.substr(pos);
        std::size_t equalsPos = entry.find('=');

        if (equalsPos != std::string::npos) {
            std::string key = entry.substr(0, equalsPos);
            std::string value = entry.substr(equalsPos + 1);
            env[key] = value;
        }

        pos += entry.size() + 1;
    }
#elif defined(__APPLE__)
    // macOS不支持直接获取其他进程的环境变量
    LOG_F(WARNING, "macOS不支持获取其他进程的环境变量");
#endif

    return env;
}

auto getProcessResources(int pid) -> ProcessResource {
    ProcessResource resources;

    // 获取CPU使用率
    resources.cpuUsage = getProcessCpuUsage(pid);

    // 获取内存使用量
    resources.memUsage = getProcessMemoryUsage(pid);

#ifdef _WIN32
    HANDLE hProcess =
        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess != NULL) {
        // 获取虚拟内存使用量
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS *)&pmc,
                                 sizeof(pmc))) {
            resources.vmUsage = pmc.PrivateUsage;
        }

        // 获取线程数
        HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hThreadSnap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te32;
            te32.dwSize = sizeof(THREADENTRY32);
            resources.threadCount = 0;

            if (Thread32First(hThreadSnap, &te32)) {
                do {
                    if (te32.th32OwnerProcessID == pid) {
                        resources.threadCount++;
                    }
                } while (Thread32Next(hThreadSnap, &te32));
            }

            CloseHandle(hThreadSnap);
        }

        CloseHandle(hProcess);
    }

    // Windows不易获取IO统计和打开文件数
    resources.ioRead = 0;
    resources.ioWrite = 0;
    resources.openFiles = 0;
#elif defined(__linux__)
    // 获取虚拟内存使用量
    std::string statmPath = "/proc/" + std::to_string(pid) + "/statm";
    std::ifstream statmFile(statmPath);
    if (statmFile) {
        std::size_t size;
        statmFile >> size;
        resources.vmUsage = size * sysconf(_SC_PAGESIZE);
    }

    // 获取线程数
    std::string statusPath = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream statusFile(statusPath);
    if (statusFile) {
        std::string line;
        while (std::getline(statusFile, line)) {
            if (line.find("Threads:") == 0) {
                std::istringstream iss(line);
                std::string label;
                iss >> label >> resources.threadCount;
                break;
            }
        }
    }

    // 获取IO统计
    std::string ioPath = "/proc/" + std::to_string(pid) + "/io";
    std::ifstream ioFile(ioPath);
    if (ioFile) {
        std::string line;
        while (std::getline(ioFile, line)) {
            if (line.find("read_bytes:") == 0) {
                std::istringstream iss(line);
                std::string label;
                iss >> label >> resources.ioRead;
            } else if (line.find("write_bytes:") == 0) {
                std::istringstream iss(line);
                std::string label;
                iss >> label >> resources.ioWrite;
            }
        }
    }

    // 获取打开的文件数
    std::string fdDir = "/proc/" + std::to_string(pid) + "/fd";
    DIR *dir = opendir(fdDir.c_str());
    if (dir != nullptr) {
        resources.openFiles = 0;
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_LNK) {
                resources.openFiles++;
            }
        }
        closedir(dir);
    }
#elif defined(__APPLE__)
    // 获取进程信息
    task_t task;
    if (task_for_pid(mach_task_self(), pid, &task) == KERN_SUCCESS) {
        // 获取虚拟内存使用量
        struct task_basic_info_64 info;
        mach_msg_type_number_t count = TASK_BASIC_INFO_64_COUNT;
        if (task_info(task, TASK_BASIC_INFO_64, (task_info_t)&info, &count) ==
            KERN_SUCCESS) {
            resources.vmUsage = info.virtual_size;
        }

        // 获取线程数
        thread_act_array_t threads;
        mach_msg_type_number_t thread_count;
        if (task_threads(task, &threads, &thread_count) == KERN_SUCCESS) {
            resources.threadCount = thread_count;
            vm_deallocate(mach_task_self(), (vm_address_t)threads,
                          thread_count * sizeof(thread_t));
        }
    }

    // macOS不易获取IO统计和打开文件数
    resources.ioRead = 0;
    resources.ioWrite = 0;
    resources.openFiles = 0;
#endif

    return resources;
}

#ifdef _WIN32
auto getProcessModules(int pid) -> std::vector<std::string> {
    std::vector<std::string> modules;

    HANDLE hProcess =
        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) {
        LOG_F(ERROR, "无法打开进程: PID={}", pid);
        return modules;
    }

    HMODULE hModules[1024];
    DWORD cbNeeded;

    if (EnumProcessModules(hProcess, hModules, sizeof(hModules), &cbNeeded)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            char moduleName[MAX_PATH];
            if (GetModuleFileNameExA(hProcess, hModules[i], moduleName,
                                     sizeof(moduleName))) {
                modules.emplace_back(moduleName);
            }
        }
    } else {
        LOG_F(ERROR, "枚举进程模块失败: PID={}, 错误码={}", pid,
              GetLastError());
    }

    CloseHandle(hProcess);
    return modules;
}
#endif

#ifdef __linux__
auto getProcessCapabilities(int pid) -> std::vector<std::string> {
    std::vector<std::string> capabilities;

#if __has_include(<sys/capability.h>)
    // 尝试读取进程能力
    cap_t caps = cap_get_pid(pid);
    if (caps == NULL) {
        LOG_F(ERROR, "无法获取进程能力: PID={}, 错误={}", pid, strerror(errno));
        return capabilities;
    }

    char *text = cap_to_text(caps, NULL);
    if (text != NULL) {
        std::string capsText = text;
        cap_free(text);

        // 解析能力字符串
        std::istringstream iss(capsText);
        std::string cap;
        while (std::getline(iss, cap, ',')) {
            capabilities.push_back(cap);
        }
    }

    cap_free(caps);
#else
    // 如果没有capability.h, 尝试从status文件获取
    std::string statusPath = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream statusFile(statusPath);

    if (statusFile) {
        std::string line;
        while (std::getline(statusFile, line)) {
            if (line.find("CapEff:") == 0 || line.find("CapPrm:") == 0 ||
                line.find("CapInh:") == 0) {
                capabilities.push_back(line);
            }
        }
    } else {
        LOG_F(ERROR, "无法打开状态文件: {}", statusPath);
    }
#endif

    return capabilities;
}
#endif

}  // namespace atom::system
