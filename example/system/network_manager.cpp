#include "atom/system/network_manager.hpp"

#include <iostream>

using namespace atom::system;

int main() {
    // Create a NetworkManager object
    NetworkManager networkManager;

    // Get the list of network interfaces
    auto interfaces = networkManager.getNetworkInterfaces();
    std::cout << "Network Interfaces:" << std::endl;
    for (const auto& iface : interfaces) {
        std::cout << "Name: " << iface.getName() << ", MAC: " << iface.getMac()
                  << ", Status: " << (iface.isUp() ? "Up" : "Down")
                  << std::endl;
        std::cout << "Addresses: ";
        for (const auto& addr : iface.getAddresses()) {
            std::cout << addr << " ";
        }
        std::cout << std::endl;
    }

    // Enable a network interface
    NetworkManager::enableInterface("eth0");
    std::cout << "Enabled interface: eth0" << std::endl;

    // Disable a network interface
    NetworkManager::disableInterface("eth0");
    std::cout << "Disabled interface: eth0" << std::endl;

    // Resolve a DNS hostname to an IP address
    std::string ipAddress = NetworkManager::resolveDNS("www.example.com");
    std::cout << "Resolved IP address for www.example.com: " << ipAddress
              << std::endl;

    // Monitor the connection status of network interfaces
    networkManager.monitorConnectionStatus();
    std::cout << "Monitoring connection status of network interfaces"
              << std::endl;

    // Get the status of a network interface
    std::string status = networkManager.getInterfaceStatus("eth0");
    std::cout << "Status of interface eth0: " << status << std::endl;

    // Get the list of DNS servers
    auto dnsServers = NetworkManager::getDNSServers();
    std::cout << "DNS Servers:" << std::endl;
    for (const auto& dns : dnsServers) {
        std::cout << dns << std::endl;
    }

    // Set the list of DNS servers
    NetworkManager::setDNSServers({"8.8.8.8", "8.8.4.4"});
    std::cout << "Set DNS servers to 8.8.8.8 and 8.8.4.4" << std::endl;

    // Add a DNS server to the list
    NetworkManager::addDNSServer("1.1.1.1");
    std::cout << "Added DNS server 1.1.1.1" << std::endl;

    // Remove a DNS server from the list
    NetworkManager::removeDNSServer("8.8.4.4");
    std::cout << "Removed DNS server 8.8.4.4" << std::endl;

    // Get the network connections of a process by its PID
    auto connections = getNetworkConnections(1234);
    std::cout << "Network connections for process 1234:" << std::endl;
    for (const auto& conn : connections) {
        std::cout << "Protocol: " << conn.protocol
                  << ", Local: " << conn.localAddress << ":" << conn.localPort
                  << ", Remote: " << conn.remoteAddress << ":"
                  << conn.remotePort << std::endl;
    }

    return 0;
}
