#ifdef _WIN32

#include "voltage_windows.hpp"

#include <PowrProf.h>
#include <Windows.h>
#include <comdef.h>
#include <wbemidl.h>

namespace atom::system {

WindowsVoltageMonitor::WindowsVoltageMonitor() { updateBatteryState(); }

WindowsVoltageMonitor::~WindowsVoltageMonitor() {
    // No specific cleanup needed
}

bool WindowsVoltageMonitor::updateBatteryState() const {
    batteryStateValid =
        CallNtPowerInformation(SystemBatteryState, NULL, 0, &batteryState,
                               sizeof(batteryState)) == ERROR_SUCCESS;

    return batteryStateValid;
}

std::optional<double> WindowsVoltageMonitor::getInputVoltage() const {
    // Windows doesn't directly provide AC power voltage
    // Use WMI to get more information, return standard voltage as example
    SYSTEM_POWER_STATUS powerStatus;
    if (GetSystemPowerStatus(&powerStatus)) {
        if (powerStatus.ACLineStatus == 1) {  // AC power connected
            // Try to get actual voltage from WMI
            auto sources = getWMIPowerInfo();
            for (const auto &source : sources) {
                if (source.type == PowerSourceType::AC && source.voltage) {
                    return source.voltage;
                }
            }

            // If WMI failed, return estimated standard voltage
            return 220.0;  // This is an estimate (could be 110V depending on
                           // country)
        }
    }
    return std::nullopt;
}

std::optional<double> WindowsVoltageMonitor::getBatteryVoltage() const {
    if (!updateBatteryState()) {
        return std::nullopt;
    }

    if (batteryState.BatteryPresent) {
        // SYSTEM_BATTERY_STATE 结构体中没有 Voltage 成员
        // 使用 WMI 作为替代
        auto sources = getWMIPowerInfo();
        for (const auto &source : sources) {
            if (source.type == PowerSourceType::Battery && source.voltage) {
                return source.voltage;
            }
        }

        // 如果无法获取实际电压，返回一个估计值
        return 12.0;  // 笔记本电池通常为 10-13V
    }

    return std::nullopt;
}

std::vector<PowerSourceInfo> WindowsVoltageMonitor::getAllPowerSources() const {
    std::vector<PowerSourceInfo> sources;

    // First try using the standard Windows API
    // Update battery state
    updateBatteryState();

    // Get system power status
    SYSTEM_POWER_STATUS powerStatus;
    if (GetSystemPowerStatus(&powerStatus)) {
        // Add AC power information
        if (powerStatus.ACLineStatus == 1) {
            PowerSourceInfo acInfo;
            acInfo.name = "AC Adapter";
            acInfo.type = PowerSourceType::AC;
            acInfo.voltage = 220.0;  // Estimated value
            sources.push_back(acInfo);
        }

        // Add battery information
        if (batteryStateValid && batteryState.BatteryPresent) {
            PowerSourceInfo batteryInfo;
            batteryInfo.name = "Main Battery";
            batteryInfo.type = PowerSourceType::Battery;

            // 获取电池电压
            auto sources = getWMIPowerInfo();
            for (const auto &source : sources) {
                if (source.type == PowerSourceType::Battery && source.voltage) {
                    batteryInfo.voltage = source.voltage;
                    break;
                }
            }

            // 如果无法通过 WMI 获取，使用默认值
            if (!batteryInfo.voltage) {
                batteryInfo.voltage = 12.0;
            }

            // Calculate battery percentage
            int percent = powerStatus.BatteryLifePercent;
            if (percent <= 100) {
                batteryInfo.chargePercent = percent;
            }

            // Charging status
            batteryInfo.isCharging =
                (powerStatus.BatteryFlag & 8) != 0;  // Charging flag

            // Rate of discharge - 使用电源状态而非电池状态获取放电速率
            if (powerStatus.BatteryFlag & 0x01) {  // Discharging
                // 我们没有实际的放电速率，因此可以使用一个估计值
                // 或者完全省略此部分
                if (batteryInfo.voltage) {
                    // 估计电流 - 一般笔记本在工作时约为 2-3A
                    batteryInfo.current = 2.5;
                }
            }

            sources.push_back(batteryInfo);
        }
    }

    // If we didn't get any sources, or if we want more detailed info,
    // try using WMI
    if (sources.empty()) {
        sources = getWMIPowerInfo();
    }

    return sources;
}

std::string WindowsVoltageMonitor::getPlatformName() const { return "Windows"; }

std::vector<PowerSourceInfo> WindowsVoltageMonitor::getWMIPowerInfo() const {
    std::vector<PowerSourceInfo> sources;

    // Initialize COM
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        return sources;
    }

    // Set general COM security levels
    hr = CoInitializeSecurity(
        NULL,
        -1,                           // Default authentication
        NULL,                         // Default authentication services
        NULL,                         // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,    // Default authentication level
        RPC_C_IMP_LEVEL_IMPERSONATE,  // Default impersonation level
        NULL,                         // Default authentication info
        EOAC_NONE,                    // Additional capabilities
        NULL                          // Reserved
    );

    if (FAILED(hr)) {
        CoUninitialize();
        return sources;
    }

    // Create WMI connection
    IWbemLocator *pLoc = NULL;
    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, (LPVOID *)&pLoc);

    if (FAILED(hr)) {
        CoUninitialize();
        return sources;
    }

    // Connect to WMI
    IWbemServices *pSvc = NULL;
    hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"),  // WMI namespace
                             NULL,                     // User name
                             NULL,                     // User password
                             0,                        // Locale
                             NULL,                     // Security flags
                             0,                        // Authority
                             0,                        // Context object
                             &pSvc                     // IWbemServices proxy
    );

    if (FAILED(hr)) {
        pLoc->Release();
        CoUninitialize();
        return sources;
    }

    // Set security levels for WMI connection
    hr = CoSetProxyBlanket(pSvc,                    // IUnknown proxy
                           RPC_C_AUTHN_WINNT,       // Authentication service
                           RPC_C_AUTHZ_NONE,        // Authorization service
                           NULL,                    // Server principal name
                           RPC_C_AUTHN_LEVEL_CALL,  // Authentication level
                           RPC_C_IMP_LEVEL_IMPERSONATE,  // Impersonation level
                           NULL,                         // Identity of client
                           EOAC_NONE  // Additional capabilities
    );

    if (FAILED(hr)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return sources;
    }

    // Query for battery information
    IEnumWbemClassObject *pEnumerator = NULL;
    hr = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_Battery"),
                         WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                         NULL, &pEnumerator);

    if (SUCCEEDED(hr)) {
        IWbemClassObject *pclsObj = NULL;
        ULONG uReturn = 0;

        while (pEnumerator) {
            hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

            if (uReturn == 0) {
                break;
            }

            PowerSourceInfo batteryInfo;
            batteryInfo.type = PowerSourceType::Battery;

            VARIANT vtProp;

            // Get battery name
            hr = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
                char name[256];
                WideCharToMultiByte(CP_UTF8, 0, vtProp.bstrVal, -1, name,
                                    sizeof(name), NULL, NULL);
                batteryInfo.name = name;
            }
            VariantClear(&vtProp);

            // Get battery voltage
            hr = pclsObj->Get(L"DesignVoltage", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_I4) {
                batteryInfo.voltage = vtProp.lVal / 1000.0;  // mV to V
            }
            VariantClear(&vtProp);

            // Get battery charge percentage
            hr = pclsObj->Get(L"EstimatedChargeRemaining", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_I4) {
                batteryInfo.chargePercent = vtProp.lVal;
            }
            VariantClear(&vtProp);

            // Get battery charging status
            hr = pclsObj->Get(L"BatteryStatus", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_I4) {
                // 2 = Charging, 1 = Discharging, other values = other states
                batteryInfo.isCharging = (vtProp.lVal == 2);
            }
            VariantClear(&vtProp);

            sources.push_back(batteryInfo);
            pclsObj->Release();
        }

        pEnumerator->Release();
    }

    // Query for power supply information (AC adapter)
    hr = pSvc->ExecQuery(bstr_t("WQL"),
                         bstr_t("SELECT * FROM Win32_PowerSupply"),
                         WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                         NULL, &pEnumerator);

    if (SUCCEEDED(hr)) {
        IWbemClassObject *pclsObj = NULL;
        ULONG uReturn = 0;

        while (pEnumerator) {
            hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

            if (uReturn == 0) {
                break;
            }

            PowerSourceInfo acInfo;
            acInfo.type = PowerSourceType::AC;

            VARIANT vtProp;

            // Get AC adapter name
            hr = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
                char name[256];
                WideCharToMultiByte(CP_UTF8, 0, vtProp.bstrVal, -1, name,
                                    sizeof(name), NULL, NULL);
                acInfo.name = name;
            } else {
                acInfo.name = "AC Power Supply";
            }
            VariantClear(&vtProp);

            // Get input voltage
            hr = pclsObj->Get(L"InputVoltage", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_I4) {
                acInfo.voltage = vtProp.lVal;
            } else {
                // Default to standard voltage if not available
                acInfo.voltage = 220.0;
            }
            VariantClear(&vtProp);

            // Get current
            hr = pclsObj->Get(L"TotalOutputPower", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_I4 && acInfo.voltage) {
                // P = VI, so I = P/V
                acInfo.current = vtProp.lVal / *acInfo.voltage;
            }
            VariantClear(&vtProp);

            sources.push_back(acInfo);
            pclsObj->Release();
        }

        pEnumerator->Release();
    }

    // Clean up
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();

    return sources;
}

}  // namespace atom::system

#endif  // _WIN32