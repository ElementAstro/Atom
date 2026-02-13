#ifndef ATOM_SYSTEM_PROCESS_HPP
#define ATOM_SYSTEM_PROCESS_HPP

#include <chrono>
#include <functional>
#include <optional>
#include "process_info.hpp"

namespace atom::system {

/**
 * @brief Gets information about all processes
 * @return A vector of pairs containing process IDs and names
 */
auto getAllProcesses() -> std::vector<std::pair<int, std::string>>;

/**
 * @brief Gets information about a process by its PID
 * @param pid The process ID
 * @return A Process struct containing information about the process
 */
[[nodiscard("The process info is not used")]]
auto getProcessInfoByPid(int pid) -> Process;

/**
 * @brief Gets information about the current process
 * @return A Process struct containing information about the current process
 */
[[nodiscard("The process info is not used")]]
auto getSelfProcessInfo() -> Process;

/**
 * @brief Returns the name of the controlling terminal
 * @return The name of the controlling terminal
 */
[[nodiscard]]
auto ctermid() -> std::string;

/**
 * @brief Checks if a process is running by its name
 * @param processName The name of the process to check
 * @return True if the process is running, otherwise false
 */
auto isProcessRunning(const std::string &processName) -> bool;

/**
 * @brief Returns the parent process ID of a given process
 * @param processId The process ID of the target process
 * @return The parent process ID if found, otherwise -1
 */
auto getParentProcessId(int processId) -> int;

/**
 * @brief Creates a process as a specified user
 * @param command The command to be executed by the new process
 * @param username The username of the user account
 * @param domain The domain of the user account
 * @param password The password of the user account
 * @return True if the process is created successfully, otherwise false
 */
auto createProcessAsUser(const std::string &command,
                         const std::string &username, const std::string &domain,
                         const std::string &password) -> bool;

/**
 * @brief Gets the process IDs of processes with the specified name
 * @param processName The name of the process
 * @return A vector of process IDs
 */
auto getProcessIdByName(const std::string &processName) -> std::vector<int>;

/**
 * @brief Gets process CPU usage percentage
 * @param pid Process ID
 * @return CPU usage percentage, returns -1 if process doesn't exist
 */
auto getProcessCpuUsage(int pid) -> double;

/**
 * @brief Gets process memory usage
 * @param pid Process ID
 * @return Memory usage in bytes, returns 0 if process doesn't exist
 */
auto getProcessMemoryUsage(int pid) -> std::size_t;

/**
 * @brief Sets process priority
 * @param pid Process ID
 * @param priority Process priority level
 * @return True if successfully set
 */
auto setProcessPriority(int pid, ProcessPriority priority) -> bool;

/**
 * @brief Gets process priority
 * @param pid Process ID
 * @return Process priority, returns std::nullopt if process doesn't exist
 */
auto getProcessPriority(int pid) -> std::optional<ProcessPriority>;

/**
 * @brief Gets list of child processes
 * @param pid Parent process ID
 * @return List of child process IDs
 */
auto getChildProcesses(int pid) -> std::vector<int>;

/**
 * @brief Gets process start time
 * @param pid Process ID
 * @return Process start time point, returns std::nullopt if process doesn't
 * exist
 */
auto getProcessStartTime(int pid)
    -> std::optional<std::chrono::system_clock::time_point>;

/**
 * @brief Gets process running time in seconds
 * @param pid Process ID
 * @return Process running time in seconds, returns -1 if process doesn't exist
 */
auto getProcessRunningTime(int pid) -> long;

/**
 * @brief Monitors process and executes callback when process status changes
 * @param pid Process ID
 * @param callback Callback function for status changes
 * @param intervalMs Check interval in milliseconds
 * @return Monitor task ID, can be used to stop monitoring
 */
auto monitorProcess(int pid,
                    std::function<void(int, const std::string &)> callback,
                    unsigned int intervalMs = 1000) -> int;

/**
 * @brief Stops process monitoring
 * @param monitorId Monitor task ID
 * @return True if successfully stopped
 */
auto stopMonitoring(int monitorId) -> bool;

/**
 * @brief Gets process command line arguments
 * @param pid Process ID
 * @return Vector of command line arguments
 */
auto getProcessCommandLine(int pid) -> std::vector<std::string>;

/**
 * @brief Gets process environment variables
 * @param pid Process ID
 * @return Map of environment variable key-value pairs
 */
auto getProcessEnvironment(int pid)
    -> std::unordered_map<std::string, std::string>;

/**
 * @brief Gets process resource usage
 * @param pid Process ID
 * @return Process resource usage information
 */
auto getProcessResources(int pid) -> ProcessResource;

#ifdef _WIN32
/**
 * @brief Gets Windows process privileges
 * @param pid Process ID
 * @return Process privileges information
 */
auto getWindowsPrivileges(int pid) -> PrivilegesInfo;

/**
 * @brief Gets list of modules loaded by process (Windows)
 * @param pid Process ID
 * @return List of module paths
 */
auto getProcessModules(int pid) -> std::vector<std::string>;
#endif

#ifdef __linux__
/**
 * @brief Gets Linux process capabilities
 * @param pid Process ID
 * @return List of process capabilities
 */
auto getProcessCapabilities(int pid) -> std::vector<std::string>;
#endif

/**
 * @brief Suspends a process
 * @param pid Process ID
 * @return True if successfully suspended
 */
auto suspendProcess(int pid) -> bool;

/**
 * @brief Resumes a suspended process
 * @param pid Process ID
 * @return True if successfully resumed
 */
auto resumeProcess(int pid) -> bool;

/**
 * @brief Sets process CPU affinity (binds to specified CPU cores)
 * @param pid Process ID
 * @param cpuIndices List of CPU core indices
 * @return True if successfully set
 */
auto setProcessAffinity(int pid, const std::vector<int> &cpuIndices) -> bool;

/**
 * @brief Gets process CPU affinity
 * @param pid Process ID
 * @return List of CPU core indices, returns empty vector if failed
 */
auto getProcessAffinity(int pid) -> std::vector<int>;

/**
 * @brief Sets process memory usage limit
 * @param pid Process ID
 * @param limitBytes Maximum memory usage in bytes
 * @return True if successfully set
 */
auto setProcessMemoryLimit(int pid, std::size_t limitBytes) -> bool;

/**
 * @brief Gets process executable path
 * @param pid Process ID
 * @return Full path to process executable
 */
auto getProcessPath(int pid) -> std::string;

/**
 * @brief Monitors process based on resource usage
 * @param pid Process ID
 * @param resourceType Resource type (cpu, memory, disk, network)
 * @param threshold Threshold (percentage for CPU, bytes for memory)
 * @param callback Callback function triggered when threshold is reached
 * @param intervalMs Check interval in milliseconds
 * @return Monitor task ID
 */
auto monitorProcessResource(
    int pid, const std::string &resourceType, double threshold,
    std::function<void(int, const std::string &, double)> callback,
    unsigned int intervalMs = 1000) -> int;

/**
 * @brief Gets process system call statistics
 * @param pid Process ID
 * @return System call statistics
 */
auto getProcessSyscalls(int pid)
    -> std::unordered_map<std::string, unsigned long>;

/**
 * @brief Gets process network connection information
 * @param pid Process ID
 * @return List of network connection information
 */
auto getProcessNetworkConnections(int pid) -> std::vector<NetworkConnection>;

/**
 * @brief Gets process file descriptor/handle information
 * @param pid Process ID
 * @return List of file descriptor/handle information
 */
auto getProcessFileDescriptors(int pid) -> std::vector<FileDescriptor>;

/**
 * @brief Gets process performance history for specified time period
 * @param pid Process ID
 * @param duration History duration
 * @param intervalMs Sampling interval in milliseconds
 * @return List of performance history data points
 */
auto getProcessPerformanceHistory(int pid, std::chrono::seconds duration,
                                  unsigned int intervalMs = 1000)
    -> PerformanceHistory;

/**
 * @brief Sets process IO priority
 * @param pid Process ID
 * @param priority IO priority (0-7, lower value means higher priority)
 * @return True if successfully set
 */
auto setProcessIOPriority(int pid, int priority) -> bool;

/**
 * @brief Gets process IO priority
 * @param pid Process ID
 * @return IO priority, returns -1 if failed
 */
auto getProcessIOPriority(int pid) -> int;

/**
 * @brief Sends signal to process
 * @param pid Process ID
 * @param signal Signal value
 * @return True if successfully sent
 */
auto sendSignalToProcess(int pid, int signal) -> bool;

/**
 * @brief Finds processes matching specified criteria
 * @param predicate Predicate function that receives Process object and returns
 * whether it matches criteria
 * @return List of matching process IDs
 */
auto findProcesses(std::function<bool(const Process &)> predicate)
    -> std::vector<int>;

}  // namespace atom::system

#endif
