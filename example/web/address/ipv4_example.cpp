/*
 * ipv4_example.cpp
 *
 * Copyright (C) 2025 Developers <example.com>
 *
 * A comprehensive example demonstrating the use of the Atom IPv4 address class
 */

#include "atom/log/loguru.hpp"
#include "atom/web/address.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace atom::web;

void demonstrateBasicIPv4Operations() {
    std::cout << "\n=== Basic IPv4 Operations ===\n";

    try {
        // Create IPv4 addresses
        IPv4 addr1("192.168.1.1");
        IPv4 addr2("10.0.0.1");
        IPv4 addr3;  // Default constructor

        std::cout << "Created IPv4 addresses:\n";
        std::cout << "  addr1: " << addr1.getAddress() << "\n";
        std::cout << "  addr2: " << addr2.getAddress() << "\n";

        // Parse an address into addr3
        if (addr3.parse("172.16.0.1")) {
            std::cout << "  addr3: " << addr3.getAddress() << " (parsed)\n";
        }

        // Print address types
        addr1.printAddressType();

        // Get address type
        std::cout << "Address type: " << addr1.getType() << "\n";

    } catch (const InvalidAddressFormat& e) {
        std::cerr << "Invalid address format: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

void demonstrateIPv4Validation() {
    std::cout << "\n=== IPv4 Validation ===\n";

    std::vector<std::string> testAddresses = {
        "192.168.1.1",      // Valid
        "10.0.0.1",         // Valid
        "255.255.255.255",  // Valid
        "0.0.0.0",          // Valid
        "256.1.1.1",        // Invalid - octet > 255
        "192.168.1",        // Invalid - incomplete
        "192.168.1.1.1",    // Invalid - too many octets
        "192.168.-1.1",     // Invalid - negative octet
        "not.an.ip.addr",   // Invalid - non-numeric
        ""                  // Invalid - empty
    };

    for (const auto& addr : testAddresses) {
        try {
            IPv4 ipv4;
            bool valid = ipv4.parse(addr);
            std::cout << "  " << (addr.empty() ? "(empty)" : addr) << " -> "
                      << (valid ? "VALID" : "INVALID") << "\n";
        } catch (const InvalidAddressFormat& e) {
            std::cout << "  " << (addr.empty() ? "(empty)" : addr)
                      << " -> INVALID (exception: " << e.what() << ")\n";
        }
    }
}

void demonstrateIPv4Conversions() {
    std::cout << "\n=== IPv4 Conversions ===\n";

    try {
        IPv4 addr("192.168.1.100");

        // Convert to binary representation
        std::string binary = addr.toBinary();
        std::cout << "Binary representation: " << binary << "\n";

        // Convert to hexadecimal
        std::string hex = addr.toHex();
        std::cout << "Hexadecimal representation: " << hex << "\n";

        // Demonstrate with different addresses
        std::vector<std::string> addresses = {"127.0.0.1", "255.255.255.255",
                                              "0.0.0.0", "10.0.0.1"};

        std::cout << "\nConversion table:\n";
        std::cout << "IP Address       | Binary                           | "
                     "Hexadecimal\n";
        std::cout << "-----------------|----------------------------------|----"
                     "--------\n";

        for (const auto& ip : addresses) {
            IPv4 addr(ip);
            std::cout << std::left << std::setw(16) << ip << " | "
                      << std::setw(32) << addr.toBinary() << " | "
                      << addr.toHex() << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in conversions: " << e.what() << "\n";
    }
}

void demonstrateIPv4RangeOperations() {
    std::cout << "\n=== IPv4 Range Operations ===\n";

    try {
        IPv4 addr("192.168.1.100");

        // Test if address is in various ranges
        std::vector<std::pair<std::string, std::string>> ranges = {
            {"192.168.1.1", "192.168.1.255"},  // Should be in range
            {"192.168.0.1", "192.168.0.255"},  // Should NOT be in range
            {"10.0.0.1", "10.255.255.255"},    // Should NOT be in range
            {"0.0.0.0", "255.255.255.255"}
            // Should be in range (entire IPv4 space)
        };

        std::cout << "Testing if " << addr.getAddress() << " is in ranges:\n";

        for (const auto& [start, end] : ranges) {
            bool inRange = addr.isInRange(start, end);
            std::cout << "  [" << start << " - " << end
                      << "]: " << (inRange ? "YES" : "NO") << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in range operations: " << e.what() << "\n";
    }
}

void demonstrateIPv4SubnetOperations() {
    std::cout << "\n=== IPv4 Subnet Operations ===\n";

    try {
        IPv4 addr("192.168.1.100");

        // Test with different subnet masks
        std::vector<std::string> masks = {
            "255.255.255.0",   // /24
            "255.255.0.0",     // /16
            "255.0.0.0",       // /8
            "255.255.255.128"  // /25
        };

        std::cout << "Subnet operations for " << addr.getAddress() << ":\n";
        std::cout << "Mask             | Network Address | Broadcast Address\n";
        std::cout << "-----------------|-----------------|------------------\n";

        for (const auto& mask : masks) {
            std::string network = addr.getNetworkAddress(mask);
            std::string broadcast = addr.getBroadcastAddress(mask);

            std::cout << std::left << std::setw(16) << mask << " | "
                      << std::setw(15) << network << " | " << broadcast << "\n";
        }

        // Test subnet membership
        std::cout << "\nSubnet membership tests:\n";
        IPv4 addr1("192.168.1.50");
        IPv4 addr2("192.168.1.200");
        IPv4 addr3("192.168.2.50");

        std::string mask = "255.255.255.0";

        std::cout << "Using mask " << mask << ":\n";
        std::cout << "  " << addr.getAddress() << " and " << addr1.getAddress()
                  << " same subnet: "
                  << (addr.isSameSubnet(addr1, mask) ? "YES" : "NO") << "\n";
        std::cout << "  " << addr.getAddress() << " and " << addr2.getAddress()
                  << " same subnet: "
                  << (addr.isSameSubnet(addr2, mask) ? "YES" : "NO") << "\n";
        std::cout << "  " << addr.getAddress() << " and " << addr3.getAddress()
                  << " same subnet: "
                  << (addr.isSameSubnet(addr3, mask) ? "YES" : "NO") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in subnet operations: " << e.what() << "\n";
    }
}

void demonstrateIPv4CIDROperations() {
    std::cout << "\n=== IPv4 CIDR Operations ===\n";

    try {
        IPv4 addr;

        // Test CIDR parsing
        std::vector<std::string> cidrNotations = {
            "192.168.1.0/24", "10.0.0.0/8", "172.16.0.0/16", "192.168.1.128/25",
            "203.0.113.0/24"};

        std::cout << "CIDR parsing examples:\n";

        for (const auto& cidr : cidrNotations) {
            if (addr.parseCIDR(cidr)) {
                std::cout << "  " << cidr << " -> " << addr.getAddress()
                          << "\n";

                // Get prefix length
                auto prefixLength = IPv4::getPrefixLength(cidr);
                if (prefixLength) {
                    std::cout << "    Prefix length: /" << *prefixLength
                              << "\n";
                }
            } else {
                std::cout << "  " << cidr << " -> FAILED TO PARSE\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in CIDR operations: " << e.what() << "\n";
    }
}

void demonstrateIPv4Comparison() {
    std::cout << "\n=== IPv4 Comparison ===\n";

    try {
        IPv4 addr1("192.168.1.1");
        IPv4 addr2("192.168.1.1");  // Same as addr1
        IPv4 addr3("192.168.1.2");  // Different from addr1

        std::cout << "Comparing IPv4 addresses:\n";
        std::cout << "  " << addr1.getAddress() << " == " << addr2.getAddress()
                  << ": " << (addr1.isEqual(addr2) ? "YES" : "NO") << "\n";
        std::cout << "  " << addr1.getAddress() << " == " << addr3.getAddress()
                  << ": " << (addr1.isEqual(addr3) ? "YES" : "NO") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in comparison: " << e.what() << "\n";
    }
}

int main(int argc, char** argv) {
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("ipv4_example.log", loguru::Append, loguru::Verbosity_MAX);

    std::cout << "============================================\n";
    std::cout << "        ATOM IPv4 ADDRESS DEMO              \n";
    std::cout << "============================================\n";

    try {
        demonstrateBasicIPv4Operations();
        demonstrateIPv4Validation();
        demonstrateIPv4Conversions();
        demonstrateIPv4RangeOperations();
        demonstrateIPv4SubnetOperations();
        demonstrateIPv4CIDROperations();
        demonstrateIPv4Comparison();

        std::cout << "\n============================================\n";
        std::cout << "        IPv4 DEMO COMPLETED                 \n";
        std::cout << "============================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
