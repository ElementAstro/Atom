#ifdef _WIN32

#include "windows.hpp"
#include <spdlog/spdlog.h>
#include <Wbemidl.h>
#include <comdef.h>
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace atom::system {

class WindowsSystemInfo::Impl {
public:
    IWbemLocator* pLoc = nullptr;
    IWbemServices* pSvc = nullptr;
    bool wmiInitialized = false;
    std::string lastError;

    ~Impl() {
        cleanup();
    }

    bool initializeWmi() {
        if (wmiInitialized) return true;

        spdlog::debug("Initializing WMI components");

        HRESULT hres = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hres)) {
            lastError = "Failed to initialize COM library. HRESULT: 0x" + std::to_string(hres);
            spdlog::error(lastError);
            return false;
        }

        hres = CoInitializeSecurity(
            nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

        if (FAILED(hres)) {
            lastError = "Failed to initialize COM security. HRESULT: 0x" + std::to_string(hres);
            spdlog::error(lastError);
            CoUninitialize();
            return false;
        }

        hres = CoCreateInstance(CLSID_WbemLocator, nullptr,
                                CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                                reinterpret_cast<LPVOID*>(&pLoc));

        if (FAILED(hres)) {
            lastError = "Failed to create IWbemLocator object. HRESULT: 0x" + std::to_string(hres);
            spdlog::error(lastError);
            CoUninitialize();
            return false;
        }

        hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0,
                                   0, 0, 0, &pSvc);

        if (FAILED(hres)) {
            lastError = "Failed to connect to WMI namespace. HRESULT: 0x" + std::to_string(hres);
            spdlog::error(lastError);
            pLoc->Release();
            CoUninitialize();
            return false;
        }

        hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                              nullptr, RPC_C_AUTHN_LEVEL_CALL,
                              RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        if (FAILED(hres)) {
            lastError = "Failed to set proxy blanket. HRESULT: 0x" + std::to_string(hres);
            spdlog::error(lastError);
            pSvc->Release();
            pLoc->Release();
            CoUninitialize();
            return false;
        }

        wmiInitialized = true;
        spdlog::debug("WMI initialized successfully");
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
        if (wmiInitialized) {
            CoUninitialize();
            wmiInitialized = false;
        }
    }

    std::string getWmiProperty(const std::wstring& wmiClass, const std::wstring& property) {
        if (!initializeWmi()) return "";

        spdlog::info("Retrieving WMI property: Class={}, Property={}",
                     std::string(wmiClass.begin(), wmiClass.end()),
                     std::string(property.begin(), property.end()));

        IEnumWbemClassObject* pEnumerator = nullptr;
        IWbemClassObject* pclsObj = nullptr;
        ULONG uReturn = 0;
        std::string result;

        HRESULT hres = pSvc->ExecQuery(
            bstr_t("WQL"), bstr_t((L"SELECT * FROM " + wmiClass).c_str()),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
            &pEnumerator);

        if (FAILED(hres)) {
            lastError = "WMI query execution failed with HRESULT: 0x" + std::to_string(hres);
            spdlog::error(lastError);
            return "";
        }

        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (0 == uReturn) break;

            VARIANT vtProp;
            hr = pclsObj->Get(property.c_str(), 0, &vtProp, nullptr, nullptr);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR && vtProp.bstrVal != nullptr) {
                result = _bstr_t(vtProp.bstrVal);
                spdlog::debug("Retrieved WMI property value: {}", result);
            }
            VariantClear(&vtProp);
            pclsObj->Release();
        }

        if (pEnumerator) pEnumerator->Release();
        return result;
    }

    std::vector<std::string> getWmiPropertyMultiple(const std::wstring& wmiClass, const std::wstring& property) {
        if (!initializeWmi()) return {};

        spdlog::info("Retrieving multiple WMI properties: Class={}, Property={}",
                     std::string(wmiClass.begin(), wmiClass.end()),
                     std::string(property.begin(), property.end()));

        IEnumWbemClassObject* pEnumerator = nullptr;
        IWbemClassObject* pclsObj = nullptr;
        ULONG uReturn = 0;
        std::vector<std::string> results;

        HRESULT hres = pSvc->ExecQuery(
            bstr_t("WQL"), bstr_t((L"SELECT * FROM " + wmiClass).c_str()),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
            &pEnumerator);

        if (FAILED(hres)) {
            lastError = "WMI query execution failed with HRESULT: 0x" + std::to_string(hres);
            spdlog::error(lastError);
            return results;
        }

        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (0 == uReturn) break;

            VARIANT vtProp;
            hr = pclsObj->Get(property.c_str(), 0, &vtProp, nullptr, nullptr);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR && vtProp.bstrVal != nullptr) {
                std::string value = static_cast<const char*>(_bstr_t(vtProp.bstrVal));
                results.emplace_back(value);
                spdlog::debug("Retrieved WMI property value: {}", value);
            }
            VariantClear(&vtProp);
            pclsObj->Release();
        }

        if (pEnumerator) pEnumerator->Release();
        return results;
    }

    std::map<std::string, std::string> getWmiObjectProperties(const std::wstring& wmiClass,
                                                               const std::vector<std::wstring>& properties) {
        std::map<std::string, std::string> result;
        if (!initializeWmi()) return result;

        IEnumWbemClassObject* pEnumerator = nullptr;
        IWbemClassObject* pclsObj = nullptr;
        ULONG uReturn = 0;

        HRESULT hres = pSvc->ExecQuery(
            bstr_t("WQL"), bstr_t((L"SELECT * FROM " + wmiClass).c_str()),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
            &pEnumerator);

        if (FAILED(hres)) {
            lastError = "WMI query execution failed with HRESULT: 0x" + std::to_string(hres);
            return result;
        }

        if (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (uReturn > 0) {
                for (const auto& property : properties) {
                    VARIANT vtProp;
                    hr = pclsObj->Get(property.c_str(), 0, &vtProp, nullptr, nullptr);
                    if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR && vtProp.bstrVal != nullptr) {
                        std::string key(property.begin(), property.end());
                        result[key] = static_cast<const char*>(_bstr_t(vtProp.bstrVal));
                    }
                    VariantClear(&vtProp);
                }
                pclsObj->Release();
            }
            pEnumerator->Release();
        }

        return result;
    }

    std::vector<std::map<std::string, std::string>> getWmiObjectsProperties(const std::wstring& wmiClass,
                                                                             const std::vector<std::wstring>& properties) {
        std::vector<std::map<std::string, std::string>> results;
        if (!initializeWmi()) return results;

        IEnumWbemClassObject* pEnumerator = nullptr;
        IWbemClassObject* pclsObj = nullptr;
        ULONG uReturn = 0;

        HRESULT hres = pSvc->ExecQuery(
            bstr_t("WQL"), bstr_t((L"SELECT * FROM " + wmiClass).c_str()),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
            &pEnumerator);

        if (FAILED(hres)) {
            lastError = "WMI query execution failed with HRESULT: 0x" + std::to_string(hres);
            return results;
        }

        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (uReturn == 0) break;

            std::map<std::string, std::string> objectProperties;
            for (const auto& property : properties) {
                VARIANT vtProp;
                hr = pclsObj->Get(property.c_str(), 0, &vtProp, nullptr, nullptr);
                if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR && vtProp.bstrVal != nullptr) {
                    std::string key(property.begin(), property.end());
                    objectProperties[key] = static_cast<const char*>(_bstr_t(vtProp.bstrVal));
                }
                VariantClear(&vtProp);
            }

            if (!objectProperties.empty()) {
                results.push_back(objectProperties);
            }
            pclsObj->Release();
        }

        if (pEnumerator) pEnumerator->Release();
        return results;
    }
};

WindowsSystemInfo::WindowsSystemInfo() : impl_(std::make_unique<Impl>()) {
    spdlog::debug("WindowsSystemInfo instance created");
}

WindowsSystemInfo::~WindowsSystemInfo() {
    spdlog::debug("WindowsSystemInfo instance destroyed");
}

WindowsSystemInfo::WindowsSystemInfo(WindowsSystemInfo&& other) noexcept
    : impl_(std::move(other.impl_)) {
    spdlog::debug("WindowsSystemInfo move constructor called");
}

WindowsSystemInfo& WindowsSystemInfo::operator=(WindowsSystemInfo&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        spdlog::debug("WindowsSystemInfo move assignment performed");
    }
    return *this;
}

SystemInfoResult<HardwareSerialData> WindowsSystemInfo::getHardwareSerials() {
    SystemInfoResult<HardwareSerialData> result;

    try {
        result.data.biosSerial = validateSerial(impl_->getWmiProperty(L"Win32_BIOS", L"SerialNumber"));
        result.data.motherboardSerial = validateSerial(impl_->getWmiProperty(L"Win32_BaseBoard", L"SerialNumber"));
        result.data.cpuSerial = validateSerial(impl_->getWmiProperty(L"Win32_Processor", L"ProcessorId"));

        auto diskSerials = impl_->getWmiPropertyMultiple(L"Win32_DiskDrive", L"SerialNumber");
        for (const auto& serial : diskSerials) {
            std::string cleanSerial = validateSerial(serial);
            if (!cleanSerial.empty()) {
                result.data.diskSerials.push_back(cleanSerial);
            }
        }

        result.data.lastUpdate = std::chrono::system_clock::now();
        result.success = result.data.isValid();

        if (!result.success) {
            result.error = SystemInfoError::HARDWARE_NOT_FOUND;
            result.errorMessage = "No valid hardware serial numbers found";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error = SystemInfoError::UNKNOWN_ERROR;
        result.errorMessage = e.what();
    }

    return result;
}

std::string WindowsSystemInfo::validateSerial(const std::string& serial) {
    if (!SystemInfoUtils::isValidSerial(serial)) {
        return "";
    }

    // Additional Windows-specific validation
    std::string cleaned = serial;

    // Remove common Windows placeholder values
    if (cleaned == "To Be Filled By O.E.M." ||
        cleaned == "System Serial Number" ||
        cleaned == "Default string" ||
        cleaned.find("OEM") != std::string::npos) {
        return "";
    }

    return cleaned;
}

bool WindowsSystemInfo::isWmiAvailable() const {
    return impl_->initializeWmi();
}

std::string WindowsSystemInfo::getLastError() const {
    return impl_->lastError;
}

SystemInfoResult<SystemIdentificationData> WindowsSystemInfo::getSystemIdentification() {
    SystemInfoResult<SystemIdentificationData> result;

    try {
        result.data.systemUuid = getSystemUuid();
        result.data.machineId = getMachineGuid();
        result.data.hostname = getComputerName();
        result.data.domainName = getDomainName();
        result.data.macAddresses = getNetworkMacAddresses();
        result.data.lastUpdate = std::chrono::system_clock::now();

        result.success = result.data.isValid();

        if (!result.success) {
            result.error = SystemInfoError::HARDWARE_NOT_FOUND;
            result.errorMessage = "No valid system identification found";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error = SystemInfoError::UNKNOWN_ERROR;
        result.errorMessage = e.what();
    }

    return result;
}

SystemInfoResult<std::vector<MemoryModuleInfo>> WindowsSystemInfo::getMemoryModules() {
    SystemInfoResult<std::vector<MemoryModuleInfo>> result;

    try {
        if (!impl_->initializeWmi()) {
            result.success = false;
            result.error = SystemInfoError::ACCESS_DENIED;
            result.errorMessage = "Failed to initialize WMI";
            return result;
        }

        std::vector<std::wstring> properties = {
            L"SerialNumber", L"Manufacturer", L"PartNumber", L"MemoryType",
            L"Capacity", L"Speed", L"FormFactor", L"DeviceLocator"
        };

        auto memoryObjects = impl_->getWmiObjectsProperties(L"Win32_PhysicalMemory", properties);

        for (const auto& memObj : memoryObjects) {
            MemoryModuleInfo module;

            auto it = memObj.find("SerialNumber");
            if (it != memObj.end()) module.serialNumber = validateSerial(it->second);

            it = memObj.find("Manufacturer");
            if (it != memObj.end()) module.manufacturer = it->second;

            it = memObj.find("PartNumber");
            if (it != memObj.end()) module.partNumber = it->second;

            it = memObj.find("Capacity");
            if (it != memObj.end()) module.sizeBytes = parseMemorySize(it->second);

            it = memObj.find("Speed");
            if (it != memObj.end()) module.speedMHz = parseMemorySpeed(it->second);

            it = memObj.find("DeviceLocator");
            if (it != memObj.end()) {
                // Extract slot number from device locator (e.g., "DIMM_A1" -> 1)
                std::string locator = it->second;
                if (!locator.empty() && std::isdigit(locator.back())) {
                    module.slot = static_cast<uint8_t>(locator.back() - '0');
                }
            }

            if (module.isValid()) {
                result.data.push_back(module);
            }
        }

        result.success = !result.data.empty();

        if (!result.success) {
            result.error = SystemInfoError::HARDWARE_NOT_FOUND;
            result.errorMessage = "No valid memory modules found";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error = SystemInfoError::UNKNOWN_ERROR;
        result.errorMessage = e.what();
    }

    return result;
}

std::string WindowsSystemInfo::getSystemUuid() {
    return impl_->getWmiProperty(L"Win32_ComputerSystemProduct", L"UUID");
}

std::string WindowsSystemInfo::getMachineGuid() {
    // Try to get machine GUID from registry
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &hKey);

    if (result != ERROR_SUCCESS) {
        return "";
    }

    char buffer[256];
    DWORD bufferSize = sizeof(buffer);
    result = RegQueryValueExA(hKey, "MachineGuid", nullptr, nullptr,
                              reinterpret_cast<LPBYTE>(buffer), &bufferSize);

    RegCloseKey(hKey);

    if (result == ERROR_SUCCESS) {
        return std::string(buffer);
    }

    return "";
}

std::string WindowsSystemInfo::getComputerName() {
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buffer);

    if (GetComputerNameA(buffer, &size)) {
        return std::string(buffer);
    }

    return "";
}

std::string WindowsSystemInfo::getDomainName() {
    return impl_->getWmiProperty(L"Win32_ComputerSystem", L"Domain");
}

std::vector<std::string> WindowsSystemInfo::getNetworkMacAddresses() {
    return impl_->getWmiPropertyMultiple(L"Win32_NetworkAdapter", L"MACAddress");
}

uint64_t WindowsSystemInfo::parseMemorySize(const std::string& sizeStr) {
    try {
        return std::stoull(sizeStr);
    } catch (...) {
        return 0;
    }
}

uint32_t WindowsSystemInfo::parseMemorySpeed(const std::string& speedStr) {
    try {
        return static_cast<uint32_t>(std::stoul(speedStr));
    } catch (...) {
        return 0;
    }
}

SystemInfoResult<std::vector<NetworkInterfaceInfo>> WindowsSystemInfo::getNetworkInterfaces() {
    SystemInfoResult<std::vector<NetworkInterfaceInfo>> result;

    try {
        if (!impl_->initializeWmi()) {
            result.success = false;
            result.error = SystemInfoError::ACCESS_DENIED;
            result.errorMessage = "Failed to initialize WMI";
            return result;
        }

        std::vector<std::wstring> properties = {
            L"Name", L"MACAddress", L"AdapterType", L"Manufacturer",
            L"ServiceName", L"NetEnabled"
        };

        auto networkObjects = impl_->getWmiObjectsProperties(L"Win32_NetworkAdapter", properties);

        for (const auto& netObj : networkObjects) {
            NetworkInterfaceInfo interface;

            auto it = netObj.find("Name");
            if (it != netObj.end()) interface.name = it->second;

            it = netObj.find("MACAddress");
            if (it != netObj.end()) interface.macAddress = it->second;

            it = netObj.find("AdapterType");
            if (it != netObj.end()) interface.type = it->second;

            it = netObj.find("Manufacturer");
            if (it != netObj.end()) interface.manufacturer = it->second;

            it = netObj.find("ServiceName");
            if (it != netObj.end()) interface.driver = it->second;

            it = netObj.find("NetEnabled");
            if (it != netObj.end()) interface.isActive = (it->second == "True");

            if (interface.isValid()) {
                result.data.push_back(interface);
            }
        }

        result.success = !result.data.empty();

        if (!result.success) {
            result.error = SystemInfoError::HARDWARE_NOT_FOUND;
            result.errorMessage = "No valid network interfaces found";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error = SystemInfoError::UNKNOWN_ERROR;
        result.errorMessage = e.what();
    }

    return result;
}

SystemInfoResult<ComprehensiveSystemInfo> WindowsSystemInfo::getComprehensiveInfo(const SystemInfoConfig& config) {
    SystemInfoResult<ComprehensiveSystemInfo> result;

    try {
        auto hardwareResult = getHardwareSerials();
        if (hardwareResult.success) {
            result.data.hardwareSerials = hardwareResult.data;
        }

        auto systemIdResult = getSystemIdentification();
        if (systemIdResult.success) {
            result.data.systemId = systemIdResult.data;
        }

        if (config.includeMemoryModules) {
            auto memoryResult = getMemoryModules();
            if (memoryResult.success) {
                result.data.memoryModules = memoryResult.data;
            }
        }

        if (config.includeNetworkInterfaces) {
            auto networkResult = getNetworkInterfaces();
            if (networkResult.success) {
                result.data.networkInterfaces = networkResult.data;
            }
        }

        result.data.lastUpdate = std::chrono::system_clock::now();
        result.success = result.data.isValid();

        if (!result.success) {
            result.error = SystemInfoError::HARDWARE_NOT_FOUND;
            result.errorMessage = "No valid system information found";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error = SystemInfoError::UNKNOWN_ERROR;
        result.errorMessage = e.what();
    }

    return result;
}

SystemInfoResult<SystemIdQuery> WindowsSystemInfo::querySystemId(SystemIdType type) {
    SystemInfoResult<SystemIdQuery> result;
    result.data.type = type;
    result.data.timestamp = std::chrono::system_clock::now();

    try {
        switch (type) {
            case SystemIdType::BIOS_SERIAL:
                result.data.value = validateSerial(impl_->getWmiProperty(L"Win32_BIOS", L"SerialNumber"));
                break;
            case SystemIdType::MOTHERBOARD_SERIAL:
                result.data.value = validateSerial(impl_->getWmiProperty(L"Win32_BaseBoard", L"SerialNumber"));
                break;
            case SystemIdType::CPU_SERIAL:
                result.data.value = validateSerial(impl_->getWmiProperty(L"Win32_Processor", L"ProcessorId"));
                break;
            case SystemIdType::SYSTEM_UUID:
                result.data.value = getSystemUuid();
                break;
            case SystemIdType::MACHINE_ID:
                result.data.value = getMachineGuid();
                break;
            case SystemIdType::MAC_ADDRESS: {
                auto macs = getNetworkMacAddresses();
                if (!macs.empty()) {
                    result.data.value = macs[0];
                }
                break;
            }
            case SystemIdType::DISK_SERIAL: {
                auto disks = impl_->getWmiPropertyMultiple(L"Win32_DiskDrive", L"SerialNumber");
                if (!disks.empty()) {
                    result.data.value = validateSerial(disks[0]);
                }
                break;
            }
            case SystemIdType::MEMORY_SERIAL: {
                auto memoryResult = getMemoryModules();
                if (memoryResult.success && !memoryResult.data.empty() &&
                    !memoryResult.data[0].serialNumber.empty()) {
                    result.data.value = memoryResult.data[0].serialNumber;
                }
                break;
            }
        }

        result.data.isAvailable = !result.data.value.empty();
        result.success = true;

        if (!result.data.isAvailable) {
            result.data.errorMessage = "System identifier not available";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error = SystemInfoError::UNKNOWN_ERROR;
        result.errorMessage = e.what();
        result.data.errorMessage = e.what();
    }

    return result;
}

std::unique_ptr<WindowsSystemInfo> createWindowsSystemInfo() {
    return std::make_unique<WindowsSystemInfo>();
}

} // namespace atom::system

#endif // _WIN32
