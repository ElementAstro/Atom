#include "priority.hpp"

#include <spdlog/spdlog.h>
#include <cstring>

#include "atom/error/exception.hpp"

namespace atom::system {

void PriorityManager::setProcessPriority(PriorityLevel level, int pid) {
    spdlog::info("Setting process priority to {} for PID {}",
                 static_cast<int>(level), pid);

#ifdef _WIN32
    DWORD priority = getPriorityFromLevel(level);
    HANDLE hProcess = pid == 0
                          ? GetCurrentProcess()
                          : OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (hProcess == nullptr) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open process: {}", error);
        THROW_RUNTIME_ERROR("Failed to open process: " + std::to_string(error));
    }

    if (SetPriorityClass(hProcess, priority) == 0) {
        DWORD error = GetLastError();
        if (pid != 0)
            CloseHandle(hProcess);
        spdlog::error("Failed to set process priority: {}", error);
        THROW_RUNTIME_ERROR("Failed to set process priority: " +
                            std::to_string(error));
    }

    if (pid != 0)
        CloseHandle(hProcess);
#else
    int priority = getPriorityFromLevel(level);
    if (setpriority(PRIO_PROCESS, pid, priority) == -1) {
        spdlog::error("Failed to set process priority: {}", strerror(errno));
        THROW_RUNTIME_ERROR("Failed to set process priority: " +
                            std::string(strerror(errno)));
    }
#endif
}

auto PriorityManager::getProcessPriority(int pid) -> PriorityLevel {
#ifdef _WIN32
    HANDLE hProcess = pid == 0
                          ? GetCurrentProcess()
                          : OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == nullptr) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open process: {}", error);
        THROW_RUNTIME_ERROR("Failed to open process: " + std::to_string(error));
    }

    DWORD priority = GetPriorityClass(hProcess);
    if (pid != 0)
        CloseHandle(hProcess);

    if (priority == 0) {
        DWORD error = GetLastError();
        spdlog::error("Failed to get process priority: {}", error);
        THROW_RUNTIME_ERROR("Failed to get process priority: " +
                            std::to_string(error));
    }

    return getLevelFromPriority(priority);
#else
    errno = 0;
    int priority = getpriority(PRIO_PROCESS, pid);
    if (priority == -1 && errno != 0) {
        spdlog::error("Failed to get process priority: {}", strerror(errno));
        THROW_RUNTIME_ERROR("Failed to get process priority: " +
                            std::string(strerror(errno)));
    }
    return getLevelFromPriority(priority);
#endif
}

void PriorityManager::setThreadPriority(
    PriorityLevel level, std::thread::native_handle_type thread) {
#ifdef _WIN32
    HANDLE hThread =
        thread == 0 ? GetCurrentThread() : reinterpret_cast<HANDLE>(thread);
    if (SetThreadPriority(hThread, getThreadPriorityFromLevel(level)) == 0) {
        DWORD error = GetLastError();
        spdlog::error("Failed to set thread priority: {}", error);
        THROW_RUNTIME_ERROR("Failed to set thread priority: " +
                            std::to_string(error));
    }
#else
    pthread_t threadId = thread == 0 ? pthread_self() : thread;
    int policy;
    struct sched_param param;

    if (pthread_getschedparam(threadId, &policy, &param) != 0) {
        spdlog::error("Failed to get current thread parameters: {}",
                      strerror(errno));
        THROW_RUNTIME_ERROR("Failed to get current thread parameters: " +
                            std::string(strerror(errno)));
    }

    param.sched_priority = getThreadPriorityFromLevel(level);
    if (pthread_setschedparam(threadId, policy, &param) != 0) {
        spdlog::error("Failed to set thread priority: {}", strerror(errno));
        THROW_RUNTIME_ERROR("Failed to set thread priority: " +
                            std::string(strerror(errno)));
    }
#endif
}

auto PriorityManager::getThreadPriority(std::thread::native_handle_type thread)
    -> PriorityLevel {
#ifdef _WIN32
    HANDLE hThread =
        thread == 0 ? GetCurrentThread() : reinterpret_cast<HANDLE>(thread);
    int priority = GetThreadPriority(hThread);
    if (priority == THREAD_PRIORITY_ERROR_RETURN) {
        DWORD error = GetLastError();
        spdlog::error("Failed to get thread priority: {}", error);
        THROW_RUNTIME_ERROR("Failed to get thread priority: " +
                            std::to_string(error));
    }
    return getLevelFromThreadPriority(priority);
#else
    pthread_t threadId = thread == 0 ? pthread_self() : thread;
    int policy;
    struct sched_param param;

    if (pthread_getschedparam(threadId, &policy, &param) != 0) {
        spdlog::error("Failed to get thread priority: {}", strerror(errno));
        THROW_RUNTIME_ERROR("Failed to get thread priority: " +
                            std::string(strerror(errno)));
    }
    return getLevelFromThreadPriority(param.sched_priority);
#endif
}

void PriorityManager::setThreadSchedulingPolicy(
    SchedulingPolicy policy, std::thread::native_handle_type thread) {
#ifdef _WIN32
    spdlog::error("Thread scheduling policy changes not supported on Windows");
    THROW_RUNTIME_ERROR(
        "Thread scheduling policy changes not supported on Windows");
#else
    pthread_t threadId = thread == 0 ? pthread_self() : thread;
    int nativePolicy;
    struct sched_param param;

    switch (policy) {
        case SchedulingPolicy::NORMAL:
            nativePolicy = SCHED_OTHER;
            break;
        case SchedulingPolicy::FIFO:
            nativePolicy = SCHED_FIFO;
            break;
        case SchedulingPolicy::ROUND_ROBIN:
            nativePolicy = SCHED_RR;
            break;
        default:
            THROW_INVALID_ARGUMENT("Invalid scheduling policy");
    }

    if (pthread_getschedparam(threadId, &nativePolicy, &param) != 0) {
        spdlog::error("Failed to get current thread parameters: {}",
                      strerror(errno));
        THROW_RUNTIME_ERROR("Failed to get current thread parameters: " +
                            std::string(strerror(errno)));
    }

    if (pthread_setschedparam(threadId, nativePolicy, &param) != 0) {
        spdlog::error("Failed to set thread scheduling policy: {}",
                      strerror(errno));
        THROW_RUNTIME_ERROR("Failed to set thread scheduling policy: " +
                            std::string(strerror(errno)));
    }
#endif
}

void PriorityManager::setProcessAffinity(const std::vector<int>& cpus,
                                         int pid) {
#ifdef _WIN32
    HANDLE hProcess = pid == 0
                          ? GetCurrentProcess()
                          : OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (hProcess == nullptr) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open process: {}", error);
        THROW_RUNTIME_ERROR("Failed to open process: " + std::to_string(error));
    }

    DWORD_PTR mask = 0;
    for (int cpu : cpus) {
        mask |= (1ULL << cpu);
    }

    if (SetProcessAffinityMask(hProcess, mask) == 0) {
        DWORD error = GetLastError();
        if (pid != 0)
            CloseHandle(hProcess);
        spdlog::error("Failed to set process affinity: {}", error);
        THROW_RUNTIME_ERROR("Failed to set process affinity: " +
                            std::to_string(error));
    }

    if (pid != 0)
        CloseHandle(hProcess);
#else
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int cpu : cpus) {
        CPU_SET(cpu, &cpuset);
    }

    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) == -1) {
        spdlog::error("Failed to set process affinity: {}", strerror(errno));
        THROW_RUNTIME_ERROR("Failed to set process affinity: " +
                            std::string(strerror(errno)));
    }
#endif
}

auto PriorityManager::getProcessAffinity(int pid) -> std::vector<int> {
    std::vector<int> cpus;

#ifdef _WIN32
    HANDLE hProcess = pid == 0
                          ? GetCurrentProcess()
                          : OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == nullptr) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open process: {}", error);
        THROW_RUNTIME_ERROR("Failed to open process: " + std::to_string(error));
    }

    DWORD_PTR processAffinityMask, systemAffinityMask;
    if (GetProcessAffinityMask(hProcess, &processAffinityMask,
                               &systemAffinityMask) == 0) {
        DWORD error = GetLastError();
        if (pid != 0)
            CloseHandle(hProcess);
        spdlog::error("Failed to get process affinity: {}", error);
        THROW_RUNTIME_ERROR("Failed to get process affinity: " +
                            std::to_string(error));
    }

    if (pid != 0)
        CloseHandle(hProcess);

    cpus.reserve(64);
    for (int i = 0; i < 64; ++i) {
        if ((processAffinityMask & (1ULL << i)) != 0) {
            cpus.push_back(i);
        }
    }
#else
    cpu_set_t cpuset;
    if (sched_getaffinity(pid, sizeof(cpu_set_t), &cpuset) == -1) {
        spdlog::error("Failed to get process affinity: {}", strerror(errno));
        THROW_RUNTIME_ERROR("Failed to get process affinity: " +
                            std::string(strerror(errno)));
    }

    cpus.reserve(CPU_SETSIZE);
    for (int i = 0; i < CPU_SETSIZE; ++i) {
        if (CPU_ISSET(i, &cpuset)) {
            cpus.push_back(i);
        }
    }
#endif
    return cpus;
}

void PriorityManager::startPriorityMonitor(
    int pid, const std::function<void(PriorityLevel)>& callback,
    std::chrono::milliseconds interval) {
    std::thread([pid, callback, interval]() {
        try {
            PriorityLevel lastPriority = getProcessPriority(pid);
            while (true) {
                std::this_thread::sleep_for(interval);
                PriorityLevel currentPriority = getProcessPriority(pid);
                if (currentPriority != lastPriority) {
                    callback(currentPriority);
                    lastPriority = currentPriority;
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Priority monitor error for PID {}: {}", pid,
                          e.what());
        }
    }).detach();
}

#ifdef _WIN32

auto PriorityManager::getPriorityFromLevel(PriorityLevel level) -> DWORD {
    switch (level) {
        case PriorityLevel::LOWEST:
            return IDLE_PRIORITY_CLASS;
        case PriorityLevel::BELOW_NORMAL:
            return BELOW_NORMAL_PRIORITY_CLASS;
        case PriorityLevel::NORMAL:
            return NORMAL_PRIORITY_CLASS;
        case PriorityLevel::ABOVE_NORMAL:
            return ABOVE_NORMAL_PRIORITY_CLASS;
        case PriorityLevel::HIGHEST:
            return HIGH_PRIORITY_CLASS;
        case PriorityLevel::REALTIME:
            return REALTIME_PRIORITY_CLASS;
        default:
            THROW_INVALID_ARGUMENT("Invalid priority level");
    }
}

auto PriorityManager::getLevelFromPriority(DWORD priority) -> PriorityLevel {
    switch (priority) {
        case IDLE_PRIORITY_CLASS:
            return PriorityLevel::LOWEST;
        case BELOW_NORMAL_PRIORITY_CLASS:
            return PriorityLevel::BELOW_NORMAL;
        case NORMAL_PRIORITY_CLASS:
            return PriorityLevel::NORMAL;
        case ABOVE_NORMAL_PRIORITY_CLASS:
            return PriorityLevel::ABOVE_NORMAL;
        case HIGH_PRIORITY_CLASS:
            return PriorityLevel::HIGHEST;
        case REALTIME_PRIORITY_CLASS:
            return PriorityLevel::REALTIME;
        default:
            THROW_INVALID_ARGUMENT("Invalid priority value");
    }
}

auto PriorityManager::getThreadPriorityFromLevel(PriorityLevel level) -> int {
    switch (level) {
        case PriorityLevel::LOWEST:
            return THREAD_PRIORITY_IDLE;
        case PriorityLevel::BELOW_NORMAL:
            return THREAD_PRIORITY_BELOW_NORMAL;
        case PriorityLevel::NORMAL:
            return THREAD_PRIORITY_NORMAL;
        case PriorityLevel::ABOVE_NORMAL:
            return THREAD_PRIORITY_ABOVE_NORMAL;
        case PriorityLevel::HIGHEST:
            return THREAD_PRIORITY_HIGHEST;
        case PriorityLevel::REALTIME:
            return THREAD_PRIORITY_TIME_CRITICAL;
        default:
            THROW_INVALID_ARGUMENT("Invalid priority level");
    }
}

auto PriorityManager::getLevelFromThreadPriority(int priority)
    -> PriorityLevel {
    switch (priority) {
        case THREAD_PRIORITY_IDLE:
            return PriorityLevel::LOWEST;
        case THREAD_PRIORITY_BELOW_NORMAL:
            return PriorityLevel::BELOW_NORMAL;
        case THREAD_PRIORITY_NORMAL:
            return PriorityLevel::NORMAL;
        case THREAD_PRIORITY_ABOVE_NORMAL:
            return PriorityLevel::ABOVE_NORMAL;
        case THREAD_PRIORITY_HIGHEST:
            return PriorityLevel::HIGHEST;
        case THREAD_PRIORITY_TIME_CRITICAL:
            return PriorityLevel::REALTIME;
        default:
            THROW_RUNTIME_ERROR("Unknown thread priority value");
    }
}

#else

auto PriorityManager::getPriorityFromLevel(PriorityLevel level) -> int {
    switch (level) {
        case PriorityLevel::LOWEST:
            return 19;
        case PriorityLevel::BELOW_NORMAL:
            return 10;
        case PriorityLevel::NORMAL:
            return 0;
        case PriorityLevel::ABOVE_NORMAL:
            return -10;
        case PriorityLevel::HIGHEST:
            return -20;
        case PriorityLevel::REALTIME:
            return sched_get_priority_max(SCHED_FIFO);
        default:
            THROW_INVALID_ARGUMENT("Invalid priority level");
    }
}

auto PriorityManager::getLevelFromPriority(int priority) -> PriorityLevel {
    static const int realtimePriority = sched_get_priority_max(SCHED_FIFO);

    if (priority == 19)
        return PriorityLevel::LOWEST;
    if (priority == 10)
        return PriorityLevel::BELOW_NORMAL;
    if (priority == 0)
        return PriorityLevel::NORMAL;
    if (priority == -10)
        return PriorityLevel::ABOVE_NORMAL;
    if (priority == -20)
        return PriorityLevel::HIGHEST;
    if (priority == realtimePriority)
        return PriorityLevel::REALTIME;

    THROW_INVALID_ARGUMENT("Invalid priority value");
}

auto PriorityManager::getThreadPriorityFromLevel(PriorityLevel level) -> int {
    return getPriorityFromLevel(level);
}

auto PriorityManager::getLevelFromThreadPriority(int priority)
    -> PriorityLevel {
    return getLevelFromPriority(priority);
}

#endif

}  // namespace atom::system
