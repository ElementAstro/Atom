#include "win32_utils.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <spdlog/spdlog.h>

// clang-format off
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
// clang-format on

namespace shortcut_detector {
namespace win32_utils {

/**
 * @brief Known keyboard hook DLL modules commonly used by applications (expanded)
 */
static const std::unordered_set<std::string> knownHookDlls = {
    "HOOK.DLL",         "KBDHOOK.DLL",          "KEYHOOK.DLL",
    "INPUTHOOK.DLL",    "WINHOOK.DLL",          "LLKEYBOARD.DLL",
    "KEYMAGIC.DLL",     "HOOKSPY.DLL",          "KEYBOARDHOOK.DLL",
    "INPUTMANAGERHOOK.DLL", "UIHOOK.DLL",       "GLOBALHOOK.DLL",
    "SYSTEMHOOK.DLL",   "APIHOOK.DLL",          "INTERCEPTOR.DLL",
    "KEYLOGGER.DLL",    "INPUTCAPTURE.DLL",     "WINAPI_HOOK.DLL"
};

/**
 * @brief System processes to exclude from hook detection (performance optimization)
 */
static const std::unordered_set<std::string> systemProcesses = {
    "SYSTEM", "SMSS.EXE", "CSRSS.EXE", "WININIT.EXE", "WINLOGON.EXE",
    "SERVICES.EXE", "LSASS.EXE", "SVCHOST.EXE", "SPOOLSV.EXE",
    "EXPLORER.EXE", "DWMWM.EXE", "DWMCORE.DLL"
};

// Global process enumerator instance for caching
static std::unique_ptr<ProcessEnumerator> g_processEnumerator;

std::vector<std::string> getProcessesWithKeyboardHooks() {
    std::vector<std::string> result;

    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        spdlog::error("Failed to create process snapshot: {}", GetLastError());
        return result;
    }

    PROCESSENTRY32 processEntry = {sizeof(PROCESSENTRY32)};

    if (Process32First(snapshot, &processEntry)) {
        do {
            if (checkProcessForKeyboardHook(processEntry.th32ProcessID)) {
                const std::string processName = getProcessName(processEntry.th32ProcessID);
                result.push_back(processName);
                spdlog::debug("Found process with keyboard hook: {}", processName);
            }
        } while (Process32Next(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    spdlog::debug("Found {} processes with keyboard hooks", result.size());
    return result;
}

bool checkProcessForKeyboardHook(DWORD processId) {
    if (processId == 0) {
        return false;
    }

    const HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                       FALSE, processId);
    if (!hProcess) {
        return false;
    }

    bool result = false;
    HMODULE modules[1024];
    DWORD needed;

    if (EnumProcessModules(hProcess, modules, sizeof(modules), &needed)) {
        const DWORD moduleCount = needed / sizeof(HMODULE);
        for (DWORD i = 0; i < moduleCount; ++i) {
            char moduleName[MAX_PATH];
            if (GetModuleFileNameExA(hProcess, modules[i], moduleName, sizeof(moduleName))) {
                std::string name = moduleName;
                const size_t pos = name.find_last_of("\\/");
                if (pos != std::string::npos) {
                    name = name.substr(pos + 1);
                }

                std::transform(name.begin(), name.end(), name.begin(), ::toupper);

                if (isHookingModule(name)) {
                    result = true;
                    break;
                }
            }
        }
    }

    CloseHandle(hProcess);
    return result;
}

std::string getProcessName(DWORD processId) {
    std::string result = "Unknown Process";

    const HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                       FALSE, processId);
    if (hProcess) {
        char name[MAX_PATH];
        if (GetModuleFileNameExA(hProcess, NULL, name, sizeof(name))) {
            const std::string fullPath = name;
            const size_t pos = fullPath.find_last_of("\\/");
            if (pos != std::string::npos) {
                result = fullPath.substr(pos + 1);
            } else {
                result = fullPath;
            }
        }
        CloseHandle(hProcess);
    }

    return result;
}

bool isHookingModule(const std::string& moduleName) {
    if (knownHookDlls.find(moduleName) != knownHookDlls.end()) {
        return true;
    }

    return (moduleName.find("HOOK") != std::string::npos) ||
           (moduleName.find("KEYB") != std::string::npos &&
            moduleName.find("MONITOR") != std::string::npos) ||
           (moduleName.find("INPUT") != std::string::npos &&
            moduleName.find("HOOK") != std::string::npos);
}

// ProcessFilter implementation
bool ProcessFilter::shouldInclude(const ProcessInfo& info) const {
    // Check PID filters first (fastest)
    if (!excludePids.empty() && excludePids.count(info.processId)) {
        return false;
    }
    if (!includePids.empty() && !includePids.count(info.processId)) {
        return false;
    }

    // Check name filters
    if (!excludeNames.empty() && excludeNames.count(info.name)) {
        return false;
    }
    if (!includeNames.empty() && !includeNames.count(info.name)) {
        return false;
    }

    // Check hook requirement
    if (onlyWithHooks && !info.hasKeyboardHook) {
        return false;
    }

    // Check system process filter
    if (!includeSystemProcesses && systemProcesses.count(info.name)) {
        return false;
    }

    return true;
}

// ProcessEnumerator implementation
ProcessEnumerator::ProcessEnumerator()
    : lastRefreshTime_(0), cacheRefreshInterval_(1000) {
    // Initialize cache on first use
}

ProcessEnumerator::~ProcessEnumerator() = default;

std::vector<ProcessInfo> ProcessEnumerator::getProcessesWithKeyboardHooks(const ProcessFilter& filter) {
    refreshProcessCache();

    std::vector<ProcessInfo> result;
    result.reserve(50); // Pre-allocate for performance

    for (const auto& [pid, info] : processCache_) {
        if (filter.shouldInclude(info)) {
            result.push_back(info);
            if (result.size() >= filter.maxResults) {
                break;
            }
        }
    }

    return result;
}

std::vector<ProcessInfo> ProcessEnumerator::getAllProcesses(bool forceRefresh) {
    if (forceRefresh) {
        clearCache();
    }
    refreshProcessCache();

    std::vector<ProcessInfo> result;
    result.reserve(processCache_.size());

    for (const auto& [pid, info] : processCache_) {
        result.push_back(info);
    }

    return result;
}

bool ProcessEnumerator::hasKeyboardHook(DWORD processId) {
    refreshProcessCache();

    auto it = processCache_.find(processId);
    return it != processCache_.end() && it->second.hasKeyboardHook;
}

std::unique_ptr<ProcessInfo> ProcessEnumerator::getProcessInfo(DWORD processId) {
    refreshProcessCache();

    auto it = processCache_.find(processId);
    if (it != processCache_.end()) {
        return std::make_unique<ProcessInfo>(it->second);
    }
    return nullptr;
}

void ProcessEnumerator::clearCache() {
    processCache_.clear();
    lastRefreshTime_ = 0;
}

void ProcessEnumerator::setCacheRefreshInterval(DWORD intervalMs) {
    cacheRefreshInterval_ = intervalMs;
}

void ProcessEnumerator::refreshProcessCache() {
    DWORD currentTime = GetTickCount();
    if (currentTime - lastRefreshTime_ < cacheRefreshInterval_) {
        return; // Cache is still fresh
    }

    processCache_.clear();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        spdlog::error("Failed to create process snapshot: {}", GetLastError());
        return;
    }

    PROCESSENTRY32 processEntry = {sizeof(PROCESSENTRY32)};

    if (Process32First(snapshot, &processEntry)) {
        do {
            ProcessInfo info(processEntry.th32ProcessID, processEntry.szExeFile);
            info.hasKeyboardHook = checkProcessForKeyboardHook(processEntry.th32ProcessID);
            info.loadedModules = getProcessModules(processEntry.th32ProcessID);

            processCache_[processEntry.th32ProcessID] = std::move(info);
        } while (Process32Next(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    lastRefreshTime_ = currentTime;

    spdlog::debug("Refreshed process cache with {} processes", processCache_.size());
}

std::vector<std::string> ProcessEnumerator::getProcessModules(DWORD processId) {
    std::vector<std::string> modules;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!hProcess) {
        return modules;
    }

    HMODULE hModules[1024];
    DWORD needed;

    if (EnumProcessModules(hProcess, hModules, sizeof(hModules), &needed)) {
        DWORD moduleCount = needed / sizeof(HMODULE);
        modules.reserve(moduleCount);

        for (DWORD i = 0; i < moduleCount; ++i) {
            char moduleName[MAX_PATH];
            if (GetModuleFileNameExA(hProcess, hModules[i], moduleName, sizeof(moduleName))) {
                std::string name = moduleName;
                size_t pos = name.find_last_of("\\/");
                if (pos != std::string::npos) {
                    name = name.substr(pos + 1);
                }
                modules.push_back(name);
            }
        }
    }

    CloseHandle(hProcess);
    return modules;
}

// Additional utility functions
std::string getProcessPath(DWORD processId) {
    std::string result;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess) {
        char path[MAX_PATH];
        if (GetModuleFileNameExA(hProcess, NULL, path, sizeof(path))) {
            result = path;
        }
        CloseHandle(hProcess);
    }

    return result;
}

SYSTEM_INFO getSystemInfo() {
    static SYSTEM_INFO sysInfo = {};
    static bool initialized = false;

    if (!initialized) {
        GetSystemInfo(&sysInfo);
        initialized = true;
    }

    return sysInfo;
}

bool hasRequiredPrivileges() {
    // Check if we can open a system process
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, 4); // System process
    if (hProcess) {
        CloseHandle(hProcess);
        return true;
    }
    return false;
}

bool enableDebugPrivileges() {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return false;
    }

    bool result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
    CloseHandle(hToken);

    return result && GetLastError() == ERROR_SUCCESS;
}

std::unordered_map<DWORD, ProcessInfo> getProcessInfoBatch(const std::vector<DWORD>& processIds) {
    std::unordered_map<DWORD, ProcessInfo> result;

    for (DWORD pid : processIds) {
        ProcessInfo info;
        info.processId = pid;
        info.name = getProcessName(pid);
        info.fullPath = getProcessPath(pid);
        info.hasKeyboardHook = checkProcessForKeyboardHook(pid);

        result[pid] = std::move(info);
    }

    return result;
}

}  // namespace win32_utils
}  // namespace shortcut_detector
