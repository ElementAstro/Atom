/*
 * common.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-4

Description: System Information Module - CPU Common Header

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_CPU_COMMON_HPP
#define ATOM_SYSTEM_MODULE_CPU_COMMON_HPP

#include "../cpu.hpp"

#include <atomic>
#include <chrono>
#include <mutex>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <psapi.h>
#include <intrin.h>
#include <iphlpapi.h>
#include <pdh.h>
#include <powrprof.h>
#include <tlhelp32.h>
#include <wincon.h>
#include <comutil.h>
#include <wbemidl.h>
// clang-format on
#ifdef _MSC_VER
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "PowrProf.lib")
#endif
#elif defined(__linux__) || defined(__ANDROID__)
#include <dirent.h>
#include <limits.h>
#include <sys/statfs.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <csignal>
#include <fstream>
#include <set>
#elif defined(__APPLE__)
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <mach/mach_init.h>
#include <mach/task_info.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#elif defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

#include "atom/log/loguru.hpp"

namespace atom::system {

namespace {
// Cache variables with a validity duration
extern std::mutex g_cacheMutex;
extern std::chrono::steady_clock::time_point g_lastCacheRefresh;
extern const std::chrono::seconds g_cacheValidDuration;

// Cached CPU info
extern std::atomic<bool> g_cacheInitialized;
extern CpuInfo g_cpuInfoCache;

/**
 * @brief Converts a string to bytes
 * @param str String like "8K" or "4M"
 * @return Size in bytes
 */
size_t stringToBytes(const std::string& str);

/**
 * @brief Get vendor from CPU identifier string
 * @param vendorId CPU vendor ID string
 * @return CPU vendor enum
 */
CpuVendor getVendorFromString(const std::string& vendorId);

bool needsCacheRefresh();

}  // anonymous namespace

// Platform-specific function declarations - these will be implemented in
// platform-specific files

#ifdef _WIN32
// Windows-specific function declarations
#elif defined(__linux__)
// Linux-specific function declarations
#elif defined(__APPLE__)
// macOS-specific function declarations
#elif defined(__FreeBSD__)
// FreeBSD-specific function declarations
#endif

}  // namespace atom::system

#endif /* ATOM_SYSTEM_MODULE_CPU_COMMON_HPP */
