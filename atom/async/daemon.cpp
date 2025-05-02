/*
 * daemon.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-11

Description: Daemon process implementation for Linux, macOS and Windows. But
there is still some problems on Windows, especially the console.

**************************************************/

#include "daemon.hpp"

#include <format>  // C++20 standard formatting library
#include <fstream>
#include <functional>
#include <mutex>
#include <ostream>
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

#ifdef __APPLE__
#include <libproc.h>     // macOS process management
#include <sys/sysctl.h>  // macOS system control
#endif

#include "atom/log/loguru.hpp"
#include "atom/utils/time.hpp"

namespace {
int g_daemon_restart_interval = 10;
std::filesystem::path g_pid_file_path = "lithium-daemon";
std::mutex g_daemon_mutex;
std::atomic<bool> g_is_daemon{false};

// Process cleanup manager - ensures PID file removal on program exit
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
                    LOG_F(INFO, "PID file removed: {}", path.string());
                }
            } catch (...) {
                // Don't throw exceptions in destructors
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

// Platform-specific process utilities
#ifdef _WIN32
// Windows platform - get process command line
// Mark as [[maybe_unused]] to avoid compiler warnings
[[maybe_unused]] auto getProcessCommandLine(DWORD pid)
    -> std::optional<std::string> {
    try {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            return std::nullopt;
        }

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (!Process32First(hSnapshot, &pe32)) {
            CloseHandle(hSnapshot);
            return std::nullopt;
        }

        do {
            if (pe32.th32ProcessID == pid) {
                CloseHandle(hSnapshot);

                // Get command line
                HANDLE hProcess = OpenProcess(
                    PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                if (hProcess) {
                    std::string result(MAX_PATH, '\0');
                    // Removed unused variable 'size'

                    // Try to get command line
                    // In a real implementation, this part would be more
                    // complex, requiring WMI or PEB methods Simplified handling
                    // here, only returning the process name
                    result = pe32.szExeFile;
                    CloseHandle(hProcess);
                    return result;
                }
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));

        CloseHandle(hSnapshot);
    } catch (...) {
        // Catch all exceptions
    }
    return std::nullopt;
}
#elif defined(__APPLE__)
// macOS platform - get process command line
[[maybe_unused]] auto getProcessCommandLine(pid_t pid)
    -> std::optional<std::string> {
    try {
        char pathBuffer[PROC_PIDPATHINFO_MAXSIZE];  // Using C-style array
                                                    // instead of std::array
        if (proc_pidpath(pid, pathBuffer, sizeof(pathBuffer)) <= 0) {
            return std::nullopt;
        }
        return std::string(pathBuffer);
    } catch (...) {
        // Catch all exceptions
    }
    return std::nullopt;
}
#else
// Linux platform - get process command line
[[maybe_unused]] auto getProcessCommandLine(pid_t pid)
    -> std::optional<std::string> {
    try {
        std::filesystem::path cmdlinePath =
            std::format("/proc/{}/cmdline", pid);
        if (!std::filesystem::exists(cmdlinePath)) {
            return std::nullopt;
        }

        std::ifstream ifs(cmdlinePath);
        if (!ifs) {
            return std::nullopt;
        }

        std::string cmdline;
        std::getline(ifs, cmdline);

        // Handle null characters in cmdline
        for (auto& c : cmdline) {
            if (c == '\0')
                c = ' ';
        }

        return cmdline;
    } catch (...) {
        // Catch all exceptions
    }
    return std::nullopt;
}
#endif

}  // namespace

namespace atom::async {

// Implement DaemonGuard destructor
DaemonGuard::~DaemonGuard() noexcept {
    if (m_pidFilePath.has_value()) {
        try {
            if (std::filesystem::exists(*m_pidFilePath)) {
                std::filesystem::remove(*m_pidFilePath);
                LOG_F(INFO, "Removed PID file: {}", m_pidFilePath->string());
            }
        } catch (...) {
            // Don't throw exceptions in destructors
        }
    }
}

auto DaemonGuard::toString() const noexcept -> std::string {
    try {
        // Using std::format (C++20) instead of stringstream
        return std::format(
            "[DaemonGuard parentId={} mainId={} parentStartTime={} "
            "mainStartTime={} restartCount={}]",
            m_parentId.id, m_mainId.id,
            utils::timeStampToString(m_parentStartTime),
            utils::timeStampToString(m_mainStartTime),
            m_restartCount.load(std::memory_order_relaxed));
    } catch (...) {
        return "[DaemonGuard toString() error]";
    }
}

template <ProcessCallback Callback>
auto DaemonGuard::realStart(int argc, char** argv, const Callback& mainCb)
    -> int {
    try {
        if (argv == nullptr) {
            throw DaemonException("Invalid argument vector (nullptr)");
        }

        // Get current process ID
        m_mainId = ProcessId::current();
        m_mainStartTime = time(nullptr);

        if (m_pidFilePath.has_value()) {
            try {
                writePidFile(*m_pidFilePath);
                ProcessCleanupManager::registerPidFile(*m_pidFilePath);
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to write PID file: {}", e.what());
                // Continue execution, don't terminate due to PID file failure
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

// Implement modern interface version
template <ModernProcessCallback Callback>
auto DaemonGuard::realStartModern(std::span<char*> args, const Callback& mainCb)
    -> int {
    try {
        // Get current process ID
        m_mainId = ProcessId::current();
        m_mainStartTime = time(nullptr);

        if (m_pidFilePath.has_value()) {
            try {
                writePidFile(*m_pidFilePath);
                ProcessCleanupManager::registerPidFile(*m_pidFilePath);
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to write PID file: {}", e.what());
                // Continue execution, don't terminate due to PID file failure
            }
        }

        return mainCb(args);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in realStartModern: {}", e.what());
        return -1;
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in realStartModern");
        return -1;
    }
}

template <ProcessCallback Callback>
auto DaemonGuard::realDaemon([[maybe_unused]] int argc, char** argv,
                             [[maybe_unused]] const Callback& __unused_mainCb)
    -> int {
    try {
        if (argv == nullptr) {
            throw DaemonException("Invalid argument vector (nullptr)");
        }

#ifdef _WIN32
        // Simulate daemon process on Windows platform
        if (!FreeConsole()) {
            LOG_F(WARNING, "Failed to free console, error: {}", GetLastError());
        }

        m_parentId.id = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE,
                                    GetCurrentProcessId());
        m_parentStartTime = time(nullptr);

        while (true) {
            PROCESS_INFORMATION processInfo;
            STARTUPINFO startupInfo;
            ZeroMemory(&processInfo, sizeof(processInfo));
            ZeroMemory(&startupInfo, sizeof(startupInfo));
            startupInfo.cb = sizeof(startupInfo);

            auto cmdLine = std::make_unique<char[]>(MAX_PATH);
            if (strncpy_s(cmdLine.get(), MAX_PATH, argv[0], strlen(argv[0])) !=
                0) {
                LOG_F(ERROR, "Failed to copy command line");
                return -1;
            }

            // Create child process
            if (!CreateProcess(nullptr, cmdLine.get(), nullptr, nullptr, FALSE,
                               CREATE_NEW_CONSOLE, nullptr, nullptr,
                               &startupInfo, &processInfo)) {
                LOG_F(ERROR, "Create process failed with error code {}",
                      GetLastError());
                return -1;
            }

            // Wait for child process to terminate
            WaitForSingleObject(processInfo.hProcess, INFINITE);

            DWORD exitCode = 0;
            if (!GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
                LOG_F(ERROR, "Failed to get exit code, error: {}",
                      GetLastError());
            }

            CloseHandle(processInfo.hProcess);
            CloseHandle(processInfo.hThread);

            // Check exit code
            if (exitCode == 0) {
                LOG_F(INFO, "Child process exited normally");
                break;
            } else if (exitCode == 9) {  // SIGKILL
                LOG_F(INFO, "Child process was killed");
                break;
            }

            // Wait before restarting child process
            m_restartCount.fetch_add(1, std::memory_order_relaxed);
            LOG_F(INFO, "Restarting child process (attempt {})",
                  m_restartCount.load());
            ::Sleep(getDaemonRestartInterval() * 1000);
        }
#elif defined(__APPLE__)
        // macOS platform daemon implementation
        // Ensure file descriptors aren't exhausted
        struct rlimit rl;
        if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
            // Set to maximum available file descriptors
            rl.rlim_cur = rl.rlim_max;
            setrlimit(RLIMIT_NOFILE, &rl);
        }

        // Create daemon process
        pid_t pid = fork();
        if (pid < 0) {
            throw DaemonException(
                std::format("Failed to fork: {}", strerror(errno)));
        }

        if (pid > 0) {  // Parent exits
            exit(EXIT_SUCCESS);
        }

        // Create new session
        if (setsid() < 0) {
            throw DaemonException(
                std::format("Failed to setsid: {}", strerror(errno)));
        }

        // Ignore terminal I/O signals and SIGHUP
        signal(SIGCHLD, SIG_IGN);
        signal(SIGHUP, SIG_IGN);

        // Ensure process doesn't become session leader
        pid = fork();
        if (pid < 0) {
            throw DaemonException(
                std::format("Second fork failed: {}", strerror(errno)));
        }

        if (pid > 0) {  // First child exits
            exit(EXIT_SUCCESS);
        }

        // Change working directory (use current directory on macOS, not root)
        const char* workDir = ".";
        if (chdir(workDir) < 0) {
            LOG_F(WARNING, "Failed to change directory: {}", strerror(errno));
        }

        // Close all open file descriptors
        for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
            close(x);
        }

        // Reopen standard input/output/error to /dev/null
        int fd = open("/dev/null", O_RDWR);
        if (fd != -1) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO) {
                close(fd);
            }
        }

        m_parentId.id = getpid();
        m_parentStartTime = time(nullptr);

        // Daemon main loop
        while (true) {
            pid_t child_pid = fork();  // Create child process
            if (child_pid == 0) {      // Child process
                m_mainId.id = getpid();
                m_mainStartTime = time(nullptr);
                LOG_F(INFO, "daemon process start pid={}", getpid());
                return realStart(0, argv, [](int, char**) { return 0; });
            }

            if (child_pid < 0) {  // Failed to create child process
                LOG_F(ERROR, "fork fail return={} errno={} errstr={}",
                      child_pid, errno, strerror(errno));
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;  // Try again
            }

            // Parent process
            int status = 0;
            waitpid(child_pid, &status, 0);  // Wait for child to exit

            // Child process abnormal exit
            if (status != 0) {
                if (WIFEXITED(status) &&
                    WEXITSTATUS(status) == 9) {  // SIGKILL signal killed child
                                                 // process, no need to restart
                    LOG_F(INFO, "daemon process killed pid={}", getpid());
                    break;
                }

                // Log and restart child process
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
            } else {  // Normal exit, exit program directly
                LOG_F(INFO, "daemon process exit normally pid={}", getpid());
                break;
            }

            // Wait before restarting child process
            m_restartCount.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(
                std::chrono::seconds(getDaemonRestartInterval()));
        }
#else
        // Linux platform daemon implementation
        // Ensure file descriptors aren't exhausted
        struct rlimit rl;
        if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
            // Set to maximum available file descriptors
            rl.rlim_cur = rl.rlim_max;
            setrlimit(RLIMIT_NOFILE, &rl);
        }

        // Create daemon process
        pid_t pid = fork();
        if (pid < 0) {
            throw DaemonException(
                std::format("Failed to fork: {}", strerror(errno)));
        }

        if (pid > 0) {  // Parent exits
            exit(EXIT_SUCCESS);
        }

        // Create new session
        if (setsid() < 0) {
            throw DaemonException(
                std::format("Failed to setsid: {}", strerror(errno)));
        }

        // Ignore terminal I/O signals and SIGHUP
        signal(SIGCHLD, SIG_IGN);
        signal(SIGHUP, SIG_IGN);

        // Ensure process doesn't become session leader
        pid = fork();
        if (pid < 0) {
            throw DaemonException(
                std::format("Second fork failed: {}", strerror(errno)));
        }

        if (pid > 0) {  // First child exits
            exit(EXIT_SUCCESS);
        }

        // Change working directory to root
        if (chdir("/") < 0) {
            LOG_F(WARNING, "Failed to change directory: {}", strerror(errno));
        }

        // Close all open file descriptors
        for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
            close(x);
        }

        // Reopen standard input/output/error to /dev/null
        int fd = open("/dev/null", O_RDWR);
        if (fd != -1) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO) {
                close(fd);
            }
        }

        m_parentId.id = getpid();
        m_parentStartTime = time(nullptr);

        while (true) {
            pid_t child_pid = fork();  // Create child process
            if (child_pid == 0) {      // Child process
                m_mainId.id = getpid();
                m_mainStartTime = time(nullptr);
                LOG_F(INFO, "daemon process start pid={}", getpid());
                return realStart(0, argv, [](int, char**) { return 0; });
            }

            if (child_pid < 0) {  // Failed to create child process
                LOG_F(ERROR, "fork fail return={} errno={} errstr={}",
                      child_pid, errno, strerror(errno));
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;  // Try again
            }

            // Parent process
            int status = 0;
            waitpid(child_pid, &status, 0);  // Wait for child to exit

            // Child process abnormal exit
            if (status != 0) {
                if (WIFEXITED(status) &&
                    WEXITSTATUS(status) == 9) {  // SIGKILL signal killed child
                                                 // process, no need to restart
                    LOG_F(INFO, "daemon process killed pid={}", getpid());
                    break;
                }

                // Log and restart child process
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
            } else {  // Normal exit, exit program directly
                LOG_F(INFO, "daemon process exit normally pid={}", getpid());
                break;
            }

            // Wait before restarting child process
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

// Implement modern interface version (using std::span)
template <ModernProcessCallback Callback>
auto DaemonGuard::realDaemonModern(
    std::span<char*> args, [[maybe_unused]] const Callback& __unused_mainCb)
    -> int {
    try {
        if (args.empty()) {
            throw DaemonException("Empty argument vector");
        }

#ifdef _WIN32
        // Simulate daemon process on Windows platform
        if (!FreeConsole()) {
            LOG_F(WARNING, "Failed to free console, error: {}", GetLastError());
        }

        m_parentId.id = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE,
                                    GetCurrentProcessId());
        m_parentStartTime = time(nullptr);

        while (true) {
            PROCESS_INFORMATION processInfo;
            STARTUPINFO startupInfo;
            ZeroMemory(&processInfo, sizeof(processInfo));
            ZeroMemory(&startupInfo, sizeof(startupInfo));
            startupInfo.cb = sizeof(startupInfo);

            auto cmdLine = std::make_unique<char[]>(MAX_PATH);
            if (strncpy_s(cmdLine.get(), MAX_PATH, args[0], strlen(args[0])) !=
                0) {
                LOG_F(ERROR, "Failed to copy command line");
                return -1;
            }

            // Create child process, using more modern CreateProcessW API
            // (requires Unicode strings)
            if (!CreateProcess(nullptr, cmdLine.get(), nullptr, nullptr, FALSE,
                               CREATE_NEW_CONSOLE, nullptr, nullptr,
                               &startupInfo, &processInfo)) {
                LOG_F(ERROR, "Create process failed with error code {}",
                      GetLastError());
                return -1;
            }

            // Wait for child process to terminate
            WaitForSingleObject(processInfo.hProcess, INFINITE);

            DWORD exitCode = 0;
            if (!GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
                LOG_F(ERROR, "Failed to get exit code, error: {}",
                      GetLastError());
            }

            CloseHandle(processInfo.hProcess);
            CloseHandle(processInfo.hThread);

            // Check exit code
            if (exitCode == 0) {
                LOG_F(INFO, "Child process exited normally");
                break;
            } else if (exitCode == 9) {  // SIGKILL
                LOG_F(INFO, "Child process was killed");
                break;
            }

            // Wait before restarting child process
            m_restartCount.fetch_add(1, std::memory_order_relaxed);
            LOG_F(INFO, "Restarting child process (attempt {})",
                  m_restartCount.load());
            ::Sleep(getDaemonRestartInterval() * 1000);
        }
#elif defined(__APPLE__) || defined(__linux__)
        // Unix platform implementation (including macOS and Linux)
        // Ensure file descriptors aren't exhausted
        struct rlimit rl;
        if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
            // Set to maximum available file descriptors
            rl.rlim_cur = rl.rlim_max;
            setrlimit(RLIMIT_NOFILE, &rl);
        }

        // Create daemon process
        pid_t pid = fork();
        if (pid < 0) {
            throw DaemonException(
                std::format("Failed to fork: {}", strerror(errno)));
        }

        if (pid > 0) {  // Parent exits
            exit(EXIT_SUCCESS);
        }

        // Create new session
        if (setsid() < 0) {
            throw DaemonException(
                std::format("Failed to setsid: {}", strerror(errno)));
        }

        // Ignore terminal I/O signals and SIGHUP
        signal(SIGCHLD, SIG_IGN);
        signal(SIGHUP, SIG_IGN);

        // Ensure process doesn't become session leader
        pid = fork();
        if (pid < 0) {
            throw DaemonException(
                std::format("Second fork failed: {}", strerror(errno)));
        }

        if (pid > 0) {  // First child exits
            exit(EXIT_SUCCESS);
        }

#ifdef __APPLE__
        // macOS uses current directory
        const char* workDir = ".";
#else
        // Linux uses root directory
        const char* workDir = "/";
#endif

        // Change working directory
        if (chdir(workDir) < 0) {
            LOG_F(WARNING, "Failed to change directory: {}", strerror(errno));
        }

        // Close all open file descriptors
        for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
            close(x);
        }

        // Reopen standard input/output/error to /dev/null
        int fd = open("/dev/null", O_RDWR);
        if (fd != -1) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO) {
                close(fd);
            }
        }

        m_parentId.id = getpid();
        m_parentStartTime = time(nullptr);

        while (true) {
            pid_t child_pid = fork();  // Create child process
            if (child_pid == 0) {      // Child process
                m_mainId.id = getpid();
                m_mainStartTime = time(nullptr);
                LOG_F(INFO, "daemon process start pid={}", getpid());
                return realStartModern(args,
                                       [](std::span<char*>) { return 0; });
            }

            if (child_pid < 0) {  // Failed to create child process
                LOG_F(ERROR, "fork fail return={} errno={} errstr={}",
                      child_pid, errno, strerror(errno));
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;  // Try again
            }

            // Parent process
            int status = 0;
            waitpid(child_pid, &status, 0);  // Wait for child to exit

            // Child process abnormal exit
            if (status != 0) {
                if (WIFEXITED(status) &&
                    WEXITSTATUS(status) == 9) {  // SIGKILL signal killed child
                                                 // process, no need to restart
                    LOG_F(INFO, "daemon process killed pid={}", getpid());
                    break;
                }

                // Log and restart child process
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
            } else {  // Normal exit, exit program directly
                LOG_F(INFO, "daemon process exit normally pid={}", getpid());
                break;
            }

            // Wait before restarting child process
            m_restartCount.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(
                std::chrono::seconds(getDaemonRestartInterval()));
        }
#endif
        return 0;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in realDaemonModern: {}", e.what());
        return -1;
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in realDaemonModern");
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

        // Check parameter boundaries
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

        if (!isDaemon) {  // No need to create daemon process
            m_parentId = ProcessId::current();
            m_parentStartTime = time(nullptr);
            m_pidFilePath = g_pid_file_path;
            return realStart(argc, argv, mainCb);
        }

        // Create daemon process
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

// Modern interface version (using std::span)
template <ModernProcessCallback Callback>
auto DaemonGuard::startDaemonModern(std::span<char*> args,
                                    const Callback& mainCb, bool isDaemon)
    -> int {
    try {
        if (args.empty()) {
            throw DaemonException("Empty argument vector");
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

        if (!isDaemon) {  // No need to create daemon process
            m_parentId = ProcessId::current();
            m_parentStartTime = time(nullptr);
            m_pidFilePath = g_pid_file_path;
            return realStartModern(args, mainCb);
        }

        // Create daemon process
        m_pidFilePath = g_pid_file_path;
        return realDaemonModern(args, mainCb);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in startDaemonModern: {}", e.what());
        return -1;
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in startDaemonModern");
        return -1;
    }
}

auto DaemonGuard::isRunning() const noexcept -> bool {
    if (!m_mainId.valid()) {
        return false;
    }

#ifdef _WIN32
    DWORD exitCode = 0;
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION, FALSE,
        static_cast<DWORD>(reinterpret_cast<uintptr_t>(m_mainId.id)));
    if (hProcess == NULL) {
        return false;
    }

    BOOL result = GetExitCodeProcess(hProcess, &exitCode);
    CloseHandle(hProcess);

    return result && exitCode == STILL_ACTIVE;
#else
    // Send signal 0 to detect if process exists
    return kill(m_mainId.id, 0) == 0;
#endif
}

void signalHandler(int signum) noexcept {
    try {
        if (signum == SIGTERM || signum == SIGINT) {
            ProcessCleanupManager::cleanup();

            // Use flag to ensure we log only once
            static std::atomic<bool> handlingSignal{false};
            bool expected = false;
            if (handlingSignal.compare_exchange_strong(expected, true)) {
                LOG_F(INFO, "Received signal {} ({}), shutting down...", signum,
                      signum == SIGTERM ? "SIGTERM" : "SIGINT");
            }

            exit(0);
        }
    } catch (...) {
        // Should not throw exceptions in signal handlers
    }
}

// New function: register signal handlers
bool registerSignalHandlers(std::span<const int> signals) noexcept {
    try {
        bool success = true;
        for (int sig : signals) {
#ifdef _WIN32
            if (signal(sig, signalHandler) == SIG_ERR) {
                LOG_F(ERROR, "Failed to register signal handler for signal {}",
                      sig);
                success = false;
            }
#else
            struct sigaction sa;
            sa.sa_handler = signalHandler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;

            if (sigaction(sig, &sa, nullptr) == -1) {
                LOG_F(ERROR,
                      "Failed to register signal handler for signal {}: {}",
                      sig, strerror(errno));
                success = false;
            }
#endif
        }
        return success;
    } catch (...) {
        return false;
    }
}

// New function: check if process is running in background
bool isProcessBackground() noexcept {
#ifdef _WIN32
    // Windows doesn't support traditional background process concept
    // Can approximate by checking if process has a console
    return GetConsoleWindow() == NULL;
#else
    // On Unix platforms, check if process group ID equals terminal process
    // group ID
    pid_t pgid = getpgrp();
    int tty_fd = STDIN_FILENO;  // Typically use standard input for checking

    // If not a terminal or process group ID differs from terminal process group
    // ID, consider it a background process
    return !isatty(tty_fd) || pgid != tcgetpgrp(tty_fd);
#endif
}

void writePidFile(const std::filesystem::path& filePath) {
    try {
        // Create directory (if it doesn't exist)
        auto parent_path = filePath.parent_path();
        if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
            std::filesystem::create_directories(parent_path);
        }

        // Open PID file in exclusive mode
        std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
        if (!ofs) {
            throw std::filesystem::filesystem_error(
                std::format("Failed to open PID file: {}", filePath.string()),
                filePath, std::make_error_code(std::errc::permission_denied));
        }

#ifdef _WIN32
        ofs << GetCurrentProcessId();
#else
        ofs << getpid();
#endif

        ofs.close();
        if (ofs.fail()) {
            throw std::filesystem::filesystem_error(
                std::format("Failed to close PID file: {}", filePath.string()),
                filePath, std::make_error_code(std::errc::io_error));
        }

        LOG_F(INFO, "Created PID file: {}", filePath.string());
    } catch (const std::filesystem::filesystem_error& e) {
        throw;
    } catch (const std::exception& e) {
        throw std::filesystem::filesystem_error(
            std::format("Failed to write PID file: {}", e.what()), filePath,
            std::make_error_code(std::errc::io_error));
    }
}

auto checkPidFile(const std::filesystem::path& filePath) noexcept -> bool {
    try {
#ifdef _WIN32
        // Windows platform check if file exists
        if (!std::filesystem::exists(filePath)) {
            return false;
        }

        // Read PID file
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

        // Check if process exists
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (hProcess == NULL) {
            return false;
        }

        DWORD exitCode = 0;
        BOOL result = GetExitCodeProcess(hProcess, &exitCode);
        CloseHandle(hProcess);

        return result && exitCode == STILL_ACTIVE;
#elif defined(__APPLE__)
        // macOS platform check if process exists
        struct stat st{};
        if (stat(filePath.c_str(), &st) != 0) {
            return false;
        }

        // Read PID
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

        // Check if process exists on macOS
        int name[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
        struct kinfo_proc info;
        size_t size = sizeof(info);

        if (sysctl(name, 4, &info, &size, NULL, 0) == -1) {
            return false;
        }

        // If size is 0, process doesn't exist
        return size > 0;
#else
        // Linux platform check if process exists
        struct stat st{};
        if (stat(filePath.c_str(), &st) != 0) {
            return false;
        }

        // Read PID
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

        // Check if process exists
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
    LOG_F(INFO, "Daemon restart interval set to {} seconds", seconds);
}

int getDaemonRestartInterval() noexcept {
    std::lock_guard<std::mutex> lock(g_daemon_mutex);
    return g_daemon_restart_interval;
}

// Explicit template instantiation
// Traditional interface template instantiation
template auto DaemonGuard::realStart<std::function<int(int, char**)>>(
    int, char**, const std::function<int(int, char**)>&) -> int;
template auto DaemonGuard::realDaemon<std::function<int(int, char**)>>(
    int, char**, const std::function<int(int, char**)>&) -> int;
template auto DaemonGuard::startDaemon<std::function<int(int, char**)>>(
    int, char**, const std::function<int(int, char**)>&, bool) -> int;

// Modern interface template instantiation
template auto
DaemonGuard::realStartModern<std::function<int(std::span<char*>)>>(
    std::span<char*>, const std::function<int(std::span<char*>)>&) -> int;
template auto
DaemonGuard::realDaemonModern<std::function<int(std::span<char*>)>>(
    std::span<char*>, const std::function<int(std::span<char*>)>&) -> int;
template auto
DaemonGuard::startDaemonModern<std::function<int(std::span<char*>)>>(
    std::span<char*>, const std::function<int(std::span<char*>)>&, bool) -> int;

}  // namespace atom::async
