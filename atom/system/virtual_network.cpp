#include "virtual_network.hpp"

#include <comdef.h>
#include <devguid.h>
#include <iphlpapi.h>
#include <netcon.h>
#include <setupapi.h>
#include <tchar.h>
#include <wbemidl.h>
#include <windows.h>

#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#endif  // _MSC_VER

// WMI连接封装，自动处理资源清理
class WmiConnection {
public:
    struct Result {
        bool success = false;
        std::wstring errorMessage;
        IWbemServices* service = nullptr;

        ~Result() {
            if (service)
                service->Release();
        }
    };

    static Result Connect() {
        Result result;
        IWbemLocator* pLoc = nullptr;

        HRESULT hr =
            CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                             IID_IWbemLocator, (LPVOID*)&pLoc);
        if (FAILED(hr)) {
            result.errorMessage = L"无法创建 WMI 定位器";
            return result;
        }

        // 使用智能指针确保资源释放
        std::unique_ptr<IWbemLocator, decltype(&Release)> locator(pLoc,
                                                                  Release);

        // 连接到 WMI
        hr = locator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0,
                                    NULL, 0, 0, &result.service);
        if (FAILED(hr)) {
            result.errorMessage = L"无法连接到 WMI 服务";
            return result;
        }

        // 设置代理安全级别
        hr = CoSetProxyBlanket(result.service, RPC_C_AUTHN_WINNT,
                               RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL,
                               RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
        if (FAILED(hr)) {
            result.errorMessage = L"无法设置 WMI 代理安全级别";
            return result;
        }

        result.success = true;
        return result;
    }

private:
    static void Release(IUnknown* ptr) {
        if (ptr)
            ptr->Release();
    }
};

// 网络适配器查询封装
class NetworkAdapterQuery {
public:
    struct Result {
        bool success = false;
        std::wstring errorMessage;
        std::wstring deviceId;
        std::wstring configPath;
        IWbemClassObject* config = nullptr;

        ~Result() {
            if (config)
                config->Release();
        }
    };

    static Result FindAdapter(IWbemServices* service,
                              const std::wstring& adapterName) {
        Result result;

        // 构建查询
        std::wstring query =
            L"SELECT * FROM Win32_NetworkAdapter WHERE NetConnectionID = '";
        query += adapterName;
        query += L"'";

        IEnumWbemClassObject* pEnum = nullptr;
        HRESULT hr = service->ExecQuery(
            _bstr_t(L"WQL"), _bstr_t(query.c_str()),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
            &pEnum);

        if (FAILED(hr)) {
            result.errorMessage = L"执行适配器查询失败";
            return result;
        }

        std::unique_ptr<IEnumWbemClassObject, decltype(&Release)> enumPtr(
            pEnum, Release);

        // 获取适配器对象
        IWbemClassObject* pAdapter = nullptr;
        ULONG returned = 0;

        hr = enumPtr->Next(WBEM_INFINITE, 1, &pAdapter, &returned);
        if (FAILED(hr) || returned == 0) {
            result.errorMessage = L"找不到指定的网络适配器";
            return result;
        }

        std::unique_ptr<IWbemClassObject, decltype(&Release)> adapterPtr(
            pAdapter, Release);

        // 获取设备ID
        VARIANT vtDeviceID;
        VariantInit(&vtDeviceID);
        hr = adapterPtr->Get(L"DeviceID", 0, &vtDeviceID, NULL, NULL);

        if (FAILED(hr) || vtDeviceID.vt != VT_BSTR) {
            result.errorMessage = L"无法获取适配器设备ID";
            VariantClear(&vtDeviceID);
            return result;
        }

        // 存储设备ID
        result.deviceId = vtDeviceID.bstrVal;
        VariantClear(&vtDeviceID);

        // 构建配置查询
        std::wstring configQuery =
            L"SELECT * FROM Win32_NetworkAdapterConfiguration WHERE Index = ";
        configQuery += result.deviceId;
        result.configPath = configQuery;

        // 执行配置查询
        IEnumWbemClassObject* pEnumConfig = nullptr;
        hr = service->ExecQuery(
            _bstr_t(L"WQL"), _bstr_t(configQuery.c_str()),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
            &pEnumConfig);

        if (FAILED(hr)) {
            result.errorMessage = L"执行适配器配置查询失败";
            return result;
        }

        std::unique_ptr<IEnumWbemClassObject, decltype(&Release)> enumConfigPtr(
            pEnumConfig, Release);

        // 获取配置对象
        ULONG uReturn = 0;
        hr = enumConfigPtr->Next(WBEM_INFINITE, 1, &result.config, &uReturn);

        if (FAILED(hr) || uReturn == 0) {
            result.errorMessage = L"无法获取适配器配置";
            return result;
        }

        result.success = true;
        return result;
    }

private:
    static void Release(IUnknown* ptr) {
        if (ptr)
            ptr->Release();
    }
};

class WmiMethodCall {
public:
    struct Result {
        bool success = false;
        std::wstring errorMessage;
    };

    static Result ExecMethod(IWbemServices* service,
                             const std::wstring& objectPath,
                             const std::wstring& methodName,
                             IWbemClassObject* inParams) {
        Result result;
        IWbemClassObject* pOutParams = nullptr;

        HRESULT hr = service->ExecMethod(_bstr_t(objectPath.c_str()),
                                         _bstr_t(methodName.c_str()), 0, NULL,
                                         inParams, &pOutParams, NULL);

        if (FAILED(hr)) {
            result.errorMessage = L"执行方法失败: " + methodName;
            return result;
        }

        std::unique_ptr<IWbemClassObject, decltype(&Release)> outParams(
            pOutParams, Release);

        // 检查返回值
        VARIANT vtReturnValue;
        VariantInit(&vtReturnValue);
        hr = outParams->Get(L"ReturnValue", 0, &vtReturnValue, NULL, 0);

        if (SUCCEEDED(hr) && vtReturnValue.vt == VT_I4 &&
            vtReturnValue.lVal == 0) {
            result.success = true;
        } else {
            result.errorMessage = L"方法执行返回错误: " + methodName;
        }

        VariantClear(&vtReturnValue);
        return result;
    }

private:
    static void Release(IWbemClassObject* ptr) {
        if (ptr)
            ptr->Release();
    }
};

class SafeArrayHelper {
public:
    template <typename T>
    static SAFEARRAY* CreateStringArray(const std::vector<T>& strings) {
        if (strings.empty())
            return nullptr;

        SAFEARRAY* pArray = SafeArrayCreateVector(
            VT_BSTR, 0, static_cast<ULONG>(strings.size()));
        if (!pArray)
            return nullptr;

        for (LONG i = 0; i < static_cast<LONG>(strings.size()); ++i) {
            BSTR bstr = SysAllocString(strings[i].c_str());
            SafeArrayPutElement(pArray, &i, bstr);
            SysFreeString(bstr);
        }

        return pArray;
    }
};

class VirtualNetworkAdapter::Impl {
public:
    Impl() {
        // 初始化 COM 以供 WMI 使用
        CoInitializeEx(0, COINIT_MULTITHREADED);
        CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
                             RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE,
                             NULL);
    }

    ~Impl() {
        // 清理 COM
        CoUninitialize();
    }

    // 创建虚拟网卡
    bool Create(const VirtualAdapterConfig& config) {
        HDEVINFO deviceInfoSet = GetNetworkDeviceInfoSet();
        if (deviceInfoSet == INVALID_HANDLE_VALUE) {
            SetLastErrorMessage(L"获取网络设备信息集失败");
            return false;
        }

        // 使用智能指针确保资源释放
        struct DeviceInfoSetDeleter {
            void operator()(HDEVINFO h) { SetupDiDestroyDeviceInfoList(h); }
        };
        std::unique_ptr<void, DeviceInfoSetDeleter> deviceInfoSetPtr(
            deviceInfoSet);

        // 初始化设备信息数据结构
        SP_DEVINFO_DATA deviceInfoData = {};
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        // 创建新的设备信息元素
        if (!SetupDiCreateDeviceInfoW(deviceInfoSet, config.adapterName.c_str(),
                                      &GUID_DEVCLASS_NET,
                                      config.description.c_str(), NULL,
                                      DICD_GENERATE_ID, &deviceInfoData)) {
            SetLastErrorMessage(L"创建设备信息失败");
            return false;
        }

        // 设置硬件 ID
        if (!SetupDiSetDeviceRegistryProperty(
                deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID,
                (BYTE*)config.hardwareID.c_str(),
                (DWORD)((config.hardwareID.length() + 1) * sizeof(WCHAR)))) {
            SetLastErrorMessage(L"设置硬件 ID 失败");
            return false;
        }

        // 注册设备
        if (!RegisterDevice(deviceInfoSet, &deviceInfoData)) {
            SetLastErrorMessage(L"注册设备失败");
            return false;
        }

        // 安装网络组件
        if (!InstallNetCfgComponents(config.adapterName)) {
            SetLastErrorMessage(L"安装网络组件失败");
            return false;
        }

        // 配置 IP
        if (!ConfigureIP(config.adapterName, config.ipAddress,
                         config.subnetMask, config.gateway)) {
            // 错误信息已在 ConfigureIP 中设置
            return false;
        }

        // 配置 DNS
        if (!ConfigureDNS(config.adapterName, config.primaryDNS,
                          config.secondaryDNS)) {
            // 错误信息已在 ConfigureDNS 中设置
            return false;
        }

        return true;
    }

    // 移除虚拟网卡
    bool Remove(const std::wstring& adapterName) {
        HDEVINFO deviceInfoSet = GetNetworkDeviceInfoSet();
        if (deviceInfoSet == INVALID_HANDLE_VALUE) {
            SetLastErrorMessage(L"获取网络设备信息集失败");
            return false;
        }

        // 使用智能指针确保资源释放
        struct DeviceInfoSetDeleter {
            void operator()(HDEVINFO h) { SetupDiDestroyDeviceInfoList(h); }
        };
        std::unique_ptr<void, DeviceInfoSetDeleter> deviceInfoSetPtr(
            deviceInfoSet);

        // 初始化设备信息数据结构
        SP_DEVINFO_DATA deviceInfoData = {};
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        // 按名称查找设备
        if (!GetDeviceInfoData(deviceInfoSet, adapterName, &deviceInfoData)) {
            SetLastErrorMessage(L"查找设备失败");
            return false;
        }

        // 移除设备
        if (!SetupDiRemoveDevice(deviceInfoSet, &deviceInfoData)) {
            SetLastErrorMessage(L"移除设备失败");
            return false;
        }

        return true;
    }

    // 使用 WMI 配置 IP 地址和相关设置
    bool ConfigureIP(const std::wstring& adapterName,
                     const std::wstring& ipAddress,
                     const std::wstring& subnetMask,
                     const std::wstring& gateway) {
        // 连接到 WMI
        auto wmiResult = WmiConnection::Connect();
        if (!wmiResult.success) {
            SetLastErrorMessage(wmiResult.errorMessage);
            return false;
        }

        // 查找网络适配器
        auto adapterResult =
            NetworkAdapterQuery::FindAdapter(wmiResult.service, adapterName);
        if (!adapterResult.success) {
            SetLastErrorMessage(adapterResult.errorMessage);
            return false;
        }

        // 获取 EnableStatic 方法
        IWbemClassObject* pInParamsDefinition = nullptr;
        HRESULT hr = adapterResult.config->GetMethod(
            L"EnableStatic", 0, &pInParamsDefinition, NULL);
        if (FAILED(hr)) {
            SetLastErrorMessage(L"无法获取 EnableStatic 方法");
            return false;
        }

        std::unique_ptr<IWbemClassObject, decltype(&Release)> inParamsDef(
            pInParamsDefinition, Release);

        // 创建方法实例
        IWbemClassObject* pClassInstance = nullptr;
        hr = inParamsDef->SpawnInstance(0, &pClassInstance);
        if (FAILED(hr)) {
            SetLastErrorMessage(L"无法创建方法实例");
            return false;
        }

        std::unique_ptr<IWbemClassObject, decltype(&Release)> classInstance(
            pClassInstance, Release);

        // 创建 IP 地址数组
        std::vector<std::wstring> ipAddresses = {ipAddress};
        std::vector<std::wstring> subnetMasks = {subnetMask};

        SAFEARRAY* pIPArray = SafeArrayHelper::CreateStringArray(ipAddresses);
        SAFEARRAY* pSubnetArray =
            SafeArrayHelper::CreateStringArray(subnetMasks);

        if (!pIPArray || !pSubnetArray) {
            SetLastErrorMessage(L"创建 IP 地址数组失败");
            if (pIPArray)
                SafeArrayDestroy(pIPArray);
            if (pSubnetArray)
                SafeArrayDestroy(pSubnetArray);
            return false;
        }

        // 设置方法参数
        VARIANT vtIPAddresses, vtSubnetMasks;
        VariantInit(&vtIPAddresses);
        VariantInit(&vtSubnetMasks);

        vtIPAddresses.vt = VT_ARRAY | VT_BSTR;
        vtIPAddresses.parray = pIPArray;

        vtSubnetMasks.vt = VT_ARRAY | VT_BSTR;
        vtSubnetMasks.parray = pSubnetArray;

        classInstance->Put(L"IPAddress", 0, &vtIPAddresses, 0);
        classInstance->Put(L"SubnetMask", 0, &vtSubnetMasks, 0);

        // 执行方法
        auto methodResult = WmiMethodCall::ExecMethod(
            wmiResult.service, adapterResult.configPath, L"EnableStatic",
            classInstance.get());

        // 清理
        VariantClear(&vtIPAddresses);
        VariantClear(&vtSubnetMasks);

        if (!methodResult.success) {
            SetLastErrorMessage(methodResult.errorMessage);
            return false;
        }

        // 如果提供了默认网关，则设置
        if (!gateway.empty()) {
            // 获取 SetGateways 方法
            IWbemClassObject* pGatewayParamsDefinition = nullptr;
            hr = adapterResult.config->GetMethod(
                L"SetGateways", 0, &pGatewayParamsDefinition, NULL);
            if (FAILED(hr)) {
                SetLastErrorMessage(L"无法获取 SetGateways 方法");
                return false;
            }

            std::unique_ptr<IWbemClassObject, decltype(&Release)>
                gatewayParamsDef(pGatewayParamsDefinition, Release);

            // 创建方法实例
            IWbemClassObject* pGatewayInstance = nullptr;
            hr = gatewayParamsDef->SpawnInstance(0, &pGatewayInstance);
            if (FAILED(hr)) {
                SetLastErrorMessage(L"无法创建网关方法实例");
                return false;
            }

            std::unique_ptr<IWbemClassObject, decltype(&Release)>
                gatewayInstance(pGatewayInstance, Release);

            // 创建网关数组
            std::vector<std::wstring> gateways = {gateway};
            SAFEARRAY* pGatewayArray =
                SafeArrayHelper::CreateStringArray(gateways);

            if (!pGatewayArray) {
                SetLastErrorMessage(L"创建网关数组失败");
                return false;
            }

            // 设置方法参数
            VARIANT vtGateways;
            VariantInit(&vtGateways);

            vtGateways.vt = VT_ARRAY | VT_BSTR;
            vtGateways.parray = pGatewayArray;

            gatewayInstance->Put(L"DefaultIPGateway", 0, &vtGateways, 0);

            // 执行方法
            auto gatewayMethodResult = WmiMethodCall::ExecMethod(
                wmiResult.service, adapterResult.configPath, L"SetGateways",
                gatewayInstance.get());

            // 清理
            VariantClear(&vtGateways);

            if (!gatewayMethodResult.success) {
                SetLastErrorMessage(gatewayMethodResult.errorMessage);
                return false;
            }
        }

        return true;
    }

    // 配置 DNS 设置
    bool ConfigureDNS(const std::wstring& adapterName,
                      const std::wstring& primaryDNS,
                      const std::wstring& secondaryDNS) {
        // 如果没有提供 DNS，则认为成功
        if (primaryDNS.empty() && secondaryDNS.empty()) {
            return true;
        }

        // 连接到 WMI
        auto wmiResult = WmiConnection::Connect();
        if (!wmiResult.success) {
            SetLastErrorMessage(wmiResult.errorMessage);
            return false;
        }

        // 查找网络适配器
        auto adapterResult =
            NetworkAdapterQuery::FindAdapter(wmiResult.service, adapterName);
        if (!adapterResult.success) {
            SetLastErrorMessage(adapterResult.errorMessage);
            return false;
        }

        // 获取 SetDNSServerSearchOrder 方法
        IWbemClassObject* pInParamsDefinition = nullptr;
        HRESULT hr = adapterResult.config->GetMethod(
            L"SetDNSServerSearchOrder", 0, &pInParamsDefinition, NULL);
        if (FAILED(hr)) {
            SetLastErrorMessage(L"无法获取 SetDNSServerSearchOrder 方法");
            return false;
        }

        std::unique_ptr<IWbemClassObject, decltype(&Release)> inParamsDef(
            pInParamsDefinition, Release);

        // 创建方法实例
        IWbemClassObject* pClassInstance = nullptr;
        hr = inParamsDef->SpawnInstance(0, &pClassInstance);
        if (FAILED(hr)) {
            SetLastErrorMessage(L"无法创建 DNS 方法实例");
            return false;
        }

        std::unique_ptr<IWbemClassObject, decltype(&Release)> classInstance(
            pClassInstance, Release);

        // 准备 DNS 服务器列表
        std::vector<std::wstring> dnsServers;
        if (!primaryDNS.empty()) {
            dnsServers.push_back(primaryDNS);
        }
        if (!secondaryDNS.empty()) {
            dnsServers.push_back(secondaryDNS);
        }

        // 创建 DNS 服务器数组
        SAFEARRAY* pDNSArray = SafeArrayHelper::CreateStringArray(dnsServers);
        if (!pDNSArray) {
            SetLastErrorMessage(L"创建 DNS 服务器数组失败");
            return false;
        }

        // 设置方法参数
        VARIANT vtDNSServers;
        VariantInit(&vtDNSServers);

        vtDNSServers.vt = VT_ARRAY | VT_BSTR;
        vtDNSServers.parray = pDNSArray;

        classInstance->Put(L"DNSServerSearchOrder", 0, &vtDNSServers, 0);

        // 执行方法
        auto methodResult = WmiMethodCall::ExecMethod(
            wmiResult.service, adapterResult.configPath,
            L"SetDNSServerSearchOrder", classInstance.get());

        // 清理
        VariantClear(&vtDNSServers);

        if (!methodResult.success) {
            SetLastErrorMessage(methodResult.errorMessage);
            return false;
        }

        return true;
    }

    // 获取最后的错误消息
    std::wstring GetLastErrorMessage() const { return m_lastErrorMessage; }

private:
    // 获取网络适配器的设备信息集
    HDEVINFO GetNetworkDeviceInfoSet() {
        return SetupDiGetClassDevs(&GUID_DEVCLASS_NET, NULL, NULL,
                                   DIGCF_PRESENT | DIGCF_PROFILE);
    }

    // 获取特定适配器的设备信息数据
    bool GetDeviceInfoData(HDEVINFO deviceInfoSet,
                           const std::wstring& adapterName,
                           PSP_DEVINFO_DATA deviceInfoData) {
        SP_DEVINFO_DATA tempDeviceInfoData = {};
        tempDeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        DWORD index = 0;
        WCHAR friendlyName[256] = {0};
        DWORD requiredSize = 0;

        while (
            SetupDiEnumDeviceInfo(deviceInfoSet, index, &tempDeviceInfoData)) {
            if (SetupDiGetDeviceRegistryProperty(
                    deviceInfoSet, &tempDeviceInfoData, SPDRP_FRIENDLYNAME,
                    NULL, (PBYTE)friendlyName, sizeof(friendlyName),
                    &requiredSize)) {
                if (wcscmp(friendlyName, adapterName.c_str()) == 0) {
                    *deviceInfoData = tempDeviceInfoData;
                    return true;
                }
            }

            index++;
        }

        return false;
    }

    // 注册设备
    bool RegisterDevice(HDEVINFO deviceInfoSet,
                        PSP_DEVINFO_DATA deviceInfoData) {
        return SetupDiCallClassInstaller(DIF_REGISTERDEVICE, deviceInfoSet,
                                         deviceInfoData)
                   ? true
                   : false;
    }

    // 安装网络配置组件
    bool InstallNetCfgComponents(const std::wstring& adapterName) {
        // Use WMI query to get network adapter information
        auto wmiResult = WmiConnection::Connect();
        if (!wmiResult.success) {
            SetLastErrorMessage(wmiResult.errorMessage);
            return false;
        }

        // Find network adapter
        auto adapterResult =
            NetworkAdapterQuery::FindAdapter(wmiResult.service, adapterName);
        if (!adapterResult.success) {
            SetLastErrorMessage(adapterResult.errorMessage);
            return false;
        }

        // Ensure TCP/IP protocol is enabled
        IWbemClassObject* pInParamsDefinition = nullptr;
        HRESULT hr = adapterResult.config->GetMethod(
            L"EnableIPProtocol", 0, &pInParamsDefinition, NULL);
        if (FAILED(hr)) {
            SetLastErrorMessage(L"Failed to get EnableIPProtocol method");
            return false;
        }

        std::unique_ptr<IWbemClassObject, decltype(&Release)> inParamsDef(
            pInParamsDefinition, Release);

        // Create method instance
        IWbemClassObject* pClassInstance = nullptr;
        hr = inParamsDef->SpawnInstance(0, &pClassInstance);
        if (FAILED(hr)) {
            SetLastErrorMessage(L"Failed to create method instance");
            return false;
        }

        std::unique_ptr<IWbemClassObject, decltype(&Release)> classInstance(
            pClassInstance, Release);

        // Execute method
        auto methodResult = WmiMethodCall::ExecMethod(
            wmiResult.service, adapterResult.configPath, L"EnableIPProtocol",
            classInstance.get());

        if (!methodResult.success) {
            SetLastErrorMessage(methodResult.errorMessage);
            return false;
        }

        // Ensure DHCP or other protocols are enabled (as needed)
        // Note: When setting static IP, DHCP is typically not required

        return true;
    }

    void SetLastErrorMessage(const std::wstring& message) {
        m_lastErrorMessage = message;
    }

    static void Release(IUnknown* ptr) {
        if (ptr)
            ptr->Release();
    }

    // 私有成员变量
    std::wstring m_lastErrorMessage;
};

// 通过委托给 pImpl 实现 VirtualNetworkAdapter 的公共方法
VirtualNetworkAdapter::VirtualNetworkAdapter()
    : pImpl(std::make_unique<Impl>()) {}

VirtualNetworkAdapter::~VirtualNetworkAdapter() = default;

// 移动构造函数
VirtualNetworkAdapter::VirtualNetworkAdapter(
    VirtualNetworkAdapter&& other) noexcept = default;

// 移动赋值运算符
VirtualNetworkAdapter& VirtualNetworkAdapter::operator=(
    VirtualNetworkAdapter&& other) noexcept = default;

bool VirtualNetworkAdapter::Create(const VirtualAdapterConfig& config) {
    return pImpl->Create(config);
}

bool VirtualNetworkAdapter::Remove(const std::wstring& adapterName) {
    return pImpl->Remove(adapterName);
}

bool VirtualNetworkAdapter::ConfigureIP(const std::wstring& adapterName,
                                        const std::wstring& ipAddress,
                                        const std::wstring& subnetMask,
                                        const std::wstring& gateway) {
    return pImpl->ConfigureIP(adapterName, ipAddress, subnetMask, gateway);
}

bool VirtualNetworkAdapter::ConfigureDNS(const std::wstring& adapterName,
                                         const std::wstring& primaryDNS,
                                         const std::wstring& secondaryDNS) {
    return pImpl->ConfigureDNS(adapterName, primaryDNS, secondaryDNS);
}

std::wstring VirtualNetworkAdapter::GetLastErrorMessage() const {
    return pImpl->GetLastErrorMessage();
}
