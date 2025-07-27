/**
 * @file windows.cpp
 * @brief Windows-specific GPU functionality implementation
 *
 * This file contains Windows-specific implementations for GPU information
 * retrieval and monitoring using Windows APIs, WMI, and DirectX.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifdef _WIN32

#include "windows.hpp"
#include "common.hpp"
#include <spdlog/spdlog.h>

// clang-format off
#include <windows.h>
#include <versionHelpers.h>
#include <setupapi.h>
#include <devguid.h>
#include <wbemidl.h>
#include <comdef.h>
// clang-format on

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "wbemuuid.lib")

namespace atom::system {

namespace {
    // Helper function to initialize COM
    bool initializeCOM() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    }

    // Helper function to get WMI service
    IWbemServices* getWMIService() {
        static IWbemServices* pSvc = nullptr;
        if (pSvc) return pSvc;

        if (!initializeCOM()) return nullptr;

        IWbemLocator* pLoc = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                     IID_IWbemLocator, reinterpret_cast<LPVOID*>(&pLoc));
        if (FAILED(hr)) return nullptr;

        hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr,
                                0, nullptr, nullptr, &pSvc);
        pLoc->Release();

        if (FAILED(hr)) return nullptr;

        hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                              RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                              nullptr, EOAC_NONE);
        if (FAILED(hr)) {
            pSvc->Release();
            return nullptr;
        }

        return pSvc;
    }
}

auto getGPUInfoWindows() -> std::string {
    spdlog::info("Starting Windows GPU information retrieval");
    std::string gpuInfo;

    if (!IsWindows10OrGreater()) {
        spdlog::warn("Windows version not supported for GPU information retrieval");
        return "Windows version not supported for GPU information retrieval";
    }

    HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(nullptr, "DISPLAY", nullptr, DIGCF_PRESENT);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        spdlog::error("Failed to get GPU device information set");
        return "Failed to get GPU information";
    }

    SP_DEVINFO_DATA deviceInfoData{};
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); ++i) {
        CHAR buffer[4096];
        DWORD dataSize = sizeof(buffer);

        if (SetupDiGetDeviceRegistryPropertyA(deviceInfoSet, &deviceInfoData, SPDRP_DEVICEDESC,
                                             nullptr, reinterpret_cast<PBYTE>(buffer), dataSize, nullptr)) {
            if (!gpuInfo.empty()) {
                gpuInfo += "\n";
            }
            gpuInfo += buffer;
            spdlog::debug("Found GPU: {}", buffer);
        }
    }
    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    spdlog::info("Windows GPU information retrieval completed");
    return gpuInfo;
}

auto getDetailedGPUInfoWindows() -> std::vector<GPUInfo> {
    std::vector<GPUInfo> gpus;
    
    IWbemServices* pSvc = getWMIService();
    if (!pSvc) {
        spdlog::error("Failed to get WMI service for GPU information");
        return gpus;
    }

    IEnumWbemClassObject* pEnumerator = nullptr;
    HRESULT hr = pSvc->ExecQuery(bstr_t("WQL"), 
                                bstr_t("SELECT * FROM Win32_VideoController"),
                                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                nullptr, &pEnumerator);

    if (FAILED(hr)) {
        spdlog::error("WMI query failed for GPU information");
        return gpus;
    }

    IWbemClassObject* pclsObj = nullptr;
    ULONG uReturn = 0;
    int gpuIndex = 0;

    while (pEnumerator) {
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (uReturn == 0) break;

        GPUInfo gpu;
        gpu.gpuIndex = gpuIndex++;
        gpu.timestamp = std::chrono::steady_clock::now();

        VARIANT vtProp;
        VariantInit(&vtProp);

        // Get GPU name
        hr = pclsObj->Get(L"Name", 0, &vtProp, nullptr, nullptr);
        if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
            gpu.name = _com_util::ConvertBSTRToString(vtProp.bstrVal);
        }
        VariantClear(&vtProp);

        // Get device ID
        hr = pclsObj->Get(L"DeviceID", 0, &vtProp, nullptr, nullptr);
        if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
            gpu.deviceId = _com_util::ConvertBSTRToString(vtProp.bstrVal);
        }
        VariantClear(&vtProp);

        // Get adapter RAM
        hr = pclsObj->Get(L"AdapterRAM", 0, &vtProp, nullptr, nullptr);
        if (SUCCEEDED(hr) && vtProp.vt == VT_I4) {
            gpu.memoryInfo.totalMemory = static_cast<size_t>(vtProp.lVal);
        }
        VariantClear(&vtProp);

        // Get driver version
        hr = pclsObj->Get(L"DriverVersion", 0, &vtProp, nullptr, nullptr);
        if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
            gpu.driver.driverVersion = _com_util::ConvertBSTRToString(vtProp.bstrVal);
        }
        VariantClear(&vtProp);

        // Get driver date
        hr = pclsObj->Get(L"DriverDate", 0, &vtProp, nullptr, nullptr);
        if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
            gpu.driver.driverDate = _com_util::ConvertBSTRToString(vtProp.bstrVal);
        }
        VariantClear(&vtProp);

        // Parse vendor from name
        gpu.vendor = parseGPUVendor(gpu.name);
        gpu.architecture = parseGPUArchitecture(gpu.name, gpu.vendor);

        // Determine GPU type (simplified logic)
        if (gpu.name.find("Intel") != std::string::npos && 
            gpu.name.find("UHD") != std::string::npos) {
            gpu.type = GPUType::INTEGRATED;
        } else {
            gpu.type = GPUType::DISCRETE;
        }

        // Set memory info timestamp
        gpu.memoryInfo.timestamp = gpu.timestamp;
        gpu.performance.timestamp = gpu.timestamp;
        gpu.compute.timestamp = gpu.timestamp;
        gpu.driver.timestamp = gpu.timestamp;

        gpus.push_back(gpu);
        pclsObj->Release();
    }

    pEnumerator->Release();
    return gpus;
}

auto getGPUCountWindows() -> int {
    auto gpus = getDetailedGPUInfoWindows();
    return static_cast<int>(gpus.size());
}

auto getGPUInfoWindows(int gpuIndex) -> GPUInfo {
    auto gpus = getDetailedGPUInfoWindows();
    if (gpuIndex >= 0 && gpuIndex < static_cast<int>(gpus.size())) {
        return gpus[gpuIndex];
    }
    return GPUInfo{};
}

auto getGPUPerformanceMetricsWindows(int gpuIndex) -> GPUPerformanceMetrics {
    GPUPerformanceMetrics metrics;
    metrics.timestamp = std::chrono::steady_clock::now();
    
    // TODO: Implement performance metrics retrieval using Windows Performance Counters
    // This would require additional Windows APIs and performance counter queries
    
    return metrics;
}

auto getGPUMemoryInfoWindows(int gpuIndex) -> GPUMemoryInfo {
    auto gpu = getGPUInfoWindows(gpuIndex);
    return gpu.memoryInfo;
}

auto getGPUTemperatureWindows(int gpuIndex) -> double {
    // TODO: Implement temperature retrieval using WMI or vendor-specific APIs
    return 0.0;
}

auto getGPUUtilizationWindows(int gpuIndex) -> double {
    // TODO: Implement utilization retrieval using Windows Performance Counters
    return 0.0;
}

auto getAllMonitorsInfoWindows() -> std::vector<MonitorInfo> {
    spdlog::info("Starting Windows monitor information retrieval");
    std::vector<MonitorInfo> monitors;

    DISPLAY_DEVICE displayDevice{};
    displayDevice.cb = sizeof(displayDevice);

    for (int deviceIndex = 0; EnumDisplayDevices(nullptr, deviceIndex, &displayDevice, 0); ++deviceIndex) {
        if (displayDevice.StateFlags & DISPLAY_DEVICE_ACTIVE) {
            MonitorInfo info;
            info.model = std::string(displayDevice.DeviceString);
            info.identifier = std::string(displayDevice.DeviceName);
            info.timestamp = std::chrono::steady_clock::now();

            // Get resolution and refresh rate
            DEVMODE devMode{};
            devMode.dmSize = sizeof(devMode);

            if (EnumDisplaySettings(displayDevice.DeviceName, ENUM_CURRENT_SETTINGS, &devMode)) {
                info.width = static_cast<int>(devMode.dmPelsWidth);
                info.height = static_cast<int>(devMode.dmPelsHeight);
                info.refreshRate = static_cast<int>(devMode.dmDisplayFrequency);
                spdlog::debug("Monitor resolution: {}x{} @ {}Hz", info.width, info.height, info.refreshRate);
            } else {
                spdlog::error("Failed to get display settings for device: {}", displayDevice.DeviceName);
                info.width = info.height = info.refreshRate = 0;
            }

            monitors.emplace_back(std::move(info));
            spdlog::debug("Found monitor: {} ({}x{} @ {}Hz)", info.model, info.width, info.height, info.refreshRate);
        }

        ZeroMemory(&displayDevice, sizeof(displayDevice));
        displayDevice.cb = sizeof(displayDevice);
    }

    spdlog::info("Windows monitor information retrieval completed, found {} monitors", monitors.size());
    return monitors;
}

// Placeholder implementations for additional functions
auto getGPUDriverInfoWindows(int gpuIndex) -> GPUDriverInfo {
    auto gpu = getGPUInfoWindows(gpuIndex);
    return gpu.driver;
}

auto getGPUComputeCapabilityWindows(int gpuIndex) -> GPUComputeCapability {
    auto gpu = getGPUInfoWindows(gpuIndex);
    return gpu.compute;
}

auto benchmarkGPUWindows(int gpuIndex, const GPUBenchmarkConfig& config) -> GPUBenchmarkResults {
    GPUBenchmarkResults results;
    results.testTime = std::chrono::steady_clock::now();
    results.testDuration = config.duration;
    // TODO: Implement GPU benchmarking
    return results;
}

auto startGPUMonitoringWindows(int gpuIndex, const GPUMonitoringConfig& config,
                              std::function<void(const GPUPerformanceMetrics&)> callback) -> bool {
    // TODO: Implement GPU monitoring
    return false;
}

auto stopGPUMonitoringWindows(int gpuIndex) -> bool {
    // TODO: Implement stop monitoring
    return false;
}

// Additional placeholder implementations for other functions would go here...

}  // namespace atom::system

#endif  // _WIN32
