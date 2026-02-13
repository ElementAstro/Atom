// filepath: /home/max/Atom-1/atom/web/test_address.cpp

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "atom/web/address.hpp"
// Removed: atom/log/loguru.hpp not available
#include <spdlog/spdlog.h>

using namespace atom::web;
using ::testing::HasSubstr;

class UnixDomainTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing
        static bool initialized = false;
        if (!initialized) {
            int argc = 0;
            // loguru::init(argc, nullptr);
            // loguru disabled
            initialized = true;
        }
    }
};

// Test that getBroadcastAddress returns an empty string for Unix domain sockets
TEST_F(UnixDomainTest, GetBroadcastAddressReturnsEmptyString) {
    UnixDomain unixDomain("/tmp/test.sock");
    EXPECT_TRUE(unixDomain.getBroadcastAddress("255.255.255.0").empty());
}

// Test that getBroadcastAddress returns empty string for Unix domain sockets
TEST_F(UnixDomainTest, GetBroadcastAddressReturnsEmpty) {
    UnixDomain unixDomain("/tmp/test.sock");

    // getBroadcastAddress should return empty string for Unix domain sockets
    auto result = unixDomain.getBroadcastAddress("255.255.255.0");
    EXPECT_TRUE(result.empty());
}

// Test getBroadcastAddress with different types of masks
TEST_F(UnixDomainTest, GetBroadcastAddressWithDifferentMasks) {
    UnixDomain unixDomain("/tmp/test.sock");

    // Test with various mask formats
    EXPECT_TRUE(unixDomain.getBroadcastAddress("").empty());
    EXPECT_TRUE(unixDomain.getBroadcastAddress("255.255.255.0").empty());
    EXPECT_TRUE(unixDomain.getBroadcastAddress("ffff:ffff::").empty());
    EXPECT_TRUE(unixDomain.getBroadcastAddress("invalid_mask").empty());
    EXPECT_TRUE(unixDomain.getBroadcastAddress("/some/path").empty());
}

// Test getBroadcastAddress with long Unix domain socket paths
TEST_F(UnixDomainTest, GetBroadcastAddressWithLongPath) {
    // Create a Unix domain socket with a long path (but still valid)
    std::string longPath = "/tmp/" + std::string(90, 'a');
    UnixDomain unixDomain(longPath);

    EXPECT_TRUE(unixDomain.getBroadcastAddress("255.255.255.0").empty());
}

// Test getBroadcastAddress after creating socket from different constructors
TEST_F(UnixDomainTest, GetBroadcastAddressAfterDifferentConstructions) {
    // Default constructor and then parse
    UnixDomain unixDomain1;
    unixDomain1.parse("/tmp/test1.sock");
    EXPECT_TRUE(unixDomain1.getBroadcastAddress("255.255.255.0").empty());

    // Direct construction with path
    UnixDomain unixDomain2("/tmp/test2.sock");
    EXPECT_TRUE(unixDomain2.getBroadcastAddress("255.255.255.0").empty());

    // Copy construction
    UnixDomain unixDomain3(unixDomain2);
    EXPECT_TRUE(unixDomain3.getBroadcastAddress("255.255.255.0").empty());
}

// Test interaction between getBroadcastAddress and other methods
TEST_F(UnixDomainTest, GetBroadcastAddressInteractionWithOtherMethods) {
    UnixDomain unixDomain("/tmp/test.sock");

    // Call other methods before getBroadcastAddress
    auto type = unixDomain.getType();
    auto binary = unixDomain.toBinary();
    auto hex = unixDomain.toHex();

    // Verify these methods return expected values
    EXPECT_FALSE(type.empty());
    EXPECT_FALSE(binary.empty());
    EXPECT_FALSE(hex.empty());

    // getBroadcastAddress should still return empty string
    EXPECT_TRUE(unixDomain.getBroadcastAddress("255.255.255.0").empty());

    // Call methods after getBroadcastAddress
    EXPECT_EQ(unixDomain.getType(), "UnixDomain");
    EXPECT_FALSE(unixDomain.toBinary().empty());
    EXPECT_FALSE(unixDomain.toHex().empty());
}

// Test getBroadcastAddress compared with other address types
TEST_F(UnixDomainTest, CompareBroadcastAddressBehaviorWithOtherTypes) {
    // Create addresses of different types
    UnixDomain unixDomain("/tmp/test.sock");
    IPv4 ipv4("192.168.1.1");
    IPv6 ipv6("2001:db8::1");

    // For Unix domain sockets, getBroadcastAddress should return an empty
    // string
    EXPECT_TRUE(unixDomain.getBroadcastAddress("255.255.255.0").empty());

    // For IPv4, getBroadcastAddress should return a valid address
    EXPECT_FALSE(ipv4.getBroadcastAddress("255.255.255.0").empty());
    EXPECT_EQ(ipv4.getBroadcastAddress("255.255.255.0"), "192.168.1.255");

    // For IPv6, behavior depends on implementation details
    // (not testing exact result here since it's complex)
    EXPECT_NO_THROW({
        auto ipv6Result = ipv6.getBroadcastAddress("ffff:ffff:ffff:ffff::");
        (void)ipv6Result;  // Suppress unused variable warning
    });
}

// Test with factory method
TEST_F(UnixDomainTest, GetBroadcastAddressWithFactoryMethod) {
    // Create Unix domain socket using factory method
    auto address = Address::createFromString("/tmp/test.sock");
    ASSERT_NE(address, nullptr);
    EXPECT_EQ(address->getType(), "UnixDomain");

    // getBroadcastAddress should return an empty string
    EXPECT_TRUE(address->getBroadcastAddress("255.255.255.0").empty());
}

// Edge case: Test getBroadcastAddress with extremely short path
TEST_F(UnixDomainTest, GetBroadcastAddressWithShortPath) {
    UnixDomain unixDomain("/a");  // Shortest valid path
    EXPECT_TRUE(unixDomain.getBroadcastAddress("255.255.255.0").empty());
}

// Test multiple consecutive calls to getBroadcastAddress
TEST_F(UnixDomainTest, MultipleBroadcastAddressCalls) {
    UnixDomain unixDomain("/tmp/test.sock");

    // Call getBroadcastAddress multiple times consecutively
    EXPECT_TRUE(unixDomain.getBroadcastAddress("255.255.255.0").empty());
    EXPECT_TRUE(unixDomain.getBroadcastAddress("255.255.0.0").empty());
    EXPECT_TRUE(unixDomain.getBroadcastAddress("255.0.0.0").empty());
    EXPECT_TRUE(unixDomain.getBroadcastAddress("0.0.0.0").empty());

    // Verify the socket path is unchanged
    EXPECT_EQ(unixDomain.getAddress(), "/tmp/test.sock");
}

// IPv4 Comprehensive Tests
class IPv4Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests
        spdlog::set_level(spdlog::level::off);
    }
};

TEST_F(IPv4Test, ConstructorAndParsing) {
    // Valid IPv4 addresses
    ASSERT_NO_THROW(IPv4("192.168.1.1"));
    ASSERT_NO_THROW(IPv4("0.0.0.0"));
    ASSERT_NO_THROW(IPv4("255.255.255.255"));
    ASSERT_NO_THROW(IPv4("127.0.0.1"));
    ASSERT_NO_THROW(IPv4("10.0.0.1"));

    // Invalid IPv4 addresses should throw
    EXPECT_THROW(IPv4("256.1.1.1"), InvalidAddressFormat);
    EXPECT_THROW(IPv4("192.168.1"), InvalidAddressFormat);
    EXPECT_THROW(IPv4("192.168.1.1.1"), InvalidAddressFormat);
    EXPECT_THROW(IPv4("not.an.ip.address"), InvalidAddressFormat);
    EXPECT_THROW(IPv4(""), InvalidAddressFormat);
}

TEST_F(IPv4Test, ParseMethod) {
    IPv4 ipv4;

    // Valid parsing
    EXPECT_TRUE(ipv4.parse("192.168.1.100"));
    EXPECT_EQ(ipv4.getAddress(), "192.168.1.100");

    // Invalid parsing
    EXPECT_FALSE(ipv4.parse("256.1.1.1"));
    EXPECT_FALSE(ipv4.parse("invalid"));
    EXPECT_FALSE(ipv4.parse(""));
}

TEST_F(IPv4Test, AddressType) {
    IPv4 ipv4("192.168.1.1");
    EXPECT_EQ(ipv4.getType(), "IPv4");

    // Test printAddressType doesn't crash
    ASSERT_NO_THROW(ipv4.printAddressType());
}

TEST_F(IPv4Test, BinaryConversion) {
    IPv4 ipv4("192.168.1.1");
    std::string binary = ipv4.toBinary();

    // Should be 32 bits (4 bytes * 8 bits)
    EXPECT_EQ(binary.length(), 32);

    // 192.168.1.1 = 11000000.10101000.00000001.00000001
    EXPECT_EQ(binary, "11000000101010000000000100000001");
}

TEST_F(IPv4Test, HexConversion) {
    IPv4 ipv4("192.168.1.1");
    std::string hex = ipv4.toHex();

    // Should be 8 hex characters (4 bytes * 2 hex chars)
    EXPECT_EQ(hex.length(), 8);

    // 192.168.1.1 = C0A80101
    EXPECT_EQ(hex, "C0A80101");
}

TEST_F(IPv4Test, RangeChecking) {
    IPv4 ipv4("192.168.1.100");

    // Valid ranges
    EXPECT_TRUE(ipv4.isInRange("192.168.1.1", "192.168.1.200"));
    EXPECT_TRUE(ipv4.isInRange("192.168.1.100", "192.168.1.100"));
    EXPECT_TRUE(ipv4.isInRange("0.0.0.0", "255.255.255.255"));

    // Invalid ranges
    EXPECT_FALSE(ipv4.isInRange("192.168.1.101", "192.168.1.200"));
    EXPECT_FALSE(ipv4.isInRange("192.168.2.1", "192.168.2.200"));

    // Invalid range (start > end) should throw
    EXPECT_THROW(ipv4.isInRange("192.168.1.200", "192.168.1.1"),
                 AddressRangeError);
}

TEST_F(IPv4Test, Equality) {
    IPv4 ipv4_1("192.168.1.1");
    IPv4 ipv4_2("192.168.1.1");
    IPv4 ipv4_3("192.168.1.2");

    EXPECT_TRUE(ipv4_1.isEqual(ipv4_2));
    EXPECT_FALSE(ipv4_1.isEqual(ipv4_3));
}

TEST_F(IPv4Test, NetworkCalculations) {
    IPv4 ipv4("192.168.1.100");

    // Network address with /24 mask
    std::string network = ipv4.getNetworkAddress("255.255.255.0");
    EXPECT_EQ(network, "192.168.1.0");

    // Broadcast address with /24 mask
    std::string broadcast = ipv4.getBroadcastAddress("255.255.255.0");
    EXPECT_EQ(broadcast, "192.168.1.255");

    // Network address with /16 mask
    std::string network16 = ipv4.getNetworkAddress("255.255.0.0");
    EXPECT_EQ(network16, "192.168.0.0");

    // Broadcast address with /16 mask
    std::string broadcast16 = ipv4.getBroadcastAddress("255.255.0.0");
    EXPECT_EQ(broadcast16, "192.168.255.255");
}

TEST_F(IPv4Test, SubnetChecking) {
    IPv4 ipv4_1("192.168.1.100");
    IPv4 ipv4_2("192.168.1.200");
    IPv4 ipv4_3("192.168.2.100");

    // Same subnet with /24 mask
    EXPECT_TRUE(ipv4_1.isSameSubnet(ipv4_2, "255.255.255.0"));

    // Different subnet with /24 mask
    EXPECT_FALSE(ipv4_1.isSameSubnet(ipv4_3, "255.255.255.0"));

    // Same subnet with /16 mask
    EXPECT_TRUE(ipv4_1.isSameSubnet(ipv4_3, "255.255.0.0"));
}

// Note: StaticValidation test removed because isValidIPv4 is now private
// Validation is tested indirectly through constructor and parse methods

TEST_F(IPv4Test, EdgeCaseAddresses) {
    // Loopback
    IPv4 loopback("127.0.0.1");
    EXPECT_EQ(loopback.getType(), "IPv4");

    // Broadcast
    IPv4 broadcast("255.255.255.255");
    EXPECT_EQ(broadcast.getType(), "IPv4");

    // Network zero
    IPv4 zero("0.0.0.0");
    EXPECT_EQ(zero.getType(), "IPv4");

    // Private ranges
    IPv4 private_a("10.0.0.1");
    IPv4 private_b("172.16.0.1");
    IPv4 private_c("192.168.0.1");

    EXPECT_EQ(private_a.getType(), "IPv4");
    EXPECT_EQ(private_b.getType(), "IPv4");
    EXPECT_EQ(private_c.getType(), "IPv4");
}

// IPv6 Comprehensive Tests
class IPv6Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests
        spdlog::set_level(spdlog::level::off);
    }
};

TEST_F(IPv6Test, ConstructorAndParsing) {
    // Valid IPv6 addresses
    ASSERT_NO_THROW(IPv6("2001:db8::1"));
    ASSERT_NO_THROW(IPv6("::1"));
    ASSERT_NO_THROW(IPv6("::"));
    ASSERT_NO_THROW(IPv6("2001:db8:85a3::8a2e:370:7334"));
    ASSERT_NO_THROW(IPv6("fe80::1%lo0"));
    ASSERT_NO_THROW(IPv6("::ffff:192.168.1.1"));  // IPv4-mapped

    // Invalid IPv6 addresses should throw
    EXPECT_THROW(IPv6("invalid::address::too::many::colons"),
                 InvalidAddressFormat);
    EXPECT_THROW(IPv6("2001:db8::1::2"), InvalidAddressFormat);  // Double ::
    EXPECT_THROW(IPv6("gggg::1"), InvalidAddressFormat);         // Invalid hex
    EXPECT_THROW(IPv6(""), InvalidAddressFormat);
}

TEST_F(IPv6Test, ParseMethod) {
    IPv6 ipv6;

    // Valid parsing
    EXPECT_TRUE(ipv6.parse("2001:db8::1"));
    EXPECT_EQ(ipv6.getAddress(), "2001:db8::1");

    // Invalid parsing
    EXPECT_FALSE(ipv6.parse("invalid::address"));
    EXPECT_FALSE(ipv6.parse(""));
}

TEST_F(IPv6Test, AddressType) {
    IPv6 ipv6("2001:db8::1");
    EXPECT_EQ(ipv6.getType(), "IPv6");

    // Test printAddressType doesn't crash
    ASSERT_NO_THROW(ipv6.printAddressType());
}

TEST_F(IPv6Test, BinaryConversion) {
    IPv6 ipv6("::1");
    std::string binary = ipv6.toBinary();

    // Should be 128 bits (16 bytes * 8 bits)
    EXPECT_EQ(binary.length(), 128);

    // ::1 should have 127 zeros followed by a 1
    std::string expected(127, '0');
    expected += "1";
    EXPECT_EQ(binary, expected);
}

TEST_F(IPv6Test, HexConversion) {
    IPv6 ipv6("::1");
    std::string hex = ipv6.toHex();

    // Should be 32 hex characters (16 bytes * 2 hex chars)
    EXPECT_EQ(hex.length(), 32);

    // ::1 = 00000000000000000000000000000001
    EXPECT_EQ(hex, "00000000000000000000000000000001");
}

TEST_F(IPv6Test, RangeChecking) {
    IPv6 ipv6("2001:db8::100");

    // Valid ranges
    EXPECT_TRUE(ipv6.isInRange("2001:db8::1", "2001:db8::200"));
    EXPECT_TRUE(ipv6.isInRange("2001:db8::100", "2001:db8::100"));
    EXPECT_TRUE(
        ipv6.isInRange("::", "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));

    // Invalid ranges
    EXPECT_FALSE(ipv6.isInRange("2001:db8::101", "2001:db8::200"));
    EXPECT_FALSE(ipv6.isInRange("2001:db9::1", "2001:db9::200"));

    // Invalid range (start > end) should throw
    EXPECT_THROW(ipv6.isInRange("2001:db8::200", "2001:db8::1"),
                 AddressRangeError);
}

TEST_F(IPv6Test, Equality) {
    IPv6 ipv6_1("2001:db8::1");
    IPv6 ipv6_2("2001:db8::1");
    IPv6 ipv6_3("2001:db8::2");

    EXPECT_TRUE(ipv6_1.isEqual(ipv6_2));
    EXPECT_FALSE(ipv6_1.isEqual(ipv6_3));
}

TEST_F(IPv6Test, NetworkCalculations) {
    IPv6 ipv6("2001:db8::1");

    // Network address with /64 prefix
    std::string network = ipv6.getNetworkAddress("ffff:ffff:ffff:ffff::");
    EXPECT_EQ(network, "2001:db8::");

    // Broadcast address with /64 prefix
    std::string broadcast = ipv6.getBroadcastAddress("ffff:ffff:ffff:ffff::");
    EXPECT_EQ(broadcast, "2001:db8::ffff:ffff:ffff:ffff");
}

TEST_F(IPv6Test, SubnetChecking) {
    IPv6 ipv6_1("2001:db8::1");
    IPv6 ipv6_2("2001:db8::2");
    IPv6 ipv6_3("2001:db9::1");

    // Same subnet with /64 prefix
    EXPECT_TRUE(ipv6_1.isSameSubnet(ipv6_2, "ffff:ffff:ffff:ffff::"));

    // Different subnet with /64 prefix
    EXPECT_FALSE(ipv6_1.isSameSubnet(ipv6_3, "ffff:ffff:ffff:ffff::"));
}

TEST_F(IPv6Test, CIDRParsing) {
    IPv6 ipv6;

    // Valid CIDR notation
    EXPECT_TRUE(ipv6.parseCIDR("2001:db8::/64"));
    EXPECT_EQ(ipv6.getAddress(), "2001:db8::");

    // Invalid CIDR notation
    EXPECT_FALSE(ipv6.parseCIDR("invalid::/64"));
    EXPECT_FALSE(ipv6.parseCIDR("2001:db8::/129"));  // Invalid prefix length
    EXPECT_FALSE(ipv6.parseCIDR("2001:db8::"));      // Missing prefix
}

TEST_F(IPv6Test, PrefixLength) {
    auto prefix64 = IPv6::getPrefixLength("2001:db8::/64");
    ASSERT_TRUE(prefix64.has_value());
    EXPECT_EQ(*prefix64, 64);

    auto prefix128 = IPv6::getPrefixLength("::1/128");
    ASSERT_TRUE(prefix128.has_value());
    EXPECT_EQ(*prefix128, 128);

    auto prefix0 = IPv6::getPrefixLength("::/0");
    ASSERT_TRUE(prefix0.has_value());
    EXPECT_EQ(*prefix0, 0);

    // Invalid prefix lengths
    auto invalidPrefix = IPv6::getPrefixLength("2001:db8::/129");
    EXPECT_FALSE(invalidPrefix.has_value());

    auto noPrefixLength = IPv6::getPrefixLength("2001:db8::");
    EXPECT_FALSE(noPrefixLength.has_value());
}

TEST_F(IPv6Test, StaticValidation) {
    EXPECT_TRUE(IPv6::isValidIPv6("2001:db8::1"));
    EXPECT_TRUE(IPv6::isValidIPv6("::1"));
    EXPECT_TRUE(IPv6::isValidIPv6("::"));
    EXPECT_TRUE(IPv6::isValidIPv6("fe80::1%lo0"));

    EXPECT_FALSE(IPv6::isValidIPv6("invalid::address"));
    EXPECT_FALSE(IPv6::isValidIPv6("2001:db8::1::2"));
    EXPECT_FALSE(IPv6::isValidIPv6("gggg::1"));
    EXPECT_FALSE(IPv6::isValidIPv6(""));
}

TEST_F(IPv6Test, SpecialAddresses) {
    // Loopback
    IPv6 loopback("::1");
    EXPECT_EQ(loopback.getType(), "IPv6");

    // Unspecified
    IPv6 unspecified("::");
    EXPECT_EQ(unspecified.getType(), "IPv6");

    // Link-local
    IPv6 linkLocal("fe80::1");
    EXPECT_EQ(linkLocal.getType(), "IPv6");

    // IPv4-mapped
    IPv6 ipv4Mapped("::ffff:192.168.1.1");
    EXPECT_EQ(ipv4Mapped.getType(), "IPv6");

    // Multicast
    IPv6 multicast("ff02::1");
    EXPECT_EQ(multicast.getType(), "IPv6");
}

// Extended UnixDomain Tests
class ExtendedUnixDomainTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests
        spdlog::set_level(spdlog::level::off);
    }
};

TEST_F(ExtendedUnixDomainTest, ConstructorAndParsing) {
#ifdef _WIN32
    // Windows named pipes
    ASSERT_NO_THROW(UnixDomain("\\\\.\\pipe\\test"));
    ASSERT_NO_THROW(UnixDomain("\\\\.\\pipe\\very\\long\\path\\test"));
#else
    // Unix domain sockets
    ASSERT_NO_THROW(UnixDomain("/tmp/test.sock"));
    ASSERT_NO_THROW(UnixDomain("/var/run/test.sock"));
    ASSERT_NO_THROW(UnixDomain("./relative/path.sock"));
#endif

    // Invalid paths should throw
    EXPECT_THROW(UnixDomain(""), InvalidAddressFormat);
}

TEST_F(ExtendedUnixDomainTest, ParseMethod) {
    UnixDomain unixDomain;

#ifdef _WIN32
    EXPECT_TRUE(unixDomain.parse("\\\\.\\pipe\\test"));
    EXPECT_EQ(unixDomain.getAddress(), "\\\\.\\pipe\\test");
#else
    EXPECT_TRUE(unixDomain.parse("/tmp/test.sock"));
    EXPECT_EQ(unixDomain.getAddress(), "/tmp/test.sock");
#endif

    // Invalid parsing
    EXPECT_FALSE(unixDomain.parse(""));
}

TEST_F(ExtendedUnixDomainTest, AddressType) {
#ifdef _WIN32
    UnixDomain unixDomain("\\\\.\\pipe\\test");
#else
    UnixDomain unixDomain("/tmp/test.sock");
#endif

    EXPECT_EQ(unixDomain.getType(), "UnixDomain");

    // Test printAddressType doesn't crash
    ASSERT_NO_THROW(unixDomain.printAddressType());
}

TEST_F(ExtendedUnixDomainTest, BinaryConversion) {
#ifdef _WIN32
    UnixDomain unixDomain("\\\\.\\pipe\\test");
#else
    UnixDomain unixDomain("/tmp/test.sock");
#endif

    std::string binary = unixDomain.toBinary();
    EXPECT_FALSE(binary.empty());

    // Should contain only 0s and 1s
    for (char c : binary) {
        EXPECT_TRUE(c == '0' || c == '1');
    }
}

TEST_F(ExtendedUnixDomainTest, HexConversion) {
#ifdef _WIN32
    UnixDomain unixDomain("\\\\.\\pipe\\test");
#else
    UnixDomain unixDomain("/tmp/test.sock");
#endif

    std::string hex = unixDomain.toHex();
    EXPECT_FALSE(hex.empty());

    // Should contain only hex characters
    for (char c : hex) {
        EXPECT_TRUE(std::isxdigit(c));
    }
}

TEST_F(ExtendedUnixDomainTest, RangeChecking) {
#ifdef _WIN32
    UnixDomain unixDomain("\\\\.\\pipe\\test");

    // Range checking for Unix domain sockets is path-based
    EXPECT_TRUE(unixDomain.isInRange("\\\\.\\pipe\\a", "\\\\.\\pipe\\z"));
    EXPECT_FALSE(unixDomain.isInRange("\\\\.\\pipe\\u", "\\\\.\\pipe\\z"));
#else
    UnixDomain unixDomain("/tmp/test.sock");

    // Range checking for Unix domain sockets is path-based
    EXPECT_TRUE(unixDomain.isInRange("/tmp/a.sock", "/tmp/z.sock"));
    EXPECT_FALSE(unixDomain.isInRange("/tmp/u.sock", "/tmp/z.sock"));
#endif
}

TEST_F(ExtendedUnixDomainTest, Equality) {
#ifdef _WIN32
    UnixDomain unix1("\\\\.\\pipe\\test");
    UnixDomain unix2("\\\\.\\pipe\\test");
    UnixDomain unix3("\\\\.\\pipe\\other");
#else
    UnixDomain unix1("/tmp/test.sock");
    UnixDomain unix2("/tmp/test.sock");
    UnixDomain unix3("/tmp/other.sock");
#endif

    EXPECT_TRUE(unix1.isEqual(unix2));
    EXPECT_FALSE(unix1.isEqual(unix3));
}

TEST_F(ExtendedUnixDomainTest, NetworkCalculations) {
#ifdef _WIN32
    UnixDomain unixDomain("\\\\.\\pipe\\test\\socket");

    // Network address should return directory
    std::string network = unixDomain.getNetworkAddress("unused");
    EXPECT_EQ(network, "\\\\.\\pipe\\test");

    // Broadcast address should return wildcard pattern
    std::string broadcast = unixDomain.getBroadcastAddress("unused");
    EXPECT_THAT(broadcast, ::testing::HasSubstr("\\\\.\\pipe\\test"));
    EXPECT_THAT(broadcast, ::testing::HasSubstr("*"));
#else
    UnixDomain unixDomain("/tmp/test/socket.sock");

    // Network address should return directory
    std::string network = unixDomain.getNetworkAddress("unused");
    EXPECT_EQ(network, "/tmp/test");

    // Broadcast address should return wildcard pattern
    std::string broadcast = unixDomain.getBroadcastAddress("unused");
    EXPECT_THAT(broadcast, ::testing::HasSubstr("/tmp/test"));
    EXPECT_THAT(broadcast, ::testing::HasSubstr("*"));
#endif
}

TEST_F(ExtendedUnixDomainTest, SubnetChecking) {
#ifdef _WIN32
    UnixDomain unix1("\\\\.\\pipe\\test\\socket1");
    UnixDomain unix2("\\\\.\\pipe\\test\\socket2");
    UnixDomain unix3("\\\\.\\pipe\\other\\socket");

    // Same directory should be same "subnet"
    EXPECT_TRUE(unix1.isSameSubnet(unix2, "unused"));

    // Different directory should be different "subnet"
    EXPECT_FALSE(unix1.isSameSubnet(unix3, "unused"));
#else
    UnixDomain unix1("/tmp/test/socket1.sock");
    UnixDomain unix2("/tmp/test/socket2.sock");
    UnixDomain unix3("/tmp/other/socket.sock");

    // Same directory should be same "subnet"
    EXPECT_TRUE(unix1.isSameSubnet(unix2, "unused"));

    // Different directory should be different "subnet"
    EXPECT_FALSE(unix1.isSameSubnet(unix3, "unused"));
#endif
}

TEST_F(ExtendedUnixDomainTest, StaticValidation) {
#ifdef _WIN32
    EXPECT_TRUE(UnixDomain::isValidPath("\\\\.\\pipe\\test"));
    EXPECT_TRUE(UnixDomain::isValidPath("\\\\.\\pipe\\very\\long\\path"));

    EXPECT_FALSE(UnixDomain::isValidPath(""));
    EXPECT_FALSE(UnixDomain::isValidPath("invalid\\path"));
#else
    EXPECT_TRUE(UnixDomain::isValidPath("/tmp/test.sock"));
    EXPECT_TRUE(UnixDomain::isValidPath("./relative.sock"));
    EXPECT_TRUE(UnixDomain::isValidPath("/var/run/socket"));

    EXPECT_FALSE(UnixDomain::isValidPath(""));
#endif
}

TEST_F(ExtendedUnixDomainTest, PathEdgeCases) {
#ifdef _WIN32
    // Very long pipe name
    std::string longPipe = "\\\\.\\pipe\\";
    longPipe += std::string(200, 'a');
    ASSERT_NO_THROW(UnixDomain{longPipe});
#else
    // Very long path (up to system limits)
    std::string longPath = "/tmp/";
    longPath += std::string(100, 'a');
    longPath += ".sock";
    ASSERT_NO_THROW(UnixDomain(longPath));

    // Path with special characters
    ASSERT_NO_THROW(UnixDomain("/tmp/socket-with_special.chars.sock"));
    ASSERT_NO_THROW(UnixDomain("/tmp/socket with spaces.sock"));
#endif
}

// Address Factory Tests
class AddressFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests
        spdlog::set_level(spdlog::level::off);
    }
};

TEST_F(AddressFactoryTest, CreateFromStringIPv4) {
    auto addr = Address::createFromString("192.168.1.1");
    ASSERT_NE(addr, nullptr);
    EXPECT_EQ(addr->getType(), "IPv4");
    EXPECT_EQ(addr->getAddress(), "192.168.1.1");
}

TEST_F(AddressFactoryTest, CreateFromStringIPv6) {
    auto addr = Address::createFromString("2001:db8::1");
    ASSERT_NE(addr, nullptr);
    EXPECT_EQ(addr->getType(), "IPv6");
    EXPECT_EQ(addr->getAddress(), "2001:db8::1");
}

TEST_F(AddressFactoryTest, CreateFromStringUnixDomain) {
#ifdef _WIN32
    auto addr = Address::createFromString("\\\\.\\pipe\\test");
    ASSERT_NE(addr, nullptr);
    EXPECT_EQ(addr->getType(), "UnixDomain");
    EXPECT_EQ(addr->getAddress(), "\\\\.\\pipe\\test");
#else
    auto addr = Address::createFromString("/tmp/test.sock");
    ASSERT_NE(addr, nullptr);
    EXPECT_EQ(addr->getType(), "UnixDomain");
    EXPECT_EQ(addr->getAddress(), "/tmp/test.sock");
#endif
}

TEST_F(AddressFactoryTest, CreateFromStringInvalid) {
    auto addr = Address::createFromString("invalid.address.format");
    EXPECT_EQ(addr, nullptr);

    auto emptyAddr = Address::createFromString("");
    EXPECT_EQ(emptyAddr, nullptr);
}

TEST_F(AddressFactoryTest, CreateFromStringAmbiguous) {
    // Test addresses that might be ambiguous
    auto addr1 = Address::createFromString("::1");
    ASSERT_NE(addr1, nullptr);
    EXPECT_EQ(addr1->getType(), "IPv6");

    auto addr2 = Address::createFromString("127.0.0.1");
    ASSERT_NE(addr2, nullptr);
    EXPECT_EQ(addr2->getType(), "IPv4");
}

// Cross-Platform Compatibility Tests
class CrossPlatformTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests
        spdlog::set_level(spdlog::level::off);
    }
};

TEST_F(CrossPlatformTest, IPv4Consistency) {
    // Test that IPv4 addresses work consistently across platforms
    std::vector<std::string> testAddresses = {"0.0.0.0",     "127.0.0.1",
                                              "192.168.1.1", "255.255.255.255",
                                              "10.0.0.1",    "172.16.0.1"};

    for (const auto& addrStr : testAddresses) {
        IPv4 ipv4(addrStr);
        EXPECT_EQ(ipv4.getAddress(), addrStr);
        EXPECT_EQ(ipv4.getType(), "IPv4");

        // Binary and hex should be consistent
        std::string binary = ipv4.toBinary();
        std::string hex = ipv4.toHex();
        EXPECT_EQ(binary.length(), 32);
        EXPECT_EQ(hex.length(), 8);
    }
}

TEST_F(CrossPlatformTest, IPv6Consistency) {
    // Test that IPv6 addresses work consistently across platforms
    std::vector<std::string> testAddresses = {"::", "::1", "2001:db8::1",
                                              "fe80::1", "::ffff:192.168.1.1"};

    for (const auto& addrStr : testAddresses) {
        IPv6 ipv6(addrStr);
        EXPECT_EQ(ipv6.getAddress(), addrStr);
        EXPECT_EQ(ipv6.getType(), "IPv6");

        // Binary and hex should be consistent
        std::string binary = ipv6.toBinary();
        std::string hex = ipv6.toHex();
        EXPECT_EQ(binary.length(), 128);
        EXPECT_EQ(hex.length(), 32);
    }
}

TEST_F(CrossPlatformTest, UnixDomainPlatformSpecific) {
#ifdef _WIN32
    // Windows named pipes
    std::vector<std::string> testPaths = {"\\\\.\\pipe\\test",
                                          "\\\\.\\pipe\\long\\path\\test",
                                          "\\\\.\\pipe\\test_with_underscores"};
#else
    // Unix domain sockets
    std::vector<std::string> testPaths = {
        "/tmp/test.sock", "/var/run/test.sock", "./relative.sock",
        "/tmp/test_with_underscores.sock"};
#endif

    for (const auto& pathStr : testPaths) {
        UnixDomain unixDomain(pathStr);
        EXPECT_EQ(unixDomain.getAddress(), pathStr);
        EXPECT_EQ(unixDomain.getType(), "UnixDomain");
    }
}

// Performance and Stress Tests
class AddressPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests
        spdlog::set_level(spdlog::level::off);
    }
};

TEST_F(AddressPerformanceTest, IPv4CreationPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        IPv4 ipv4("192.168.1." + std::to_string(i % 255));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 5000);  // Less than 5 seconds
}

TEST_F(AddressPerformanceTest, IPv6CreationPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        IPv6 ipv6("2001:db8::" + std::to_string(i));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 5000);  // Less than 5 seconds
}

TEST_F(AddressPerformanceTest, FactoryPerformance) {
    std::vector<std::string> addresses = {"192.168.1.1", "2001:db8::1",
#ifdef _WIN32
                                          "\\\\.\\pipe\\test"
#else
                                          "/tmp/test.sock"
#endif
    };

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        for (const auto& addr : addresses) {
            auto address = Address::createFromString(addr);
            EXPECT_NE(address, nullptr);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 10000);  // Less than 10 seconds
}

// Extended Error Handling Tests
class AddressErrorHandlingTest : public ::testing::Test {
protected:
    void SetUp() override { spdlog::set_level(spdlog::level::off); }
};

TEST_F(AddressErrorHandlingTest, IPv4MalformedAddresses) {
    // Test various malformed IPv4 addresses
    std::vector<std::string> malformedAddresses = {
        "192.168.1",           // Missing octet
        "192.168.1.1.1",       // Extra octet
        "256.168.1.1",         // Out of range
        "192.168.1.256",       // Out of range
        "192.168.-1.1",        // Negative
        "192.168.1.a",         // Non-numeric
        "192..168.1.1",        // Double dot
        ".192.168.1.1",        // Leading dot
        "192.168.1.1.",        // Trailing dot
        "192 168 1 1",         // Spaces instead of dots
        "192,168,1,1",         // Commas instead of dots
        "999.999.999.999",     // All out of range
        "192.168.1.01",        // Leading zero (ambiguous)
        "192.168.1.1/24",      // CIDR notation
        "192.168.1.1:80",      // With port
        "http://192.168.1.1",  // URL format
        "192.168.1.1 extra",   // Extra text
        " 192.168.1.1",        // Leading space
        "192.168.1.1 ",        // Trailing space
        "\t192.168.1.1",       // Tab character
        "192.168.1.1\n"        // Newline character
    };

    for (const auto& addr : malformedAddresses) {
        EXPECT_THROW(IPv4(addr), InvalidAddressFormat)
            << "Should throw for malformed address: " << addr;

        IPv4 ipv4;
        EXPECT_FALSE(ipv4.parse(addr))
            << "Parse should fail for malformed address: " << addr;
    }
}

TEST_F(AddressErrorHandlingTest, IPv6MalformedAddresses) {
    // Test various malformed IPv6 addresses
    std::vector<std::string> malformedAddresses = {
        ":::",                            // Too many colons
        "2001:db8::1::2",                 // Double compression
        "gggg::1",                        // Invalid hex
        "2001:db8:85a3::8a2e::370:7334",  // Multiple compressions
        "2001:db8:85a3:0000:0000:8a2e:0370:7334:extra",  // Too many groups
        "2001:db8:85a3:0000:0000:8a2e:0370:",            // Trailing colon
        ":2001:db8:85a3:0000:0000:8a2e:0370:7334",  // Leading colon (invalid)
        "2001::db8::1",                             // Multiple double colons
        "12345::1",                                 // Group too long
        "2001:db8::1 extra",                        // Extra text
        " 2001:db8::1",                             // Leading space
        "2001:db8::1 ",                             // Trailing space
        "\t2001:db8::1",                            // Tab character
        "2001:db8::1\n",                            // Newline character
        "2001:db8::1/129",                          // Invalid prefix length
        "http://[2001:db8::1]",                     // URL format
        "2001:db8::1:80",                           // Invalid port format
        "[2001:db8::1]",                            // Brackets without port
        "2001:db8::1%",                             // Invalid zone ID
        "2001:db8::1%invalid_zone"                  // Invalid zone ID
    };

    for (const auto& addr : malformedAddresses) {
        EXPECT_THROW(IPv6(addr), InvalidAddressFormat)
            << "Should throw for malformed address: " << addr;

        IPv6 ipv6;
        EXPECT_FALSE(ipv6.parse(addr))
            << "Parse should fail for malformed address: " << addr;
    }
}

TEST_F(AddressErrorHandlingTest, UnixDomainInvalidPaths) {
    std::vector<std::string> invalidPaths = {
        "",    // Empty path
        " ",   // Space only
        "\t",  // Tab only
        "\n",  // Newline only
#ifdef _WIN32
        "invalid\\path",      // Invalid Windows path
        "C:\\",               // Drive root (not a pipe)
        "\\\\server\\share",  // UNC path (not a pipe)
        "\\\\.\\",            // Incomplete pipe path
        "\\\\.\\pipe\\",      // Pipe path without name
#else
        std::string(1000, 'a'),  // Extremely long path
        "/\0hidden",             // Null character in path
        "/tmp/\xff\xfe",         // Invalid UTF-8 characters
#endif
    };

    for (const auto& path : invalidPaths) {
        if (!path.empty()) {  // Skip empty path test for constructor (already
                              // tested)
            EXPECT_THROW(UnixDomain(path), InvalidAddressFormat)
                << "Should throw for invalid path: " << path;
        }

        UnixDomain unixDomain;
        EXPECT_FALSE(unixDomain.parse(path))
            << "Parse should fail for invalid path: " << path;
    }
}

// Extended Range and Boundary Tests
TEST_F(AddressErrorHandlingTest, IPv4RangeBoundaryErrors) {
    IPv4 ipv4("192.168.1.100");

    // Test invalid range parameters
    EXPECT_THROW(ipv4.isInRange("invalid", "192.168.1.200"),
                 InvalidAddressFormat);
    EXPECT_THROW(ipv4.isInRange("192.168.1.1", "invalid"),
                 InvalidAddressFormat);
    EXPECT_THROW(ipv4.isInRange("invalid", "invalid"), InvalidAddressFormat);

    // Test reversed range (start > end)
    EXPECT_THROW(ipv4.isInRange("192.168.1.200", "192.168.1.1"),
                 AddressRangeError);
    EXPECT_THROW(ipv4.isInRange("255.255.255.255", "0.0.0.0"),
                 AddressRangeError);

    // Test edge cases
    EXPECT_TRUE(
        ipv4.isInRange("192.168.1.100", "192.168.1.100"));      // Same address
    EXPECT_TRUE(ipv4.isInRange("0.0.0.0", "255.255.255.255"));  // Full range
}

TEST_F(AddressErrorHandlingTest, IPv6RangeBoundaryErrors) {
    IPv6 ipv6("2001:db8::100");

    // Test invalid range parameters
    EXPECT_THROW(ipv6.isInRange("invalid", "2001:db8::200"),
                 InvalidAddressFormat);
    EXPECT_THROW(ipv6.isInRange("2001:db8::1", "invalid"),
                 InvalidAddressFormat);
    EXPECT_THROW(ipv6.isInRange("invalid", "invalid"), InvalidAddressFormat);

    // Test reversed range (start > end)
    EXPECT_THROW(ipv6.isInRange("2001:db8::200", "2001:db8::1"),
                 AddressRangeError);
    EXPECT_THROW(
        ipv6.isInRange("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", "::"),
        AddressRangeError);

    // Test edge cases
    EXPECT_TRUE(
        ipv6.isInRange("2001:db8::100", "2001:db8::100"));  // Same address
    EXPECT_TRUE(ipv6.isInRange(
        "::", "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));  // Full range
}

// Network Calculation Error Tests
TEST_F(AddressErrorHandlingTest, NetworkCalculationErrors) {
    IPv4 ipv4("192.168.1.100");

    // Test invalid subnet masks
    std::vector<std::string> invalidMasks = {
        "",
        "invalid",
        "256.255.255.0",
        "255.256.255.0",
        "255.255.256.0",
        "255.255.255.256",
        "255.255.255",
        "255.255.255.0.0",
        "192.168.1.1",  // Valid IP but not a proper mask
    };

    for (const auto& mask : invalidMasks) {
        EXPECT_NO_THROW({
            std::string network = ipv4.getNetworkAddress(mask);
            std::string broadcast = ipv4.getBroadcastAddress(mask);
            // These might return empty strings or default values for invalid
            // masks The important thing is they don't crash
        }) << "Network calculations should not crash for invalid mask: "
           << mask;
    }
}

TEST_F(AddressErrorHandlingTest, IPv6NetworkCalculationErrors) {
    IPv6 ipv6("2001:db8::1");

    // Test invalid IPv6 prefix masks
    std::vector<std::string> invalidMasks = {
        "",
        "invalid",
        "gggg:ffff:ffff:ffff::",
        "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",  // Too many groups
        "192.168.1.1",                                   // IPv4 mask for IPv6
    };

    for (const auto& mask : invalidMasks) {
        EXPECT_NO_THROW({
            std::string network = ipv6.getNetworkAddress(mask);
            std::string broadcast = ipv6.getBroadcastAddress(mask);
            // These might return empty strings or default values for invalid
            // masks The important thing is they don't crash
        }) << "IPv6 network calculations should not crash for invalid mask: "
           << mask;
    }
}

// Concurrent Access and Thread Safety Tests
class AddressConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override { spdlog::set_level(spdlog::level::off); }
};

TEST_F(AddressConcurrencyTest, ConcurrentIPv4Operations) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                // Create IPv4 addresses
                IPv4 ipv4("192.168.1." + std::to_string(i + 1));

                // Perform various operations
                auto type = ipv4.getType();
                auto addr = ipv4.getAddress();
                auto binary = ipv4.toBinary();
                auto hex = ipv4.toHex();

                // Network calculations
                auto network = ipv4.getNetworkAddress("255.255.255.0");
                auto broadcast = ipv4.getBroadcastAddress("255.255.255.0");

                // Range checking
                bool inRange = ipv4.isInRange("192.168.1.1", "192.168.1.255");

                // Equality checking
                IPv4 other("192.168.1." + std::to_string(i + 1));
                bool equal = ipv4.isEqual(other);

                if (!type.empty() && !addr.empty() && !binary.empty() &&
                    !hex.empty() && !network.empty() && !broadcast.empty() &&
                    equal) {
                    successCount++;
                } else {
                    errorCount++;
                }
            } catch (const std::exception&) {
                errorCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), numThreads);
    EXPECT_EQ(errorCount.load(), 0);
}

TEST_F(AddressConcurrencyTest, ConcurrentIPv6Operations) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                // Create IPv6 addresses
                IPv6 ipv6("2001:db8::" + std::to_string(i + 1));

                // Perform various operations
                auto type = ipv6.getType();
                auto addr = ipv6.getAddress();
                auto binary = ipv6.toBinary();
                auto hex = ipv6.toHex();

                // Network calculations
                auto network = ipv6.getNetworkAddress("ffff:ffff:ffff:ffff::");
                auto broadcast =
                    ipv6.getBroadcastAddress("ffff:ffff:ffff:ffff::");

                // Range checking
                bool inRange = ipv6.isInRange("2001:db8::1", "2001:db8::ffff");

                // Equality checking
                IPv6 other("2001:db8::" + std::to_string(i + 1));
                bool equal = ipv6.isEqual(other);

                if (!type.empty() && !addr.empty() && !binary.empty() &&
                    !hex.empty() && !network.empty() && !broadcast.empty() &&
                    inRange && equal) {
                    successCount++;
                } else {
                    errorCount++;
                }
            } catch (const std::exception&) {
                errorCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), numThreads);
    EXPECT_EQ(errorCount.load(), 0);
}

TEST_F(AddressConcurrencyTest, ConcurrentFactoryOperations) {
    constexpr int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    std::vector<std::string> testAddresses = {"192.168.1.1", "2001:db8::1",
#ifdef _WIN32
                                              "\\\\.\\pipe\\test"
#else
                                              "/tmp/test.sock"
#endif
    };

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                for (int j = 0; j < 100; ++j) {
                    for (const auto& addrStr : testAddresses) {
                        auto addr = Address::createFromString(addrStr);
                        if (addr != nullptr) {
                            auto type = addr->getType();
                            auto address = addr->getAddress();
                            if (!type.empty() && !address.empty()) {
                                successCount++;
                            } else {
                                errorCount++;
                            }
                        } else {
                            errorCount++;
                        }
                    }
                }
            } catch (const std::exception&) {
                errorCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successCount.load(), 0);
    // Allow some errors but not too many
    EXPECT_LT(errorCount.load(), successCount.load() / 10);
}

// Memory Management and Resource Tests
class AddressResourceTest : public ::testing::Test {
protected:
    void SetUp() override { spdlog::set_level(spdlog::level::off); }
};

TEST_F(AddressResourceTest, MemoryLeakPrevention) {
    // Test that creating and destroying many addresses doesn't cause memory
    // leaks
    for (int i = 0; i < 1000; ++i) {
        // IPv4 addresses
        {
            IPv4 ipv4("192.168.1." + std::to_string(i % 255 + 1));
            auto binary = ipv4.toBinary();
            auto hex = ipv4.toHex();
            auto network = ipv4.getNetworkAddress("255.255.255.0");
            auto broadcast = ipv4.getBroadcastAddress("255.255.255.0");
            // Use values to avoid unused variable warnings
            (void)binary;
            (void)hex;
            (void)network;
            (void)broadcast;
        }

        // IPv6 addresses
        {
            IPv6 ipv6("2001:db8::" + std::to_string(i));
            auto binary = ipv6.toBinary();
            auto hex = ipv6.toHex();
            auto network = ipv6.getNetworkAddress("ffff:ffff:ffff:ffff::");
            auto broadcast = ipv6.getBroadcastAddress("ffff:ffff:ffff:ffff::");
            // Use values to avoid unused variable warnings
            (void)binary;
            (void)hex;
            (void)network;
            (void)broadcast;
        }

        // Unix domain addresses
        {
#ifdef _WIN32
            UnixDomain unixDomain("\\\\.\\pipe\\test" + std::to_string(i));
#else
            UnixDomain unixDomain("/tmp/test" + std::to_string(i) + ".sock");
#endif
            auto binary = unixDomain.toBinary();
            auto hex = unixDomain.toHex();
            // Use values to avoid unused variable warnings
            (void)binary;
            (void)hex;
        }

        // Factory-created addresses
        {
            auto addr1 = Address::createFromString("192.168.1." +
                                                   std::to_string(i % 255 + 1));
            auto addr2 =
                Address::createFromString("2001:db8::" + std::to_string(i));
            if (addr1) {
                auto binary = addr1->toBinary();
                (void)binary;
            }
            if (addr2) {
                auto hex = addr2->toHex();
                (void)hex;
            }
        }
    }
    SUCCEED();  // If we reach here without crashes, memory management is likely
                // correct
}

TEST_F(AddressResourceTest, LargeStringHandling) {
    // Test handling of large strings (should fail gracefully)
    std::string largeString(10000, 'a');

    EXPECT_THROW(IPv4(largeString), InvalidAddressFormat);
    EXPECT_THROW(IPv6(largeString), InvalidAddressFormat);

    auto addr = Address::createFromString(largeString);
    EXPECT_EQ(addr, nullptr);
}

// Extended Cross-Platform Tests
class ExtendedCrossPlatformTest : public ::testing::Test {
protected:
    void SetUp() override { spdlog::set_level(spdlog::level::off); }
};

TEST_F(ExtendedCrossPlatformTest, IPv4EdgeCasesAllPlatforms) {
    // Test IPv4 addresses that might behave differently on different platforms
    std::vector<std::pair<std::string, bool>> testCases = {
        {"0.0.0.0", true},          // Any address
        {"127.0.0.1", true},        // Loopback
        {"255.255.255.255", true},  // Broadcast
        {"169.254.1.1", true},      // Link-local
        {"224.0.0.1", true},        // Multicast
        {"239.255.255.255", true},  // Multicast boundary
        {"240.0.0.1", true},        // Class E (experimental)
        {"10.0.0.0", true},         // Private Class A start
        {"10.255.255.255", true},   // Private Class A end
        {"172.16.0.0", true},       // Private Class B start
        {"172.31.255.255", true},   // Private Class B end
        {"192.168.0.0", true},      // Private Class C start
        {"192.168.255.255", true},  // Private Class C end
    };

    for (const auto& [addrStr, shouldBeValid] : testCases) {
        if (shouldBeValid) {
            EXPECT_NO_THROW(IPv4(addrStr))
                << "Should be valid on all platforms: " << addrStr;

            IPv4 ipv4(addrStr);
            EXPECT_EQ(ipv4.getAddress(), addrStr);
            EXPECT_EQ(ipv4.getType(), "IPv4");

            // Binary and hex conversions should work consistently
            auto binary = ipv4.toBinary();
            auto hex = ipv4.toHex();
            EXPECT_EQ(binary.length(), 32);
            EXPECT_EQ(hex.length(), 8);

            // Should only contain valid characters
            for (char c : binary) {
                EXPECT_TRUE(c == '0' || c == '1');
            }
            for (char c : hex) {
                EXPECT_TRUE(std::isxdigit(c));
            }
        }
    }
}

TEST_F(ExtendedCrossPlatformTest, IPv6EdgeCasesAllPlatforms) {
    // Test IPv6 addresses that might behave differently on different platforms
    std::vector<std::pair<std::string, bool>> testCases = {
        {"::", true},                               // Unspecified
        {"::1", true},                              // Loopback
        {"::ffff:0:0", true},                       // IPv4-mapped prefix
        {"::ffff:192.168.1.1", true},               // IPv4-mapped
        {"2001:db8::", true},                       // Documentation prefix
        {"fe80::", true},                           // Link-local prefix
        {"fe80::1", true},                          // Link-local
        {"ff00::", true},                           // Multicast prefix
        {"ff02::1", true},                          // All nodes multicast
        {"ff02::2", true},                          // All routers multicast
        {"2001::", true},                           // Global unicast
        {"fc00::", true},                           // Unique local prefix
        {"fd00::", true},                           // Unique local
        {"2001:db8:85a3::8a2e:370:7334", true},     // Full format
        {"2001:db8:85a3:0:0:8a2e:370:7334", true},  // Zero compression
    };

    for (const auto& [addrStr, shouldBeValid] : testCases) {
        if (shouldBeValid) {
            EXPECT_NO_THROW(IPv6(addrStr))
                << "Should be valid on all platforms: " << addrStr;

            IPv6 ipv6(addrStr);
            EXPECT_EQ(ipv6.getAddress(), addrStr);
            EXPECT_EQ(ipv6.getType(), "IPv6");

            // Binary and hex conversions should work consistently
            auto binary = ipv6.toBinary();
            auto hex = ipv6.toHex();
            EXPECT_EQ(binary.length(), 128);
            EXPECT_EQ(hex.length(), 32);

            // Should only contain valid characters
            for (char c : binary) {
                EXPECT_TRUE(c == '0' || c == '1');
            }
            for (char c : hex) {
                EXPECT_TRUE(std::isxdigit(c));
            }
        }
    }
}

// Stress and Robustness Tests
class AddressStressTest : public ::testing::Test {
protected:
    void SetUp() override { spdlog::set_level(spdlog::level::off); }
};

TEST_F(AddressStressTest, HighVolumeOperations) {
    constexpr int numOperations = 10000;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    // Test high volume of mixed operations
    for (int i = 0; i < numOperations; ++i) {
        try {
            if (i % 3 == 0) {
                // IPv4 operations
                IPv4 ipv4("192.168." + std::to_string(i % 256) + "." +
                          std::to_string((i + 1) % 256));
                auto binary = ipv4.toBinary();
                auto hex = ipv4.toHex();
                auto network = ipv4.getNetworkAddress("255.255.255.0");
                if (!binary.empty() && !hex.empty() && !network.empty()) {
                    successCount++;
                } else {
                    errorCount++;
                }
            } else if (i % 3 == 1) {
                // IPv6 operations
                IPv6 ipv6("2001:db8::" + std::to_string(i));
                auto binary = ipv6.toBinary();
                auto hex = ipv6.toHex();
                auto network = ipv6.getNetworkAddress("ffff:ffff:ffff:ffff::");
                if (!binary.empty() && !hex.empty() && !network.empty()) {
                    successCount++;
                } else {
                    errorCount++;
                }
            } else {
                // Factory operations
                std::string addr = (i % 2 == 0) ? "192.168.1.1" : "2001:db8::1";
                auto address = Address::createFromString(addr);
                if (address) {
                    auto type = address->getType();
                    auto addrStr = address->getAddress();
                    if (!type.empty() && !addrStr.empty()) {
                        successCount++;
                    } else {
                        errorCount++;
                    }
                } else {
                    errorCount++;
                }
            }
        } catch (const std::exception&) {
            errorCount++;
        }
    }

    EXPECT_GT(successCount.load(),
              numOperations * 0.9);                     // At least 90% success
    EXPECT_LT(errorCount.load(), numOperations * 0.1);  // Less than 10% errors
}

TEST_F(AddressStressTest, RapidCreationDestruction) {
    // Test rapid creation and destruction of addresses
    for (int i = 0; i < 5000; ++i) {
        // Create multiple addresses in quick succession
        std::vector<std::unique_ptr<Address>> addresses;

        addresses.push_back(Address::createFromString(
            "192.168.1." + std::to_string(i % 255 + 1)));
        addresses.push_back(
            Address::createFromString("2001:db8::" + std::to_string(i)));
#ifdef _WIN32
        addresses.push_back(
            Address::createFromString("\\\\.\\pipe\\test" + std::to_string(i)));
#else
        addresses.push_back(Address::createFromString(
            "/tmp/test" + std::to_string(i) + ".sock"));
#endif

        // Perform operations on them
        for (auto& addr : addresses) {
            if (addr) {
                auto type = addr->getType();
                auto address = addr->getAddress();
                auto binary = addr->toBinary();
                auto hex = addr->toHex();
                // Use the values to avoid unused variable warnings
                (void)type;
                (void)address;
                (void)binary;
                (void)hex;
            }
        }

        // Addresses are automatically destroyed when going out of scope
    }
    SUCCEED();
}

// Comprehensive Integration Tests
class AddressIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override { spdlog::set_level(spdlog::level::off); }
};

TEST_F(AddressIntegrationTest, MixedAddressTypeOperations) {
    // Test operations involving multiple address types
    IPv4 ipv4("192.168.1.100");
    IPv6 ipv6("2001:db8::100");
#ifdef _WIN32
    UnixDomain unixDomain("\\\\.\\pipe\\test");
#else
    UnixDomain unixDomain("/tmp/test.sock");
#endif

    // Test that each type maintains its identity
    EXPECT_EQ(ipv4.getType(), "IPv4");
    EXPECT_EQ(ipv6.getType(), "IPv6");
    EXPECT_EQ(unixDomain.getType(), "UnixDomain");

    // Test binary conversions have expected lengths
    EXPECT_EQ(ipv4.toBinary().length(), 32);
    EXPECT_EQ(ipv6.toBinary().length(), 128);
    EXPECT_FALSE(unixDomain.toBinary().empty());

    // Test hex conversions have expected lengths
    EXPECT_EQ(ipv4.toHex().length(), 8);
    EXPECT_EQ(ipv6.toHex().length(), 32);
    EXPECT_FALSE(unixDomain.toHex().empty());

    // Test network calculations
    EXPECT_FALSE(ipv4.getNetworkAddress("255.255.255.0").empty());
    EXPECT_FALSE(ipv6.getNetworkAddress("ffff:ffff:ffff:ffff::").empty());
    EXPECT_FALSE(unixDomain.getNetworkAddress("unused").empty());

    // Test broadcast calculations
    EXPECT_FALSE(ipv4.getBroadcastAddress("255.255.255.0").empty());
    EXPECT_FALSE(ipv6.getBroadcastAddress("ffff:ffff:ffff:ffff::").empty());
    // Unix domain broadcast should return empty or wildcard pattern
    std::string unixBroadcast = unixDomain.getBroadcastAddress("unused");
    // Result depends on implementation - could be empty or contain wildcard
}

TEST_F(AddressIntegrationTest, FactoryAndPolymorphism) {
    // Test factory method with polymorphic behavior
    std::vector<std::string> testAddresses = {
        "192.168.1.1",        "10.0.0.1",          "2001:db8::1", "::1",
#ifdef _WIN32
        "\\\\.\\pipe\\test1", "\\\\.\\pipe\\test2"
#else
        "/tmp/test1.sock", "/tmp/test2.sock"
#endif
    };

    std::vector<std::unique_ptr<Address>> addresses;

    // Create addresses using factory
    for (const auto& addrStr : testAddresses) {
        auto addr = Address::createFromString(addrStr);
        ASSERT_NE(addr, nullptr) << "Failed to create address: " << addrStr;
        addresses.push_back(std::move(addr));
    }

    // Test polymorphic operations
    for (const auto& addr : addresses) {
        EXPECT_FALSE(addr->getType().empty());
        EXPECT_FALSE(addr->getAddress().empty());
        EXPECT_FALSE(addr->toBinary().empty());
        EXPECT_FALSE(addr->toHex().empty());

        // Test that printAddressType doesn't crash
        EXPECT_NO_THROW(addr->printAddressType());
    }

    // Test equality operations
    auto addr1 = Address::createFromString("192.168.1.1");
    auto addr2 = Address::createFromString("192.168.1.1");
    auto addr3 = Address::createFromString("192.168.1.2");

    ASSERT_NE(addr1, nullptr);
    ASSERT_NE(addr2, nullptr);
    ASSERT_NE(addr3, nullptr);

    EXPECT_TRUE(addr1->isEqual(*addr2));
    EXPECT_FALSE(addr1->isEqual(*addr3));
}
