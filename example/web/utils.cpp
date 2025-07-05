/*
 * network_utils_example.cpp
 *
 * Copyright (C) 2025 Developers <example.com>
 *
 * A comprehensive example demonstrating the use of the Atom Network Utilities
 */

#include "atom/web/utils.hpp"
#include "atom/log/loguru.hpp"

#include <chrono>
#include <iostream>
#include <thread>

// Function to print a vector of strings (useful for displaying IP addresses and
// port lists)
void printVector(const std::vector<std::string>& vec,
                 const std::string& label) {
    std::cout << label << ":\n";
    if (vec.empty()) {
        std::cout << "  (empty)\n";
    } else {
        for (const auto& item : vec) {
            std::cout << "  - " << item << "\n";
        }
    }
    std::cout << std::endl;
}

// Function to print a vector of ports
void printPorts(const std::vector<uint16_t>& ports, const std::string& label) {
    std::cout << label << ":\n";
    if (ports.empty()) {
        std::cout << "  (no open ports found)\n";
    } else {
        for (const auto& port : ports) {
            std::cout << "  - " << port << "\n";
        }
    }
    std::cout << std::endl;
}

int main(int argc, char** argv) {
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("network_utils_example.log", loguru::Append,
                     loguru::Verbosity_MAX);

    LOG_F(INFO, "Network Utils Example Application Starting");

    try {
        std::cout << "============================================\n";
        std::cout << "        ATOM NETWORK UTILITIES DEMO         \n";
        std::cout << "============================================\n\n";

        // PART 1: Initialize Windows Socket API (only needed on Windows)
        std::cout << "INITIALIZING NETWORK SUBSYSTEM...\n";
        bool initialized = atom::web::initializeWindowsSocketAPI();
        std::cout << "Network subsystem initialization: "
                  << (initialized ? "SUCCESS" : "FAILED OR NOT NEEDED")
                  << "\n\n";

        // PART 2: IP Address Resolution
        std::cout << "============================================\n";
        std::cout << "           IP ADDRESS RESOLUTION            \n";
        std::cout << "============================================\n\n";

        // Example 1: Get IP addresses for a domain
        std::cout << "Resolving IP addresses for 'github.com'...\n";
        auto githubIps = atom::web::getIPAddresses("github.com");
        printVector(githubIps, "GitHub IP Addresses");

        // Example 2: Get local IP addresses
        std::cout << "Getting local IP addresses...\n";
        auto localIps = atom::web::getLocalIPAddresses();
        printVector(localIps, "Local IP Addresses");

        // Example 3: Validate IP addresses
        std::vector<std::string> testIps = {
            "192.168.1.1",
            "256.1.1.1",  // invalid
            "2001:0db8:85a3:0000:0000:8a2e:0370:7334",
            "not-an-ip"  // invalid
        };

        // PART 3: Port Operations
        std::cout << "============================================\n";
        std::cout << "             PORT OPERATIONS                \n";
        std::cout << "============================================\n\n";

        // Example 1: Check if a port is in use
        uint16_t testPort = 8080;
        std::cout << "Checking if port " << testPort << " is in use...\n";
        bool portInUse = atom::web::isPortInUse(testPort);
        std::cout << "Port " << testPort << " is "
                  << (portInUse ? "in use" : "not in use") << "\n\n";

        // Example 2: Asynchronously check multiple ports
        std::cout << "Asynchronously checking multiple ports...\n";
        std::vector<uint16_t> portsToCheck = {80, 443, 3306, 5432, 27017};
        std::vector<std::future<bool>> futures;

        for (auto port : portsToCheck) {
            futures.push_back(atom::web::isPortInUseAsync(port));
        }

        for (size_t i = 0; i < portsToCheck.size(); ++i) {
            std::cout << "Port " << portsToCheck[i] << " is "
                      << (futures[i].get() ? "in use" : "not in use") << "\n";
        }
        std::cout << std::endl;

        // Example 3: Get process ID on port (if port is in use)
        if (portInUse) {
            std::cout << "Getting process ID on port " << testPort << "...\n";
            auto pid = atom::web::getProcessIDOnPort(testPort);
            if (pid) {
                std::cout << "Process ID on port " << testPort << ": " << *pid
                          << "\n";

                // Example 4: Check and kill program on port (commented out for
                // safety) std::cout << "Attempting to kill program on port " <<
                // testPort << "...\n"; bool killed =
                // atom::web::checkAndKillProgramOnPort(testPort); std::cout <<
                // "Kill attempt " << (killed ? "succeeded" : "failed") << "\n";
            } else {
                std::cout << "No process found on port " << testPort << "\n";
            }
        }
        std::cout << std::endl;

        // PART 4: Port Scanning
        std::cout << "============================================\n";
        std::cout << "              PORT SCANNING                 \n";
        std::cout << "============================================\n\n";

        // Example 1: Scan a single port
        std::string hostToScan = "example.com";
        uint16_t portToScan = 80;  // HTTP port
        std::cout << "Scanning port " << portToScan << " on " << hostToScan
                  << "...\n";
        bool portOpen = atom::web::scanPort(hostToScan, portToScan);
        std::cout << "Port " << portToScan << " is "
                  << (portOpen ? "open" : "closed") << " on " << hostToScan
                  << "\n\n";

        // Example 2: Scan a range of ports (using a small range for demo
        // purposes)
        uint16_t startPort = 79;
        uint16_t endPort = 85;
        std::cout << "Scanning ports " << startPort << "-" << endPort << " on "
                  << hostToScan << "...\n";
        auto openPorts =
            atom::web::scanPortRange(hostToScan, startPort, endPort);
        printPorts(openPorts, "Open Ports");

        // Example 3: Asynchronous port scanning
        std::cout << "Starting asynchronous port scan " << startPort << "-"
                  << endPort << " on " << hostToScan << "...\n";
        auto futurePortScan =
            atom::web::scanPortRangeAsync(hostToScan, startPort, endPort);

        std::cout << "Doing other work while scan is in progress...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::cout << "Retrieving asynchronous scan results...\n";
        auto asyncOpenPorts = futurePortScan.get();
        printPorts(asyncOpenPorts, "Open Ports (Async Scan)");

        // PART 5: Internet Connectivity Check
        std::cout << "============================================\n";
        std::cout << "         INTERNET CONNECTIVITY CHECK        \n";
        std::cout << "============================================\n\n";

        std::cout << "Checking internet connectivity...\n";
        bool hasInternet = atom::web::checkInternetConnectivity();
        std::cout << "Internet connectivity: "
                  << (hasInternet ? "AVAILABLE" : "NOT AVAILABLE") << "\n\n";

        // PART 6: Advanced Address Info Operations
        std::cout << "============================================\n";
        std::cout << "      ADVANCED ADDRESS INFO OPERATIONS      \n";
        std::cout << "============================================\n\n";

#if defined(__linux__) || defined(__APPLE__)
        // These functions are only available on Linux and macOS

        // Example 1: Get address info
        std::string hostname = "github.com";
        std::string service = "443";
        std::cout << "Getting address info for " << hostname << ":" << service
                  << "...\n";
        try {
            auto addrInfo = atom::web::getAddrInfo(hostname, service);

            // Example 2: Convert address info to string
            std::cout << "Address info as text:\n";
            std::cout << atom::web::addrInfoToString(addrInfo.get(), false)
                      << "\n";

            // Example 3: Convert address info to JSON
            std::cout << "Address info as JSON:\n";
            std::cout << atom::web::addrInfoToString(addrInfo.get(), true)
                      << "\n";

            // Example 4: Filter address info by family
            std::cout << "Filtering for IPv4 addresses only...\n";
            auto filteredInfo =
                atom::web::filterAddrInfo(addrInfo.get(), AF_INET);
            if (filteredInfo) {
                std::cout << "IPv4 addresses:\n";
                std::cout << atom::web::addrInfoToString(filteredInfo.get(),
                                                         false)
                          << "\n";
            } else {
                std::cout << "No IPv4 addresses found.\n";
            }

            // Example 5: Sort address info
            std::cout << "Sorting address info...\n";
            auto sortedInfo = atom::web::sortAddrInfo(addrInfo.get());
            if (sortedInfo) {
                std::cout << "Sorted address info:\n";
                std::cout << atom::web::addrInfoToString(sortedInfo.get(),
                                                         false)
                          << "\n";
            } else {
                std::cout << "Failed to sort address info.\n";
            }

            // Example 6: Compare address info
            if (addrInfo->ai_next != nullptr) {
                std::cout << "Comparing two address info entries...\n";
                bool areEqual = atom::web::compareAddrInfo(addrInfo.get(),
                                                           addrInfo->ai_next);
                std::cout << "Address info entries are "
                          << (areEqual ? "equal" : "different") << "\n\n";
            }

            // Example 7: Dump address info
            std::cout << "Dumping address info to a new structure...\n";
            std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>
                dstAddrInfo(nullptr, ::freeaddrinfo);
            int dumpResult =
                atom::web::dumpAddrInfo(dstAddrInfo, addrInfo.get());
            if (dumpResult == 0) {
                std::cout << "Address info dump successful\n";
                std::cout << "Dumped address info:\n";
                std::cout << atom::web::addrInfoToString(dstAddrInfo.get(),
                                                         false)
                          << "\n";
            } else {
                std::cout << "Address info dump failed with code: "
                          << dumpResult << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
#else
        std::cout << "Advanced address info operations are only available on "
                     "Linux and macOS.\n\n";
#endif

        std::cout << "============================================\n";
        std::cout << "        NETWORK UTILS DEMO COMPLETED        \n";
        std::cout << "============================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        LOG_F(ERROR, "Exception: %s", e.what());
        return 1;
    }

    LOG_F(INFO, "Network Utils Example Application Completed Successfully");
    return 0;
}
