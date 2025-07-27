#ifdef _WIN32

#include "windows.hpp"

// clang-format off
#include <windows.h>
#include <conio.h>
#include <devguid.h>
#include <poclass.h>
#include <powersetting.h>
#include <setupapi.h>
#include <winnt.h>
// clang-format on

#include <memory>
#include <spdlog/spdlog.h>

namespace atom::system::battery::windows {

auto WindowsBatteryProvider::getBatteryInfo() -> std::optional<BatteryInfo> {
    spdlog::debug("Getting Windows battery info via system power status");
    BatteryInfo info;

    SYSTEM_POWER_STATUS powerStatus{};
    if (GetSystemPowerStatus(&powerStatus) != 0) {
        info.isBatteryPresent = powerStatus.BatteryFlag != 128;
        info.isCharging = powerStatus.BatteryFlag == 8 || powerStatus.ACLineStatus == 1;
        info.batteryLifePercent = static_cast<float>(powerStatus.BatteryLifePercent);
        info.batteryLifeTime = powerStatus.BatteryLifeTime == 0xFFFFFFFF
                                   ? 0.0f
                                   : static_cast<float>(powerStatus.BatteryLifeTime);
        info.batteryFullLifeTime = powerStatus.BatteryFullLifeTime == 0xFFFFFFFF
                                       ? 0.0f
                                       : static_cast<float>(powerStatus.BatteryFullLifeTime);

        // Set power state
        if (info.isCharging) {
            info.powerState = PowerState::CHARGING;
        } else if (info.batteryLifePercent >= 95.0f) {
            info.powerState = PowerState::FULL;
        } else if (info.batteryLifePercent <= 5.0f) {
            info.powerState = PowerState::CRITICAL;
        } else {
            info.powerState = PowerState::DISCHARGING;
        }

        info.lastUpdated = std::chrono::system_clock::now();

        spdlog::debug("Battery present: {}, charging: {}, level: {:.2f}%",
                      info.isBatteryPresent, info.isCharging, info.batteryLifePercent);
        return info;
    } else {
        spdlog::error("Failed to get system power status: {}", GetLastError());
        return std::nullopt;
    }
}

auto WindowsBatteryProvider::getDetailedBatteryInfo() -> BatteryResult {
    auto basicInfo = getBatteryInfo();
    if (!basicInfo) {
        return BatteryError::READ_ERROR;
    }

    BatteryInfo info = std::move(*basicInfo);

    // Get detailed information using device APIs
    HDEVINFO hdev = SetupDiGetClassDevs(&GUID_DEVCLASS_BATTERY, 0, 0,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hdev == INVALID_HANDLE_VALUE) {
        spdlog::error("SetupDiGetClassDevs failed: {}", GetLastError());
        return BatteryError::ACCESS_DENIED;
    }

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

    if (!SetupDiEnumDeviceInterfaces(hdev, NULL, &GUID_DEVCLASS_BATTERY, 0, &did)) {
        spdlog::error("SetupDiEnumDeviceInterfaces failed: {}", GetLastError());
        return info;  // Return basic info if detailed fails
    }

    DWORD cbRequired = 0;
    SetupDiGetDeviceInterfaceDetail(hdev, &did, NULL, 0, &cbRequired, NULL);

    if (cbRequired == 0) {
        spdlog::warn("No device interface detail available");
        return info;
    }

    auto pspdidd = std::make_unique<BYTE[]>(cbRequired);
    auto detailData = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(pspdidd.get());
    detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    if (!SetupDiGetDeviceInterfaceDetail(hdev, &did, detailData, cbRequired, NULL, NULL)) {
        spdlog::error("SetupDiGetDeviceInterfaceDetail failed: {}", GetLastError());
        return info;
    }

    HANDLE hBattery = CreateFile(detailData->DevicePath, GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (hBattery == INVALID_HANDLE_VALUE) {
        spdlog::error("CreateFile failed for battery device: {}", GetLastError());
        return info;
    }

    struct FileHandleCloser {
        HANDLE handle;
        ~FileHandleCloser() {
            if (handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);
        }
    } batteryFileHandleCloser{hBattery};

    // Query battery information
    BATTERY_QUERY_INFORMATION bqi{};
    DWORD bytesReturned;

    // Get manufacturer name
    bqi.InformationLevel = BatteryManufactureName;
    std::wstring manufacturerName(128, L'\0');
    if (DeviceIoControl(hBattery, IOCTL_BATTERY_QUERY_INFORMATION, &bqi, sizeof(bqi),
                        manufacturerName.data(), manufacturerName.size() * sizeof(wchar_t),
                        &bytesReturned, NULL) &&
        bytesReturned > 0) {
        manufacturerName.resize(bytesReturned / sizeof(wchar_t) > 0
                                    ? (bytesReturned / sizeof(wchar_t)) - 1
                                    : 0);
        info.manufacturer.assign(manufacturerName.begin(), manufacturerName.end());
    }

    // Get device name (model)
    bqi.InformationLevel = BatteryDeviceName;
    std::wstring modelName(128, L'\0');
    if (DeviceIoControl(hBattery, IOCTL_BATTERY_QUERY_INFORMATION, &bqi, sizeof(bqi),
                        modelName.data(), modelName.size() * sizeof(wchar_t), &bytesReturned,
                        NULL) &&
        bytesReturned > 0) {
        modelName.resize(bytesReturned / sizeof(wchar_t) > 0 ? (bytesReturned / sizeof(wchar_t)) - 1
                                                             : 0);
        info.model.assign(modelName.begin(), modelName.end());
    }

    return info;
}

auto WindowsBatteryProvider::getAllBatteries() -> MultiBatteryInfo {
    MultiBatteryInfo result;

    // For now, Windows implementation focuses on primary battery
    auto batteryResult = getDetailedBatteryInfo();
    if (auto* batteryInfo = std::get_if<BatteryInfo>(&batteryResult)) {
        if (batteryInfo->isBatteryPresent) {
            result.batteries.push_back(*batteryInfo);
            result.combined = *batteryInfo;
            result.activeBatteryCount = 1;
            result.totalCapacity = batteryInfo->energyFull;
            result.totalEnergyRemaining = batteryInfo->energyNow;
        }
    }

    return result;
}

auto WindowsBatteryProvider::supportsAdvancedFeatures() -> bool {
    // Check if running on Windows 8 or later for advanced battery features
    OSVERSIONINFOEX osvi{};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 6;
    osvi.dwMinorVersion = 2;  // Windows 8

    DWORDLONG conditionMask = 0;
    VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);

    return VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION, conditionMask);
}

auto WindowsPowerManager::setPowerPlan(PowerPlan plan) -> std::optional<bool> {
    GUID planGuid{};
    switch (plan) {
        case PowerPlan::BALANCED:
            planGuid = GUID_TYPICAL_POWER_SAVINGS;
            break;
        case PowerPlan::PERFORMANCE:
            planGuid = GUID_MIN_POWER_SAVINGS;
            break;
        case PowerPlan::POWER_SAVER:
            planGuid = GUID_MAX_POWER_SAVINGS;
            break;
        case PowerPlan::CUSTOM:
            spdlog::error("Setting custom power plans by enum not supported without GUID");
            return std::nullopt;
        default:
            spdlog::error("Invalid power plan specified for Windows");
            return std::nullopt;
    }

    spdlog::info("Setting Windows power plan");
    DWORD result = PowerSetActiveScheme(NULL, &planGuid);
    if (result != ERROR_SUCCESS) {
        spdlog::error("Failed to set power plan: error {}", result);
        return false;
    }
    spdlog::info("Windows power plan successfully changed");
    return true;
}

auto WindowsPowerManager::getCurrentPowerPlan() -> std::optional<PowerPlan> {
    struct LibraryHandle {
        HMODULE handle;
        explicit LibraryHandle(const wchar_t* name) : handle(LoadLibraryW(name)) {}
        ~LibraryHandle() {
            if (handle)
                FreeLibrary(handle);
        }
        operator HMODULE() const { return handle; }
    } hPowrProf{L"powrprof.dll"};

    if (!hPowrProf) {
        spdlog::error("Failed to load powrprof.dll: {}", GetLastError());
        return std::nullopt;
    }

    using PFN_PowerGetActiveScheme = DWORD(WINAPI*)(HKEY, GUID**);
    auto pGetActiveScheme = reinterpret_cast<PFN_PowerGetActiveScheme>(
        GetProcAddress(hPowrProf, "PowerGetActiveScheme"));

    if (!pGetActiveScheme) {
        spdlog::error("Failed to get PowerGetActiveScheme address: {}", GetLastError());
        return std::nullopt;
    }

    GUID* pActiveSchemeGuid = nullptr;
    if (pGetActiveScheme(NULL, &pActiveSchemeGuid) == ERROR_SUCCESS && pActiveSchemeGuid) {
        struct GuidDeleter {
            void operator()(GUID* ptr) {
                if (ptr)
                    LocalFree(ptr);
            }
        };
        std::unique_ptr<GUID, GuidDeleter> activeGuidPtr(pActiveSchemeGuid);

        if (IsEqualGUID(*activeGuidPtr, GUID_MAX_POWER_SAVINGS)) {
            return PowerPlan::POWER_SAVER;
        } else if (IsEqualGUID(*activeGuidPtr, GUID_TYPICAL_POWER_SAVINGS)) {
            return PowerPlan::BALANCED;
        } else if (IsEqualGUID(*activeGuidPtr, GUID_MIN_POWER_SAVINGS)) {
            return PowerPlan::PERFORMANCE;
        } else {
            return PowerPlan::CUSTOM;
        }
    }

    spdlog::error("Failed to get active power scheme: {}", GetLastError());
    return std::nullopt;
}

auto WindowsPowerManager::getAvailablePowerPlans() -> std::vector<std::string> {
    return {"Balanced", "High performance", "Power saver"};
}

}  // namespace atom::system::battery::windows

#endif  // _WIN32
