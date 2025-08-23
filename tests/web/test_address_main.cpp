#include "atom/web/address.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

// Helper function for displaying results
void printLine() {
    std::cout << "--------------------------------------------------------"
              << std::endl;
}

void printHeader(const std::string& title) {
    printLine();
    std::cout << title << std::endl;
    printLine();
}

// Test function for address creation
bool testAddressCreation(const std::string& addressStr,
                         const std::string& expectedType) {
    try {
        auto address = atom::web::Address::createFromString(addressStr);
        if (!address) {
            std::cout << "Failed to create address from: " << addressStr
                      << std::endl;
            return false;
        }

        std::cout << "Created " << address->getType()
                  << " address: " << address->getAddress() << std::endl;
        return address->getType() == expectedType;
    } catch (const atom::web::AddressException& e) {
        std::cout << "Exception creating address: " << e.what() << std::endl;
        return false;
    }
}

// Test various operations on IPv4 addresses
void demonstrateIPv4Operations() {
    printHeader("IPv4 Address Operations");

    try {
        // Create an IPv4 address
        atom::web::IPv4 ip1("192.168.1.1");
        std::cout << "IPv4 address created: " << ip1.getAddress() << std::endl;

        // Print address type
        std::cout << "Address type: ";
        ip1.printAddressType();

        // Convert to binary and hex
        std::cout << "Binary representation: " << ip1.toBinary() << std::endl;
        std::cout << "Hex representation: " << ip1.toHex() << std::endl;

        // Test equality
        atom::web::IPv4 ip2("192.168.1.1");
        atom::web::IPv4 ip3("192.168.1.2");
        std::cout << "ip1 == ip2: " << (ip1.isEqual(ip2) ? "true" : "false")
                  << std::endl;
        std::cout << "ip1 == ip3: " << (ip1.isEqual(ip3) ? "true" : "false")
                  << std::endl;

        // Test range inclusion
        std::cout << "ip1 in range 192.168.1.0 - 192.168.1.255: "
                  << (ip1.isInRange("192.168.1.0", "192.168.1.255") ? "true"
                                                                    : "false")
                  << std::endl;
        std::cout << "ip1 in range 10.0.0.0 - 10.255.255.255: "
                  << (ip1.isInRange("10.0.0.0", "10.255.255.255") ? "true"
                                                                  : "false")
                  << std::endl;

        // Network operations
        std::string subnetMask = "255.255.255.0";
        std::cout << "Network address with mask " << subnetMask << ": "
                  << ip1.getNetworkAddress(subnetMask) << std::endl;
        std::cout << "Broadcast address with mask " << subnetMask << ": "
                  << ip1.getBroadcastAddress(subnetMask) << std::endl;

        // Subnet comparison
        atom::web::IPv4 ip4("192.168.1.100");
        atom::web::IPv4 ip5("192.168.2.1");
        std::cout << "ip1 and ip4 in same subnet with mask " << subnetMask
                  << ": "
                  << (ip1.isSameSubnet(ip4, subnetMask) ? "true" : "false")
                  << std::endl;
        std::cout << "ip1 and ip5 in same subnet with mask " << subnetMask
                  << ": "
                  << (ip1.isSameSubnet(ip5, subnetMask) ? "true" : "false")
                  << std::endl;

        // CIDR notation
        atom::web::IPv4 cidrIp;
        cidrIp.parseCIDR("192.168.1.0/24");
        std::cout << "CIDR address: " << cidrIp.getAddress() << std::endl;

        // Extract prefix length
        auto prefixLength = atom::web::IPv4::getPrefixLength("192.168.1.0/24");
        if (prefixLength) {
            std::cout << "Prefix length from CIDR: " << *prefixLength
                      << std::endl;
        }
    } catch (const atom::web::AddressException& e) {
        std::cout << "Exception during IPv4 operations: " << e.what()
                  << std::endl;
    }
}

// Test various operations on IPv6 addresses
void demonstrateIPv6Operations() {
    printHeader("IPv6 Address Operations");

    try {
        // Create an IPv6 address
        atom::web::IPv6 ip1("2001:db8::1");
        std::cout << "IPv6 address created: " << ip1.getAddress() << std::endl;

        // Print address type
        std::cout << "Address type: ";
        ip1.printAddressType();

        // Convert to binary and hex
        std::cout << "Hex representation: " << ip1.toHex() << std::endl;
        std::cout << "Binary representation (first 32 bits): "
                  << ip1.toBinary().substr(0, 32) << "..." << std::endl;

        // Test equality
        atom::web::IPv6 ip2("2001:db8::1");
        atom::web::IPv6 ip3("2001:db8::2");
        std::cout << "ip1 == ip2: " << (ip1.isEqual(ip2) ? "true" : "false")
                  << std::endl;
        std::cout << "ip1 == ip3: " << (ip1.isEqual(ip3) ? "true" : "false")
                  << std::endl;

        // Test range inclusion
        std::cout << "ip1 in range 2001:db8::0 - 2001:db8::ffff: "
                  << (ip1.isInRange("2001:db8::0", "2001:db8::ffff") ? "true"
                                                                     : "false")
                  << std::endl;
        std::cout << "ip1 in range 2001:db9::0 - 2001:db9::ffff: "
                  << (ip1.isInRange("2001:db9::0", "2001:db9::ffff") ? "true"
                                                                     : "false")
                  << std::endl;

        // Network operations
        std::string subnetMask = "ffff:ffff:ffff:ffff::";
        std::cout << "Network address with mask " << subnetMask << ": "
                  << ip1.getNetworkAddress(subnetMask) << std::endl;
        std::cout << "Broadcast address with mask " << subnetMask << ": "
                  << ip1.getBroadcastAddress(subnetMask) << std::endl;

        // Subnet comparison
        atom::web::IPv6 ip4("2001:db8::100");
        atom::web::IPv6 ip5("2001:db9::1");
        std::cout << "ip1 and ip4 in same subnet with mask " << subnetMask
                  << ": "
                  << (ip1.isSameSubnet(ip4, subnetMask) ? "true" : "false")
                  << std::endl;
        std::cout << "ip1 and ip5 in same subnet with mask " << subnetMask
                  << ": "
                  << (ip1.isSameSubnet(ip5, subnetMask) ? "true" : "false")
                  << std::endl;

        // CIDR notation
        atom::web::IPv6 cidrIp;
        cidrIp.parseCIDR("2001:db8::/64");
        std::cout << "CIDR address: " << cidrIp.getAddress() << std::endl;

        // Extract prefix length
        auto prefixLength = atom::web::IPv6::getPrefixLength("2001:db8::/64");
        if (prefixLength) {
            std::cout << "Prefix length from CIDR: " << *prefixLength
                      << std::endl;
        }

        // Validate IPv6 addresses
        std::cout << "Is '2001:db8::1' a valid IPv6? "
                  << (atom::web::IPv6::isValidIPv6("2001:db8::1") ? "Yes"
                                                                  : "No")
                  << std::endl;
        std::cout << "Is 'not-an-ipv6' a valid IPv6? "
                  << (atom::web::IPv6::isValidIPv6("not-an-ipv6") ? "Yes"
                                                                  : "No")
                  << std::endl;
    } catch (const atom::web::AddressException& e) {
        std::cout << "Exception during IPv6 operations: " << e.what()
                  << std::endl;
    }
}

// Test operations on Unix Domain socket addresses
void demonstrateUnixDomainOperations() {
    printHeader("Unix Domain Socket Operations");

    try {
        // Create a Unix domain socket address
#ifdef _WIN32
        atom::web::UnixDomain unixAddr("\\\\.\\pipe\\testpipe");
        std::cout << "Windows named pipe created: " << unixAddr.getAddress()
                  << std::endl;
#else
        atom::web::UnixDomain unixAddr("/tmp/test.sock");
        std::cout << "Unix domain socket created: " << unixAddr.getAddress()
                  << std::endl;
#endif

        // Print address type
        std::cout << "Address type: ";
        unixAddr.printAddressType();

        // Convert to binary and hex
        std::cout << "Hex representation (first 20 chars): "
                  << unixAddr.toHex().substr(0, 20) << "..." << std::endl;
        std::cout << "Binary representation (first 32 bits): "
                  << unixAddr.toBinary().substr(0, 32) << "..." << std::endl;

        // Test equality
#ifdef _WIN32
        atom::web::UnixDomain unixAddr2("\\\\.\\pipe\\testpipe");
        atom::web::UnixDomain unixAddr3("\\\\.\\pipe\\otherpipe");
#else
        atom::web::UnixDomain unixAddr2("/tmp/test.sock");
        atom::web::UnixDomain unixAddr3("/tmp/other.sock");
#endif
        std::cout << "unixAddr == unixAddr2: "
                  << (unixAddr.isEqual(unixAddr2) ? "true" : "false")
                  << std::endl;
        std::cout << "unixAddr == unixAddr3: "
                  << (unixAddr.isEqual(unixAddr3) ? "true" : "false")
                  << std::endl;

        // Test range inclusion (lexicographical for Unix domain sockets)
#ifdef _WIN32
        std::cout << "unixAddr in range '\\\\.\\\pipe\\a' - '\\\\.\\\pipe\\z': "
                  << (unixAddr.isInRange("\\\\.\\pipe\\a", "\\\\.\\pipe\\z")
                          ? "true"
                          : "false")
                  << std::endl;
#else
        std::cout << "unixAddr in range '/tmp/a.sock' - '/tmp/z.sock': "
                  << (unixAddr.isInRange("/tmp/a.sock", "/tmp/z.sock")
                          ? "true"
                          : "false")
                  << std::endl;
#endif

        // Demonstrate operations that don't make sense for Unix domain sockets
        std::cout << "Network address (not applicable): "
                  << unixAddr.getNetworkAddress("255.255.255.0") << std::endl;
        std::cout << "Broadcast address (not applicable): "
                  << unixAddr.getBroadcastAddress("255.255.255.0") << std::endl;
        std::cout << "Same subnet (not applicable): "
                  << (unixAddr.isSameSubnet(unixAddr2, "255.255.255.0")
                          ? "true"
                          : "false")
                  << std::endl;

        // Validate paths
#ifdef _WIN32
        std::cout << "Is '\\\\.\\pipe\\testpipe' a valid path? "
                  << (atom::web::UnixDomain::isValidPath(
                          "\\\\.\\pipe\\testpipe")
                          ? "Yes"
                          : "No")
                  << std::endl;
        std::cout << "Is 'invalid:path' a valid path? "
                  << (atom::web::UnixDomain::isValidPath("invalid:path") ? "Yes"
                                                                         : "No")
                  << std::endl;
#else
        std::cout << "Is '/tmp/test.sock' a valid path? "
                  << (atom::web::UnixDomain::isValidPath("/tmp/test.sock")
                          ? "Yes"
                          : "No")
                  << std::endl;
        std::cout << "Is '' a valid path? "
                  << (atom::web::UnixDomain::isValidPath("") ? "Yes" : "No")
                  << std::endl;
#endif
    } catch (const atom::web::AddressException& e) {
        std::cout << "Exception during Unix domain operations: " << e.what()
                  << std::endl;
    }
}

// Test address factory method
void demonstrateAddressFactory() {
    printHeader("Address Factory Method");

    std::vector<std::pair<std::string, std::string>> testAddresses = {
        {"192.168.1.1", "IPv4"},
        {"2001:db8::1", "IPv6"},
#ifdef _WIN32
        {"\\\\.\\pipe\\testpipe", "UnixDomain"},
#else
        {"/tmp/test.sock", "UnixDomain"},
#endif
        {"not-an-address", ""},  // Should fail
    };

    for (const auto& [address, expectedType] : testAddresses) {
        std::cout << "Testing address: " << address << std::endl;
        try {
            auto addrObj = atom::web::Address::createFromString(address);
            if (addrObj) {
                std::cout << "  Created " << addrObj->getType()
                          << " address: " << addrObj->getAddress() << std::endl;
                if (addrObj->getType() == expectedType) {
                    std::cout << "  ✓ Type matches expected: " << expectedType
                              << std::endl;
                } else {
                    std::cout << "  ✗ Type mismatch! Expected: " << expectedType
                              << ", Got: " << addrObj->getType() << std::endl;
                }
            } else {
                std::cout << "  ✗ Failed to create address" << std::endl;
                if (expectedType.empty()) {
                    std::cout << "  ✓ Expected failure for invalid address"
                              << std::endl;
                } else {
                    std::cout << "  ✗ Should have created a " << expectedType
                              << " address" << std::endl;
                }
            }
        } catch (const atom::web::AddressException& e) {
            std::cout << "  ✗ Exception: " << e.what() << std::endl;
            if (expectedType.empty()) {
                std::cout << "  ✓ Expected exception for invalid address"
                          << std::endl;
            } else {
                std::cout << "  ✗ Should have created a " << expectedType
                          << " address" << std::endl;
            }
        }
        std::cout << std::endl;
    }
}

// Test exception handling
void demonstrateExceptionHandling() {
    printHeader("Exception Handling");

    // Test InvalidAddressFormat exception
    std::cout << "Testing InvalidAddressFormat exception:" << std::endl;
    try {
        atom::web::IPv4 invalidIp("999.999.999.999");  // Invalid IPv4
        std::cout << "  ✗ Should have thrown an exception" << std::endl;
    } catch (const atom::web::InvalidAddressFormat& e) {
        std::cout << "  ✓ Caught expected exception: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  ✗ Caught unexpected exception: " << e.what()
                  << std::endl;
    }

    // Test AddressRangeError exception
    std::cout << "\nTesting AddressRangeError exception:" << std::endl;
    try {
        atom::web::IPv4 ip("192.168.1.1");
        ip.isInRange("192.168.1.100", "192.168.1.10");  // End < Start
        std::cout << "  ✗ Should have thrown an exception" << std::endl;
    } catch (const atom::web::AddressRangeError& e) {
        std::cout << "  ✓ Caught expected exception: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  ✗ Caught unexpected exception: " << e.what()
                  << std::endl;
    }
}

// Comprehensive example showing all features together
void comprehensiveExample() {
    printHeader("Comprehensive Example");

    try {
        // Create addresses of different types
        auto ipv4Addr = atom::web::Address::createFromString("192.168.1.1");
        auto ipv6Addr = atom::web::Address::createFromString("2001:db8::1");
#ifdef _WIN32
        auto unixAddr =
            atom::web::Address::createFromString("\\\\.\\pipe\\testpipe");
#else
        auto unixAddr = atom::web::Address::createFromString("/tmp/test.sock");
#endif

        std::cout << "Created addresses:" << std::endl;
        std::cout << "  IPv4: " << ipv4Addr->getAddress() << std::endl;
        std::cout << "  IPv6: " << ipv6Addr->getAddress() << std::endl;
        std::cout << "  Unix: " << unixAddr->getAddress() << std::endl;
        std::cout << std::endl;

        // Polymorphic operations
        std::vector<std::unique_ptr<atom::web::Address>> addresses;
        addresses.push_back(std::move(ipv4Addr));
        addresses.push_back(std::move(ipv6Addr));
        addresses.push_back(std::move(unixAddr));

        std::cout << "Address information:" << std::endl;
        for (const auto& addr : addresses) {
            std::cout << "  Address: " << addr->getAddress() << std::endl;
            std::cout << "  Type: " << addr->getType() << std::endl;
            std::cout << "  Hex: " << addr->toHex() << std::endl;
            std::cout << "  Binary (first 16 bits): "
                      << addr->toBinary().substr(0, 16) << "..." << std::endl;
            std::cout << std::endl;
        }

        // Create more addresses for comparison
        atom::web::IPv4 ip1("192.168.1.1");
        atom::web::IPv4 ip2("192.168.1.2");
        atom::web::IPv4 ip3("10.0.0.1");

        // Subnet operations
        std::cout << "Subnet operations:" << std::endl;
        std::cout << "  Network address of " << ip1.getAddress()
                  << " (255.255.255.0): "
                  << ip1.getNetworkAddress("255.255.255.0") << std::endl;
        std::cout << "  Broadcast address of " << ip1.getAddress()
                  << " (255.255.255.0): "
                  << ip1.getBroadcastAddress("255.255.255.0") << std::endl;
        std::cout << "  " << ip1.getAddress() << " and " << ip2.getAddress()
                  << " in same subnet: "
                  << (ip1.isSameSubnet(ip2, "255.255.255.0") ? "Yes" : "No")
                  << std::endl;
        std::cout << "  " << ip1.getAddress() << " and " << ip3.getAddress()
                  << " in same subnet: "
                  << (ip1.isSameSubnet(ip3, "255.255.255.0") ? "Yes" : "No")
                  << std::endl;

        // CIDR notation
        atom::web::IPv4 cidrIp;
        cidrIp.parseCIDR("192.168.0.0/16");
        std::cout << "\nCIDR operations:" << std::endl;
        std::cout << "  CIDR address: " << cidrIp.getAddress() << std::endl;
        std::cout << "  " << ip1.getAddress() << " in subnet "
                  << cidrIp.getAddress() << ": "
                  << (ip1.isInRange("192.168.0.0", "192.168.255.255") ? "Yes"
                                                                      : "No")
                  << std::endl;
        std::cout << "  " << ip3.getAddress() << " in subnet "
                  << cidrIp.getAddress() << ": "
                  << (ip3.isInRange("192.168.0.0", "192.168.255.255") ? "Yes"
                                                                      : "No")
                  << std::endl;

    } catch (const atom::web::AddressException& e) {
        std::cout << "Exception in comprehensive example: " << e.what()
                  << std::endl;
    }
}

int main() {
    std::cout << "Network Address Classes - Usage Examples" << std::endl;
    std::cout << "=======================================" << std::endl
              << std::endl;

    // Demonstrate basic address creation using the factory
    demonstrateAddressFactory();

    // Demonstrate operations specific to each address type
    demonstrateIPv4Operations();
    demonstrateIPv6Operations();
    demonstrateUnixDomainOperations();

    // Demonstrate exception handling
    demonstrateExceptionHandling();

    // Put it all together
    comprehensiveExample();

    return 0;
}