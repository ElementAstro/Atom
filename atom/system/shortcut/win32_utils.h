#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <memory>

namespace shortcut_detector {
namespace win32_utils {

/**
 * @brief Process information structure for efficient caching
 */
struct ProcessInfo {
    DWORD processId;
    std::string name;
    std::string fullPath;
    bool hasKeyboardHook;
    std::vector<std::string> loadedModules;

    ProcessInfo() : processId(0), hasKeyboardHook(false) {}
    ProcessInfo(DWORD pid, const std::string& processName)
        : processId(pid), name(processName), hasKeyboardHook(false) {}
};

/**
 * @brief Filter criteria for process enumeration
 */
struct ProcessFilter {
    std::unordered_set<std::string> includeNames;     // Only include these process names
    std::unordered_set<std::string> excludeNames;     // Exclude these process names
    std::unordered_set<DWORD> includePids;            // Only include these PIDs
    std::unordered_set<DWORD> excludePids;            // Exclude these PIDs
    bool onlyWithHooks{true};                         // Only return processes with hooks
    bool includeSystemProcesses{false};               // Include system processes
    size_t maxResults{100};                           // Maximum number of results

    bool shouldInclude(const ProcessInfo& info) const;
};

/**
 * @brief Performance-optimized process enumeration
 */
class ProcessEnumerator {
public:
    ProcessEnumerator();
    ~ProcessEnumerator();

    /**
     * @brief Get processes with keyboard hooks (optimized)
     *
     * @param filter Optional filter criteria
     * @return std::vector<ProcessInfo> Process information
     */
    std::vector<ProcessInfo> getProcessesWithKeyboardHooks(
        const ProcessFilter& filter = ProcessFilter{});

    /**
     * @brief Get all processes (cached)
     *
     * @param forceRefresh Force refresh of process list
     * @return std::vector<ProcessInfo> All process information
     */
    std::vector<ProcessInfo> getAllProcesses(bool forceRefresh = false);

    /**
     * @brief Check if specific process has keyboard hooks (fast)
     *
     * @param processId Process ID
     * @return bool True if process has keyboard hooks
     */
    bool hasKeyboardHook(DWORD processId);

    /**
     * @brief Get process information by PID (cached)
     *
     * @param processId Process ID
     * @return std::unique_ptr<ProcessInfo> Process info or nullptr
     */
    std::unique_ptr<ProcessInfo> getProcessInfo(DWORD processId);

    /**
     * @brief Clear internal caches
     */
    void clearCache();

    /**
     * @brief Set cache refresh interval
     *
     * @param intervalMs Refresh interval in milliseconds
     */
    void setCacheRefreshInterval(DWORD intervalMs);

private:
    std::unordered_map<DWORD, ProcessInfo> processCache_;
    DWORD lastRefreshTime_;
    DWORD cacheRefreshInterval_;

    void refreshProcessCache();
    bool isHookingModule(const std::string& moduleName);
    std::vector<std::string> getProcessModules(DWORD processId);
};

/**
 * @brief Get a list of processes that have keyboard hooks installed (legacy)
 *
 * @return std::vector<std::string> Process names
 */
std::vector<std::string> getProcessesWithKeyboardHooks();

/**
 * @brief Check if a process has keyboard hooks (legacy)
 *
 * @param processId Process ID
 * @return bool True if process has keyboard hooks
 */
bool checkProcessForKeyboardHook(DWORD processId);

/**
 * @brief Get executable name from process ID (optimized)
 *
 * @param processId Process ID
 * @return std::string Process name
 */
std::string getProcessName(DWORD processId);

/**
 * @brief Get full executable path from process ID
 *
 * @param processId Process ID
 * @return std::string Full process path
 */
std::string getProcessPath(DWORD processId);

/**
 * @brief Check if module is a hook DLL (optimized with static lookup)
 *
 * @param moduleName Module name
 * @return bool True if likely to be hook DLL
 */
bool isHookingModule(const std::string& moduleName);

/**
 * @brief Get system information for performance optimization
 *
 * @return SYSTEM_INFO System information
 */
SYSTEM_INFO getSystemInfo();

/**
 * @brief Check if current process has required privileges
 *
 * @return bool True if has required privileges
 */
bool hasRequiredPrivileges();

/**
 * @brief Enable debug privileges for better process access
 *
 * @return bool True if privileges were enabled successfully
 */
bool enableDebugPrivileges();

/**
 * @brief Batch process information retrieval for efficiency
 *
 * @param processIds Vector of process IDs to query
 * @return std::unordered_map<DWORD, ProcessInfo> Process information map
 */
std::unordered_map<DWORD, ProcessInfo> getProcessInfoBatch(
    const std::vector<DWORD>& processIds);

}  // namespace win32_utils
}  // namespace shortcut_detector
