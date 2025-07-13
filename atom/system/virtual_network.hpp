#ifndef VIRTUAL_NETWORK_ADAPTER_H
#define VIRTUAL_NETWORK_ADAPTER_H

#include <memory>
#include <string>

/**
 * @brief Configuration structure for virtual network adapter
 */
struct VirtualAdapterConfig {
    std::wstring adapterName;   ///< Name of the virtual adapter
    std::wstring hardwareID;    ///< Hardware ID for the adapter
    std::wstring description;   ///< Description of the adapter
    std::wstring ipAddress;     ///< IP address to assign
    std::wstring subnetMask;    ///< Subnet mask to assign
    std::wstring gateway;       ///< Default gateway to assign
    std::wstring primaryDNS;    ///< Primary DNS server
    std::wstring secondaryDNS;  ///< Secondary DNS server
};

/**
 * @brief Main class for managing virtual network adapters
 * @details This class provides an interface for creating, configuring and
 * removing virtual network adapters using the Pimpl idiom to hide
 * implementation details.
 */
class VirtualNetworkAdapter {
public:
    VirtualNetworkAdapter();
    ~VirtualNetworkAdapter();

    // Disable copying
    VirtualNetworkAdapter(const VirtualNetworkAdapter&) = delete;
    VirtualNetworkAdapter& operator=(const VirtualNetworkAdapter&) = delete;

    // Enable moving
    VirtualNetworkAdapter(VirtualNetworkAdapter&&) noexcept;
    VirtualNetworkAdapter& operator=(VirtualNetworkAdapter&&) noexcept;

    /**
     * @brief Creates a virtual network adapter
     * @param config Configuration parameters for the adapter
     * @return true if creation succeeded, false otherwise
     */
    bool Create(const VirtualAdapterConfig& config);

    /**
     * @brief Removes a virtual network adapter
     * @param adapterName Name of the adapter to remove
     * @return true if removal succeeded, false otherwise
     */
    bool Remove(const std::wstring& adapterName);

    /**
     * @brief Configures IP settings for an adapter
     * @param adapterName Name of the adapter to configure
     * @param ipAddress IP address to assign
     * @param subnetMask Subnet mask to assign
     * @param gateway Default gateway to assign
     * @return true if configuration succeeded, false otherwise
     */
    bool ConfigureIP(const std::wstring& adapterName,
                     const std::wstring& ipAddress,
                     const std::wstring& subnetMask,
                     const std::wstring& gateway);

    /**
     * @brief Configures DNS settings for an adapter
     * @param adapterName Name of the adapter to configure
     * @param primaryDNS Primary DNS server
     * @param secondaryDNS Secondary DNS server
     * @return true if configuration succeeded, false otherwise
     */
    bool ConfigureDNS(const std::wstring& adapterName,
                      const std::wstring& primaryDNS,
                      const std::wstring& secondaryDNS);

    /**
     * @brief Gets the last error message
     * @return Last error message as wide string
     */
    std::wstring GetLastErrorMessage() const;

private:
    /// Forward declaration of implementation class
    class Impl;

    /// Pointer to implementation (Pimpl idiom)
    std::unique_ptr<Impl> pImpl;
};

#endif  // VIRTUAL_NETWORK_ADAPTER_H
