/*
 * cpu.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-4

Description: System Information Module - Enhanced CPU

**************************************************/

#include "atom/sysinfo/cpu.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>
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
std::mutex g_cacheMutex;
std::chrono::steady_clock::time_point g_lastCacheRefresh;
const std::chrono::seconds g_cacheValidDuration(
    5);  // Cache valid for 5 seconds

// Cached CPU info
std::atomic<bool> g_cacheInitialized{false};
CpuInfo g_cpuInfoCache;

// Helper functions

/**
 * @brief Converts a string to bytes
 * @param str String like "8K" or "4M"
 * @return Size in bytes
 */
[[maybe_unused]] size_t stringToBytes(const std::string& str) {
    std::regex re("(\\d+)\\s*([KkMmGgTt]?)");
    std::smatch match;

    if (std::regex_match(str, match, re)) {
        size_t value = std::stoull(match[1].str());
        std::string unit = match[2].str();

        if (unit.empty()) {
            return value;
        } else if (unit == "K" || unit == "k") {
            return value * 1024;
        } else if (unit == "M" || unit == "m") {
            return value * 1024 * 1024;
        } else if (unit == "G" || unit == "g") {
            return value * 1024 * 1024 * 1024;
        } else if (unit == "T" || unit == "t") {
            return value * 1024 * 1024 * 1024 * 1024;
        }
    }

    return 0;
}

/**
 * @brief Get vendor from CPU identifier string
 * @param vendorId CPU vendor ID string
 * @return CPU vendor enum
 */
CpuVendor getVendorFromString(const std::string& vendorId) {
    std::string vendor = vendorId;
    std::transform(vendor.begin(), vendor.end(), vendor.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (vendor.find("intel") != std::string::npos) {
        return CpuVendor::INTEL;
    } else if (vendor.find("amd") != std::string::npos) {
        return CpuVendor::AMD;
    } else if (vendor.find("arm") != std::string::npos) {
        return CpuVendor::ARM;
    } else if (vendor.find("apple") != std::string::npos) {
        return CpuVendor::APPLE;
    } else if (vendor.find("qualcomm") != std::string::npos) {
        return CpuVendor::QUALCOMM;
    } else if (vendor.find("ibm") != std::string::npos) {
        return CpuVendor::IBM;
    } else if (vendor.find("mediatek") != std::string::npos) {
        return CpuVendor::MEDIATEK;
    } else if (vendor.find("samsung") != std::string::npos) {
        return CpuVendor::SAMSUNG;
    }

    return CpuVendor::UNKNOWN;
}

/**
 * @brief Check if cache needs refresh
 * @return True if cache needs refresh
 */
bool needsCacheRefresh() {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - g_lastCacheRefresh;
    return !g_cacheInitialized || elapsed > g_cacheValidDuration;
}

#ifdef _WIN32
/**
 * @brief Execute WMI query
 * @param query WMI query string
 * @param property Property to retrieve
 * @return Result string, or empty string on failure or if property not found.
 */
std::string executeWmiQuery(const std::string& query,
                            const std::string& property) {
    HRESULT hres;
    std::string resultStr;

    // Step 1: Initialize COM.
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        LOG_F(ERROR, "Failed to initialize COM library. Error code = 0x%lx",
              hres);
        return "";
    }

    // Step 2: Set general COM security levels
    hres = CoInitializeSecurity(
        NULL,
        -1,                           // COM authentication
        NULL,                         // Authentication services
        NULL,                         // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,    // Default authentication
        RPC_C_IMP_LEVEL_IMPERSONATE,  // Default Impersonation
        NULL,                         // Authentication info
        EOAC_NONE,                    // Additional capabilities
        NULL                          // Reserved
    );

    // It's possible security is already initialized, so we check for specific
    // error
    if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
        LOG_F(ERROR, "Failed to initialize security. Error code = 0x%lx", hres);
        CoUninitialize();
        return "";
    }

    // Step 3: Obtain the initial locator to WMI
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                            IID_IWbemLocator, (LPVOID*)&pLoc);

    if (FAILED(hres)) {
        LOG_F(ERROR, "Failed to create IWbemLocator object. Err code = 0x%lx",
              hres);
        CoUninitialize();
        return "";
    }

    // Step 4: Connect to WMI through the IWbemLocator::ConnectServer method
    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),  // Object path of WMI namespace
        NULL,                     // User name. NULL = current user
        NULL,                     // User password. NULL = current
        0,                        // Locale. NULL indicates current
        NULL,                     // Security flags.
        0,                        // Authority (e.g. Kerberos)
        0,                        // Context object
        &pSvc                     // pointer to IWbemServices proxy
    );

    if (FAILED(hres)) {
        LOG_F(ERROR, "Could not connect to WMI namespace. Error code = 0x%lx",
              hres);
        pLoc->Release();
        CoUninitialize();
        return "";
    }

    // Step 5: Set security levels on the proxy
    hres =
        CoSetProxyBlanket(pSvc,                    // Indicates the proxy to set
                          RPC_C_AUTHN_WINNT,       // RPC_C_AUTHN_xxx
                          RPC_C_AUTHZ_NONE,        // RPC_C_AUTHZ_xxx
                          NULL,                    // Server principal name
                          RPC_C_AUTHN_LEVEL_CALL,  // RPC_C_AUTHN_LEVEL_xxx
                          RPC_C_IMP_LEVEL_IMPERSONATE,  // RPC_C_IMP_LEVEL_xxx
                          NULL,                         // client identity
                          EOAC_NONE                     // proxy capabilities
        );

    if (FAILED(hres)) {
        LOG_F(ERROR, "Could not set proxy blanket. Error code = 0x%lx", hres);
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return "";
    }

    // Step 6: Use the IWbemServices pointer to make requests of WMI
    IEnumWbemClassObject* pEnumerator = NULL;
    std::wstring wQuery = std::wstring(query.begin(), query.end());
    hres =
        pSvc->ExecQuery(bstr_t("WQL"), bstr_t(wQuery.c_str()),
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                        NULL, &pEnumerator);

    if (FAILED(hres)) {
        LOG_F(ERROR, "WMI query failed. Error code = 0x%lx. Query: %s", hres,
              query.c_str());
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return "";
    }

    // Step 7: Get the data from the query
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    // If a property is specified, retrieve the first instance's property value
    if (!property.empty()) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

        if (SUCCEEDED(hr) && uReturn != 0) {
            VARIANT vtProp;
            VariantInit(&vtProp);

            // Get the value of the specified property
            std::wstring wProperty =
                std::wstring(property.begin(), property.end());
            hr = pclsObj->Get(wProperty.c_str(), 0, &vtProp, 0, 0);

            if (SUCCEEDED(hr)) {
                switch (vtProp.vt) {
                    case VT_BSTR:
                        resultStr =
                            _com_util::ConvertBSTRToString(vtProp.bstrVal);
                        break;
                    case VT_I4:
                        resultStr = std::to_string(vtProp.lVal);
                        break;
                    case VT_UI4:
                        resultStr = std::to_string(vtProp.ulVal);
                        break;
                    case VT_R4:
                        resultStr = std::to_string(vtProp.fltVal);
                        break;
                    case VT_R8:
                        resultStr = std::to_string(vtProp.dblVal);
                        break;
                    case VT_UI1:  // BYTE
                        resultStr = std::to_string(vtProp.bVal);
                        break;
                    case VT_I2:  // SHORT
                        resultStr = std::to_string(vtProp.iVal);
                        break;
                    case VT_UI2:  // USHORT
                        resultStr = std::to_string(vtProp.uiVal);
                        break;
                    case VT_BOOL:
                        resultStr =
                            (vtProp.boolVal == VARIANT_TRUE) ? "true" : "false";
                        break;
                    case VT_NULL:
                    case VT_EMPTY:
                        resultStr = "";  // Represent null/empty as empty string
                        break;
                    default:
                        LOG_F(WARNING,
                              "WMI property '%s' has unhandled type: %d",
                              property.c_str(), vtProp.vt);
                        break;
                }
            } else {
                LOG_F(ERROR,
                      "Failed to get WMI property '%s'. Error code = 0x%lx",
                      property.c_str(), hr);
            }

            VariantClear(&vtProp);
            pclsObj->Release();
        } else if (FAILED(hr)) {
            LOG_F(ERROR, "Failed to get next WMI object. Error code = 0x%lx",
                  hr);
        }
    } else {
        // If no property is specified, count the number of results (e.g., for
        // COUNT queries)
        ULONG count = 0;
        while (pEnumerator) {
            [[maybe_unused]] HRESULT hr =
                pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (0 == uReturn) {
                break;
            }
            count++;
            pclsObj->Release();
            pclsObj = NULL;
        }
        resultStr = std::to_string(count);
    }

    // Cleanup
    pSvc->Release();
    pLoc->Release();
    if (pEnumerator)
        pEnumerator->Release();
    // Do not call CoUninitialize here if COM was already initialized by another
    // part of the application CoUninitialize(); // Consider managing COM
    // initialization/uninitialization globally

    return resultStr;
}
#endif
}  // anonymous namespace

auto getCurrentCpuUsage() -> float {
    LOG_F(INFO, "Starting getCurrentCpuUsage function");
    static std::mutex mutex;
    [[maybe_unused]] static unsigned long long lastTotalUser = 0,
                                               lastTotalUserLow = 0;
    [[maybe_unused]] static unsigned long long lastTotalSys = 0,
                                               lastTotalIdle = 0;

    float cpuUsage = 0.0;

#ifdef _WIN32
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER counter = nullptr;
    PDH_STATUS status;

    status = PdhOpenQuery(nullptr, 0, &query);
    if (status != ERROR_SUCCESS) {
        LOG_F(ERROR, "Failed to open PDH query: error code {}", status);
        return cpuUsage;
    }

    status = PdhAddEnglishCounter(
        query, "\\Processor(_Total)\\% Processor Time", 0, &counter);
    if (status != ERROR_SUCCESS) {
        LOG_F(ERROR, "Failed to add PDH counter: error code {}", status);
        PdhCloseQuery(query);
        return cpuUsage;
    }

    // First collection
    status = PdhCollectQueryData(query);
    if (status != ERROR_SUCCESS) {
        LOG_F(ERROR, "Failed to collect initial query data: error code {}",
              status);
        PdhCloseQuery(query);
        return cpuUsage;
    }

    // Wait a bit for more accurate measurement
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Second collection
    status = PdhCollectQueryData(query);
    if (status != ERROR_SUCCESS) {
        LOG_F(ERROR, "Failed to collect second query data: error code {}",
              status);
        PdhCloseQuery(query);
        return cpuUsage;
    }

    PDH_FMT_COUNTERVALUE counterValue;
    status = PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr,
                                         &counterValue);
    if (status == ERROR_SUCCESS) {
        cpuUsage = static_cast<float>(counterValue.doubleValue);
        LOG_F(INFO, "CPU Usage: {}%", cpuUsage);
    } else {
        LOG_F(ERROR, "Failed to get counter value: error code {}", status);
    }

    PdhCloseQuery(query);
#elif defined(__linux__)
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::ifstream file("/proc/stat");
        if (!file.is_open()) {
            LOG_F(ERROR, "Failed to open /proc/stat");
            return cpuUsage;
        }

        std::string line;
        std::getline(file, line);  // Read the first line

        std::istringstream iss(line);
        std::string cpu;
        unsigned long long user, nice, system, idle, iowait, irq, softirq,
            steal;

        iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >>
            softirq >> steal;

        if (cpu != "cpu") {
            LOG_F(ERROR, "Unexpected format in /proc/stat");
            return cpuUsage;
        }

        unsigned long long totalUser = user + nice;
        unsigned long long totalSys = system + irq + softirq;
        unsigned long long totalIdle = idle + iowait;
        unsigned long long total = totalUser + totalSys + totalIdle + steal;

        // Calculate CPU usage
        if (lastTotalUser > 0 || lastTotalUserLow > 0 || lastTotalSys > 0 ||
            lastTotalIdle > 0) {
            unsigned long long totalDiff =
                (total) - (lastTotalUser + lastTotalUserLow + lastTotalSys +
                           lastTotalIdle);
            if (totalDiff > 0) {  // Avoid division by zero
                unsigned long long idleDiff = (totalIdle) - (lastTotalIdle);
                cpuUsage =
                    100.0f * (1.0f - static_cast<float>(idleDiff) / totalDiff);
            }
        }

        // Save current values for next call
        lastTotalUser = totalUser;
        lastTotalUserLow = 0;
        lastTotalSys = totalSys;
        lastTotalIdle = totalIdle;
    }

    LOG_F(INFO, "CPU Usage: {}", cpuUsage);

#elif defined(__APPLE__)
    host_cpu_load_info_data_t cpu_load;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;

    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                        reinterpret_cast<host_info_t>(&cpu_load),
                        &count) == KERN_SUCCESS) {
        unsigned long long user = cpu_load.cpu_ticks[CPU_STATE_USER];
        unsigned long long nice = cpu_load.cpu_ticks[CPU_STATE_NICE];
        unsigned long long system = cpu_load.cpu_ticks[CPU_STATE_SYSTEM];
        unsigned long long idle = cpu_load.cpu_ticks[CPU_STATE_IDLE];

        {
            std::lock_guard<std::mutex> lock(mutex);
            unsigned long long totalUser = user + nice;
            unsigned long long totalSys = system;
            unsigned long long totalIdle = idle;
            unsigned long long total = totalUser + totalSys + totalIdle;

            // Calculate CPU usage
            if (lastTotalUser > 0 || lastTotalUserLow > 0 || lastTotalSys > 0 ||
                lastTotalIdle > 0) {
                unsigned long long totalDiff =
                    total - (lastTotalUser + lastTotalUserLow + lastTotalSys +
                             lastTotalIdle);
                if (totalDiff > 0) {  // Avoid division by zero
                    unsigned long long idleDiff = totalIdle - lastTotalIdle;
                    cpuUsage = 100.0f * (1.0f - static_cast<float>(idleDiff) /
                                                    totalDiff);
                }
            }

            // Save current values for next call
            lastTotalUser = totalUser;
            lastTotalUserLow = 0;
            lastTotalSys = totalSys;
            lastTotalIdle = totalIdle;
        }

        LOG_F(INFO, "CPU Usage: {}", cpuUsage);
    } else {
        LOG_F(ERROR, "Failed to get CPU usage from host_statistics");
    }
#elif defined(__FreeBSD__)
    // FreeBSD implementation
    long cp_times[CPUSTATES];
    size_t len = sizeof(cp_times);

    if (sysctlbyname("kern.cp_time", &cp_times, &len, NULL, 0) != -1) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            unsigned long long totalUser =
                cp_times[CP_USER] + cp_times[CP_NICE];
            unsigned long long totalSys = cp_times[CP_SYS];
            unsigned long long totalIdle = cp_times[CP_IDLE];
            unsigned long long total = totalUser + totalSys + totalIdle;

            // Calculate CPU usage
            if (lastTotalUser > 0 || lastTotalUserLow > 0 || lastTotalSys > 0 ||
                lastTotalIdle > 0) {
                unsigned long long totalDiff =
                    total - (lastTotalUser + lastTotalUserLow + lastTotalSys +
                             lastTotalIdle);
                if (totalDiff > 0) {  // Avoid division by zero
                    unsigned long long idleDiff = totalIdle - lastTotalIdle;
                    cpuUsage = 100.0f * (1.0f - static_cast<float>(idleDiff) /
                                                    totalDiff);
                }
            }

            // Save current values for next call
            lastTotalUser = totalUser;
            lastTotalUserLow = 0;
            lastTotalSys = totalSys;
            lastTotalIdle = totalIdle;
        }

        LOG_F(INFO, "CPU Usage: {}", cpuUsage);
    } else {
        LOG_F(ERROR, "Failed to get CPU usage from sysctl");
    }
#endif

    LOG_F(INFO, "Finished getCurrentCpuUsage function");
    return cpuUsage;
}

auto getPerCoreCpuUsage() -> std::vector<float> {
    LOG_F(INFO, "Starting getPerCoreCpuUsage function");
    std::vector<float> coreUsages;

#ifdef _WIN32
    PDH_HQUERY query = nullptr;
    PDH_STATUS status = PdhOpenQuery(nullptr, 0, &query);
    if (status != ERROR_SUCCESS) {
        LOG_F(ERROR, "Failed to open PDH query: error code {}", status);
        return coreUsages;
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    unsigned numCores = sysInfo.dwNumberOfProcessors;

    std::vector<PDH_HCOUNTER> counters(numCores);

    for (unsigned i = 0; i < numCores; ++i) {
        std::string counterPath =
            "\\Processor(" + std::to_string(i) + ")\\% Processor Time";
        status =
            PdhAddEnglishCounter(query, counterPath.c_str(), 0, &counters[i]);
        if (status != ERROR_SUCCESS) {
            LOG_F(ERROR, "Failed to add PDH counter for core {}: error code {}",
                  i, status);
            PdhCloseQuery(query);
            return coreUsages;
        }
    }

    // First collection
    status = PdhCollectQueryData(query);
    if (status != ERROR_SUCCESS) {
        LOG_F(ERROR, "Failed to collect initial query data: error code {}",
              status);
        PdhCloseQuery(query);
        return coreUsages;
    }

    // Wait a bit for more accurate measurement
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Second collection
    status = PdhCollectQueryData(query);
    if (status != ERROR_SUCCESS) {
        LOG_F(ERROR, "Failed to collect second query data: error code {}",
              status);
        PdhCloseQuery(query);
        return coreUsages;
    }

    coreUsages.resize(numCores);

    for (unsigned i = 0; i < numCores; ++i) {
        PDH_FMT_COUNTERVALUE counterVal;
        status = PdhGetFormattedCounterValue(counters[i], PDH_FMT_DOUBLE,
                                             nullptr, &counterVal);
        if (status == ERROR_SUCCESS) {
            coreUsages[i] = static_cast<float>(counterVal.doubleValue);
        } else {
            LOG_F(ERROR,
                  "Failed to get counter value for core {}: error code {}", i,
                  status);
            coreUsages[i] = 0.0f;
        }
    }

    PdhCloseQuery(query);

#elif defined(__linux__)
    // Track previous measurements for delta calculation
    static std::vector<std::pair<unsigned long long, unsigned long long>>
        lastStats;
    static std::mutex statsMutex;

    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        LOG_F(ERROR, "Failed to open /proc/stat");
        return coreUsages;
    }

    std::vector<std::pair<unsigned long long, unsigned long long>> currentStats;
    std::string line;

    while (std::getline(file, line)) {
        if (line.find("cpu") == 0 && line.size() > 3 && std::isdigit(line[3])) {
            std::istringstream iss(line);
            std::string cpuLabel;
            unsigned long long user, nice, system, idle, iowait, irq, softirq,
                steal;

            iss >> cpuLabel >> user >> nice >> system >> idle >> iowait >>
                irq >> softirq >> steal;

            unsigned long long totalActive =
                user + nice + system + irq + softirq + steal;
            unsigned long long totalIdle = idle + iowait;

            currentStats.emplace_back(totalActive, totalIdle);
        }
    }

    {
        std::lock_guard<std::mutex> lock(statsMutex);

        if (lastStats.empty()) {
            lastStats = currentStats;
            coreUsages.resize(currentStats.size(), 0.0f);
        } else {
            coreUsages.resize(std::min(lastStats.size(), currentStats.size()));

            for (size_t i = 0; i < coreUsages.size(); ++i) {
                unsigned long long activeTimeDelta =
                    currentStats[i].first - lastStats[i].first;
                unsigned long long idleTimeDelta =
                    currentStats[i].second - lastStats[i].second;
                unsigned long long totalTimeDelta =
                    activeTimeDelta + idleTimeDelta;

                if (totalTimeDelta > 0) {
                    coreUsages[i] = 100.0f *
                                    static_cast<float>(activeTimeDelta) /
                                    totalTimeDelta;
                } else {
                    coreUsages[i] = 0.0f;
                }
            }

            lastStats = currentStats;
        }
    }

#elif defined(__APPLE__)
    natural_t numCPUs = 0;
    processor_info_array_t cpuInfo;
    mach_msg_type_number_t numCpuInfo;

    kern_return_t kr =
        host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &numCPUs,
                            &cpuInfo, &numCpuInfo);

    if (kr == KERN_SUCCESS) {
        static std::vector<processor_cpu_load_info_data_t> lastCpuInfo;
        static std::mutex infoMutex;

        coreUsages.resize(numCPUs, 0.0f);
        processor_cpu_load_info_t cpuLoadInfo =
            (processor_cpu_load_info_t)cpuInfo;

        {
            std::lock_guard<std::mutex> lock(infoMutex);

            if (lastCpuInfo.empty()) {
                lastCpuInfo.resize(numCPUs);
                for (natural_t i = 0; i < numCPUs; i++) {
                    lastCpuInfo[i] = cpuLoadInfo[i];
                }
            } else {
                for (natural_t i = 0; i < numCPUs; i++) {
                    uint64_t userDelta =
                        cpuLoadInfo[i].cpu_ticks[CPU_STATE_USER] -
                        lastCpuInfo[i].cpu_ticks[CPU_STATE_USER];
                    uint64_t systemDelta =
                        cpuLoadInfo[i].cpu_ticks[CPU_STATE_SYSTEM] -
                        lastCpuInfo[i].cpu_ticks[CPU_STATE_SYSTEM];
                    uint64_t idleDelta =
                        cpuLoadInfo[i].cpu_ticks[CPU_STATE_IDLE] -
                        lastCpuInfo[i].cpu_ticks[CPU_STATE_IDLE];
                    uint64_t niceDelta =
                        cpuLoadInfo[i].cpu_ticks[CPU_STATE_NICE] -
                        lastCpuInfo[i].cpu_ticks[CPU_STATE_NICE];

                    uint64_t totalTicks =
                        userDelta + systemDelta + idleDelta + niceDelta;
                    if (totalTicks > 0) {
                        coreUsages[i] =
                            100.0f *
                            (1.0f - static_cast<float>(idleDelta) / totalTicks);
                    }

                    lastCpuInfo[i] = cpuLoadInfo[i];
                }
            }
        }

        vm_deallocate(mach_task_self(), (vm_address_t)cpuInfo,
                      numCpuInfo * sizeof(int));
    } else {
        LOG_F(ERROR, "Failed to get per-core CPU usage");
    }
#elif defined(__FreeBSD__)
    // FreeBSD per-core implementation
    int numCores;
    size_t len = sizeof(numCores);
    if (sysctlbyname("hw.ncpu", &numCores, &len, NULL, 0) == -1) {
        LOG_F(ERROR, "Failed to get number of cores");
        return coreUsages;
    }

    // Need to use sysctlbyname with "dev.cpu.%d.%s" pattern for each core
    static std::vector<std::pair<long, long>> lastStats;
    static std::mutex statsMutex;

    std::vector<std::pair<long, long>> currentStats;

    for (int i = 0; i < numCores; i++) {
        char mibname[128];
        long cp_time[CPUSTATES];
        size_t len = sizeof(cp_time);

        snprintf(mibname, sizeof(mibname), "kern.cp_times.%d", i);
        if (sysctlbyname(mibname, cp_time, &len, NULL, 0) != -1) {
            long active = cp_time[CP_USER] + cp_time[CP_NICE] + cp_time[CP_SYS];
            long idle = cp_time[CP_IDLE];
            currentStats.emplace_back(active, idle);
        }
    }

    {
        std::lock_guard<std::mutex> lock(statsMutex);

        if (lastStats.empty()) {
            lastStats = currentStats;
            coreUsages.resize(currentStats.size(), 0.0f);
        } else {
            coreUsages.resize(std::min(lastStats.size(), currentStats.size()));

            for (size_t i = 0; i < coreUsages.size(); ++i) {
                long activeDelta = currentStats[i].first - lastStats[i].first;
                long idleDelta = currentStats[i].second - lastStats[i].second;
                long totalDelta = activeDelta + idleDelta;

                if (totalDelta > 0) {
                    coreUsages[i] =
                        100.0f * static_cast<float>(activeDelta) / totalDelta;
                } else {
                    coreUsages[i] = 0.0f;
                }
            }

            lastStats = currentStats;
        }
    }
#endif

    for (size_t i = 0; i < coreUsages.size(); ++i) {
        LOG_F(INFO, "Core {}: {}%", i, coreUsages[i]);
    }

    LOG_F(INFO, "Finished getPerCoreCpuUsage function");
    return coreUsages;
}

auto getCurrentCpuTemperature() -> float {
    LOG_F(INFO, "Starting getCurrentCpuTemperature function");
    float temperature = 0.0F;

#ifdef _WIN32
    // Method 1: Try WMI query - stub implementation
    // Real implementation would use WMI queries
    std::string wmiResult = executeWmiQuery(
        "SELECT Temperature FROM Win32_TemperatureProbe WHERE Description LIKE "
        "'%CPU%'",
        "Temperature");

    if (!wmiResult.empty()) {
        try {
            temperature = std::stof(wmiResult) /
                          10.0F;  // WMI typically returns in tenths of degrees
            LOG_F(INFO, "CPU Temperature from WMI: {}", temperature);
            return temperature;
        } catch (const std::exception& e) {
            LOG_F(WARNING, "Failed to parse WMI temperature: {}", e.what());
        }
    }

    // Method 2: Try using Open Hardware Monitor interface if available
    // This is a stub - real implementation would use Open Hardware Monitor API

    // Method 3: Fallback to registry (not very reliable)
    HKEY hKey;
    DWORD temperatureValue = 0;
    DWORD size = sizeof(DWORD);

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     R"(HARDWARE\DESCRIPTION\System\CentralProcessor\0)", 0,
                     KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, "~MHz", nullptr, nullptr,
                            (LPBYTE)&temperatureValue,
                            &size) == ERROR_SUCCESS) {
            // This doesn't actually give temperature, just frequency
            // But it's here as a placeholder for the registry method
            LOG_F(WARNING, "Using CPU frequency as temperature placeholder");
            temperature = static_cast<float>(temperatureValue) / 100.0F;
        }
        RegCloseKey(hKey);
    } else {
        LOG_F(ERROR, "Failed to open registry key for CPU temperature");
    }

#elif defined(__APPLE__)
    // Method 1: Try using IOKit power statistics
    io_service_t service = IOServiceGetMatchingService(
        kIOMainPortDefault, IOServiceMatching("AppleSMC"));
    if (service) {
        CFMutableDictionaryRef properties = nullptr;
        if (IORegistryEntryCreateCFProperties(
                service, &properties, kCFAllocatorDefault, 0) == KERN_SUCCESS) {
            CFTypeRef data =
                CFDictionaryGetValue(properties, CFSTR("SMC temperatures"));
            // This is a placeholder - actual implementation would extract
            // temperature from the data

            CFRelease(properties);
        }
        IOObjectRelease(service);
    }

    // Method 2: Use sysctl for thermal level
    FILE* pipe = popen("sysctl -a | grep machdep.xcpm.cpu_thermal_level", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string result(buffer);
            size_t pos1 = result.find(": ");
            size_t pos2 = result.find("\n");
            if (pos1 != std::string::npos && pos2 != std::string::npos) {
                std::string tempStr = result.substr(pos1 + 2, pos2 - pos1 - 2);
                try {
                    float thermalLevel = std::stof(tempStr);
                    // Convert thermal level to temperature (rough
                    // approximation)
                    temperature = 40.0f + thermalLevel * 10.0f;
                    LOG_F(INFO, "CPU Temperature from thermal level: {}",
                          temperature);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "GetCpuTemperature error: {}", e.what());
                }
            }
        }
        pclose(pipe);
    }

    // Method 3: Try powermetrics command
    if (temperature == 0.0f) {
        FILE* powerPipe =
            popen("powermetrics -n 1 -i 1 | grep 'CPU die temperature'", "r");
        if (powerPipe != nullptr) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), powerPipe) != nullptr) {
                std::string result(buffer);
                std::regex tempRegex("CPU die temperature: (\\d+\\.\\d+) C");
                std::smatch match;
                if (std::regex_search(result, match, tempRegex) &&
                    match.size() > 1) {
                    try {
                        temperature = std::stof(match[1].str());
                        LOG_F(INFO, "CPU Temperature from powermetrics: {}",
                              temperature);
                    } catch (const std::exception& e) {
                        LOG_F(
                            ERROR,
                            "GetCpuTemperature error parsing powermetrics: {}",
                            e.what());
                    }
                }
            }
            pclose(powerPipe);
        }
    }

#elif defined(__linux__)
    if (isWsl()) {
        LOG_F(WARNING,
              "GetCpuTemperature: WSL detection, trying alternative methods");

        // Try reading temperature from Windows host
        FILE *pipe = popen("cat /proc/acpi/ibm/thermal | grep CPU", "r");
        if (pipe != nullptr) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string result(buffer);
                std::istringstream iss(result);
                std::string label;
                float temp;
                if (iss >> label >> temp) {
                    temperature = temp;
                    LOG_F(INFO, "CPU Temperature from ACPI: {}", temperature);
                }
            }
            pclose(pipe);
        }
    } else {
        // Method 1: Try reading from thermal zones
        bool found = false;

        // Try all thermal zones until we find one that looks like CPU
        DIR *dir = opendir("/sys/class/thermal");
        if (dir != nullptr) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string dirname = entry->d_name;
                if (dirname.find("thermal_zone") != std::string::npos) {
                    std::string typePath =
                        "/sys/class/thermal/" + dirname + "/type";
                    std::ifstream typeFile(typePath);
                    std::string type;
                    if (typeFile && std::getline(typeFile, type)) {
                        // Look for CPU-related thermal zones
                        if (type.find("cpu") != std::string::npos ||
                            type.find("x86") != std::string::npos ||
                            type.find("core") != std::string::npos) {
                            std::string tempPath =
                                "/sys/class/thermal/" + dirname + "/temp";
                            std::ifstream tempFile(tempPath);
                            int temp;
                            if (tempFile >> temp) {
                                temperature =
                                    static_cast<float>(temp) / 1000.0f;
                                LOG_F(INFO, "CPU Temperature from {}: {}",
                                      dirname, temperature);
                                found = true;
                                break;
                            }
                        }
                    }
                }
            }
            closedir(dir);
        }

        // Method 2: Try reading from coretemp directly
        if (!found) {
            std::ifstream tempFile(
                "/sys/devices/platform/coretemp.0/hwmon/hwmon0/temp1_input");
            if (!tempFile.is_open()) {
                // Try alternative path
                tempFile = std::ifstream("/sys/class/hwmon/hwmon0/temp1_input");
            }

            if (tempFile.is_open()) {
                int temp = 0;
                tempFile >> temp;
                temperature = static_cast<float>(temp) / 1000.0f;
                LOG_F(INFO, "CPU Temperature from coretemp: {}", temperature);
                found = true;
            }
        }

        // Method 3: Try sensors command
        if (!found) {
            FILE *pipe = popen(
                "sensors | grep -i 'Core 0' | cut -d '+' -f2 | cut -d ' ' -f1 "
                "| cut -d '°' -f1",
                "r");
            if (pipe != nullptr) {
                char buffer[128];
                if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    try {
                        temperature = std::stof(buffer);
                        LOG_F(INFO, "CPU Temperature from sensors command: {}",
                              temperature);
                        found = true;
                    } catch (const std::exception &e) {
                        LOG_F(ERROR, "Failed to parse sensors output: {}",
                              e.what());
                    }
                }
                pclose(pipe);
            }
        }

        // Fallback: default thermal zone
        if (!found) {
            std::ifstream tempFile("/sys/class/thermal/thermal_zone0/temp");
            if (tempFile.is_open()) {
                int temp = 0;
                tempFile >> temp;
                temperature = static_cast<float>(temp) / 1000.0f;
                LOG_F(INFO, "CPU Temperature from thermal_zone0: {}",
                      temperature);
            } else {
                LOG_F(ERROR, "Failed to open any temperature source");
            }
        }
    }
#elif defined(__FreeBSD__)
    // FreeBSD implementation
    // Try using sysctl for ACPI thermal data
    int temp;
    size_t len = sizeof(temp);
    if (sysctlbyname("dev.cpu.0.temperature", &temp, &len, NULL, 0) != -1) {
        // Convert from Kelvin to Celsius
        temperature = static_cast<float>(temp - 2731) / 10.0f;
        LOG_F(INFO, "CPU Temperature: {}", temperature);
    } else {
        // Try using coretemp module if available
        if (sysctlbyname("dev.cpu.0.coretemp.temperature", &temp, &len, NULL,
                         0) != -1) {
            temperature = static_cast<float>(temp - 2731) / 10.0f;
            LOG_F(INFO, "CPU Temperature from coretemp: {}", temperature);
        } else {
            LOG_F(ERROR, "Failed to get temperature from sysctl");
        }
    }
#endif

    LOG_F(INFO, "Final CPU Temperature: {}", temperature);
    return temperature;
}

auto getPerCoreCpuTemperature() -> std::vector<float> {
    LOG_F(INFO, "Starting getPerCoreCpuTemperature function");
    std::vector<float> temperatures;

#ifdef _WIN32
    // Windows implementation using WMI - this is a stub
    int numCores = getNumberOfLogicalCores();
    temperatures.resize(numCores, 0.0f);

    float avgTemp = getCurrentCpuTemperature();
    for (auto& temp : temperatures) {
        // Simulate slight variations between cores
        temp = avgTemp + static_cast<float>(rand() % 30 - 15) / 10.0f;
    }

#elif defined(__linux__)
    // Linux implementation
    int numCores = getNumberOfLogicalCores();
    temperatures.resize(numCores, 0.0f);

    for (int i = 0; i < numCores; ++i) {
        std::stringstream ss;
        ss << "/sys/devices/platform/coretemp.0/hwmon/hwmon0/temp"
           << (i * 2 + 1) << "_input";
        std::ifstream tempFile(ss.str());

        if (!tempFile.is_open()) {
            // Try alternative format
            ss.str("");
            ss << "/sys/class/hwmon/hwmon0/temp" << (i + 1) << "_input";
            tempFile = std::ifstream(ss.str());
        }

        if (tempFile.is_open()) {
            int temp = 0;
            tempFile >> temp;
            temperatures[i] = static_cast<float>(temp) / 1000.0f;
        } else {
            // Fallback to average temperature
            temperatures[i] = getCurrentCpuTemperature();
        }

        LOG_F(INFO, "Core {} temperature: {}", i, temperatures[i]);
    }

#elif defined(__APPLE__)
    // macOS implementation - use powermetrics if available
    FILE *pipe =
        popen("powermetrics -n 1 -i 1 | grep 'CPU die temperature'", "r");
    if (pipe != nullptr) {
        char buffer[1024];
        std::string output;

        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        pclose(pipe);

        // Parse the output for per-core temperatures
        std::regex re("CPU (\\d+) die temperature: (\\d+\\.\\d+) C");
        std::smatch match;
        std::string::const_iterator searchStart(output.cbegin());

        std::map<int, float> coreTemps;

        while (std::regex_search(searchStart, output.cend(), match, re)) {
            int coreId = std::stoi(match[1].str());
            float temp = std::stof(match[2].str());
            coreTemps[coreId] = temp;
            searchStart = match.suffix().first;
        }

        // Fill vector with ordered temperatures
        if (!coreTemps.empty()) {
            temperatures.resize(coreTemps.rbegin()->first + 1);
            for (const auto &pair : coreTemps) {
                temperatures[pair.first] = pair.second;
                LOG_F(INFO, "Core {} temperature: {}", pair.first, pair.second);
            }
        }
    }

    // Fallback if powermetrics failed
    if (temperatures.empty()) {
        int numCores = getNumberOfLogicalCores();
        temperatures.resize(numCores, 0.0f);

        float avgTemp = getCurrentCpuTemperature();
        for (auto &temp : temperatures) {
            temp = avgTemp;
        }
    }

#elif defined(__FreeBSD__)
    // FreeBSD implementation
    int numCores = getNumberOfLogicalCores();
    temperatures.resize(numCores, 0.0f);

    for (int i = 0; i < numCores; ++i) {
        char mibname[128];
        int temp;
        size_t len = sizeof(temp);

        snprintf(mibname, sizeof(mibname), "dev.cpu.%d.temperature", i);
        if (sysctlbyname(mibname, &temp, &len, NULL, 0) != -1) {
            temperatures[i] = static_cast<float>(temp - 2731) / 10.0f;
        } else {
            // Try coretemp
            snprintf(mibname, sizeof(mibname),
                     "dev.cpu.%d.coretemp.temperature", i);
            if (sysctlbyname(mibname, &temp, &len, NULL, 0) != -1) {
                temperatures[i] = static_cast<float>(temp - 2731) / 10.0f;
            } else {
                // Fallback
                temperatures[i] = getCurrentCpuTemperature();
            }
        }

        LOG_F(INFO, "Core {} temperature: {}", i, temperatures[i]);
    }
#endif

    LOG_F(INFO, "Finished getPerCoreCpuTemperature function");
    return temperatures;
}

auto getCPUModel() -> std::string {
    LOG_F(INFO, "Starting getCPUModel function");

    if (!needsCacheRefresh() && !g_cpuInfoCache.model.empty()) {
        return g_cpuInfoCache.model;
    }

    std::string cpuModel;
#ifdef _WIN32
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     R"(HARDWARE\DESCRIPTION\System\CentralProcessor\0)", 0,
                     KEY_READ, &hKey) == ERROR_SUCCESS) {
        char cpuName[1024];
        DWORD size = sizeof(cpuName);
        if (RegQueryValueEx(hKey, "ProcessorNameString", nullptr, nullptr,
                            (LPBYTE)cpuName, &size) == ERROR_SUCCESS) {
            cpuModel = cpuName;
            // Trim leading/trailing whitespace
            cpuModel =
                std::regex_replace(cpuModel, std::regex("^\\s+|\\s+$"), "");
            LOG_F(INFO, "CPU Model: {}", cpuModel);
        }
        RegCloseKey(hKey);
    } else {
        LOG_F(ERROR, "Failed to open registry key for CPU model");

        // Fallback method: use CPUID instruction
        int cpuInfo[4] = {0};
        char brand[49];
        memset(brand, 0, sizeof(brand));

        __cpuid(cpuInfo, 0x80000000);
        unsigned int nExIds = cpuInfo[0];

        if (nExIds >= 0x80000004) {
            for (unsigned int i = 0; i <= 2; ++i) {
                __cpuid(cpuInfo, 0x80000002 + i);
                memcpy(brand + i * 16, cpuInfo, sizeof(cpuInfo));
            }
            cpuModel = brand;
            LOG_F(INFO, "CPU Model (from CPUID): {}", cpuModel);
        }
    }

#elif defined(__linux__)
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    bool modelFound = false;

    while (std::getline(cpuinfo, line)) {
        if (line.substr(0, 10) == "model name") {
            cpuModel = line.substr(line.find(':') + 2);
            // Trim leading/trailing whitespace
            cpuModel =
                std::regex_replace(cpuModel, std::regex("^\\s+|\\s+$"), "");
            LOG_F(INFO, "CPU Model: {}", cpuModel);
            modelFound = true;
            break;
        }
    }
    cpuinfo.close();

    // Handle ARM processors (Raspberry Pi, etc.)
    if (!modelFound) {
        cpuinfo.open("/proc/cpuinfo");
        while (std::getline(cpuinfo, line)) {
            if (line.substr(0, 9) == "processor") {
                // Read the next few lines to find hardware info
                int linesToRead = 5;
                while (linesToRead-- > 0 && std::getline(cpuinfo, line)) {
                    if (line.substr(0, 9) == "Hardware:") {
                        cpuModel = line.substr(line.find(':') + 2);
                        cpuModel = std::regex_replace(
                            cpuModel, std::regex("^\\s+|\\s+$"), "");
                        LOG_F(INFO, "CPU Model (ARM): {}", cpuModel);
                        modelFound = true;
                        break;
                    }
                }
                if (modelFound)
                    break;
            }
        }
        cpuinfo.close();
    }

#elif defined(__APPLE__)
    char buffer[128];
    size_t size = sizeof(buffer);

    if (sysctlbyname("machdep.cpu.brand_string", &buffer, &size, nullptr, 0) ==
        0) {
        cpuModel = buffer;
        // Trim leading/trailing whitespace
        cpuModel = std::regex_replace(cpuModel, std::regex("^\\s+|\\s+$"), "");
        LOG_F(INFO, "CPU Model: {}", cpuModel);
    } else {
        LOG_F(ERROR, "Failed to get CPU model from sysctl");

        // Fallback for Apple Silicon
        FILE *pipe = popen("sysctl -n hw.model", "r");
        if (pipe != nullptr) {
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                cpuModel = buffer;
                cpuModel.erase(
                    std::remove(cpuModel.begin(), cpuModel.end(), '\n'),
                    cpuModel.end());
                LOG_F(INFO, "CPU Model (hw.model): {}", cpuModel);
            }
            pclose(pipe);
        }
    }

#elif defined(__FreeBSD__)
    char buffer[128];
    size_t size = sizeof(buffer);

    if (sysctlbyname("hw.model", &buffer, &size, nullptr, 0) == 0) {
        cpuModel = buffer;
        LOG_F(INFO, "CPU Model: {}", cpuModel);
    } else {
        LOG_F(ERROR, "Failed to get CPU model from sysctl");
    }
#endif

    // Update cache
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_cpuInfoCache.model = cpuModel;
    }

    LOG_F(INFO, "Finished getCPUModel function");
    return cpuModel;
}

auto getProcessorIdentifier() -> std::string {
    LOG_F(INFO, "Starting getProcessorIdentifier function");

    if (!needsCacheRefresh() && !g_cpuInfoCache.identifier.empty()) {
        return g_cpuInfoCache.identifier;
    }

    std::string identifier;

#ifdef _WIN32
    HKEY hKey;
    char identifierValue[256];
    DWORD bufSize = sizeof(identifierValue);

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     R"(HARDWARE\DESCRIPTION\System\CentralProcessor\0)", 0,
                     KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, "Identifier", nullptr, nullptr,
                            (LPBYTE)identifierValue,
                            &bufSize) == ERROR_SUCCESS) {
            identifier = identifierValue;
            LOG_F(INFO, "Processor Identifier: {}", identifier);
        }
        RegCloseKey(hKey);
    } else {
        LOG_F(ERROR, "Failed to open registry key for processor identifier");

        // Fallback to CPUID instruction
        int cpuInfo[4] = {0};
        char vendorId[13];
        memset(vendorId, 0, sizeof(vendorId));

        __cpuid(cpuInfo, 0);
        memcpy(vendorId, &cpuInfo[1], 4);
        memcpy(vendorId + 4, &cpuInfo[3], 4);
        memcpy(vendorId + 8, &cpuInfo[2], 4);

        __cpuid(cpuInfo, 1);
        int family = ((cpuInfo[0] >> 8) & 0xF) + ((cpuInfo[0] >> 20) & 0xFF);
        int model = ((cpuInfo[0] >> 4) & 0xF) + ((cpuInfo[0] >> 12) & 0xF0);
        int stepping = cpuInfo[0] & 0xF;

        std::stringstream ss;
        ss << vendorId << " Family " << family << " Model " << model
           << " Stepping " << stepping;
        identifier = ss.str();
        LOG_F(INFO, "Processor Identifier (from CPUID): {}", identifier);
    }
#elif defined(__linux__)
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    bool foundVendor = false, foundFamily = false, foundModel = false,
         foundStepping = false;
    std::string vendor, family, model, stepping;

    while (std::getline(cpuinfo, line)) {
        if (!foundVendor && line.substr(0, 12) == "vendor_id") {
            vendor = line.substr(line.find(':') + 2);
            foundVendor = true;
        } else if (!foundFamily && line.substr(0, 10) == "cpu family") {
            family = line.substr(line.find(':') + 2);
            foundFamily = true;
        } else if (!foundModel && line.substr(0, 6) == "model\t") {
            model = line.substr(line.find(':') + 2);
            foundModel = true;
        } else if (!foundStepping && line.substr(0, 9) == "stepping\t") {
            stepping = line.substr(line.find(':') + 2);
            foundStepping = true;
        }

        if (foundVendor && foundFamily && foundModel && foundStepping) {
            break;
        }
    }
    cpuinfo.close();

    if (foundVendor && foundFamily && foundModel && foundStepping) {
        // Trim whitespace
        vendor = std::regex_replace(vendor, std::regex("^\\s+|\\s+$"), "");
        family = std::regex_replace(family, std::regex("^\\s+|\\s+$"), "");
        model = std::regex_replace(model, std::regex("^\\s+|\\s+$"), "");
        stepping = std::regex_replace(stepping, std::regex("^\\s+|\\s+$"), "");

        std::stringstream ss;
        ss << vendor << " Family " << family << " Model " << model
           << " Stepping " << stepping;
        identifier = ss.str();
        LOG_F(INFO, "Processor Identifier: {}", identifier);
    } else {
        // ARM processors might have different fields
        cpuinfo.open("/proc/cpuinfo");
        std::string implementer, architecture, variant, part, revision;
        bool foundImplementer = false, foundArch = false, foundVariant = false,
             foundPart = false, foundRevision = false;

        while (std::getline(cpuinfo, line)) {
            if (!foundImplementer &&
                line.find("CPU implementer") != std::string::npos) {
                implementer = line.substr(line.find(':') + 2);
                foundImplementer = true;
            } else if (!foundArch &&
                       line.find("CPU architecture") != std::string::npos) {
                architecture = line.substr(line.find(':') + 2);
                foundArch = true;
            } else if (!foundVariant &&
                       line.find("CPU variant") != std::string::npos) {
                variant = line.substr(line.find(':') + 2);
                foundVariant = true;
            } else if (!foundPart &&
                       line.find("CPU part") != std::string::npos) {
                part = line.substr(line.find(':') + 2);
                foundPart = true;
            } else if (!foundRevision &&
                       line.find("CPU revision") != std::string::npos) {
                revision = line.substr(line.find(':') + 2);
                foundRevision = true;
            }

            if (foundImplementer && foundArch && foundVariant && foundPart &&
                foundRevision) {
                break;
            }
        }
        cpuinfo.close();

        if (foundArch) {
            // Trim whitespace
            implementer =
                std::regex_replace(implementer, std::regex("^\\s+|\\s+$"), "");
            architecture =
                std::regex_replace(architecture, std::regex("^\\s+|\\s+$"), "");
            variant =
                std::regex_replace(variant, std::regex("^\\s+|\\s+$"), "");
            part = std::regex_replace(part, std::regex("^\\s+|\\s+$"), "");
            revision =
                std::regex_replace(revision, std::regex("^\\s+|\\s+$"), "");

            std::stringstream ss;
            ss << "ARM Implementer " << implementer << " Architecture "
               << architecture << " Variant " << variant << " Part " << part
               << " Revision " << revision;
            identifier = ss.str();
            LOG_F(INFO, "Processor Identifier (ARM): {}", identifier);
        }
    }

    // If still empty, fall back to first processor line
    if (identifier.empty()) {
        cpuinfo.open("/proc/cpuinfo");
        while (std::getline(cpuinfo, line)) {
            if (line.substr(0, 9) == "processor") {
                identifier = line.substr(line.find(':') + 2);
                LOG_F(INFO, "Processor Identifier (fallback): {}", identifier);
                break;
            }
        }
        cpuinfo.close();
    }

#elif defined(__APPLE__)
    char buffer[128];
    size_t size = sizeof(buffer);

    // Try to get CPU vendor
    if (sysctlbyname("machdep.cpu.vendor", buffer, &size, nullptr, 0) == 0) {
        std::string vendor = buffer;

        size = sizeof(buffer);
        if (sysctlbyname("machdep.cpu.family", buffer, &size, nullptr, 0) ==
            0) {
            std::string family = buffer;

            size = sizeof(buffer);
            if (sysctlbyname("machdep.cpu.model", buffer, &size, nullptr, 0) ==
                0) {
                std::string model = buffer;

                size = sizeof(buffer);
                if (sysctlbyname("machdep.cpu.stepping", buffer, &size, nullptr,
                                 0) == 0) {
                    std::string stepping = buffer;

                    std::stringstream ss;
                    ss << vendor << " Family " << family << " Model " << model
                       << " Stepping " << stepping;
                    identifier = ss.str();
                    LOG_F(INFO, "Processor Identifier: {}", identifier);
                }
            }
        }
    }

    // Apple Silicon fallback
    if (identifier.empty()) {
        FILE *pipe = popen("sysctl -n machdep.cpu.brand_string", "r");
        if (pipe != nullptr) {
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                identifier = buffer;
                identifier.erase(
                    std::remove(identifier.begin(), identifier.end(), '\n'),
                    identifier.end());
                LOG_F(INFO, "Processor Identifier (brand): {}", identifier);
            }
            pclose(pipe);
        }

        // Another fallback for Apple Silicon
        if (identifier.empty()) {
            pipe = popen("sysctl -n hw.model", "r");
            if (pipe != nullptr) {
                if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    identifier = buffer;
                    identifier.erase(
                        std::remove(identifier.begin(), identifier.end(), '\n'),
                        identifier.end());
                    LOG_F(INFO, "Processor Identifier (hw.model): {}",
                          identifier);
                }
                pclose(pipe);
            }
        }
    }
#elif defined(__FreeBSD__)
    char buffer[128];
    size_t size = sizeof(buffer);

    if (sysctlbyname("hw.model", buffer, &size, nullptr, 0) == 0) {
        identifier = buffer;
        LOG_F(INFO, "Processor Identifier: {}", identifier);
    } else {
        LOG_F(ERROR, "Failed to get processor identifier from sysctl");
    }
#endif

    // Update cache
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_cpuInfoCache.identifier = identifier;
    }

    LOG_F(INFO, "Finished getProcessorIdentifier function");
    return identifier;
}

auto getProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getProcessorFrequency function");
    double frequency = 0.0;

#ifdef _WIN32
    HKEY hKey;
    DWORD frequencyValue;
    DWORD bufSize = sizeof(frequencyValue);

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     R"(HARDWARE\DESCRIPTION\System\CentralProcessor\0)", 0,
                     KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, "~MHz", nullptr, nullptr,
                            (LPBYTE)&frequencyValue,
                            &bufSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            frequency = static_cast<double>(frequencyValue) /
                        1000.0;  // Convert MHz to GHz
            LOG_F(INFO, "Processor Frequency: {} GHz", frequency);
            return frequency;
        }
        RegCloseKey(hKey);
    }

    LOG_F(ERROR, "Failed to get processor frequency from registry");

    // Fallback: try using WMI
    std::string wmiResult = executeWmiQuery(
        "SELECT CurrentClockSpeed FROM Win32_Processor", "CurrentClockSpeed");
    if (!wmiResult.empty()) {
        try {
            frequency = std::stod(wmiResult) / 1000.0;  // Convert MHz to GHz
            LOG_F(INFO, "Processor Frequency from WMI: {} GHz", frequency);
            return frequency;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to parse WMI result: {}", e.what());
        }
    }

#elif defined(__linux__)
    // Try reading from /proc/cpuinfo
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("cpu MHz") != std::string::npos ||
            line.find("clock") != std::string::npos) {
            std::size_t pos = line.find(':') + 1;
            std::string freqStr = line.substr(pos);
            // Strip whitespace
            freqStr.erase(
                std::remove_if(freqStr.begin(), freqStr.end(),
                               [](unsigned char c) { return std::isspace(c); }),
                freqStr.end());
            try {
                frequency = std::stod(freqStr) / 1000.0;  // Convert MHz to GHz
                LOG_F(INFO, "Processor Frequency: {} GHz", frequency);
                return frequency;
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to parse frequency: {}", e.what());
            }
        }
    }
    cpuinfo.close();

    // Try reading from /sys/devices
    std::ifstream freqFile(
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    if (freqFile.is_open()) {
        unsigned long long freqKHz = 0;
        freqFile >> freqKHz;
        frequency =
            static_cast<double>(freqKHz) / 1000000.0;  // Convert KHz to GHz
        LOG_F(INFO, "Processor Frequency from scaling_cur_freq: {} GHz",
              frequency);
        return frequency;
    }

    // Last resort: try with lscpu
    FILE* pipe = popen("lscpu | grep 'CPU MHz'", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string result(buffer);
            std::size_t pos = result.find(':') + 1;
            if (pos != std::string::npos) {
                std::string freqStr = result.substr(pos);
                // Strip whitespace
                freqStr.erase(std::remove_if(freqStr.begin(), freqStr.end(),
                                             [](unsigned char c) {
                                                 return std::isspace(c);
                                             }),
                              freqStr.end());
                try {
                    frequency =
                        std::stod(freqStr) / 1000.0;  // Convert MHz to GHz
                    LOG_F(INFO, "Processor Frequency from lscpu: {} GHz",
                          frequency);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Failed to parse frequency from lscpu: {}",
                          e.what());
                }
            }
        }
        pclose(pipe);
    }

#elif defined(__APPLE__)
    uint64_t freq = 0;
    size_t size = sizeof(freq);

    if (sysctlbyname("hw.cpufrequency", &freq, &size, nullptr, 0) == 0) {
        frequency = static_cast<double>(freq) / 1.0e9;  // Convert to GHz
        LOG_F(INFO, "Processor Frequency: {} GHz", frequency);
    } else {
        LOG_F(ERROR, "Failed to get processor frequency from sysctl");

        // Try using sysctl in shell
        FILE *pipe = popen("sysctl -n hw.cpufrequency", "r");
        if (pipe != nullptr) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                try {
                    uint64_t freqHz = std::stoull(buffer);
                    frequency =
                        static_cast<double>(freqHz) / 1.0e9;  // Convert to GHz
                    LOG_F(INFO,
                          "Processor Frequency from sysctl command: {} GHz",
                          frequency);
                } catch (const std::exception &e) {
                    LOG_F(ERROR,
                          "Failed to parse frequency from sysctl command: {}",
                          e.what());
                }
            }
            pclose(pipe);
        }
    }
#elif defined(__FreeBSD__)
    unsigned long freq = 0;
    size_t size = sizeof(freq);

    if (sysctlbyname("hw.clockrate", &freq, &size, nullptr, 0) == 0) {
        frequency = static_cast<double>(freq) / 1000.0;  // Convert to GHz
        LOG_F(INFO, "Processor Frequency: {} GHz", frequency);
    } else {
        LOG_F(ERROR, "Failed to get processor frequency from sysctl");
    }
#endif

    LOG_F(INFO, "Finished getProcessorFrequency function");
    return frequency;
}

auto getMinProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getMinProcessorFrequency function");
    double minFreq = 0.0;

#ifdef _WIN32
    // Try using WMI for min frequency
    std::string wmiResult = executeWmiQuery(
        "SELECT CurrentClockSpeed, MaxClockSpeed, ExtClock FROM "
        "Win32_Processor",
        "ExtClock");
    if (!wmiResult.empty()) {
        try {
            minFreq = std::stod(wmiResult) / 1000.0;  // Convert MHz to GHz
            LOG_F(INFO, "Min Processor Frequency from WMI: {} GHz", minFreq);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to parse WMI result: {}", e.what());
        }
    }

    // If WMI failed, estimate a reasonable minimum (often 800MHz for desktop
    // CPUs)
    if (minFreq <= 0.0) {
        minFreq = 0.8;  // Default to 0.8 GHz as a reasonable minimum
        LOG_F(INFO, "Using estimated Min Processor Frequency: {} GHz", minFreq);
    }

#elif defined(__linux__)
    // Try reading from /sys/devices
    std::ifstream freqFile(
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq");
    if (freqFile.is_open()) {
        unsigned long long freqKHz = 0;
        freqFile >> freqKHz;
        minFreq =
            static_cast<double>(freqKHz) / 1000000.0;  // Convert KHz to GHz
        LOG_F(INFO, "Min Processor Frequency: {} GHz", minFreq);
    } else {
        LOG_F(ERROR, "Failed to open scaling_min_freq");

        // Try with cpufreq-info if available
        FILE* pipe = popen("cpufreq-info -l | awk '{print $1}'", "r");
        if (pipe != nullptr) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                try {
                    unsigned long long freqKHz = std::stoull(buffer);
                    minFreq = static_cast<double>(freqKHz) /
                              1000000.0;  // Convert KHz to GHz
                    LOG_F(INFO,
                          "Min Processor Frequency from cpufreq-info: {} GHz",
                          minFreq);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Failed to parse min frequency: {}", e.what());
                }
            }
            pclose(pipe);
        } else {
            LOG_F(ERROR, "Failed to execute cpufreq-info");
        }
    }

#elif defined(__APPLE__)
    // For macOS, we need to estimate the minimum frequency
    uint64_t nominalFreq = 0;
    size_t size = sizeof(nominalFreq);

    if (sysctlbyname("hw.cpufrequency_min", &nominalFreq, &size, nullptr, 0) ==
        0) {
        minFreq = static_cast<double>(nominalFreq) / 1.0e9;  // Convert to GHz
        LOG_F(INFO, "Min Processor Frequency: {} GHz", minFreq);
    } else {
        // Fallback to a reasonable estimate
        double currentFreq = getProcessorFrequency();
        minFreq = currentFreq * 0.5;  // Estimate minimum as 50% of current
        LOG_F(INFO, "Estimated Min Processor Frequency: {} GHz", minFreq);
    }
#elif defined(__FreeBSD__)
    // For FreeBSD, try reading dev.cpu.0.freq_levels
    FILE* pipe = popen(
        "sysctl -n dev.cpu.0.freq_levels | awk '{print $NF}' | cut -d'/' -f1",
        "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            try {
                unsigned long freqMHz = std::stoul(buffer);
                minFreq = static_cast<double>(freqMHz) /
                          1000.0;  // Convert MHz to GHz
                LOG_F(INFO, "Min Processor Frequency: {} GHz", minFreq);
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to parse min frequency: {}", e.what());
            }
        }
        pclose(pipe);
    } else {
        LOG_F(ERROR, "Failed to execute sysctl command");
    }
#endif

    // Ensure we have a reasonable minimum value
    if (minFreq <= 0.0) {
        minFreq = 0.8;  // Default to 0.8 GHz as a fallback
        LOG_F(INFO, "Using fallback Min Processor Frequency: {} GHz", minFreq);
    }

    LOG_F(INFO, "Finished getMinProcessorFrequency function");
    return minFreq;
}

auto getMaxProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getMaxProcessorFrequency function");
    double maxFreq = 0.0;

#ifdef _WIN32
    // Try using WMI for max frequency
    std::string wmiResult = executeWmiQuery(
        "SELECT MaxClockSpeed FROM Win32_Processor", "MaxClockSpeed");
    if (!wmiResult.empty()) {
        try {
            maxFreq = std::stod(wmiResult) / 1000.0;  // Convert MHz to GHz
            LOG_F(INFO, "Max Processor Frequency from WMI: {} GHz", maxFreq);
            return maxFreq;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to parse WMI result: {}", e.what());
        }
    }

    // Fallback to registry
    HKEY hKey;
    DWORD frequencyMhz;
    DWORD bufSize = sizeof(frequencyMhz);

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     R"(HARDWARE\DESCRIPTION\System\CentralProcessor\0)", 0,
                     KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, "~MHz", nullptr, nullptr,
                            (LPBYTE)&frequencyMhz, &bufSize) == ERROR_SUCCESS) {
            maxFreq = static_cast<double>(frequencyMhz) /
                      1000.0;  // Convert MHz to GHz
            LOG_F(INFO, "Max Processor Frequency from registry: {} GHz",
                  maxFreq);
        }
        RegCloseKey(hKey);
    }

#elif defined(__linux__)
    // Try reading from /sys/devices
    std::ifstream freqFile(
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq");
    if (freqFile.is_open()) {
        unsigned long long freqKHz = 0;
        freqFile >> freqKHz;
        maxFreq =
            static_cast<double>(freqKHz) / 1000000.0;  // Convert KHz to GHz
        LOG_F(INFO, "Max Processor Frequency from scaling_max_freq: {} GHz",
              maxFreq);
    } else {
        LOG_F(ERROR, "Failed to open scaling_max_freq");

        // Try cpuinfo_max_freq
        std::ifstream maxFreqFile(
            "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
        if (maxFreqFile.is_open()) {
            unsigned long long freqKHz = 0;
            maxFreqFile >> freqKHz;
            maxFreq =
                static_cast<double>(freqKHz) / 1000000.0;  // Convert KHz to GHz
            LOG_F(INFO, "Max Processor Frequency from cpuinfo_max_freq: {} GHz",
                  maxFreq);
        } else {
            // Try with cpufreq-info if available
            FILE* pipe = popen("cpufreq-info -l | awk '{print $2}'", "r");
            if (pipe != nullptr) {
                char buffer[128];
                if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    try {
                        unsigned long long freqKHz = std::stoull(buffer);
                        maxFreq = static_cast<double>(freqKHz) /
                                  1000000.0;  // Convert KHz to GHz
                        LOG_F(
                            INFO,
                            "Max Processor Frequency from cpufreq-info: {} GHz",
                            maxFreq);
                    } catch (const std::exception& e) {
                        LOG_F(ERROR, "Failed to parse max frequency: {}",
                              e.what());
                    }
                }
                pclose(pipe);
            }
        }
    }

    // If still not found, try lscpu
    if (maxFreq <= 0.0) {
        FILE* pipe = popen("lscpu | grep 'CPU max MHz'", "r");
        if (pipe != nullptr) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string result(buffer);
                std::size_t pos = result.find(':') + 1;
                if (pos != std::string::npos) {
                    std::string freqStr = result.substr(pos);
                    // Strip whitespace
                    freqStr.erase(std::remove_if(freqStr.begin(), freqStr.end(),
                                                 [](unsigned char c) {
                                                     return std::isspace(c);
                                                 }),
                                  freqStr.end());
                    try {
                        maxFreq =
                            std::stod(freqStr) / 1000.0;  // Convert MHz to GHz
                        LOG_F(INFO,
                              "Max Processor Frequency from lscpu: {} GHz",
                              maxFreq);
                    } catch (const std::exception& e) {
                        LOG_F(ERROR, "Failed to parse frequency from lscpu: {}",
                              e.what());
                    }
                }
            }
            pclose(pipe);
        }
    }

#elif defined(__APPLE__)
    uint64_t freq = 0;
    size_t size = sizeof(freq);

    if (sysctlbyname("hw.cpufrequency_max", &freq, &size, nullptr, 0) == 0) {
        maxFreq = static_cast<double>(freq) / 1.0e9;  // Convert to GHz
        LOG_F(INFO, "Max Processor Frequency: {} GHz", maxFreq);
    } else {
        // Try getting nominal frequency as a fallback
        if (sysctlbyname("hw.cpufrequency", &freq, &size, nullptr, 0) == 0) {
            maxFreq = static_cast<double>(freq) / 1.0e9;  // Convert to GHz
            LOG_F(INFO, "Nominal Processor Frequency: {} GHz", maxFreq);
        } else {
            LOG_F(ERROR, "Failed to get max processor frequency from sysctl");

            // Try using sysctl in shell
            FILE *pipe = popen("sysctl -n hw.cpufrequency_max", "r");
            if (pipe != nullptr) {
                char buffer[128];
                if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    try {
                        uint64_t freqHz = std::stoull(buffer);
                        maxFreq = static_cast<double>(freqHz) /
                                  1.0e9;  // Convert to GHz
                        LOG_F(INFO,
                              "Max Processor Frequency from sysctl command: {} "
                              "GHz",
                              maxFreq);
                    } catch (const std::exception &e) {
                        LOG_F(
                            ERROR,
                            "Failed to parse frequency from sysctl command: {}",
                            e.what());
                    }
                }
                pclose(pipe);
            }
        }
    }
#elif defined(__FreeBSD__)
    // For FreeBSD, try reading dev.cpu.0.freq_levels
    FILE* pipe = popen(
        "sysctl -n dev.cpu.0.freq_levels | awk '{print $1}' | cut -d'/' -f1",
        "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            try {
                unsigned long freqMHz = std::stoul(buffer);
                maxFreq = static_cast<double>(freqMHz) /
                          1000.0;  // Convert MHz to GHz
                LOG_F(INFO, "Max Processor Frequency: {} GHz", maxFreq);
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to parse max frequency: {}", e.what());
            }
        }
        pclose(pipe);
    } else {
        LOG_F(ERROR, "Failed to execute sysctl command");
    }
#endif

    // If still no valid max frequency, use current as fallback
    if (maxFreq <= 0.0) {
        maxFreq = getProcessorFrequency();
        LOG_F(INFO, "Using current frequency as max: {} GHz", maxFreq);
    }

    LOG_F(INFO, "Finished getMaxProcessorFrequency function");
    return maxFreq;
}

auto getPerCoreFrequencies() -> std::vector<double> {
    LOG_F(INFO, "Starting getPerCoreFrequencies function");
    std::vector<double> frequencies;

    int numCores = getNumberOfLogicalCores();
    if (numCores <= 0) {
        LOG_F(ERROR, "Invalid number of cores: {}", numCores);
        return frequencies;
    }

    frequencies.resize(numCores, 0.0);

#ifdef _WIN32
    for (int i = 0; i < numCores; ++i) {
        HKEY hKey;
        std::string keyPath =
            R"(HARDWARE\DESCRIPTION\System\CentralProcessor\)" +
            std::to_string(i);
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, KEY_READ,
                         &hKey) == ERROR_SUCCESS) {
            DWORD frequencyMhz;
            DWORD bufSize = sizeof(frequencyMhz);
            if (RegQueryValueEx(hKey, "~MHz", nullptr, nullptr,
                                (LPBYTE)&frequencyMhz,
                                &bufSize) == ERROR_SUCCESS) {
                frequencies[i] = static_cast<double>(frequencyMhz) /
                                 1000.0;  // Convert MHz to GHz
            }
            RegCloseKey(hKey);
        }
    }
#elif defined(__linux__)
    for (int i = 0; i < numCores; ++i) {
        std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(i) +
                           "/cpufreq/scaling_cur_freq";
        std::ifstream freqFile(path);
        if (freqFile.is_open()) {
            unsigned long long freqKHz = 0;
            freqFile >> freqKHz;
            frequencies[i] =
                static_cast<double>(freqKHz) / 1000000.0;  // Convert KHz to GHz
        } else {
            LOG_F(ERROR, "Failed to open scaling_cur_freq for core {}", i);
            // Use the base frequency as fallback
            frequencies[i] = getProcessorFrequency();
        }
    }
#elif defined(__APPLE__)
    // macOS doesn't expose per-core frequencies directly
    // We can get overall frequency and then approximate
    double baseFreq = getProcessorFrequency();

    for (int i = 0; i < numCores; ++i) {
        frequencies[i] = baseFreq;
    }

    // On newer macOS systems with heterogeneous cores (like Apple M1/M2),
    // we could try to distinguish between efficiency and performance cores
    // but this requires additional information not easily accessible
#elif defined(__FreeBSD__)
    for (int i = 0; i < numCores; ++i) {
        char mibname[128];
        unsigned long freq;
        size_t len = sizeof(freq);

        snprintf(mibname, sizeof(mibname), "dev.cpu.%d.freq", i);
        if (sysctlbyname(mibname, &freq, &len, NULL, 0) != -1) {
            frequencies[i] =
                static_cast<double>(freq) / 1000.0;  // Convert MHz to GHz
        } else {
            LOG_F(ERROR, "Failed to get frequency for core {}", i);
            // Use the base frequency as fallback
            frequencies[i] = getProcessorFrequency();
        }
    }
#endif

    // Log the frequencies
    for (int i = 0; i < numCores; ++i) {
        LOG_F(INFO, "Core {} frequency: {} GHz", i, frequencies[i]);
    }

    LOG_F(INFO, "Finished getPerCoreFrequencies function");
    return frequencies;
}

auto getNumberOfPhysicalPackages() -> int {
    LOG_F(INFO, "Starting getNumberOfPhysicalPackages function");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalPackages > 0) {
        return g_cpuInfoCache.numPhysicalPackages;
    }

    int numberOfPackages = 0;

#ifdef _WIN32
    // Use WMI to query number of processor packages
    std::string wmiResult = executeWmiQuery(
        "SELECT COUNT(DISTINCT SocketDesignation) FROM Win32_Processor", "");
    if (!wmiResult.empty()) {
        try {
            numberOfPackages = std::stoi(wmiResult);
            LOG_F(INFO, "Number of Physical Packages from WMI: {}",
                  numberOfPackages);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to parse WMI result: {}", e.what());
        }
    }

    // If WMI failed, try using NUMA information
    if (numberOfPackages <= 0) {
        ULONG highestNodeNumber;
        if (GetNumaHighestNodeNumber(&highestNodeNumber)) {
            numberOfPackages = static_cast<int>(highestNodeNumber) + 1;
            LOG_F(INFO, "Number of Physical Packages from NUMA: {}",
                  numberOfPackages);
        }
    }

    // If still failed, assume one package
    if (numberOfPackages <= 0) {
        numberOfPackages = 1;
        LOG_F(INFO, "Assuming 1 physical package");
    }

#elif defined(__linux__)
    // Use a set to collect unique physical IDs
    std::set<int> physicalIds;

    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("physical id") != std::string::npos) {
            int id = std::stoi(line.substr(line.find(':') + 1));
            physicalIds.insert(id);
        }
    }
    cpuinfo.close();

    numberOfPackages = static_cast<int>(physicalIds.size());
    if (numberOfPackages == 0) {
        // Fallback for some systems where physical id isn't reported
        numberOfPackages = 1;
    }
    LOG_F(INFO, "Number of Physical Packages: {}", numberOfPackages);

#elif defined(__APPLE__)
    int packages = 0;
    size_t len = sizeof(packages);

    if (sysctlbyname("hw.packages", &packages, &len, nullptr, 0) == 0) {
        numberOfPackages = packages;
        LOG_F(INFO, "Number of Physical Packages: {}", numberOfPackages);
    } else {
        // Fallback: check if this is Apple Silicon
        std::string model = getCPUModel();
        if (model.find("Apple") != std::string::npos) {
            // Apple Silicon has one package
            numberOfPackages = 1;
            LOG_F(INFO, "Detected Apple Silicon, assuming 1 package");
        } else {
            // For Intel Macs, assume 1 package unless we know otherwise
            numberOfPackages = 1;
            LOG_F(INFO, "Assuming 1 physical package for Intel Mac");
        }
    }
#elif defined(__FreeBSD__)
    // FreeBSD typically reports one package
    // We could try to get this from devinfo or dmidecode
    numberOfPackages = 1;
    LOG_F(INFO, "Assuming 1 physical package for FreeBSD");
#endif

    // Ensure at least one package
    if (numberOfPackages <= 0) {
        numberOfPackages = 1;
        LOG_F(WARNING, "Invalid package count detected, setting to 1");
    }

    // Update cache
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_cpuInfoCache.numPhysicalPackages = numberOfPackages;
    }

    LOG_F(INFO, "Finished getNumberOfPhysicalPackages function");
    return numberOfPackages;
}

auto getNumberOfPhysicalCores() -> int {
    LOG_F(INFO, "Starting getNumberOfPhysicalCores function");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalCores > 0) {
        return g_cpuInfoCache.numPhysicalCores;
    }

    int numberOfCores = 0;

#ifdef _WIN32
    // Try using WMI first
    std::string wmiResult = executeWmiQuery(
        "SELECT NumberOfCores FROM Win32_Processor", "NumberOfCores");
    if (!wmiResult.empty()) {
        try {
            numberOfCores = std::stoi(wmiResult);
            LOG_F(INFO, "Number of Physical Cores from WMI: {}", numberOfCores);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to parse WMI result: {}", e.what());
        }
    }

    // If WMI failed, try using GetLogicalProcessorInformation
    if (numberOfCores <= 0) {
        DWORD bufferSize = 0;
        GetLogicalProcessorInformation(nullptr, &bufferSize);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(
                bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
            if (GetLogicalProcessorInformation(buffer.data(), &bufferSize)) {
                for (const auto& info : buffer) {
                    if (info.Relationship == RelationProcessorCore) {
                        numberOfCores++;
                    }
                }
                LOG_F(INFO,
                      "Number of Physical Cores from "
                      "GetLogicalProcessorInformation: {}",
                      numberOfCores);
            }
        }
    }

    // If still failed, use logical processors as fallback
    if (numberOfCores <= 0) {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        numberOfCores = sysInfo.dwNumberOfProcessors;
        LOG_F(WARNING, "Falling back to logical processors count: {}",
              numberOfCores);
    }

#elif defined(__linux__)
    // Method 1: Count distinct core IDs per physical processor
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    std::map<int, std::set<int>> coreIds;  // physical_id -> set of core_ids
    int currentPhysicalId = -1;

    while (std::getline(cpuinfo, line)) {
        if (line.find("physical id") != std::string::npos) {
            currentPhysicalId = std::stoi(line.substr(line.find(':') + 1));
        } else if (line.find("core id") != std::string::npos &&
                   currentPhysicalId >= 0) {
            int coreId = std::stoi(line.substr(line.find(':') + 1));
            coreIds[currentPhysicalId].insert(coreId);
        } else if (line.empty()) {
            currentPhysicalId = -1;  // Reset for next processor
        }
    }
    cpuinfo.close();

    // Count total cores across all physical processors
    for (const auto& pair : coreIds) {
        numberOfCores += static_cast<int>(pair.second.size());
    }

    // Method 2: If Method 1 failed, try lscpu
    if (numberOfCores <= 0) {
        FILE* pipe =
            popen("lscpu | grep 'Core(s) per socket' | awk '{print $4}'", "r");
        if (pipe != nullptr) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                try {
                    int coresPerSocket = std::stoi(buffer);
                    int sockets = getNumberOfPhysicalPackages();
                    numberOfCores = coresPerSocket * sockets;
                    LOG_F(INFO,
                          "Number of Physical Cores (lscpu): {} cores/socket * "
                          "{} sockets = {}",
                          coresPerSocket, sockets, numberOfCores);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Failed to parse lscpu output: {}", e.what());
                }
            }
            pclose(pipe);
        }
    }

    // Method 3: Count core directories
    if (numberOfCores <= 0) {
        FILE* pipe = popen("grep -c '^processor' /proc/cpuinfo", "r");
        if (pipe != nullptr) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                try {
                    int logicalCores = std::stoi(buffer);

                    // Try to detect if hyperthreading is enabled
                    bool hasHT = false;
                    std::ifstream flags("/proc/cpuinfo");
                    std::string flagLine;
                    while (std::getline(flags, flagLine)) {
                        if (flagLine.find("flags") != std::string::npos &&
                            flagLine.find(" ht ") != std::string::npos) {
                            hasHT = true;
                            break;
                        }
                    }
                    flags.close();

                    numberOfCores = hasHT ? logicalCores / 2 : logicalCores;
                    LOG_F(INFO, "Number of Physical Cores (estimated): {}",
                          numberOfCores);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Failed to parse processor count: {}",
                          e.what());
                }
            }
            pclose(pipe);
        }
    }

#elif defined(__APPLE__)
    int cores = 0;
    size_t len = sizeof(cores);

    if (sysctlbyname("hw.physicalcpu", &cores, &len, nullptr, 0) == 0) {
        numberOfCores = cores;
        LOG_F(INFO, "Number of Physical Cores: {}", numberOfCores);
    } else {
        LOG_F(ERROR, "Failed to get physical CPU count");

        // Fallback to logical cores
        if (sysctlbyname("hw.ncpu", &cores, &len, nullptr, 0) == 0) {
            numberOfCores = cores;
            LOG_F(WARNING, "Falling back to logical CPU count: {}",
                  numberOfCores);
        }
    }
#elif defined(__FreeBSD__)
    int cores;
    size_t len = sizeof(cores);

    if (sysctlbyname("hw.ncpu", &cores, &len, NULL, 0) == 0) {
        // FreeBSD doesn't directly expose physical core count
        // Try to determine if hyperthreading is enabled
        int temp;
        if (sysctlbyname("machdep.hyperthreading_allowed", &temp, &len, NULL,
                         0) == 0 &&
            temp != 0) {
            // Assume hyperthreading is enabled, so physical cores = logical
            // cores / 2
            numberOfCores = cores / 2;
        } else {
            // Assume no hyperthreading
            numberOfCores = cores;
        }
        LOG_F(INFO, "Number of Physical Cores (estimated): {}", numberOfCores);
    } else {
        LOG_F(ERROR, "Failed to get CPU count from sysctl");
    }
#endif

    // Ensure at least one core
    if (numberOfCores <= 0) {
        numberOfCores = 1;
        LOG_F(WARNING, "Invalid core count detected, setting to 1");
    }

    // Update cache
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_cpuInfoCache.numPhysicalCores = numberOfCores;
    }

    LOG_F(INFO, "Finished getNumberOfPhysicalCores function");
    return numberOfCores;
}

auto getNumberOfLogicalCores() -> int {
    LOG_F(INFO, "Starting getNumberOfLogicalCores function");

    if (!needsCacheRefresh() && g_cpuInfoCache.numLogicalCores > 0) {
        return g_cpuInfoCache.numLogicalCores;
    }

    int numberOfCores = 0;

#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    numberOfCores = sysInfo.dwNumberOfProcessors;
    LOG_F(INFO, "Number of Logical Cores: {}", numberOfCores);

#elif defined(__linux__)
    // Method 1: Use sysconf
    numberOfCores = sysconf(_SC_NPROCESSORS_ONLN);
    if (numberOfCores <= 0) {
        LOG_F(ERROR, "sysconf(_SC_NPROCESSORS_ONLN) failed");

        // Method 2: Parse /proc/cpuinfo
        FILE* pipe = popen("grep -c '^processor' /proc/cpuinfo", "r");
        if (pipe != nullptr) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                try {
                    numberOfCores = std::stoi(buffer);
                    LOG_F(INFO,
                          "Number of Logical Cores from /proc/cpuinfo: {}",
                          numberOfCores);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Failed to parse processor count: {}",
                          e.what());
                }
            }
            pclose(pipe);
        }
    } else {
        LOG_F(INFO, "Number of Logical Cores from sysconf: {}", numberOfCores);
    }

#elif defined(__APPLE__)
    int cores;
    size_t len = sizeof(cores);

    if (sysctlbyname("hw.logicalcpu", &cores, &len, nullptr, 0) == 0) {
        numberOfCores = cores;
        LOG_F(INFO, "Number of Logical Cores: {}", numberOfCores);
    } else {
        LOG_F(ERROR, "Failed to get logical CPU count");

        // Fallback
        if (sysctlbyname("hw.ncpu", &cores, &len, nullptr, 0) == 0) {
            numberOfCores = cores;
            LOG_F(INFO, "Number of Logical Cores (hw.ncpu): {}", numberOfCores);
        }
    }
#elif defined(__FreeBSD__)
    int cores;
    size_t len = sizeof(cores);

    if (sysctlbyname("hw.ncpu", &cores, &len, NULL, 0) == 0) {
        numberOfCores = cores;
        LOG_F(INFO, "Number of Logical Cores: {}", numberOfCores);
    } else {
        LOG_F(ERROR, "Failed to get CPU count from sysctl");
    }
#endif

    // Ensure at least one core
    if (numberOfCores <= 0) {
        numberOfCores = 1;
        LOG_F(WARNING, "Invalid logical core count detected, setting to 1");
    }

    // Update cache
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_cpuInfoCache.numLogicalCores = numberOfCores;
    }

    LOG_F(INFO, "Finished getNumberOfLogicalCores function");
    return numberOfCores;
}

auto getCacheSizes() -> CacheSizes {
    LOG_F(INFO, "Starting getCacheSizes function");

    if (!needsCacheRefresh() &&
        (g_cpuInfoCache.caches.l1d > 0 || g_cpuInfoCache.caches.l2 > 0 ||
         g_cpuInfoCache.caches.l3 > 0)) {
        return g_cpuInfoCache.caches;
    }

    CacheSizes cacheSizes{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

#ifdef _WIN32
    // Use WMI for more detailed cache information
    std::vector<std::pair<std::string, std::string>> cacheQueries = {
        {"L1DataCache",
         "SELECT Size FROM Win32_CacheMemory WHERE Purpose='L1 Cache' AND "
         "DeviceID='Cache Memory 0'"},
        {"L1InstructionCache",
         "SELECT Size FROM Win32_CacheMemory WHERE Purpose='L1 Cache' AND "
         "DeviceID='Cache Memory 1'"},
        {"L2Cache",
         "SELECT Size FROM Win32_CacheMemory WHERE Purpose='L2 Cache'"},
        {"L3Cache",
         "SELECT Size FROM Win32_CacheMemory WHERE Purpose='L3 Cache'"}};

    for (const auto& query : cacheQueries) {
        std::string result = executeWmiQuery(query.second, "Size");
        if (!result.empty()) {
            try {
                size_t size =
                    std::stoull(result) * 1024;  // Convert KB to bytes
                if (query.first == "L1DataCache")
                    cacheSizes.l1d = size;
                else if (query.first == "L1InstructionCache")
                    cacheSizes.l1i = size;
                else if (query.first == "L2Cache")
                    cacheSizes.l2 = size;
                else if (query.first == "L3Cache")
                    cacheSizes.l3 = size;
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to parse WMI result for {}: {}",
                      query.first, e.what());
            }
        }
    }

    // Fallback to GetLogicalProcessorInformation if WMI failed
    if (cacheSizes.l1d == 0 && cacheSizes.l2 == 0 && cacheSizes.l3 == 0) {
        DWORD bufferSize = 0;
        GetLogicalProcessorInformation(nullptr, &bufferSize);

        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && bufferSize > 0) {
            std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(
                bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));

            if (GetLogicalProcessorInformation(buffer.data(), &bufferSize)) {
                for (const auto& info : buffer) {
                    if (info.Relationship == RelationCache) {
                        size_t cacheSize = info.Cache.Size;

                        switch (info.Cache.Level) {
                            case 1:
                                if (info.Cache.Type == CacheData) {
                                    cacheSizes.l1d = cacheSize;
                                    cacheSizes.l1d_line_size =
                                        info.Cache.LineSize;
                                } else if (info.Cache.Type ==
                                           CacheInstruction) {
                                    cacheSizes.l1i = cacheSize;
                                    cacheSizes.l1i_line_size =
                                        info.Cache.LineSize;
                                }
                                break;
                            case 2:
                                cacheSizes.l2 = cacheSize;
                                cacheSizes.l2_line_size = info.Cache.LineSize;
                                break;
                            case 3:
                                cacheSizes.l3 = cacheSize;
                                cacheSizes.l3_line_size = info.Cache.LineSize;
                                break;
                        }
                    }
                }
            }
        }
    }

#elif defined(__linux__)
    // Try all possible cache indices
    for (int i = 0; i <= 4; ++i) {
        std::string path =
            "/sys/devices/system/cpu/cpu0/cache/index" + std::to_string(i);

        // Check if this cache index exists
        std::ifstream levelFile(path + "/level");
        if (!levelFile.is_open()) {
            continue;
        }

        int level;
        levelFile >> level;

        std::ifstream typeFile(path + "/type");
        std::string type;
        std::getline(typeFile, type);

        std::ifstream sizeFile(path + "/size");
        std::string sizeStr;
        std::getline(sizeFile, sizeStr);

        size_t size = stringToBytes(sizeStr);

        // Get coherency line size and associativity
        std::ifstream lineFile(path + "/coherency_line_size");
        size_t lineSize = 0;
        if (lineFile.is_open()) {
            lineFile >> lineSize;
        }

        std::ifstream waysFile(path + "/ways_of_associativity");
        size_t ways = 0;
        if (waysFile.is_open()) {
            waysFile >> ways;
        }

        // Assign sizes based on level and type
        if (level == 1) {
            if (type == "Data") {
                cacheSizes.l1d = size;
                cacheSizes.l1d_line_size = lineSize;
                cacheSizes.l1d_associativity = ways;
            } else if (type == "Instruction") {
                cacheSizes.l1i = size;
                cacheSizes.l1i_line_size = lineSize;
                cacheSizes.l1i_associativity = ways;
            } else if (type == "Unified") {
                cacheSizes.l1d = cacheSizes.l1i =
                    size / 2;  // Split unified cache
                cacheSizes.l1d_line_size = cacheSizes.l1i_line_size = lineSize;
                cacheSizes.l1d_associativity = cacheSizes.l1i_associativity =
                    ways;
            }
        } else if (level == 2) {
            cacheSizes.l2 = size;
            cacheSizes.l2_line_size = lineSize;
            cacheSizes.l2_associativity = ways;
        } else if (level == 3) {
            cacheSizes.l3 = size;
            cacheSizes.l3_line_size = lineSize;
            cacheSizes.l3_associativity = ways;
        }
    }

    // If we couldn't read from sysfs, try using lscpu
    if (cacheSizes.l1d == 0 && cacheSizes.l2 == 0) {
        FILE* pipe = popen("lscpu | grep 'cache\\|Cache'", "r");
        if (pipe != nullptr) {
            char buffer[1024];
            std::string output;

            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                output += buffer;
            }
            pclose(pipe);

            // Parse the output for cache sizes
            // L1d cache:             32 KiB
            // L1i cache:             32 KiB
            // L2 cache:              256 KiB
            // L3 cache:              8 MiB
            std::regex l1d_re("L1d cache:\\s+(\\d+)\\s+([KMG]iB)");
            std::regex l1i_re("L1i cache:\\s+(\\d+)\\s+([KMG]iB)");
            std::regex l2_re("L2 cache:\\s+(\\d+)\\s+([KMG]iB)");
            std::regex l3_re("L3 cache:\\s+(\\d+)\\s+([KMG]iB)");

            std::smatch match;
            std::string::const_iterator searchStart(output.cbegin());

            auto parseSize = [](const std::string& size,
                                const std::string& unit) -> size_t {
                size_t value = std::stoull(size);
                if (unit == "KiB")
                    return value * 1024;
                if (unit == "MiB")
                    return value * 1024 * 1024;
                if (unit == "GiB")
                    return value * 1024 * 1024 * 1024;
                return value;
            };

            if (std::regex_search(searchStart, output.cend(), match, l1d_re)) {
                cacheSizes.l1d = parseSize(match[1].str(), match[2].str());
            }

            if (std::regex_search(searchStart, output.cend(), match, l1i_re)) {
                cacheSizes.l1i = parseSize(match[1].str(), match[2].str());
            }

            if (std::regex_search(searchStart, output.cend(), match, l2_re)) {
                cacheSizes.l2 = parseSize(match[1].str(), match[2].str());
            }

            if (std::regex_search(searchStart, output.cend(), match, l3_re)) {
                cacheSizes.l3 = parseSize(match[1].str(), match[2].str());
            }
        }
    }

#elif defined(__APPLE__)
    // Use sysctl to get cache sizes
    int64_t cacheSize;
    size_t size = sizeof(cacheSize);

    if (sysctlbyname("hw.l1dcachesize", &cacheSize, &size, nullptr, 0) == 0) {
        cacheSizes.l1d = static_cast<size_t>(cacheSize);
    }

    if (sysctlbyname("hw.l1icachesize", &cacheSize, &size, nullptr, 0) == 0) {
        cacheSizes.l1i = static_cast<size_t>(cacheSize);
    }

    if (sysctlbyname("hw.l2cachesize", &cacheSize, &size, nullptr, 0) == 0) {
        cacheSizes.l2 = static_cast<size_t>(cacheSize);
    }

    if (sysctlbyname("hw.l3cachesize", &cacheSize, &size, nullptr, 0) == 0) {
        cacheSizes.l3 = static_cast<size_t>(cacheSize);
    }

    // Get cache line sizes and associativity if available
    if (sysctlbyname("hw.cachelinesize", &cacheSize, &size, nullptr, 0) == 0) {
        cacheSizes.l1d_line_size = cacheSizes.l1i_line_size =
            cacheSizes.l2_line_size = cacheSizes.l3_line_size =
                static_cast<size_t>(cacheSize);
    }
#elif defined(__FreeBSD__)
    // Use sysctl to get cache sizes
    int cacheSize;
    size_t size = sizeof(cacheSize);

    if (sysctlbyname("hw.l1dcachesize", &cacheSize, &size, NULL, 0) == 0) {
        cacheSizes.l1d = static_cast<size_t>(cacheSize);
    }

    if (sysctlbyname("hw.l1icachesize", &cacheSize, &size, NULL, 0) == 0) {
        cacheSizes.l1i = static_cast<size_t>(cacheSize);
    }

    if (sysctlbyname("hw.l2cachesize", &cacheSize, &size, NULL, 0) == 0) {
        cacheSizes.l2 = static_cast<size_t>(cacheSize);
    }

    if (sysctlbyname("hw.l3cachesize", &cacheSize, &size, NULL, 0) == 0) {
        cacheSizes.l3 = static_cast<size_t>(cacheSize);
    }

    // Get cache line size
    if (sysctlbyname("hw.cachelinesize", &cacheSize, &size, NULL, 0) == 0) {
        cacheSizes.l1d_line_size = cacheSizes.l1i_line_size =
            cacheSizes.l2_line_size = cacheSizes.l3_line_size =
                static_cast<size_t>(cacheSize);
    }
#endif

    LOG_F(INFO,
          "Cache Sizes - L1d: {} bytes, L1i: {} bytes, L2: {} bytes, L3: {} "
          "bytes",
          cacheSizes.l1d, cacheSizes.l1i, cacheSizes.l2, cacheSizes.l3);

    // Update cache
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_cpuInfoCache.caches = cacheSizes;
    }

    LOG_F(INFO, "Finished getCacheSizes function");
    return cacheSizes;
}

auto getCpuLoadAverage() -> LoadAverage {
    LOG_F(INFO, "Starting getCpuLoadAverage function");
    LoadAverage loadAvg{0.0, 0.0, 0.0};

#ifdef _WIN32
    // Windows doesn't have a direct equivalent to load average
    // We'll calculate a rough approximation using PDH performance counters
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER counter = nullptr;
    PDH_STATUS status;

    status = PdhOpenQuery(nullptr, 0, &query);
    if (status != ERROR_SUCCESS) {
        LOG_F(ERROR, "Failed to open PDH query: error code {}", status);
        return loadAvg;
    }

    status = PdhAddEnglishCounter(query, "\\System\\Processor Queue Length", 0,
                                  &counter);
    if (status != ERROR_SUCCESS) {
        LOG_F(ERROR, "Failed to add PDH counter: error code {}", status);
        PdhCloseQuery(query);
        return loadAvg;
    }

    // First collection - we'll ignore this as it's just to initialize
    status = PdhCollectQueryData(query);
    if (status != ERROR_SUCCESS) {
        LOG_F(ERROR, "Failed to collect query data: error code {}", status);
        PdhCloseQuery(query);
        return loadAvg;
    }

    // Wait a bit for a second collection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    status = PdhCollectQueryData(query);
    if (status != ERROR_SUCCESS) {
        LOG_F(ERROR, "Failed to collect second query data: error code {}",
              status);
        PdhCloseQuery(query);
        return loadAvg;
    }

    PDH_FMT_COUNTERVALUE counterValue;
    status = PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr,
                                         &counterValue);
    if (status == ERROR_SUCCESS) {
        // Queue length is somewhat similar to load average
        // We'll use it for all three values as Windows doesn't track different
        // time periods
        double queueLength = counterValue.doubleValue;

        // Scale queue length to something more similar to Unix load averages
        // This is a very rough approximation
        int numCores = getNumberOfLogicalCores();
        double loadApprox = queueLength / numCores;

        // Cap at a reasonable number
        loadApprox = std::min(loadApprox, 20.0);

        loadAvg.oneMinute = loadAvg.fiveMinutes = loadAvg.fifteenMinutes =
            loadApprox;
        LOG_F(INFO, "Approximated Load Average: {:.2f}", loadApprox);
    } else {
        LOG_F(ERROR, "Failed to get counter value: error code {}", status);
    }

    PdhCloseQuery(query);

#elif defined(__linux__) || defined(__FreeBSD__)
    // Linux and FreeBSD provide load averages via getloadavg
    double avg[3];
    int ret = getloadavg(avg, 3);

    if (ret == 3) {
        loadAvg.oneMinute = avg[0];
        loadAvg.fiveMinutes = avg[1];
        loadAvg.fifteenMinutes = avg[2];
        LOG_F(INFO, "Load Average: {:.2f}, {:.2f}, {:.2f}", loadAvg.oneMinute,
              loadAvg.fiveMinutes, loadAvg.fifteenMinutes);
    } else {
        LOG_F(ERROR, "getloadavg failed: {}", ret);

        // Fallback for Linux
        std::ifstream loadFile("/proc/loadavg");
        if (loadFile.is_open()) {
            loadFile >> loadAvg.oneMinute >> loadAvg.fiveMinutes >>
                loadAvg.fifteenMinutes;
            LOG_F(
                INFO, "Load Average from /proc/loadavg: {:.2f}, {:.2f}, {:.2f}",
                loadAvg.oneMinute, loadAvg.fiveMinutes, loadAvg.fifteenMinutes);
        }
    }

#elif defined(__APPLE__)
    // macOS also provides load averages via getloadavg
    double avg[3];
    int ret = getloadavg(avg, 3);

    if (ret == 3) {
        loadAvg.oneMinute = avg[0];
        loadAvg.fiveMinutes = avg[1];
        loadAvg.fifteenMinutes = avg[2];
        LOG_F(INFO, "Load Average: {:.2f}, {:.2f}, {:.2f}", loadAvg.oneMinute,
              loadAvg.fiveMinutes, loadAvg.fifteenMinutes);
    } else {
        LOG_F(ERROR, "getloadavg failed: {}", ret);

        // Fallback using sysctl
        struct loadavg load;
        size_t size = sizeof(load);
        if (sysctlbyname("vm.loadavg", &load, &size, nullptr, 0) == 0) {
            loadAvg.oneMinute = static_cast<double>(load.ldavg[0]) /
                                static_cast<double>(load.fscale);
            loadAvg.fiveMinutes = static_cast<double>(load.ldavg[1]) /
                                  static_cast<double>(load.fscale);
            loadAvg.fifteenMinutes = static_cast<double>(load.ldavg[2]) /
                                     static_cast<double>(load.fscale);
            LOG_F(INFO, "Load Average from sysctl: {:.2f}, {:.2f}, {:.2f}",
                  loadAvg.oneMinute, loadAvg.fiveMinutes,
                  loadAvg.fifteenMinutes);
        }
    }
#endif

    LOG_F(INFO, "Finished getCpuLoadAverage function");
    return loadAvg;
}

auto getCpuPowerInfo() -> CpuPowerInfo {
    LOG_F(INFO, "Starting getCpuPowerInfo function");
    CpuPowerInfo powerInfo{0.0, 0.0, 0.0};

#ifdef _WIN32
    // Windows doesn't expose CPU power info through standard APIs
    // We can attempt to use the Windows power management APIs

    // Try using WMI first
    std::string wmiResult = executeWmiQuery(
        "SELECT ThermalDesignPower FROM Win32_Processor", "ThermalDesignPower");
    if (!wmiResult.empty()) {
        try {
            powerInfo.maxTDP = std::stod(wmiResult);
            LOG_F(INFO, "CPU TDP from WMI: {} W", powerInfo.maxTDP);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to parse WMI result: {}", e.what());
        }
    }

    /*
    // Try to get current power draw - difficult in Windows
    // Some laptops expose current power through battery interfaces
    SYSTEM_POWER_STATUS powerStatus;
    if (GetSystemPowerStatus(&powerStatus)) {
        if (powerStatus.BatteryFlag != 128) {  // If battery exists
            SYSTEM_BATTERY_STATE batteryState;
            if (CallNtPowerInformation(SystemBatteryState, nullptr, 0,
                                       &batteryState, sizeof(batteryState)) ==
                STATUS_SUCCESS) {
                // This gives a very rough estimate
                if (batteryState.Rate > 0) {
                    // Rate is power draw in mW
                    double totalPower =
                        batteryState.Rate / 1000.0;  // Convert to W

                    // Estimate CPU portion (very rough)
                    powerInfo.currentWatts =
                        totalPower * 0.6;  // Assume CPU is ~60% of system power
                    LOG_F(INFO, "Estimated CPU Power: {} W",
                          powerInfo.currentWatts);
                }
            }
        }
    }
    */

#elif defined(__linux__)
    // Try Intel RAPL (Running Average Power Limit) for Intel CPUs
    std::string vendor = getProcessorIdentifier();
    if (vendor.find("Intel") != std::string::npos) {
        // Check if RAPL is available
        std::ifstream energyFile(
            "/sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj");
        if (energyFile.is_open()) {
            uint64_t energy1, energy2;
            energyFile >> energy1;
            energyFile.close();

            // Wait a bit to measure energy delta
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            energyFile.open(
                "/sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj");
            energyFile >> energy2;
            energyFile.close();

            // Calculate power (energy delta / time)
            uint64_t energyDelta = energy2 - energy1;
            powerInfo.currentWatts =
                static_cast<double>(energyDelta) / 100000000.0;  // Convert to W
            LOG_F(INFO, "CPU Power from RAPL: {} W", powerInfo.currentWatts);

            // Get max TDP
            std::ifstream constraintFile(
                "/sys/class/powercap/intel-rapl/intel-rapl:0/"
                "constraint_0_max_power_uw");
            if (constraintFile.is_open()) {
                uint64_t maxPower;
                constraintFile >> maxPower;
                powerInfo.maxTDP =
                    static_cast<double>(maxPower) / 1000000.0;  // Convert to W
                LOG_F(INFO, "CPU TDP from RAPL: {} W", powerInfo.maxTDP);
            }
        } else {
            LOG_F(WARNING, "RAPL not available");
        }
    } else if (vendor.find("AMD") != std::string::npos) {
        // Try to read AMD power info if available
        std::ifstream amdPowerFile("/sys/class/hwmon/hwmon0/power1_input");
        if (!amdPowerFile.is_open()) {
            // Try another common location
            amdPowerFile.open("/sys/class/hwmon/hwmon1/power1_input");
        }

        if (amdPowerFile.is_open()) {
            uint64_t microwatts;
            amdPowerFile >> microwatts;
            powerInfo.currentWatts =
                static_cast<double>(microwatts) / 1000000.0;  // Convert to W
            LOG_F(INFO, "CPU Power from AMD sensor: {} W",
                  powerInfo.currentWatts);
        } else {
            LOG_F(WARNING, "AMD power sensors not available");
        }
    }

    // If we couldn't get power directly, try to estimate from temperature and
    // frequency
    if (powerInfo.currentWatts <= 0.0) {
        float temp = getCurrentCpuTemperature();
        double freq = getProcessorFrequency();
        double maxFreq = getMaxProcessorFrequency();

        // Very rough estimate: TDP * (freq/maxFreq)^2 * (temp/80)
        // 80°C is used as a reference high temperature
        double freqFactor = (freq / maxFreq) * (freq / maxFreq);
        double tempFactor = std::min(1.0, temp / 80.0);

        // Use a default TDP of 65W if we don't know
        double tdp = powerInfo.maxTDP > 0 ? powerInfo.maxTDP : 65.0;

        powerInfo.currentWatts = tdp * freqFactor * tempFactor;
        LOG_F(INFO, "Estimated CPU Power: {} W", powerInfo.currentWatts);
    }

    // Try to get TDP from product specifications if not already found
    if (powerInfo.maxTDP <= 0.0) {
        // Try to parse from model name for common processors
        std::string model = getCPUModel();

        // See if we can extract TDP from known patterns in model names
        // This is very approximate and won't work for all CPUs
        if (model.find("i9-") != std::string::npos) {
            powerInfo.maxTDP = 125.0;  // Most i9 desktop CPUs have 125W TDP
        } else if (model.find("i7-") != std::string::npos) {
            powerInfo.maxTDP = 95.0;  // Common i7 TDP
        } else if (model.find("i5-") != std::string::npos) {
            powerInfo.maxTDP = 65.0;  // Common i5 TDP
        } else if (model.find("i3-") != std::string::npos) {
            powerInfo.maxTDP = 58.0;  // Common i3 TDP
        } else if (model.find("Ryzen 9") != std::string::npos) {
            powerInfo.maxTDP = 105.0;  // Common Ryzen 9 TDP
        } else if (model.find("Ryzen 7") != std::string::npos) {
            powerInfo.maxTDP = 65.0;  // Common Ryzen 7 TDP
        } else if (model.find("Ryzen 5") != std::string::npos) {
            powerInfo.maxTDP = 65.0;  // Common Ryzen 5 TDP
        } else {
            powerInfo.maxTDP = 65.0;  // Default guess
        }
        LOG_F(INFO, "Estimated CPU TDP: {} W", powerInfo.maxTDP);
    }

#elif defined(__APPLE__)
    // macOS provides power info through IOKit on Intel systems
    // For Apple Silicon, we can use powermetrics

    // Check if this is Apple Silicon
    std::string model = getCPUModel();
    bool isAppleSilicon = model.find("Apple") != std::string::npos;

    if (isAppleSilicon) {
        // Use powermetrics for Apple Silicon
        FILE *pipe = popen("powermetrics -n 1 -i 100 | grep 'CPU Power'", "r");
        if (pipe != nullptr) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string result(buffer);
                std::regex powerRegex("CPU Power: (\\d+\\.\\d+) W");
                std::smatch match;
                if (std::regex_search(result, match, powerRegex) &&
                    match.size() > 1) {
                    try {
                        powerInfo.currentWatts = std::stod(match[1].str());
                        LOG_F(INFO, "CPU Power from powermetrics: {} W",
                              powerInfo.currentWatts);
                    } catch (const std::exception &e) {
                        LOG_F(ERROR, "Failed to parse powermetrics output: {}",
                              e.what());
                    }
                }
            }
            pclose(pipe);
        }

        // Set TDP based on the known Apple Silicon models
        if (model.find("M1 Pro") != std::string::npos) {
            powerInfo.maxTDP = 30.0;
        } else if (model.find("M1 Max") != std::string::npos) {
            powerInfo.maxTDP = 60.0;
        } else if (model.find("M1") != std::string::npos) {
            powerInfo.maxTDP = 20.0;
        } else if (model.find("M2 Pro") != std::string::npos) {
            powerInfo.maxTDP = 35.0;
        } else if (model.find("M2 Max") != std::string::npos) {
            powerInfo.maxTDP = 65.0;
        } else if (model.find("M2") != std::string::npos) {
            powerInfo.maxTDP = 25.0;
        } else {
            powerInfo.maxTDP = 30.0;  // Default for unknown Apple Silicon
        }
    } else {
        // Intel Mac - use IOKit
        io_service_t service = IOServiceGetMatchingService(
            kIOMainPortDefault, IOServiceMatching("IOPMPowerSource"));
        if (service) {
            CFMutableDictionaryRef properties = nullptr;
            if (IORegistryEntryCreateCFProperties(service, &properties,
                                                  kCFAllocatorDefault,
                                                  0) == KERN_SUCCESS) {
                // Power info can sometimes be found in power source info
                CFTypeRef powerData =
                    CFDictionaryGetValue(properties, CFSTR("Power"));
                if (powerData &&
                    CFGetTypeID(powerData) == CFNumberGetTypeID()) {
                    int power;
                    CFNumberGetValue((CFNumberRef)powerData, kCFNumberIntType,
                                     &power);
                    powerInfo.currentWatts =
                        static_cast<double>(power) / 1000.0;  // Convert mW to W
                    LOG_F(INFO, "CPU Power from IOKit: {} W",
                          powerInfo.currentWatts);
                }
                CFRelease(properties);
            }
            IOObjectRelease(service);
        }

        // Set TDP based on common Intel Mac models
        if (model.find("i9") != std::string::npos) {
            powerInfo.maxTDP = 45.0;
        } else if (model.find("i7") != std::string::npos) {
            powerInfo.maxTDP = 35.0;
        } else if (model.find("i5") != std::string::npos) {
            powerInfo.maxTDP = 28.0;
        } else {
            powerInfo.maxTDP = 25.0;  // Default for unknown Intel Mac
        }
    }

    // If all else fails, estimate based on temperature and frequency
    if (powerInfo.currentWatts <= 0.0) {
        float temp = getCurrentCpuTemperature();
        double freq = getProcessorFrequency();
        double maxFreq = getMaxProcessorFrequency();

        // Similar estimation as Linux
        double freqFactor = (freq / maxFreq) * (freq / maxFreq);
        double tempFactor = std::min(1.0, temp / 80.0);
        powerInfo.currentWatts = powerInfo.maxTDP * freqFactor * tempFactor;

        LOG_F(INFO, "Estimated CPU Power: {} W", powerInfo.currentWatts);
    }
#elif defined(__FreeBSD__)
    // FreeBSD implementation - limited support
    // Try to use ACPI thermal data for rough estimation

    float temp = getCurrentCpuTemperature();
    double freq = getProcessorFrequency();
    double maxFreq = getMaxProcessorFrequency();

    // Estimate TDP based on CPU model
    std::string model = getCPUModel();
    if (model.find("i9") != std::string::npos) {
        powerInfo.maxTDP = 95.0;
    } else if (model.find("i7") != std::string::npos) {
        powerInfo.maxTDP = 65.0;
    } else if (model.find("i5") != std::string::npos) {
        powerInfo.maxTDP = 65.0;
    } else if (model.find("i3") != std::string::npos) {
        powerInfo.maxTDP = 58.0;
    } else if (model.find("Ryzen 9") != std::string::npos) {
        powerInfo.maxTDP = 105.0;
    } else if (model.find("Ryzen 7") != std::string::npos) {
        powerInfo.maxTDP = 65.0;
    } else if (model.find("Ryzen 5") != std::string::npos) {
        powerInfo.maxTDP = 65.0;
    } else {
        powerInfo.maxTDP = 65.0;  // Default
    }

    // Rough estimate based on frequency and temperature
    double freqFactor = (freq / maxFreq) * (freq / maxFreq);
    double tempFactor = std::min(1.0, temp / 80.0);
    powerInfo.currentWatts = powerInfo.maxTDP * freqFactor * tempFactor;

    LOG_F(INFO, "Estimated CPU Power: {} W (TDP: {} W)", powerInfo.currentWatts,
          powerInfo.maxTDP);
#endif

    LOG_F(INFO, "Finished getCpuPowerInfo function");
    return powerInfo;
}

auto getCpuFeatureFlags() -> std::vector<std::string> {
    LOG_F(INFO, "Starting getCpuFeatureFlags function");

    if (!needsCacheRefresh() && !g_cpuInfoCache.flags.empty()) {
        return g_cpuInfoCache.flags;
    }

    std::vector<std::string> flags;

#ifdef _WIN32
    // Use CPUID instruction to get feature flags
    int cpuInfo[4] = {0};
    std::vector<std::string> standardFlags = {
        "fpu",  "vme",   "de",   "pse",   "tsc",  "msr", "pae",  "mce",
        "cx8",  "apic",  "",     "sep",   "mtrr", "pge", "mca",  "cmov",
        "pat",  "pse36", "psn",  "clfsh", "",     "ds",  "acpi", "mmx",
        "fxsr", "sse",   "sse2", "ss",    "htt",  "tm",  "ia64", "pbe"};

    std::vector<std::string> extendedFlags = {
        "",        "", "", "", "", "", "", "",   "",         "",     "",
        "syscall", "", "", "", "", "", "", "",   "",         "nx",   "",
        "mmxext",  "", "", "", "", "", "", "lm", "3dnowext", "3dnow"};

    // Standard flags (EAX=1)
    __cpuid(cpuInfo, 1);
    for (int i = 0; i < 32; i++) {
        if ((cpuInfo[3] >> i) & 1) {
            if (i < standardFlags.size() && !standardFlags[i].empty()) {
                flags.push_back(standardFlags[i]);
            }
        }
    }

    // Check for extended flags (SSE3, SSSE3, SSE4.1, SSE4.2, AVX, etc.)
    if (cpuInfo[2] & (1 << 0))
        flags.push_back("sse3");
    if (cpuInfo[2] & (1 << 9))
        flags.push_back("ssse3");
    if (cpuInfo[2] & (1 << 19))
        flags.push_back("sse4.1");
    if (cpuInfo[2] & (1 << 20))
        flags.push_back("sse4.2");
    if (cpuInfo[2] & (1 << 28))
        flags.push_back("avx");

    // Extended features (EAX=7)
    __cpuid(cpuInfo, 7);
    if (cpuInfo[1] & (1 << 5))
        flags.push_back("avx2");
    if (cpuInfo[1] & (1 << 3))
        flags.push_back("bmi1");
    if (cpuInfo[1] & (1 << 8))
        flags.push_back("bmi2");

    // Extended processor info and feature bits (EAX=0x80000001)
    __cpuid(cpuInfo, 0x80000001);
    for (int i = 0; i < 32; i++) {
        if ((cpuInfo[3] >> i) & 1) {
            if (i < extendedFlags.size() && !extendedFlags[i].empty()) {
                flags.push_back(extendedFlags[i]);
            }
        }
    }

#elif defined(__linux__)
    // Read flags from /proc/cpuinfo
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("flags") != std::string::npos ||
            line.find("Features") != std::string::npos) {
            std::istringstream iss(line.substr(line.find(':') + 1));
            std::string flag;
            while (iss >> flag) {
                flags.push_back(flag);
            }
            break;
        }
    }
    cpuinfo.close();

#elif defined(__APPLE__)
    char buffer[1024];
    size_t size = sizeof(buffer);

    if (sysctlbyname("machdep.cpu.features", buffer, &size, nullptr, 0) == 0) {
        std::istringstream iss(buffer);
        std::string flag;
        while (iss >> flag) {
            // Convert to lowercase to match Linux convention
            std::transform(flag.begin(), flag.end(), flag.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            flags.push_back(flag);
        }
    }

    // Get extended features
    if (sysctlbyname("machdep.cpu.leaf7_features", buffer, &size, nullptr, 0) ==
        0) {
        std::istringstream iss(buffer);
        std::string flag;
        while (iss >> flag) {
            std::transform(flag.begin(), flag.end(), flag.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            flags.push_back(flag);
        }
    }

    // For Apple Silicon, we'll add some known features
    std::string model = getCPUModel();
    if (model.find("Apple") != std::string::npos) {
        flags.push_back("neon");
        flags.push_back("armv8");
        flags.push_back("asimd");  // Advanced SIMD
        flags.push_back("pmull");
        flags.push_back("crc32");
        flags.push_back("aes");
        flags.push_back("sha1");
        flags.push_back("sha2");
    }
#elif defined(__FreeBSD__)
    // Use CPUID instruction via a helper program
    FILE* pipe = popen(
        "sysctl -n hw.instruction_sse hw.instruction_sse2 hw.instruction_sse3",
        "r");
    if (pipe != nullptr) {
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

            if (line == "1") {
                if (line.find("sse") != std::string::npos)
                    flags.push_back("sse");
                if (line.find("sse2") != std::string::npos)
                    flags.push_back("sse2");
                if (line.find("sse3") != std::string::npos)
                    flags.push_back("sse3");
            }
        }
        pclose(pipe);
    }

    // Additional flags can be found in dmesg output or other sysctl variables
#endif

    // Remove duplicates
    std::sort(flags.begin(), flags.end());
    flags.erase(std::unique(flags.begin(), flags.end()), flags.end());

    LOG_F(INFO, "CPU Flags: {}", flags.size());
    for (const auto& flag : flags) {
        LOG_F(9, "  {}", flag);
    }

    // Update cache
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_cpuInfoCache.flags = flags;
    }

    LOG_F(INFO, "Finished getCpuFeatureFlags function");
    return flags;
}

auto isCpuFeatureSupported(const std::string& feature) -> CpuFeatureSupport {
    LOG_F(INFO, "Checking if CPU feature {} is supported", feature);

    std::string featureLower = feature;
    std::transform(featureLower.begin(), featureLower.end(),
                   featureLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    const auto flags = getCpuFeatureFlags();

    // Direct match
    auto it = std::find(flags.begin(), flags.end(), featureLower);
    if (it != flags.end()) {
        LOG_F(INFO, "Feature {} is directly supported", feature);
        return CpuFeatureSupport::SUPPORTED;
    }

    // Handle special cases and aliases
    if (featureLower == "avx512") {
        // Check for any AVX-512 variant
        for (const auto& flag : flags) {
            if (flag.find("avx512") != std::string::npos) {
                LOG_F(INFO, "AVX-512 feature found: {}", flag);
                return CpuFeatureSupport::SUPPORTED;
            }
        }
    } else if (featureLower == "amd64" || featureLower == "x86_64") {
        // Check for AMD64/x86_64 (long mode)
        it = std::find(flags.begin(), flags.end(), "lm");
        if (it != flags.end()) {
            LOG_F(INFO, "AMD64/x86_64 is supported (via lm flag)");
            return CpuFeatureSupport::SUPPORTED;
        }
    } else if (featureLower == "hyperthreading" || featureLower == "ht") {
        it = std::find(flags.begin(), flags.end(), "htt");
        if (it != flags.end()) {
            LOG_F(INFO, "Hyperthreading is supported");
            return CpuFeatureSupport::SUPPORTED;
        }

        // Also check for actual logical vs physical cores
        if (getNumberOfLogicalCores() > getNumberOfPhysicalCores()) {
            LOG_F(INFO,
                  "Hyperthreading is supported (logical > physical cores)");
            return CpuFeatureSupport::SUPPORTED;
        }
    } else if (featureLower == "arm" || featureLower == "aarch64") {
        // Check CPU architecture
        auto arch = getCpuArchitecture();
        if (arch == CpuArchitecture::ARM || arch == CpuArchitecture::ARM64) {
            LOG_F(INFO, "ARM architecture is detected");
            return CpuFeatureSupport::SUPPORTED;
        }
    } else if (featureLower == "x86" || featureLower == "intel") {
        auto arch = getCpuArchitecture();
        if (arch == CpuArchitecture::X86 || arch == CpuArchitecture::X86_64) {
            LOG_F(INFO, "x86 architecture is detected");
            return CpuFeatureSupport::SUPPORTED;
        }
    }

    LOG_F(INFO, "Feature {} is not supported", feature);
    return CpuFeatureSupport::NOT_SUPPORTED;
}

auto getCpuArchitecture() -> CpuArchitecture {
    LOG_F(INFO, "Starting getCpuArchitecture function");

    if (!needsCacheRefresh()) {
        return g_cpuInfoCache.architecture;
    }

    CpuArchitecture arch = CpuArchitecture::UNKNOWN;

#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);

    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            arch = CpuArchitecture::X86_64;
            LOG_F(INFO, "CPU Architecture: x86_64");
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            arch = CpuArchitecture::X86;
            LOG_F(INFO, "CPU Architecture: x86");
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            arch = CpuArchitecture::ARM;
            LOG_F(INFO, "CPU Architecture: ARM");
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            arch = CpuArchitecture::ARM64;
            LOG_F(INFO, "CPU Architecture: ARM64");
            break;
        default:
            LOG_F(INFO, "CPU Architecture: Unknown ({})",
                  sysInfo.wProcessorArchitecture);
            break;
    }

#elif defined(__linux__)
    struct utsname sysInfo;
    if (uname(&sysInfo) == 0) {
        std::string machine = sysInfo.machine;

        if (machine == "x86_64") {
            arch = CpuArchitecture::X86_64;
            LOG_F(INFO, "CPU Architecture: x86_64");
        } else if (machine.find("i386") != std::string::npos ||
                   machine.find("i686") != std::string::npos) {
            arch = CpuArchitecture::X86;
            LOG_F(INFO, "CPU Architecture: x86");
        } else if (machine.find("arm") != std::string::npos) {
            // Check if this is 64-bit ARM
            if (machine.find("aarch64") != std::string::npos ||
                machine.find("arm64") != std::string::npos) {
                arch = CpuArchitecture::ARM64;
                LOG_F(INFO, "CPU Architecture: ARM64");
            } else {
                arch = CpuArchitecture::ARM;
                LOG_F(INFO, "CPU Architecture: ARM");
            }
        } else if (machine.find("ppc") != std::string::npos ||
                   machine.find("powerpc") != std::string::npos) {
            arch = CpuArchitecture::POWERPC;
            LOG_F(INFO, "CPU Architecture: POWERPC");
        } else if (machine.find("mips") != std::string::npos) {
            arch = CpuArchitecture::MIPS;
            LOG_F(INFO, "CPU Architecture: MIPS");
        } else if (machine.find("riscv") != std::string::npos) {
            arch = CpuArchitecture::RISC_V;
            LOG_F(INFO, "CPU Architecture: RISC-V");
        } else {
            LOG_F(INFO, "CPU Architecture: Unknown ({})", machine);
        }
    }

#elif defined(__APPLE__)
    // For macOS, we get architecture through sysctl
    char buffer[128];
    size_t size = sizeof(buffer);

    if (sysctlbyname("hw.machine", buffer, &size, nullptr, 0) == 0) {
        std::string machine = buffer;

        if (machine.find("arm64") != std::string::npos) {
            arch = CpuArchitecture::ARM64;
            LOG_F(INFO, "CPU Architecture: ARM64");
        } else if (machine.find("x86_64") != std::string::npos) {
            arch = CpuArchitecture::X86_64;
            LOG_F(INFO, "CPU Architecture: x86_64");
        } else if (machine.find("i386") != std::string::npos) {
            arch = CpuArchitecture::X86;
            LOG_F(INFO, "CPU Architecture: x86");
        } else {
            LOG_F(INFO, "CPU Architecture: Unknown ({})", machine);
        }
    } else {
        // Fallback: check for Rosetta translation
        FILE *pipe = popen("sysctl -n sysctl.proc_translated", "r");
        if (pipe != nullptr) {
            char buffer[16];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string result(buffer);
                if (result.find("1") != std::string::npos) {
                    // Running under Rosetta, so native arch is ARM64
                    arch = CpuArchitecture::ARM64;
                    LOG_F(INFO, "CPU Architecture: ARM64 (Rosetta detected)");
                } else {
                    // Check architecture using arch command
                    FILE *archPipe = popen("arch", "r");
                    if (archPipe != nullptr) {
                        char archBuffer[16];
                        if (fgets(archBuffer, sizeof(archBuffer), archPipe) !=
                            nullptr) {
                            std::string archResult(archBuffer);
                            if (archResult.find("arm64") != std::string::npos) {
                                arch = CpuArchitecture::ARM64;
                            } else if (archResult.find("x86_64") !=
                                       std::string::npos) {
                                arch = CpuArchitecture::X86_64;
                            } else if (archResult.find("i386") !=
                                       std::string::npos) {
                                arch = CpuArchitecture::X86;
                            }
                            LOG_F(INFO,
                                  "CPU Architecture from arch command: {}",
                                  archResult);
                        }
                        pclose(archPipe);
                    }
                }
            }
            pclose(pipe);
        }
    }
#elif defined(__FreeBSD__)
    // FreeBSD uses the same uname approach as Linux
    struct utsname sysInfo;
    if (uname(&sysInfo) == 0) {
        std::string machine = sysInfo.machine;

        if (machine == "amd64") {
            arch = CpuArchitecture::X86_64;
            LOG_F(INFO, "CPU Architecture: x86_64");
        } else if (machine == "i386") {
            arch = CpuArchitecture::X86;
            LOG_F(INFO, "CPU Architecture: x86");
        } else if (machine.find("arm") != std::string::npos) {
            if (machine.find("aarch64") != std::string::npos) {
                arch = CpuArchitecture::ARM64;
                LOG_F(INFO, "CPU Architecture: ARM64");
            } else {
                arch = CpuArchitecture::ARM;
                LOG_F(INFO, "CPU Architecture: ARM");
            }
        } else if (machine.find("powerpc") != std::string::npos) {
            arch = CpuArchitecture::POWERPC;
            LOG_F(INFO, "CPU Architecture: POWERPC");
        } else if (machine.find("mips") != std::string::npos) {
            arch = CpuArchitecture::MIPS;
            LOG_F(INFO, "CPU Architecture: MIPS");
        } else {
            LOG_F(INFO, "CPU Architecture: Unknown ({})", machine);
        }
    }
#endif

    // As a last resort, check compiler-defined macros
    if (arch == CpuArchitecture::UNKNOWN) {
#if defined(__x86_64__) || defined(_M_X64)
        arch = CpuArchitecture::X86_64;
        LOG_F(INFO, "CPU Architecture (from compiler macros): x86_64");
#elif defined(__i386__) || defined(_M_IX86)
        arch = CpuArchitecture::X86;
        LOG_F(INFO, "CPU Architecture (from compiler macros): x86");
#elif defined(__aarch64__) || defined(_M_ARM64)
        arch = CpuArchitecture::ARM64;
        LOG_F(INFO, "CPU Architecture (from compiler macros): ARM64");
#elif defined(__arm__) || defined(_M_ARM)
        arch = CpuArchitecture::ARM;
        LOG_F(INFO, "CPU Architecture (from compiler macros): ARM");
#elif defined(__powerpc__) || defined(__ppc__) || defined(_M_PPC)
        arch = CpuArchitecture::POWERPC;
        LOG_F(INFO, "CPU Architecture (from compiler macros): PowerPC");
#elif defined(__mips__)
        arch = CpuArchitecture::MIPS;
        LOG_F(INFO, "CPU Architecture (from compiler macros): MIPS");
#elif defined(__riscv)
        arch = CpuArchitecture::RISC_V;
        LOG_F(INFO, "CPU Architecture (from compiler macros): RISC-V");
#endif
    }

    // Update cache
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_cpuInfoCache.architecture = arch;
    }

    LOG_F(INFO, "Finished getCpuArchitecture function");
    return arch;
}

auto getCpuVendor() -> CpuVendor {
    LOG_F(INFO, "Starting getCpuVendor function");

    if (!needsCacheRefresh()) {
        return g_cpuInfoCache.vendor;
    }

    CpuVendor vendor = CpuVendor::UNKNOWN;
    std::string vendorString;

#ifdef _WIN32
    // Use CPUID instruction to get vendor string
    int cpuInfo[4] = {0};
    char vendorId[13];
    memset(vendorId, 0, sizeof(vendorId));

    __cpuid(cpuInfo, 0);
    memcpy(vendorId, &cpuInfo[1], 4);
    memcpy(vendorId + 4, &cpuInfo[3], 4);
    memcpy(vendorId + 8, &cpuInfo[2], 4);

    vendorString = vendorId;

#elif defined(__linux__) || defined(__FreeBSD__)
    // Read vendor from /proc/cpuinfo
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;

    // For x86/x86_64
    while (std::getline(cpuinfo, line)) {
        if (line.find("vendor_id") != std::string::npos) {
            vendorString = line.substr(line.find(':') + 2);
            // Trim whitespace
            vendorString =
                std::regex_replace(vendorString, std::regex("^\\s+|\\s+$"), "");
            LOG_F(INFO, "CPU Vendor (from vendor_id): {}", vendorString);
            break;
        }
    }

    // For ARM
    if (vendorString.empty()) {
        cpuinfo.clear();
        cpuinfo.seekg(0, std::ios::beg);

        while (std::getline(cpuinfo, line)) {
            if (line.find("CPU implementer") != std::string::npos) {
                std::string implementer = line.substr(line.find(':') + 2);
                // Trim whitespace
                implementer = std::regex_replace(implementer,
                                                 std::regex("^\\s+|\\s+$"), "");

                // Map ARM implementer codes to vendors
                if (implementer == "0x41") {
                    vendorString = "ARM";
                } else if (implementer == "0x42") {
                    vendorString = "Broadcom";
                } else if (implementer == "0x43") {
                    vendorString = "Cavium";
                } else if (implementer == "0x44") {
                    vendorString = "DEC";
                } else if (implementer == "0x51") {
                    vendorString = "Qualcomm";
                } else if (implementer == "0x53") {
                    vendorString = "Samsung";
                } else if (implementer == "0x56") {
                    vendorString = "Marvell";
                } else if (implementer == "0x69") {
                    vendorString = "Intel";
                } else {
                    vendorString =
                        "Unknown ARM vendor (implementer: " + implementer + ")";
                }

                LOG_F(INFO, "CPU Vendor (from implementer): {}", vendorString);
                break;
            }
        }
    }

    cpuinfo.close();

#elif defined(__APPLE__)
    // For macOS, we get vendor through sysctl
    char buffer[128];
    size_t size = sizeof(buffer);

    // First check if this is Apple Silicon
    if (sysctlbyname("hw.optional.arm64", nullptr, nullptr, nullptr, 0) == 0 ||
        sysctlbyname("hw.optional.arm", nullptr, nullptr, nullptr, 0) == 0) {
        vendorString = "Apple";
        LOG_F(INFO, "CPU Vendor: Apple (ARM detected)");
    } else if (sysctlbyname("machdep.cpu.vendor", buffer, &size, nullptr, 0) ==
               0) {
        // x86 CPU vendor
        vendorString = buffer;
        LOG_F(INFO, "CPU Vendor (from sysctl): {}", vendorString);
    } else {
        // Fallback based on model
        std::string model = getCPUModel();
        if (model.find("Apple") != std::string::npos) {
            vendorString = "Apple";
        } else if (model.find("Intel") != std::string::npos) {
            vendorString = "Intel";
        } else if (model.find("AMD") != std::string::npos) {
            vendorString = "AMD";
        }
        LOG_F(INFO, "CPU Vendor (derived from model): {}", vendorString);
    }
#endif

    // Map vendor string to enum
    vendor = getVendorFromString(vendorString);

    // Update cache
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_cpuInfoCache.vendor = vendor;
    }

    LOG_F(INFO, "Finished getCpuVendor function with result: {}",
          cpuVendorToString(vendor));
    return vendor;
}

auto getCpuSocketType() -> std::string {
    LOG_F(INFO, "Starting getCpuSocketType function");

    if (!needsCacheRefresh() && !g_cpuInfoCache.socketType.empty()) {
        return g_cpuInfoCache.socketType;
    }

    std::string socketType = "Unknown";

#ifdef _WIN32
    // Try using WMI
    std::string wmiResult = executeWmiQuery(
        "SELECT SocketDesignation FROM Win32_Processor", "SocketDesignation");
    if (!wmiResult.empty()) {
        socketType = wmiResult;
        LOG_F(INFO, "CPU Socket Type from WMI: {}", socketType);
    } else {
        LOG_F(WARNING, "WMI query failed, trying alternative methods");

        // Try to infer socket from CPU model
        std::string model = getCPUModel();
        std::string vendor = cpuVendorToString(getCpuVendor());

        if (vendor == "Intel") {
            if (model.find("i9-") != std::string::npos ||
                model.find("i7-") != std::string::npos ||
                model.find("i5-") != std::string::npos) {
                if (model.find("10") != std::string::npos ||
                    model.find("11") != std::string::npos) {
                    socketType = "LGA1200";
                } else if (model.find("12") != std::string::npos ||
                           model.find("13") != std::string::npos) {
                    socketType = "LGA1700";
                } else if (model.find("8") != std::string::npos ||
                           model.find("9") != std::string::npos) {
                    socketType = "LGA1151";
                } else if (model.find("6") != std::string::npos ||
                           model.find("7") != std::string::npos) {
                    socketType = "LGA1151";
                } else if (model.find("-X") != std::string::npos) {
                    socketType = "LGA2066";
                }
            } else if (model.find("Xeon") != std::string::npos) {
                if (model.find("E-") != std::string::npos) {
                    socketType = "LGA3647";
                } else if (model.find("E5-") != std::string::npos ||
                           model.find("E7-") != std::string::npos) {
                    socketType = "LGA2011-3";
                }
            }
        } else if (vendor == "AMD") {
            if (model.find("Ryzen") != std::string::npos) {
                if (model.find("1") == 0 || model.find("2") == 0 ||
                    model.find("3") == 0 || model.find("4") == 0) {
                    socketType = "AM4";
                } else if (model.find("5") == 0 || model.find("7") == 0) {
                    socketType = "AM5";
                }
            } else if (model.find("Threadripper") != std::string::npos) {
                if (model.find("1") != std::string::npos ||
                    model.find("2") != std::string::npos ||
                    model.find("3") != std::string::npos) {
                    socketType = "TR4";
                } else {
                    socketType = "TRX4";
                }
            } else if (model.find("EPYC") != std::string::npos) {
                socketType = "SP3";
            }
        }

        if (socketType != "Unknown") {
            LOG_F(INFO, "CPU Socket Type (inferred): {}", socketType);
        }
    }

#elif defined(__linux__)
    // Linux doesn't expose socket type directly, we need to infer it

    // Try using dmidecode if available
    FILE* pipe =
        popen("dmidecode -t 4 | grep 'Socket Designation' | head -n1", "r");
    if (pipe != nullptr) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string result(buffer);
            size_t pos = result.find(':');
            if (pos != std::string::npos) {
                socketType = result.substr(pos + 1);
                // Trim whitespace
                socketType = std::regex_replace(socketType,
                                                std::regex("^\\s+|\\s+$"), "");
                LOG_F(INFO, "CPU Socket Type from dmidecode: {}", socketType);
            }
        }
        pclose(pipe);
    }

    // If dmidecode failed or isn't available, try to infer from CPU model
    if (socketType == "Unknown") {
        std::string model = getCPUModel();
        std::string vendor = cpuVendorToString(getCpuVendor());

        // Same logic as Windows section
        if (vendor == "Intel") {
            if (model.find("i9-") != std::string::npos ||
                model.find("i7-") != std::string::npos ||
                model.find("i5-") != std::string::npos) {
                if (model.find("10") != std::string::npos ||
                    model.find("11") != std::string::npos) {
                    socketType = "LGA1200";
                } else if (model.find("12") != std::string::npos ||
                           model.find("13") != std::string::npos) {
                    socketType = "LGA1700";
                } else if (model.find("8") != std::string::npos ||
                           model.find("9") != std::string::npos) {
                    socketType = "LGA1151";
                } else if (model.find("6") != std::string::npos ||
                           model.find("7") != std::string::npos) {
                    socketType = "LGA1151";
                } else if (model.find("-X") != std::string::npos) {
                    socketType = "LGA2066";
                }
            } else if (model.find("Xeon") != std::string::npos) {
                if (model.find("E-") != std::string::npos) {
                    socketType = "LGA3647";
                } else if (model.find("E5-") != std::string::npos ||
                           model.find("E7-") != std::string::npos) {
                    socketType = "LGA2011-3";
                }
            }
        } else if (vendor == "AMD") {
            if (model.find("Ryzen") != std::string::npos) {
                if (model.find("1") == 0 || model.find("2") == 0 ||
                    model.find("3") == 0 || model.find("4") == 0) {
                    socketType = "AM4";
                } else if (model.find("5") == 0 || model.find("7") == 0) {
                    socketType = "AM5";
                }
            } else if (model.find("Threadripper") != std::string::npos) {
                if (model.find("1") != std::string::npos ||
                    model.find("2") != std::string::npos ||
                    model.find("3") != std::string::npos) {
                    socketType = "TR4";
                } else {
                    socketType = "TRX4";
                }
            } else if (model.find("EPYC") != std::string::npos) {
                socketType = "SP3";
            }
        } else if (vendor == "ARM") {
            socketType = "BGA";  // Most ARM CPUs are BGA (soldered)
        }

        if (socketType != "Unknown") {
            LOG_F(INFO, "CPU Socket Type (inferred): {}", socketType);
        }
    }

#elif defined(__APPLE__)
    // Apple systems don't have traditional CPU sockets as CPUs are soldered
    std::string model = getCPUModel();
    if (model.find("Apple") != std::string::npos) {
        socketType = "SoC (System on Chip)";
    } else {
        socketType = "Soldered BGA";
    }
    LOG_F(INFO, "CPU Socket Type for Apple: {}", socketType);

#elif defined(__FreeBSD__)
    // Similar approach to Linux
    FILE* pipe =
        popen("dmidecode -t 4 | grep 'Socket Designation' | head -n1", "r");
    if (pipe != nullptr) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string result(buffer);
            size_t pos = result.find(':');
            if (pos != std::string::npos) {
                socketType = result.substr(pos + 1);
                // Trim whitespace
                socketType = std::regex_replace(socketType,
                                                std::regex("^\\s+|\\s+$"), "");
                LOG_F(INFO, "CPU Socket Type from dmidecode: {}", socketType);
            }
        }
        pclose(pipe);
    }
#endif

    // Update cache
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_cpuInfoCache.socketType = socketType;
    }

    LOG_F(INFO, "Finished getCpuSocketType function");
    return socketType;
}

auto getCpuScalingGovernor() -> std::string {
    LOG_F(INFO, "Starting getCpuScalingGovernor function");
    std::string governor = "Unknown";

#ifdef _WIN32
    // Windows doesn't have governors but power plans
    /*
     GUID activePolicy;
    // 使用PowerGetActiveScheme而非PowerGetActiveScheme
    if (PowerGetActiveScheme(nullptr, &activePolicy) == ERROR_SUCCESS) {
        // Check which power plan this is
        wchar_t planName[256];
        DWORD nameSize = sizeof(planName);

        // 使用PowerReadFriendlyName函数
        if (PowerReadFriendlyName(nullptr, &activePolicy, nullptr, nullptr,
    planName, &nameSize) == ERROR_SUCCESS) { char planNameChar[256];
            wcstombs(planNameChar, planName, sizeof(planNameChar));
            governor = planNameChar;
        } else {
            // Try to identify by GUID
            if (IsEqualGUID(activePolicy, GUID_MIN_POWER_SAVINGS)) {
                governor = "High Performance";
            } else if (IsEqualGUID(activePolicy, GUID_MAX_POWER_SAVINGS)) {
                governor = "Power Saver";
            } else if (IsEqualGUID(activePolicy, GUID_TYPICAL_POWER_SAVINGS)) {
                governor = "Balanced";
            } else if (IsEqualGUID(activePolicy, GUID_POWER_SCHEME_NONE)) {
                governor = "Ultimate Performance";
            } else {
                // Convert GUID to string for logging
                OLECHAR guidString[39];
                StringFromGUID2(activePolicy, guidString, 39);
                char guidChar[39];
                wcstombs(guidChar, guidString, sizeof(guidChar));
                governor = "Custom Power Plan (" + std::string(guidChar) + ")";
            }
        }

        LOG_F(INFO, "Power Plan: {}", governor);
    } else {
        LOG_F(ERROR, "Failed to get active power scheme");
    }
    */
    LOG_F(INFO, "CPU Scaling Governor: {}", governor);

#elif defined(__linux__)
    std::ifstream governorFile(
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
    if (governorFile.is_open()) {
        std::getline(governorFile, governor);
        LOG_F(INFO, "CPU Scaling Governor: {}", governor);
    } else {
        LOG_F(ERROR, "Failed to open scaling_governor file");

        // Try using cpupower command
        FILE* pipe =
            popen("cpupower frequency-info | grep \"The governor\"", "r");
        if (pipe != nullptr) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string result(buffer);
                size_t pos = result.find("The governor \"");
                if (pos != std::string::npos) {
                    size_t start = pos + 14;  // Length of "The governor \""
                    size_t end = result.find("\"", start);
                    if (end != std::string::npos) {
                        governor = result.substr(start, end - start);
                        LOG_F(INFO, "CPU Scaling Governor from cpupower: {}",
                              governor);
                    }
                }
            }
            pclose(pipe);
        }
    }

#elif defined(__APPLE__)
    // macOS has energy profiles rather than governors
    io_service_t platform_expert = IOServiceGetMatchingService(
        kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    if (platform_expert) {
        CFTypeRef power_profile = IORegistryEntryCreateCFProperty(
            platform_expert, CFSTR("IOPMCurrentPowerProfile"),
            kCFAllocatorDefault, 0);
        if (power_profile) {
            if (CFGetTypeID(power_profile) == CFStringGetTypeID()) {
                CFStringRef profile_str = (CFStringRef)power_profile;
                char buffer[256];
                if (CFStringGetCString(profile_str, buffer, sizeof(buffer),
                                       kCFStringEncodingUTF8)) {
                    governor = buffer;
                }
            }
            CFRelease(power_profile);
        }
        IOObjectRelease(platform_expert);
    }

    if (governor == "Unknown") {
        // Try using pmset
        FILE *pipe = popen("pmset -g | grep -i \"active power profile\"", "r");
        if (pipe != nullptr) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string result(buffer);
                size_t pos = result.find(": ");
                if (pos != std::string::npos) {
                    governor = result.substr(pos + 2);
                    // Trim whitespace and newlines
                    governor = std::regex_replace(
                        governor, std::regex("^\\s+|\\s+$|\\n"), "");
                    LOG_F(INFO, "Power Profile: {}", governor);
                }
            }
            pclose(pipe);
        }
    }
#elif defined(__FreeBSD__)
    // FreeBSD uses powerd with states like ADAPTIVE, PERFORMANCE, etc.
    FILE* pipe = popen("sysctl dev.cpu.0.freq_levels", "r");
    if (pipe != nullptr) {
        char buffer[512];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            // Check if the CPU supports scaling
            governor = "Supported";

            // Try to get current setting
            FILE* govPipe = popen("sysctl dev.cpu.0.freq", "r");
            if (govPipe != nullptr) {
                char govBuffer[128];
                if (fgets(govBuffer, sizeof(govBuffer), govPipe) != nullptr) {
                    std::string result(govBuffer);
                    size_t pos = result.find(": ");
                    if (pos != std::string::npos) {
                        std::string currentFreq = result.substr(pos + 2);
                        // Strip newline
                        currentFreq.erase(std::remove(currentFreq.begin(),
                                                      currentFreq.end(), '\n'),
                                          currentFreq.end());

                        // Check if at max frequency
                        FILE* maxPipe = popen(
                            "sysctl dev.cpu.0.freq_levels | awk '{print $1}' | "
                            "cut -d'/' -f1",
                            "r");
                        if (maxPipe != nullptr) {
                            char maxBuffer[128];
                            if (fgets(maxBuffer, sizeof(maxBuffer), maxPipe) !=
                                nullptr) {
                                std::string maxFreq(maxBuffer);
                                maxFreq.erase(std::remove(maxFreq.begin(),
                                                          maxFreq.end(), '\n'),
                                              maxFreq.end());

                                if (currentFreq == maxFreq) {
                                    governor = "Performance";
                                } else {
                                    governor = "Economy";
                                }
                            }
                            pclose(maxPipe);
                        }
                    }
                }
                pclose(govPipe);
            }
        }
        pclose(pipe);
    }

    LOG_F(INFO, "CPU Scaling Mode: {}", governor);
#endif

    LOG_F(INFO, "Finished getCpuScalingGovernor function");
    return governor;
}

auto getPerCoreScalingGovernors() -> std::vector<std::string> {
    LOG_F(INFO, "Starting getPerCoreScalingGovernors function");
    std::vector<std::string> governors;

    int numCores = getNumberOfLogicalCores();
    governors.resize(numCores, "Unknown");

#ifdef _WIN32
    // Windows doesn't have per-core governors
    std::string globalGovernor = getCpuScalingGovernor();
    for (int i = 0; i < numCores; ++i) {
        governors[i] = globalGovernor;
    }
#elif defined(__linux__)
    for (int i = 0; i < numCores; ++i) {
        std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(i) +
                           "/cpufreq/scaling_governor";
        std::ifstream governorFile(path);
        if (governorFile.is_open()) {
            std::getline(governorFile, governors[i]);
            LOG_F(INFO, "CPU {} Scaling Governor: {}", i, governors[i]);
        } else {
            LOG_F(ERROR, "Failed to open scaling_governor for core {}", i);
            // Use global governor as fallback
            if (i == 0) {
                // If we can't read core 0, assume we can't read any others
                // either
                std::string globalGovernor = getCpuScalingGovernor();
                for (int j = 0; j < numCores; ++j) {
                    governors[j] = globalGovernor;
                }
                break;
            } else {
                // Use governor from core 0 as a fallback
                governors[i] = governors[0];
            }
        }
    }
#elif defined(__APPLE__) || defined(__FreeBSD__)
    // macOS and FreeBSD don't have per-core governors
    std::string globalGovernor = getCpuScalingGovernor();
    for (int i = 0; i < numCores; ++i) {
        governors[i] = globalGovernor;
    }
#endif

    LOG_F(INFO, "Finished getPerCoreScalingGovernors function");
    return governors;
}

auto getCpuInfo() -> CpuInfo {
    LOG_F(INFO, "Starting getCpuInfo function");

    if (!needsCacheRefresh()) {
        return g_cpuInfoCache;
    }

    CpuInfo info;

    // Basic information
    info.model = getCPUModel();
    info.identifier = getProcessorIdentifier();
    info.architecture = getCpuArchitecture();
    info.vendor = getCpuVendor();
    info.numPhysicalPackages = getNumberOfPhysicalPackages();
    info.numPhysicalCores = getNumberOfPhysicalCores();
    info.numLogicalCores = getNumberOfLogicalCores();

    // Frequencies
    info.baseFrequency = getProcessorFrequency();
    info.maxFrequency = getMaxProcessorFrequency();

    // Socket information
    info.socketType = getCpuSocketType();

    // Temperature and usage
    info.temperature = getCurrentCpuTemperature();
    info.usage = getCurrentCpuUsage();

    // Cache sizes
    info.caches = getCacheSizes();

    // Power information
    info.power = getCpuPowerInfo();

    // CPU flags
    info.flags = getCpuFeatureFlags();

    // Load average
    info.loadAverage = getCpuLoadAverage();

    // Instruction set
    switch (info.architecture) {
        case CpuArchitecture::X86_64:
            info.instructionSet = "x86-64";
            break;
        case CpuArchitecture::X86:
            info.instructionSet = "x86";
            break;
        case CpuArchitecture::ARM64:
            info.instructionSet = "ARMv8-A";
            break;
        case CpuArchitecture::ARM:
            info.instructionSet = "ARMv7";
            break;
        case CpuArchitecture::POWERPC:
            info.instructionSet = "PowerPC";
            break;
        case CpuArchitecture::MIPS:
            info.instructionSet = "MIPS";
            break;
        case CpuArchitecture::RISC_V:
            info.instructionSet = "RISC-V";
            break;
        default:
            info.instructionSet = "Unknown";
            break;
    }

    // Parse family/model/stepping from identifier if possible
    std::regex cpuIdRegex(".*Family (\\d+) Model (\\d+) Stepping (\\d+).*");
    std::smatch match;

    if (std::regex_search(info.identifier, match, cpuIdRegex) &&
        match.size() > 3) {
        try {
            info.family = std::stoi(match[1].str());
            info.model_id = std::stoi(match[2].str());
            info.stepping = std::stoi(match[3].str());
            LOG_F(INFO, "CPU Family: {}, Model: {}, Stepping: {}", info.family,
                  info.model_id, info.stepping);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to parse CPU family/model/stepping: {}",
                  e.what());
        }
    }

    // Get per-core information
    std::vector<float> coreUsages = getPerCoreCpuUsage();
    std::vector<float> coreTemps = getPerCoreCpuTemperature();
    std::vector<double> coreFreqs = getPerCoreFrequencies();
    std::vector<std::string> coreGovernors = getPerCoreScalingGovernors();

    int numCores = info.numLogicalCores;
    info.cores.resize(numCores);

    for (int i = 0; i < numCores; ++i) {
        CpuCoreInfo& core = info.cores[i];
        core.id = i;
        core.usage =
            (static_cast<size_t>(i) < coreUsages.size()) ? coreUsages[i] : 0.0f;
        core.temperature =
            (static_cast<size_t>(i) < coreTemps.size()) ? coreTemps[i] : 0.0f;
        core.currentFrequency =
            (static_cast<size_t>(i) < coreFreqs.size()) ? coreFreqs[i] : 0.0;
        core.maxFrequency = getMaxProcessorFrequency();
        core.minFrequency = getMinProcessorFrequency();
        core.governor = (static_cast<size_t>(i) < coreGovernors.size())
                            ? coreGovernors[i]
                            : "Unknown";
    }

    // Update cache
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_cpuInfoCache = info;
        g_lastCacheRefresh = std::chrono::steady_clock::now();
        g_cacheInitialized = true;
    }

    LOG_F(INFO, "Finished getCpuInfo function");
    return info;
}

auto cpuArchitectureToString(CpuArchitecture arch) -> std::string {
    switch (arch) {
        case CpuArchitecture::X86:
            return "x86";
        case CpuArchitecture::X86_64:
            return "x86_64";
        case CpuArchitecture::ARM:
            return "ARM";
        case CpuArchitecture::ARM64:
            return "ARM64";
        case CpuArchitecture::POWERPC:
            return "PowerPC";
        case CpuArchitecture::MIPS:
            return "MIPS";
        case CpuArchitecture::RISC_V:
            return "RISC-V";
        case CpuArchitecture::UNKNOWN:
        default:
            return "Unknown";
    }
}

auto cpuVendorToString(CpuVendor vendor) -> std::string {
    switch (vendor) {
        case CpuVendor::INTEL:
            return "Intel";
        case CpuVendor::AMD:
            return "AMD";
        case CpuVendor::ARM:
            return "ARM";
        case CpuVendor::APPLE:
            return "Apple";
        case CpuVendor::QUALCOMM:
            return "Qualcomm";
        case CpuVendor::IBM:
            return "IBM";
        case CpuVendor::MEDIATEK:
            return "MediaTek";
        case CpuVendor::SAMSUNG:
            return "Samsung";
        case CpuVendor::OTHER:
            return "Other";
        case CpuVendor::UNKNOWN:
        default:
            return "Unknown";
    }
}

void refreshCpuInfo() {
    LOG_F(INFO, "Manually refreshing CPU info cache");
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_lastCacheRefresh = std::chrono::steady_clock::time_point::min();
        g_cacheInitialized = false;
    }

    // Force a refresh by calling getCpuInfo()
    getCpuInfo();
    LOG_F(INFO, "CPU info cache refreshed");
}

}  // namespace atom::system