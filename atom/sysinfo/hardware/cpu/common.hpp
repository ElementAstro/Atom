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

#include "cpu.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

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
#include <wbemidl.h>
#ifdef _MSC_VER
#include <comutil.h>
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "PowrProf.lib")
#pragma comment(lib, "comsuppw.lib")
#else
// MinGW-compatible BSTR wrapper (replaces _bstr_t from comutil.h)
class bstr_t {
public:
    bstr_t(const wchar_t* str) : bstr_(SysAllocString(str)) {}
    bstr_t(const char* str) {
        if (str) {
            int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
            if (size > 0) {
                std::vector<wchar_t> wstr(size);
                MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr.data(), size);
                bstr_ = SysAllocString(wstr.data());
            } else {
                bstr_ = nullptr;
            }
        } else {
            bstr_ = nullptr;
        }
    }
    ~bstr_t() { if (bstr_) SysFreeString(bstr_); }

    bstr_t(const bstr_t&) = delete;
    bstr_t& operator=(const bstr_t&) = delete;

    operator BSTR() const { return bstr_; }
    BSTR GetBSTR() const { return bstr_; }

private:
    BSTR bstr_;
};

// Alias for compatibility
using _bstr_t = bstr_t;
#endif
// clang-format on
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

namespace atom::system {

// Cache variables (moved out of anonymous namespace)
extern std::mutex g_cacheMutex;
extern std::chrono::steady_clock::time_point g_lastCacheRefresh;
extern const std::chrono::seconds g_cacheValidDuration;

// Cached CPU info
extern std::atomic<bool> g_cacheInitialized;
extern CpuInfo g_cpuInfoCache;

// Forward declarations for functions implemented in common.cpp
size_t stringToBytes(const std::string& str);
CpuVendor getVendorFromString(const std::string& vendorId);
bool needsCacheRefresh();

}  // namespace atom::system

#endif /* ATOM_SYSTEM_MODULE_CPU_COMMON_HPP */
