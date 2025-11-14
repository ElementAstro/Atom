/**
 * @file network_basic.cpp
 * @brief Basic example demonstrating fundamental network operations
 *
 * This example provides a gentle introduction to network management using
 * the Atom System module. It covers:
 * - Basic network interface discovery
 * - Simple interface information retrieval
 * - Basic DNS operations
 * - Network connection listing
 *
 * @note This is a beginner-friendly example
 * @author Atom Framework
 * @date 2024
 */

#include <iomanip>
#include <iostream>
#include "atom/system/network_manager.hpp"
#include "atom/system/process.hpp"

using namespace atom::system;

int main() {
    try {
        std::cout << "=== Basic Network Operations Example ===" << std::endl;
        std::cout << "Learning fundamental network management\n" << std::endl;

        // Create a NetworkManager object
        NetworkManager networkManager;

        // 1. Network Interface Discovery
        std::cout << "[1. Network Interface Discovery]" << std::endl;
        auto interfaces = networkManager.getNetworkInterfaces();
        std::cout << "Found " << interfaces.size() << " network interface(s)"
                  << std::endl;

        if (interfaces.empty()) {
            std::cout << "No network interfaces found." << std::endl;
            std::cout << "This may indicate insufficient permissions or no "
                         "network hardware."
                      << std::endl;
            return 1;
        }

        // Display basic interface information
        std::cout << "\nNetwork Interfaces:" << std::endl;
        std::cout << std::setw(15) << "Name" << " | " << std::setw(18)
                  << "MAC Address" << " | " << std::setw(8) << "Status" << " | "
                  << "IP Address" << std::endl;
        std::cout << std::string(60, '-') << std::endl;

        for (const auto& iface : interfaces) {
            std::cout << std::setw(15) << iface.getName() << " | "
                      << std::setw(18) << iface.getMac() << " | "
                      << std::setw(8) << (iface.isUp() ? "UP" : "DOWN")
                      << " | ";

            auto addresses = iface.getAddresses();
            if (!addresses.empty()) {
                std::cout << addresses[0];
                if (addresses.size() > 1) {
                    std::cout << " (+" << (addresses.size() - 1) << " more)";
                }
            } else {
                std::cout << "No IP assigned";
            }
            std::cout << std::endl;
        }

        // 2. Interface Details
        std::cout << "\n[2. Interface Details]" << std::endl;
        if (!interfaces.empty()) {
            const auto& firstInterface = interfaces[0];
            std::cout << "Details for interface '" << firstInterface.getName()
                      << "':" << std::endl;
            std::cout << "  Name: " << firstInterface.getName() << std::endl;
            std::cout << "  MAC Address: " << firstInterface.getMac()
                      << std::endl;
            std::cout << "  Status: "
                      << (firstInterface.isUp() ? "UP ✓" : "DOWN ✗")
                      << std::endl;

            auto addresses = firstInterface.getAddresses();
            if (!addresses.empty()) {
                std::cout << "  IP Addresses:" << std::endl;
                for (const auto& addr : addresses) {
                    std::cout << "    " << addr << std::endl;
                }
            } else {
                std::cout << "  IP Addresses: None assigned" << std::endl;
            }
        }

        // 3. DNS Operations
        std::cout << "\n[3. DNS Operations]" << std::endl;

        // Get current DNS servers
        std::cout << "Current DNS servers:" << std::endl;
        try {
            auto dnsServers = NetworkManager::getDNSServers();
            if (dnsServers.empty()) {
                std::cout << "  No DNS servers found or accessible"
                          << std::endl;
            } else {
                for (size_t i = 0; i < dnsServers.size(); i++) {
                    std::cout << "  " << (i + 1) << ". " << dnsServers[i]
                              << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cout << "  Error getting DNS servers: " << e.what()
                      << std::endl;
        }

        // Test DNS resolution
        std::cout << "\nTesting DNS resolution:" << std::endl;
        std::vector<std::string> testHosts = {"www.google.com",
                                              "www.example.com"};

        for (const auto& host : testHosts) {
            try {
                std::string ip = NetworkManager::resolveDNS(host);
                std::cout << "  " << std::setw(20) << host << " -> " << ip
                          << std::endl;
            } catch (const std::exception& e) {
                std::cout << "  " << std::setw(20) << host
                          << " -> Failed: " << e.what() << std::endl;
            }
        }

        // 4. Interface Status
        std::cout << "\n[4. Interface Status]" << std::endl;

        std::cout << "Interface status check:" << std::endl;
        for (const auto& iface : interfaces) {
            try {
                std::string status =
                    networkManager.getInterfaceStatus(iface.getName());
                std::cout << "  " << std::setw(15) << iface.getName() << ": "
                          << status << std::endl;
            } catch (const std::exception& e) {
                std::cout << "  " << std::setw(15) << iface.getName()
                          << ": Error - " << e.what() << std::endl;
            }
        }

        // 5. Network Connections
        std::cout << "\n[5. Network Connections]" << std::endl;

        std::cout << "Checking network connections for current process..."
                  << std::endl;
        try {
            auto currentProcess = getSelfProcessInfo();
            auto connections = getNetworkConnections(currentProcess.pid);

            if (connections.empty()) {
                std::cout << "No network connections found for current process"
                          << std::endl;
            } else {
                std::cout << "Found " << connections.size()
                          << " connection(s):" << std::endl;
                std::cout << std::setw(8) << "Protocol" << " | "
                          << std::setw(25) << "Local Address" << " | "
                          << "Remote Address" << std::endl;
                std::cout << std::string(60, '-') << std::endl;

                for (const auto& conn : connections) {
                    std::cout << std::setw(8) << conn.protocol << " | "
                              << std::setw(25)
                              << (conn.localAddress + ":" +
                                  std::to_string(conn.localPort))
                              << " | "
                              << (conn.remoteAddress + ":" +
                                  std::to_string(conn.remotePort))
                              << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cout << "Error getting network connections: " << e.what()
                      << std::endl;
        }

        // 6. Network Interface Categories
        std::cout << "\n[6. Network Interface Categories]" << std::endl;

        int ethernetCount = 0, wirelessCount = 0, loopbackCount = 0,
            otherCount = 0;

        for (const auto& iface : interfaces) {
            std::string name = iface.getName();
            if (name.find("eth") == 0 || name.find("en") == 0) {
                ethernetCount++;
            } else if (name.find("wlan") == 0 || name.find("wifi") == 0 ||
                       name.find("wl") == 0) {
                wirelessCount++;
            } else if (name.find("lo") == 0) {
                loopbackCount++;
            } else {
                otherCount++;
            }
        }

        std::cout << "Interface categories:" << std::endl;
        std::cout << "  Ethernet interfaces: " << ethernetCount << std::endl;
        std::cout << "  Wireless interfaces: " << wirelessCount << std::endl;
        std::cout << "  Loopback interfaces: " << loopbackCount << std::endl;
        std::cout << "  Other interfaces: " << otherCount << std::endl;

        // 7. Active Interfaces
        std::cout << "\n[7. Active Interfaces]" << std::endl;

        int activeCount = 0;
        int inactiveCount = 0;

        for (const auto& iface : interfaces) {
            if (iface.isUp()) {
                activeCount++;
            } else {
                inactiveCount++;
            }
        }

        std::cout << "Interface status summary:" << std::endl;
        std::cout << "  Active (UP): " << activeCount << std::endl;
        std::cout << "  Inactive (DOWN): " << inactiveCount << std::endl;
        std::cout << "  Total: " << interfaces.size() << std::endl;

        std::cout << "\n=== Basic Network Operations Complete ===" << std::endl;
        std::cout << "This example demonstrated:" << std::endl;
        std::cout << "- Network interface discovery" << std::endl;
        std::cout << "- Basic interface information retrieval" << std::endl;
        std::cout << "- DNS server listing and resolution" << std::endl;
        std::cout << "- Interface status checking" << std::endl;
        std::cout << "- Network connection listing" << std::endl;
        std::cout << "- Interface categorization and analysis" << std::endl;
        std::cout << "\nNext steps: Try the advanced network example for more "
                     "features!"
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Network error: " << e.what() << std::endl;
        std::cerr << "\nPossible causes:" << std::endl;
        std::cerr
            << "- Insufficient permissions (try running as administrator/root)"
            << std::endl;
        std::cerr << "- Network subsystem not available" << std::endl;
        std::cerr << "- Platform not supported" << std::endl;
        return 1;
    }

    return 0;
}
