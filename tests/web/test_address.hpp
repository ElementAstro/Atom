// filepath: /home/max/Atom-1/atom/web/test_address.cpp

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <memory>

#include "address.hpp"
#include "atom/log/loguru.hpp"

using namespace atom::web;
using ::testing::HasSubstr;

class UnixDomainTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing
        static bool initialized = false;
        if (!initialized) {
            loguru::init(0, nullptr);
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            initialized = true;
        }
    }
};

// Test that getBroadcastAddress returns an empty string for Unix domain sockets
TEST_F(UnixDomainTest, GetBroadcastAddressReturnsEmptyString) {
    UnixDomain unixDomain("/tmp/test.sock");
    EXPECT_TRUE(unixDomain.getBroadcastAddress("255.255.255.0").empty());
}

// Test that getBroadcastAddress logs a warning for Unix domain sockets
TEST_F(UnixDomainTest, GetBroadcastAddressLogsWarning) {
    // Capture log output
    std::string logOutput;
    loguru::set_fatal_handler([&logOutput](const loguru::Message& message) {
        logOutput = message.message;
        return false;
    });

    // Redirect the ERROR messages to our capture handler
    loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
    loguru::add_callback("test_callback",
                         [&logOutput](void*, const loguru::Message& message) {
                             logOutput = message.message;
                         },
                         loguru::Verbosity_WARNING);

    // Execute the method that should log a warning
    UnixDomain unixDomain("/tmp/test.sock");
    unixDomain.getBroadcastAddress("255.255.255.0");

    // Check if the log contains the expected warning message
    EXPECT_THAT(logOutput, HasSubstr("getBroadcastAddress operation not applicable for Unix domain sockets"));

    // Clean up
    loguru::remove_callback("test_callback");
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
    unixDomain.getType();
    unixDomain.toBinary();
    unixDomain.toHex();

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

    // For Unix domain sockets, getBroadcastAddress should return an empty string
    EXPECT_TRUE(unixDomain.getBroadcastAddress("255.255.255.0").empty());

    // For IPv4, getBroadcastAddress should return a valid address
    EXPECT_FALSE(ipv4.getBroadcastAddress("255.255.255.0").empty());
    EXPECT_EQ(ipv4.getBroadcastAddress("255.255.255.0"), "192.168.1.255");

    // For IPv6, behavior depends on implementation details
    // (not testing exact result here since it's complex)
    EXPECT_NO_THROW(ipv6.getBroadcastAddress("ffff:ffff:ffff:ffff::"));
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
    UnixDomain unixDomain("/a"); // Shortest valid path
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
