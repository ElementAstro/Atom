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
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace atom::async {

// 自定义异常类型
class DaemonException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// 进程回调函数概念
template <typename T>
concept ProcessCallback = requires(T callback, int argc, char **argv) {
    { callback(argc, argv) } -> std::convertible_to<int>;
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

    // 阻止拷贝
    DaemonGuard(const DaemonGuard &) = delete;
    DaemonGuard &operator=(const DaemonGuard &) = delete;

    // 允许移动
    DaemonGuard(DaemonGuard &&) noexcept = delete;
    DaemonGuard &operator=(DaemonGuard &&) noexcept = delete;

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

private:
#ifdef _WIN32
    HANDLE m_parentId = 0;
    HANDLE m_mainId = 0;
#else
    pid_t m_parentId = 0; /**< The parent process ID. */
    pid_t m_mainId = 0;   /**< The child process ID. */
#endif
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

}  // namespace atom::async

#endif
