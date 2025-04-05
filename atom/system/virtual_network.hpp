#ifndef VIRTUAL_NETWORK_ADAPTER_H
#define VIRTUAL_NETWORK_ADAPTER_H

#include <memory>
#include <string>


// 虚拟适配器配置结构体
struct VirtualAdapterConfig {
    std::wstring adapterName;
    std::wstring hardwareID;
    std::wstring description;
    std::wstring ipAddress;
    std::wstring subnetMask;
    std::wstring gateway;
    std::wstring primaryDNS;
    std::wstring secondaryDNS;
};

// 主类声明 - 只暴露公共接口
class VirtualNetworkAdapter {
public:
    VirtualNetworkAdapter();
    ~VirtualNetworkAdapter();

    // 禁止拷贝
    VirtualNetworkAdapter(const VirtualNetworkAdapter&) = delete;
    VirtualNetworkAdapter& operator=(const VirtualNetworkAdapter&) = delete;

    // 允许移动
    VirtualNetworkAdapter(VirtualNetworkAdapter&&) noexcept;
    VirtualNetworkAdapter& operator=(VirtualNetworkAdapter&&) noexcept;

    // 创建虚拟网卡
    bool Create(const VirtualAdapterConfig& config);

    // 移除虚拟网卡
    bool Remove(const std::wstring& adapterName);

    // 配置IP设置
    bool ConfigureIP(const std::wstring& adapterName,
                     const std::wstring& ipAddress,
                     const std::wstring& subnetMask,
                     const std::wstring& gateway);

    // 配置DNS设置
    bool ConfigureDNS(const std::wstring& adapterName,
                      const std::wstring& primaryDNS,
                      const std::wstring& secondaryDNS);

    // 获取最后的错误信息
    std::wstring GetLastErrorMessage() const;

private:
    // 前向声明实现类
    class Impl;

    // 指向实现的指针
    std::unique_ptr<Impl> pImpl;
};

#endif  // VIRTUAL_NETWORK_ADAPTER_H