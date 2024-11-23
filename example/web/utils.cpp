#include "atom/web/utils.hpp"

#include <iostream>

using namespace atom::web;

int main() {
    // Check if a port is in use
    int port = 8080;
    bool portInUse = isPortInUse(port);
    std::cout << "Port " << port << " is in use: " << std::boolalpha
              << portInUse << std::endl;

    // Check if there is any program running on the specified port and kill it
    // if found
    bool programKilled = checkAndKillProgramOnPort(port);
    std::cout << "Program on port " << port
              << " was terminated: " << std::boolalpha << programKilled
              << std::endl;

#if defined(__linux__) || defined(__APPLE__)
    // Get address information for a given hostname and service
    struct addrinfo* addrInfo = getAddrInfo("www.google.com", "http");
    if (addrInfo) {
        std::cout << "Address information retrieved successfully." << std::endl;

        // Convert address information to string
        std::string addrStr = addrInfoToString(addrInfo, true);
        std::cout << "Address information: " << addrStr << std::endl;

        // Dump address information from source to destination
        struct addrinfo* dst = nullptr;
        if (dumpAddrInfo(&dst, addrInfo) == 0) {
            std::cout << "Address information dumped successfully."
                      << std::endl;
            freeAddrInfo(dst);
        } else {
            std::cout << "Failed to dump address information." << std::endl;
        }

        // Compare two address information structures
        struct addrinfo* addrInfo2 = getAddrInfo("www.google.com", "http");
        bool addrInfoEqual = compareAddrInfo(addrInfo, addrInfo2);
        std::cout << "Address information structures are equal: "
                  << std::boolalpha << addrInfoEqual << std::endl;
        freeAddrInfo(addrInfo2);

        // Filter address information by family
        struct addrinfo* filtered = filterAddrInfo(addrInfo, AF_INET);
        if (filtered) {
            std::cout << "Filtered address information retrieved successfully."
                      << std::endl;
            freeAddrInfo(filtered);
        } else {
            std::cout << "No address information matched the filter."
                      << std::endl;
        }

        // Sort address information by family
        struct addrinfo* sorted = sortAddrInfo(addrInfo);
        if (sorted) {
            std::cout << "Sorted address information retrieved successfully."
                      << std::endl;
            freeAddrInfo(sorted);
        } else {
            std::cout << "Failed to sort address information." << std::endl;
        }

        // Free address information
        freeAddrInfo(addrInfo);
    } else {
        std::cout << "Failed to retrieve address information." << std::endl;
    }
#endif

    return 0;
}