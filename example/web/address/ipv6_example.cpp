/*
 * ipv6_example.cpp
 *
 * Copyright (C) 2025 Developers <example.com>
 *
 * A comprehensive example demonstrating the use of the Atom IPv6 address class
 */

#include "atom/log/atomlog.hpp"
#include "atom/web/address.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace atom::web;

void demonstrateBasicIPv6Operations() {
    std::cout << "\n=== Basic IPv6 Operations ===\n";

    try {
        // Create IPv6 addresses
        IPv6 addr1("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
        IPv6 addr2("::1");      // Loopback
        IPv6 addr3("fe80::1");  // Link-local
        IPv6 addr4;             // Default constructor

        std::cout << "Created IPv6 addresses:\n";
        std::cout << "  addr1: " << addr1.getAddress() << "\n";
        std::cout << "  addr2: " << addr2.getAddress() << " (loopback)\n";
        std::cout << "  addr3: " << addr3.getAddress() << " (link-local)\n";

        // Parse an address into addr4
        if (addr4.parse("2001:db8::1")) {
            std::cout << "  addr4: " << addr4.getAddress() << " (parsed)\n";
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

void demonstrateIPv6Validation() {
    std::cout << "\n=== IPv6 Validation ===\n";

    std::vector<std::string> testAddresses = {
        "2001:0db8:85a3:0000:0000:8a2e:0370:7334",  // Valid - full form
        "2001:db8:85a3::8a2e:370:7334",             // Valid - compressed
        "::1",                                      // Valid - loopback
        "fe80::1",                                  // Valid - link-local
        "::",                                       // Valid - all zeros
        "2001:db8::1",                              // Valid - compressed
        "::ffff:192.0.2.1",                         // Valid - IPv4-mapped
        "2001:0db8:85a3::8a2e:370g:7334",           // Invalid - 'g' not hex
        "2001:0db8:85a3:0000:0000:8a2e:0370:7334:extra",  // Invalid - too many
                                                          // groups
        "2001::85a3::7334",     // Invalid - double compression
        "192.168.1.1",          // Invalid - IPv4 address
        "not:an:ipv6:address",  // Invalid - non-hex
        ""                      // Invalid - empty
    };

    for (const auto& addr : testAddresses) {
        try {
            IPv6 ipv6;
            bool valid = ipv6.parse(addr);
            std::cout << "  " << (addr.empty() ? "(empty)" : addr) << " -> "
                      << (valid ? "VALID" : "INVALID") << "\n";
        } catch (const InvalidAddressFormat& e) {
            std::cout << "  " << (addr.empty() ? "(empty)" : addr)
                      << " -> INVALID (exception)\n";
        }
    }
}

void demonstrateIPv6Conversions() {
    std::cout << "\n=== IPv6 Conversions ===\n";

    try {
        std::vector<std::string> addresses = {"2001:db8::1", "::1", "fe80::1",
                                              "::"};

        std::cout << "IPv6 conversion examples:\n";
        std::cout << "Address                    | Binary (first 32 bits)      "
                     "    | Hexadecimal\n";
        std::cout << "---------------------------|-----------------------------"
                     "-----|------------\n";

        for (const auto& ip : addresses) {
            IPv6 addr(ip);
            std::string binary = addr.toBinary();
            std::string hex = addr.toHex();

            // Show only first 32 bits of binary for readability
            std::string shortBinary = binary.substr(0, 32) + "...";

            std::cout << std::left << std::setw(26) << ip << " | "
                      << std::setw(32) << shortBinary << " | " << hex << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in conversions: " << e.what() << "\n";
    }
}

void demonstrateIPv6RangeOperations() {
    std::cout << "\n=== IPv6 Range Operations ===\n";

    try {
        IPv6 addr("2001:db8::100");

        // Test if address is in various ranges
        std::vector<std::pair<std::string, std::string>> ranges = {
            {"2001:db8::", "2001:db8::ffff"},  // Should be in range
            {"2001:db8::1", "2001:db8::200"},  // Should be in range
            {"fe80::", "fe80::ffff"},          // Should NOT be in range
            {"::", "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"}
            // Should be in range (entire IPv6 space)
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

void demonstrateIPv6SubnetOperations() {
    std::cout << "\n=== IPv6 Subnet Operations ===\n";

    try {
        IPv6 addr("2001:db8:85a3::8a2e:370:7334");

        // Test with different prefix lengths (IPv6 uses prefix lengths instead
        // of subnet masks)
        std::vector<std::string> prefixes = {
            "ffff:ffff:ffff:ffff::",  // /64
            "ffff:ffff:ffff::",       // /48
            "ffff:ffff::",            // /32
            "ffff::"                  // /16
        };

        std::cout << "Subnet operations for " << addr.getAddress() << ":\n";
        std::cout << "Prefix               | Network Address\n";
        std::cout
            << "---------------------|----------------------------------\n";

        for (const auto& prefix : prefixes) {
            std::string network = addr.getNetworkAddress(prefix);

            std::cout << std::left << std::setw(20) << prefix << " | "
                      << network << "\n";
        }

        // Test subnet membership
        std::cout << "\nSubnet membership tests:\n";
        IPv6 addr1("2001:db8:85a3::1");
        IPv6 addr2("2001:db8:85a3::ffff");
        IPv6 addr3("2001:db8:85a4::1");

        std::string prefix = "ffff:ffff:ffff::";  // /48

        std::cout << "Using prefix " << prefix << " (/48):\n";
        std::cout << "  " << addr.getAddress() << " and " << addr1.getAddress()
                  << " same subnet: "
                  << (addr.isSameSubnet(addr1, prefix) ? "YES" : "NO") << "\n";
        std::cout << "  " << addr.getAddress() << " and " << addr2.getAddress()
                  << " same subnet: "
                  << (addr.isSameSubnet(addr2, prefix) ? "YES" : "NO") << "\n";
        std::cout << "  " << addr.getAddress() << " and " << addr3.getAddress()
                  << " same subnet: "
                  << (addr.isSameSubnet(addr3, prefix) ? "YES" : "NO") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in subnet operations: " << e.what() << "\n";
    }
}

void demonstrateIPv6CIDROperations() {
    std::cout << "\n=== IPv6 CIDR Operations ===\n";

    try {
        IPv6 addr;

        // Test CIDR parsing
        std::vector<std::string> cidrNotations = {
            "2001:db8::/32", "2001:db8:85a3::/48", "fe80::/64", "::1/128",
            "2001:db8:85a3:8d3:1319:8a2e:370:7344/64"};

        std::cout << "CIDR parsing examples:\n";

        for (const auto& cidr : cidrNotations) {
            if (addr.parseCIDR(cidr)) {
                std::cout << "  " << cidr << " -> " << addr.getAddress()
                          << "\n";

                // Get prefix length
                auto prefixLength = IPv6::getPrefixLength(cidr);
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

void demonstrateIPv6Comparison() {
    std::cout << "\n=== IPv6 Comparison ===\n";

    try {
        IPv6 addr1("2001:db8::1");
        IPv6 addr2(
            "2001:0db8:0000:0000:0000:0000:0000:0001");  // Same as addr1
                                                         // (expanded form)
        IPv6 addr3("2001:db8::2");                       // Different from addr1

        std::cout << "Comparing IPv6 addresses:\n";
        std::cout << "  " << addr1.getAddress() << " == " << addr2.getAddress()
                  << ": " << (addr1.isEqual(addr2) ? "YES" : "NO") << "\n";
        std::cout << "  " << addr1.getAddress() << " == " << addr3.getAddress()
                  << ": " << (addr1.isEqual(addr3) ? "YES" : "NO") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in comparison: " << e.what() << "\n";
    }
}

void demonstrateIPv6SpecialAddresses() {
    std::cout << "\n=== IPv6 Special Addresses ===\n";

    std::vector<std::pair<std::string, std::string>> specialAddresses = {
        {"::", "Unspecified address"},
        {"::1", "Loopback address"},
        {"fe80::", "Link-local unicast prefix"},
        {"ff00::", "Multicast prefix"},
        {"2001:db8::", "Documentation prefix"},
        {"::ffff:0:0", "IPv4-mapped IPv6 prefix"}};

    std::cout << "Special IPv6 addresses:\n";
    std::cout << "Address              | Description\n";
    std::cout << "---------------------|---------------------------\n";

    for (const auto& [addr, desc] : specialAddresses) {
        try {
            IPv6 ipv6(addr);
            std::cout << std::left << std::setw(20) << addr << " | " << desc
                      << "\n";
        } catch (const std::exception& e) {
            std::cout << std::left << std::setw(20) << addr
                      << " | ERROR: " << e.what() << "\n";
        }
    }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
    std::cout << "============================================\n";
    std::cout << "        ATOM IPv6 ADDRESS DEMO              \n";
    std::cout << "============================================\n";

    try {
        demonstrateBasicIPv6Operations();
        demonstrateIPv6Validation();
        demonstrateIPv6Conversions();
        demonstrateIPv6RangeOperations();
        demonstrateIPv6SubnetOperations();
        demonstrateIPv6CIDROperations();
        demonstrateIPv6Comparison();
        demonstrateIPv6SpecialAddresses();

        std::cout << "\n============================================\n";
        std::cout << "        IPv6 DEMO COMPLETED                 \n";
        std::cout << "============================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
