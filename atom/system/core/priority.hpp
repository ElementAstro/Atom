#ifndef ATOM_SYSTEM_PRIORITY_HPP
#define ATOM_SYSTEM_PRIORITY_HPP

#include <chrono>
#include <functional>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace atom::system {

/**
 * @class PriorityManager
 * @brief Manages process and thread priorities and affinities with
 * cross-platform support.
 */
class PriorityManager {
public:
    /**
     * @enum PriorityLevel
     * @brief Defines various priority levels from lowest to realtime.
     */
    enum class PriorityLevel {
        LOWEST,
        BELOW_NORMAL,
        NORMAL,
        ABOVE_NORMAL,
        HIGHEST,
        REALTIME
    };

    /**
     * @enum SchedulingPolicy
     * @brief Defines scheduling policies for thread execution.
     */
    enum class SchedulingPolicy { NORMAL, FIFO, ROUND_ROBIN };

    /**
     * @brief Sets the priority of a process.
     * @param level The priority level to set
     * @param pid The process ID (0 for current process)
     */
    static void setProcessPriority(PriorityLevel level, int pid = 0);

    /**
     * @brief Gets the priority of a process.
     * @param pid The process ID (0 for current process)
     * @return The current priority level of the process
     */
    static auto getProcessPriority(int pid = 0) -> PriorityLevel;

    /**
     * @brief Sets the priority of a thread.
     * @param level The priority level to set
     * @param thread The native handle of the thread (0 for current thread)
     */
    static void setThreadPriority(PriorityLevel level,
                                  std::thread::native_handle_type thread = 0);

    /**
     * @brief Gets the priority of a thread.
     * @param thread The native handle of the thread (0 for current thread)
     * @return The current priority level of the thread
     */
    static auto getThreadPriority(std::thread::native_handle_type thread = 0)
        -> PriorityLevel;

    /**
     * @brief Sets the scheduling policy of a thread.
     * @param policy The scheduling policy to set
     * @param thread The native handle of the thread (0 for current thread)
     */
    static void setThreadSchedulingPolicy(
        SchedulingPolicy policy, std::thread::native_handle_type thread = 0);

    /**
     * @brief Sets the CPU affinity of a process.
     * @param cpus Vector of CPU indices to set affinity to
     * @param pid The process ID (0 for current process)
     */
    static void setProcessAffinity(const std::vector<int>& cpus, int pid = 0);

    /**
     * @brief Gets the CPU affinity of a process.
     * @param pid The process ID (0 for current process)
     * @return Vector of CPU indices the process is affinitized to
     */
    static auto getProcessAffinity(int pid = 0) -> std::vector<int>;

    /**
     * @brief Starts monitoring the priority of a process.
     * @param pid The process ID to monitor
     * @param callback Callback function called when priority changes
     * @param interval Check interval (default: 1 second)
     */
    static void startPriorityMonitor(
        int pid, const std::function<void(PriorityLevel)>& callback,
        std::chrono::milliseconds interval = std::chrono::seconds(1));

private:
#ifdef _WIN32
    static auto getPriorityFromLevel(PriorityLevel level) -> DWORD;
    static auto getLevelFromPriority(DWORD priority) -> PriorityLevel;
#else
    static auto getPriorityFromLevel(PriorityLevel level) -> int;
    static auto getLevelFromPriority(int priority) -> PriorityLevel;
#endif
    static auto getThreadPriorityFromLevel(PriorityLevel level) -> int;
    static auto getLevelFromThreadPriority(int priority) -> PriorityLevel;
};

}  // namespace atom::system

#endif  // ATOM_SYSTEM_PRIORITY_HPP
