/*
 * thread_utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-24

Description: Unified thread configuration utilities for cross-platform
             thread affinity and priority management

**************************************************/

#ifndef ATOM_ASYNC_EXECUTION_THREAD_UTILS_HPP
#define ATOM_ASYNC_EXECUTION_THREAD_UTILS_HPP

#include <thread>
#include <vector>

#include "atom/macro.hpp"

#if defined(ATOM_PLATFORM_WINDOWS)
#include <processthreadsapi.h>
#include "../../../cmake/WindowsCompat.hpp"
#elif defined(ATOM_PLATFORM_APPLE)
#include <dispatch/dispatch.h>
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#elif defined(ATOM_PLATFORM_LINUX)
#include <pthread.h>
#include <sched.h>
#endif

namespace atom::async {

/**
 * @brief Thread configuration utility class
 *
 * Provides cross-platform thread affinity and priority management.
 * This class consolidates all platform-specific thread configuration code.
 */
class ThreadUtils {
public:
    /**
     * @brief Thread priority levels
     */
    enum class Priority {
        Lowest,
        BelowNormal,
        Normal,
        AboveNormal,
        Highest,
        TimeCritical
    };

    /**
     * @brief CPU affinity modes for thread binding
     */
    enum class AffinityMode {
        None,        // No CPU affinity settings
        Sequential,  // Threads assigned to cores sequentially
        Spread,      // Threads spread across different cores
        CorePinned,  // Threads pinned to specified cores
        Automatic    // Automatically adjust (relies on OS scheduling)
    };

    /**
     * @brief Set the CPU affinity for the current thread
     * @param cpuId The CPU core ID to bind to
     * @return true if successful, false otherwise
     */
    static bool setThreadAffinity(int cpuId) noexcept {
        if (cpuId < 0) {
            return false;
        }

        const unsigned int numCores = std::thread::hardware_concurrency();
        if (numCores == 0) {
            return false;
        }

        // Normalize cpuId to valid range
        const unsigned int normalizedCpuId =
            static_cast<unsigned int>(cpuId) % numCores;

#if defined(ATOM_PLATFORM_WINDOWS)
        DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << normalizedCpuId);
        return SetThreadAffinityMask(GetCurrentThread(), mask) != 0;

#elif defined(ATOM_PLATFORM_LINUX)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(normalizedCpuId, &cpuset);
        return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t),
                                      &cpuset) == 0;

#elif defined(ATOM_PLATFORM_APPLE)
        // macOS uses thread affinity policy (soft affinity)
        thread_affinity_policy_data_t policy = {
            static_cast<integer_t>(normalizedCpuId)};
        return thread_policy_set(pthread_mach_thread_np(pthread_self()),
                                 THREAD_AFFINITY_POLICY,
                                 (thread_policy_t)&policy,
                                 THREAD_AFFINITY_POLICY_COUNT) == KERN_SUCCESS;
#else
        (void)normalizedCpuId;
        return false;
#endif
    }

    /**
     * @brief Set the CPU affinity based on thread ID and mode
     * @param threadId The logical thread ID
     * @param mode The affinity mode to use
     * @param pinnedCores Optional vector of specific cores for CorePinned mode
     * @return true if successful, false otherwise
     */
    static bool setThreadAffinityByMode(
        size_t threadId, AffinityMode mode,
        const std::vector<int>& pinnedCores = {}) noexcept {
        if (mode == AffinityMode::None || mode == AffinityMode::Automatic) {
            return true;  // No action needed
        }

        const unsigned int numCores = std::thread::hardware_concurrency();
        if (numCores <= 1) {
            return true;  // No need for affinity on single-core systems
        }

        unsigned int coreId = 0;

        switch (mode) {
            case AffinityMode::Sequential:
                coreId = static_cast<unsigned int>(threadId % numCores);
                break;

            case AffinityMode::Spread:
                // Spread threads across different cores (skip alternating)
                coreId = static_cast<unsigned int>((threadId * 2) % numCores);
                break;

            case AffinityMode::CorePinned:
                if (!pinnedCores.empty()) {
                    coreId = static_cast<unsigned int>(
                        pinnedCores[threadId % pinnedCores.size()]);
                } else {
                    coreId = static_cast<unsigned int>(threadId % numCores);
                }
                break;

            default:
                return true;
        }

        return setThreadAffinity(static_cast<int>(coreId));
    }

    /**
     * @brief Set the priority for the current thread
     * @param priority The priority level to set
     * @return true if successful, false otherwise
     */
    static bool setThreadPriority(Priority priority) noexcept {
#if defined(ATOM_PLATFORM_WINDOWS)
        int winPriority;
        switch (priority) {
            case Priority::Lowest:
                winPriority = THREAD_PRIORITY_LOWEST;
                break;
            case Priority::BelowNormal:
                winPriority = THREAD_PRIORITY_BELOW_NORMAL;
                break;
            case Priority::Normal:
                winPriority = THREAD_PRIORITY_NORMAL;
                break;
            case Priority::AboveNormal:
                winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
                break;
            case Priority::Highest:
                winPriority = THREAD_PRIORITY_HIGHEST;
                break;
            case Priority::TimeCritical:
                winPriority = THREAD_PRIORITY_TIME_CRITICAL;
                break;
            default:
                winPriority = THREAD_PRIORITY_NORMAL;
                break;
        }
        return SetThreadPriority(GetCurrentThread(), winPriority) != 0;

#elif defined(ATOM_PLATFORM_LINUX) || defined(ATOM_PLATFORM_APPLE)
        int policy;
        struct sched_param param{};

        if (pthread_getschedparam(pthread_self(), &policy, &param) != 0) {
            return false;
        }

        const int minPriority = sched_get_priority_min(policy);
        const int maxPriority = sched_get_priority_max(policy);
        const int priorityRange = maxPriority - minPriority;

        switch (priority) {
            case Priority::Lowest:
                param.sched_priority = minPriority;
                break;
            case Priority::BelowNormal:
                param.sched_priority = minPriority + priorityRange / 4;
                break;
            case Priority::Normal:
                param.sched_priority = minPriority + priorityRange / 2;
                break;
            case Priority::AboveNormal:
                param.sched_priority = maxPriority - priorityRange / 4;
                break;
            case Priority::Highest:
            case Priority::TimeCritical:
                param.sched_priority = maxPriority;
                break;
            default:
                param.sched_priority = minPriority + priorityRange / 2;
                break;
        }

        return pthread_setschedparam(pthread_self(), policy, &param) == 0;
#else
        (void)priority;
        return false;
#endif
    }

    /**
     * @brief Set the priority for a thread by its native handle
     * @param handle The native thread handle
     * @param priority The priority level to set
     * @return true if successful, false otherwise
     */
    static bool setThreadPriority(std::thread::native_handle_type handle,
                                  Priority priority) noexcept {
#if defined(ATOM_PLATFORM_WINDOWS)
        int winPriority;
        switch (priority) {
            case Priority::Lowest:
                winPriority = THREAD_PRIORITY_LOWEST;
                break;
            case Priority::BelowNormal:
                winPriority = THREAD_PRIORITY_BELOW_NORMAL;
                break;
            case Priority::Normal:
                winPriority = THREAD_PRIORITY_NORMAL;
                break;
            case Priority::AboveNormal:
                winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
                break;
            case Priority::Highest:
                winPriority = THREAD_PRIORITY_HIGHEST;
                break;
            case Priority::TimeCritical:
                winPriority = THREAD_PRIORITY_TIME_CRITICAL;
                break;
            default:
                winPriority = THREAD_PRIORITY_NORMAL;
                break;
        }
        return ::SetThreadPriority(reinterpret_cast<HANDLE>(handle),
                                   winPriority) != 0;

#elif defined(ATOM_PLATFORM_LINUX) || defined(ATOM_PLATFORM_APPLE)
        int policy;
        struct sched_param param{};

        if (pthread_getschedparam(handle, &policy, &param) != 0) {
            return false;
        }

        const int minPriority = sched_get_priority_min(policy);
        const int maxPriority = sched_get_priority_max(policy);
        const int priorityRange = maxPriority - minPriority;

        switch (priority) {
            case Priority::Lowest:
                param.sched_priority = minPriority;
                break;
            case Priority::BelowNormal:
                param.sched_priority = minPriority + priorityRange / 4;
                break;
            case Priority::Normal:
                param.sched_priority = minPriority + priorityRange / 2;
                break;
            case Priority::AboveNormal:
                param.sched_priority = maxPriority - priorityRange / 4;
                break;
            case Priority::Highest:
            case Priority::TimeCritical:
                param.sched_priority = maxPriority;
                break;
            default:
                param.sched_priority = minPriority + priorityRange / 2;
                break;
        }

        return pthread_setschedparam(handle, policy, &param) == 0;
#else
        (void)handle;
        (void)priority;
        return false;
#endif
    }

    /**
     * @brief Set a custom name for the current thread (for debugging)
     * @param name The thread name (max 15 chars on Linux/macOS)
     * @return true if successful, false otherwise
     */
    static bool setThreadName(const char* name) noexcept {
        if (!name) {
            return false;
        }

#if defined(ATOM_PLATFORM_WINDOWS) && _WIN32_WINNT >= 0x0602
        // Windows 8 and higher
        wchar_t wideName[16];
        size_t len = strlen(name);
        if (len > 15)
            len = 15;
        for (size_t i = 0; i < len; ++i) {
            wideName[i] = static_cast<wchar_t>(name[i]);
        }
        wideName[len] = L'\0';
        return SUCCEEDED(SetThreadDescription(GetCurrentThread(), wideName));

#elif defined(ATOM_PLATFORM_LINUX) || defined(ATOM_PLATFORM_APPLE)
        return pthread_setname_np(pthread_self(), name) == 0;
#else
        (void)name;
        return false;
#endif
    }

    /**
     * @brief Get the number of hardware threads available
     * @return Number of hardware threads, or 1 if detection fails
     */
    static unsigned int getHardwareConcurrency() noexcept {
        unsigned int cores = std::thread::hardware_concurrency();
        return cores > 0 ? cores : 1;
    }

    // Delete constructor - static utility class
    ThreadUtils() = delete;
    ~ThreadUtils() = delete;
    ThreadUtils(const ThreadUtils&) = delete;
    ThreadUtils& operator=(const ThreadUtils&) = delete;
};

// Backward compatibility alias
using ThreadConfig = ThreadUtils;

}  // namespace atom::async

#endif  // ATOM_ASYNC_EXECUTION_THREAD_UTILS_HPP
