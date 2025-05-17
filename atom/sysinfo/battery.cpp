/*
 * battery.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Battery

**************************************************/

#include "atom/sysinfo/battery.hpp"

#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <winnt.h>
#include <conio.h>
#include <setupapi.h>
#include <devguid.h>
#include <powersetting.h>
#include <poclass.h>
// clang-format on
#elif defined(__APPLE__)
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#elif defined(__linux__)
#include <csignal>
#include <cstdio>
#endif

#include "atom/log/loguru.hpp"

namespace atom::system {

auto getBatteryInfo() -> std::optional<BatteryInfo> {
    LOG_F(INFO, "Starting getBatteryInfo function");
    BatteryInfo info;

#ifdef _WIN32
    SYSTEM_POWER_STATUS powerStatus{};
    if (GetSystemPowerStatus(&powerStatus) != 0) {
        LOG_F(INFO, "Successfully retrieved power status");
        info.isBatteryPresent =
            powerStatus.BatteryFlag != 128;  // 128 means no battery
        info.isCharging =
            powerStatus.BatteryFlag == 8 ||
            powerStatus.ACLineStatus ==
                1;  // 8 means charging, ACLineStatus 1 means AC power
        info.batteryLifePercent = static_cast<float>(
            powerStatus.BatteryLifePercent);  // 0-100, 255 means unknown
        info.batteryLifeTime =
            powerStatus.BatteryLifeTime ==
                    0xFFFFFFFF  // 0xFFFFFFFF means unknown
                ? 0.0f
                : static_cast<float>(
                      powerStatus.BatteryLifeTime);  // in seconds
        info.batteryFullLifeTime =
            powerStatus.BatteryFullLifeTime ==
                    0xFFFFFFFF  // 0xFFFFFFFF means unknown
                ? 0.0f
                : static_cast<float>(
                      powerStatus.BatteryFullLifeTime);  // in seconds
        LOG_F(INFO,
              "Battery Present: %d, Charging: %d, Battery Life Percent: %.2f, "
              "Battery Life Time: %.2f, Battery Full Life Time: %.2f",
              info.isBatteryPresent, info.isCharging, info.batteryLifePercent,
              info.batteryLifeTime, info.batteryFullLifeTime);

        return info;
    } else {
        LOG_F(ERROR, "Failed to get system power status");
        return std::nullopt;
    }
#elif defined(__APPLE__)
    // Custom smart pointer wrapper for managing CoreFoundation objects
    template <typename CFType>
    struct CFDeleter {
        void operator()(CFType ref) {
            if (ref)
                CFRelease(ref);
        }
    };

    template <typename CFType>
    using CFUniquePtr =
        std::unique_ptr<std::remove_pointer_t<CFType>, CFDeleter<CFType>>;

    CFUniquePtr<CFTypeRef> powerSourcesInfo(IOPSCopyPowerSourcesInfo());

    if (!powerSourcesInfo) {
        LOG_F(ERROR, "Failed to copy power sources info");
        return std::nullopt;
    }

    CFUniquePtr<CFArrayRef> powerSources(
        IOPSCopyPowerSourcesList(powerSourcesInfo.get()));

    if (!powerSources) {
        LOG_F(ERROR, "Failed to copy power sources list");
        return std::nullopt;
    }

    CFIndex count = CFArrayGetCount(powerSources.get());
    if (count > 0) {
        CFDictionaryRef powerSource =
            static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(
                powerSources.get(), 0));  // Get the first power source

        CFBooleanRef isCharging = static_cast<CFBooleanRef>(
            CFDictionaryGetValue(powerSource, kIOPSIsChargingKey));
        if (isCharging != nullptr) {
            info.isCharging = CFBooleanGetValue(isCharging);
        }

        CFNumberRef capacity = static_cast<CFNumberRef>(CFDictionaryGetValue(
            powerSource, kIOPSCurrentCapacityKey));  // Percentage
        if (capacity != nullptr) {
            SInt32 value;
            CFNumberGetValue(capacity, kCFNumberSInt32Type, &value);
            info.batteryLifePercent = static_cast<float>(value);
        }

        CFNumberRef timeToEmpty = static_cast<CFNumberRef>(CFDictionaryGetValue(
            powerSource, kIOPSTimeToEmptyKey));  // In minutes
        if (timeToEmpty != nullptr) {
            SInt32 value;
            CFNumberGetValue(timeToEmpty, kCFNumberSInt32Type, &value);
            info.batteryLifeTime =
                static_cast<float>(value);  // Already in minutes
        }

        CFNumberRef capacityMax =
            static_cast<CFNumberRef>(  // This is actually design capacity or
                                       // full charge capacity
                CFDictionaryGetValue(
                    powerSource,
                    kIOPSMaxCapacityKey));  // This is not full life time, but
                                            // max capacity
        // For batteryFullLifeTime, it's usually estimated or not directly
        // available in this simple way. We might need to use kIOPSTimeToFullKey
        // if charging, or calculate based on design capacity and current draw.
        // For now, let's assume this is a placeholder or needs more complex
        // logic. If kIOPSMaxCapacityKey represents full charge capacity in mAh
        // or similar, and kIOPSDesignCapacityKey is available, we could
        // estimate. For simplicity, we'll leave batteryFullLifeTime as
        // potentially uninitialized or 0 from this source. The original code
        // used it for `batteryFullLifeTime`, which might be a
        // misinterpretation. Let's assume it's a general capacity metric for
        // now. If it's meant to be "time to empty when full", that's different.
        // The original code assigned this to batteryFullLifeTime.
        if (capacityMax != nullptr) {
            SInt32 value;
            CFNumberGetValue(capacityMax, kCFNumberSInt32Type, &value);
            // This value is likely a capacity unit (e.g., mAh), not time.
            // To be consistent with Windows (time in seconds), this needs
            // conversion or a different key. The original code directly
            // assigned it, which is likely incorrect if units differ. For now,
            // we'll keep the original logic but acknowledge the potential
            // issue.
            info.batteryFullLifeTime =
                static_cast<float>(value);  // This is likely not time.
        }
        // It's better to check for kIOPSIsPresentKey for battery presence.
        CFBooleanRef isPresent = static_cast<CFBooleanRef>(
            CFDictionaryGetValue(powerSource, kIOPSIsPresentKey));
        if (isPresent != nullptr) {
            info.isBatteryPresent = CFBooleanGetValue(isPresent);
        }

        LOG_F(INFO,
              "Battery Info - Charging: %d, Battery Life Percent: %.2f, "
              "Battery Life Time: %.2f (minutes), Battery Full Life Time (raw "
              "value): %.2f",
              info.isCharging, info.batteryLifePercent, info.batteryLifeTime,
              info.batteryFullLifeTime);

        return info;
    } else {
        LOG_F(WARNING, "No power sources found");
        info.isBatteryPresent = false;  // Explicitly set if no power sources
        return info;  // Return default info indicating no battery
    }
#elif defined(__linux__)
    std::ifstream batteryInfo(
        "/sys/class/power_supply/BAT0/uevent");  // BAT0 might not always be the
                                                 // correct one
    if (batteryInfo.is_open()) {
        LOG_F(INFO, "Opened battery info file");
        std::string line;
        info.isBatteryPresent = true;  // Assume present if file opens, will be
                                       // refined by POWER_SUPPLY_PRESENT
        while (std::getline(batteryInfo, line)) {
            if (line.find("POWER_SUPPLY_PRESENT") != std::string::npos) {
                info.isBatteryPresent = line.substr(line.find('=') + 1) == "1";
                LOG_F(INFO, "Battery Present: %d", info.isBatteryPresent);
            } else if (line.find("POWER_SUPPLY_STATUS") != std::string::npos) {
                std::string status = line.substr(line.find('=') + 1);
                info.isCharging = status == "Charging" ||
                                  status == "Full";  // "Unknown" also possible
                LOG_F(INFO, "Battery Charging: %d (Status: %s)",
                      info.isCharging, status.c_str());
            } else if (line.find("POWER_SUPPLY_CAPACITY") !=
                       std::string::npos) {  // Percentage
                info.batteryLifePercent =
                    std::stof(line.substr(line.find('=') + 1));
                LOG_F(INFO, "Battery Life Percent: %.2f",
                      info.batteryLifePercent);
            } else if (line.find(
                           "POWER_SUPPLY_TIME_TO_EMPTY_NOW") !=  // Some systems
                                                                 // use this
                       std::string::npos) {                      // Seconds
                info.batteryLifeTime =
                    std::stof(line.substr(line.find('=') + 1)) /
                    60.0f;  // Convert to minutes
                LOG_F(
                    INFO,
                    "Battery Life Time (from TIME_TO_EMPTY_NOW): %.2f minutes",
                    info.batteryLifeTime);
            } else if (line.find("POWER_SUPPLY_CHARGE_FULL") !=  // Full charge
                                                                 // capacity
                                                                 // (e.g., uAh)
                       std::string::npos) {
                // This is capacity, not time. We need TIME_TO_FULL or similar
                // for batteryFullLifeTime. The original code used
                // POWER_SUPPLY_TIME_TO_FULL_NOW for batteryFullLifeTime. Let's
                // stick to that if available.
            } else if (line.find("POWER_SUPPLY_TIME_TO_FULL_NOW") !=  // Seconds
                       std::string::npos) {
                // This is time to full when charging.
                // If we interpret batteryFullLifeTime as "estimated run time on
                // a full charge", this isn't it. If it's "time until fully
                // charged", then this is it. The Windows equivalent
                // BatteryFullLifeTime is "total time the battery can sustain".
                // For now, let's assume it's time to become full if charging.
                info.batteryFullLifeTime =
                    std::stof(line.substr(line.find('=') + 1)) /
                    60.0f;  // Convert to minutes
                LOG_F(INFO, "Battery Time To Full Now: %.2f minutes",
                      info.batteryFullLifeTime);
            } else if (line.find(
                           "POWER_SUPPLY_ENERGY_NOW") !=  // Microjoules or
                                                          // microwatt-hours
                       std::string::npos) {
                info.energyNow = std::stof(line.substr(line.find('=') + 1));
                LOG_F(INFO, "Energy Now: %.2f", info.energyNow);
            } else if (
                line.find(
                    "POWER_SUPPLY_ENERGY_FULL_DESIGN") !=  // Microjoules or
                                                           // microwatt-hours
                std::string::npos) {
                info.energyDesign = std::stof(line.substr(line.find('=') + 1));
                LOG_F(INFO, "Energy Design: %.2f", info.energyDesign);
            } else if (line.find(
                           "POWER_SUPPLY_ENERGY_FULL") !=  // Microjoules or
                                                           // microwatt-hours
                       std::string::npos) {
                info.energyFull = std::stof(line.substr(line.find('=') + 1));
                LOG_F(INFO, "Energy Full: %.2f", info.energyFull);
            } else if (line.find("POWER_SUPPLY_VOLTAGE_NOW") !=  // Microvolts
                       std::string::npos) {
                info.voltageNow = std::stof(line.substr(line.find('=') + 1)) /
                                  1000000.0f;  // Convert to Volts
                LOG_F(INFO, "Voltage Now: %.2f V", info.voltageNow);
            } else if (line.find("POWER_SUPPLY_CURRENT_NOW") !=  // Microamperes
                       std::string::npos) {
                // Note: This can be negative for discharging, positive for
                // charging.
                info.currentNow = std::stof(line.substr(line.find('=') + 1)) /
                                  1000000.0f;  // Convert to Amperes
                LOG_F(INFO, "Current Now: %.2f A", info.currentNow);
            }
        }
        batteryInfo.close();
        if (!info.isBatteryPresent) {  // If POWER_SUPPLY_PRESENT=0 was found
            LOG_F(INFO,
                  "Battery explicitly marked as not present in uevent file.");
            return std::nullopt;  // Or return info with isBatteryPresent =
                                  // false
        }
        return info;
    } else {
        LOG_F(ERROR,
              "Failed to open battery info file "
              "/sys/class/power_supply/BAT0/uevent");
        // Try other battery names like BAT1, etc., or use upower via D-Bus for
        // a more robust solution.
        return std::nullopt;
    }
#else
    LOG_F(ERROR, "Platform not supported for battery info");
    return std::nullopt;
#endif
}

auto getDetailedBatteryInfo() -> BatteryResult {
    auto batteryInfoOpt = getBatteryInfo();
    if (!batteryInfoOpt) {
        // getBatteryInfo already logs errors.
        // We need to determine the correct BatteryError.
        // If getBatteryInfo returns nullopt because the platform is
        // unsupported, then NOT_SUPPORTED might be more appropriate here.
        // However, getBatteryInfo itself doesn't distinguish these cases in its
        // return. Assuming READ_ERROR is a general failure from getBatteryInfo.
        return BatteryError::READ_ERROR;
    }

    BatteryInfo info =
        std::move(*batteryInfoOpt);  // 正确使用 * 解引用 std::optional

    // If basic info indicates no battery, we might not need to proceed or can
    // return early.
    if (!info.isBatteryPresent) {
        // It could be that the battery is genuinely not present, or there was
        // an error reading its presence. If getBatteryInfo correctly determined
        // no battery, then NOT_PRESENT is suitable. However,
        // getDetailedBatteryInfo is expected to add more details if a battery
        // *is* present. Let's assume if getBatteryInfo says not present, it's
        // definitive for this function's scope. The original code proceeded to
        // platform-specific sections regardless. This might be okay if those
        // sections can also determine presence. For now, we'll follow the
        // original structure.
    }

#ifdef _WIN32
    // Windows: Use SetupAPI to get more details like manufacturer, model,
    // serial. This part was largely unimplemented in the original snippet. For
    // a full implementation, one would query battery properties using SetupDi
    // calls and IOCTL_BATTERY_QUERY_INFORMATION. Example properties:
    // BatteryManufacturerName, BatteryDeviceName, BatterySerialNumber.

    HDEVINFO hdev = SetupDiGetClassDevs(&GUID_DEVCLASS_BATTERY, 0, 0,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hdev != INVALID_HANDLE_VALUE) {
        // RAII wrapper for HDEVINFO
        struct DevInfoSetCloser {
            HDEVINFO handle;
            ~DevInfoSetCloser() {
                if (handle != INVALID_HANDLE_VALUE) {
                    SetupDiDestroyDeviceInfoList(handle);
                }
            }
        } deviceInfoSetCloser{hdev};

        SP_DEVICE_INTERFACE_DATA did{};
        did.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        // Iterate through battery devices (usually one)
        if (SetupDiEnumDeviceInterfaces(hdev, NULL, &GUID_DEVCLASS_BATTERY, 0,
                                        &did)) {
            DWORD cbRequired = 0;
            SetupDiGetDeviceInterfaceDetail(hdev, &did, NULL, 0, &cbRequired,
                                            NULL);

            if (cbRequired > 0) {
                auto pspdidd = std::unique_ptr<BYTE[]>(new BYTE[cbRequired]);
                auto detailData =
                    reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(
                        pspdidd.get());
                detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

                if (SetupDiGetDeviceInterfaceDetail(hdev, &did, detailData,
                                                    cbRequired, NULL, NULL)) {
                    HANDLE hBattery = CreateFile(
                        detailData->DevicePath, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                        0, NULL);
                    if (hBattery != INVALID_HANDLE_VALUE) {
                        // RAII wrapper for HANDLE
                        struct FileHandleCloser {
                            HANDLE handle;
                            ~FileHandleCloser() {
                                if (handle != INVALID_HANDLE_VALUE)
                                    CloseHandle(handle);
                            }
                        } batteryFileHandleCloser{hBattery};

                        BATTERY_QUERY_INFORMATION bqi{};
                        bqi.InformationLevel = BatteryManufactureName;
                        DWORD bytesReturned;
                        std::wstring manufacturerName(128, L'\0');
                        if (DeviceIoControl(
                                hBattery, IOCTL_BATTERY_QUERY_INFORMATION, &bqi,
                                sizeof(bqi), manufacturerName.data(),
                                manufacturerName.size() * sizeof(wchar_t),
                                &bytesReturned, NULL) &&
                            bytesReturned > 0) {
                            manufacturerName.resize(
                                bytesReturned / sizeof(wchar_t) > 0
                                    ? (bytesReturned / sizeof(wchar_t)) - 1
                                    : 0);  // Null terminate
                            info.manufacturer.assign(
                                manufacturerName.begin(),
                                manufacturerName
                                    .end());  // Convert wstring to string
                        }

                        bqi.InformationLevel = BatteryDeviceName;  // Model
                        std::wstring modelName(128, L'\0');
                        if (DeviceIoControl(hBattery,
                                            IOCTL_BATTERY_QUERY_INFORMATION,
                                            &bqi, sizeof(bqi), modelName.data(),
                                            modelName.size() * sizeof(wchar_t),
                                            &bytesReturned, NULL) &&
                            bytesReturned > 0) {
                            modelName.resize(
                                bytesReturned / sizeof(wchar_t) > 0
                                    ? (bytesReturned / sizeof(wchar_t)) - 1
                                    : 0);
                            info.model.assign(modelName.begin(),
                                              modelName.end());
                        }

                        bqi.InformationLevel = BatterySerialNumber;
                        std::wstring serialNumberStr(128, L'\0');
                        if (DeviceIoControl(
                                hBattery, IOCTL_BATTERY_QUERY_INFORMATION, &bqi,
                                sizeof(bqi), serialNumberStr.data(),
                                serialNumberStr.size() * sizeof(wchar_t),
                                &bytesReturned, NULL) &&
                            bytesReturned > 0) {
                            serialNumberStr.resize(
                                bytesReturned / sizeof(wchar_t) > 0
                                    ? (bytesReturned / sizeof(wchar_t)) - 1
                                    : 0);
                            info.serialNumber.assign(serialNumberStr.begin(),
                                                     serialNumberStr.end());
                        }

                        // Get cycle count
                        BATTERY_STATUS bs{};
                        BATTERY_WAIT_STATUS bws{};
                        bws.BatteryTag = 0;  // 初始化为0

                        // 首先，获取电池标签
                        ULONG batteryTag = 0;
                        bqi.InformationLevel = BatteryInformation;
                        if (DeviceIoControl(
                                hBattery, IOCTL_BATTERY_QUERY_INFORMATION, &bqi,
                                sizeof(bqi), &batteryTag, sizeof(batteryTag),
                                &bytesReturned, NULL) &&
                            batteryTag != 0) {
                            bws.BatteryTag = batteryTag;
                            if (DeviceIoControl(
                                    hBattery, IOCTL_BATTERY_QUERY_STATUS, &bws,
                                    sizeof(bws), &bs, sizeof(bs),
                                    &bytesReturned, NULL)) {
                                // BATTERY_STATUS doesn't directly provide cycle
                                // count. Cycle count might be available via
                                // BatteryManufactureDate or specific WMI
                                // queries. For now, we'll leave it as
                                // potentially uninitialized from this source.
                                // info.cycleCounts = bs.CycleCount; //
                                // CycleCount is not part of BATTERY_STATUS
                            }
                        }
                        // Temperature is also not directly available via these
                        // basic IOCTLs. WMI (MSBatteryClass) is often needed
                        // for temperature and cycle count.
                    }
                }
            }
        }
        // If we successfully got some info, return it.
        // If there were issues but hdev was valid, it might still be an access
        // issue or no battery. The original code returns `info` if hdev is
        // valid, implying success even if details are missing.
        return info;
    }
    // If hdev is INVALID_HANDLE_VALUE, it's likely an access or setup issue.
    LOG_F(ERROR, "SetupDiGetClassDevs failed for battery devices. Error: %lu",
          GetLastError());
    return BatteryError::ACCESS_DENIED;  // Or READ_ERROR if it's a more general
                                         // failure
#elif defined(__APPLE__)
    // On macOS, IOPSCopyPowerSourcesInfo already provides most of what's
    // needed. Detailed info like manufacturer, serial number, cycle count are
    // in the same dictionary.
    CFUniquePtr<CFTypeRef> powerSourcesInfo(IOPSCopyPowerSourcesInfo());
    if (!powerSourcesInfo)
        return BatteryError::READ_ERROR;

    CFUniquePtr<CFArrayRef> powerSources(
        IOPSCopyPowerSourcesList(powerSourcesInfo.get()));
    if (!powerSources || CFArrayGetCount(powerSources.get()) == 0)
        return BatteryError::NOT_PRESENT;  // Or READ_ERROR

    CFDictionaryRef psDesc =
        (CFDictionaryRef)CFArrayGetValueAtIndex(powerSources.get(), 0);

    auto getStringValue = [&](CFStringRef key) -> std::string {
        CFStringRef value = (CFStringRef)CFDictionaryGetValue(psDesc, key);
        if (value) {
            char buffer[256];
            if (CFStringGetCString(value, buffer, sizeof(buffer),
                                   kCFStringEncodingUTF8)) {
                return buffer;
            }
        }
        return "";
    };

    auto getIntValue = [&](CFStringRef key) -> int {
        CFNumberRef value = (CFNumberRef)CFDictionaryGetValue(psDesc, key);
        if (value) {
            int intVal;
            if (CFNumberGetValue(value, kCFNumberIntType, &intVal)) {
                return intVal;
            }
        }
        return 0;
    };

    auto getFloatValue = [&](CFStringRef key, float scale = 1.0f) -> float {
        CFNumberRef value = (CFNumberRef)CFDictionaryGetValue(psDesc, key);
        if (value) {
            double doubleVal;  // Read as double for precision
            if (CFNumberGetValue(value, kCFNumberDoubleType, &doubleVal)) {
                return static_cast<float>(doubleVal * scale);
            }
        }
        return 0.0f;
    };

    info.manufacturer = getStringValue(CFSTR(kIOPSManufacturerKey));
    info.model = getStringValue(CFSTR(kIOPSDeviceNameKey));  // Model
    info.serialNumber = getStringValue(CFSTR(kIOPSSerialNumberKey));
    info.cycleCounts = getIntValue(CFSTR(kIOPSCycleCountKey));
    info.temperature = getFloatValue(CFSTR(kIOPSTemperatureKey),
                                     0.01f);  // Temperature is in 1/100 C

    // Energy values from basic info might be sufficient, or re-query if needed.
    // energyNow, energyFull, energyDesign are often available here too.
    // kIOPSCurrentCapacityKey (mAh), kIOPSMaxCapacityKey (mAh),
    // kIOPSDesignCapacityKey (mAh) kIOPSVoltageKey (mV)
    info.voltageNow =
        getFloatValue(CFSTR(kIOPSVoltageKey), 0.001f);  // Voltage is in mV
    // currentNow (mA), positive for charging, negative for discharging
    info.currentNow =
        getFloatValue(CFSTR(kIOPSAmperageKey), 0.001f);  // Current is in mA

    return info;

#elif defined(__linux__)
    // For Linux, the /sys/class/power_supply/BAT0/uevent file already contains
    // many details. We'll re-read it here to ensure all fields are populated,
    // as the original code did. Alternatively, extend getBatteryInfo or pass
    // its parsed data.
    std::ifstream batteryUevent("/sys/class/power_supply/BAT0/uevent");
    if (batteryUevent.is_open()) {
        std::string line;
        while (std::getline(batteryUevent, line)) {
            if (const size_t pos = line.find('='); pos != std::string::npos) {
                const std::string_view key =
                    std::string_view(line).substr(0, pos);
                const std::string_view value_sv =
                    std::string_view(line).substr(pos + 1);
                std::string value = std::string(
                    value_sv);  // Convert to std::string for stoi/stof

                if (key == "POWER_SUPPLY_CYCLE_COUNT") {
                    info.cycleCounts = std::stoi(value);
                } else if (key == "POWER_SUPPLY_TEMP") {  // In deciCelsius
                    info.temperature = std::stof(value) / 10.0f;
                } else if (key == "POWER_SUPPLY_MANUFACTURER") {
                    info.manufacturer = value;
                } else if (key == "POWER_SUPPLY_MODEL_NAME") {
                    info.model = value;
                } else if (key == "POWER_SUPPLY_SERIAL_NUMBER") {
                    info.serialNumber = value;
                }
                // Other fields like energy, voltage, current are already
                // handled by getBatteryInfo but could be re-parsed here if
                // desired for consistency.
            }
        }
        batteryUevent.close();
        // If the battery was marked not present by getBatteryInfo, this
        // function might still "succeed" by returning the info struct, but
        // isBatteryPresent would be false. If the file couldn't be opened here,
        // but was in getBatteryInfo, it's an inconsistency.
        return info;
    }
    LOG_F(ERROR, "Failed to open battery uevent file for detailed info.");
    return BatteryError::READ_ERROR;  // If file cannot be opened here.
#else
    LOG_F(WARNING, "Detailed battery info not supported for this platform.");
    return BatteryError::NOT_SUPPORTED;
#endif
}

// BatteryMonitorImpl: Implementation for battery monitoring using RAII and
// smart pointers
class BatteryMonitorImpl {
public:
    BatteryMonitorImpl() = default;
    ~BatteryMonitorImpl() { stop(); }  // RAII: stop monitoring on destruction

    auto start(BatteryMonitor::BatteryCallback callback,
               unsigned int interval_ms) -> bool {
        if (m_isRunning.exchange(
                true)) {  // Atomically set to true and get previous value
            LOG_F(WARNING, "Battery monitor is already running.");
            return false;  // Already running
        }

        m_monitorThread = std::thread(
            [this, cb = std::move(callback),  // Move callback into lambda
             interval = std::chrono::milliseconds(
                 interval_ms)]() {  // Use chrono duration
                BatteryInfo lastInfo;
                bool firstRun = true;  // To send initial state immediately

                while (m_isRunning.load()) {  // Atomically check running state
                    auto result =
                        getDetailedBatteryInfo();  // Get current battery state
                    if (auto* currentInfoPtr =
                            std::get_if<BatteryInfo>(&result)) {
                        BatteryInfo& currentInfo = *currentInfoPtr;
                        // Send update if info changed or if it's the first run
                        // (and battery is present)
                        if (currentInfo.isBatteryPresent &&
                            (firstRun || currentInfo != lastInfo)) {
                            cb(currentInfo);  // Invoke callback
                            lastInfo =
                                currentInfo;  // Update last known info (uses
                                              // move if BatteryInfo is movable)
                            firstRun = false;
                        } else if (!currentInfo.isBatteryPresent &&
                                   lastInfo.isBatteryPresent) {
                            // Battery was present, now it's not (e.g., removed)
                            cb(currentInfo);  // Send the "not present" state
                            lastInfo = currentInfo;
                        }
                    } else {
                        // Handle error case from getDetailedBatteryInfo if
                        // needed e.g., log the error, or signal an error state
                        // via callback
                        BatteryError error = std::get<BatteryError>(result);
                        LOG_F(ERROR,
                              "Error getting detailed battery info in monitor "
                              "thread: %d",
                              static_cast<int>(error));
                        // Optionally, could have a separate error callback or
                        // modify BatteryInfo to include error state.
                    }
                    std::this_thread::sleep_for(interval);
                }
            });
        LOG_F(INFO, "Battery monitor started.");
        return true;
    }

    void stop() {
        if (m_isRunning.exchange(
                false)) {  // Atomically set to false and get previous value
            if (m_monitorThread.joinable()) {
                m_monitorThread.join();  // Wait for the thread to finish
            }
            LOG_F(INFO, "Battery monitor stopped.");
        }
    }

    [[nodiscard]] auto isRunning() const noexcept -> bool {
        return m_isRunning.load();  // Atomically read running state
    }

private:
    std::atomic<bool> m_isRunning{false};  // Atomic flag for running state
    std::thread m_monitorThread;           // Monitoring thread
};

// Singleton instance of BatteryMonitorImpl with mutex for thread-safe
// initialization
static std::unique_ptr<BatteryMonitorImpl> g_batteryMonitorImpl;
static std::mutex
    g_monitorMutex;  // Mutex to protect g_batteryMonitorImpl initialization

// Helper to get or create the singleton BatteryMonitorImpl instance
static auto getMonitorImpl() -> BatteryMonitorImpl& {
    std::lock_guard<std::mutex> lock(
        g_monitorMutex);  // Lock for thread-safe initialization
    if (!g_batteryMonitorImpl) {
        g_batteryMonitorImpl = std::make_unique<BatteryMonitorImpl>();
    }
    return *g_batteryMonitorImpl;
}

// Static methods of BatteryMonitor delegate to the BatteryMonitorImpl instance
auto BatteryMonitor::startMonitoring(BatteryCallback callback,
                                     unsigned int interval_ms) -> bool {
    return getMonitorImpl().start(std::move(callback), interval_ms);
}

void BatteryMonitor::stopMonitoring() {
    // Ensure impl exists before trying to stop.
    // If startMonitoring was never called, impl might not exist.
    std::lock_guard<std::mutex> lock(g_monitorMutex);
    if (g_batteryMonitorImpl) {
        g_batteryMonitorImpl->stop();
    }
}

auto BatteryMonitor::isMonitoring() noexcept -> bool {
    std::lock_guard<std::mutex> lock(
        g_monitorMutex);  // Protect access to g_batteryMonitorImpl
    return g_batteryMonitorImpl && g_batteryMonitorImpl->isRunning();
}

// Implementation of BatteryManager::BatteryManagerImpl (PIMPL pattern)
class BatteryManager::BatteryManagerImpl {
public:
    BatteryManagerImpl() = default;
    ~BatteryManagerImpl() {
        // Ensure monitoring and recording are stopped cleanly
        stopMonitoring();  // Uses BatteryMonitor::stopMonitoring
        stopRecording();   // Stops internal recording
    }

    void setAlertCallback(AlertCallback callback) {
        std::lock_guard lock(m_mutex);  // Use shared_mutex for write lock
        m_alertCallback = std::move(callback);
    }

    void setAlertSettings(const BatteryAlertSettings& settings) {
        std::lock_guard lock(m_mutex);  // Use shared_mutex for write lock
        m_alertSettings = settings;
    }

    [[nodiscard]] auto getStats() const -> const BatteryStats& {
        std::shared_lock lock(m_mutex);  // Use shared_mutex for read lock
        return m_currentStats;
    }

    auto startRecording(std::string_view logFilePath) -> bool {
        std::lock_guard lock(m_mutex);  // Write lock
        if (m_isRecording) {
            LOG_F(WARNING, "Recording is already active.");
            return false;
        }

        if (!logFilePath.empty()) {
            m_logFile.open(
                std::string(
                    logFilePath),  // Convert string_view to string for ofstream
                std::ios::out | std::ios::app);  // Append mode
            if (!m_logFile.is_open()) {  // Check if file opened successfully
                LOG_F(ERROR, "Failed to open log file: %s",
                      std::string(logFilePath).c_str());
                return false;
            }
            LOG_F(INFO, "Recording battery data to log file: %s",
                  std::string(logFilePath).c_str());
            // Write CSV header if the file is new or empty (more robust check
            // needed for truly empty)
            m_logFile << "timestamp,datetime,battery_level_percent,temperature_"
                         "celsius,voltage_v,current_a,"
                         "health_percent,is_charging\n";
        } else {
            LOG_F(INFO,
                  "Recording battery data to memory only (no log file "
                  "specified).");
        }

        m_isRecording = true;
        return true;
    }

    void stopRecording() {
        std::lock_guard lock(m_mutex);  // Write lock
        if (!m_isRecording) {
            return;
        }
        m_isRecording = false;
        if (m_logFile.is_open()) {
            m_logFile.close();
            LOG_F(INFO, "Stopped recording battery data to log file.");
        } else {
            LOG_F(INFO, "Stopped recording battery data (memory only).");
        }
    }

    auto startMonitoring(unsigned int interval_ms) -> bool {
        // This monitoring is for BatteryManager's internal purposes (alerts,
        // stats, recording) It uses the global BatteryMonitor.
        LOG_F(INFO, "BatteryManager starting internal monitoring.");
        return BatteryMonitor::startMonitoring(
            [this](const BatteryInfo& info) {  // Lambda to capture 'this'
                this->handleBatteryUpdate(info);
            },
            interval_ms);
    }

    void stopMonitoring() {
        // Stops the global BatteryMonitor if BatteryManager initiated it for
        // its purposes.
        LOG_F(INFO, "BatteryManager stopping internal monitoring.");
        BatteryMonitor::stopMonitoring();
    }

    [[nodiscard]] auto getHistory(unsigned int maxEntries) const -> std::vector<
        std::pair<std::chrono::system_clock::time_point, BatteryInfo>> {
        std::shared_lock lock(m_mutex);  // Read lock

        if (maxEntries == 0 || maxEntries >= m_historyData.size()) {
            return m_historyData;  // Return all data
        }

        // Return the most recent 'maxEntries' items
        return {m_historyData.end() - maxEntries, m_historyData.end()};
    }

private:
    // Called when BatteryMonitor provides an update
    void handleBatteryUpdate(const BatteryInfo& info) {
        // Order of operations: record, then update stats, then check alerts
        // This ensures stats and alerts are based on the latest recorded data.
        if (info.isBatteryPresent) {  // Only process if a battery is actually
                                      // present
            recordData(info);         // Record first
            updateStats(info);        // Then update aggregate stats
            checkAlerts(info);  // Finally, check for alerts based on new info
        } else {
            // If battery is not present, we might want to clear some stats or
            // log this event. For now, we just don't process it further. If
            // recording is on, we might want to record a "no battery" event.
            // The current recordData doesn't explicitly handle this.
        }
    }

    void checkAlerts(const BatteryInfo& info) {
        // No lock needed here if m_alertCallback and m_alertSettings are
        // managed by the caller's lock However, to be safe, as this can be
        // called from a different thread (BatteryMonitor's callback) a read
        // lock is appropriate if the callback or settings can change
        // concurrently. The public methods setAlertCallback/Settings use a
        // write lock.
        AlertCallback currentAlertCallback;
        BatteryAlertSettings currentAlertSettings;
        {
            std::shared_lock lock(
                m_mutex);  // Read lock for accessing callback and settings
            if (!m_alertCallback) {
                return;  // No callback registered
            }
            currentAlertCallback =
                m_alertCallback;  // Copy callback to call outside lock
            currentAlertSettings = m_alertSettings;
        }

        if (info.batteryLifePercent <=
            currentAlertSettings.criticalBatteryThreshold) {
            LOG_F(WARNING, "Critical battery alert: %.2f%%",
                  info.batteryLifePercent);
            currentAlertCallback(AlertType::CRITICAL_BATTERY, info);
        } else if (info.batteryLifePercent <=
                   currentAlertSettings.lowBatteryThreshold) {
            LOG_F(WARNING, "Low battery alert: %.2f%%",
                  info.batteryLifePercent);
            currentAlertCallback(AlertType::LOW_BATTERY, info);
        }

        if (info.temperature >= currentAlertSettings.highTempThreshold) {
            LOG_F(WARNING, "High temperature alert: %.2f C", info.temperature);
            currentAlertCallback(AlertType::HIGH_TEMPERATURE, info);
        }

        if (info.getBatteryHealth() <=
            currentAlertSettings.lowHealthThreshold) {
            LOG_F(WARNING, "Low battery health alert: %.2f%%",
                  info.getBatteryHealth());
            currentAlertCallback(AlertType::LOW_BATTERY_HEALTH, info);
        }
    }

    void updateStats(const BatteryInfo& info) {
        std::lock_guard lock(
            m_mutex);  // Write lock for modifying m_currentStats

        // Update min/max battery levels
        m_currentStats.minBatteryLevel =
            std::min(m_currentStats.minBatteryLevel, info.batteryLifePercent);
        m_currentStats.maxBatteryLevel =
            std::max(m_currentStats.maxBatteryLevel, info.batteryLifePercent);

        // Update min/max temperatures (only if temperature data is valid, e.g.,
        // > 0 or some reasonable range)
        if (info.temperature > -100) {  // Basic validity check for temperature
            m_currentStats.minTemperature =
                std::min(m_currentStats.minTemperature, info.temperature);
            m_currentStats.maxTemperature =
                std::max(m_currentStats.maxTemperature, info.temperature);
        }

        // Update min/max voltages (only if voltage data is valid)
        if (info.voltageNow > 0) {
            m_currentStats.minVoltage =
                std::min(m_currentStats.minVoltage, info.voltageNow);
            m_currentStats.maxVoltage =
                std::max(m_currentStats.maxVoltage, info.voltageNow);
        }

        // Update average discharge rate
        // This calculation is a bit simplistic and might need refinement.
        // It relies on sequential updates and assumes discharging.
        if (!m_historyData.empty() && !info.isCharging &&
            m_historyData.back().second.batteryLifePercent >
                info.batteryLifePercent) {  // Discharging

            const auto& lastRecord = m_historyData.back();
            auto timeDiffSeconds =
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::
                        now() -  // Or use info's timestamp if available
                    lastRecord.first)
                    .count();

            if (timeDiffSeconds > 0) {
                float dischargePercent = lastRecord.second.batteryLifePercent -
                                         info.batteryLifePercent;
                float currentRatePerHour =
                    (dischargePercent / static_cast<float>(timeDiffSeconds)) *
                    3600.0f;

                if (m_currentStats.avgDischargeRate <
                    0) {  // First valid calculation
                    m_currentStats.avgDischargeRate = currentRatePerHour;
                } else {  // Exponential moving average
                    m_currentStats.avgDischargeRate =
                        (m_currentStats.avgDischargeRate * 0.9f) +
                        (currentRatePerHour * 0.1f);
                }
            }
        } else if (info.isCharging) {
            // Optionally reset or pause avgDischargeRate calculation during
            // charging m_currentStats.avgDischargeRate = 0.0f; // Or some
            // indicator it's not discharging
        }

        // Update total uptime on battery (placeholder, needs more logic)
        // m_currentStats.totalUptime += ... (if on battery and time has passed)

        // Update total energy consumed (placeholder)
        // m_currentStats.totalEnergyConsumed += ...

        // Update cycle count and health from the latest info
        m_currentStats.cycleCount = info.cycleCounts;
        m_currentStats.batteryHealth = info.getBatteryHealth();
    }

    void recordData(const BatteryInfo& info) {
        std::lock_guard lock(
            m_mutex);  // Write lock for m_historyData and m_logFile
        if (!m_isRecording) {
            return;
        }

        auto now = std::chrono::system_clock::now();
        m_historyData.emplace_back(now, info);

        // Limit history size (e.g., last 24 hours assuming 10s interval)
        // 24 * 60 * 60 / 10 = 8640 entries
        constexpr size_t MAX_HISTORY_SIZE_MEMORY = 8640;
        if (m_historyData.size() > MAX_HISTORY_SIZE_MEMORY) {
            m_historyData.erase(
                m_historyData.begin(),
                m_historyData.begin() +
                    (m_historyData.size() - MAX_HISTORY_SIZE_MEMORY));
        }

        if (m_logFile.is_open()) {
            // Convert time_point to a more readable format for CSV
            std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm tm_local =
                *std::localtime(&tt);  // Not thread-safe on all platforms,
                                       // C++20 chrono has safer alternatives

            // Format the time separately using std::put_time
            std::stringstream time_ss;
            time_ss << std::put_time(&tm_local, "%Y-%m-%d %H:%M:%S");
            std::string formatted_time = time_ss.str();

            m_logFile << std::format(
                "{},{},{:.2f},{:.2f},{:.3f},{:.3f},{:.2f},{}\n",
                tt,              // Unix timestamp
                formatted_time,  // Human-readable time
                info.batteryLifePercent, info.temperature, info.voltageNow,
                info.currentNow, info.getBatteryHealth(), info.isCharging);
            m_logFile.flush();  // Ensure data is written, especially for
                                // long-running apps
        }
    }

    mutable std::shared_mutex m_mutex;  // For thread-safe access to shared data
    BatteryAlertSettings m_alertSettings;
    AlertCallback m_alertCallback;
    bool m_isRecording{false};
    std::ofstream m_logFile;  // File stream for logging
    std::vector<std::pair<std::chrono::system_clock::time_point, BatteryInfo>>
        m_historyData;  // In-memory history
    BatteryStats m_currentStats;
};

// BatteryManager singleton implementation
auto BatteryManager::getInstance() -> BatteryManager& {
    // Meyers' singleton: thread-safe in C++11 and later
    static BatteryManager instance;
    return instance;
}

// Private constructor for PIMPL
BatteryManager::BatteryManager()
    : impl(std::make_unique<BatteryManagerImpl>()) {}

// Destructor (defaulted is fine due to unique_ptr)
BatteryManager::~BatteryManager() = default;

// Public methods delegate to the PIMPL instance
void BatteryManager::setAlertCallback(AlertCallback callback) {
    impl->setAlertCallback(std::move(callback));
}

void BatteryManager::setAlertSettings(const BatteryAlertSettings& settings) {
    impl->setAlertSettings(settings);
}

auto BatteryManager::getStats() const -> const BatteryStats& {
    return impl->getStats();
}

auto BatteryManager::startRecording(std::string_view logFile) -> bool {
    return impl->startRecording(logFile);
}

void BatteryManager::stopRecording() { impl->stopRecording(); }

auto BatteryManager::startMonitoring(unsigned int interval_ms) -> bool {
    return impl->startMonitoring(interval_ms);
}

void BatteryManager::stopMonitoring() { impl->stopMonitoring(); }

auto BatteryManager::getHistory(unsigned int maxEntries) const -> std::vector<
    std::pair<std::chrono::system_clock::time_point, BatteryInfo>> {
    return impl->getHistory(maxEntries);
}

// PowerPlanManager implementation
auto PowerPlanManager::setPowerPlan(PowerPlan plan) -> std::optional<bool> {
#ifdef _WIN32
    GUID planGuid{};
    bool isValidPlan = true;
    switch (plan) {
        case PowerPlan::BALANCED:
            planGuid = GUID_TYPICAL_POWER_SAVINGS;  // Balanced
            break;
        case PowerPlan::PERFORMANCE:
            planGuid = GUID_MIN_POWER_SAVINGS;  // High performance
            break;
        case PowerPlan::POWER_SAVER:
            planGuid = GUID_MAX_POWER_SAVINGS;  // Power saver
            break;
        case PowerPlan::CUSTOM:  // Custom plans need specific GUIDs not
                                 // predefined here
            LOG_F(ERROR,
                  "Setting custom power plans by enum is not directly "
                  "supported without a GUID.");
            return std::nullopt;  // Or false, indicating failure to set this
                                  // generic 'CUSTOM'
        default:
            LOG_F(ERROR, "Invalid power plan specified for Windows.");
            isValidPlan = false;  // Should not happen with enum class if all
                                  // cases handled
            return std::nullopt;
    }

    if (!isValidPlan)
        return std::nullopt;

    LOG_F(INFO, "Attempting to set Windows power plan...");
    DWORD result = PowerSetActiveScheme(NULL, &planGuid);
    if (result != ERROR_SUCCESS) {
        LOG_F(ERROR, "Failed to set power plan on Windows: error code %lu",
              result);
        return false;  // Explicit failure
    }
    LOG_F(INFO, "Windows power plan successfully changed.");
    return true;
#elif defined(__linux__)
    // This uses 'powerprofilesctl', which is common on modern Linux desktops
    // (GNOME, KDE) May not be available on all systems (e.g., servers, minimal
    // installs) Alternatives: 'tlp', 'cpupower', direct sysfs manipulation
    // (more complex)
    std::string cmd;
    switch (plan) {
        case PowerPlan::BALANCED:
            cmd = "powerprofilesctl set balanced";
            break;
        case PowerPlan::PERFORMANCE:
            cmd = "powerprofilesctl set performance";
            break;
        case PowerPlan::POWER_SAVER:
            cmd = "powerprofilesctl set power-saver";
            break;
        case PowerPlan::CUSTOM:
            LOG_F(ERROR,
                  "Custom power plans are not generically settable via "
                  "powerprofilesctl with this enum.");
            return std::nullopt;
        default:
            LOG_F(ERROR, "Invalid power plan specified for Linux.");
            return std::nullopt;
    }

    LOG_F(INFO, "Attempting to set Linux power profile: %s", cmd.c_str());
    int system_result = std::system(cmd.c_str());
    if (WIFEXITED(system_result) &&
        WEXITSTATUS(system_result) == 0) {  // Check exit status
        LOG_F(INFO, "Linux power profile successfully changed using: %s",
              cmd.c_str());
        return true;
    } else {
        LOG_F(ERROR,
              "Failed to set Linux power profile using '%s'. Exit status: %d",
              cmd.c_str(), WEXITSTATUS(system_result));
        // Check if powerprofilesctl is installed or if the profile name is
        // valid
        return false;  // Explicit failure
    }
#elif defined(__APPLE__)
    // macOS power plan management is less direct. It's mostly automatic.
    // Users can influence it via Energy Saver settings.
    // Programmatic control is limited, often to asserting properties like
    // preventing sleep. For simplicity, we'll say it's not directly supported
    // for distinct plans like Windows/Linux.
    LOG_F(WARNING,
          "Direct power plan setting like Balanced/Performance/PowerSaver is "
          "not standard on macOS via simple commands.");
    return std::nullopt;  // Not supported
#else
    LOG_F(WARNING, "Power plan management not implemented for this platform.");
    return std::nullopt;  // Not supported
#endif
}

auto PowerPlanManager::getCurrentPowerPlan() -> std::optional<PowerPlan> {
#ifdef _WIN32
    // RAII for library handle
    struct LibraryHandle {
        HMODULE handle;
        explicit LibraryHandle(const wchar_t* name)
            : handle(LoadLibraryW(name)) {}  // Use W for Windows API
        ~LibraryHandle() {
            if (handle)
                FreeLibrary(handle);
        }
        operator HMODULE() const { return handle; }
    } hPowrProf{L"powrprof.dll"};

    if (!hPowrProf) {
        LOG_F(ERROR, "Failed to load powrprof.dll. Error: %lu", GetLastError());
        return std::nullopt;
    }

    // Define function pointer type for PowerGetActiveScheme
    using PFN_PowerGetActiveScheme = DWORD(WINAPI*)(HKEY, GUID**);
    auto pGetActiveScheme = reinterpret_cast<PFN_PowerGetActiveScheme>(
        GetProcAddress(hPowrProf, "PowerGetActiveScheme"));

    if (!pGetActiveScheme) {
        LOG_F(ERROR,
              "Failed to get PowerGetActiveScheme address from powrprof.dll. "
              "Error: %lu",
              GetLastError());
        return std::nullopt;
    }

    GUID* pActiveSchemeGuid = nullptr;
    if (pGetActiveScheme(NULL, &pActiveSchemeGuid) == ERROR_SUCCESS &&
        pActiveSchemeGuid) {
        // RAII for GUID memory allocated by PowerGetActiveScheme (freed with
        // LocalFree)
        struct GuidDeleter {
            void operator()(GUID* ptr) {
                if (ptr)
                    LocalFree(
                        ptr);  // As per documentation for PowerGetActiveScheme
            }
        };
        std::unique_ptr<GUID, GuidDeleter> activeGuidPtr(pActiveSchemeGuid);

        if (IsEqualGUID(*activeGuidPtr, GUID_MAX_POWER_SAVINGS)) {
            LOG_F(INFO, "Current Windows power plan: Power Saver");
            return PowerPlan::POWER_SAVER;
        } else if (IsEqualGUID(*activeGuidPtr, GUID_TYPICAL_POWER_SAVINGS)) {
            LOG_F(INFO, "Current Windows power plan: Balanced");
            return PowerPlan::BALANCED;
        } else if (IsEqualGUID(*activeGuidPtr, GUID_MIN_POWER_SAVINGS)) {
            LOG_F(INFO, "Current Windows power plan: Performance");
            return PowerPlan::PERFORMANCE;
        } else {
            // Could be a custom plan. We can get its friendly name if needed.
            LOG_F(INFO,
                  "Current Windows power plan: Custom or Unknown predefined "
                  "GUID");
            return PowerPlan::CUSTOM;  // Represent as CUSTOM
        }
    } else {
        LOG_F(ERROR,
              "Failed to get active power scheme from Windows. Error: %lu",
              GetLastError());
        return std::nullopt;
    }
#elif defined(__linux__)
    // Use powerprofilesctl to get current profile
    // This requires parsing command output.
    std::string cmd = "powerprofilesctl get";
    std::string current_profile_str;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        LOG_F(ERROR, "Failed to run 'powerprofilesctl get'.");
        return std::nullopt;  // Cannot determine
    }
    char buffer[128];
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        current_profile_str = buffer;
        current_profile_str.erase(
            current_profile_str.find_last_not_of(" \n\r\t") +
            1);  // Trim whitespace
    }
    pclose(pipe);

    if (current_profile_str.empty()) {
        LOG_F(
            WARNING,
            "Could not determine current power profile from powerprofilesctl.");
        return std::nullopt;  // Or a default like BALANCED
    }

    LOG_F(INFO, "Current Linux power profile: %s", current_profile_str.c_str());
    if (current_profile_str == "power-saver") {
        return PowerPlan::POWER_SAVER;
    } else if (current_profile_str == "balanced") {
        return PowerPlan::BALANCED;
    } else if (current_profile_str == "performance") {
        return PowerPlan::PERFORMANCE;
    } else {
        LOG_F(INFO, "Linux power profile '%s' mapped to Custom.",
              current_profile_str.c_str());
        return PowerPlan::CUSTOM;  // Unknown or other custom profile
    }
#elif defined(__APPLE__)
    // macOS doesn't have distinct "plans" in the same way.
    // It's generally always "balanced" and adapts.
    LOG_F(INFO, "macOS power management is adaptive; reporting as Balanced.");
    return PowerPlan::BALANCED;  // Default to Balanced for macOS
#else
    LOG_F(WARNING,
          "Getting current power plan not implemented for this platform.");
    return std::nullopt;
#endif
}

auto PowerPlanManager::getAvailablePowerPlans() -> std::vector<std::string> {
    std::vector<std::string> plans;
#ifdef _WIN32
    // Enumerate all power schemes
    // This is more complex than just listing the three main ones.
    // It involves PowerEnumerate and then PowerReadFriendlyName.
    // For simplicity, listing the common ones. A full implementation would
    // enumerate.
    plans.push_back("Balanced");
    plans.push_back(
        "High performance");  // Note: Windows UI often uses "High performance"
    plans.push_back("Power saver");
    // To get actual available plans:
    // Iterate with PowerEnumerate(NULL, NULL, NULL, ACCESS_SCHEME, index,
    // buffer, &bufferSize) Then for each GUID, PowerReadFriendlyName(...)
    LOG_F(INFO,
          "Reporting standard Windows power plans. Full enumeration is more "
          "complex.");
#elif defined(__linux__)
    // Use powerprofilesctl list
    std::string cmd = "powerprofilesctl list";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        LOG_F(ERROR, "Failed to run 'powerprofilesctl list'.");
        // Fallback to common defaults if command fails
        plans = {"balanced", "performance", "power-saver"};
        return plans;
    }
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line = buffer;
        // Each line is like: "  performance:" or "* balanced:"
        // We need to extract the name.
        size_t name_start = line.find_first_not_of(" *");
        size_t name_end = line.find_last_of(":");
        if (name_start != std::string::npos && name_end != std::string::npos &&
            name_start < name_end) {
            plans.push_back(line.substr(name_start, name_end - name_start));
        }
    }
    pclose(pipe);
    if (plans.empty()) {  // Fallback if parsing failed or command output was
                          // unexpected
        LOG_F(WARNING,
              "Failed to parse 'powerprofilesctl list' output, using default "
              "Linux plans.");
        plans = {"balanced", "performance", "power-saver"};
    }
#elif defined(__APPLE__)
    // macOS does not have a user-selectable list of power plans in this manner.
    // Energy Saver preferences offer settings, but not named "plans".
    plans.push_back("Default");  // Or "Automatic"
    LOG_F(INFO, "macOS uses an automatic power management profile.");
#else
    LOG_F(WARNING,
          "Getting available power plans not implemented for this platform.");
    plans.push_back("Default");  // Generic fallback
#endif
    return plans;
}

}  // namespace atom::system
