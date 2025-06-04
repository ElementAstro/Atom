#ifndef ATOM_SYSTEM_NETWORK_MANAGER_HPP
#define ATOM_SYSTEM_NETWORK_MANAGER_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @struct NetworkConnection
 * @brief Represents a network connection.
 */
struct NetworkConnection {
    std::string protocol;       ///< Protocol (TCP or UDP).
    std::string localAddress;   ///< Local IP address.
    std::string remoteAddress;  ///< Remote IP address.
    int localPort;              ///< Local port number.
    int remotePort;             ///< Remote port number.
} ATOM_ALIGNAS(128);

/**
 * @class NetworkInterface
 * @brief Represents a network interface.
 */
class NetworkInterface {
public:
    /**
     * @brief Constructs a NetworkInterface object.
     * @param name The name of the network interface.
     * @param addresses The IP addresses associated with the network interface.
     * @param mac The MAC address of the network interface.
     * @param isUp The status of the network interface (true if up, false if
     * down).
     */
    NetworkInterface(std::string name, std::vector<std::string> addresses,
                     std::string mac, bool isUp);

    /**
     * @brief Gets the name of the network interface.
     * @return The name of the network interface.
     */
    [[nodiscard]] auto getName() const -> const std::string&;

    /**
     * @brief Gets the IP addresses associated with the network interface.
     * @return A vector of IP addresses.
     */
    [[nodiscard]] auto getAddresses() const -> const std::vector<std::string>&;

    /**
     * @brief Gets the IP addresses associated with the network interface.
     * @return A vector of IP addresses.
     */
    auto getAddresses() -> std::vector<std::string>&;

    /**
     * @brief Gets the MAC address of the network interface.
     * @return The MAC address.
     */
    [[nodiscard]] auto getMac() const -> const std::string&;

    /**
     * @brief Checks if the network interface is up.
     * @return True if the interface is up, false otherwise.
     */
    [[nodiscard]] auto isUp() const -> bool;

private:
    class NetworkInterfaceImpl;
    std::shared_ptr<NetworkInterfaceImpl> impl_;
};

/**
 * @class NetworkManager
 * @brief Manages network interfaces and connections.
 */
class NetworkManager {
public:
    /**
     * @brief Constructs a NetworkManager object.
     */
    NetworkManager();

    /**
     * @brief Destructs the NetworkManager object.
     */
    ~NetworkManager();

    /**
     * @brief Gets the list of network interfaces.
     * @return A vector of NetworkInterface objects.
     */
    auto getNetworkInterfaces() -> std::vector<NetworkInterface>;

    /**
     * @brief Enables a network interface.
     * @param interfaceName The name of the network interface to enable.
     */
    static void enableInterface(const std::string& interfaceName);

    /**
     * @brief Disables a network interface.
     * @param interfaceName The name of the network interface to disable.
     */
    static void disableInterface(const std::string& interfaceName);

    /**
     * @brief Resolves a DNS hostname to an IP address.
     * @param hostname The DNS hostname to resolve.
     * @return The resolved IP address as a string.
     */
    static auto resolveDNS(const std::string& hostname) -> std::string;

    /**
     * @brief Monitors the connection status of network interfaces.
     */
    void monitorConnectionStatus();

    /**
     * @brief Gets the status of a network interface.
     * @param interfaceName The name of the network interface.
     * @return The status of the network interface as a string.
     */
    auto getInterfaceStatus(const std::string& interfaceName) -> std::string;

    /**
     * @brief Gets the list of DNS servers.
     * @return A vector of DNS server addresses.
     */
    static auto getDNSServers() -> std::vector<std::string>;

    /**
     * @brief Sets the list of DNS servers.
     * @param dnsServers A vector of DNS server addresses.
     */
    static void setDNSServers(const std::vector<std::string>& dnsServers);

    /**
     * @brief Adds a DNS server to the list.
     * @param dns The DNS server address to add.
     */
    static void addDNSServer(const std::string& dns);

    /**
     * @brief Removes a DNS server from the list.
     * @param dns The DNS server address to remove.
     */
    static void removeDNSServer(const std::string& dns);

private:
    class NetworkManagerImpl;
    std::unique_ptr<NetworkManagerImpl> impl_;

    /**
     * @brief Gets the MAC address of a network interface.
     * @param interfaceName The name of the network interface.
     * @return The MAC address as an optional string.
     */
    static auto getMacAddress(const std::string& interfaceName)
        -> std::optional<std::string>;

    /**
     * @brief Checks if a network interface is up.
     * @param interfaceName The name of the network interface.
     * @return True if the interface is up, false otherwise.
     */
    auto isInterfaceUp(const std::string& interfaceName) -> bool;

    /**
     * @brief Status check loop for monitoring network interfaces.
     */
    void statusCheckLoop();
};

/**
 * @brief Gets the network connections of a process by its PID.
 * @param pid The process ID.
 * @return A vector of NetworkConnection structs representing the network
 * connections.
 */
auto getNetworkConnections(int pid) -> std::vector<NetworkConnection>;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_NETWORK_MANAGER_HPP