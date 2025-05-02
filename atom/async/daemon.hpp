/*
 * daemon.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-11

Description: Daemon process implementation

**************************************************/

#ifndef ATOM_SERVER_DAEMON_HPP
#define ATOM_SERVER_DAEMON_HPP

#include <atomic>
#include <concepts>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <optional>
#include <source_location>  // C++20 feature
#include <span>             // C++20 feature
#include <stdexcept>
#include <string>
#include <string_view>  // More efficient string view

#ifdef __APPLE__
// macOS specific headers
#include <mach/mach_time.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace atom::async {

// Using std::string_view to optimize exception type
class DaemonException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;

    // Using std::source_location to record where the exception occurred
    explicit DaemonException(
        std::string_view what_arg,
        const std::source_location &location = std::source_location::current())
        : std::runtime_error(std::string(what_arg) + " [" +
                             location.file_name() + ":" +
                             std::to_string(location.line()) + "]") {}
};

// Process callback function concept, using std::span instead of char**
// parameters to provide a safer interface
template <typename T>
concept ProcessCallback = requires(T callback, int argc, char **argv) {
    { callback(argc, argv) } -> std::convertible_to<int>;
};

// Enhanced process callback function concept, supporting std::span interface
template <typename T>
concept ModernProcessCallback = requires(T callback, std::span<char *> args) {
    { callback(args) } -> std::convertible_to<int>;
};

// Platform-independent process identifier type
struct ProcessId {
#ifdef _WIN32
    HANDLE id = 0;
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
        return ProcessId{GetCurrentProcess()};
#else
        return ProcessId{getpid()};
#endif
    }

    // Check if the process ID is valid
    [[nodiscard]] constexpr bool valid() const noexcept {
#ifdef _WIN32
        return id != 0;
#else
        return id > 0;
#endif
    }

    // Reset to invalid process ID
    constexpr void reset() noexcept {
#ifdef _WIN32
        id = 0;
#else
        id = 0;
#endif
    }
};

// Class for managing process information
class DaemonGuard {
public:
    /**
     * @brief Default constructor.
     */
    DaemonGuard() noexcept = default;

    /**
     * @brief Destructor that ensures proper cleanup
     */
    ~DaemonGuard() noexcept;

    // Prevent copying
    DaemonGuard(const DaemonGuard &) = delete;
    DaemonGuard &operator=(const DaemonGuard &) = delete;

    // Allow moving

    /**
     * @brief Converts process information to a string.
     *
     * @return The process information as a string.
     */
    [[nodiscard]] auto toString() const noexcept -> std::string;

    /**
     * @brief Starts a child process to execute the actual task.
     *
     * @param argc The number of command line arguments.
     * @param argv An array of command line arguments.
     * @param mainCb The main callback function to be executed in the child
     * process.
     * @return The return value of the main callback function.
     * @throws DaemonException if process creation fails
     */
    template <ProcessCallback Callback>
    auto realStart(int argc, char **argv, const Callback &mainCb) -> int;

    /**
     * @brief Start a child process to execute the task using modern interface
     * (C++20)
     *
     * @param args Command line arguments as span view
     * @param mainCb The main callback function to be executed in the child
     * process
     * @return The return value of the main callback function
     * @throws DaemonException if process creation fails
     */
    template <ModernProcessCallback Callback>
    auto realStartModern(std::span<char *> args, const Callback &mainCb) -> int;

    /**
     * @brief Starts a child process to execute the actual task.
     *
     * @param argc The number of command line arguments.
     * @param argv An array of command line arguments.
     * @param mainCb The main callback function to be executed in the child
     * process.
     * @return The return value of the main callback function.
     * @throws DaemonException if daemon process creation fails
     */
    template <ProcessCallback Callback>
    auto realDaemon(int argc, char **argv, const Callback &mainCb) -> int;

    /**
     * @brief Start a daemon process to execute the task using modern interface
     * (C++20)
     *
     * @param args Command line arguments as span view
     * @param mainCb The main callback function to be executed in the child
     * process
     * @return The return value of the main callback function
     * @throws DaemonException if daemon process creation fails
     */
    template <ModernProcessCallback Callback>
    auto realDaemonModern(std::span<char *> args, const Callback &mainCb)
        -> int;

    /**
     * @brief Starts the process. If a daemon process needs to be created, it
     * will create the daemon process first.
     *
     * @param argc The number of command line arguments.
     * @param argv An array of command line arguments.
     * @param mainCb The main callback function to be executed.
     * @param isDaemon Determines if a daemon process should be created.
     * @return The return value of the main callback function.
     * @throws DaemonException if process creation fails
     */
    template <ProcessCallback Callback>
    auto startDaemon(int argc, char **argv, const Callback &mainCb,
                     bool isDaemon) -> int;

    /**
     * @brief Start a process or daemon process using modern interface (C++20)
     *
     * @param args Command line arguments as span view
     * @param mainCb The main callback function to be executed
     * @param isDaemon Determines if a daemon process should be created
     * @return The return value of the main callback function
     * @throws DaemonException if process creation fails
     */
    template <ModernProcessCallback Callback>
    auto startDaemonModern(std::span<char *> args, const Callback &mainCb,
                           bool isDaemon) -> int;

    /**
     * @brief Get the number of restart attempts
     *
     * @return Current restart count
     */
    [[nodiscard]] auto getRestartCount() const noexcept -> int {
        return m_restartCount.load(std::memory_order_relaxed);
    }

    /**
     * @brief Checks if the daemon is running
     *
     * @return true if daemon is running, false otherwise
     */
    [[nodiscard]] auto isRunning() const noexcept -> bool;

    /**
     * @brief Set the PID file path
     *
     * @param path Path for the PID file
     */
    void setPidFilePath(const std::filesystem::path &path) noexcept {
        m_pidFilePath = path;
    }

    /**
     * @brief Get the current PID file path
     *
     * @return Current path for the PID file, or empty if not set
     */
    [[nodiscard]] auto getPidFilePath() const noexcept
        -> std::optional<std::filesystem::path> {
        return m_pidFilePath;
    }

private:
    ProcessId m_parentId; /**< Parent process ID */
    ProcessId m_mainId;   /**< Child process ID */

    time_t m_parentStartTime = 0; /**< The start time of the parent process. */
    time_t m_mainStartTime = 0;   /**< The start time of the child process. */
    std::atomic<int> m_restartCount{0}; /**< The number of restarts. */
    std::optional<std::filesystem::path>
        m_pidFilePath; /**< Path to the PID file */
};

/**
 * @brief Signal handler function.
 *
 * @param signum The signal number.
 */
void signalHandler(int signum) noexcept;

/**
 * @brief Writes the process ID to a file.
 *
 * @param filePath Path to write the PID file (optional)
 * @throws std::filesystem::filesystem_error if file operation fails
 */
void writePidFile(const std::filesystem::path &filePath = "lithium-daemon");

/**
 * @brief Checks if the process ID file exists.
 *
 * @param filePath Path to the PID file (optional)
 * @return True if the process ID file exists and the process is running, false
 * otherwise.
 */
auto checkPidFile(
    const std::filesystem::path &filePath = "lithium-daemon") noexcept -> bool;

/**
 * @brief Set the restart interval for daemon processes
 *
 * @param seconds Interval in seconds
 * @throws std::invalid_argument if seconds is less than or equal to zero
 */
void setDaemonRestartInterval(int seconds);

/**
 * @brief Get the current daemon restart interval
 *
 * @return Interval in seconds
 */
[[nodiscard]] int getDaemonRestartInterval() noexcept;

/**
 * @brief Register signals to be monitored
 *
 * @param signals Array of signals to monitor
 * @return true if all signals were successfully registered, false otherwise
 */
bool registerSignalHandlers(std::span<const int> signals) noexcept;

/**
 * @brief Check if the current process is running in the background
 *
 * @return true if the process is running in the background, false otherwise
 */
[[nodiscard]] bool isProcessBackground() noexcept;

}  // namespace atom::async

#endif
