#include "windows.hpp"
#include "common.hpp"

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#include <intrin.h>
#include <wbemidl.h>
#include <comdef.h>
#include <iostream>
#include <vector>
#include <string>

#pragma comment(lib, "wbemuuid.lib")
#endif

#include <spdlog/spdlog.h>

namespace atom::system::virtual_env::platform::windows_impl {

#ifdef _WIN32

auto detectVirtualizationWindows() -> VirtualizationResult {
    VirtualizationResult result;
    result.platform = "Windows";
    
    // Check registry for virtualization indicators
    auto regInfo = checkRegistryVirtualization();
    if (!regInfo.empty()) {
        for (const auto& info : regInfo) {
            result.indicators.push_back("Registry: " + info);
            result.confidence += 0.2;
        }
    }
    
    // Check WMI for system information
    auto wmiInfo = getWMISystemInfo();
    if (!wmiInfo.empty()) {
        for (const auto& [key, value] : wmiInfo) {
            if (containsVMKeywords(value)) {
                result.indicators.push_back("WMI " + key + ": " + value);
                result.confidence += 0.2;
            }
        }
    }
    
    // Check for VM-specific services
    auto vmServices = checkVirtualizationServices();
    for (const auto& service : vmServices) {
        result.indicators.push_back("VM Service: " + service);
        result.confidence += 0.1;
    }
    
    // Check for VM-specific processes
    auto vmProcesses = checkVirtualizationProcesses();
    for (const auto& process : vmProcesses) {
        result.indicators.push_back("VM Process: " + process);
        result.confidence += 0.1;
    }
    
    // Check hardware information
    auto hwInfo = checkHardwareVirtualization();
    if (!hwInfo.empty()) {
        for (const auto& info : hwInfo) {
            result.indicators.push_back("Hardware: " + info);
            result.confidence += 0.15;
        }
    }
    
    // Limit confidence to 1.0
    result.confidence = std::min(result.confidence, 1.0);
    result.is_virtual = result.confidence > 0.3;
    
    return result;
}

auto checkRegistryVirtualization() -> std::vector<std::string> {
    std::vector<std::string> indicators;
    
    // Check for VMware registry entries
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     _T("SOFTWARE\\VMware, Inc.\\VMware Tools"), 0, KEY_READ,
                     &hKey) == ERROR_SUCCESS) {
        indicators.push_back("VMware Tools registry key found");
        RegCloseKey(hKey);
    }
    
    // Check for VirtualBox registry entries
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     _T("SOFTWARE\\Oracle\\VirtualBox Guest Additions"), 0, KEY_READ,
                     &hKey) == ERROR_SUCCESS) {
        indicators.push_back("VirtualBox Guest Additions registry key found");
        RegCloseKey(hKey);
    }
    
    // Check for Hyper-V registry entries
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     _T("SOFTWARE\\Microsoft\\Virtual Machine\\Guest\\Parameters"), 0, KEY_READ,
                     &hKey) == ERROR_SUCCESS) {
        indicators.push_back("Hyper-V guest registry key found");
        RegCloseKey(hKey);
    }
    
    // Check system BIOS information
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     _T("HARDWARE\\DESCRIPTION\\System\\BIOS"), 0, KEY_READ,
                     &hKey) == ERROR_SUCCESS) {
        
        std::array<TCHAR, 256> biosInfo;
        DWORD bufSize = sizeof(biosInfo);
        
        if (RegQueryValueEx(hKey, _T("SystemManufacturer"), nullptr, nullptr,
                            reinterpret_cast<LPBYTE>(biosInfo.data()),
                            &bufSize) == ERROR_SUCCESS) {
            std::string manufacturer(biosInfo.data());
            if (containsVMKeywords(manufacturer)) {
                indicators.push_back("BIOS SystemManufacturer: " + manufacturer);
            }
        }
        
        bufSize = sizeof(biosInfo);
        if (RegQueryValueEx(hKey, _T("SystemProductName"), nullptr, nullptr,
                            reinterpret_cast<LPBYTE>(biosInfo.data()),
                            &bufSize) == ERROR_SUCCESS) {
            std::string productName(biosInfo.data());
            if (containsVMKeywords(productName)) {
                indicators.push_back("BIOS SystemProductName: " + productName);
            }
        }
        
        RegCloseKey(hKey);
    }
    
    return indicators;
}

auto getWMISystemInfo() -> std::unordered_map<std::string, std::string> {
    std::unordered_map<std::string, std::string> info;
    
    HRESULT hres;
    
    // Initialize COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        return info;
    }
    
    // Set general COM security levels
    hres = CoInitializeSecurity(
        nullptr,
        -1,                          // COM authentication
        nullptr,                     // Authentication services
        nullptr,                     // Reserved
        RPC_C_AUTHN_LEVEL_NONE,      // Default authentication
        RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation
        nullptr,                     // Authentication info
        EOAC_NONE,                   // Additional capabilities
        nullptr                      // Reserved
    );
    
    if (FAILED(hres)) {
        CoUninitialize();
        return info;
    }
    
    // Obtain the initial locator to WMI
    IWbemLocator *pLoc = nullptr;
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID *) &pLoc);
    
    if (FAILED(hres)) {
        CoUninitialize();
        return info;
    }
    
    // Connect to WMI through the IWbemLocator::ConnectServer method
    IWbemServices *pSvc = nullptr;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
        nullptr,                 // User name. NULL = current user
        nullptr,                 // User password. NULL = current
        0,                       // Locale. NULL indicates current
        NULL,                    // Security flags.
        0,                       // Authority (for example, Kerberos)
        0,                       // Context object
        &pSvc                    // pointer to IWbemServices proxy
    );
    
    if (FAILED(hres)) {
        pLoc->Release();
        CoUninitialize();
        return info;
    }
    
    // Set security levels on the proxy
    hres = CoSetProxyBlanket(
        pSvc,                        // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
        nullptr,                     // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx
        RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
        nullptr,                     // client identity
        EOAC_NONE                    // proxy capabilities
    );
    
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return info;
    }
    
    // Query for computer system information
    IEnumWbemClassObject* pEnumerator = nullptr;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_ComputerSystem"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &pEnumerator);
    
    if (SUCCEEDED(hres)) {
        IWbemClassObject *pclsObj = nullptr;
        ULONG uReturn = 0;
        
        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            
            if (0 == uReturn) {
                break;
            }
            
            VARIANT vtProp;
            
            // Get Manufacturer
            hr = pclsObj->Get(L"Manufacturer", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
                info["Manufacturer"] = _com_util::ConvertBSTRToString(vtProp.bstrVal);
            }
            VariantClear(&vtProp);
            
            // Get Model
            hr = pclsObj->Get(L"Model", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
                info["Model"] = _com_util::ConvertBSTRToString(vtProp.bstrVal);
            }
            VariantClear(&vtProp);
            
            pclsObj->Release();
        }
        
        pEnumerator->Release();
    }
    
    // Cleanup
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
    
    return info;
}

auto checkVirtualizationServices() -> std::vector<std::string> {
    std::vector<std::string> vmServices;
    
    std::vector<std::string> serviceNames = {
        "VMTools", "VBoxService", "vmicheartbeat", "vmicvss",
        "vmicshutdown", "vmicexchange", "QEMU Guest Agent"
    };
    
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (scManager == nullptr) {
        return vmServices;
    }
    
    for (const auto& serviceName : serviceNames) {
        SC_HANDLE service = OpenService(scManager, serviceName.c_str(), SERVICE_QUERY_STATUS);
        if (service != nullptr) {
            vmServices.push_back(serviceName);
            CloseServiceHandle(service);
        }
    }
    
    CloseServiceHandle(scManager);
    return vmServices;
}

auto checkVirtualizationProcesses() -> std::vector<std::string> {
    std::vector<std::string> vmProcesses;
    
    std::string output = executeCommand("tasklist");
    
    std::vector<std::string> processNames = {
        "vmtoolsd.exe", "VBoxService.exe", "VBoxTray.exe",
        "qemu-ga.exe", "xenservice.exe"
    };
    
    for (const auto& processName : processNames) {
        if (output.find(processName) != std::string::npos) {
            vmProcesses.push_back(processName);
        }
    }
    
    return vmProcesses;
}

auto checkHardwareVirtualization() -> std::vector<std::string> {
    std::vector<std::string> hwInfo;
    
    // Check PCI devices
    std::string pciOutput = executeCommand("wmic path Win32_PnPEntity get Name");
    if (containsVMKeywords(pciOutput)) {
        hwInfo.push_back("VM PCI devices detected");
    }
    
    // Check video controller
    std::string videoOutput = executeCommand("wmic path win32_videocontroller get caption");
    if (containsVMKeywords(videoOutput)) {
        hwInfo.push_back("VM video controller detected");
    }
    
    // Check disk drives
    std::string diskOutput = executeCommand("wmic diskdrive get caption,model");
    if (containsVMKeywords(diskOutput)) {
        hwInfo.push_back("VM disk drives detected");
    }
    
    // Check network adapters
    std::string netOutput = executeCommand("wmic path Win32_NetworkAdapter get Name");
    if (containsVMKeywords(netOutput)) {
        hwInfo.push_back("VM network adapters detected");
    }
    
    return hwInfo;
}

auto getWindowsVirtualizationType() -> std::string {
    // Check WMI system information
    auto wmiInfo = getWMISystemInfo();
    
    if (wmiInfo.count("Manufacturer")) {
        const std::string& manufacturer = wmiInfo["Manufacturer"];
        if (manufacturer.find("VMware") != std::string::npos) {
            return "VMware";
        }
        if (manufacturer.find("innotek") != std::string::npos ||
            manufacturer.find("Oracle") != std::string::npos) {
            return "VirtualBox";
        }
        if (manufacturer.find("Microsoft") != std::string::npos) {
            return "Hyper-V";
        }
        if (manufacturer.find("QEMU") != std::string::npos) {
            return "QEMU";
        }
        if (manufacturer.find("Xen") != std::string::npos) {
            return "Xen";
        }
    }
    
    if (wmiInfo.count("Model")) {
        const std::string& model = wmiInfo["Model"];
        if (model.find("Virtual Machine") != std::string::npos) {
            return "Hyper-V";
        }
        if (model.find("VMware") != std::string::npos) {
            return "VMware";
        }
        if (model.find("VirtualBox") != std::string::npos) {
            return "VirtualBox";
        }
    }
    
    // Check registry for more specific information
    auto regInfo = checkRegistryVirtualization();
    for (const auto& info : regInfo) {
        if (info.find("VMware") != std::string::npos) {
            return "VMware";
        }
        if (info.find("VirtualBox") != std::string::npos) {
            return "VirtualBox";
        }
        if (info.find("Hyper-V") != std::string::npos) {
            return "Hyper-V";
        }
    }
    
    return "Unknown";
}

#else

// Non-Windows implementations (stubs)
auto detectVirtualizationWindows() -> VirtualizationResult {
    VirtualizationResult result;
    result.platform = "Non-Windows";
    result.is_virtual = false;
    result.confidence = 0.0;
    return result;
}

auto checkRegistryVirtualization() -> std::vector<std::string> {
    return {};
}

auto getWMISystemInfo() -> std::unordered_map<std::string, std::string> {
    return {};
}

auto checkVirtualizationServices() -> std::vector<std::string> {
    return {};
}

auto checkVirtualizationProcesses() -> std::vector<std::string> {
    return {};
}

auto checkHardwareVirtualization() -> std::vector<std::string> {
    return {};
}

auto getWindowsVirtualizationType() -> std::string {
    return "Not Windows";
}

#endif

} // namespace atom::system::virtual_env::platform::windows_impl
