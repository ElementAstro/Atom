/*
 * daemon.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-11

Description: Daemon process implementation for Linux and Windows. But there is
still some problems on Windows, especially the console.

**************************************************/

#include "daemon.hpp"

#include <fstream>
#include <functional>
#include <mutex>
#include <ostream>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <TlHelp32.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "atom/log/loguru.hpp"
#include "atom/utils/time.hpp"

namespace {
int g_daemon_restart_interval = 10;
std::filesystem::path g_pid_file_path = "lithium-daemon";
std::mutex g_daemon_mutex;
std::atomic<bool> g_is_daemon{false};

// 进程清理管理器 - 确保在程序退出时移除PID文件
class ProcessCleanupManager {
public:
    static void registerPidFile(const std::filesystem::path& path) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_pidFiles.push_back(path);
    }

    static void cleanup() noexcept {
        std::lock_guard<std::mutex> lock(s_mutex);
        for (const auto& path : s_pidFiles) {
            try {
                if (std::filesystem::exists(path)) {
                    std::filesystem::remove(path);
                }
            } catch (...) {
                // 在析构函数中不抛出异常
            }
        }
        s_pidFiles.clear();
    }

private:
    static std::mutex s_mutex;
    static std::vector<std::filesystem::path> s_pidFiles;
};

std::mutex ProcessCleanupManager::s_mutex;
std::vector<std::filesystem::path> ProcessCleanupManager::s_pidFiles;
}  // namespace

namespace atom::async {

// 实现 DaemonGuard 析构函数
DaemonGuard::~DaemonGuard() noexcept {
    if (m_pidFilePath.has_value()) {
        try {
            if (std::filesystem::exists(*m_pidFilePath)) {
                std::filesystem::remove(*m_pidFilePath);
            }
        } catch (...) {
            // 析构函数中不抛出异常
        }
    }
}

auto DaemonGuard::toString() const noexcept -> std::string {
    try {
        std::stringstream stringStream;
        stringStream << "[DaemonGuard parentId=" << m_parentId
                     << " mainId=" << m_mainId << " parentStartTime="
                     << utils::timeStampToString(m_parentStartTime)
                     << " mainStartTime="
                     << utils::timeStampToString(m_mainStartTime)
                     << " restartCount="
                     << m_restartCount.load(std::memory_order_relaxed) << "]";
        return stringStream.str();
    } catch (...) {
        return "[DaemonGuard toString() error]";
    }
}

template <ProcessCallback Callback>
auto DaemonGuard::realStart(int argc, char** argv,
                            const Callback& mainCb) -> int {
    try {
        if (argv == nullptr) {
            throw DaemonException("Invalid argument vector (nullptr)");
        }

#ifdef _WIN32
        m_mainId = reinterpret_cast<HANDLE>(
            static_cast<intptr_t>(GetCurrentProcessId()));
#else
        m_mainId = getpid();
#endif
        m_mainStartTime = time(nullptr);

        if (m_pidFilePath.has_value()) {
            try {
                writePidFile(*m_pidFilePath);
                ProcessCleanupManager::registerPidFile(*m_pidFilePath);
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to write PID file: {}", e.what());
                // 继续执行，不要因为PID文件失败而终止程序
            }
        }

        return mainCb(argc, argv);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in realStart: {}", e.what());
        return -1;
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in realStart");
        return -1;
    }
}

template <ProcessCallback Callback>
auto DaemonGuard::realDaemon(int argc, char** argv,
                             const Callback& mainCb) -> int {
    try {
        if (argv == nullptr) {
            throw DaemonException("Invalid argument vector (nullptr)");
        }

#ifdef _WIN32
        // 在 Windows 平台下模拟守护进程
        if (!FreeConsole()) {
            LOG_F(WARNING, "Failed to free console, error: {}", GetLastError());
        }

        m_parentId = reinterpret_cast<HANDLE>(
            static_cast<intptr_t>(GetCurrentProcessId()));
        m_parentStartTime = time(nullptr);

        while (true) {
            PROCESS_INFORMATION processInfo;
            STARTUPINFO startupInfo;
            ZeroMemory(&processInfo, sizeof(processInfo));
            ZeroMemory(&startupInfo, sizeof(startupInfo));
            startupInfo.cb = sizeof(startupInfo);

            std::unique_ptr<char[]> cmdLine(new char[MAX_PATH]);
            if (strncpy_s(cmdLine.get(), MAX_PATH, argv[0], strlen(argv[0])) !=
                0) {
                LOG_F(ERROR, "Failed to copy command line");
                return -1;
            }

            if (!CreateProcess(nullptr, cmdLine.get(), nullptr, nullptr, FALSE,
                               CREATE_NEW_CONSOLE, nullptr, nullptr,
                               &startupInfo, &processInfo)) {
                LOG_F(ERROR, "Create process failed with error code {}",
                      GetLastError());
                return -1;
            }

            // 等待子进程结束
            WaitForSingleObject(processInfo.hProcess, INFINITE);

            DWORD exitCode = 0;
            if (!GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
                LOG_F(ERROR, "Failed to get exit code, error: {}",
                      GetLastError());
            }

            CloseHandle(processInfo.hProcess);
            CloseHandle(processInfo.hThread);

            // 检查退出代码
            if (exitCode == 0) {
                LOG_F(INFO, "Child process exited normally");
                break;
            } else if (exitCode == 9) {  // SIGKILL
                LOG_F(INFO, "Child process was killed");
                break;
            }

            // 等待一段时间后重新启动子进程
            m_restartCount.fetch_add(1, std::memory_order_relaxed);
            LOG_F(INFO, "Restarting child process (attempt {})",
                  m_restartCount.load());
            Sleep(getDaemonRestartInterval() * 1000);
        }
#else
        // 确保文件描述符不会耗尽
        struct rlimit rl;
        if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
            // 设置为最大可用文件描述符
            rl.rlim_cur = rl.rlim_max;
            setrlimit(RLIMIT_NOFILE, &rl);
        }

        // 创建守护进程
        pid_t pid = fork();
        if (pid < 0) {
            throw DaemonException(std::string("Failed to fork: ") +
                                  strerror(errno));
        }

        if (pid > 0) {  // 父进程退出
            exit(EXIT_SUCCESS);
        }

        // 创建新会话
        if (setsid() < 0) {
            throw DaemonException(std::string("Failed to setsid: ") +
                                  strerror(errno));
        }

        // 忽略终端I/O信号和SIGHUP
        signal(SIGCHLD, SIG_IGN);
        signal(SIGHUP, SIG_IGN);

        // 确保不会成为会话领导者
        pid = fork();
        if (pid < 0) {
            throw DaemonException(std::string("Second fork failed: ") +
                                  strerror(errno));
        }

        if (pid > 0) {  // 第一个子进程退出
            exit(EXIT_SUCCESS);
        }

        // 更改工作目录为根目录
        if (chdir("/") < 0) {
            LOG_F(WARNING, "Failed to change directory: {}", strerror(errno));
        }

        // 关闭所有打开的文件描述符
        for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
            close(x);
        }

        // 重新打开标准输入/输出/错误到/dev/null
        int fd = open("/dev/null", O_RDWR);
        if (fd != -1) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO) {
                close(fd);
            }
        }

        m_parentId = getpid();
        m_parentStartTime = time(nullptr);

        while (true) {
            pid_t child_pid = fork();  // 创建子进程
            if (child_pid == 0) {      // 子进程
                m_mainId = getpid();
                m_mainStartTime = time(nullptr);
                LOG_F(INFO, "daemon process start pid={}", getpid());
                return realStart(argc, argv, mainCb);
            }

            if (child_pid < 0) {  // 创建子进程失败
                LOG_F(ERROR, "fork fail return={} errno={} errstr={}",
                      child_pid, errno, strerror(errno));
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;  // 尝试再次创建
            }

            // 父进程
            int status = 0;
            waitpid(child_pid, &status, 0);  // 等待子进程退出

            // 子进程异常退出
            if (status != 0) {
                if (WIFEXITED(status) &&
                    WEXITSTATUS(status) ==
                        9) {  // SIGKILL 信号杀死子进程，不需要重新启动
                    LOG_F(INFO, "daemon process killed pid={}", getpid());
                    break;
                }

                // 记录日志并重新启动子进程
                if (WIFEXITED(status)) {
                    LOG_F(ERROR, "child exited with status {} pid={}",
                          WEXITSTATUS(status), child_pid);
                } else if (WIFSIGNALED(status)) {
                    LOG_F(ERROR, "child killed by signal {} pid={}",
                          WTERMSIG(status), child_pid);
                } else {
                    LOG_F(ERROR, "child crashed with unknown status {} pid={}",
                          status, child_pid);
                }
            } else {  // 正常退出，直接退出程序
                LOG_F(INFO, "daemon process exit normally pid={}", getpid());
                break;
            }

            // 等待一段时间后重新启动子进程
            m_restartCount.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(
                std::chrono::seconds(getDaemonRestartInterval()));
        }
#endif
        return 0;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in realDaemon: {}", e.what());
        return -1;
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in realDaemon");
        return -1;
    }
}

template <ProcessCallback Callback>
auto DaemonGuard::startDaemon(int argc, char** argv, const Callback& mainCb,
                              bool isDaemon) -> int {
    try {
        if (argv == nullptr) {
            throw DaemonException("Invalid argument vector (nullptr)");
        }

        // 检查参数边界
        if (argc < 0) {
            LOG_F(WARNING, "Invalid argc value: {}, using 0 instead", argc);
            argc = 0;
        }

        std::atomic_store_explicit(&g_is_daemon, isDaemon,
                                   std::memory_order_relaxed);

#ifdef _WIN32
        if (isDaemon) {
            if (!AllocConsole()) {
                LOG_F(WARNING, "Failed to allocate console, error: {}",
                      GetLastError());
            }

            FILE* fpstdout = nullptr;
            FILE* fpstderr = nullptr;

            if (freopen_s(&fpstdout, "CONOUT$", "w", stdout) != 0) {
                LOG_F(ERROR, "Failed to redirect stdout");
                return -1;
            }

            if (freopen_s(&fpstderr, "CONOUT$", "w", stderr) != 0) {
                LOG_F(ERROR, "Failed to redirect stderr");
                return -1;
            }
        }
#endif

        if (!isDaemon) {  // 不需要创建守护进程
#ifdef _WIN32
            m_parentId = reinterpret_cast<HANDLE>(
                static_cast<intptr_t>(GetCurrentProcessId()));
#else
            m_parentId = getpid();
#endif
            m_parentStartTime = time(nullptr);
            m_pidFilePath = g_pid_file_path;
            return realStart(argc, argv, mainCb);
        }

        // 创建守护进程
        m_pidFilePath = g_pid_file_path;
        return realDaemon(argc, argv, mainCb);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in startDaemon: {}", e.what());
        return -1;
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in startDaemon");
        return -1;
    }
}

auto DaemonGuard::isRunning() const noexcept -> bool {
#ifdef _WIN32
    if (m_mainId == 0)
        return false;

    DWORD exitCode = 0;
    HANDLE hProcess =
        OpenProcess(PROCESS_QUERY_INFORMATION, FALSE,
                    static_cast<DWORD>(reinterpret_cast<uintptr_t>(m_mainId)));
    if (hProcess == NULL)
        return false;

    BOOL result = GetExitCodeProcess(hProcess, &exitCode);
    CloseHandle(hProcess);

    return result && exitCode == STILL_ACTIVE;
#else
    if (m_mainId <= 0)
        return false;

    // 发送信号0来检测进程是否存在
    return kill(m_mainId, 0) == 0;
#endif
}

void signalHandler(int signum) noexcept {
    try {
        if (signum == SIGTERM || signum == SIGINT) {
            ProcessCleanupManager::cleanup();

            // 使用标志确保只记录一次
            static std::atomic<bool> handlingSignal{false};
            bool expected = false;
            if (handlingSignal.compare_exchange_strong(expected, true)) {
                LOG_F(INFO, "Received signal {} ({}), shutting down...", signum,
                      signum == SIGTERM ? "SIGTERM" : "SIGINT");
            }

            exit(0);
        }
    } catch (...) {
        // 信号处理程序中不应抛出异常
    }
}

void writePidFile(const std::filesystem::path& filePath) {
    try {
        std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
        if (!ofs) {
            throw std::filesystem::filesystem_error(
                "Failed to open PID file", filePath,
                std::make_error_code(std::errc::permission_denied));
        }

#ifdef _WIN32
        ofs << GetCurrentProcessId();
#else
        ofs << getpid();
#endif

        ofs.close();
        if (ofs.fail()) {
            throw std::filesystem::filesystem_error(
                "Failed to close PID file", filePath,
                std::make_error_code(std::errc::io_error));
        }
    } catch (const std::filesystem::filesystem_error& e) {
        throw;
    } catch (const std::exception& e) {
        throw std::filesystem::filesystem_error(
            std::string("Failed to write PID file: ") + e.what(), filePath,
            std::make_error_code(std::errc::io_error));
    }
}

auto checkPidFile(const std::filesystem::path& filePath) noexcept -> bool {
    try {
#ifdef _WIN32
        // Windows 平台检查文件是否存在
        if (!std::filesystem::exists(filePath)) {
            return false;
        }

        // 读取PID文件
        std::ifstream ifs(filePath);
        if (!ifs) {
            return false;
        }

        DWORD pid = 0;
        ifs >> pid;
        ifs.close();

        if (pid == 0) {
            return false;
        }

        // 检查进程是否存在
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (hProcess == NULL) {
            return false;
        }

        DWORD exitCode = 0;
        BOOL result = GetExitCodeProcess(hProcess, &exitCode);
        CloseHandle(hProcess);

        return result && exitCode == STILL_ACTIVE;
#else
        // 检查文件是否存在
        struct stat st {};
        if (stat(filePath.c_str(), &st) != 0) {
            return false;
        }

        // 读取PID
        std::ifstream ifs(filePath);
        if (!ifs) {
            return false;
        }

        pid_t pid = -1;
        ifs >> pid;
        ifs.close();

        if (pid <= 0) {
            return false;
        }

        // 检查进程是否存在
        return kill(pid, 0) == 0 || errno != ESRCH;
#endif
    } catch (...) {
        return false;
    }
}

void setDaemonRestartInterval(int seconds) {
    if (seconds <= 0) {
        throw std::invalid_argument(
            "Restart interval must be greater than zero");
    }

    std::lock_guard<std::mutex> lock(g_daemon_mutex);
    g_daemon_restart_interval = seconds;
}

int getDaemonRestartInterval() noexcept {
    std::lock_guard<std::mutex> lock(g_daemon_mutex);
    return g_daemon_restart_interval;
}

template auto DaemonGuard::realStart<std::function<int(int, char**)>>(
    int, char**, const std::function<int(int, char**)>&) -> int;
template auto DaemonGuard::realDaemon<std::function<int(int, char**)>>(
    int, char**, const std::function<int(int, char**)>&) -> int;
template auto DaemonGuard::startDaemon<std::function<int(int, char**)>>(
    int, char**, const std::function<int(int, char**)>&, bool) -> int;

}  // namespace atom::async
