/*
 * address_factory_example.cpp
 *
 * Copyright (C) 2025 Developers <example.com>
 *
 * A comprehensive example demonstrating the use of the Atom Address factory
 * and polymorphic address handling
 */

#include "atom/log/loguru.hpp"
#include "atom/web/address.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace atom::web;

void demonstrateAddressFactory() {
    std::cout << "\n=== Address Factory Demonstration ===\n";

    std::vector<std::string> testAddresses = {
        "192.168.1.1",  // IPv4
        "10.0.0.1",     // IPv4
        "2001:db8::1",  // IPv6
        "::1",          // IPv6 loopback
        "fe80::1",      // IPv6 link-local
#ifdef _WIN32
        "\\\\.\\pipe\\test_pipe",     // Windows Named Pipe
        "\\\\.\\pipe\\another_pipe",  // Windows Named Pipe
#else
        "/tmp/socket1",      // Unix Domain Socket
        "/var/run/socket2",  // Unix Domain Socket
#endif
        "invalid.address.format",  // Invalid
        "256.1.1.1",               // Invalid IPv4
        "2001::85a3::7334",        // Invalid IPv6
        ""                         // Empty
    };

    std::cout << "Testing automatic address type detection:\n";
    std::cout << "Address                        | Type        | Status\n";
    std::cout << "-------------------------------|-------------|--------\n";

    for (const auto& addrStr : testAddresses) {
        try {
            auto address = Address::createFromString(addrStr);

            std::string displayAddr =
                addrStr.empty()
                    ? "(empty)"
                    : (addrStr.length() > 30 ? addrStr.substr(0, 27) + "..."
                                             : addrStr);

            if (address) {
                std::cout << std::left << std::setw(30) << displayAddr << " | "
                          << std::setw(11) << address->getType()
                          << " | SUCCESS\n";
            } else {
                std::cout << std::left << std::setw(30) << displayAddr << " | "
                          << std::setw(11) << "Unknown" << " | FAILED\n";
            }
        } catch (const std::exception& e) {
            std::string displayAddr =
                addrStr.empty()
                    ? "(empty)"
                    : (addrStr.length() > 30 ? addrStr.substr(0, 27) + "..."
                                             : addrStr);
            std::cout << std::left << std::setw(30) << displayAddr << " | "
                      << std::setw(11) << "Error" << " | EXCEPTION\n";
        }
    }
}

void demonstratePolymorphicOperations() {
    std::cout << "\n=== Polymorphic Address Operations ===\n";

    std::vector<std::unique_ptr<Address>> addresses;

    try {
        // Create different types of addresses
        addresses.push_back(std::make_unique<IPv4>("192.168.1.100"));
        addresses.push_back(std::make_unique<IPv6>("2001:db8::100"));
#ifdef _WIN32
        addresses.push_back(std::make_unique<UnixDomain>("\\\\.\\pipe\\test"));
#else
        addresses.push_back(std::make_unique<UnixDomain>("/tmp/test_socket"));
#endif

        std::cout << "Demonstrating polymorphic operations:\n";

        for (size_t i = 0; i < addresses.size(); ++i) {
            const auto& addr = addresses[i];

            std::cout << "\nAddress " << (i + 1) << ":\n";
            std::cout << "  Address: " << addr->getAddress() << "\n";
            std::cout << "  Type: " << addr->getType() << "\n";

            // Print address type (virtual function call)
            std::cout << "  ";
            addr->printAddressType();

            // Convert to binary and hex
            std::cout << "  Binary: " << addr->toBinary().substr(0, 32)
                      << "...\n";
            std::cout << "  Hex: " << addr->toHex().substr(0, 20) << "...\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in polymorphic operations: " << e.what() << "\n";
    }
}

void demonstrateAddressComparison() {
    std::cout << "\n=== Address Comparison ===\n";

    try {
        // Create pairs of addresses for comparison
        std::vector<
            std::pair<std::unique_ptr<Address>, std::unique_ptr<Address>>>
            addressPairs;

        // Same IPv4 addresses
        addressPairs.emplace_back(std::make_unique<IPv4>("192.168.1.1"),
                                  std::make_unique<IPv4>("192.168.1.1"));

        // Different IPv4 addresses
        addressPairs.emplace_back(std::make_unique<IPv4>("192.168.1.1"),
                                  std::make_unique<IPv4>("192.168.1.2"));

        // Same IPv6 addresses (different representations)
        addressPairs.emplace_back(
            std::make_unique<IPv6>("2001:db8::1"),
            std::make_unique<IPv6>("2001:0db8:0000:0000:0000:0000:0000:0001"));

        // Different types (IPv4 vs IPv6)
        addressPairs.emplace_back(std::make_unique<IPv4>("192.168.1.1"),
                                  std::make_unique<IPv6>("::1"));

        std::cout << "Address comparison results:\n";

        for (size_t i = 0; i < addressPairs.size(); ++i) {
            const auto& [addr1, addr2] = addressPairs[i];

            bool areEqual = addr1->isEqual(*addr2);

            std::cout << "  Pair " << (i + 1) << ":\n";
            std::cout << "    " << addr1->getAddress() << " ("
                      << addr1->getType() << ")\n";
            std::cout << "    " << addr2->getAddress() << " ("
                      << addr2->getType() << ")\n";
            std::cout << "    Equal: " << (areEqual ? "YES" : "NO") << "\n\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in address comparison: " << e.what() << "\n";
    }
}

void demonstrateConvenienceFunction() {
    std::cout << "\n=== Convenience Function Usage ===\n";

    std::vector<std::string> testAddresses = {"192.168.1.1", "2001:db8::1",
#ifdef _WIN32
                                              "\\\\.\\pipe\\test_pipe"
#else
                                              "/tmp/socket"
#endif
    };

    std::cout << "Using createAddress convenience function:\n";

    for (const auto& addrStr : testAddresses) {
        try {
            // Use the convenience function from main.hpp
            auto address = createAddress(addrStr);

            if (address) {
                std::cout << "  " << addrStr << " -> " << address->getType()
                          << " address created successfully\n";

                // Demonstrate some operations
                std::cout << "    Binary: " << address->toBinary().substr(0, 20)
                          << "...\n";
                std::cout << "    Hex: " << address->toHex().substr(0, 15)
                          << "...\n";
            } else {
                std::cout << "  " << addrStr
                          << " -> Failed to create address\n";
            }

        } catch (const std::exception& e) {
            std::cout << "  " << addrStr << " -> Exception: " << e.what()
                      << "\n";
        }
    }
}

void demonstrateAddressRangeChecking() {
    std::cout << "\n=== Cross-Type Range Checking ===\n";

    try {
        // Create different types of addresses
        auto ipv4Addr = std::make_unique<IPv4>("192.168.1.100");
        auto ipv6Addr = std::make_unique<IPv6>("2001:db8::100");

        std::cout << "Range checking examples:\n";

        // IPv4 range checking
        std::cout << "  IPv4 " << ipv4Addr->getAddress()
                  << " in range [192.168.1.1 - 192.168.1.255]: "
                  << (ipv4Addr->isInRange("192.168.1.1", "192.168.1.255")
                          ? "YES"
                          : "NO")
                  << "\n";

        std::cout << "  IPv4 " << ipv4Addr->getAddress()
                  << " in range [10.0.0.1 - 10.255.255.255]: "
                  << (ipv4Addr->isInRange("10.0.0.1", "10.255.255.255") ? "YES"
                                                                        : "NO")
                  << "\n";

        // IPv6 range checking
        std::cout << "  IPv6 " << ipv6Addr->getAddress()
                  << " in range [2001:db8:: - 2001:db8::ffff]: "
                  << (ipv6Addr->isInRange("2001:db8::", "2001:db8::ffff")
                          ? "YES"
                          : "NO")
                  << "\n";

        std::cout << "  IPv6 " << ipv6Addr->getAddress()
                  << " in range [fe80:: - fe80::ffff]: "
                  << (ipv6Addr->isInRange("fe80::", "fe80::ffff") ? "YES"
                                                                  : "NO")
                  << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in range checking: " << e.what() << "\n";
    }
}

void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling ===\n";

    std::vector<std::string> invalidAddresses = {
        "999.999.999.999",   // Invalid IPv4
        "2001::85a3::7334",  // Invalid IPv6 (double ::)
        "not.an.address",    // Invalid format
        "",                  // Empty string
#ifdef _WIN32
        "\\\\invalid\\pipe\\format",  // Invalid named pipe format
#else
        std::string(200, 'a'),  // Too long for Unix socket
#endif
    };

    std::cout << "Testing error handling with invalid addresses:\n";

    for (const auto& addr : invalidAddresses) {
        try {
            auto address = Address::createFromString(addr);

            std::string displayAddr =
                addr.empty()
                    ? "(empty)"
                    : (addr.length() > 30 ? addr.substr(0, 27) + "..." : addr);

            if (address) {
                std::cout << "  " << displayAddr
                          << " -> Unexpectedly succeeded as "
                          << address->getType() << "\n";
            } else {
                std::cout << "  " << displayAddr
                          << " -> Correctly failed (returned nullptr)\n";
            }

        } catch (const InvalidAddressFormat& e) {
            std::string displayAddr =
                addr.empty()
                    ? "(empty)"
                    : (addr.length() > 30 ? addr.substr(0, 27) + "..." : addr);
            std::cout << "  " << displayAddr
                      << " -> Correctly threw InvalidAddressFormat\n";
        } catch (const std::exception& e) {
            std::string displayAddr =
                addr.empty()
                    ? "(empty)"
                    : (addr.length() > 30 ? addr.substr(0, 27) + "..." : addr);
            std::cout << "  " << displayAddr
                      << " -> Threw exception: " << e.what() << "\n";
        }
    }
}

int main(int argc, char** argv) {
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("address_factory_example.log", loguru::Append,
                     loguru::Verbosity_MAX);

    std::cout << "============================================\n";
    std::cout << "     ATOM ADDRESS FACTORY DEMO             \n";
    std::cout << "============================================\n";

    try {
        demonstrateAddressFactory();
        demonstratePolymorphicOperations();
        demonstrateAddressComparison();
        demonstrateConvenienceFunction();
        demonstrateAddressRangeChecking();
        demonstrateErrorHandling();

        std::cout << "\n============================================\n";
        std::cout << "     ADDRESS FACTORY DEMO COMPLETED        \n";
        std::cout << "============================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
