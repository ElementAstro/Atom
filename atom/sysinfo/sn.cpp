#include "sn.hpp"

#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <Wbemidl.h>
#include <comdef.h>
#pragma comment(lib, "wbemuuid.lib")

class HardwareInfo::Impl {
public:
    /**
     * @brief Get a single WMI property value
     * @param wmiClass WMI class name
     * @param property Property name to retrieve
     * @return Property value as string
     */
    static auto getWmiProperty(const std::wstring& wmiClass,
                               const std::wstring& property) -> std::string {
        spdlog::info("Retrieving WMI property: Class={}, Property={}",
                     std::string(wmiClass.begin(), wmiClass.end()),
                     std::string(property.begin(), property.end()));

        IWbemLocator* pLoc = nullptr;
        IWbemServices* pSvc = nullptr;
        IEnumWbemClassObject* pEnumerator = nullptr;
        IWbemClassObject* pclsObj = nullptr;
        ULONG uReturn = 0;
        std::string result;

        if (!initializeWmi(pLoc, pSvc)) {
            spdlog::error("Failed to initialize WMI");
            return "";
        }

        HRESULT hres = pSvc->ExecQuery(
            bstr_t("WQL"), bstr_t((L"SELECT * FROM " + wmiClass).c_str()),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
            &pEnumerator);

        if (FAILED(hres)) {
            spdlog::error("WMI query execution failed with HRESULT: 0x{:x}",
                          hres);
            cleanup(pLoc, pSvc, pEnumerator);
            return "";
        }

        while (pEnumerator) {
            HRESULT hr =
                pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (0 == uReturn) {
                break;
            }

            VARIANT vtProp;
            hr = pclsObj->Get(property.c_str(), 0, &vtProp, nullptr, nullptr);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR &&
                vtProp.bstrVal != nullptr) {
                result = _bstr_t(vtProp.bstrVal);
                spdlog::debug("Retrieved WMI property value: {}", result);
            }
            VariantClear(&vtProp);
            pclsObj->Release();
        }

        cleanup(pLoc, pSvc, pEnumerator);
        return result;
    }

    /**
     * @brief Get multiple WMI property values
     * @param wmiClass WMI class name
     * @param property Property name to retrieve
     * @return Vector of property values
     */
    static auto getWmiPropertyMultiple(const std::wstring& wmiClass,
                                       const std::wstring& property)
        -> std::vector<std::string> {
        spdlog::info(
            "Retrieving multiple WMI properties: Class={}, Property={}",
            std::string(wmiClass.begin(), wmiClass.end()),
            std::string(property.begin(), property.end()));

        IWbemLocator* pLoc = nullptr;
        IWbemServices* pSvc = nullptr;
        IEnumWbemClassObject* pEnumerator = nullptr;
        IWbemClassObject* pclsObj = nullptr;
        ULONG uReturn = 0;
        std::vector<std::string> results;

        if (!initializeWmi(pLoc, pSvc)) {
            spdlog::error("Failed to initialize WMI");
            return results;
        }

        HRESULT hres = pSvc->ExecQuery(
            bstr_t("WQL"), bstr_t((L"SELECT * FROM " + wmiClass).c_str()),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
            &pEnumerator);

        if (FAILED(hres)) {
            spdlog::error("WMI query execution failed with HRESULT: 0x{:x}",
                          hres);
            cleanup(pLoc, pSvc, pEnumerator);
            return results;
        }

        while (pEnumerator) {
            HRESULT hr =
                pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (0 == uReturn) {
                break;
            }

            VARIANT vtProp;
            hr = pclsObj->Get(property.c_str(), 0, &vtProp, nullptr, nullptr);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR &&
                vtProp.bstrVal != nullptr) {
                std::string value =
                    static_cast<const char*>(_bstr_t(vtProp.bstrVal));
                results.emplace_back(value);
                spdlog::debug("Retrieved WMI property value: {}", value);
            }
            VariantClear(&vtProp);
            pclsObj->Release();
        }

        cleanup(pLoc, pSvc, pEnumerator);
        return results;
    }

    /**
     * @brief Initialize WMI components
     * @param pLoc WMI locator pointer
     * @param pSvc WMI services pointer
     * @return true if initialization successful, false otherwise
     */
    static auto initializeWmi(IWbemLocator*& pLoc, IWbemServices*& pSvc)
        -> bool {
        spdlog::debug("Initializing WMI components");

        HRESULT hres = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hres)) {
            spdlog::error("Failed to initialize COM library. HRESULT: 0x{:x}",
                          hres);
            return false;
        }

        hres = CoInitializeSecurity(
            nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

        if (FAILED(hres)) {
            spdlog::error("Failed to initialize COM security. HRESULT: 0x{:x}",
                          hres);
            CoUninitialize();
            return false;
        }

        hres = CoCreateInstance(CLSID_WbemLocator, nullptr,
                                CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                                reinterpret_cast<LPVOID*>(&pLoc));

        if (FAILED(hres)) {
            spdlog::error(
                "Failed to create IWbemLocator object. HRESULT: 0x{:x}", hres);
            CoUninitialize();
            return false;
        }

        hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0,
                                   0, 0, 0, &pSvc);

        if (FAILED(hres)) {
            spdlog::error("Failed to connect to WMI namespace. HRESULT: 0x{:x}",
                          hres);
            pLoc->Release();
            CoUninitialize();
            return false;
        }

        hres =
            CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                              nullptr, RPC_C_AUTHN_LEVEL_CALL,
                              RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        if (FAILED(hres)) {
            spdlog::error("Failed to set proxy blanket. HRESULT: 0x{:x}", hres);
            pSvc->Release();
            pLoc->Release();
            CoUninitialize();
            return false;
        }

        spdlog::debug("WMI initialized successfully");
        return true;
    }

    /**
     * @brief Clean up WMI resources
     * @param pLoc WMI locator pointer
     * @param pSvc WMI services pointer
     * @param pEnumerator WMI enumerator pointer
     */
    static void cleanup(IWbemLocator* pLoc, IWbemServices* pSvc,
                        IEnumWbemClassObject* pEnumerator) {
        spdlog::debug("Cleaning up WMI resources");

        if (pEnumerator) {
            pEnumerator->Release();
        }
        if (pSvc) {
            pSvc->Release();
        }
        if (pLoc) {
            pLoc->Release();
        }
        CoUninitialize();
    }

    auto getBiosSerialNumber() const -> std::string {
        spdlog::info("Retrieving BIOS serial number");
        return getWmiProperty(L"Win32_BIOS", L"SerialNumber");
    }

    auto getMotherboardSerialNumber() const -> std::string {
        spdlog::info("Retrieving motherboard serial number");
        return getWmiProperty(L"Win32_BaseBoard", L"SerialNumber");
    }

    auto getCpuSerialNumber() const -> std::string {
        spdlog::info("Retrieving CPU serial number");
        return getWmiProperty(L"Win32_Processor", L"ProcessorId");
    }

    auto getDiskSerialNumbers() const -> std::vector<std::string> {
        spdlog::info("Retrieving disk serial numbers");
        return getWmiPropertyMultiple(L"Win32_DiskDrive", L"SerialNumber");
    }
};

#else
#include <filesystem>
#include <fstream>

class HardwareInfo::Impl {
public:
    /**
     * @brief Read content from a file
     * @param path File path to read
     * @param key Optional key to search for in the file
     * @return File content or value associated with key
     */
    auto readFile(const std::string& path, const std::string& key = "") const
        -> std::string {
        spdlog::debug("Reading file: {}", path);

        if (!std::filesystem::exists(path)) {
            spdlog::warn("File does not exist: {}", path);
            return "";
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open file: {}", path);
            return "";
        }

        std::string content;
        if (key.empty()) {
            std::getline(file, content);
            spdlog::debug("Read content from {}: {}", path, content);
        } else {
            std::string line;
            while (std::getline(file, line)) {
                if (line.find(key) != std::string::npos) {
                    auto pos = line.find(":");
                    if (pos != std::string::npos && pos + 2 < line.length()) {
                        content = line.substr(pos + 2);
                        spdlog::debug("Found key '{}' with value: {}", key,
                                      content);
                    }
                    break;
                }
            }
        }

        return content;
    }

    auto getBiosSerialNumber() const -> std::string {
        spdlog::info("Retrieving BIOS serial number");
        return readFile("/sys/class/dmi/id/product_serial");
    }

    auto getMotherboardSerialNumber() const -> std::string {
        spdlog::info("Retrieving motherboard serial number");
        return readFile("/sys/class/dmi/id/board_serial");
    }

    auto getCpuSerialNumber() const -> std::string {
        spdlog::info("Retrieving CPU serial number");
        return readFile("/proc/cpuinfo", "Serial");
    }

    auto getDiskSerialNumbers() const -> std::vector<std::string> {
        spdlog::info("Retrieving disk serial numbers");
        std::vector<std::string> serials;

        for (const auto& entry :
             std::filesystem::directory_iterator("/sys/block")) {
            if (entry.is_directory()) {
                std::string serialPath =
                    entry.path().string() + "/device/serial";
                std::string serial = readFile(serialPath);
                if (!serial.empty()) {
                    serials.push_back(serial);
                }
            }
        }

        return serials;
    }
};

#endif

HardwareInfo::HardwareInfo() : impl_(new Impl()) {
    spdlog::debug("HardwareInfo instance created");
}

HardwareInfo::~HardwareInfo() {
    spdlog::debug("HardwareInfo instance destroyed");
    delete impl_;
}

HardwareInfo::HardwareInfo(const HardwareInfo& other)
    : impl_(new Impl(*other.impl_)) {
    spdlog::debug("HardwareInfo copy constructor called");
}

HardwareInfo& HardwareInfo::operator=(const HardwareInfo& other) {
    if (this != &other) {
        delete impl_;
        impl_ = new Impl(*other.impl_);
        spdlog::debug("HardwareInfo copy assignment performed");
    }
    return *this;
}

HardwareInfo::HardwareInfo(HardwareInfo&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
    spdlog::debug("HardwareInfo move constructor called");
}

HardwareInfo& HardwareInfo::operator=(HardwareInfo&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
        spdlog::debug("HardwareInfo move assignment performed");
    }
    return *this;
}

auto HardwareInfo::getBiosSerialNumber() -> std::string {
    return impl_->getBiosSerialNumber();
}

auto HardwareInfo::getMotherboardSerialNumber() -> std::string {
    return impl_->getMotherboardSerialNumber();
}

auto HardwareInfo::getCpuSerialNumber() -> std::string {
    return impl_->getCpuSerialNumber();
}

auto HardwareInfo::getDiskSerialNumbers() -> std::vector<std::string> {
    return impl_->getDiskSerialNumbers();
}
