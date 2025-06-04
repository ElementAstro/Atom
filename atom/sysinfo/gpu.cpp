/*
 * gpu.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - GPU

**************************************************/

#include "atom/sysinfo/gpu.hpp"

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <versionHelpers.h>
#include <setupapi.h>
// clang-format on
#elif defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/IOKitLib.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#if __has_include(<X11/extensions/Xrandr.h>)
#include <X11/extensions/Xrandr.h>
#endif
#include <fstream>
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

auto getGPUInfo() -> std::string {
    spdlog::info("Starting GPU information retrieval");
    std::string gpuInfo;

#ifdef _WIN32
    if (!IsWindows10OrGreater()) {
        spdlog::warn(
            "Windows version not supported for GPU information retrieval");
        return "Windows version not supported for GPU information retrieval";
    }

    HDEVINFO deviceInfoSet =
        SetupDiGetClassDevsA(nullptr, "DISPLAY", nullptr, DIGCF_PRESENT);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        spdlog::error("Failed to get GPU device information set");
        return "Failed to get GPU information";
    }

    SP_DEVINFO_DATA deviceInfoData{};
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData);
         ++i) {
        CHAR buffer[4096];
        DWORD dataSize = sizeof(buffer);

        if (SetupDiGetDeviceRegistryPropertyA(
                deviceInfoSet, &deviceInfoData, SPDRP_DEVICEDESC, nullptr,
                reinterpret_cast<PBYTE>(buffer), dataSize, nullptr)) {
            if (!gpuInfo.empty()) {
                gpuInfo += "\n";
            }
            gpuInfo += buffer;
            spdlog::debug("Found GPU: {}", buffer);
        }
    }
    SetupDiDestroyDeviceInfoList(deviceInfoSet);

#elif defined(__linux__)
    std::ifstream file("/proc/driver/nvidia/gpus/0/information");
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (!gpuInfo.empty()) {
                gpuInfo += "\n";
            }
            gpuInfo += line;
        }
        spdlog::debug("Retrieved GPU info from NVIDIA driver");
    } else {
        spdlog::warn("Failed to open NVIDIA GPU information file");
        gpuInfo = "GPU information not available";
    }

#elif defined(__APPLE__)
    io_iterator_t iterator;
    CFMutableDictionaryRef matchDict = IOServiceMatching("IOPCIDevice");

    if (!matchDict) {
        spdlog::error("Failed to create IOPCIDevice matching dictionary");
        return "Failed to get GPU information";
    }

    kern_return_t kr = IOServiceGetMatchingServices(kIOMasterPortDefault,
                                                    matchDict, &iterator);
    if (kr != KERN_SUCCESS) {
        spdlog::error("IOServiceGetMatchingServices failed with error: {}", kr);
        return "Failed to get GPU information";
    }

    io_service_t service;
    std::string gpuNames;
    constexpr size_t bufferSize = 256;
    char buffer[bufferSize];

    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        CFTypeRef classCodeProp = IORegistryEntryCreateCFProperty(
            service, CFSTR("class-code"), kCFAllocatorDefault, 0);

        bool isDisplayController = false;
        if (classCodeProp && CFGetTypeID(classCodeProp) == CFDataGetTypeID()) {
            CFDataRef classCodeData = static_cast<CFDataRef>(classCodeProp);
            if (CFDataGetLength(classCodeData) > 0) {
                const UInt8* bytes = CFDataGetBytePtr(classCodeData);
                if (bytes[0] == 0x03) {
                    isDisplayController = true;
                }
            }
        }

        if (classCodeProp) {
            CFRelease(classCodeProp);
        }

        if (isDisplayController) {
            CFTypeRef modelProp = IORegistryEntryCreateCFProperty(
                service, CFSTR("model"), kCFAllocatorDefault, 0);

            if (modelProp && CFGetTypeID(modelProp) == CFStringGetTypeID()) {
                CFStringRef modelStr = static_cast<CFStringRef>(modelProp);
                const char* cStr =
                    CFStringGetCStringPtr(modelStr, kCFStringEncodingUTF8);

                if (cStr || CFStringGetCString(modelStr, buffer, bufferSize,
                                               kCFStringEncodingUTF8)) {
                    if (!gpuNames.empty()) {
                        gpuNames += "\n";
                    }
                    gpuNames += cStr ? cStr : buffer;
                }
            }

            if (modelProp) {
                CFRelease(modelProp);
            }
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);

    gpuInfo = gpuNames.empty() ? "No GPU found" : gpuNames;
    spdlog::debug("macOS GPU info: {}", gpuInfo);

#else
    gpuInfo = "GPU information not supported on this platform";
    spdlog::warn("GPU information retrieval not supported on this platform");
#endif

    spdlog::info("GPU information retrieval completed");
    return gpuInfo;
}

#ifdef _WIN32
namespace {
auto getMonitorModel(const DISPLAY_DEVICE& displayDevice) -> std::string {
    return std::string(displayDevice.DeviceString);
}

void getMonitorResolutionAndRefreshRate(const std::string& deviceName,
                                        int& width, int& height,
                                        int& refreshRate) {
    DEVMODE devMode{};
    devMode.dmSize = sizeof(devMode);

    if (EnumDisplaySettings(deviceName.c_str(), ENUM_CURRENT_SETTINGS,
                            &devMode)) {
        width = static_cast<int>(devMode.dmPelsWidth);
        height = static_cast<int>(devMode.dmPelsHeight);
        refreshRate = static_cast<int>(devMode.dmDisplayFrequency);
        spdlog::debug("Monitor resolution: {}x{} @ {}Hz", width, height,
                      refreshRate);
    } else {
        spdlog::error("Failed to get display settings for device: {}",
                      deviceName);
        width = height = refreshRate = 0;
    }
}
}  // namespace

auto getAllMonitorsInfo() -> std::vector<MonitorInfo> {
    spdlog::info("Starting monitor information retrieval");
    std::vector<MonitorInfo> monitors;

    DISPLAY_DEVICE displayDevice{};
    displayDevice.cb = sizeof(displayDevice);

    for (int deviceIndex = 0;
         EnumDisplayDevices(nullptr, deviceIndex, &displayDevice, 0);
         ++deviceIndex) {
        if (displayDevice.StateFlags & DISPLAY_DEVICE_ACTIVE) {
            MonitorInfo info;
            info.model = getMonitorModel(displayDevice);
            info.identifier = std::string(displayDevice.DeviceName);

            getMonitorResolutionAndRefreshRate(displayDevice.DeviceName,
                                               info.width, info.height,
                                               info.refreshRate);

            monitors.emplace_back(std::move(info));
            spdlog::debug("Found monitor: {} ({}x{} @ {}Hz)", info.model,
                          info.width, info.height, info.refreshRate);
        }

        ZeroMemory(&displayDevice, sizeof(displayDevice));
        displayDevice.cb = sizeof(displayDevice);
    }

    spdlog::info("Monitor information retrieval completed, found {} monitors",
                 monitors.size());
    return monitors;
}

#elif defined(__linux__)
auto getAllMonitorsInfo() -> std::vector<MonitorInfo> {
    spdlog::info("Starting Linux monitor information retrieval");
    std::vector<MonitorInfo> monitors;

#if __has_include(<X11/extensions/Xrandr.h>)
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        spdlog::error("Unable to open X display");
        return monitors;
    }

    Window root = DefaultRootWindow(display);
    XRRScreenResources* screenRes = XRRGetScreenResources(display, root);
    if (!screenRes) {
        XCloseDisplay(display);
        spdlog::error("Unable to get X screen resources");
        return monitors;
    }

    for (int i = 0; i < screenRes->noutput; ++i) {
        XRROutputInfo* outputInfo =
            XRRGetOutputInfo(display, screenRes, screenRes->outputs[i]);
        if (!outputInfo || outputInfo->connection == RR_Disconnected) {
            if (outputInfo)
                XRRFreeOutputInfo(outputInfo);
            continue;
        }

        MonitorInfo info;
        info.model = std::string(outputInfo->name);
        info.identifier = std::string(outputInfo->name);

        if (outputInfo->crtc) {
            XRRCrtcInfo* crtcInfo =
                XRRGetCrtcInfo(display, screenRes, outputInfo->crtc);
            if (crtcInfo) {
                info.width = static_cast<int>(crtcInfo->width);
                info.height = static_cast<int>(crtcInfo->height);

                if (crtcInfo->mode != None) {
                    for (int j = 0; j < screenRes->nmode; ++j) {
                        if (screenRes->modes[j].id == crtcInfo->mode) {
                            const XRRModeInfo& mode = screenRes->modes[j];
                            info.refreshRate = static_cast<int>(
                                static_cast<double>(mode.dotClock) /
                                (static_cast<double>(mode.hTotal) *
                                 mode.vTotal));
                            break;
                        }
                    }
                }

                XRRFreeCrtcInfo(crtcInfo);
                spdlog::debug("Found Linux monitor: {} ({}x{} @ {}Hz)",
                              info.model, info.width, info.height,
                              info.refreshRate);
            }
        }

        monitors.emplace_back(std::move(info));
        XRRFreeOutputInfo(outputInfo);
    }

    XRRFreeScreenResources(screenRes);
    XCloseDisplay(display);
    spdlog::info(
        "Linux monitor information retrieval completed, found {} monitors",
        monitors.size());
#else
    spdlog::error("Xrandr extension not available");
#endif
    return monitors;
}

#elif defined(__APPLE__)
auto getAllMonitorsInfo() -> std::vector<MonitorInfo> {
    spdlog::info("Starting macOS monitor information retrieval");
    std::vector<MonitorInfo> monitors;

    uint32_t displayCount;
    CGGetActiveDisplayList(0, nullptr, &displayCount);

    if (displayCount == 0) {
        spdlog::warn("No active displays found");
        return monitors;
    }

    std::vector<CGDirectDisplayID> displays(displayCount);
    CGGetActiveDisplayList(displayCount, displays.data(), &displayCount);

    monitors.reserve(displayCount);
    for (uint32_t i = 0; i < displayCount; ++i) {
        CGDirectDisplayID displayID = displays[i];
        MonitorInfo info;

        info.identifier = std::to_string(displayID);
        info.width = static_cast<int>(CGDisplayPixelsWide(displayID));
        info.height = static_cast<int>(CGDisplayPixelsHigh(displayID));

        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayID);
        if (mode) {
            info.refreshRate =
                static_cast<int>(CGDisplayModeGetRefreshRate(mode));
            CGDisplayModeRelease(mode);
        }

        info.model = "Display";
        monitors.emplace_back(std::move(info));

        spdlog::debug("Found macOS monitor: {} ({}x{} @ {}Hz)", info.identifier,
                      info.width, info.height, info.refreshRate);
    }

    spdlog::info(
        "macOS monitor information retrieval completed, found {} monitors",
        monitors.size());
    return monitors;
}

#else
auto getAllMonitorsInfo() -> std::vector<MonitorInfo> {
    spdlog::warn(
        "Monitor information retrieval not supported on this platform");
    return {};
}
#endif

}  // namespace atom::system
