#include "bios.hpp"

#ifdef _WIN32
#include <comdef.h>
#include <sysinfoapi.h>
#include <wbemidl.h>
#include <winbase.h>
#include <windows.h>
#if defined(_MSC_VER)
#pragma comment(lib, "wbemuuid.lib")
#endif

namespace {
/**
 * @brief RAII COM initializer
 */
class ComInitializer {
public:
    explicit ComInitializer(COINIT coinit = COINIT_MULTITHREADED)
        : initialized_(false) {
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

    ComInitializer(const ComInitializer&) = delete;
    ComInitializer& operator=(const ComInitializer&) = delete;

    void release() { initialized_ = false; }

private:
    bool initialized_;
};

/**
 * @brief Smart pointer for COM interfaces
 */
template <typename T>
class ComPtr {
public:
    ComPtr() : ptr_(nullptr) {}
    explicit ComPtr(T* ptr) : ptr_(ptr) {}

    ~ComPtr() {
        if (ptr_) {
            ptr_->Release();
        }
    }

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

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    T* get() const { return ptr_; }
    T** getAddressOf() { return &ptr_; }
    T* operator->() const { return ptr_; }

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

#include <spdlog/spdlog.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <unordered_map>

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
        spdlog::error("Failed to refresh BIOS info: {}", e.what());
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

        hres =
            CoSetProxyBlanket(pSvc.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                              nullptr, RPC_C_AUTHN_LEVEL_CALL,
                              RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        if (FAILED(hres)) {
            throw std::runtime_error("Could not set proxy blanket");
        }

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
            }
        }

        auto currentTime = std::chrono::system_clock::now();
        std::tm tm = {};
        std::istringstream ss(cachedInfo.releaseDate);
        ss >> std::get_time(&tm, "%Y%m%d%H%M%S");
        auto biosTime =
            std::chrono::system_clock::from_time_t(std::mktime(&tm));

        auto biosAge = std::chrono::duration_cast<std::chrono::days>(
                           currentTime - biosTime)
                           .count();
        status.biosAgeInDays = static_cast<int>(biosAge);

        if (biosAge > 730) {
            status.warnings.push_back(
                "BIOS is over 2 years old. Consider checking for updates.");
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to check BIOS health: {}", e.what());
        status.isHealthy = false;
        status.errors.push_back(e.what());
    }
#elif __linux__
    try {
        std::vector<std::string> checkItems = {
            "sudo dmidecode -t 0", "sudo dmidecode -t memory",
            "sudo dmidecode -t processor", "sudo dmidecode -t system"};

        for (const auto& cmd : checkItems) {
            std::array<char, 4096> buffer;
            std::string result;
            std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(cmd.c_str(), "r"),
                                                       pclose);

            if (!pipe) {
                throw std::runtime_error("popen() failed for command: " + cmd);
            }

            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }

            if (result.find("Error") != std::string::npos ||
                result.find("Failure") != std::string::npos ||
                result.find("Critical") != std::string::npos) {
                status.isHealthy = false;
                status.errors.push_back("Issue detected in " + cmd + ": " +
                                        result.substr(0, 100) + "...");
            }
        }

        std::unique_ptr<FILE, int (*)(FILE*)> pipe(
            popen("journalctl -b | grep -i 'bios\\|firmware\\|uefi' | grep -i "
                  "'error\\|fail\\|warning'",
                  "r"),
            pclose);

        if (pipe) {
            std::array<char, 4096> buffer;
            std::string result;
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }

            if (!result.empty()) {
                status.warnings.push_back(
                    "BIOS-related warnings in system logs");
            }
        }

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

            if (biosAge > 730) {
                status.warnings.push_back(
                    "BIOS is over 2 years old. Consider checking for updates.");
            }
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to check BIOS health: {}", e.what());
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
    } catch (const std::exception& e) {
        spdlog::error("Failed to check for BIOS updates: {}", e.what());
    }

    return updateInfo;
}

std::vector<std::string> BiosInfo::getSMBIOSData() const {
    std::vector<std::string> smbiosData;

#ifdef _WIN32
    try {
        // Windows implementation using WMI
    } catch (const std::exception& e) {
        spdlog::error("Failed to get SMBIOS data: {}", e.what());
    }
#elif __linux__
    try {
        std::string cmd = "sudo dmidecode";
        // Linux implementation
    } catch (const std::exception& e) {
        spdlog::error("Failed to get SMBIOS data: {}", e.what());
    }
#endif

    return smbiosData;
}

bool BiosInfo::setSecureBoot(bool enable) {
    if (!isSecureBootSupported()) {
        spdlog::error("Secure Boot is not supported on this system");
        return false;
    }

    try {
#ifdef _WIN32
        spdlog::info("Attempting to {} Secure Boot via UEFI variables",
                     enable ? "enable" : "disable");
        spdlog::warn(
            "System will need to be restarted for changes to take effect");
        return false;
#elif __linux__
        spdlog::info("Attempting to {} Secure Boot",
                     enable ? "enable" : "disable");

        if (geteuid() != 0) {
            spdlog::error(
                "Root privileges required to modify Secure Boot settings");
            return false;
        }

        std::array<char, 128> buffer;
        std::unique_ptr<FILE, int (*)(FILE*)> mountCheck(
            popen("mount | grep efivarfs", "r"), pclose);

        if (!mountCheck ||
            !fgets(buffer.data(), buffer.size(), mountCheck.get())) {
            spdlog::error("EFI variables filesystem not available");
            return false;
        }

        const std::string secureBootVar =
            "/sys/firmware/efi/efivars/"
            "SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c";
        std::string backupCmd = "cp " + secureBootVar + " /tmp/SecureBoot.bak";

        if (std::system(backupCmd.c_str()) != 0) {
            spdlog::error("Failed to backup current Secure Boot state");
            return false;
        }

        spdlog::warn(
            "System will need to be restarted for changes to take effect");
        spdlog::error(
            "Direct modification of Secure Boot state is restricted for "
            "security reasons");
        return false;
#else
        spdlog::error(
            "Secure Boot modification not supported on this platform");
        return false;
#endif
    } catch (const std::exception& e) {
        spdlog::error("Failed to set Secure Boot: {}", e.what());
        return false;
    }
}

bool BiosInfo::isSecureBootSupported() {
    try {
#ifdef _WIN32
        DWORD size = 0;
        BYTE buffer[1] = {0};

        BOOL result = GetFirmwareEnvironmentVariableA(
            "SecureBoot", "{8be4df61-93ca-11d2-aa0d-00e098032b8c}", buffer,
            size);

        DWORD error = GetLastError();
        if (error == ERROR_INSUFFICIENT_BUFFER || result) {
            return true;
        }

        spdlog::info("SecureBoot check failed with error code: {}", error);
        return false;
#elif __linux__
        std::ifstream efi_dir("/sys/firmware/efi");
        if (!efi_dir.good()) {
            spdlog::info(
                "EFI variables directory not found, SecureBoot not supported");
            return false;
        }

        std::ifstream secure_boot_var(
            "/sys/firmware/efi/efivars/"
            "SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c");
        if (secure_boot_var.good()) {
            spdlog::info("SecureBoot variable found, SecureBoot is supported");
            return true;
        }

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
                    spdlog::info("SecureBoot found via efibootmgr: {}", result);
                    return true;
                }
            }
        }

        spdlog::info("No evidence of SecureBoot support found");
        return false;
#else
        spdlog::info("SecureBoot check not implemented for this platform");
        return false;
#endif
    } catch (const std::exception& e) {
        spdlog::error("Failed to check Secure Boot support: {}", e.what());
        return false;
    }
}

bool BiosInfo::backupBiosSettings(const std::string& filepath) {
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

std::string BiosInfo::getManufacturerUpdateUrl() const {
    static const std::unordered_map<std::string, std::string> manufacturerUrls =
        {
            {"Dell Inc.",
             "https://www.dell.com/support/driver/home/index.html"},
            {"LENOVO", "https://pcsupport.lenovo.com/"},
            {"HP", "https://support.hp.com/drivers"},
        };

    auto it = manufacturerUrls.find(cachedInfo.manufacturer);
    return (it != manufacturerUrls.end()) ? it->second : "";
}

#ifdef _WIN32
BiosInfoData BiosInfo::fetchBiosInfo() {
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

        hres =
            CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                             IID_IWbemLocator, (LPVOID*)pLoc.getAddressOf());
        if (FAILED(hres)) {
            throw std::runtime_error("Failed to create IWbemLocator object");
        }

        hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0,
                                   0, 0, 0, pSvc.getAddressOf());
        if (FAILED(hres)) {
            throw std::runtime_error("Could not connect to WMI namespace");
        }

        hres =
            CoSetProxyBlanket(pSvc.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
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
            if (uReturn == 0)
                break;

            auto getBiosProperty = [&](const wchar_t* prop) -> std::string {
                VARIANT vtProp;
                VariantInit(&vtProp);
                if (SUCCEEDED(
                        pclsObj->Get(prop, 0, &vtProp, nullptr, nullptr))) {
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
        }
    } catch (const std::exception& e) {
        spdlog::error("Error fetching BIOS info: {}", e.what());
        throw;
    }

    return biosInfo;
}

#elif __linux__
BiosInfoData BiosInfo::fetchBiosInfo() {
    spdlog::info("Fetching BIOS information");
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
        spdlog::error("Error fetching BIOS info: {}", e.what());
        throw;
    }

    return biosInfo;
}
#endif

bool BiosInfo::isUEFIBootSupported() {
#ifdef _WIN32
    try {
        ComInitializer com;
        ComPtr<IWbemLocator> pLoc;
        ComPtr<IWbemServices> pSvc;

        HRESULT hres =
            CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                             IID_IWbemLocator, (LPVOID*)pLoc.getAddressOf());
        if (FAILED(hres) || !pLoc.get())
            return false;

        hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, 0,
                                   0, 0, 0, pSvc.getAddressOf());
        if (FAILED(hres) || !pSvc.get())
            return false;

        ComPtr<IEnumWbemClassObject> pEnumerator;
        hres = pSvc->ExecQuery(
            bstr_t("WQL"), bstr_t("SELECT * FROM MSFirmwareUefiInfo"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
            pEnumerator.getAddressOf());

        return SUCCEEDED(hres) && pEnumerator.get();
    } catch (...) {
        return false;
    }
#elif __linux__
    std::ifstream efi_dir("/sys/firmware/efi");
    if (efi_dir.good()) {
        return true;
    }

    std::unique_ptr<FILE, int (*)(FILE*)> pipe(
        popen("command -v efibootmgr", "r"), pclose);
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            return true;
        }
    }
    return false;
#else
    return false;
#endif
}

bool BiosInfo::restoreBiosSettings(const std::string& filepath) {
    try {
        std::ifstream in(filepath, std::ios::binary);
        if (!in) {
            spdlog::error("Failed to open BIOS settings backup file: {}",
                          filepath);
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        if (content.empty()) {
            spdlog::warn("BIOS settings backup file is empty: {}", filepath);
        }

        spdlog::info(
            "BIOS settings restoration from {} (simulated) successful.",
            filepath);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to restore BIOS settings: {}", e.what());
        return false;
    }
}

bool BiosInfo::setUEFIBoot(bool enable) {
    if (!isUEFIBootSupported()) {
        spdlog::error("UEFI Boot is not supported on this system");
        return false;
    }

    try {
#ifdef _WIN32
        spdlog::info("Attempting to {} UEFI Boot mode",
                     enable ? "enable" : "disable");

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
            spdlog::error(
                "Administrator privileges required to modify UEFI boot "
                "settings");
            return false;
        }

        std::string command = "bcdedit /set {bootmgr} path \\EFI\\";
        command += enable ? "Microsoft\\Boot\\bootmgfw.efi"
                          : "Legacy\\Boot\\bootmgfw.efi";

        spdlog::info("Executing command: {}", command);
        int result = ::system(command.c_str());

        if (result != 0) {
            spdlog::error("Failed to set UEFI boot mode, command returned: {}",
                          result);
            return false;
        }

        spdlog::warn(
            "System will need to be restarted for changes to take effect");
        return true;
#elif __linux__
        spdlog::info("Attempting to {} UEFI Boot mode",
                     enable ? "enable" : "disable");

        if (geteuid() != 0) {
            spdlog::error(
                "Root privileges required to modify UEFI boot settings");
            return false;
        }

        if (access("/usr/bin/efibootmgr", X_OK) != 0) {
            spdlog::error(
                "efibootmgr not found, cannot modify UEFI boot settings");
            return false;
        }

        std::string command;
        if (enable) {
            command =
                "efibootmgr --create --disk /dev/sda --part 1 "
                "--loader \\\\EFI\\\\BOOT\\\\BOOTX64.EFI --label \"UEFI OS\" "
                "--quiet";
        } else {
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

            std::string bootNum;
            size_t pos = bootEntries.find("Boot");
            if (pos != std::string::npos && bootEntries.length() > pos + 8) {
                bootNum = bootEntries.substr(pos + 4, 4);
                command = "efibootmgr -b " + bootNum + " -B --quiet";
            } else {
                spdlog::error("Could not find UEFI boot entry to disable");
                return false;
            }
        }

        spdlog::info("Executing command: {}", command);
        int result = ::system(command.c_str());

        if (result != 0) {
            spdlog::error("Failed to set UEFI boot mode, command returned: {}",
                          result);
            return false;
        }

        spdlog::warn(
            "System will need to be restarted for changes to take effect");
        return true;
#else
        spdlog::error(
            "Setting UEFI boot mode is not supported on this platform");
        return false;
#endif
    } catch (const std::exception& e) {
        spdlog::error("Failed to set UEFI boot mode: {}", e.what());
        return false;
    }
}

}  // namespace atom::system
