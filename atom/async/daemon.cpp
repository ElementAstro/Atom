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
                if (std::filesystem::exists(
                        path)) {  // Check if file exists before removing
                    std::filesystem::remove(path);
                    LOG_F(INFO, "PID file {} removed during cleanup.",
                          path.string());
                }
            } catch (const std::filesystem::filesystem_error& e) {
                LOG_F(ERROR, "Error removing PID file {} during cleanup: {}",
                      path.string(), e.what());
            } catch (...) {
                LOG_F(ERROR,
                      "Unknown error removing PID file {} during cleanup.",
                      path.string());
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
            LOG_F(ERROR, "CreateToolhelp32Snapshot failed with error: {}",
                  GetLastError());
            return std::nullopt;
        }

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (!Process32First(hSnapshot, &pe32)) {
            LOG_F(ERROR, "Process32First failed with error: {}",
                  GetLastError());
            CloseHandle(hSnapshot);
            return std::nullopt;
        }

        do {
            if (pe32.th32ProcessID == pid) {
                CloseHandle(hSnapshot);
// Return executable file path as an approximation of command line.
// Full command line retrieval is more complex.
#ifdef UNICODE
                std::wstring wstr(pe32.szExeFile);
                if (wstr.empty())
                    return std::nullopt;
                int size_needed =
                    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                                        NULL, 0, NULL, NULL);
                if (size_needed == 0)
                    return std::nullopt;
                std::string strTo(size_needed, 0);
                WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                                    &strTo[0], size_needed, NULL, NULL);
                return strTo;
#else
                return std::string(pe32.szExeFile);
#endif
            }
        } while (Process32Next(hSnapshot, &pe32));

        CloseHandle(hSnapshot);
        LOG_F(WARNING,
              "Process with PID {} not found for getProcessCommandLine.", pid);
    } catch (const std::exception& e) {
        LOG_F(ERROR,
              "Exception in getProcessCommandLine (Windows) for PID {}: {}",
              pid, e.what());
    } catch (...) {
        LOG_F(
            ERROR,
            "Unknown exception in getProcessCommandLine (Windows) for PID {}.",
            pid);
    }
    return std::nullopt;
}
#elif defined(__APPLE__)
// macOS platform - get process command line
[[maybe_unused]] auto getProcessCommandLine(pid_t pid)
    -> std::optional<std::string> {
    try {
        char pathBuffer[PROC_PIDPATHINFO_MAXSIZE];
        if (proc_pidpath(pid, pathBuffer, sizeof(pathBuffer)) <= 0) {
            LOG_F(ERROR, "proc_pidpath failed for PID {}: {}", pid,
                  strerror(errno));
            return std::nullopt;
        }
        // proc_pidpath gets the executable path. For full command line,
        // KERN_PROCARGS2 is needed but complex. Returning path as per original
        // simplified logic.
        return std::string(pathBuffer);
    } catch (const std::exception& e) {
        LOG_F(ERROR,
              "Exception in getProcessCommandLine (macOS) for PID {}: {}", pid,
              e.what());
    } catch (...) {
        LOG_F(ERROR,
              "Unknown exception in getProcessCommandLine (macOS) for PID {}.",
              pid);
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
            LOG_F(WARNING, "cmdline file not found for PID {}: {}", pid,
                  cmdlinePath.string());
            return std::nullopt;
        }

        std::ifstream ifs(cmdlinePath,
                          std::ios::binary);  // Open in binary mode
        if (!ifs) {
            LOG_F(ERROR, "Failed to open cmdline file for PID {}: {}", pid,
                  cmdlinePath.string());
            return std::nullopt;
        }

        std::string cmdline_content((std::istreambuf_iterator<char>(ifs)),
                                    std::istreambuf_iterator<char>());

        if (cmdline_content.empty()) {
            return std::nullopt;
        }

        // Replace null characters (except the last one if it's a double null)
        // with spaces
        std::string result_cmdline;
        for (size_t i = 0; i < cmdline_content.length(); ++i) {
            if (cmdline_content[i] == '\\0') {
                // If it's the last char or the next one is also null, stop
                // (common for end of cmdline)
                if (i == cmdline_content.length() - 1 ||
                    (i < cmdline_content.length() - 1 &&
                     cmdline_content[i + 1] == '\\0')) {
                    if (!result_cmdline.empty() && result_cmdline.back() == ' ')
                        result_cmdline.pop_back();  // remove trailing space
                    break;
                }
                result_cmdline += ' ';
            } else {
                result_cmdline += cmdline_content[i];
            }
        }
        // Trim trailing space if any from the loop logic
        if (!result_cmdline.empty() && result_cmdline.back() == ' ') {
            result_cmdline.pop_back();
        }
        return result_cmdline;

    } catch (const std::filesystem::filesystem_error& e) {
        LOG_F(
            ERROR,
            "Filesystem error in getProcessCommandLine (Linux) for PID {}: {}",
            pid, e.what());
    } catch (const std::exception& e) {
        LOG_F(ERROR,
              "Exception in getProcessCommandLine (Linux) for PID {}: {}", pid,
              e.what());
    } catch (...) {
        LOG_F(ERROR,
              "Unknown exception in getProcessCommandLine (Linux) for PID {}.",
              pid);
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
            // The actual removal of PID file is handled by
            // ProcessCleanupManager::cleanup() at exit. This destructor might
            // be called in the parent process after forking, where it shouldn't
            // remove a PID file owned by the child. Logging existence for
            // debugging purposes.
            if (std::filesystem::exists(*m_pidFilePath)) {
                LOG_F(INFO,
                      "DaemonGuard destructor: PID file {} exists. Cleanup is "
                      "deferred to ProcessCleanupManager.",
                      m_pidFilePath->string());
            }
        } catch (const std::filesystem::filesystem_error& e) {
            // Log errors if std::filesystem::exists throws, though it's usually
            // noexcept.
            LOG_F(ERROR,
                  "Filesystem error in ~DaemonGuard() checking PID file {}: {}",
                  m_pidFilePath->string(), e.what());
        } catch (...) {
            // Catch any other unexpected exceptions.
            LOG_F(ERROR,
                  "Unknown error in ~DaemonGuard() checking PID file {}.",
                  m_pidFilePath->string());
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
                writePidFile(*m_pidFilePath);  // Non-daemon process might also
                                               // want a PID file
                // ProcessCleanupManager::registerPidFile(*m_pidFilePath); //
                // Already done in writePidFile
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to write PID file {} in realStart: {}",
                      m_pidFilePath->string(), e.what());
                // Decide if this is fatal. For now, log and continue.
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
        if (args.empty() || args[0] == nullptr) {
            throw DaemonException(
                "args must not be empty and args[0] not null in "
                "realStartModern");
        }
        // Get current process ID
        m_mainId = ProcessId::current();
        m_mainStartTime = time(nullptr);

        if (m_pidFilePath.has_value()) {
            try {
                writePidFile(*m_pidFilePath);  // Non-daemon process might also
                                               // want a PID file
                // ProcessCleanupManager::registerPidFile(*m_pidFilePath); //
                // Already done in writePidFile
            } catch (const std::exception& e) {
                LOG_F(ERROR,
                      "Failed to write PID file {} in realStartModern: {}",
                      m_pidFilePath->string(), e.what());
                // Decide if this is fatal. For now, log and continue.
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
auto DaemonGuard::realDaemon(
    [[maybe_unused]] int argc, char** argv,
    [[maybe_unused]] const Callback& mainCb)  // mainCb used by child
    -> int {
    try {
        if (argv == nullptr) {
            throw DaemonException("Invalid argument vector (nullptr)");
        }
        LOG_F(INFO, "Attempting to start daemon process...");
        m_parentId = ProcessId::current();
        m_parentStartTime = time(nullptr);

#ifdef _WIN32
        // Windows daemonization: launch a new detached process.
        // The parent process (this one) will exit after launching.
        // The child process re-runs the executable. The child needs logic to
        // detect it's the daemon worker (e.g., via a special command-line
        // argument not handled here, or by checking parent). This function, as
        // part of the parent, launches the child and returns 0 for parent to
        // exit.
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        std::string cmdLine;
        char exePath[MAX_PATH];
        if (!GetModuleFileName(NULL, exePath, MAX_PATH)) {
            throw DaemonException(std::format(
                "GetModuleFileName failed in realDaemon: {}", GetLastError()));
        }
        cmdLine = "\"" + std::string(exePath) + "\"";
        // Pass existing arguments. A robust solution would add a specific flag
        // for the child.
        for (int i = 1; i < argc; ++i) {
            if (argv[i] != nullptr) {  // Ensure argv[i] is not null
                cmdLine += " \"" + std::string(argv[i]) + "\"";
            }
        }
        // Example: cmdLine += " --daemon-worker"; // Child would check for this

        if (!CreateProcess(NULL,  // Application name (use cmdLine)
                           const_cast<char*>(cmdLine.c_str()),  // Command line
                           NULL,              // Process security attributes
                           NULL,              // Thread security attributes
                           FALSE,             // Inherit handles
                           DETACHED_PROCESS,  // Creation flags
                           NULL,              // Environment
                           NULL,              // Current directory
                           &si,               // Startup Info
                           &pi)               // Process Information
        ) {
            throw DaemonException(std::format(
                "CreateProcess failed in realDaemon: {}", GetLastError()));
        }

        LOG_F(INFO,
              "Windows: Parent (PID {}) launched detached process (PID {}). "
              "Parent will exit.",
              m_parentId.id, pi.dwProcessId);
        CloseHandle(pi.hProcess);  // Parent doesn't need these handles
        CloseHandle(pi.hThread);
        return 0;  // Signal to startDaemon that parent should exit

#elif defined(__APPLE__) || defined(__linux__)  // Unix-like daemonization
        pid_t pid = fork();
        if (pid < 0) {  // Fork failed
            throw DaemonException(
                std::format("fork failed in realDaemon: {}", strerror(errno)));
        }

        if (pid > 0) {  // Parent process
            LOG_F(INFO,
                  "Parent process (PID {}) forked child (PID {}). Parent "
                  "exiting.",
                  getpid(), pid);
            m_mainId.id = pid;  // Store child PID in parent's guard (parent is
                                // about to exit)
            return 0;           // Indicates parent should exit.
        }

        // Child process continues (pid == 0)
        // This instance of DaemonGuard is now in the child.
        m_parentId.reset();  // Child has no parent in the context of this guard
                             // object logic
        m_mainId = ProcessId::current();
        m_mainStartTime = time(nullptr);
        // g_is_daemon should be true, set by startDaemon. Re-affirm if
        // necessary. std::atomic_store_explicit(&g_is_daemon, true,
        // std::memory_order_relaxed);

        LOG_F(INFO, "Child process (PID {}) starting as daemon.", m_mainId.id);

        if (setsid() < 0) {  // Create a new session and process group
            throw DaemonException(std::format(
                "setsid failed in realDaemon child: {}", strerror(errno)));
        }

        // Second fork to ensure daemon cannot reacquire a controlling terminal
        pid = fork();
        if (pid < 0) {
            throw DaemonException(std::format(
                "Second fork failed in realDaemon: {}", strerror(errno)));
        }
        if (pid > 0) {  // First child (intermediate) exits
            LOG_F(INFO,
                  "First child (PID {}) forked second child (PID {}). First "
                  "child exiting.",
                  getpid(), pid);
            exit(0);
        }

        // Second child (actual daemon) continues
        m_mainId = ProcessId::current();
        m_mainStartTime = time(nullptr);
        LOG_F(INFO, "Actual daemon process (PID {}) starting.", m_mainId.id);

        if (chdir("/") < 0) {
            LOG_F(WARNING,
                  "chdir(\"/\") failed in realDaemon: {}. Continuing...",
                  strerror(errno));
        }
        umask(0);  // Set file mode creation mask to 0

        // Close standard file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // Redirect standard I/O to /dev/null
        int fd_dev_null = open("/dev/null", O_RDWR);
        if (fd_dev_null != -1) {
            dup2(fd_dev_null, STDIN_FILENO);
            dup2(fd_dev_null, STDOUT_FILENO);
            dup2(fd_dev_null, STDERR_FILENO);
            if (fd_dev_null > STDERR_FILENO) {
                close(fd_dev_null);
            }
        } else {
            LOG_F(WARNING,
                  "Failed to open /dev/null for redirecting stdio in daemon.");
        }

        if (m_pidFilePath.has_value()) {
            try {
                writePidFile(*m_pidFilePath);  // Daemon writes its own PID
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to write PID file {} in daemon: {}",
                      m_pidFilePath->string(), e.what());
            }
        }

        LOG_F(INFO,
              "Daemon process (PID {}) initialized. Calling main callback.",
              m_mainId.id);
        return mainCb(argc, argv);  // Call the actual worker function

#else
        LOG_F(ERROR, "Daemon mode is not supported on this platform.");
        throw DaemonException("Daemon mode not supported on this platform.");
#endif
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
    std::span<char*> args,
    [[maybe_unused]] const Callback& mainCb)  // mainCb used by child
    -> int {
    try {
        if (args.empty() || args[0] == nullptr) {
            throw DaemonException(
                "args must not be empty and args[0] not null in "
                "realDaemonModern");
        }
        LOG_F(INFO, "Attempting to start daemon process (modern interface)...");
        m_parentId = ProcessId::current();
        m_parentStartTime = time(nullptr);

#ifdef _WIN32
        // Similar to realDaemon for Windows
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        std::string cmdLine;
        char exePath[MAX_PATH];
        if (!GetModuleFileName(NULL, exePath, MAX_PATH)) {
            throw DaemonException(
                std::format("GetModuleFileName failed in realDaemonModern: {}",
                            GetLastError()));
        }
        cmdLine = "\"" + std::string(exePath) + "\"";
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] != nullptr) {
                cmdLine += " \"" + std::string(args[i]) + "\"";
            }
        }
        // Example: cmdLine += " --daemon-worker";

        if (!CreateProcess(NULL, const_cast<char*>(cmdLine.c_str()), NULL, NULL,
                           FALSE, DETACHED_PROCESS, NULL, NULL, &si, &pi)) {
            throw DaemonException(
                std::format("CreateProcess failed in realDaemonModern: {}",
                            GetLastError()));
        }

        LOG_F(INFO,
              "Windows: Parent (PID {}) launched detached process (PID {}). "
              "Parent will exit (modern).",
              m_parentId.id, pi.dwProcessId);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 0;  // Signal to startDaemonModern that parent should exit

#elif defined(__APPLE__) || defined(__linux__)  // Unix-like daemonization
        pid_t pid = fork();
        if (pid < 0) {
            throw DaemonException(std::format(
                "fork failed in realDaemonModern: {}", strerror(errno)));
        }
        if (pid > 0) {  // Parent
            LOG_F(INFO,
                  "Parent process (PID {}) forked child (PID {}). Parent "
                  "exiting (modern).",
                  getpid(), pid);
            m_mainId.id = pid;
            return 0;  // Parent exits
        }

        // Child process
        m_parentId.reset();
        m_mainId = ProcessId::current();
        m_mainStartTime = time(nullptr);
        LOG_F(INFO, "Child process (PID {}) starting as daemon (modern).",
              m_mainId.id);

        if (setsid() < 0) {
            throw DaemonException(
                std::format("setsid failed in realDaemonModern child: {}",
                            strerror(errno)));
        }

        pid = fork();  // Second fork
        if (pid < 0) {
            throw DaemonException(std::format(
                "Second fork failed in realDaemonModern: {}", strerror(errno)));
        }
        if (pid > 0) {  // First child exits
            LOG_F(INFO,
                  "First child (PID {}) forked second child (PID {}). First "
                  "child exiting (modern).",
                  getpid(), pid);
            exit(0);
        }

        // Second child (actual daemon)
        m_mainId = ProcessId::current();
        m_mainStartTime = time(nullptr);
        LOG_F(INFO, "Actual daemon process (PID {}) starting (modern).",
              m_mainId.id);

        if (chdir("/") < 0) {
            LOG_F(WARNING,
                  "chdir(\"/\") failed in realDaemonModern: {}. Continuing...",
                  strerror(errno));
        }
        umask(0);

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        int fd_dev_null = open("/dev/null", O_RDWR);
        if (fd_dev_null != -1) {
            dup2(fd_dev_null, STDIN_FILENO);
            dup2(fd_dev_null, STDOUT_FILENO);
            dup2(fd_dev_null, STDERR_FILENO);
            if (fd_dev_null > STDERR_FILENO)
                close(fd_dev_null);
        } else {
            LOG_F(WARNING,
                  "Failed to open /dev/null for redirecting stdio in modern "
                  "daemon.");
        }

        if (m_pidFilePath.has_value()) {
            try {
                writePidFile(*m_pidFilePath);
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to write PID file {} in modern daemon: {}",
                      m_pidFilePath->string(), e.what());
            }
        }

        LOG_F(INFO,
              "Daemon process (PID {}) initialized. Calling main callback "
              "(modern).",
              m_mainId.id);
        return mainCb(args);  // Call worker

#else
        LOG_F(ERROR, "Daemon mode is not supported on this platform (modern).");
        throw DaemonException(
            "Daemon mode not supported on this platform (modern).");
#endif
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
            // signal() on Windows has limitations. SetConsoleCtrlHandler is
            // better for console events. For this generic handler, we'll use
            // signal() but acknowledge its limits.
            if (signal(sig, signalHandler) == SIG_ERR) {
                LOG_F(WARNING,
                      "Failed to register signal handler for signal {} on "
                      "Windows using CRT signal(). This may not work as "
                      "expected for all signal types.",
                      sig);
                // success = false; // Optionally mark as failure
            } else {
                LOG_F(INFO,
                      "Registered signal handler for signal {} on Windows "
                      "using CRT signal().",
                      sig);
            }
#else  // Unix-like (macOS, Linux)
            struct sigaction sa;
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = signalHandler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags =
                SA_RESTART;  // Important for syscalls interrupted by signals

            if (sigaction(sig, &sa, NULL) == -1) {
                LOG_F(ERROR,
                      "Failed to register signal handler for signal {} (Unix): "
                      "{}",
                      sig, strerror(errno));
                success = false;
            } else {
                LOG_F(INFO,
                      "Successfully registered signal handler for signal {} "
                      "(Unix).",
                      sig);
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
        auto parent_path = filePath.parent_path();
        if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
            if (!std::filesystem::create_directories(parent_path)) {
                throw DaemonException(
                    std::format("Failed to create directory for PID file: {}",
                                parent_path.string()));
            }
            LOG_F(INFO, "Created directory for PID file: {}",
                  parent_path.string());
        }

        std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
        if (!ofs) {
            throw DaemonException(std::format(
                "Failed to open PID file for writing: {}", filePath.string()));
        }

        ProcessId current_pid_struct = ProcessId::current();
#ifdef _WIN32
        // ProcessId struct stores HANDLE for _WIN32, but we need DWORD for PID.
        // GetCurrentProcessId() is more direct for writing the numeric PID.
        DWORD pid_val = GetCurrentProcessId();
        ofs << pid_val;
#else
        pid_t pid_val = current_pid_struct.id;  // pid_t is already numeric
        ofs << pid_val;
#endif
        if (ofs.fail()) {
            throw DaemonException(std::format("Failed to write PID to file: {}",
                                              filePath.string()));
        }
        // Explicitly close before checking failbit again, as some errors might
        // only show on close.
        ofs.close();
        if (ofs.fail()) {
            throw DaemonException(
                std::format("Failed to close PID file after writing: {}",
                            filePath.string()));
        }

        LOG_F(INFO, "Created PID file: {} with PID: {}", filePath.string(),
              pid_val);
        ProcessCleanupManager::registerPidFile(
            filePath);  // Register for cleanup on exit

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
        if (!std::filesystem::exists(filePath)) {
            return false;  // PID file doesn't exist
        }

        std::ifstream ifs(filePath);
        if (!ifs) {
            LOG_F(WARNING,
                  "PID file {} exists but cannot be opened for reading.",
                  filePath.string());
            return false;
        }

        long pid_from_file = 0;
        ifs >> pid_from_file;
        if (ifs.fail() || pid_from_file <= 0) {
            LOG_F(WARNING,
                  "PID file {} does not contain a valid PID. Content problem "
                  "or empty file.",
                  filePath.string());
            ifs.close();
            // Consider removing stale/invalid PID file here after logging
            // try { std::filesystem::remove(filePath); } catch(...) {}
            return false;
        }
        ifs.close();

#ifdef _WIN32
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                      static_cast<DWORD>(pid_from_file));
        if (hProcess == NULL) {
            // If OpenProcess fails with ERROR_INVALID_PARAMETER, process likely
            // doesn't exist. Other errors (like ERROR_ACCESS_DENIED) mean it
            // might exist but we can't query.
            if (GetLastError() == ERROR_INVALID_PARAMETER) {
                LOG_F(INFO,
                      "Process with PID {} from file {} not found (OpenProcess "
                      "ERROR_INVALID_PARAMETER). Stale PID file?",
                      pid_from_file, filePath.string());
            } else {
                LOG_F(WARNING,
                      "OpenProcess failed for PID {} from file {}. Error: {}. "
                      "Assuming not accessible/running.",
                      pid_from_file, filePath.string(), GetLastError());
            }
            return false;
        }
        DWORD exitCode;
        BOOL result = GetExitCodeProcess(hProcess, &exitCode);
        CloseHandle(hProcess);
        if (!result) {
            LOG_F(
                WARNING,
                "GetExitCodeProcess failed for PID {} from file {}. Error: {}",
                pid_from_file, filePath.string(), GetLastError());
            return false;
        }
        return exitCode == STILL_ACTIVE;
#elif defined(__APPLE__) || defined(__linux__)  // Unix-like
        if (kill(static_cast<pid_t>(pid_from_file), 0) == 0) {
            return true;  // Process exists
        } else {
            if (errno == ESRCH) {
                LOG_F(INFO,
                      "Process with PID {} from file {} does not exist "
                      "(ESRCH). Stale PID file?",
                      pid_from_file, filePath.string());
                // Consider removing stale PID file here
                // try { std::filesystem::remove(filePath); } catch(...) {}
            } else if (errno == EPERM) {
                LOG_F(WARNING,
                      "No permission to signal PID {} from file {}, but "
                      "process likely exists (EPERM).",
                      pid_from_file, filePath.string());
                return true;  // Assume running as we can't prove otherwise due
                              // to permissions
            } else {
                LOG_F(WARNING,
                      "kill(PID, 0) failed for PID {} from file {}: {}. "
                      "Assuming not running.",
                      pid_from_file, filePath.string(), strerror(errno));
            }
            return false;
        }
#else
        LOG_F(WARNING,
              "checkPidFile not fully implemented for this platform. Assuming "
              "process not running.");
        return false;
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
