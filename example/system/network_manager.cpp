/**
 * @file network_manager_advanced.cpp
 * @brief Comprehensive example demonstrating advanced network management
 *
 * This example showcases advanced network management capabilities including:
 * - Network interface discovery and detailed analysis
 * - Advanced DNS management and monitoring
 * - Network connection tracking and analysis
 * - Interface configuration and control
 * - Network performance monitoring
 * - Network diagnostics and troubleshooting
 * - Cross-platform network operations
 *
 * @warning Network operations may require elevated privileges
 * @note Cross-platform compatibility: Windows, Linux, macOS
 * @author Atom Framework
 * @date 2024
 */

#include "atom/system/network_manager.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <thread>
#include "atom/system/process.hpp"

using namespace atom::system;

/**
 * @brief Print a formatted section header
 */
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

/**
 * @brief Print detailed interface information
 */
void printInterfaceDetails(const NetworkInterface& iface) {
    std::cout << "\n--- Interface Details ---" << std::endl;
    std::cout << "Name: " << iface.getName() << std::endl;
    std::cout << "MAC Address: " << iface.getMac() << std::endl;
    std::cout << "Status: " << (iface.isUp() ? "UP ✓" : "DOWN ✗") << std::endl;

    auto addresses = iface.getAddresses();
    if (!addresses.empty()) {
        std::cout << "IP Addresses:" << std::endl;
        for (const auto& addr : addresses) {
            std::cout << "  " << addr << std::endl;
        }
    } else {
        std::cout << "IP Addresses: None assigned" << std::endl;
    }
}

/**
 * @brief Analyze network interfaces by name patterns
 */
void analyzeInterfacesByName(const std::vector<NetworkInterface>& interfaces) {
    std::map<std::string, std::vector<NetworkInterface>> nameGroups;

    for (const auto& iface : interfaces) {
        std::string name = iface.getName();
        std::string category = "Other";

        // Categorize by common naming patterns
        if (name.find("eth") == 0 || name.find("en") == 0) {
            category = "Ethernet";
        } else if (name.find("wlan") == 0 || name.find("wifi") == 0 ||
                   name.find("wl") == 0) {
            category = "Wireless";
        } else if (name.find("lo") == 0) {
            category = "Loopback";
        } else if (name.find("docker") == 0 || name.find("br-") == 0) {
            category = "Virtual/Bridge";
        } else if (name.find("tun") == 0 || name.find("tap") == 0) {
            category = "Tunnel";
        }

        nameGroups[category].push_back(iface);
    }

    std::cout << "\nInterface Analysis by Category:" << std::endl;
    for (const auto& [category, ifaceList] : nameGroups) {
        std::cout << "\n"
                  << category << " Interfaces (" << ifaceList.size()
                  << "):" << std::endl;
        for (const auto& iface : ifaceList) {
            std::cout << "  " << iface.getName() << " - "
                      << (iface.isUp() ? "UP" : "DOWN") << " - "
                      << iface.getMac() << std::endl;
        }
    }
}

int main() {
    try {
        std::cout << "=== Advanced Network Management Example ===" << std::endl;
        std::cout
            << "Demonstrating comprehensive network management capabilities\n"
            << std::endl;

        // Create a NetworkManager object
        NetworkManager networkManager;

        // 1. Network Interface Discovery and Analysis
        printSection("Network Interface Discovery and Analysis");

        auto interfaces = networkManager.getNetworkInterfaces();
        std::cout << "Discovered " << interfaces.size()
                  << " network interface(s)" << std::endl;

        if (interfaces.empty()) {
            std::cout << "No network interfaces found. This may indicate:"
                      << std::endl;
            std::cout << "- Insufficient permissions" << std::endl;
            std::cout << "- Network subsystem not available" << std::endl;
            std::cout << "- Platform not supported" << std::endl;
            return 1;
        }

        // Display basic interface information
        std::cout << "\nInterface Summary:" << std::endl;
        std::cout << std::setw(15) << "Name" << " | " << std::setw(18)
                  << "MAC Address" << " | " << std::setw(8) << "Status" << " | "
                  << "Addresses" << std::endl;
        std::cout << std::string(70, '-') << std::endl;

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
                std::cout << "No IP";
            }
            std::cout << std::endl;
        }

        // Analyze interfaces by name patterns
        analyzeInterfacesByName(interfaces);

        // 2. Detailed Interface Information
        printSection("Detailed Interface Information");

        // Show details for the first few interfaces
        for (size_t i = 0; i < std::min(interfaces.size(), size_t(3)); i++) {
            printInterfaceDetails(interfaces[i]);
        }

        if (interfaces.size() > 3) {
            std::cout << "\n... and " << (interfaces.size() - 3)
                      << " more interfaces" << std::endl;
        }

        // 3. DNS Management
        printSection("DNS Management");

        std::cout << "Current DNS servers:" << std::endl;
        try {
            auto dnsServers = NetworkManager::getDNSServers();
            if (dnsServers.empty()) {
                std::cout << "No DNS servers configured or accessible"
                          << std::endl;
            } else {
                for (size_t i = 0; i < dnsServers.size(); i++) {
                    std::cout << "  " << (i + 1) << ". " << dnsServers[i]
                              << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cout << "Error getting DNS servers: " << e.what() << std::endl;
        }

        // Test DNS resolution
        std::cout << "\nTesting DNS resolution:" << std::endl;
        std::vector<std::string> testHosts = {
            "www.google.com", "www.github.com", "www.example.com"};

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

        // 4. Interface Status Monitoring
        printSection("Interface Status Monitoring");

        std::cout << "Getting interface status for available interfaces:"
                  << std::endl;
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

        // 5. Network Connection Analysis
        printSection("Network Connection Analysis");

        std::cout << "Analyzing network connections for current process..."
                  << std::endl;
        try {
            // Get current process PID
            auto currentProcess = getSelfProcessInfo();
            auto connections = getNetworkConnections(currentProcess.pid);
            if (connections.empty()) {
                std::cout << "No network connections found for current process"
                          << std::endl;
            } else {
                std::cout << "Found " << connections.size()
                          << " connection(s):" << std::endl;
                std::cout << std::setw(8) << "Protocol" << " | "
                          << std::setw(20) << "Local Address" << " | "
                          << std::setw(20) << "Remote Address" << std::endl;
                std::cout << std::string(60, '-') << std::endl;

                for (const auto& conn : connections) {
                    std::cout << std::setw(8) << conn.protocol << " | "
                              << std::setw(20)
                              << (conn.localAddress + ":" +
                                  std::to_string(conn.localPort))
                              << " | " << std::setw(20)
                              << (conn.remoteAddress + ":" +
                                  std::to_string(conn.remotePort))
                              << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cout << "Error getting network connections: " << e.what()
                      << std::endl;
        }

        // 6. Network Interface Control (Demonstration Only)
        printSection("Network Interface Control (Safe Mode)");

        std::cout
            << "⚠️  Interface control operations require elevated privileges"
            << std::endl;
        std::cout << "This section demonstrates the API without actually "
                     "modifying interfaces\n"
                  << std::endl;

        if (!interfaces.empty()) {
            std::string testInterface = interfaces[0].getName();
            std::cout << "Example operations for interface '" << testInterface
                      << "':" << std::endl;
            std::cout
                << "  - Enable interface: NetworkManager::enableInterface(\""
                << testInterface << "\")" << std::endl;
            std::cout
                << "  - Disable interface: NetworkManager::disableInterface(\""
                << testInterface << "\")" << std::endl;
            std::cout << "  - Get status: networkManager.getInterfaceStatus(\""
                      << testInterface << "\")" << std::endl;

            std::cout << "\nNote: These operations are not executed to avoid "
                         "disrupting network connectivity"
                      << std::endl;
        }

        // 7. DNS Server Management (Demonstration Only)
        printSection("DNS Server Management (Safe Mode)");

        std::cout << "⚠️  DNS server modification requires elevated privileges"
                  << std::endl;
        std::cout << "This section demonstrates the API without actually "
                     "modifying DNS settings\n"
                  << std::endl;

        std::cout << "Example DNS management operations:" << std::endl;
        std::cout << "  - Set DNS servers: "
                     "NetworkManager::setDNSServers({\"8.8.8.8\", \"8.8.4.4\"})"
                  << std::endl;
        std::cout
            << "  - Add DNS server: NetworkManager::addDNSServer(\"1.1.1.1\")"
            << std::endl;
        std::cout << "  - Remove DNS server: "
                     "NetworkManager::removeDNSServer(\"8.8.4.4\")"
                  << std::endl;

        std::cout << "\nNote: These operations are not executed to avoid "
                     "disrupting DNS resolution"
                  << std::endl;

        // 8. Connection Monitoring
        printSection("Connection Monitoring");

        std::cout << "Starting connection monitoring for 5 seconds..."
                  << std::endl;
        std::cout
            << "(This will monitor network interface status in the background)"
            << std::endl;

        try {
            networkManager.monitorConnectionStatus();
            std::this_thread::sleep_for(std::chrono::seconds(5));
            std::cout << "Connection monitoring completed" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Error during connection monitoring: " << e.what()
                      << std::endl;
        }

        std::cout << "\n=== Advanced Network Management Complete ==="
                  << std::endl;
        std::cout << "This example demonstrated:" << std::endl;
        std::cout << "- Network interface discovery and analysis" << std::endl;
        std::cout << "- Detailed interface information retrieval" << std::endl;
        std::cout << "- DNS management and resolution testing" << std::endl;
        std::cout << "- Interface status monitoring" << std::endl;
        std::cout << "- Network connection analysis" << std::endl;
        std::cout << "- Safe demonstration of control operations" << std::endl;
        std::cout << "- Background connection monitoring" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Network management error: " << e.what() << std::endl;
        std::cerr << "\nPossible causes:" << std::endl;
        std::cerr
            << "- Insufficient permissions (try running as administrator/root)"
            << std::endl;
        std::cerr << "- Network subsystem not available" << std::endl;
        std::cerr << "- Platform not supported" << std::endl;
        std::cerr << "- Network configuration issues" << std::endl;
        return 1;
    }

    return 0;
}
