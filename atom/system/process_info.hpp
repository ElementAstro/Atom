#ifndef ATOM_SYSTEM_PROCESS_INFO_HPP
#define ATOM_SYSTEM_PROCESS_INFO_HPP

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/macro.hpp"

namespace fs = std::filesystem;

namespace atom::system {
/**
 * @struct ProcessResource
 * @brief 表示进程使用的资源信息
 */
struct ProcessResource {
    double cpuUsage;        ///< CPU使用率（百分比）
    std::size_t memUsage;   ///< 内存使用量（字节）
    std::size_t vmUsage;    ///< 虚拟内存使用量（字节）
    std::size_t ioRead;     ///< IO读取字节数
    std::size_t ioWrite;    ///< IO写入字节数
    int threadCount;        ///< 线程数量
    std::size_t openFiles;  ///< 打开的文件数量
} ATOM_ALIGNAS(64);

/**
 * @struct Process
 * @brief Represents a system process with detailed information.
 */
struct Process {
    int pid;               ///< 进程ID
    int ppid;              ///< 父进程ID
    std::string name;      ///< 进程名称
    std::string command;   ///< 启动进程的命令
    std::string output;    ///< 进程输出
    fs::path path;         ///< 进程可执行文件的路径
    std::string status;    ///< 进程状态
    std::string username;  ///< 进程所有者用户名
    int priority;          ///< 进程优先级

    std::chrono::system_clock::time_point startTime;  ///< 进程启动时间
    ProcessResource resources;  ///< 进程资源使用情况

    std::unordered_map<std::string, std::string> environment;  ///< 进程环境变量

#if defined(_WIN32)
    void *handle;  ///< Handle to the process (Windows only).
    std::vector<std::string> modules;  ///< 加载的模块列表
#endif
    bool isBackground;  ///< 指示进程是否在后台运行
} ATOM_ALIGNAS(128);

/**
 * @enum ProcessPriority
 * @brief 进程优先级枚举
 */
enum class ProcessPriority {
    IDLE,     ///< 空闲优先级
    LOW,      ///< 低优先级
    NORMAL,   ///< 正常优先级
    HIGH,     ///< 高优先级
    REALTIME  ///< 实时优先级
};

/**
 * @struct PrivilegesInfo
 * @brief Contains privileges information of a user.
 */
struct PrivilegesInfo {
    std::string username;                 ///< 用户名
    std::string groupname;                ///< 组名
    std::vector<std::string> privileges;  ///< 权限列表
    bool isAdmin;                         ///< 是否为管理员
    std::vector<std::string> groups;      ///< 用户所属组列表
} ATOM_ALIGNAS(128);

/**
 * @struct NetworkConnection
 * @brief 表示进程的网络连接信息
 */
struct NetworkConnection {
    std::string protocol;           ///< 协议(TCP/UDP)
    std::string localAddress;       ///< 本地地址
    int localPort;                  ///< 本地端口
    std::string remoteAddress;      ///< 远程地址
    int remotePort;                 ///< 远程端口
    std::string status;             ///< 连接状态
} ATOM_ALIGNAS(64);

/**
 * @struct FileDescriptor
 * @brief 表示进程打开的文件描述符或句柄
 */
struct FileDescriptor {
    int fd;                         ///< 文件描述符/句柄ID
    std::string path;               ///< 文件路径
    std::string type;               ///< 文件类型(regular, socket, pipe等)
    std::string mode;               ///< 访问模式(r, w, rw等)
} ATOM_ALIGNAS(64);

/**
 * @struct PerformanceDataPoint
 * @brief 表示进程在特定时间点的性能数据
 */
struct PerformanceDataPoint {
    std::chrono::system_clock::time_point timestamp;  ///< 时间戳
    double cpuUsage;                ///< CPU使用率
    std::size_t memoryUsage;        ///< 内存使用量
    std::size_t ioReadBytes;        ///< IO读取字节数
    std::size_t ioWriteBytes;       ///< IO写入字节数
} ATOM_ALIGNAS(64);

/**
 * @struct PerformanceHistory
 * @brief 表示进程的性能历史数据
 */
struct PerformanceHistory {
    int pid;                        ///< 进程ID
    std::vector<PerformanceDataPoint> dataPoints;  ///< 性能数据点列表
} ATOM_ALIGNAS(64);

}  // namespace atom::system

#endif
