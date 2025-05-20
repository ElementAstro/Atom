/*
 * daemon.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-11

Description: Daemon process implementation (Header-Only Library)

**************************************************/

#ifndef ATOM_SERVER_DAEMON_HPP
#define ATOM_SERVER_DAEMON_HPP

// Standard C++ Includes
#include <atomic>
#include <concepts>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <format>  // C++20 standard formatting library
#include <fstream>
#include <mutex>
#include <optional>
#include <ostream>
#include <source_location>  // C++20 feature
#include <span>             // C++20 feature
#include <stdexcept>
#include <string>
#include <string_view>   // More efficient string view
#include <vector>

// Platform-specific Includes
#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <TlHelp32.h>  // For getProcessCommandLine
// clang-format on
#else
#include <fcntl.h>         // For open, O_RDWR
#include <signal.h>        // For signal, sigaction
#include <sys/resource.h>  // For setrlimit, etc. (though not directly used in current daemonize, good for context)
#include <sys/stat.h>      // For umask, stat
#include <sys/wait.h>      // For waitpid
#include <unistd.h>        // For fork, setsid, chdir, getpid, etc.
#endif

#ifdef __APPLE__
#include <libproc.h>  // For proc_pidpath (macOS process management)
#include <mach/mach_time.h>  // For timing (if needed, currently not directly used by daemon logic)
#include <sys/sysctl.h>  // For macOS system control (if KERN_PROCARGS2 were used)
#endif

// External Dependencies (assumed to be available)
#include "atom/log/loguru.hpp"  // Logging library
#include "atom/utils/time.hpp"  // Time utilities

namespace atom::async {

// Using std::string_view to optimize exception type
class DaemonException : public std::runtime_error {
public:
    // Inherit constructors from std::runtime_error
    using std::runtime_error::runtime_error;

    // Using std::source_location to record where the exception occurred
    explicit DaemonException(
        std::string_view what_arg,
        const std::source_location& location = std::source_location::current())
        : std::runtime_error(std::string(what_arg) + " [" +
                             location.file_name() + ":" +
                             std::to_string(location.line()) + ":" +
                             std::to_string(location.column()) + " (" +
                             location.function_name() + ")]") {}
};

// Process callback function concept, using std::span instead of char**
// parameters to provide a safer interface
template <typename T>
concept ProcessCallback = requires(T callback, int argc, char** argv) {
    { callback(argc, argv) } -> std::convertible_to<int>;
};

// Enhanced process callback function concept, supporting std::span interface
template <typename T>
concept ModernProcessCallback = requires(T callback, std::span<char*> args) {
    { callback(args) } -> std::convertible_to<int>;
};

// Platform-independent process identifier type
struct ProcessId {
#ifdef _WIN32
    HANDLE id = nullptr;  // Changed from 0 to nullptr for HANDLE
#else
    pid_t id = 0;
#endif

    // Default constructor
    constexpr ProcessId() noexcept = default;

    // Construct from platform-specific type
#ifdef _WIN32
    explicit constexpr ProcessId(HANDLE handle) noexcept : id(handle) {}
#else
    explicit constexpr ProcessId(pid_t pid) noexcept : id(pid) {}
#endif

    // Static method to get the current process ID
    [[nodiscard]] static ProcessId current() noexcept {
#ifdef _WIN32
        return ProcessId{GetCurrentProcess()};  // Returns a pseudo-handle
#else
        return ProcessId{getpid()};
#endif
    }

    // Check if the process ID is valid
    [[nodiscard]] constexpr bool valid() const noexcept {
#ifdef _WIN32
        return id != nullptr && id != INVALID_HANDLE_VALUE;
#else
        return id > 0;
#endif
    }

    // Reset to invalid process ID
    constexpr void reset() noexcept {
#ifdef _WIN32
        id = nullptr;
#else
        id = 0;
#endif
    }
};

// Global daemon-related configurations, inline for header-only
inline int g_daemon_restart_interval = 10;  // seconds
inline std::filesystem::path g_pid_file_path =
    "lithium-daemon";              // Default PID file name
inline std::mutex g_daemon_mutex;  // Mutex for g_daemon_restart_interval
inline std::atomic<bool> g_is_daemon{
    false};  // Global flag indicating if the process is in daemon mode

namespace {  // Anonymous namespace for implementation details

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
    inline static std::mutex s_mutex;
    inline static std::vector<std::filesystem::path> s_pidFiles;
};

// Platform-specific process utilities
#ifdef _WIN32
// Windows platform - get process command line
[[maybe_unused]] inline auto getProcessCommandLine(DWORD pid)
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
[[maybe_unused]] inline auto getProcessCommandLine(pid_t pid)
    -> std::optional<std::string> {
    try {
        char pathBuffer[PROC_PIDPATHINFO_MAXSIZE];
        if (proc_pidpath(pid, pathBuffer, sizeof(pathBuffer)) <= 0) {
            LOG_F(ERROR, "proc_pidpath failed for PID {}: {}", pid,
                  strerror(errno));
            return std::nullopt;
        }
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
#else  // Linux
// Linux platform - get process command line
[[maybe_unused]] inline auto getProcessCommandLine(pid_t pid)
    -> std::optional<std::string> {
    try {
        std::filesystem::path cmdlinePath =
            std::format("/proc/{}/cmdline", pid);
        if (!std::filesystem::exists(cmdlinePath)) {
            LOG_F(WARNING, "cmdline file not found for PID {}: {}", pid,
                  cmdlinePath.string());
            return std::nullopt;
        }

        std::ifstream ifs(cmdlinePath, std::ios::binary);
        if (!ifs) {
            LOG_F(ERROR, "Failed to open cmdline file for PID {}: {}", pid,
                  cmdlinePath.string());
            return std::nullopt;
        }

        std::string cmdline_content((std::istreambuf_iterator<char>(ifs)),
                                    std::istreambuf_iterator<char>());
        if (cmdline_content.empty())
            return std::nullopt;

        std::string result_cmdline;
        for (size_t i = 0; i < cmdline_content.length(); ++i) {
            if (cmdline_content[i] == '\0') {  // Corrected null character check
                if (i == cmdline_content.length() - 1 ||
                    (i < cmdline_content.length() - 1 &&
                     cmdline_content[i + 1] == '\0')) {
                    if (!result_cmdline.empty() && result_cmdline.back() == ' ')
                        result_cmdline.pop_back();
                    break;
                }
                result_cmdline += ' ';
            } else {
                result_cmdline += cmdline_content[i];
            }
        }
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

// Class for managing process information
class DaemonGuard {
public:
    DaemonGuard() noexcept = default;
    ~DaemonGuard() noexcept;

    DaemonGuard(const DaemonGuard&) = delete;
    DaemonGuard& operator=(const DaemonGuard&) = delete;

    [[nodiscard]] auto toString() const noexcept -> std::string;

    template <ProcessCallback Callback>
    auto realStart(int argc, char** argv, const Callback& mainCb) -> int;

    template <ModernProcessCallback Callback>
    auto realStartModern(std::span<char*> args, const Callback& mainCb) -> int;

    template <ProcessCallback Callback>
    auto realDaemon(int argc, char** argv, const Callback& mainCb) -> int;

    template <ModernProcessCallback Callback>
    auto realDaemonModern(std::span<char*> args, const Callback& mainCb) -> int;

    template <ProcessCallback Callback>
    auto startDaemon(int argc, char** argv, const Callback& mainCb,
                     bool isDaemon) -> int;

    template <ModernProcessCallback Callback>
    auto startDaemonModern(std::span<char*> args, const Callback& mainCb,
                           bool isDaemon) -> int;

    [[nodiscard]] auto getRestartCount() const noexcept -> int {
        return m_restartCount.load(std::memory_order_relaxed);
    }

    [[nodiscard]] auto isRunning() const noexcept -> bool;

    void setPidFilePath(const std::filesystem::path& path) noexcept {
        m_pidFilePath = path;
    }

    [[nodiscard]] auto getPidFilePath() const noexcept
        -> std::optional<std::filesystem::path> {
        return m_pidFilePath;
    }

private:
    ProcessId m_parentId;
    ProcessId m_mainId;
    time_t m_parentStartTime = 0;
    time_t m_mainStartTime = 0;
    std::atomic<int> m_restartCount{0};
    std::optional<std::filesystem::path> m_pidFilePath;
};

// Forward declaration for writePidFile used in DaemonGuard methods
inline void writePidFile(
    const std::filesystem::path& filePath = g_pid_file_path);

// Implementations for DaemonGuard methods
inline DaemonGuard::~DaemonGuard() noexcept {
    if (m_pidFilePath.has_value()) {
        try {
            if (std::filesystem::exists(*m_pidFilePath)) {
                LOG_F(INFO,
                      "DaemonGuard destructor: PID file {} exists. Cleanup is "
                      "deferred to ProcessCleanupManager.",
                      m_pidFilePath->string());
            }
        } catch (const std::filesystem::filesystem_error& e) {
            LOG_F(ERROR,
                  "Filesystem error in ~DaemonGuard() checking PID file {}: {}",
                  m_pidFilePath->string(), e.what());
        } catch (...) {
            LOG_F(ERROR,
                  "Unknown error in ~DaemonGuard() checking PID file {}.",
                  m_pidFilePath->string());
        }
    }
}

inline auto DaemonGuard::toString() const noexcept -> std::string {
    try {
        return std::format(
            "[DaemonGuard parentId={} mainId={} parentStartTime={} "
            "mainStartTime={} restartCount={}]",
            m_parentId.id, m_mainId.id,  // Note: Printing HANDLE on Windows
                                         // might not be user-friendly
            utils::timeStampToString(m_parentStartTime),
            utils::timeStampToString(m_mainStartTime),
            m_restartCount.load(std::memory_order_relaxed));
    } catch (const std::format_error& fe) {
        LOG_F(ERROR, "std::format error in DaemonGuard::toString(): %s",
              fe.what());
        return "[DaemonGuard toString() format error]";
    } catch (...) {
        return "[DaemonGuard toString() unknown error]";
    }
}

template <ProcessCallback Callback>
auto DaemonGuard::realStart(int argc, char** argv, const Callback& mainCb)
    -> int {
    try {
        if (argv == nullptr && argc > 0) {  // argc can be 0 with argv==nullptr
            throw DaemonException(
                "Invalid argument vector (nullptr with argc > 0)");
        }
        m_mainId = ProcessId::current();
        m_mainStartTime = time(nullptr);

        if (m_pidFilePath.has_value()) {
            try {
                writePidFile(*m_pidFilePath);
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to write PID file {} in realStart: {}",
                      m_pidFilePath->string(), e.what());
            }
        }
        return mainCb(argc, argv);
    } catch (const DaemonException&) {
        throw;  // Re-throw DaemonException to preserve source location
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in realStart: {}", e.what());
        throw DaemonException(std::string("Exception in realStart: ") +
                              e.what());
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in realStart");
        throw DaemonException("Unknown exception in realStart");
    }
    return -1;  // Should be unreachable if exceptions are thrown
}

template <ModernProcessCallback Callback>
auto DaemonGuard::realStartModern(std::span<char*> args, const Callback& mainCb)
    -> int {
    try {
        if (args.empty() || args[0] == nullptr) {
            throw DaemonException(
                "args must not be empty and args[0] not null in "
                "realStartModern");
        }
        m_mainId = ProcessId::current();
        m_mainStartTime = time(nullptr);

        if (m_pidFilePath.has_value()) {
            try {
                writePidFile(*m_pidFilePath);
            } catch (const std::exception& e) {
                LOG_F(ERROR,
                      "Failed to write PID file {} in realStartModern: {}",
                      m_pidFilePath->string(), e.what());
            }
        }
        return mainCb(args);
    } catch (const DaemonException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in realStartModern: {}", e.what());
        throw DaemonException(std::string("Exception in realStartModern: ") +
                              e.what());
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in realStartModern");
        throw DaemonException("Unknown exception in realStartModern");
    }
    return -1;
}

template <ProcessCallback Callback>
auto DaemonGuard::realDaemon(int argc, char** argv,
                             [[maybe_unused]] const Callback& mainCb) -> int {
    try {
        if (argv == nullptr && argc > 0) {
            throw DaemonException(
                "Invalid argument vector (nullptr with argc > 0)");
        }
        LOG_F(INFO, "Attempting to start daemon process...");
        m_parentId = ProcessId::current();
        m_parentStartTime = time(nullptr);

#ifdef _WIN32
        STARTUPINFOA si;  // Use STARTUPINFOA for char*
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        std::string cmdLine;
        char exePath[MAX_PATH];
        if (!GetModuleFileNameA(NULL, exePath,
                                MAX_PATH)) {  // Use GetModuleFileNameA
            throw DaemonException(std::format(
                "GetModuleFileNameA failed in realDaemon: {}", GetLastError()));
        }
        cmdLine = "\"" + std::string(exePath) + "\"";
        for (int i = 1; i < argc; ++i) {
            if (argv[i] != nullptr) {
                cmdLine += " \"" + std::string(argv[i]) + "\"";
            }
        }
        // cmdLine += " --daemon-worker"; // Example flag

        if (!CreateProcessA(NULL, const_cast<char*>(cmdLine.c_str()), NULL,
                            NULL, FALSE, DETACHED_PROCESS, NULL, NULL, &si,
                            &pi)) {
            throw DaemonException(std::format(
                "CreateProcessA failed in realDaemon: {}", GetLastError()));
        }
        LOG_F(INFO,
              "Windows: Parent (PID {}) launched detached process (PID {}). "
              "Parent will exit.",
              GetProcessId(m_parentId.id), pi.dwProcessId);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 0;  // Parent exits

#elif defined(__APPLE__) || defined(__linux__)
        pid_t pid = fork();
        if (pid < 0) {
            throw DaemonException(
                std::format("fork failed in realDaemon: {}", strerror(errno)));
        }
        if (pid > 0) {  // Parent process
            LOG_F(INFO,
                  "Parent process (PID {}) forked child (PID {}). Parent "
                  "exiting.",
                  getpid(), pid);
            // m_mainId.id = pid; // Parent's DaemonGuard is about to be
            // destroyed. This has limited use.
            return 0;  // Parent exits
        }

        // Child process (pid == 0)
        m_parentId.reset();  // Child has no logical parent in this DaemonGuard
                             // instance
        m_mainId = ProcessId::current();
        m_mainStartTime = time(nullptr);
        std::atomic_store_explicit(
            &g_is_daemon, true,
            std::memory_order_relaxed);  // Child is the daemon

        LOG_F(INFO, "Child process (PID {}) starting as daemon.", m_mainId.id);
        if (setsid() < 0) {
            throw DaemonException(std::format(
                "setsid failed in realDaemon child: {}", strerror(errno)));
        }

        pid = fork();  // Second fork
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

        // Second child (actual daemon)
        m_mainId = ProcessId::current();  // Update PID after second fork
        m_mainStartTime = time(nullptr);
        LOG_F(INFO, "Actual daemon process (PID {}) starting.", m_mainId.id);

        if (chdir("/") < 0) {
            LOG_F(WARNING,
                  "chdir(\"/\") failed in realDaemon: {}. Continuing...",
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
                  "Failed to open /dev/null for redirecting stdio in daemon.");
        }

        if (m_pidFilePath.has_value()) {
            try {
                writePidFile(*m_pidFilePath);
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to write PID file {} in daemon: {}",
                      m_pidFilePath->string(), e.what());
            }
        }
        LOG_F(INFO,
              "Daemon process (PID {}) initialized. Calling main callback.",
              m_mainId.id);
        return mainCb(argc, argv);  // Call worker
#else
        LOG_F(ERROR, "Daemon mode is not supported on this platform.");
        throw DaemonException("Daemon mode not supported on this platform.");
#endif
    } catch (const DaemonException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in realDaemon: {}", e.what());
        throw DaemonException(std::string("Exception in realDaemon: ") +
                              e.what());
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in realDaemon");
        throw DaemonException("Unknown exception in realDaemon");
    }
    return -1;
}

template <ModernProcessCallback Callback>
auto DaemonGuard::realDaemonModern(std::span<char*> args,
                                   [[maybe_unused]] const Callback& mainCb)
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
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        std::string cmdLine;
        char exePath[MAX_PATH];
        if (!GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
            throw DaemonException(
                std::format("GetModuleFileNameA failed in realDaemonModern: {}",
                            GetLastError()));
        }
        cmdLine = "\"" + std::string(exePath) + "\"";
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] != nullptr) {
                cmdLine += " \"" + std::string(args[i]) + "\"";
            }
        }
        // cmdLine += " --daemon-worker";

        if (!CreateProcessA(NULL, const_cast<char*>(cmdLine.c_str()), NULL,
                            NULL, FALSE, DETACHED_PROCESS, NULL, NULL, &si,
                            &pi)) {
            throw DaemonException(
                std::format("CreateProcessA failed in realDaemonModern: {}",
                            GetLastError()));
        }
        LOG_F(INFO,
              "Windows: Parent (PID {}) launched detached process (PID {}). "
              "Parent will exit (modern).",
              GetProcessId(m_parentId.id), pi.dwProcessId);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 0;  // Parent exits

#elif defined(__APPLE__) || defined(__linux__)
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
            return 0;  // Parent exits
        }

        // Child process
        m_parentId.reset();
        m_mainId = ProcessId::current();
        m_mainStartTime = time(nullptr);
        std::atomic_store_explicit(&g_is_daemon, true,
                                   std::memory_order_relaxed);

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
    } catch (const DaemonException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in realDaemonModern: {}", e.what());
        throw DaemonException(std::string("Exception in realDaemonModern: ") +
                              e.what());
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in realDaemonModern");
        throw DaemonException("Unknown exception in realDaemonModern");
    }
    return -1;
}

template <ProcessCallback Callback>
auto DaemonGuard::startDaemon(int argc, char** argv, const Callback& mainCb,
                              bool isDaemonParam) -> int {
    try {
        if (argv == nullptr && argc > 0) {
            throw DaemonException(
                "Invalid argument vector (nullptr with argc > 0)");
        }
        if (argc < 0) {
            LOG_F(WARNING, "Invalid argc value: {}, using 0 instead", argc);
            argc = 0;
        }

        std::atomic_store_explicit(&g_is_daemon, isDaemonParam,
                                   std::memory_order_relaxed);
        m_pidFilePath = g_pid_file_path;  // Use global default path unless
                                          // setPidFilePath is called

#ifdef _WIN32
        // Windows specific console handling for daemon mode.
        // This logic might be better placed inside the "daemon child" part of
        // realDaemon, after it's confirmed it is the daemon worker. However,
        // keeping original placement from daemon.cpp.
        if (g_is_daemon.load(std::memory_order_relaxed)) {
            // If it's meant to be a daemon, it might not need a console.
            // Or, if it needs one for logging, AllocConsole might be used.
            // The original code had AllocConsole here.
            // If FreeConsole() is called, freopen_s to CONOUT$ might fail or be
            // irrelevant. For a true Windows service/daemon, console
            // interaction is usually avoided. For a detached process that still
            // logs to a console it creates:
            if (GetConsoleWindow() == NULL) {  // Only if no console attached
                if (!AllocConsole()) {
                    LOG_F(WARNING,
                          "Failed to allocate console for daemon, error: {}",
                          GetLastError());
                } else {
                    FILE* fpstdout = nullptr;
                    FILE* fpstderr = nullptr;
                    if (freopen_s(&fpstdout, "CONOUT$", "w", stdout) != 0) {
                        LOG_F(ERROR,
                              "Failed to redirect stdout to new console");
                        // Potentially return -1 or throw
                    }
                    if (freopen_s(&fpstderr, "CONOUT$", "w", stderr) != 0) {
                        LOG_F(ERROR,
                              "Failed to redirect stderr to new console");
                        // Potentially return -1 or throw
                    }
                }
            }
        }
#endif

        if (!g_is_daemon.load(std::memory_order_relaxed)) {
            m_parentId =
                ProcessId::current();  // In non-daemon mode, parent is self.
            m_parentStartTime = time(nullptr);
            return realStart(argc, argv, mainCb);
        } else {
            // This will attempt to launch the daemon.
            // The parent part of realDaemon will return 0 and exit.
            // The child part of realDaemon (or the new process on Windows) will
            // execute mainCb.
            return realDaemon(argc, argv, mainCb);
        }
    } catch (const DaemonException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in startDaemon: {}", e.what());
        throw DaemonException(std::string("Exception in startDaemon: ") +
                              e.what());
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in startDaemon");
        throw DaemonException("Unknown exception in startDaemon");
    }
    return -1;
}

template <ModernProcessCallback Callback>
auto DaemonGuard::startDaemonModern(std::span<char*> args,
                                    const Callback& mainCb, bool isDaemonParam)
    -> int {
    try {
        if (args.empty() || args[0] == nullptr) {
            throw DaemonException(
                "Empty or invalid argument vector in startDaemonModern");
        }

        std::atomic_store_explicit(&g_is_daemon, isDaemonParam,
                                   std::memory_order_relaxed);
        m_pidFilePath = g_pid_file_path;

#ifdef _WIN32
        if (g_is_daemon.load(std::memory_order_relaxed)) {
            if (GetConsoleWindow() == NULL) {
                if (!AllocConsole()) {
                    LOG_F(WARNING,
                          "Failed to allocate console for modern daemon, "
                          "error: {}",
                          GetLastError());
                } else {
                    FILE* fpstdout = nullptr;
                    FILE* fpstderr = nullptr;
                    if (freopen_s(&fpstdout, "CONOUT$", "w", stdout) != 0) {
                        LOG_F(ERROR,
                              "Failed to redirect stdout to new console "
                              "(modern)");
                    }
                    if (freopen_s(&fpstderr, "CONOUT$", "w", stderr) != 0) {
                        LOG_F(ERROR,
                              "Failed to redirect stderr to new console "
                              "(modern)");
                    }
                }
            }
        }
#endif

        if (!g_is_daemon.load(std::memory_order_relaxed)) {
            m_parentId = ProcessId::current();
            m_parentStartTime = time(nullptr);
            return realStartModern(args, mainCb);
        } else {
            return realDaemonModern(args, mainCb);
        }
    } catch (const DaemonException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in startDaemonModern: {}", e.what());
        throw DaemonException(std::string("Exception in startDaemonModern: ") +
                              e.what());
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in startDaemonModern");
        throw DaemonException("Unknown exception in startDaemonModern");
    }
    return -1;
}

inline auto DaemonGuard::isRunning() const noexcept -> bool {
    if (!m_mainId.valid()) {
        return false;
    }
#ifdef _WIN32
    // m_mainId.id is GetCurrentProcess() pseudo-handle if set by
    // ProcessId::current() We need the DWORD PID to check if the process is
    // running.
    DWORD processIdToCheck = GetProcessId(m_mainId.id);
    if (processIdToCheck == 0) {  // GetProcessId failed
        LOG_F(WARNING,
              "isRunning: GetProcessId failed for handle {:p}, error: {}",
              (void*)m_mainId.id, GetLastError());
        return false;  // Cannot determine PID
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                  FALSE, processIdToCheck);
    if (hProcess == NULL) {
        // This can happen if the process has exited or due to insufficient
        // permissions. ERROR_INVALID_PARAMETER typically means the PID doesn't
        // exist. ERROR_ACCESS_DENIED means it exists but we can't open it. For
        // simplicity, if we can't open it, we might assume it's not "running"
        // from our perspective or inaccessible.
        if (GetLastError() !=
            ERROR_ACCESS_DENIED) {  // If not access denied, likely not running
            LOG_F(INFO,
                  "isRunning: OpenProcess failed for PID {}, error: {}. "
                  "Assuming not running.",
                  processIdToCheck, GetLastError());
        } else {
            LOG_F(WARNING,
                  "isRunning: OpenProcess failed for PID {} with "
                  "ACCESS_DENIED. Assuming running but inaccessible.",
                  processIdToCheck);
            return true;  // Process exists but we can't query further, assume
                          // running.
        }
        return false;
    }

    DWORD exitCode = 0;
    BOOL result = GetExitCodeProcess(hProcess, &exitCode);
    CloseHandle(hProcess);

    if (!result) {
        LOG_F(WARNING,
              "isRunning: GetExitCodeProcess failed for PID {}, error: {}",
              processIdToCheck, GetLastError());
        return false;  // Cannot determine status
    }
    return exitCode == STILL_ACTIVE;
#else  // Unix-like
    return kill(m_mainId.id, 0) == 0;
#endif
}

// Free functions
inline void signalHandler(int signum) noexcept {
    try {
        // Use flag to ensure cleanup and logging happen only once per signal
        // type if re-entrant
        static std::atomic<bool> s_is_shutting_down{false};
        bool already_shutting_down =
            s_is_shutting_down.exchange(true, std::memory_order_relaxed);

        if (!already_shutting_down) {
            LOG_F(INFO, "Received signal {} ({}), initiating shutdown...",
                  signum,
                  (signum == SIGTERM
                       ? "SIGTERM"
                       : (signum == SIGINT ? "SIGINT" : "Unknown Signal")));

            ProcessCleanupManager::cleanup();  // Perform cleanup actions like
                                               // removing PID file

            // Re-raise signal to default handler for proper termination,
            // or exit directly. Exiting directly is simpler here.
            // For a more robust solution, one might re-raise or use self-pipe
            // for async-safe actions.
            std::exit(signum == 0 ? EXIT_SUCCESS
                                  : 128 + signum);  // Exit with signal code
        } else {
            LOG_F(INFO, "Received signal {} during shutdown, ignoring.",
                  signum);
        }
    } catch (const std::exception& e) {
        // Loguru might not be safe here if it allocates or uses complex C++
        // features. A raw write to stderr or syslog might be safer in a signal
        // handler's catch block. For now, relying on LOG_F, assuming it's
        // reasonably safe or the risk is accepted.
        LOG_F(ERROR, "Exception in signalHandler: %s", e.what());
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in signalHandler.");
    }
    // If exit wasn't called, and to be absolutely safe for signal handlers:
    // _Exit(128 + signum); // or std::_Exit
}

inline bool registerSignalHandlers(std::span<const int> signals) noexcept {
    try {
        bool success = true;
        for (int sig : signals) {
#ifdef _WIN32
            // signal() on Windows is CRT specific. SetConsoleCtrlHandler is
            // better for CTRL_C_EVENT etc. For SIGTERM/SIGINT, signal() might
            // work for console apps.
            if (signal(sig, signalHandler) == SIG_ERR) {
                LOG_F(WARNING,
                      "Failed to register signal handler for signal {} on "
                      "Windows using CRT signal().",
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
            sigemptyset(&sa.sa_mask);  // Block no signals during handler
                                       // execution by default
            sa.sa_flags = SA_RESTART;  // Restart syscalls if possible

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
    } catch (...) {  // Should not happen as operations are low-level
        LOG_F(ERROR, "Unknown exception in registerSignalHandlers.");
        return false;
    }
}

inline bool isProcessBackground() noexcept {
#ifdef _WIN32
    // A process is considered background if it has no visible window or
    // console. GetConsoleWindow() == NULL checks if there's an associated
    // console. This is an approximation. True background services are managed
    // differently.
    return GetConsoleWindow() == NULL;
#else
    // On Unix, a common check is if the process group ID is different from
    // the terminal's foreground process group ID.
    // Or, if it's not attached to a TTY.
    int tty_fd = STDIN_FILENO;  // Check against stdin, could also use
                                // STDOUT_FILENO or STDERR_FILENO
    if (!isatty(tty_fd)) {
        return true;  // Not attached to a terminal
    }
    pid_t pgid = getpgrp();  // Get current process's group ID
    pid_t tty_pgid =
        tcgetpgrp(tty_fd);  // Get foreground process group ID of the terminal
    if (tty_pgid == -1) {
        // Error in tcgetpgrp, could mean no controlling terminal or other issue
        // Log this as a warning, and conservatively assume not background if
        // isatty was true.
        LOG_F(WARNING, "isProcessBackground: tcgetpgrp failed: %s",
              strerror(errno));
        return false;
    }
    return pgid != tty_pgid;
#endif
}

inline void writePidFile(
    const std::filesystem::path&
        filePath) {  // Default from g_pid_file_path used at call site if needed
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

#ifdef _WIN32
        DWORD pid_val = GetCurrentProcessId();
#else
        pid_t pid_val = getpid();
#endif
        ofs << pid_val;

        if (ofs.fail()) {  // Check after writing
            ofs.close();   // Attempt to close even if write failed
            throw DaemonException(std::format("Failed to write PID to file: {}",
                                              filePath.string()));
        }
        ofs.close();
        if (ofs.fail()) {  // Check after closing
            throw DaemonException(
                std::format("Failed to close PID file after writing: {}",
                            filePath.string()));
        }

        LOG_F(INFO, "Created PID file: {} with PID: {}", filePath.string(),
              pid_val);
        ProcessCleanupManager::registerPidFile(filePath);

    } catch (const std::filesystem::filesystem_error& e) {
        // Log and rethrow as DaemonException for consistency, or just rethrow
        LOG_F(ERROR, "Filesystem error in writePidFile for {}: {}",
              filePath.string(), e.what());
        throw DaemonException(
            std::format("Filesystem error writing PID file {}: {}",
                        filePath.string(), e.what()));
    } catch (const DaemonException&) {
        throw;                           // Re-throw existing DaemonException
    } catch (const std::exception& e) {  // Catch other std::exceptions
        LOG_F(ERROR, "Standard exception in writePidFile for {}: {}",
              filePath.string(), e.what());
        throw DaemonException(std::format("Failed to write PID file {}: {}",
                                          filePath.string(), e.what()));
    }
    // No catch(...) to let unknown exceptions propagate if not std::exception
    // or filesystem_error
}

inline auto checkPidFile(const std::filesystem::path& filePath) noexcept
    -> bool {  // Default from g_pid_file_path used at call site
    try {
        if (!std::filesystem::exists(filePath)) {
            return false;
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
        if (ifs.fail() || ifs.bad() ||
            pid_from_file <= 0) {  // Check failbit and badbit
            LOG_F(WARNING,
                  "PID file {} does not contain a valid PID. Content problem "
                  "or empty file.",
                  filePath.string());
            ifs.close();
            // Consider removing stale/invalid PID file here after logging
            // try { if (std::filesystem::exists(filePath))
            // std::filesystem::remove(filePath); } catch(...) {}
            return false;
        }
        ifs.close();

#ifdef _WIN32
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                      static_cast<DWORD>(pid_from_file));
        if (hProcess == NULL) {
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
            // try { if (std::filesystem::exists(filePath))
            // std::filesystem::remove(filePath); } catch(...) {} // Stale
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
#elif defined(__APPLE__) || defined(__linux__)
        if (kill(static_cast<pid_t>(pid_from_file), 0) == 0) {
            return true;  // Process exists
        } else {
            if (errno == ESRCH) {
                LOG_F(INFO,
                      "Process with PID {} from file {} does not exist "
                      "(ESRCH). Stale PID file?",
                      pid_from_file, filePath.string());
                // try { if (std::filesystem::exists(filePath))
                // std::filesystem::remove(filePath); } catch(...) {} // Stale
            } else if (errno == EPERM) {
                LOG_F(WARNING,
                      "No permission to signal PID {} from file {}, but "
                      "process likely exists (EPERM).",
                      pid_from_file, filePath.string());
                return true;  // Assume running due to permissions
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
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in checkPidFile for {}: {}", filePath.string(),
              e.what());
        return false;
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in checkPidFile for {}.",
              filePath.string());
        return false;
    }
}

inline void setDaemonRestartInterval(int seconds) {
    if (seconds <= 0) {
        throw std::invalid_argument(
            "Restart interval must be greater than zero");
    }
    std::lock_guard<std::mutex> lock(g_daemon_mutex);
    g_daemon_restart_interval = seconds;
    LOG_F(INFO, "Daemon restart interval set to {} seconds", seconds);
}

inline int getDaemonRestartInterval() noexcept {
    std::lock_guard<std::mutex> lock(g_daemon_mutex);
    return g_daemon_restart_interval;
}

}  // namespace atom::async

#endif  // ATOM_SERVER_DAEMON_HPP
