#include "atom/web/address.hpp"

#include <iostream>

using namespace atom::web;

int main() {
    // Create an IPv4 address
    IPv4 ipv4("192.168.1.1");
    std::cout << "IPv4 address: " << ipv4.getAddress() << std::endl;

    // Parse an IPv4 address
    bool parsedIPv4 = ipv4.parse("192.168.1.100");
    std::cout << "Parsed IPv4: " << std::boolalpha << parsedIPv4 << std::endl;

    // Print the address type
    ipv4.printAddressType();

    // Check if the IPv4 address is within a specified range
    bool isInRangeIPv4 = ipv4.isInRange("192.168.1.0", "192.168.1.255");
    std::cout << "IPv4 is in range: " << std::boolalpha << isInRangeIPv4
              << std::endl;

    // Convert the IPv4 address to its binary representation
    std::string binaryIPv4 = ipv4.toBinary();
    std::cout << "IPv4 binary: " << binaryIPv4 << std::endl;

    // Check if two IPv4 addresses are equal
    IPv4 ipv4_2("192.168.1.100");
    bool isEqualIPv4 = ipv4.isEqual(ipv4_2);
    std::cout << "IPv4 addresses are equal: " << std::boolalpha << isEqualIPv4
              << std::endl;

    // Get the IPv4 address type
    std::string typeIPv4 = ipv4.getType();
    std::cout << "IPv4 type: " << typeIPv4 << std::endl;

    // Get the network address given a subnet mask
    std::string networkAddressIPv4 = ipv4.getNetworkAddress("255.255.255.0");
    std::cout << "IPv4 network address: " << networkAddressIPv4 << std::endl;

    // Get the broadcast address given a subnet mask
    std::string broadcastAddressIPv4 =
        ipv4.getBroadcastAddress("255.255.255.0");
    std::cout << "IPv4 broadcast address: " << broadcastAddressIPv4
              << std::endl;

    // Check if two IPv4 addresses are in the same subnet
    bool isSameSubnetIPv4 = ipv4.isSameSubnet(ipv4_2, "255.255.255.0");
    std::cout << "IPv4 addresses are in the same subnet: " << std::boolalpha
              << isSameSubnetIPv4 << std::endl;

    // Convert the IPv4 address to its hexadecimal representation
    std::string hexIPv4 = ipv4.toHex();
    std::cout << "IPv4 hex: " << hexIPv4 << std::endl;

    // Parse an IPv4 address in CIDR notation
    bool parsedCIDRIPv4 = ipv4.parseCIDR("192.168.1.0/24");
    std::cout << "Parsed IPv4 CIDR: " << std::boolalpha << parsedCIDRIPv4
              << std::endl;

    // Create an IPv6 address
    IPv6 ipv6("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
    std::cout << "IPv6 address: " << ipv6.getAddress() << std::endl;

    // Parse an IPv6 address
    bool parsedIPv6 = ipv6.parse("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
    std::cout << "Parsed IPv6: " << std::boolalpha << parsedIPv6 << std::endl;

    // Print the address type
    ipv6.printAddressType();

    // Check if the IPv6 address is within a specified range
    bool isInRangeIPv6 =
        ipv6.isInRange("2001:0db8:85a3:0000:0000:8a2e:0370:0000",
                       "2001:0db8:85a3:0000:0000:8a2e:0370:ffff");
    std::cout << "IPv6 is in range: " << std::boolalpha << isInRangeIPv6
              << std::endl;

    // Convert the IPv6 address to its binary representation
    std::string binaryIPv6 = ipv6.toBinary();
    std::cout << "IPv6 binary: " << binaryIPv6 << std::endl;

    // Check if two IPv6 addresses are equal
    IPv6 ipv6_2("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
    bool isEqualIPv6 = ipv6.isEqual(ipv6_2);
    std::cout << "IPv6 addresses are equal: " << std::boolalpha << isEqualIPv6
              << std::endl;

    // Get the IPv6 address type
    std::string typeIPv6 = ipv6.getType();
    std::cout << "IPv6 type: " << typeIPv6 << std::endl;

    // Get the network address given a subnet mask
    std::string networkAddressIPv6 =
        ipv6.getNetworkAddress("ffff:ffff:ffff:ffff::");
    std::cout << "IPv6 network address: " << networkAddressIPv6 << std::endl;

    // Get the broadcast address given a subnet mask
    std::string broadcastAddressIPv6 =
        ipv6.getBroadcastAddress("ffff:ffff:ffff:ffff::");
    std::cout << "IPv6 broadcast address: " << broadcastAddressIPv6
              << std::endl;

    // Check if two IPv6 addresses are in the same subnet
    bool isSameSubnetIPv6 = ipv6.isSameSubnet(ipv6_2, "ffff:ffff:ffff:ffff::");
    std::cout << "IPv6 addresses are in the same subnet: " << std::boolalpha
              << isSameSubnetIPv6 << std::endl;

    // Convert the IPv6 address to its hexadecimal representation
    std::string hexIPv6 = ipv6.toHex();
    std::cout << "IPv6 hex: " << hexIPv6 << std::endl;

    // Parse an IPv6 address in CIDR notation
    bool parsedCIDRIPv6 = ipv6.parseCIDR("2001:0db8:85a3::/64");
    std::cout << "Parsed IPv6 CIDR: " << std::boolalpha << parsedCIDRIPv6
              << std::endl;

    // Create a Unix domain socket address
    UnixDomain unixDomain("/tmp/socket");
    std::cout << "Unix domain socket address: " << unixDomain.getAddress()
              << std::endl;

    // Parse a Unix domain socket address
    bool parsedUnixDomain = unixDomain.parse("/tmp/socket");
    std::cout << "Parsed Unix domain socket: " << std::boolalpha
              << parsedUnixDomain << std::endl;

    // Print the address type
    unixDomain.printAddressType();

    // Check if the Unix domain socket address is within a specified range
    bool isInRangeUnixDomain =
        unixDomain.isInRange("/tmp/socket1", "/tmp/socket2");
    std::cout << "Unix domain socket is in range: " << std::boolalpha
              << isInRangeUnixDomain << std::endl;

    // Convert the Unix domain socket address to its binary representation
    std::string binaryUnixDomain = unixDomain.toBinary();
    std::cout << "Unix domain socket binary: " << binaryUnixDomain << std::endl;

    // Check if two Unix domain socket addresses are equal
    UnixDomain unixDomain_2("/tmp/socket");
    bool isEqualUnixDomain = unixDomain.isEqual(unixDomain_2);
    std::cout << "Unix domain socket addresses are equal: " << std::boolalpha
              << isEqualUnixDomain << std::endl;

    // Get the Unix domain socket address type
    std::string typeUnixDomain = unixDomain.getType();
    std::cout << "Unix domain socket type: " << typeUnixDomain << std::endl;

    // Get the network address given a subnet mask (not applicable for Unix
    // domain sockets)
    std::string networkAddressUnixDomain = unixDomain.getNetworkAddress("");
    std::cout << "Unix domain socket network address: "
              << networkAddressUnixDomain << std::endl;

    // Get the broadcast address given a subnet mask (not applicable for Unix
    // domain sockets)
    std::string broadcastAddressUnixDomain = unixDomain.getBroadcastAddress("");
    std::cout << "Unix domain socket broadcast address: "
              << broadcastAddressUnixDomain << std::endl;

    // Check if two Unix domain socket addresses are in the same subnet (not
    // applicable for Unix domain sockets)
    bool isSameSubnetUnixDomain = unixDomain.isSameSubnet(unixDomain_2, "");
    std::cout << "Unix domain socket addresses are in the same subnet: "
              << std::boolalpha << isSameSubnetUnixDomain << std::endl;

    // Convert the Unix domain socket address to its hexadecimal representation
    std::string hexUnixDomain = unixDomain.toHex();
    std::cout << "Unix domain socket hex: " << hexUnixDomain << std::endl;

    return 0;
}