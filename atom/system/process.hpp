#ifndef ATOM_SYSTEM_PROCESS_HPP
#define ATOM_SYSTEM_PROCESS_HPP

#include <chrono>
#include <functional>
#include <optional>
#include "process_info.hpp"

namespace atom::system {
/**
 * @brief Gets information about all processes.
 * @return A vector of pairs containing process IDs and names.
 */
auto getAllProcesses() -> std::vector<std::pair<int, std::string>>;

/**
 * @brief Gets information about a process by its PID.
 * @param pid The process ID.
 * @return A Process struct containing information about the process.
 */
[[nodiscard("The process info is not used")]]
auto getProcessInfoByPid(int pid) -> Process;

/**
 * @brief Gets information about the current process.
 * @return A Process struct containing information about the current process.
 */
[[nodiscard("The process info is not used")]] auto getSelfProcessInfo()
    -> Process;

/**
 * @brief Returns the name of the controlling terminal.
 *
 * This function returns the name of the controlling terminal associated with
 * the current process.
 *
 * @return The name of the controlling terminal.
 */
[[nodiscard]] auto ctermid() -> std::string;

/**
 * @brief Checks if a process is running by its name.
 *
 * This function checks if a process with the specified name is currently
 * running.
 *
 * @param processName The name of the process to check.
 * @return bool True if the process is running, otherwise false.
 */
auto isProcessRunning(const std::string &processName) -> bool;

/**
 * @brief Returns the parent process ID of a given process.
 *
 * This function retrieves the parent process ID (PPID) of a specified process.
 * If the process is not found or an error occurs, the function returns -1.
 *
 * @param processId The process ID of the target process.
 * @return int The parent process ID if found, otherwise -1.
 */
auto getParentProcessId(int processId) -> int;

/**
 * @brief Creates a process as a specified user.
 *
 * This function creates a new process using the specified user credentials.
 * It logs in the user, duplicates the user token, and creates a new process
 * with the specified command. This function is only available on Windows.
 *
 * @param command The command to be executed by the new process.
 * @param username The username of the user account.
 * @param domain The domain of the user account.
 * @param password The password of the user account.
 * @return bool True if the process is created successfully, otherwise false.
 */
auto createProcessAsUser(const std::string &command,
                         const std::string &username, const std::string &domain,
                         const std::string &password) -> bool;

/**
 * @brief Gets the process IDs of processes with the specified name.
 * @param processName The name of the process.
 * @return A vector of process IDs.
 */
auto getProcessIdByName(const std::string &processName) -> std::vector<int>;

/**
 * @brief 获取进程的CPU使用率
 * @param pid 进程ID
 * @return CPU使用率百分比，如果进程不存在则返回-1
 */
auto getProcessCpuUsage(int pid) -> double;

/**
 * @brief 获取进程的内存使用情况
 * @param pid 进程ID
 * @return 内存使用字节数，如果进程不存在则返回0
 */
auto getProcessMemoryUsage(int pid) -> std::size_t;

/**
 * @brief 设置进程优先级
 * @param pid 进程ID
 * @param priority 优先级
 * @return 是否设置成功
 */
auto setProcessPriority(int pid, ProcessPriority priority) -> bool;

/**
 * @brief 获取进程优先级
 * @param pid 进程ID
 * @return 进程优先级，如果进程不存在则返回std::nullopt
 */
auto getProcessPriority(int pid) -> std::optional<ProcessPriority>;

/**
 * @brief 获取进程的子进程列表
 * @param pid 父进程ID
 * @return 子进程ID列表
 */
auto getChildProcesses(int pid) -> std::vector<int>;

/**
 * @brief 获取进程的启动时间
 * @param pid 进程ID
 * @return 进程启动时间点，如果进程不存在则返回std::nullopt
 */
auto getProcessStartTime(int pid)
    -> std::optional<std::chrono::system_clock::time_point>;

/**
 * @brief 获取进程的运行时间（秒）
 * @param pid 进程ID
 * @return 进程运行时间（秒），如果进程不存在则返回-1
 */
auto getProcessRunningTime(int pid) -> long;

/**
 * @brief 监控进程并在进程状态变化时执行回调函数
 * @param pid 进程ID
 * @param callback 状态变化时的回调函数
 * @param intervalMs 检查间隔（毫秒）
 * @return 监控任务ID，可用于停止监控
 */
auto monitorProcess(int pid,
                    std::function<void(int, const std::string &)> callback,
                    unsigned int intervalMs = 1000) -> int;

/**
 * @brief 停止进程监控
 * @param monitorId 监控任务ID
 * @return 是否成功停止
 */
auto stopMonitoring(int monitorId) -> bool;

/**
 * @brief 获取进程的命令行参数
 * @param pid 进程ID
 * @return 命令行参数向量
 */
auto getProcessCommandLine(int pid) -> std::vector<std::string>;

/**
 * @brief 获取进程的环境变量
 * @param pid 进程ID
 * @return 环境变量的键值对映射
 */
auto getProcessEnvironment(int pid)
    -> std::unordered_map<std::string, std::string>;

/**
 * @brief 获取进程的资源使用情况
 * @param pid 进程ID
 * @return 资源使用情况
 */
auto getProcessResources(int pid) -> ProcessResource;

#ifdef _WIN32
auto getWindowsPrivileges(int pid) -> PrivilegesInfo;

/**
 * @brief 获取进程加载的模块列表（Windows）
 * @param pid 进程ID
 * @return 模块路径列表
 */
auto getProcessModules(int pid) -> std::vector<std::string>;
#endif

#ifdef __linux__
/**
 * @brief 获取Linux系统上的进程能力 (capabilities)
 * @param pid 进程ID
 * @return 进程能力列表
 */
auto getProcessCapabilities(int pid) -> std::vector<std::string>;
#endif

/**
 * @brief 挂起（暂停）一个进程
 * @param pid 进程ID
 * @return 是否成功挂起
 */
auto suspendProcess(int pid) -> bool;

/**
 * @brief 恢复一个被挂起的进程
 * @param pid 进程ID
 * @return 是否成功恢复
 */
auto resumeProcess(int pid) -> bool;

/**
 * @brief 设置进程的CPU亲和性（绑定到指定CPU核心）
 * @param pid 进程ID
 * @param cpuIndices CPU核心索引列表
 * @return 是否设置成功
 */
auto setProcessAffinity(int pid, const std::vector<int>& cpuIndices) -> bool;

/**
 * @brief 获取进程的CPU亲和性
 * @param pid 进程ID
 * @return CPU核心索引列表，如果失败则返回空vector
 */
auto getProcessAffinity(int pid) -> std::vector<int>;

/**
 * @brief 设置进程的内存使用限制
 * @param pid 进程ID
 * @param limitBytes 最大内存使用量（字节）
 * @return 是否设置成功
 */
auto setProcessMemoryLimit(int pid, std::size_t limitBytes) -> bool;

/**
 * @brief 获取进程的执行路径
 * @param pid 进程ID
 * @return 进程可执行文件的完整路径
 */
auto getProcessPath(int pid) -> std::string;

/**
 * @brief 基于资源使用情况监控进程
 * @param pid 进程ID
 * @param resourceType 资源类型 (cpu, memory, disk, network)
 * @param threshold 阈值（对CPU是百分比，对内存是字节数）
 * @param callback 阈值触发时的回调函数
 * @param intervalMs 检查间隔（毫秒）
 * @return 监控任务ID
 */
auto monitorProcessResource(int pid, const std::string& resourceType, 
                           double threshold,
                           std::function<void(int, const std::string&, double)> callback,
                           unsigned int intervalMs = 1000) -> int;

/**
 * @brief 获取进程的系统调用统计信息
 * @param pid 进程ID
 * @return 系统调用统计信息
 */
auto getProcessSyscalls(int pid) -> std::unordered_map<std::string, unsigned long>;

/**
 * @brief 获取进程的网络连接信息
 * @param pid 进程ID
 * @return 网络连接信息列表
 */
auto getProcessNetworkConnections(int pid) -> std::vector<NetworkConnection>;

/**
 * @brief 获取进程的文件描述符/句柄信息
 * @param pid 进程ID
 * @return 文件描述符/句柄信息列表
 */
auto getProcessFileDescriptors(int pid) -> std::vector<FileDescriptor>;

/**
 * @brief 获取指定时间段内的进程性能历史
 * @param pid 进程ID
 * @param duration 历史时长
 * @param intervalMs 采样间隔（毫秒）
 * @return 性能历史数据点列表
 */
auto getProcessPerformanceHistory(int pid, 
                                 std::chrono::seconds duration,
                                 unsigned int intervalMs = 1000) -> PerformanceHistory;

/**
 * @brief 设置进程的IO优先级
 * @param pid 进程ID
 * @param priority IO优先级(0-7,值越小优先级越高)
 * @return 是否设置成功
 */
auto setProcessIOPriority(int pid, int priority) -> bool;

/**
 * @brief 获取进程的IO优先级
 * @param pid 进程ID
 * @return IO优先级，如果失败返回-1
 */
auto getProcessIOPriority(int pid) -> int;

/**
 * @brief 向进程发送信号
 * @param pid 进程ID
 * @param signal 信号值
 * @return 是否发送成功
 */
auto sendSignalToProcess(int pid, int signal) -> bool;

/**
 * @brief 查找符合指定条件的进程
 * @param predicate 判断函数，接收Process对象，返回是否符合条件
 * @return 符合条件的进程ID列表
 */
auto findProcesses(std::function<bool(const Process&)> predicate) -> std::vector<int>;

}  // namespace atom::system

#endif
