#include "bios.hpp"

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <winbase.h>
#include <comdef.h>
#include <sysinfoapi.h> 
#include <wbemidl.h>
// clang-format on
#if defined(_MSC_VER)
#pragma comment(lib, "wbemuuid.lib")
#endif

// 添加 RAII 风格的 COM 初始化器
namespace {
class ComInitializer {
public:
    ComInitializer(COINIT coinit = COINIT_MULTITHREADED) : initialized_(false) {
        HRESULT hr = CoInitializeEx(nullptr, coinit);
        if (SUCCEEDED(hr) || hr == S_FALSE) {
            initialized_ = true;
        } else {
            throw std::runtime_error("Failed to initialize COM library");
        }
    }

    ~ComInitializer() {
        if (initialized_) {
            CoUninitialize();
        }
    }

    // 禁止拷贝和移动
    ComInitializer(const ComInitializer&) = delete;
    ComInitializer& operator=(const ComInitializer&) = delete;

    // 释放所有权
    void release() { initialized_ = false; }

private:
    bool initialized_;
};

// COM 接口的智能指针模板
template <typename T>
class ComPtr {
public:
    ComPtr() : ptr_(nullptr) {}

    ComPtr(T* ptr) : ptr_(ptr) {}

    ~ComPtr() {
        if (ptr_) {
            ptr_->Release();
        }
    }

    // 移动构造和赋值
    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }

    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            if (ptr_) {
                ptr_->Release();
            }
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    // 禁止拷贝
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    // 访问器
    T* get() const { return ptr_; }
    T** getAddressOf() { return &ptr_; }

    // 操作符重载
    T* operator->() const { return ptr_; }

    // 释放所有权
    T* release() {
        T* tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }

private:
    T* ptr_;
};
}  // namespace
#endif

#include <chrono>
#include <fstream>
#include <memory>
#include <sstream>
#include <unordered_map>

#include "atom/log/loguru.hpp"

namespace atom::system {

bool BiosInfoData::isValid() const {
    return !version.empty() && !manufacturer.empty() && !releaseDate.empty();
}

std::string BiosInfoData::toString() const {
    std::stringstream ss;
    ss << "BIOS Information:\n"
       << "Version: " << version << "\n"
       << "Manufacturer: " << manufacturer << "\n"
       << "Release Date: " << releaseDate << "\n"
       << "Serial Number: " << serialNumber << "\n"
       << "Characteristics: " << characteristics << "\n"
       << "Upgradeable: " << (isUpgradeable ? "Yes" : "No");
    return ss.str();
}

BiosInfo& BiosInfo::getInstance() {
    static BiosInfo instance;
    return instance;
}

const BiosInfoData& BiosInfo::getBiosInfo(bool forceUpdate) {
    auto now = std::chrono::system_clock::now();
    if (forceUpdate || (now - cacheTime) > CACHE_DURATION) {
        refreshBiosInfo();
    }
    return cachedInfo;
}

bool BiosInfo::refreshBiosInfo() {
    try {
        cachedInfo = fetchBiosInfo();
        cacheTime = std::chrono::system_clock::now();
        return cachedInfo.isValid();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to refresh BIOS info: {}", e.what());
        return false;
    }
}

BiosHealthStatus BiosInfo::checkHealth() const {
    BiosHealthStatus status;
    status.isHealthy = true;
    status.lastCheckTime =
        std::chrono::system_clock::now().time_since_epoch().count();

#ifdef _WIN32
    try {
        // Check BIOS health status using WMI
        IWbemLocator* pLoc = nullptr;
        IWbemServices* pSvc = nullptr;

        // Initialize COM
        HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hres)) {
            throw std::runtime_error("Failed to initialize COM library");
        }

        // Set COM security levels
        hres = CoInitializeSecurity(
            nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

        if (FAILED(hres)) {
            CoUninitialize();
            throw std::runtime_error("Failed to initialize security");
        }

        // Obtain the initial locator to WMI
        hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                                IID_IWbemLocator, (LPVOID*)&pLoc);

        if (FAILED(hres)) {
            CoUninitialize();
            throw std::runtime_error("Failed to create IWbemLocator object");
        }

        // Connect to WMI
        hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0,
                                   0, 0, 0, &pSvc);
        if (FAILED(hres)) {
            pLoc->Release();
            CoUninitialize();
            throw std::runtime_error("Could not connect to WMI namespace");
        }

        // Set security levels on the proxy
        hres =
            CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                              nullptr, RPC_C_AUTHN_LEVEL_CALL,
                              RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        if (FAILED(hres)) {
            pSvc->Release();
            pLoc->Release();
            CoUninitialize();
            throw std::runtime_error("Could not set proxy blanket");
        }

        // Query system event log for BIOS errors
        IEnumWbemClassObject* pEnumerator = nullptr;
        hres = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_NTLogEvent WHERE LogFile='System' AND "
                   "EventCode='7' AND SourceName='Microsoft-Windows-BIOS' AND "
                   "TimeWritten > '20230101000000.000000-000'"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
            &pEnumerator);

        if (FAILED(hres)) {
            pSvc->Release();
            pLoc->Release();
            CoUninitialize();
            throw std::runtime_error("WMI query failed");
        }

        // Process results
        IWbemClassObject* pclsObj = nullptr;
        ULONG uReturn = 0;

        while (pEnumerator) {
            pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

            if (uReturn == 0)
                break;

            VARIANT vtProp;
            VariantInit(&vtProp);

            if (SUCCEEDED(pclsObj->Get(L"Message", 0, &vtProp, 0, 0))) {
                status.isHealthy = false;
                status.errors.push_back(
                    _com_util::ConvertBSTRToString(vtProp.bstrVal));
            }

            VariantClear(&vtProp);
            pclsObj->Release();
        }

        // Clean up
        pEnumerator->Release();
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();

        // Check BIOS age
        auto currentTime = std::chrono::system_clock::now();
        // Parse releaseDate from BIOS info
        std::tm tm = {};
        std::istringstream ss(cachedInfo.releaseDate);
        ss >> std::get_time(&tm, "%Y%m%d%H%M%S");
        auto biosTime =
            std::chrono::system_clock::from_time_t(std::mktime(&tm));

        auto biosAge = std::chrono::duration_cast<std::chrono::days>(
                           currentTime - biosTime)
                           .count();
        status.biosAgeInDays = static_cast<int>(biosAge);

        // Flag if BIOS is older than 2 years
        if (biosAge > 730) {  // ~2 years
            status.warnings.push_back(
                "BIOS is over 2 years old. Consider checking for updates.");
        }

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to check BIOS health: {}", e.what());
        status.isHealthy = false;
        status.errors.push_back(e.what());
    }
#elif __linux__
    try {
        // Check BIOS health on Linux using dmidecode and system logs
        std::vector<std::string> checkItems = {
            "sudo dmidecode -t 0",          // BIOS information
            "sudo dmidecode -t memory",     // Memory errors
            "sudo dmidecode -t processor",  // CPU errors
            "sudo dmidecode -t system"      // System errors
        };

        for (const auto& cmd : checkItems) {
            std::array<char, 4096> buffer;
            std::string result;
            struct PCloseDeleter {
                void operator()(FILE* fp) const { pclose(fp); }
            };
            std::unique_ptr<FILE, PCloseDeleter> pipe(popen(cmd.c_str(), "r"));

            if (!pipe) {
                throw std::runtime_error("popen() failed for command: " + cmd);
            }

            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }

            // Check for error indicators in the output
            if (result.find("Error") != std::string::npos ||
                result.find("Failure") != std::string::npos ||
                result.find("Critical") != std::string::npos) {
                status.isHealthy = false;
                status.errors.push_back("Issue detected in " + cmd + ": " +
                                        result.substr(0, 100) + "...");
            }
        }

        // Check system journal for BIOS-related errors
        {
            std::array<char, 4096> buffer;
            std::string result;
            std::unique_ptr<FILE, int (*)(FILE*)> pipe(
                popen("journalctl -b | grep -i 'bios\\|firmware\\|uefi' | grep "
                      "-i 'error\\|fail\\|warning'",
                      "r"),
                pclose);

            if (pipe) {
                while (fgets(buffer.data(), buffer.size(), pipe.get()) !=
                       nullptr) {
                    result += buffer.data();
                }

                if (!result.empty()) {
                    status.warnings.push_back(
                        "BIOS-related warnings in system logs");

                    std::istringstream iss(result);
                    std::string line;
                    int count = 0;

                    while (std::getline(iss, line) && count < 5) {
                        count++;
                    }
                }
            }
        }

        // Check BIOS age
        if (!cachedInfo.releaseDate.empty()) {
            std::tm tm = {};
            std::istringstream ss(cachedInfo.releaseDate);
            ss >> std::get_time(&tm, "%m/%d/%Y");
            auto biosTime =
                std::chrono::system_clock::from_time_t(std::mktime(&tm));
            auto currentTime = std::chrono::system_clock::now();

            auto biosAge = std::chrono::duration_cast<std::chrono::hours>(
                               currentTime - biosTime)
                               .count() /
                           24;

            status.biosAgeInDays = static_cast<int>(biosAge);

            // Flag if BIOS is older than 2 years
            if (biosAge > 730) {  // ~2 years
                status.warnings.push_back(
                    "BIOS is over 2 years old. Consider checking for updates.");
            }
        }

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to check BIOS health: {}", e.what());
        status.isHealthy = false;
        status.errors.push_back(e.what());
    }
#endif

    return status;
}

BiosUpdateInfo BiosInfo::checkForUpdates() const {
    BiosUpdateInfo updateInfo;
    updateInfo.currentVersion = cachedInfo.version;
    updateInfo.updateAvailable = false;

    try {
        std::string manufacturerUrl = getManufacturerUpdateUrl();
        // ... 实现从制造商网站检查更新的逻辑 ...
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to check for BIOS updates: {}", e.what());
    }

    return updateInfo;
}

std::vector<std::string> BiosInfo::getSMBIOSData() const {
    std::vector<std::string> smbiosData;

#ifdef _WIN32
    try {
        // 使用Windows Management Instrumentation (WMI)获取SMBIOS数据
        // ... Windows实现代码 ...
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to get SMBIOS data: {}", e.what());
    }
#elif __linux__
    try {
        std::string cmd = "sudo dmidecode";
        // ... Linux实现代码 ...
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to get SMBIOS data: {}", e.what());
    }
#endif

    return smbiosData;
}
bool BiosInfo::setSecureBoot(bool enable) {
    if (!isSecureBootSupported()) {
        LOG_F(ERROR, "Secure Boot is not supported on this system");
        return false;
    }

    try {
#ifdef _WIN32
        // Windows implementation using WMI and UEFI variables
        IWbemLocator* pLoc = nullptr;
        IWbemServices* pSvc = nullptr;

        // Initialize COM
        HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hres)) {
            throw std::runtime_error("Failed to initialize COM library");
        }

        // COM cleanup RAII helper
        struct COM_Cleanup {
            IWbemLocator* loc;
            IWbemServices* svc;
            COM_Cleanup(IWbemLocator* l, IWbemServices* s) : loc(l), svc(s) {}
            ~COM_Cleanup() {
                if (svc)
                    svc->Release();
                if (loc)
                    loc->Release();
                CoUninitialize();
            }
        };

        // Set COM security levels
        hres = CoInitializeSecurity(
            nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
        if (FAILED(hres)) {
            CoUninitialize();
            throw std::runtime_error("Failed to initialize security");
        }

        // Obtain WMI locator
        hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                                IID_IWbemLocator, (LPVOID*)&pLoc);
        if (FAILED(hres)) {
            CoUninitialize();
            throw std::runtime_error("Failed to create IWbemLocator object");
        }

        // Connect to WMI
        hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, 0,
                                   0, 0, 0, &pSvc);
        if (FAILED(hres)) {
            pLoc->Release();
            CoUninitialize();
            throw std::runtime_error("Could not connect to WMI namespace");
        }

        COM_Cleanup cleanup(pLoc, pSvc);

        // Set security levels on the proxy
        hres =
            CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                              nullptr, RPC_C_AUTHN_LEVEL_CALL,
                              RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        if (FAILED(hres)) {
            throw std::runtime_error("Could not set proxy blanket");
        }

        // Execute method to modify UEFI variable
        LOG_F(INFO, "Attempting to {} Secure Boot via UEFI variables",
              enable ? "enable" : "disable");

        // Note: This operation typically requires administrator privileges
        // and usually needs a reboot to take effect
        LOG_F(WARNING,
              "System will need to be restarted for changes to take effect");

        // This is just a placeholder - actual implementation would require
        // creating or modifying a scheduled task that runs at boot to
        // use the SetFirmwareEnvironmentVariable API with administrator
        // privileges
        return false;  // Direct modification not possible, return false
#elif __linux__
        // Linux implementation using efibootmgr and efivarfs
        LOG_F(INFO, "Attempting to {} Secure Boot",
              enable ? "enable" : "disable");

        // Check if we have root privileges
        if (geteuid() != 0) {
            LOG_F(ERROR,
                  "Root privileges required to modify Secure Boot settings");
            return false;
        }

        // Check if efivarfs is mounted
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, int (*)(FILE*)> mountCheck(
            popen("mount | grep efivarfs", "r"), pclose);

        if (!mountCheck ||
            !fgets(buffer.data(), buffer.size(), mountCheck.get())) {
            LOG_F(ERROR, "EFI variables filesystem not available");
            return false;
        }

        // Path to SecureBoot EFI variable
        const std::string secureBootVar =
            "/sys/firmware/efi/efivars/"
            "SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c";

        // Create backup of current value
        std::string backupCmd = "cp " + secureBootVar + " /tmp/SecureBoot.bak";
        if (std::system(backupCmd.c_str()) != 0) {
            LOG_F(ERROR, "Failed to backup current Secure Boot state");
            return false;
        }

        // Prepare new value (this is simplified - actual implementation would
        // need to correctly format the EFI variable including its attributes)
        std::string newValue = enable ? "01" : "00";

        LOG_F(WARNING,
              "System will need to be restarted for changes to take effect");
        LOG_F(ERROR,
              "Direct modification of Secure Boot state is restricted for "
              "security reasons");

        // In practice, this would require more complex interaction with the
        // system's UEFI firmware, potentially through a custom bootloader or
        // direct firmware access
        return false;  // Direct modification not possible, return false
#else
        LOG_F(ERROR, "Secure Boot modification not supported on this platform");
        return false;
#endif
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to set Secure Boot: {}", e.what());
        return false;
    }
}

bool BiosInfo::isSecureBootSupported() {
    try {
#ifdef _WIN32
        // Windows平台检测安全引导是否支持
        // 使用 GetFirmwareEnvironmentVariable API 检查 UEFI 安全引导变量
        DWORD size = 0;
        BYTE buffer[1] = {0};

        // 首先需要检查是否拥有足够权限
        // 尝试读取 SecureBoot 变量的值来确认支持
        BOOL result = GetFirmwareEnvironmentVariableA(
            "SecureBoot", "{8be4df61-93ca-11d2-aa0d-00e098032b8c}", buffer,
            size);

        // 如果函数失败但错误为
        // ERROR_INSUFFICIENT_BUFFER，则变量存在（需要更多空间） 这表明系统支持
        // SecureBoot
        DWORD error = GetLastError();
        if (error == ERROR_INSUFFICIENT_BUFFER || result) {
            return true;
        }

        // 其它错误可能表示无法访问或不支持
        LOG_F(INFO, "SecureBoot check failed with error code: {}", error);
        return false;
#elif __linux__
        // Linux平台检测安全引导是否支持
        // 检查 efivarfs 是否挂载且存在 SecureBoot 变量

        // 首先检查是否有 efivarfs 文件系统
        std::ifstream efi_dir("/sys/firmware/efi");
        if (!efi_dir.good()) {
            LOG_F(
                INFO,
                "EFI variables directory not found, SecureBoot not supported");
            return false;
        }

        // 检查 SecureBoot 变量是否存在
        std::ifstream secure_boot_var(
            "/sys/firmware/efi/efivars/"
            "SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c");
        if (secure_boot_var.good()) {
            LOG_F(INFO, "SecureBoot variable found, SecureBoot is supported");
            return true;
        }

        // 尝试使用 efibootmgr 命令检查
        if (access("/usr/bin/efibootmgr", X_OK) == 0) {
            std::array<char, 128> buffer;
            std::string result;
            std::unique_ptr<FILE, int (*)(FILE*)> pipe(
                popen("efibootmgr -v | grep -i secureboot", "r"), pclose);

            if (pipe) {
                while (fgets(buffer.data(), buffer.size(), pipe.get()) !=
                       nullptr) {
                    result += buffer.data();
                }

                if (!result.empty()) {
                    LOG_F(INFO, "SecureBoot found via efibootmgr: {}", result);
                    return true;
                }
            }
        }

        LOG_F(INFO, "No evidence of SecureBoot support found");
        return false;
#else
        // 其他平台不支持
        LOG_F(INFO, "SecureBoot check not implemented for this platform");
        return false;
#endif
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to check Secure Boot support: {}", e.what());
        return false;
    }
}

bool BiosInfo::backupBiosSettings(const std::string& filepath) {
    try {
        std::ofstream out(filepath, std::ios::binary);
        if (!out) {
            throw std::runtime_error("Cannot open file for writing");
        }

        // 实现BIOS设置备份逻辑
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to backup BIOS settings: {}", e.what());
        return false;
    }
}

std::string BiosInfo::getManufacturerUpdateUrl() const {
    // 根据不同制造商返回相应的更新检查URL
    static const std::unordered_map<std::string, std::string> manufacturerUrls =
        {
            {"Dell Inc.",
             "https://www.dell.com/support/driver/home/index.html"},
            {"LENOVO", "https://pcsupport.lenovo.com/"},
            {"HP", "https://support.hp.com/drivers"},
            // ... 添加更多制造商 ...
        };

    auto it = manufacturerUrls.find(cachedInfo.manufacturer);
    return (it != manufacturerUrls.end()) ? it->second : "";
}

#ifdef _WIN32
BiosInfoData BiosInfo::fetchBiosInfo() {
    LOG_F(INFO, "Fetching BIOS information");
    BiosInfoData biosInfo;

    // 使用智能指针管理COM对象
    struct COM_Deleter {
        void operator()(void*) { CoUninitialize(); }
    };
    std::unique_ptr<void, COM_Deleter> com_init(nullptr, COM_Deleter());

    HRESULT hresult;

    // Initialize COM library
    hresult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hresult)) {
        LOG_F(ERROR, "Failed to initialize COM library. Error code = 0x{:x}",
              hresult);
        return biosInfo;
    }

    // Set COM security
    hresult = CoInitializeSecurity(
        nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

    if (FAILED(hresult)) {
        LOG_F(ERROR, "Failed to initialize security. Error code = 0x{:x}",
              hresult);
        return biosInfo;
    }

    // Obtain the initial locator to WMI
    IWbemLocator* pLoc = nullptr;
    hresult = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                               IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hresult)) {
        LOG_F(ERROR,
              "Failed to create IWbemLocator object. Error code = 0x{:x}",
              hresult);
        return biosInfo;
    }

    // Connect to WMI namespace
    IWbemServices* pSvc = nullptr;
    hresult = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0,
                                  0, 0, 0, &pSvc);
    if (FAILED(hresult)) {
        LOG_F(ERROR, "Could not connect to WMI namespace. Error code = 0x{:x}",
              hresult);
        pLoc->Release();
        return biosInfo;
    }

    // Set security levels on the proxy
    hresult =
        CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                          RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                          nullptr, EOAC_NONE);

    if (FAILED(hresult)) {
        LOG_F(ERROR, "Could not set proxy blanket. Error code = 0x{:x}",
              hresult);
        pSvc->Release();
        pLoc->Release();
        return biosInfo;
    }

    // 扩展WQL查询以获取更多信息
    const wchar_t* query = L"SELECT * FROM Win32_BIOS";

    IEnumWbemClassObject* pEnumerator = nullptr;
    hresult =
        pSvc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(query),
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                        nullptr, &pEnumerator);

    if (FAILED(hresult)) {
        LOG_F(ERROR, "Query for BIOS information failed. Error code = 0x{:x}",
              hresult);
        pSvc->Release();
        pLoc->Release();
        return biosInfo;
    }

    IWbemClassObject* pclsObj = nullptr;
    ULONG uReturn = 0;

    while (pEnumerator != nullptr) {
        pEnumerator->Next(WBEM_NO_WAIT, 1, &pclsObj, &uReturn);
        if (0 == uReturn) {
            break;
        }

        auto getBiosProperty = [&](const wchar_t* prop) -> std::string {
            VARIANT vtProp;
            VariantInit(&vtProp);
            if (SUCCEEDED(pclsObj->Get(prop, 0, &vtProp, nullptr, nullptr))) {
                std::string result =
                    (vtProp.vt == VT_BSTR)
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
        biosInfo.isUpgradeable =
            getBiosProperty(L"BIOSVersion").find("Upgradeable") !=
            std::string::npos;

        pclsObj->Release();
    }

    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();

    return biosInfo;
}

#elif __linux__
BiosInfoData BiosInfo::fetchBiosInfo() {
    LOG_F(INFO, "Fetching BIOS information");
    BiosInfoData biosInfo;

    try {
        std::array<std::string, 2> commands = {"sudo dmidecode -t bios",
                                               "sudo dmidecode -t system"};

        for (const auto& cmd : commands) {
            std::array<char, 128> buffer;
            std::string result;
            std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(cmd.c_str(), "r"),
                                                       pclose);

            if (!pipe) {
                LOG_F(ERROR, "popen() failed!");
                throw std::runtime_error("popen() failed!");
            }

            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }

            std::istringstream iss(result);
            std::string line;

            while (std::getline(iss, line)) {
                if (line.find("Version:") != std::string::npos) {
                    biosInfo.version = line.substr(line.find(":") + 2);
                } else if (line.find("Vendor:") != std::string::npos) {
                    biosInfo.manufacturer = line.substr(line.find(":") + 2);
                } else if (line.find("Release Date:") != std::string::npos) {
                    biosInfo.releaseDate = line.substr(line.find(":") + 2);
                } else if (line.find("Serial Number:") != std::string::npos) {
                    biosInfo.serialNumber = line.substr(line.find(":") + 2);
                } else if (line.find("Characteristics:") != std::string::npos) {
                    biosInfo.characteristics = line.substr(line.find(":") + 2);
                } else if (line.find("BIOS is upgradeable") !=
                           std::string::npos) {
                    biosInfo.isUpgradeable = true;
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error fetching BIOS info: {}", e.what());
        throw;
    }

    return biosInfo;
}
#endif

bool BiosInfo::isUEFIBootSupported() {
    // Add implementation for checking UEFI boot support
    // This is a placeholder and needs actual OS-specific implementation
#ifdef _WIN32
    // Windows: Check if system is booted in UEFI mode
    // Using WMI to query SecureBoot variable as a proxy for UEFI
    try {
        IWbemLocator* pLoc = nullptr;
        IWbemServices* pSvc = nullptr;
        HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hres))
            return false;

        struct CleanupCOM {
            ~CleanupCOM() { CoUninitialize(); }
        } cleanup;

        hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                                IID_IWbemLocator, (LPVOID*)&pLoc);
        if (FAILED(hres) || !pLoc)
            return false;

        hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, 0,
                                   0, 0, 0, &pSvc);
        if (FAILED(hres) || !pSvc) {
            pLoc->Release();
            return false;
        }

        // Look for UEFI variables
        IEnumWbemClassObject* pEnumerator = nullptr;
        hres = pSvc->ExecQuery(
            bstr_t("WQL"), bstr_t("SELECT * FROM MSFirmwareUefiInfo"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
            &pEnumerator);

        bool isUEFI = SUCCEEDED(hres) && pEnumerator;

        if (pEnumerator)
            pEnumerator->Release();
        pSvc->Release();
        pLoc->Release();

        return isUEFI;
    } catch (...) {
        return false;
    }
#elif __linux__
    // Linux: Check for /sys/firmware/efi or efibootmgr
    std::ifstream efi_dir("/sys/firmware/efi");
    if (efi_dir.good()) {
        return true;
    }
    // Fallback to checking efibootmgr command availability
    std::unique_ptr<FILE, int (*)(FILE*)> pipe(
        popen("command -v efibootmgr", "r"), pclose);
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            return true;  // Command exists
        }
    }
    return false;
#else
    return false;  // Not supported on other platforms
#endif
}

bool BiosInfo::restoreBiosSettings(const std::string& filepath) {
    try {
        std::ifstream in(filepath, std::ios::binary);
        if (!in) {
            LOG_F(ERROR, "Failed to open BIOS settings backup file: {}",
                  filepath);
            return false;
        }

        // Placeholder for actual BIOS settings restoration logic
        // This would involve complex, hardware-specific operations
        // and potentially require direct hardware access or OS APIs
        // For example, writing to CMOS or UEFI variables.
        // This is highly dependent on the system and BIOS vendor.
        LOG_F(INFO, "Placeholder: Restoring BIOS settings from {}", filepath);
        // Simulate reading the file content
        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        if (content.empty()) {
            LOG_F(WARNING, "BIOS settings backup file is empty: {}", filepath);
            // Depending on requirements, this might be an error or just a
            // warning
        }
        // Actual restoration logic would go here.

        LOG_F(INFO, "BIOS settings restoration from {} (simulated) successful.",
              filepath);
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to restore BIOS settings: {}", e.what());
        return false;
    }
}

bool BiosInfo::setUEFIBoot(bool enable) {
    if (!isUEFIBootSupported()) {
        LOG_F(ERROR, "UEFI Boot is not supported on this system");
        return false;
    }

    try {
#ifdef _WIN32
        // Windows实现 - 需要管理员权限才能修改UEFI引导设置
        LOG_F(INFO, "Attempting to {} UEFI Boot mode",
              enable ? "enable" : "disable");

        // 检查当前权限
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

        if (!isElevated) {
            LOG_F(ERROR,
                  "Administrator privileges required to modify UEFI boot "
                  "settings");
            return false;
        }

        // 这里需要通过BCDEdit或类似工具来修改引导配置
        // 由于这是系统级别的操作，通常需要创建一个管理员任务
        // 下面是使用bcdedit的示例（需要以管理员身份运行）

        // 构建命令
        std::string command = "bcdedit /set {bootmgr} path \\EFI\\";
        command += enable ? "Microsoft\\Boot\\bootmgfw.efi"
                          : "Legacy\\Boot\\bootmgfw.efi";

        LOG_F(INFO, "Executing command: {}", command);

        // 在实际产品中，应该使用更安全的API调用而非system
        // 这里仅作为示例
        int result = ::system(command.c_str());

        if (result != 0) {
            LOG_F(ERROR, "Failed to set UEFI boot mode, command returned: {}",
                  result);
            return false;
        }

        LOG_F(WARNING,
              "System will need to be restarted for changes to take effect");
        return true;
#elif __linux__
        // Linux实现 - 同样需要root权限
        LOG_F(INFO, "Attempting to {} UEFI Boot mode",
              enable ? "enable" : "disable");

        // 检查权限
        if (geteuid() != 0) {
            LOG_F(ERROR,
                  "Root privileges required to modify UEFI boot settings");
            return false;
        }

        // 检查efibootmgr是否可用
        if (access("/usr/bin/efibootmgr", X_OK) != 0) {
            LOG_F(ERROR,
                  "efibootmgr not found, cannot modify UEFI boot settings");
            return false;
        }

        // 构建命令
        std::string command;
        if (enable) {
            // 启用UEFI引导（禁用Legacy引导）
            command =
                "efibootmgr --create --disk /dev/sda --part 1 "
                "--loader \\\\EFI\\\\BOOT\\\\BOOTX64.EFI --label \"UEFI OS\" "
                "--quiet";
        } else {
            // 禁用UEFI引导（需要系统支持Legacy引导模式）
            // 首先查找UEFI引导项
            std::array<char, 256> buffer;
            std::string bootEntries;
            std::unique_ptr<FILE, int (*)(FILE*)> pipe(
                popen("efibootmgr | grep \"UEFI OS\"", "r"), pclose);

            if (pipe) {
                while (fgets(buffer.data(), buffer.size(), pipe.get()) !=
                       nullptr) {
                    bootEntries += buffer.data();
                }
            }

            // 解析引导项编号
            std::string bootNum;
            size_t pos = bootEntries.find("Boot");
            if (pos != std::string::npos && bootEntries.length() > pos + 8) {
                bootNum = bootEntries.substr(pos + 4, 4);
                command = "efibootmgr -b " + bootNum + " -B --quiet";
            } else {
                LOG_F(ERROR, "Could not find UEFI boot entry to disable");
                return false;
            }
        }

        LOG_F(INFO, "Executing command: {}", command);
        int result = ::system(command.c_str());

        if (result != 0) {
            LOG_F(ERROR, "Failed to set UEFI boot mode, command returned: {}",
                  result);
            return false;
        }

        LOG_F(WARNING,
              "System will need to be restarted for changes to take effect");
        return true;
#else
        // 其他平台不支持
        LOG_F(ERROR,
              "Setting UEFI boot mode is not supported on this platform");
        return false;
#endif
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to set UEFI boot mode: {}", e.what());
        return false;
    }
}

}  // namespace atom::system
