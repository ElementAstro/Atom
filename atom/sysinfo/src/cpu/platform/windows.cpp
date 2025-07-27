/*
 * windows.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-4

Description: System Information Module - CPU Windows Implementation

**************************************************/

#include <thread>
#ifdef _WIN32

#include "common.hpp"

#include <powersetting.h>
#include <powrprof.h>  // Add this header for PowerGetActiveScheme
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>

#pragma comment(lib, "PowrProf.lib")  // Link against the PowerProf library
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "wbemuuid.lib")

namespace atom::system {

// Enhanced Windows-specific caching and utilities
namespace {
    // Thread-safe caching for Windows-specific data
    std::shared_mutex g_wmi_mutex;
    std::atomic<bool> g_wmi_initialized{false};

    // Performance counter caching
    struct PerformanceCounterCache {
        std::atomic<std::chrono::steady_clock::time_point> last_update{};
        std::atomic<bool> valid{false};
        PDH_HQUERY query{nullptr};
        std::vector<PDH_HCOUNTER> counters;
        std::mutex mutex;
    };

    inline PerformanceCounterCache g_cpu_perf_cache;
    inline PerformanceCounterCache g_core_perf_cache;
    constexpr auto PERF_CACHE_DURATION = std::chrono::milliseconds(100);
}

/**
 * @brief Enhanced WMI helper class for Windows CPU information
 */
class WmiHelper {
private:
    IWbemLocator* pLoc = nullptr;
    IWbemServices* pSvc = nullptr;
    bool initialized = false;

public:
    WmiHelper() {
        initialize();
    }

    ~WmiHelper() {
        cleanup();
    }

    bool initialize() {
        if (initialized) return true;

        HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hres) && hres != RPC_E_CHANGED_MODE) {
            return false;
        }

        hres = CoInitializeSecurity(
            nullptr, -1, nullptr, nullptr,
            RPC_C_AUTHN_LEVEL_NONE, RPC_C_IMP_LEVEL_IMPERSONATE,
            nullptr, EOAC_NONE, nullptr);

        if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
            CoUninitialize();
            return false;
        }

        hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                               IID_IWbemLocator, (LPVOID*)&pLoc);
        if (FAILED(hres)) {
            CoUninitialize();
            return false;
        }

        hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr,
                                  0, NULL, 0, 0, &pSvc);
        if (FAILED(hres)) {
            pLoc->Release();
            CoUninitialize();
            return false;
        }

        hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                                nullptr, RPC_C_AUTHN_LEVEL_CALL,
                                RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        if (FAILED(hres)) {
            pSvc->Release();
            pLoc->Release();
            CoUninitialize();
            return false;
        }

        initialized = true;
        return true;
    }

    void cleanup() {
        if (pSvc) {
            pSvc->Release();
            pSvc = nullptr;
        }
        if (pLoc) {
            pLoc->Release();
            pLoc = nullptr;
        }
        if (initialized) {
            CoUninitialize();
            initialized = false;
        }
    }

    std::string queryString(const std::wstring& query, const std::wstring& property) {
        if (!initialized) return "";

        IEnumWbemClassObject* pEnumerator = nullptr;
        HRESULT hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t(query.c_str()),
                                      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                      nullptr, &pEnumerator);

        if (FAILED(hres)) return "";

        IWbemClassObject* pclsObj = nullptr;
        ULONG uReturn = 0;
        std::string result;

        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (uReturn == 0) break;

            VARIANT vtProp;
            hr = pclsObj->Get(property.c_str(), 0, &vtProp, nullptr, nullptr);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR && vtProp.bstrVal != nullptr) {
                result = _bstr_t(vtProp.bstrVal);
            }
            VariantClear(&vtProp);
            pclsObj->Release();
            break; // Get first result
        }

        pEnumerator->Release();
        return result;
    }

    std::vector<std::string> queryStringArray(const std::wstring& query, const std::wstring& property) {
        if (!initialized) return {};

        IEnumWbemClassObject* pEnumerator = nullptr;
        HRESULT hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t(query.c_str()),
                                      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                      nullptr, &pEnumerator);

        if (FAILED(hres)) return {};

        IWbemClassObject* pclsObj = nullptr;
        ULONG uReturn = 0;
        std::vector<std::string> results;

        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (uReturn == 0) break;

            VARIANT vtProp;
            hr = pclsObj->Get(property.c_str(), 0, &vtProp, nullptr, nullptr);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR && vtProp.bstrVal != nullptr) {
                results.push_back(_bstr_t(vtProp.bstrVal));
            }
            VariantClear(&vtProp);
            pclsObj->Release();
        }

        pEnumerator->Release();
        return results;
    }
};

// Forward declarations for enhanced Windows functions
auto getCurrentCpuUsage_Windows() -> float;
auto getPerCoreCpuUsage_Windows() -> std::vector<float>;
auto getCurrentCpuTemperature_Windows() -> float;
auto getPerCoreCpuTemperature_Windows() -> std::vector<float>;
auto getCPUModel_Windows() -> std::string;

auto getCurrentCpuUsage_Windows() -> float {
    spdlog::info("Invoking getCurrentCpuUsage_Windows to retrieve overall CPU usage on Windows.");

    static PDH_HQUERY cpuQuery = nullptr;
    static PDH_HCOUNTER cpuTotal = nullptr;
    static bool initialized = false;

    float cpuUsage = 0.0F;

    if (!initialized) {
        PdhOpenQuery(nullptr, 0, &cpuQuery);
        PdhAddEnglishCounter(cpuQuery, "\\Processor(_Total)\\% Processor Time",
                             0, &cpuTotal);
        PdhCollectQueryData(cpuQuery);
        initialized = true;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        PdhCollectQueryData(cpuQuery);
    }

    PDH_FMT_COUNTERVALUE counterVal;
    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, nullptr, &counterVal);
    cpuUsage = static_cast<float>(counterVal.doubleValue);

    cpuUsage = std::max(0.0F, std::min(100.0F, cpuUsage));

    spdlog::info("Overall CPU usage on Windows: {:.2f}%", cpuUsage);
    return cpuUsage;
}

auto getPerCoreCpuUsage() -> std::vector<float> {
    spdlog::info("Invoking getPerCoreCpuUsage to retrieve per-core CPU usage statistics on Windows.");

    static PDH_HQUERY cpuQuery = nullptr;
    static std::vector<PDH_HCOUNTER> cpuCounters;
    static bool initialized = false;

    int numCores = getNumberOfLogicalCores();
    std::vector<float> coreUsages(numCores, 0.0F);

    if (!initialized) {
        PdhOpenQuery(nullptr, 0, &cpuQuery);
        cpuCounters.resize(numCores);

        for (int i = 0; i < numCores; i++) {
            std::string counterPath =
                "\\Processor(" + std::to_string(i) + ")\\% Processor Time";
            PdhAddEnglishCounter(cpuQuery, counterPath.c_str(), 0,
                                 &cpuCounters[i]);
        }

        PdhCollectQueryData(cpuQuery);
        initialized = true;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        PdhCollectQueryData(cpuQuery);
    }

    PdhCollectQueryData(cpuQuery);

    for (int i = 0; i < numCores; i++) {
        PDH_FMT_COUNTERVALUE counterVal;
        PdhGetFormattedCounterValue(cpuCounters[i], PDH_FMT_DOUBLE, nullptr,
                                    &counterVal);
        coreUsages[i] = static_cast<float>(counterVal.doubleValue);
        coreUsages[i] = std::max(0.0F, std::min(100.0F, coreUsages[i]));
    }

    spdlog::info("Collected per-core CPU usage for {} logical cores on Windows.", numCores);
    return coreUsages;
}

auto getCurrentCpuTemperature() -> float {
    spdlog::info("Invoking getCurrentCpuTemperature to retrieve CPU temperature on Windows using WMI.");

    static std::atomic<float> cached_temp{0.0f};
    static std::atomic<std::chrono::steady_clock::time_point> last_temp_read{};
    constexpr auto TEMP_CACHE_DURATION = std::chrono::seconds(2);

    auto now = std::chrono::steady_clock::now();
    auto last_read = last_temp_read.load(std::memory_order_acquire);

    if (now - last_read < TEMP_CACHE_DURATION) {
        float temp = cached_temp.load(std::memory_order_acquire);
        if (temp > 0.0f) {
            spdlog::debug("Using cached CPU temperature: {:.2f}°C", temp);
            return temp;
        }
    }

    float temperature = 0.0f;

    try {
        WmiHelper wmi;

        // Try different WMI classes for temperature
        std::vector<std::wstring> temp_queries = {
            L"SELECT * FROM MSAcpi_ThermalZoneTemperature",
            L"SELECT * FROM Win32_TemperatureProbe",
            L"SELECT * FROM CIM_TemperatureSensor"
        };

        for (const auto& query : temp_queries) {
            std::string temp_str;
            if (query.find(L"MSAcpi_ThermalZoneTemperature") != std::wstring::npos) {
                temp_str = wmi.queryString(query, L"CurrentTemperature");
                if (!temp_str.empty()) {
                    // MSAcpi returns temperature in tenths of Kelvin
                    double temp_kelvin = std::stod(temp_str) / 10.0;
                    temperature = static_cast<float>(temp_kelvin - 273.15);
                    break;
                }
            } else {
                temp_str = wmi.queryString(query, L"CurrentReading");
                if (!temp_str.empty()) {
                    temperature = std::stof(temp_str);
                    break;
                }
            }
        }

        // Fallback: Try to read from Open Hardware Monitor if available
        if (temperature <= 0.0f) {
            std::string ohm_temp = wmi.queryString(
                L"SELECT * FROM Sensor WHERE SensorType='Temperature' AND Name LIKE '%CPU%'",
                L"Value");
            if (!ohm_temp.empty()) {
                temperature = std::stof(ohm_temp);
            }
        }

        // Validate temperature range
        if (temperature > 0.0f && temperature < 150.0f) {
            cached_temp.store(temperature, std::memory_order_release);
            last_temp_read.store(now, std::memory_order_release);
        } else {
            temperature = 0.0f; // Invalid reading
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in getCurrentCpuTemperature: {}", e.what());
        temperature = 0.0f;
    }

    spdlog::info("CPU temperature on Windows: {:.2f}°C", temperature);
    return temperature;
}

auto getPerCoreCpuTemperature() -> std::vector<float> {
    spdlog::info("Invoking getPerCoreCpuTemperature to retrieve per-core CPU temperatures on Windows (placeholder implementation).");

    int numCores = getNumberOfLogicalCores();
    std::vector<float> temperatures(numCores, 0.0F);

    spdlog::info("Per-core CPU temperatures on Windows: placeholder values for {} logical cores.", numCores);
    return temperatures;
}

auto getCPUModel() -> std::string {
    spdlog::info("Invoking getCPUModel to retrieve the CPU model string on Windows.");

    if (!needsCacheRefresh() && !g_cpuInfoCache.model.empty()) {
        return g_cpuInfoCache.model;
    }

    std::string cpuModel = "Unknown";

    int cpuInfo[4] = {-1};
    char cpuBrandString[64] = {0};

    __cpuid(cpuInfo, 0x80000000);
    unsigned int nExIds = cpuInfo[0];

    if (nExIds >= 0x80000004) {
        for (unsigned int i = 0x80000002; i <= 0x80000004; i++) {
            __cpuid(cpuInfo, i);
            memcpy(cpuBrandString + (i - 0x80000002) * 16, cpuInfo,
                   sizeof(cpuInfo));
        }
        cpuModel = cpuBrandString;
    }

    cpuModel.erase(0, cpuModel.find_first_not_of(" \t\n\r\f\v"));
    cpuModel.erase(cpuModel.find_last_not_of(" \t\n\r\f\v") + 1);

    spdlog::info("Detected CPU model on Windows: {}", cpuModel);
    return cpuModel;
}

auto getProcessorIdentifier() -> std::string {
    spdlog::info("Invoking getProcessorIdentifier to retrieve the processor identifier on Windows.");

    if (!needsCacheRefresh() && !g_cpuInfoCache.identifier.empty()) {
        return g_cpuInfoCache.identifier;
    }

    std::string identifier = "Unknown";

    int cpuInfo[4] = {0};
    char vendorID[13] = {0};

    __cpuid(cpuInfo, 0);
    memcpy(vendorID, &cpuInfo[1], sizeof(int));
    memcpy(vendorID + 4, &cpuInfo[3], sizeof(int));
    memcpy(vendorID + 8, &cpuInfo[2], sizeof(int));
    vendorID[12] = '\0';

    __cpuid(cpuInfo, 1);
    int family = (cpuInfo[0] >> 8) & 0xF;
    int model = (cpuInfo[0] >> 4) & 0xF;
    int extModel = (cpuInfo[0] >> 16) & 0xF;
    int extFamily = (cpuInfo[0] >> 20) & 0xFF;
    int stepping = cpuInfo[0] & 0xF;

    if (family == 0xF) {
        family += extFamily;
    }

    if (family == 0x6 || family == 0xF) {
        model = (extModel << 4) | model;
    }

    identifier = std::string(vendorID) + " Family " + std::to_string(family) +
                 " Model " + std::to_string(model) + " Stepping " +
                 std::to_string(stepping);

    spdlog::info("Constructed processor identifier on Windows: {}", identifier);
    return identifier;
}

auto getProcessorFrequency() -> double {
    spdlog::info("Invoking getProcessorFrequency to retrieve the current CPU frequency on Windows.");

    DWORD bufSize = sizeof(DWORD);
    DWORD mhz = 0;

    if (RegGetValue(HKEY_LOCAL_MACHINE,
                    "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                    "~MHz", RRF_RT_REG_DWORD, nullptr, &mhz,
                    &bufSize) == ERROR_SUCCESS) {
        double frequency = static_cast<double>(mhz) / 1000.0;
        spdlog::info("Current CPU frequency on Windows: {:.3f} GHz", frequency);
        return frequency;
    }

    spdlog::warn("Failed to retrieve current CPU frequency on Windows.");
    return 0.0;
}

auto getMinProcessorFrequency() -> double {
    spdlog::info("Invoking getMinProcessorFrequency to retrieve the minimum CPU frequency on Windows.");

    double minFreq = 0.0;

    double currentFreq = getProcessorFrequency();
    if (currentFreq > 0) {
        minFreq = currentFreq * 0.5;
        spdlog::info("Estimated minimum CPU frequency as half of current: {:.3f} GHz", minFreq);
    }

    spdlog::info("Minimum CPU frequency on Windows: {:.3f} GHz (estimated)", minFreq);
    return minFreq;
}

auto getMaxProcessorFrequency() -> double {
    spdlog::info("Invoking getMaxProcessorFrequency to retrieve the maximum CPU frequency on Windows.");

    DWORD bufSize = sizeof(DWORD);
    DWORD mhz = 0;

    if (RegGetValue(HKEY_LOCAL_MACHINE,
                    "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                    "~MHz", RRF_RT_REG_DWORD, nullptr, &mhz,
                    &bufSize) == ERROR_SUCCESS) {
        double frequency = static_cast<double>(mhz) / 1000.0;
        spdlog::info("Maximum CPU frequency on Windows: {:.3f} GHz", frequency);
        return frequency;
    }

    spdlog::warn("Failed to retrieve maximum CPU frequency on Windows. Using current frequency as fallback.");
    return getProcessorFrequency();
}

auto getPerCoreFrequencies() -> std::vector<double> {
    spdlog::info("Invoking getPerCoreFrequencies to retrieve per-core CPU frequencies on Windows.");

    int numCores = getNumberOfLogicalCores();
    std::vector<double> frequencies(numCores, 0.0);

    double frequency = getProcessorFrequency();

    for (int i = 0; i < numCores; i++) {
        frequencies[i] = frequency;
    }

    spdlog::info("Assigned CPU frequency {:.3f} GHz to all {} logical cores on Windows.", frequency, numCores);
    return frequencies;
}

auto getNumberOfPhysicalPackages() -> int {
    spdlog::info("Invoking getNumberOfPhysicalPackages to determine the number of physical CPU packages on Windows.");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalPackages > 0) {
        return g_cpuInfoCache.numPhysicalPackages;
    }

    int numberOfPackages = 1;

    spdlog::info("Number of physical CPU packages detected on Windows: {}", numberOfPackages);
    return numberOfPackages;
}

auto getNumberOfPhysicalCores() -> int {
    spdlog::info("Invoking getNumberOfPhysicalCores to determine the number of physical CPU cores on Windows.");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalCores > 0) {
        return g_cpuInfoCache.numPhysicalCores;
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    int numberOfCores = sysInfo.dwNumberOfProcessors;

    bool hasHyperthreading = false;

    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    hasHyperthreading = (cpuInfo[3] & (1 << 28)) != 0;

    if (hasHyperthreading && numberOfCores > 1) {
        numberOfCores = numberOfCores / 2;
    }

    numberOfCores = std::max(1, numberOfCores);

    spdlog::info("Number of physical CPU cores detected on Windows: {}", numberOfCores);
    return numberOfCores;
}

auto getNumberOfLogicalCores() -> int {
    spdlog::info("Invoking getNumberOfLogicalCores to determine the number of logical CPU cores on Windows.");

    if (!needsCacheRefresh() && g_cpuInfoCache.numLogicalCores > 0) {
        return g_cpuInfoCache.numLogicalCores;
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    int numberOfCores = sysInfo.dwNumberOfProcessors;

    spdlog::info("Number of logical CPU cores detected on Windows: {}", numberOfCores);
    return numberOfCores;
}

auto getCacheSizes() -> CacheSizes {
    spdlog::info("Invoking getCacheSizes to retrieve CPU cache sizes on Windows.");

    if (!needsCacheRefresh() &&
        (g_cpuInfoCache.caches.l1d > 0 || g_cpuInfoCache.caches.l2 > 0 ||
         g_cpuInfoCache.caches.l3 > 0)) {
        return g_cpuInfoCache.caches;
    }

    CacheSizes cacheSizes{};
    cacheSizes.l1d = cacheSizes.l1i = cacheSizes.l2 = cacheSizes.l3 = 0;
    cacheSizes.l1d_line_size = cacheSizes.l1i_line_size = cacheSizes.l2_line_size = cacheSizes.l3_line_size = 0;
    cacheSizes.l1d_associativity = cacheSizes.l1i_associativity = cacheSizes.l2_associativity = cacheSizes.l3_associativity = 0;

    int cpuInfo[4] = {0};

    __cpuid(cpuInfo, 0);
    int maxFunc = cpuInfo[0];

    if (maxFunc >= 4) {
        for (int i = 0;; i++) {
            __cpuidex(cpuInfo, 4, i);

            if ((cpuInfo[0] & 0x1F) == 0)
                break;

            int level = (cpuInfo[0] >> 5) & 0x7;
            int type = cpuInfo[0] & 0x1F;
            int lineSize = (cpuInfo[1] & 0xFFF) + 1;
            int associativity = ((cpuInfo[1] >> 22) & 0x3FF) + 1;
            int sets = cpuInfo[2] + 1;
            int totalSize = (associativity * lineSize * sets);

            switch (level) {
                case 1:
                    if (type == 1) {
                        cacheSizes.l1d = totalSize;
                        cacheSizes.l1d_line_size = lineSize;
                        cacheSizes.l1d_associativity = associativity;
                    } else if (type == 2) {
                        cacheSizes.l1i = totalSize;
                        cacheSizes.l1i_line_size = lineSize;
                        cacheSizes.l1i_associativity = associativity;
                    }
                    break;
                case 2:
                    cacheSizes.l2 = totalSize;
                    cacheSizes.l2_line_size = lineSize;
                    cacheSizes.l2_associativity = associativity;
                    break;
                case 3:
                    cacheSizes.l3 = totalSize;
                    cacheSizes.l3_line_size = lineSize;
                    cacheSizes.l3_associativity = associativity;
                    break;
            }
        }
    }

    spdlog::info("Cache sizes on Windows: L1d={} KB, L1i={} KB, L2={} KB, L3={} KB",
          cacheSizes.l1d / 1024, cacheSizes.l1i / 1024, cacheSizes.l2 / 1024,
          cacheSizes.l3 / 1024);

    return cacheSizes;
}

auto getCpuLoadAverage() -> LoadAverage {
    spdlog::info("Invoking getCpuLoadAverage to retrieve system load averages on Windows (approximated from CPU usage).");

    LoadAverage loadAvg{};
    loadAvg.oneMinute = loadAvg.fiveMinutes = loadAvg.fifteenMinutes = 0.0;
    loadAvg.running_processes = loadAvg.total_processes = 0;

    float cpuUsage = getCurrentCpuUsage();

    int numCores = getNumberOfLogicalCores();
    double load = (cpuUsage / 100.0) * numCores;

    loadAvg.oneMinute = load;
    loadAvg.fiveMinutes = load;
    loadAvg.fifteenMinutes = load;

    spdlog::info("System load averages on Windows (approximated): 1min={:.2f}, 5min={:.2f}, 15min={:.2f}",
          loadAvg.oneMinute, loadAvg.fiveMinutes, loadAvg.fifteenMinutes);

    return loadAvg;
}

auto getCpuPowerInfo() -> CpuPowerInfo {
    spdlog::info("Invoking getCpuPowerInfo to retrieve CPU power information on Windows using WMI and power APIs.");

    CpuPowerInfo powerInfo{};
    powerInfo.currentWatts = powerInfo.maxTDP = powerInfo.energyImpact = 0.0;
    powerInfo.baseWatts = powerInfo.peakWatts = powerInfo.averageWatts = 0.0;
    powerInfo.voltage = powerInfo.current = powerInfo.power_efficiency = 0.0;
    powerInfo.energy_consumed = 0;
    powerInfo.power_limit_throttling = false;

    try {
        WmiHelper wmi;

        // Get TDP information from processor
        std::string tdp_str = wmi.queryString(
            L"SELECT * FROM Win32_Processor", L"MaxClockSpeed");
        if (!tdp_str.empty()) {
            // Estimate TDP based on max clock speed (rough approximation)
            double max_mhz = std::stod(tdp_str);
            powerInfo.maxTDP = (max_mhz / 1000.0) * 15.0; // Rough estimate: 15W per GHz
        }

        // Try to get power consumption from WMI
        std::string power_str = wmi.queryString(
            L"SELECT * FROM Win32_PerfRawData_Counters_ProcessorInformation WHERE Name='_Total'",
            L"ProcessorPowerMHz");
        if (!power_str.empty()) {
            double power_mhz = std::stod(power_str);
            powerInfo.currentWatts = (power_mhz / 1000.0) * 10.0; // Rough estimate
        }

        // Get power scheme information
        GUID* activeScheme = nullptr;
        if (PowerGetActiveScheme(nullptr, &activeScheme) == ERROR_SUCCESS) {
            LPWSTR schemeName = nullptr;
            DWORD schemeNameSize = 0;

            if (PowerReadFriendlyName(nullptr, activeScheme, nullptr, nullptr,
                                    nullptr, &schemeNameSize) == ERROR_SUCCESS) {
                schemeName = new WCHAR[schemeNameSize];
                if (PowerReadFriendlyName(nullptr, activeScheme, nullptr, nullptr,
                                        (PUCHAR)schemeName, &schemeNameSize) == ERROR_SUCCESS) {
                    std::wstring ws(schemeName);
                    powerInfo.power_profile = std::string(ws.begin(), ws.end());
                }
                delete[] schemeName;
            }
            LocalFree(activeScheme);
        }

        // Check for power limit throttling
        std::string throttle_str = wmi.queryString(
            L"SELECT * FROM Win32_PerfRawData_Counters_ProcessorInformation WHERE Name='_Total'",
            L"ProcessorPerformanceLimitFlags");
        if (!throttle_str.empty()) {
            int throttle_flags = std::stoi(throttle_str);
            powerInfo.power_limit_throttling = (throttle_flags & 0x02) != 0; // Power limit throttling flag
        }

        // Calculate power efficiency (performance per watt)
        if (powerInfo.currentWatts > 0) {
            float cpu_usage = getCurrentCpuUsage();
            powerInfo.power_efficiency = cpu_usage / powerInfo.currentWatts;
        }

        spdlog::info("Windows CPU Power: Current={:.2f}W, TDP={:.2f}W, Profile={}, Throttling={}",
                     powerInfo.currentWatts, powerInfo.maxTDP, powerInfo.power_profile,
                     powerInfo.power_limit_throttling);

    } catch (const std::exception& e) {
        spdlog::error("Exception in getCpuPowerInfo: {}", e.what());
    }

    return powerInfo;
}

auto getCpuFeatureFlags() -> std::vector<std::string> {
    spdlog::info("Invoking getCpuFeatureFlags to retrieve CPU feature flags on Windows.");

    if (!needsCacheRefresh() && !g_cpuInfoCache.flags.empty()) {
        return g_cpuInfoCache.flags;
    }

    std::vector<std::string> flags;

    int cpuInfo[4] = {0};

    __cpuid(cpuInfo, 1);

    if (cpuInfo[3] & (1 << 0))
        flags.push_back("fpu");
    if (cpuInfo[3] & (1 << 1))
        flags.push_back("vme");
    if (cpuInfo[3] & (1 << 2))
        flags.push_back("de");
    if (cpuInfo[3] & (1 << 3))
        flags.push_back("pse");
    if (cpuInfo[3] & (1 << 4))
        flags.push_back("tsc");
    if (cpuInfo[3] & (1 << 5))
        flags.push_back("msr");
    if (cpuInfo[3] & (1 << 6))
        flags.push_back("pae");
    if (cpuInfo[3] & (1 << 7))
        flags.push_back("mce");
    if (cpuInfo[3] & (1 << 8))
        flags.push_back("cx8");
    if (cpuInfo[3] & (1 << 9))
        flags.push_back("apic");
    if (cpuInfo[3] & (1 << 11))
        flags.push_back("sep");
    if (cpuInfo[3] & (1 << 12))
        flags.push_back("mtrr");
    if (cpuInfo[3] & (1 << 13))
        flags.push_back("pge");
    if (cpuInfo[3] & (1 << 14))
        flags.push_back("mca");
    if (cpuInfo[3] & (1 << 15))
        flags.push_back("cmov");
    if (cpuInfo[3] & (1 << 16))
        flags.push_back("pat");
    if (cpuInfo[3] & (1 << 17))
        flags.push_back("pse36");
    if (cpuInfo[3] & (1 << 18))
        flags.push_back("psn");
    if (cpuInfo[3] & (1 << 19))
        flags.push_back("clfsh");
    if (cpuInfo[3] & (1 << 21))
        flags.push_back("ds");
    if (cpuInfo[3] & (1 << 22))
        flags.push_back("acpi");
    if (cpuInfo[3] & (1 << 23))
        flags.push_back("mmx");
    if (cpuInfo[3] & (1 << 24))
        flags.push_back("fxsr");
    if (cpuInfo[3] & (1 << 25))
        flags.push_back("sse");
    if (cpuInfo[3] & (1 << 26))
        flags.push_back("sse2");
    if (cpuInfo[3] & (1 << 27))
        flags.push_back("ss");
    if (cpuInfo[3] & (1 << 28))
        flags.push_back("htt");
    if (cpuInfo[3] & (1 << 29))
        flags.push_back("tm");
    if (cpuInfo[3] & (1 << 31))
        flags.push_back("pbe");

    if (cpuInfo[2] & (1 << 0))
        flags.push_back("sse3");
    if (cpuInfo[2] & (1 << 1))
        flags.push_back("pclmulqdq");
    if (cpuInfo[2] & (1 << 3))
        flags.push_back("monitor");
    if (cpuInfo[2] & (1 << 4))
        flags.push_back("ds_cpl");
    if (cpuInfo[2] & (1 << 5))
        flags.push_back("vmx");
    if (cpuInfo[2] & (1 << 6))
        flags.push_back("smx");
    if (cpuInfo[2] & (1 << 7))
        flags.push_back("est");
    if (cpuInfo[2] & (1 << 8))
        flags.push_back("tm2");
    if (cpuInfo[2] & (1 << 9))
        flags.push_back("ssse3");
    if (cpuInfo[2] & (1 << 13))
        flags.push_back("cx16");
    if (cpuInfo[2] & (1 << 19))
        flags.push_back("sse4_1");
    if (cpuInfo[2] & (1 << 20))
        flags.push_back("sse4_2");
    if (cpuInfo[2] & (1 << 21))
        flags.push_back("x2apic");
    if (cpuInfo[2] & (1 << 22))
        flags.push_back("movbe");
    if (cpuInfo[2] & (1 << 23))
        flags.push_back("popcnt");
    if (cpuInfo[2] & (1 << 25))
        flags.push_back("aes");
    if (cpuInfo[2] & (1 << 26))
        flags.push_back("xsave");
    if (cpuInfo[2] & (1 << 28))
        flags.push_back("avx");
    if (cpuInfo[2] & (1 << 29))
        flags.push_back("f16c");
    if (cpuInfo[2] & (1 << 30))
        flags.push_back("rdrnd");

    __cpuid(cpuInfo, 0x80000000);
    unsigned int nExIds = cpuInfo[0];

    if (nExIds >= 0x80000001) {
        __cpuid(cpuInfo, 0x80000001);

        if (cpuInfo[3] & (1 << 11))
            flags.push_back("syscall");
        if (cpuInfo[3] & (1 << 20))
            flags.push_back("nx");
        if (cpuInfo[3] & (1 << 29))
            flags.push_back("lm");

        if (cpuInfo[2] & (1 << 0))
            flags.push_back("lahf_lm");
        if (cpuInfo[2] & (1 << 5))
            flags.push_back("abm");
        if (cpuInfo[2] & (1 << 6))
            flags.push_back("sse4a");
        if (cpuInfo[2] & (1 << 8))
            flags.push_back("3dnowprefetch");
        if (cpuInfo[2] & (1 << 11))
            flags.push_back("xop");
        if (cpuInfo[2] & (1 << 12))
            flags.push_back("fma4");
    }

    __cpuidex(cpuInfo, 7, 0);

    if (cpuInfo[1] & (1 << 5))
        flags.push_back("avx2");
    if (cpuInfo[1] & (1 << 3))
        flags.push_back("bmi1");
    if (cpuInfo[1] & (1 << 8))
        flags.push_back("bmi2");

    if (cpuInfo[1] & (1 << 16))
        flags.push_back("avx512f");
    if (cpuInfo[1] & (1 << 17))
        flags.push_back("avx512dq");
    if (cpuInfo[1] & (1 << 21))
        flags.push_back("avx512ifma");
    if (cpuInfo[1] & (1 << 26))
        flags.push_back("avx512pf");
    if (cpuInfo[1] & (1 << 27))
        flags.push_back("avx512er");
    if (cpuInfo[1] & (1 << 28))
        flags.push_back("avx512cd");
    if (cpuInfo[1] & (1 << 30))
        flags.push_back("avx512bw");
    if (cpuInfo[1] & (1 << 31))
        flags.push_back("avx512vl");

    if (cpuInfo[2] & (1 << 1))
        flags.push_back("avx512vbmi");
    if (cpuInfo[2] & (1 << 6))
        flags.push_back("avx512vbmi2");

    spdlog::info("Collected {} CPU feature flags on Windows.", flags.size());

    return flags;
}

auto getCpuArchitecture() -> CpuArchitecture {
    spdlog::info("Invoking getCpuArchitecture to determine the CPU architecture on Windows.");

    if (!needsCacheRefresh()) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheInitialized &&
            g_cpuInfoCache.architecture != CpuArchitecture::UNKNOWN) {
            return g_cpuInfoCache.architecture;
        }
    }

    CpuArchitecture arch = CpuArchitecture::UNKNOWN;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            arch = CpuArchitecture::X86_64;
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            arch = CpuArchitecture::X86;
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            arch = CpuArchitecture::ARM;
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            arch = CpuArchitecture::ARM64;
            break;
        default:
            arch = CpuArchitecture::UNKNOWN;
            break;
    }

    spdlog::info("Detected CPU architecture on Windows: {}", cpuArchitectureToString(arch));
    return arch;
}

auto getCpuVendor() -> CpuVendor {
    spdlog::info("Invoking getCpuVendor to determine the CPU vendor on Windows.");

    if (!needsCacheRefresh()) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheInitialized && g_cpuInfoCache.vendor != CpuVendor::UNKNOWN) {
            return g_cpuInfoCache.vendor;
        }
    }

    CpuVendor vendor = CpuVendor::UNKNOWN;
    std::string vendorString;

    int cpuInfo[4] = {0};
    char vendorID[13] = {0};

    __cpuid(cpuInfo, 0);
    memcpy(vendorID, &cpuInfo[1], sizeof(int));
    memcpy(vendorID + 4, &cpuInfo[3], sizeof(int));
    memcpy(vendorID + 8, &cpuInfo[2], sizeof(int));
    vendorID[12] = '\0';

    vendorString = vendorID;
    vendor = getVendorFromString(vendorString);

    spdlog::info("Detected CPU vendor on Windows: {} ({})", vendorString, cpuVendorToString(vendor));
    return vendor;
}

auto getCpuSocketType() -> std::string {
    spdlog::info("Invoking getCpuSocketType to retrieve the CPU socket type on Windows (placeholder implementation).");

    if (!needsCacheRefresh() && !g_cpuInfoCache.socketType.empty()) {
        return g_cpuInfoCache.socketType;
    }

    std::string socketType = "Unknown";

    spdlog::info("CPU socket type on Windows: {} (no direct method available, placeholder value)", socketType);
    return socketType;
}

auto getCpuScalingGovernor() -> std::string {
    spdlog::info("Invoking getCpuScalingGovernor to retrieve the CPU scaling governor (power plan) on Windows.");

    std::string governor = "Unknown";

    GUID* activePlanGuid = NULL;
    if (PowerGetActiveScheme(NULL, &activePlanGuid) == ERROR_SUCCESS) {
        DWORD bufferSize = 0;
        PowerReadFriendlyName(NULL, activePlanGuid, NULL, NULL, NULL,
                              &bufferSize);

        if (bufferSize > 0) {
            std::vector<BYTE> buffer(bufferSize);

            if (PowerReadFriendlyName(NULL, activePlanGuid, NULL, NULL,
                                      buffer.data(),
                                      &bufferSize) == ERROR_SUCCESS) {
                LPWSTR friendlyName = reinterpret_cast<LPWSTR>(buffer.data());

                int narrowBufferSize = WideCharToMultiByte(
                    CP_UTF8, 0, friendlyName, -1, NULL, 0, NULL, NULL);
                if (narrowBufferSize > 0) {
                    std::vector<char> narrowBuffer(narrowBufferSize);
                    if (WideCharToMultiByte(CP_UTF8, 0, friendlyName, -1,
                                            narrowBuffer.data(),
                                            narrowBufferSize, NULL, NULL) > 0) {
                        governor = narrowBuffer.data();
                    }
                }
            }
        }

        LocalFree(activePlanGuid);
    }

    spdlog::info("CPU scaling governor (power plan) on Windows: {}", governor);
    return governor;
}

auto getPerCoreScalingGovernors() -> std::vector<std::string> {
    spdlog::info("Invoking getPerCoreScalingGovernors to retrieve per-core CPU scaling governors on Windows.");

    int numCores = getNumberOfLogicalCores();
    std::vector<std::string> governors(numCores);

    std::string governor = getCpuScalingGovernor();

    for (int i = 0; i < numCores; ++i) {
        governors[i] = governor;
    }

    spdlog::info("Assigned CPU scaling governor '{}' to all {} logical cores on Windows.", governor, numCores);
    return governors;
}

// Enhanced Windows-specific CPU functions for new features

/**
 * @brief Get CPU topology information using Windows APIs
 * @return CpuTopology structure with detailed topology information
 */
auto getCpuTopology() -> CpuTopology {
    spdlog::debug("getCpuTopology_Windows: Reading CPU topology information");

    CpuTopology topology{};

    try {
        WmiHelper wmi;

        // Get processor information
        std::string num_cores_str = wmi.queryString(
            L"SELECT * FROM Win32_Processor", L"NumberOfCores");
        std::string num_logical_str = wmi.queryString(
            L"SELECT * FROM Win32_Processor", L"NumberOfLogicalProcessors");

        if (!num_cores_str.empty() && !num_logical_str.empty()) {
            int physical_cores = std::stoi(num_cores_str);
            int logical_cores = std::stoi(num_logical_str);

            topology.packages = 1; // Assume single package for now
            topology.cores_per_package = physical_cores;
            topology.threads_per_core = logical_cores / physical_cores;
            topology.hyperthreading_enabled = (topology.threads_per_core > 1);
        }

        // Get NUMA node information
        ULONG highest_node_number = 0;
        if (GetNumaHighestNodeNumber(&highest_node_number)) {
            topology.numa_nodes = highest_node_number + 1;
            topology.numa_enabled = (topology.numa_nodes > 1);

            // Build NUMA node CPU mapping
            topology.numa_node_cpus.resize(topology.numa_nodes);
            for (ULONG node = 0; node <= highest_node_number; ++node) {
                ULONGLONG processor_mask = 0;
                if (GetNumaNodeProcessorMask(node, &processor_mask)) {
                    for (int cpu = 0; cpu < 64; ++cpu) {
                        if (processor_mask & (1ULL << cpu)) {
                            topology.numa_node_cpus[node].push_back(cpu);
                        }
                    }
                }
            }
        }

        spdlog::info("Windows CPU Topology: {} packages, {} NUMA nodes, {} cores/package, {} threads/core",
                     topology.packages, topology.numa_nodes, topology.cores_per_package, topology.threads_per_core);

    } catch (const std::exception& e) {
        spdlog::error("Exception in getCpuTopology: {}", e.what());
    }

    return topology;
}

/**
 * @brief Get CPU security and vulnerability information for Windows
 * @return CpuSecurityInfo structure with security details
 */
auto getCpuSecurityInfo() -> CpuSecurityInfo {
    spdlog::debug("getCpuSecurityInfo_Windows: Reading CPU security information");

    CpuSecurityInfo security{};

    try {
        WmiHelper wmi;

        // Get processor features that relate to security
        std::vector<std::string> features = getCpuFeatureFlags();

        // Check for common security features
        for (const auto& feature : features) {
            CpuVulnerabilityInfo vuln_info;

            if (feature == "smep" || feature == "smap") {
                vuln_info.name = feature;
                vuln_info.status = "Mitigation: Hardware feature enabled";
                vuln_info.hardware_mitigation = true;
                vuln_info.software_mitigation = false;
                security.vulnerabilities.push_back(vuln_info);
            } else if (feature == "ibrs" || feature == "stibp") {
                vuln_info.name = "spectre_v2";
                vuln_info.status = "Mitigation: Hardware feature enabled";
                vuln_info.hardware_mitigation = true;
                vuln_info.software_mitigation = false;
                security.vulnerabilities.push_back(vuln_info);
            }
        }

        // Get microcode version from registry
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD dataSize = 256;
            WCHAR microcode[256];
            if (RegQueryValueEx(hKey, L"Update Revision", nullptr, nullptr,
                               (LPBYTE)microcode, &dataSize) == ERROR_SUCCESS) {
                std::wstring ws(microcode);
                security.microcode.version = std::string(ws.begin(), ws.end());
            }
            RegCloseKey(hKey);
        }

        // Check Windows security features
        security.secure_boot_enabled = false; // Would need to check UEFI variables

        spdlog::info("Windows CPU Security: Found {} vulnerabilities, microcode version: {}",
                     security.vulnerabilities.size(), security.microcode.version);

    } catch (const std::exception& e) {
        spdlog::error("Exception in getCpuSecurityInfo: {}", e.what());
    }

    return security;
}

/**
 * @brief Get enhanced thermal information for Windows
 * @return ThermalInfo structure with detailed thermal data
 */
auto getCpuThermalInfo() -> ThermalInfo {
    spdlog::debug("getCpuThermalInfo_Windows: Reading enhanced thermal information");

    ThermalInfo thermal{};
    thermal.current_temp = getCurrentCpuTemperature();
    thermal.thermal_throttling_active = false;
    thermal.throttle_state = ThermalThrottleState::NORMAL;

    try {
        WmiHelper wmi;

        // Try to get thermal zone information
        std::string critical_temp_str = wmi.queryString(
            L"SELECT * FROM Win32_TemperatureProbe", L"MaxReading");
        if (!critical_temp_str.empty()) {
            thermal.critical_temp = std::stof(critical_temp_str);
        }

        // Check for thermal throttling
        std::string throttle_str = wmi.queryString(
            L"SELECT * FROM Win32_PerfRawData_Counters_ProcessorInformation WHERE Name='_Total'",
            L"ProcessorPerformanceLimitFlags");
        if (!throttle_str.empty()) {
            int throttle_flags = std::stoi(throttle_str);
            thermal.thermal_throttling_active = (throttle_flags & 0x01) != 0; // Thermal throttling flag

            if (thermal.thermal_throttling_active) {
                thermal.throttle_state = ThermalThrottleState::MODERATE_THROTTLE;
            }
        }

        // Calculate thermal margin
        if (thermal.critical_temp > 0 && thermal.current_temp > 0) {
            thermal.thermal_margin = thermal.critical_temp - thermal.current_temp;
        }

        thermal.thermal_zone = "Windows Thermal Zone";

        spdlog::info("Windows CPU Thermal: Current: {:.1f}°C, Critical: {:.1f}°C, Margin: {:.1f}°C, Throttling: {}",
                     thermal.current_temp, thermal.critical_temp, thermal.thermal_margin, thermal.thermal_throttling_active);

    } catch (const std::exception& e) {
        spdlog::error("Exception in getCpuThermalInfo: {}", e.what());
    }

    return thermal;
}

/**
 * @brief Check if CPU is currently throttling on Windows
 * @return True if CPU is throttling due to thermal or power limits
 */
auto isCpuThrottling() -> bool {
    try {
        WmiHelper wmi;
        std::string throttle_str = wmi.queryString(
            L"SELECT * FROM Win32_PerfRawData_Counters_ProcessorInformation WHERE Name='_Total'",
            L"ProcessorPerformanceLimitFlags");

        if (!throttle_str.empty()) {
            int throttle_flags = std::stoi(throttle_str);
            return (throttle_flags & 0x03) != 0; // Thermal or power throttling
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception in isCpuThrottling: {}", e.what());
    }
    return false;
}

}  // namespace atom::system

#endif /* _WIN32 */
