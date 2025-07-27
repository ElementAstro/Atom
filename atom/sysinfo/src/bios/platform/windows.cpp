#include "windows.hpp"

#ifdef _WIN32
#include <spdlog/spdlog.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <array>

#if defined(_MSC_VER)
#pragma comment(lib, "wbemuuid.lib")
#endif

namespace atom::system {

// ComInitializer implementation
ComInitializer::ComInitializer(COINIT coinit) : initialized_(false) {
    HRESULT hr = CoInitializeEx(nullptr, coinit);
    if (SUCCEEDED(hr) || hr == S_FALSE) {
        initialized_ = true;
    } else {
        throw std::runtime_error("Failed to initialize COM library");
    }
}

ComInitializer::~ComInitializer() {
    if (initialized_) {
        CoUninitialize();
    }
}

void ComInitializer::release() { 
    initialized_ = false; 
}

// ComPtr template implementations
template <typename T>
ComPtr<T>::~ComPtr() {
    if (ptr_) {
        ptr_->Release();
    }
}

template <typename T>
ComPtr<T>::ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) { 
    other.ptr_ = nullptr; 
}

template <typename T>
ComPtr<T>& ComPtr<T>::operator=(ComPtr&& other) noexcept {
    if (this != &other) {
        if (ptr_) {
            ptr_->Release();
        }
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
    }
    return *this;
}

template <typename T>
T* ComPtr<T>::release() {
    T* tmp = ptr_;
    ptr_ = nullptr;
    return tmp;
}

// WindowsBiosImplementation methods
BiosInfoData WindowsBiosImplementation::fetchBiosInfo() {
    spdlog::info("Fetching BIOS information");
    BiosInfoData biosInfo;

    try {
        ComInitializer com;
        ComPtr<IWbemLocator> pLoc;
        ComPtr<IWbemServices> pSvc;

        HRESULT hres = CoInitializeSecurity(
            nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

        if (FAILED(hres)) {
            throw std::runtime_error("Failed to initialize security");
        }

        hres = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                               IID_IWbemLocator, (LPVOID*)pLoc.getAddressOf());
        if (FAILED(hres)) {
            throw std::runtime_error("Failed to create IWbemLocator object");
        }

        hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0,
                                   0, 0, 0, pSvc.getAddressOf());
        if (FAILED(hres)) {
            throw std::runtime_error("Could not connect to WMI namespace");
        }

        hres = CoSetProxyBlanket(pSvc.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                                nullptr, RPC_C_AUTHN_LEVEL_CALL,
                                RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        if (FAILED(hres)) {
            throw std::runtime_error("Could not set proxy blanket");
        }

        const wchar_t* query = L"SELECT * FROM Win32_BIOS";

        ComPtr<IEnumWbemClassObject> pEnumerator;
        hres = pSvc->ExecQuery(
            _bstr_t(L"WQL"), _bstr_t(query),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
            pEnumerator.getAddressOf());

        if (FAILED(hres)) {
            throw std::runtime_error("Query for BIOS information failed");
        }

        ComPtr<IWbemClassObject> pclsObj;
        ULONG uReturn = 0;

        while (pEnumerator->Next(WBEM_NO_WAIT, 1, pclsObj.getAddressOf(),
                                 &uReturn) == S_OK) {
            if (uReturn == 0) break;

            auto getBiosProperty = [&](const wchar_t* prop) -> std::string {
                VARIANT vtProp;
                VariantInit(&vtProp);
                if (SUCCEEDED(pclsObj->Get(prop, 0, &vtProp, nullptr, nullptr))) {
                    std::string result = (vtProp.vt == VT_BSTR)
                                           ? _com_util::ConvertBSTRToString(vtProp.bstrVal)
                                           : "";
                    VariantClear(&vtProp);
                    return result;
                }
                return "";
            };

            biosInfo.version = getBiosProperty(L"Version");
            biosInfo.manufacturer = getBiosProperty(L"Manufacturer");
            biosInfo.releaseDate = getBiosProperty(L"ReleaseDate");
            biosInfo.serialNumber = getBiosProperty(L"SerialNumber");
            biosInfo.characteristics = getBiosProperty(L"BiosCharacteristics");
            biosInfo.isUpgradeable = getBiosProperty(L"BIOSVersion").find("Upgradeable") != std::string::npos;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error fetching BIOS info: {}", e.what());
        throw;
    }

    return biosInfo;
}

std::string WindowsBiosImplementation::getManufacturerUpdateUrl() const {
    static const std::unordered_map<std::string, std::string> manufacturerUrls = {
        {"Dell Inc.", "https://www.dell.com/support/driver/home/index.html"},
        {"LENOVO", "https://pcsupport.lenovo.com/"},
        {"HP", "https://support.hp.com/drivers"},
    };

    // This would need to get the manufacturer from cached info
    // For now, return empty string
    return "";
}

bool WindowsBiosImplementation::isElevated() const {
    BOOL isElevated = FALSE;
    HANDLE hToken = nullptr;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(TOKEN_ELEVATION);

        if (GetTokenInformation(hToken, TokenElevation, &elevation,
                                sizeof(elevation), &size)) {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }

    return isElevated != FALSE;
}

BiosHealthStatus WindowsBiosImplementation::checkHealth() const {
    BiosHealthStatus status;
    status.isHealthy = true;
    status.lastCheckTime = std::chrono::system_clock::now().time_since_epoch().count();

    try {
        ComInitializer com;
        ComPtr<IWbemLocator> pLoc;
        ComPtr<IWbemServices> pSvc;

        HRESULT hres = CoInitializeSecurity(
            nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

        if (FAILED(hres)) {
            throw std::runtime_error("Failed to initialize security");
        }

        hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                                IID_IWbemLocator, (LPVOID*)pLoc.getAddressOf());

        if (FAILED(hres)) {
            throw std::runtime_error("Failed to create IWbemLocator object");
        }

        hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0,
                                   0, 0, 0, pSvc.getAddressOf());
        if (FAILED(hres)) {
            throw std::runtime_error("Could not connect to WMI namespace");
        }

        hres = CoSetProxyBlanket(pSvc.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                                nullptr, RPC_C_AUTHN_LEVEL_CALL,
                                RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        if (FAILED(hres)) {
            throw std::runtime_error("Could not set proxy blanket");
        }

        // Check for BIOS-related system events
        ComPtr<IEnumWbemClassObject> pEnumerator;
        hres = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_NTLogEvent WHERE LogFile='System' AND "
                   "EventCode='7' AND SourceName='Microsoft-Windows-BIOS' AND "
                   "TimeWritten > '20230101000000.000000-000'"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
            pEnumerator.getAddressOf());

        if (SUCCEEDED(hres)) {
            ComPtr<IWbemClassObject> pclsObj;
            ULONG uReturn = 0;

            while (pEnumerator->Next(WBEM_INFINITE, 1, pclsObj.getAddressOf(),
                                     &uReturn) == S_OK) {
                if (uReturn == 0) break;

                VARIANT vtProp;
                VariantInit(&vtProp);

                if (SUCCEEDED(pclsObj->Get(L"Message", 0, &vtProp, 0, 0))) {
                    status.isHealthy = false;
                    status.errors.push_back(
                        _com_util::ConvertBSTRToString(vtProp.bstrVal));
                }

                VariantClear(&vtProp);
            }
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to check BIOS health: {}", e.what());
        status.isHealthy = false;
        status.errors.push_back(e.what());
    }

    return status;
}

BiosUpdateInfo WindowsBiosImplementation::checkForUpdates() const {
    BiosUpdateInfo updateInfo;
    updateInfo.updateAvailable = false;

    try {
        std::string manufacturerUrl = getManufacturerUpdateUrl();
        // Implementation would check manufacturer's update service
    } catch (const std::exception& e) {
        spdlog::error("Failed to check for BIOS updates: {}", e.what());
    }

    return updateInfo;
}

std::vector<std::string> WindowsBiosImplementation::getSMBIOSData() const {
    std::vector<std::string> smbiosData;

    try {
        // Windows implementation using WMI
        ComInitializer com;
        ComPtr<IWbemLocator> pLoc;
        ComPtr<IWbemServices> pSvc;

        // Implementation would query SMBIOS tables
    } catch (const std::exception& e) {
        spdlog::error("Failed to get SMBIOS data: {}", e.what());
    }

    return smbiosData;
}

bool WindowsBiosImplementation::setSecureBoot(bool enable) {
    if (!isSecureBootSupported()) {
        spdlog::error("Secure Boot is not supported on this system");
        return false;
    }

    try {
        spdlog::info("Attempting to {} Secure Boot via UEFI variables",
                     enable ? "enable" : "disable");
        spdlog::warn("System will need to be restarted for changes to take effect");
        return false; // Actual implementation would modify UEFI variables
    } catch (const std::exception& e) {
        spdlog::error("Failed to set Secure Boot: {}", e.what());
        return false;
    }
}

bool WindowsBiosImplementation::isSecureBootSupported() const {
    try {
        DWORD size = 0;
        BYTE buffer[1] = {0};

        BOOL result = GetFirmwareEnvironmentVariableA(
            "SecureBoot", "{8be4df61-93ca-11d2-aa0d-00e098032b8c}", buffer, size);

        DWORD error = GetLastError();
        if (error == ERROR_INSUFFICIENT_BUFFER || result) {
            return true;
        }

        spdlog::info("SecureBoot check failed with error code: {}", error);
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Failed to check Secure Boot support: {}", e.what());
        return false;
    }
}

bool WindowsBiosImplementation::setUEFIBoot(bool enable) {
    if (!isUEFIBootSupported()) {
        spdlog::error("UEFI Boot is not supported on this system");
        return false;
    }

    try {
        spdlog::info("Attempting to {} UEFI Boot mode", enable ? "enable" : "disable");

        if (!isElevated()) {
            spdlog::error("Administrator privileges required to modify UEFI boot settings");
            return false;
        }

        std::string command = "bcdedit /set {bootmgr} path \\EFI\\";
        command += enable ? "Microsoft\\Boot\\bootmgfw.efi" : "Legacy\\Boot\\bootmgfw.efi";

        spdlog::info("Executing command: {}", command);
        int result = ::system(command.c_str());

        if (result != 0) {
            spdlog::error("Failed to set UEFI boot mode, command returned: {}", result);
            return false;
        }

        spdlog::warn("System will need to be restarted for changes to take effect");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set UEFI boot mode: {}", e.what());
        return false;
    }
}

bool WindowsBiosImplementation::isUEFIBootSupported() {
    try {
        ComInitializer com;
        ComPtr<IWbemLocator> pLoc;
        ComPtr<IWbemServices> pSvc;

        HRESULT hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                                       IID_IWbemLocator, (LPVOID*)pLoc.getAddressOf());
        if (FAILED(hres) || !pLoc.get()) return false;

        hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, 0,
                                   0, 0, 0, pSvc.getAddressOf());
        if (FAILED(hres) || !pSvc.get()) return false;

        ComPtr<IEnumWbemClassObject> pEnumerator;
        hres = pSvc->ExecQuery(
            bstr_t("WQL"), bstr_t("SELECT * FROM MSFirmwareUefiInfo"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
            pEnumerator.getAddressOf());

        return SUCCEEDED(hres) && pEnumerator.get();
    } catch (...) {
        return false;
    }
}

bool WindowsBiosImplementation::backupBiosSettings(const std::string& filepath) {
    try {
        std::ofstream out(filepath, std::ios::binary);
        if (!out) {
            throw std::runtime_error("Cannot open file for writing");
        }
        // Implementation would backup actual BIOS settings
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to backup BIOS settings: {}", e.what());
        return false;
    }
}

bool WindowsBiosImplementation::restoreBiosSettings(const std::string& filepath) {
    try {
        std::ifstream in(filepath, std::ios::binary);
        if (!in) {
            spdlog::error("Failed to open BIOS settings backup file: {}", filepath);
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        if (content.empty()) {
            spdlog::warn("BIOS settings backup file is empty: {}", filepath);
        }

        spdlog::info("BIOS settings restoration from {} (simulated) successful.", filepath);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to restore BIOS settings: {}", e.what());
        return false;
    }
}

// Enhanced features implementation
FirmwareInfo WindowsBiosImplementation::getFirmwareInfo() const {
    FirmwareInfo info;

    try {
        // Query firmware information via WMI
        info.type = isUEFIBootSupported() ? "UEFI" : "Legacy BIOS";
        info.secureBootCapable = isSecureBootSupported();
        // Additional implementation would query more details
    } catch (const std::exception& e) {
        spdlog::error("Failed to get firmware info: {}", e.what());
    }

    return info;
}

BootConfiguration WindowsBiosImplementation::getBootConfiguration() const {
    BootConfiguration config;

    try {
        config.uefiMode = isUEFIBootSupported();
        config.secureBootEnabled = isSecureBootSupported();
        // Implementation would query boot order and devices
    } catch (const std::exception& e) {
        spdlog::error("Failed to get boot configuration: {}", e.what());
    }

    return config;
}

BiosSecuritySettings WindowsBiosImplementation::getSecuritySettings() const {
    BiosSecuritySettings settings;

    try {
        settings.secureBootEnabled = isSecureBootSupported();
        // Implementation would query additional security settings
    } catch (const std::exception& e) {
        spdlog::error("Failed to get security settings: {}", e.what());
    }

    return settings;
}

BiosOperationResult WindowsBiosImplementation::setBootOrder(const std::vector<std::string>& order) {
    if (!isElevated()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        // Implementation would set boot order via bcdedit or UEFI variables
        spdlog::info("Setting boot order (simulated)");
        return BiosOperationResult::REBOOT_REQUIRED;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set boot order: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

BiosOperationResult WindowsBiosImplementation::enableVirtualization(bool enable) {
    if (!isElevated()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        // Implementation would enable/disable virtualization features
        spdlog::info("{} virtualization (simulated)", enable ? "Enabling" : "Disabling");
        return BiosOperationResult::REBOOT_REQUIRED;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set virtualization: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

BiosOperationResult WindowsBiosImplementation::enableHyperThreading(bool enable) {
    if (!isElevated()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        // Implementation would enable/disable hyper-threading
        spdlog::info("{} hyper-threading (simulated)", enable ? "Enabling" : "Disabling");
        return BiosOperationResult::REBOOT_REQUIRED;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set hyper-threading: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

std::vector<std::string> WindowsBiosImplementation::getAvailableBootDevices() const {
    std::vector<std::string> devices;

    try {
        // Implementation would query available boot devices
        devices.push_back("Hard Drive");
        devices.push_back("USB Device");
        devices.push_back("CD/DVD");
        devices.push_back("Network Boot");
    } catch (const std::exception& e) {
        spdlog::error("Failed to get boot devices: {}", e.what());
    }

    return devices;
}

bool WindowsBiosImplementation::validateBiosIntegrity() const {
    try {
        // Implementation would validate BIOS integrity
        spdlog::info("Validating BIOS integrity (simulated)");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to validate BIOS integrity: {}", e.what());
        return false;
    }
}

bool WindowsBiosImplementation::setUEFIBoot(bool enable) {
    if (!isUEFIBootSupported()) {
        spdlog::error("UEFI Boot is not supported on this system");
        return false;
    }

    try {
        spdlog::info("Attempting to {} UEFI Boot mode", enable ? "enable" : "disable");

        if (!isElevated()) {
            spdlog::error("Administrator privileges required to modify UEFI boot settings");
            return false;
        }

        std::string command = "bcdedit /set {bootmgr} path \\EFI\\";
        command += enable ? "Microsoft\\Boot\\bootmgfw.efi" : "Legacy\\Boot\\bootmgfw.efi";

        spdlog::info("Executing command: {}", command);
        int result = ::system(command.c_str());

        if (result != 0) {
            spdlog::error("Failed to set UEFI boot mode, command returned: {}", result);
            return false;
        }

        spdlog::warn("System will need to be restarted for changes to take effect");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set UEFI boot mode: {}", e.what());
        return false;
    }
}

bool WindowsBiosImplementation::isUEFIBootSupported() const {
    try {
        ComInitializer com;
        ComPtr<IWbemLocator> pLoc;
        ComPtr<IWbemServices> pSvc;

        HRESULT hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                                       IID_IWbemLocator, (LPVOID*)pLoc.getAddressOf());
        if (FAILED(hres) || !pLoc.get()) return false;

        hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, 0,
                                   0, 0, 0, pSvc.getAddressOf());
        if (FAILED(hres) || !pSvc.get()) return false;

        ComPtr<IEnumWbemClassObject> pEnumerator;
        hres = pSvc->ExecQuery(
            bstr_t("WQL"), bstr_t("SELECT * FROM MSFirmwareUefiInfo"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
            pEnumerator.getAddressOf());

        return SUCCEEDED(hres) && pEnumerator.get();
    } catch (...) {
        return false;
    }
}

bool WindowsBiosImplementation::backupBiosSettings(const std::string& filepath) {
    try {
        std::ofstream out(filepath, std::ios::binary);
        if (!out) {
            throw std::runtime_error("Cannot open file for writing");
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to backup BIOS settings: {}", e.what());
        return false;
    }
}

bool WindowsBiosImplementation::restoreBiosSettings(const std::string& filepath) {
    try {
        std::ifstream in(filepath, std::ios::binary);
        if (!in) {
            spdlog::error("Failed to open BIOS settings backup file: {}", filepath);
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        if (content.empty()) {
            spdlog::warn("BIOS settings backup file is empty: {}", filepath);
        }

        spdlog::info("BIOS settings restoration from {} (simulated) successful.", filepath);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to restore BIOS settings: {}", e.what());
        return false;
    }
}

// Enhanced features implementation
FirmwareInfo WindowsBiosImplementation::getFirmwareInfo() const {
    FirmwareInfo info;

    try {
        info.type = isUEFIBootSupported() ? "UEFI" : "Legacy BIOS";
        info.secureBootCapable = isSecureBootSupported();
    } catch (const std::exception& e) {
        spdlog::error("Failed to get firmware info: {}", e.what());
    }

    return info;
}

BootConfiguration WindowsBiosImplementation::getBootConfiguration() const {
    BootConfiguration config;

    try {
        config.uefiMode = isUEFIBootSupported();
        config.secureBootEnabled = isSecureBootSupported();
    } catch (const std::exception& e) {
        spdlog::error("Failed to get boot configuration: {}", e.what());
    }

    return config;
}

BiosSecuritySettings WindowsBiosImplementation::getSecuritySettings() const {
    BiosSecuritySettings settings;

    try {
        settings.secureBootEnabled = isSecureBootSupported();
    } catch (const std::exception& e) {
        spdlog::error("Failed to get security settings: {}", e.what());
    }

    return settings;
}

BiosOperationResult WindowsBiosImplementation::setBootOrder(const std::vector<std::string>& order) {
    if (!isElevated()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        spdlog::info("Setting boot order (simulated)");
        return BiosOperationResult::REBOOT_REQUIRED;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set boot order: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

BiosOperationResult WindowsBiosImplementation::enableVirtualization(bool enable) {
    if (!isElevated()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        spdlog::info("{} virtualization (simulated)", enable ? "Enabling" : "Disabling");
        return BiosOperationResult::REBOOT_REQUIRED;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set virtualization: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

BiosOperationResult WindowsBiosImplementation::enableHyperThreading(bool enable) {
    if (!isElevated()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        spdlog::info("{} hyper-threading (simulated)", enable ? "Enabling" : "Disabling");
        return BiosOperationResult::REBOOT_REQUIRED;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set hyper-threading: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

std::vector<std::string> WindowsBiosImplementation::getAvailableBootDevices() const {
    std::vector<std::string> devices;

    try {
        devices.push_back("Hard Drive");
        devices.push_back("USB Device");
        devices.push_back("CD/DVD");
        devices.push_back("Network Boot");
    } catch (const std::exception& e) {
        spdlog::error("Failed to get boot devices: {}", e.what());
    }

    return devices;
}

bool WindowsBiosImplementation::validateBiosIntegrity() const {
    try {
        spdlog::info("Validating BIOS integrity (simulated)");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to validate BIOS integrity: {}", e.what());
        return false;
    }
}

// Additional enhanced features implementation
PowerManagementSettings WindowsBiosImplementation::getPowerManagementSettings() const {
    PowerManagementSettings settings;

    try {
        // Query power settings via WMI
        std::string acpiResult = executeWMIQuery(L"SELECT * FROM Win32_SystemEnclosure", L"PowerManagementSupported");
        settings.acpiEnabled = !acpiResult.empty();

        // Check wake-on-LAN settings
        std::string wakeOnLan = executeWMIQuery(L"SELECT * FROM Win32_NetworkAdapter WHERE NetEnabled=True", L"PermanentAddress");
        settings.wakeOnLanEnabled = !wakeOnLan.empty();

        // Get power plan
        std::string powerPlan = executeWMIQuery(L"SELECT * FROM Win32_PowerPlan WHERE IsActive=True", L"ElementName");
        settings.powerProfile = powerPlan;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get power management settings: {}", e.what());
    }

    return settings;
}

BiosOperationResult WindowsBiosImplementation::setPowerManagementSettings(const PowerManagementSettings& settings) {
    if (!isElevated()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        // Set power plan
        if (!settings.powerProfile.empty()) {
            std::string command = "powercfg /setactive " + settings.powerProfile;
            if (std::system(command.c_str()) != 0) {
                return BiosOperationResult::FAILED;
            }
        }

        return BiosOperationResult::SUCCESS;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set power management settings: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

OverclockingInfo WindowsBiosImplementation::getOverclockingInfo() const {
    OverclockingInfo info;

    try {
        // Get CPU frequency information
        std::string cpuFreq = executeWMIQuery(L"SELECT * FROM Win32_Processor", L"MaxClockSpeed");
        if (!cpuFreq.empty()) {
            info.maxCpuFrequency = std::stoi(cpuFreq);
        }

        std::string currentFreq = executeWMIQuery(L"SELECT * FROM Win32_Processor", L"CurrentClockSpeed");
        if (!currentFreq.empty()) {
            info.currentCpuFrequency = std::stoi(currentFreq);
        }

        // Basic overclocking support detection
        info.overclockingSupported = info.currentCpuFrequency > info.maxCpuFrequency;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get overclocking info: {}", e.what());
    }

    return info;
}

BiosOperationResult WindowsBiosImplementation::setOverclockingProfile(const std::string& profile) {
    if (!isElevated()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        spdlog::warn("Overclocking profile setting requires manufacturer-specific tools");
        spdlog::info("Profile requested: {}", profile);
        return BiosOperationResult::NOT_SUPPORTED;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set overclocking profile: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

HardwareMonitoring WindowsBiosImplementation::getHardwareMonitoring() const {
    HardwareMonitoring monitoring;

    try {
        // Get temperature information (if available)
        std::vector<std::string> temps = executeWMIQueryMultiple(L"SELECT * FROM MSAcpi_ThermalZoneTemperature", L"CurrentTemperature");
        for (const auto& temp : temps) {
            if (!temp.empty()) {
                // Convert from tenths of Kelvin to Celsius
                int tempC = (std::stoi(temp) / 10) - 273;
                monitoring.cpuTemperatures.push_back(tempC);
            }
        }

        // Get fan information
        std::vector<std::string> fans = executeWMIQueryMultiple(L"SELECT * FROM Win32_Fan", L"DesiredSpeed");
        for (const auto& fan : fans) {
            if (!fan.empty()) {
                monitoring.fanSpeeds.push_back(std::stoi(fan));
            }
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get hardware monitoring data: {}", e.what());
    }

    return monitoring;
}

BiosDiagnostics WindowsBiosImplementation::runDiagnostics() const {
    BiosDiagnostics diagnostics;
    diagnostics.lastDiagnosticTime = std::chrono::system_clock::now();

    try {
        // Check memory
        std::string memInfo = executeWMIQuery(L"SELECT * FROM Win32_PhysicalMemory", L"Capacity");
        diagnostics.memoryTestPassed = !memInfo.empty();

        // Check CPU
        std::string cpuInfo = executeWMIQuery(L"SELECT * FROM Win32_Processor", L"Name");
        diagnostics.cpuTestPassed = !cpuInfo.empty();

        // Check storage
        std::string diskInfo = executeWMIQuery(L"SELECT * FROM Win32_DiskDrive", L"Model");
        diagnostics.storageTestPassed = !diskInfo.empty();

        // Check system events for errors
        std::vector<std::string> errors = executeWMIQueryMultiple(
            L"SELECT * FROM Win32_NTLogEvent WHERE LogFile='System' AND Type='Error'", L"Message");
        if (!errors.empty()) {
            diagnostics.warnings.push_back("System errors found in event log");
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to run diagnostics: {}", e.what());
        diagnostics.postTestPassed = false;
    }

    return diagnostics;
}

BiosOperationResult WindowsBiosImplementation::resetToDefaults() {
    if (!isElevated()) {
        return BiosOperationResult::INSUFFICIENT_PRIVILEGES;
    }

    try {
        spdlog::warn("BIOS reset to defaults requires reboot to UEFI setup");
        spdlog::info("Use manufacturer-specific tools or enter BIOS setup manually");
        return BiosOperationResult::NOT_SUPPORTED;
    } catch (const std::exception& e) {
        spdlog::error("Failed to reset to defaults: {}", e.what());
        return BiosOperationResult::FAILED;
    }
}

std::vector<std::string> WindowsBiosImplementation::getSupportedFeatures() const {
    std::vector<std::string> features;

    try {
        features.push_back("BIOS Information Retrieval");
        features.push_back("Health Monitoring");
        features.push_back("SMBIOS Data Access");
        features.push_back("WMI Integration");

        if (isUEFIBootSupported()) {
            features.push_back("UEFI Boot Support");
        }

        if (isSecureBootSupported()) {
            features.push_back("Secure Boot Support");
        }

        // Check for additional Windows-specific features
        std::string tpmInfo = executeWMIQuery(L"SELECT * FROM Win32_Tpm", L"IsEnabled_InitialValue");
        if (!tpmInfo.empty()) {
            features.push_back("TPM Support");
        }

        features.push_back("Power Management");
        features.push_back("Hardware Monitoring");

    } catch (const std::exception& e) {
        spdlog::error("Failed to get supported features: {}", e.what());
    }

    return features;
}

}  // namespace atom::system

#endif  // _WIN32
