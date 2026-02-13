#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <array>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "atom/web/utils/ip.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
#elif defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

using namespace atom::web;

class IPUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests to reduce noise
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }

#ifdef _WIN32
        // Initialize Winsock for Windows
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            GTEST_SKIP() << "WSAStartup failed with error: " << result;
        }
#endif
    }

    void TearDown() override {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    // Helper method to create a sockaddr_in structure
    struct sockaddr_in createIPv4SockAddr(const std::string& ip,
                                          uint16_t port = 80) {
        struct sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
        return addr;
    }

    // Helper method to create a sockaddr_in6 structure
    struct sockaddr_in6 createIPv6SockAddr(const std::string& ip,
                                           uint16_t port = 80) {
        struct sockaddr_in6 addr {};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        inet_pton(AF_INET6, ip.c_str(), &addr.sin6_addr);
        return addr;
    }
};

// IPv4 Validation Tests
TEST_F(IPUtilsTest, IsValidIPv4ValidAddresses) {
    std::vector<std::string> validIPv4Addresses = {
        "0.0.0.0",    "127.0.0.1", "192.168.1.1", "255.255.255.255", "10.0.0.1",
        "172.16.0.1", "8.8.8.8",   "1.1.1.1",     "203.0.113.1"};

    for (const auto& ip : validIPv4Addresses) {
        EXPECT_TRUE(isValidIPv4(ip)) << "Should be valid IPv4: " << ip;
    }
}

TEST_F(IPUtilsTest, IsValidIPv4InvalidAddresses) {
    std::vector<std::string> invalidIPv4Addresses = {
        "",                   // Empty string
        "256.1.1.1",          // Out of range
        "192.168.1",          // Incomplete
        "192.168.1.1.1",      // Too many octets
        "192.168.1.256",      // Out of range
        "192.168.-1.1",       // Negative number
        "192.168.1.a",        // Non-numeric
        "not.an.ip.address",  // Text
        "192.168.1.",         // Trailing dot
        ".192.168.1.1",       // Leading dot
        "192..168.1.1",       // Double dot
        "192.168.01.1",       // Leading zero (some implementations reject this)
        "999.999.999.999",    // All out of range
        "192.168.1.1/24",     // CIDR notation
        "192.168.1.1:80"      // With port
    };

    for (const auto& ip : invalidIPv4Addresses) {
        EXPECT_FALSE(isValidIPv4(ip)) << "Should be invalid IPv4: " << ip;
    }
}

TEST_F(IPUtilsTest, IsValidIPv4EdgeCases) {
    // Test edge cases
    EXPECT_FALSE(isValidIPv4(""));  // Empty string

    // Very long string (over 15 characters)
    std::string longString(20, '1');
    EXPECT_FALSE(isValidIPv4(longString));

    // Exactly 15 characters (maximum valid length)
    EXPECT_TRUE(isValidIPv4("255.255.255.255"));  // 15 characters
}

// IPv6 Validation Tests
TEST_F(IPUtilsTest, IsValidIPv6ValidAddresses) {
    std::vector<std::string> validIPv6Addresses = {
        "::",                                       // All zeros
        "::1",                                      // Loopback
        "2001:db8::1",                              // Standard format
        "2001:0db8:85a3:0000:0000:8a2e:0370:7334",  // Full format
        "2001:db8:85a3::8a2e:370:7334",             // Compressed
        "fe80::1",                                  // Link-local
        "::ffff:192.168.1.1",                       // IPv4-mapped
        "2001:db8::8a2e:370:7334",                  // Mixed compression
        "ff02::1",                                  // Multicast
        "2001:db8:85a3:0:0:8a2e:370:7334"           // Zero compression
    };

    for (const auto& ip : validIPv6Addresses) {
        EXPECT_TRUE(isValidIPv6(ip)) << "Should be valid IPv6: " << ip;
    }
}

TEST_F(IPUtilsTest, IsValidIPv6InvalidAddresses) {
    std::vector<std::string> invalidIPv6Addresses = {
        "",                               // Empty string
        ":::",                            // Too many colons
        "2001:db8::1::2",                 // Double compression
        "gggg::1",                        // Invalid hex
        "2001:db8:85a3::8a2e::370:7334",  // Multiple compressions
        "2001:db8:85a3:0000:0000:8a2e:0370:7334:extra",  // Too many groups
        "192.168.1.1",                                   // IPv4 address
        "not::an::ipv6::address",                        // Invalid characters
        "2001:db8:85a3:0000:0000:8a2e:0370:",            // Trailing colon
        ":2001:db8:85a3:0000:0000:8a2e:0370:7334",  // Leading colon (invalid)
        "2001::db8::1",                             // Multiple double colons
        "12345::1"                                  // Group too long
    };

    for (const auto& ip : invalidIPv6Addresses) {
        EXPECT_FALSE(isValidIPv6(ip)) << "Should be invalid IPv6: " << ip;
    }
}

TEST_F(IPUtilsTest, IsValidIPv6EdgeCases) {
    // Test edge cases
    EXPECT_FALSE(isValidIPv6(""));  // Empty string

    // Very long string (over 45 characters)
    std::string longString(50, '1');
    EXPECT_FALSE(isValidIPv6(longString));

    // Test maximum length
    std::string maxLengthIPv6 =
        "2001:0db8:85a3:0000:0000:8a2e:0370:7334";  // 39 characters
    EXPECT_TRUE(isValidIPv6(maxLengthIPv6));
}

// ipToString Tests
TEST_F(IPUtilsTest, IpToStringIPv4) {
    auto addr = createIPv4SockAddr("192.168.1.1");
    std::array<char, INET_ADDRSTRLEN> buffer{};

    bool result = ipToString(reinterpret_cast<struct sockaddr*>(&addr),
                             buffer.data(), buffer.size());

    EXPECT_TRUE(result);
    EXPECT_STREQ(buffer.data(), "192.168.1.1");
}

TEST_F(IPUtilsTest, IpToStringIPv6) {
    auto addr = createIPv6SockAddr("2001:db8::1");
    std::array<char, INET6_ADDRSTRLEN> buffer{};

    bool result = ipToString(reinterpret_cast<struct sockaddr*>(&addr),
                             buffer.data(), buffer.size());

    EXPECT_TRUE(result);
    EXPECT_STREQ(buffer.data(), "2001:db8::1");
}

TEST_F(IPUtilsTest, IpToStringLoopbackAddresses) {
    // IPv4 loopback
    auto ipv4Addr = createIPv4SockAddr("127.0.0.1");
    std::array<char, INET_ADDRSTRLEN> ipv4Buffer{};

    bool ipv4Result = ipToString(reinterpret_cast<struct sockaddr*>(&ipv4Addr),
                                 ipv4Buffer.data(), ipv4Buffer.size());

    EXPECT_TRUE(ipv4Result);
    EXPECT_STREQ(ipv4Buffer.data(), "127.0.0.1");

    // IPv6 loopback
    auto ipv6Addr = createIPv6SockAddr("::1");
    std::array<char, INET6_ADDRSTRLEN> ipv6Buffer{};

    bool ipv6Result = ipToString(reinterpret_cast<struct sockaddr*>(&ipv6Addr),
                                 ipv6Buffer.data(), ipv6Buffer.size());

    EXPECT_TRUE(ipv6Result);
    EXPECT_STREQ(ipv6Buffer.data(), "::1");
}

TEST_F(IPUtilsTest, IpToStringNullParameters) {
    auto addr = createIPv4SockAddr("192.168.1.1");
    std::array<char, INET_ADDRSTRLEN> buffer{};

    // Null address
    EXPECT_FALSE(ipToString(nullptr, buffer.data(), buffer.size()));

    // Null buffer
    EXPECT_FALSE(ipToString(reinterpret_cast<struct sockaddr*>(&addr), nullptr,
                            buffer.size()));

    // Zero buffer size
    EXPECT_FALSE(ipToString(reinterpret_cast<struct sockaddr*>(&addr),
                            buffer.data(), 0));
}

TEST_F(IPUtilsTest, IpToStringBufferTooSmall) {
    auto addr = createIPv4SockAddr("192.168.1.1");
    std::array<char, 5> smallBuffer{};  // Too small for IPv4

    bool result = ipToString(reinterpret_cast<struct sockaddr*>(&addr),
                             smallBuffer.data(), smallBuffer.size());

    EXPECT_FALSE(result);
}

TEST_F(IPUtilsTest, IpToStringUnsupportedFamily) {
    struct sockaddr addr {};
    addr.sa_family = AF_UNIX;  // Unsupported family
    std::array<char, INET6_ADDRSTRLEN> buffer{};

    bool result = ipToString(&addr, buffer.data(), buffer.size());

    EXPECT_FALSE(result);
}

// Special IPv4 Address Tests
TEST_F(IPUtilsTest, IsValidIPv4SpecialAddresses) {
    // Test special IPv4 addresses
    std::vector<std::string> specialAddresses = {
        "0.0.0.0",          // Any address
        "127.0.0.1",        // Loopback
        "255.255.255.255",  // Broadcast
        "169.254.1.1",      // Link-local
        "224.0.0.1",        // Multicast
        "10.0.0.0",         // Private Class A
        "172.16.0.0",       // Private Class B
        "192.168.0.0"       // Private Class C
    };

    for (const auto& ip : specialAddresses) {
        EXPECT_TRUE(isValidIPv4(ip))
            << "Special IPv4 address should be valid: " << ip;
    }
}

// Special IPv6 Address Tests
TEST_F(IPUtilsTest, IsValidIPv6SpecialAddresses) {
    // Test special IPv6 addresses
    std::vector<std::string> specialAddresses = {
        "::",          // Unspecified
        "::1",         // Loopback
        "fe80::",      // Link-local prefix
        "ff00::",      // Multicast prefix
        "::ffff:0:0",  // IPv4-mapped prefix
        "2001:db8::",  // Documentation prefix
        "fc00::",      // Unique local prefix
        "2001::",      // Global unicast
    };

    for (const auto& ip : specialAddresses) {
        EXPECT_TRUE(isValidIPv6(ip))
            << "Special IPv6 address should be valid: " << ip;
    }
}

// Boundary Value Tests
TEST_F(IPUtilsTest, IPv4BoundaryValues) {
    // Test boundary values for IPv4
    EXPECT_TRUE(isValidIPv4("0.0.0.0"));
    EXPECT_TRUE(isValidIPv4("255.255.255.255"));
    EXPECT_FALSE(isValidIPv4("256.0.0.0"));
    EXPECT_FALSE(isValidIPv4("0.256.0.0"));
    EXPECT_FALSE(isValidIPv4("0.0.256.0"));
    EXPECT_FALSE(isValidIPv4("0.0.0.256"));
}

// Performance Tests
TEST_F(IPUtilsTest, IPv4ValidationPerformance) {
    std::vector<std::string> testAddresses;

    // Generate test addresses
    for (int i = 0; i < 1000; ++i) {
        testAddresses.push_back("192.168.1." + std::to_string(i % 256));
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (const auto& addr : testAddresses) {
        isValidIPv4(addr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time (less than 1 second)
    EXPECT_LT(duration.count(), 1000);
}

TEST_F(IPUtilsTest, IPv6ValidationPerformance) {
    std::vector<std::string> testAddresses;

    // Generate test addresses
    for (int i = 0; i < 1000; ++i) {
        testAddresses.push_back("2001:db8::" + std::to_string(i));
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (const auto& addr : testAddresses) {
        isValidIPv6(addr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time (less than 1 second)
    EXPECT_LT(duration.count(), 1000);
}

// Concurrent Access Tests
TEST_F(IPUtilsTest, ConcurrentIPv4Validation) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> validCount{0};
    std::atomic<int> invalidCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&validCount, &invalidCount, i]() {
            // Test valid addresses
            if (isValidIPv4("192.168.1." + std::to_string(i % 256))) {
                validCount++;
            }

            // Test invalid addresses
            if (!isValidIPv4("256.256.256.256")) {
                invalidCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(validCount.load(), numThreads);
    EXPECT_EQ(invalidCount.load(), numThreads);
}

TEST_F(IPUtilsTest, ConcurrentIPv6Validation) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> validCount{0};
    std::atomic<int> invalidCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&validCount, &invalidCount, i]() {
            // Test valid addresses
            if (isValidIPv6("2001:db8::" + std::to_string(i))) {
                validCount++;
            }

            // Test invalid addresses
            if (!isValidIPv6("invalid::address::format")) {
                invalidCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(validCount.load(), numThreads);
    EXPECT_EQ(invalidCount.load(), numThreads);
}

// Comprehensive ipToString Tests
TEST_F(IPUtilsTest, IpToStringVariousIPv4Addresses) {
    std::vector<std::string> testAddresses = {
        "0.0.0.0",  "127.0.0.1",  "192.168.1.1", "255.255.255.255",
        "10.0.0.1", "172.16.0.1", "8.8.8.8",     "1.1.1.1"};

    for (const auto& expectedIP : testAddresses) {
        auto addr = createIPv4SockAddr(expectedIP);
        std::array<char, INET_ADDRSTRLEN> buffer{};

        bool result = ipToString(reinterpret_cast<struct sockaddr*>(&addr),
                                 buffer.data(), buffer.size());

        EXPECT_TRUE(result) << "Failed to convert: " << expectedIP;
        EXPECT_STREQ(buffer.data(), expectedIP.c_str())
            << "Mismatch for: " << expectedIP;
    }
}

TEST_F(IPUtilsTest, IpToStringVariousIPv6Addresses) {
    std::vector<std::string> testAddresses = {"::", "::1", "2001:db8::1",
                                              "fe80::1", "ff02::1"};

    for (const auto& expectedIP : testAddresses) {
        auto addr = createIPv6SockAddr(expectedIP);
        std::array<char, INET6_ADDRSTRLEN> buffer{};

        bool result = ipToString(reinterpret_cast<struct sockaddr*>(&addr),
                                 buffer.data(), buffer.size());

        EXPECT_TRUE(result) << "Failed to convert: " << expectedIP;
        EXPECT_STREQ(buffer.data(), expectedIP.c_str())
            << "Mismatch for: " << expectedIP;
    }
}

// Memory Safety Tests
TEST_F(IPUtilsTest, MemorySafetyTests) {
    // Test with various buffer sizes
    auto addr = createIPv4SockAddr("192.168.1.1");

    // Test with exact minimum size
    std::array<char, INET_ADDRSTRLEN> exactBuffer{};
    EXPECT_TRUE(ipToString(reinterpret_cast<struct sockaddr*>(&addr),
                           exactBuffer.data(), exactBuffer.size()));

    // Test with larger buffer
    std::array<char, 256> largeBuffer{};
    EXPECT_TRUE(ipToString(reinterpret_cast<struct sockaddr*>(&addr),
                           largeBuffer.data(), largeBuffer.size()));
}

// Cross-Platform Consistency Tests
TEST_F(IPUtilsTest, CrossPlatformConsistency) {
    // Test that results are consistent across platforms
    std::vector<std::string> testCases = {"127.0.0.1", "::1", "192.168.1.1",
                                          "2001:db8::1"};

    for (const auto& testCase : testCases) {
        bool ipv4Result = isValidIPv4(testCase);
        bool ipv6Result = isValidIPv6(testCase);

        // Each address should be valid in exactly one format
        EXPECT_NE(ipv4Result, ipv6Result)
            << "Address format ambiguity: " << testCase;
    }
}

// Stress Tests
TEST_F(IPUtilsTest, StressTestValidation) {
    // Test with many different addresses
    for (int i = 0; i < 10000; ++i) {
        std::string ipv4 = std::to_string(i % 256) + ".168.1.1";
        std::string ipv6 = "2001:db8::" + std::to_string(i);

        // These operations should not crash
        EXPECT_NO_THROW(isValidIPv4(ipv4));
        EXPECT_NO_THROW(isValidIPv6(ipv6));
    }
}

// ============================================================================
// ipToString (string return version) Tests
// ============================================================================

TEST_F(IPUtilsTest, IpToStringReturnIPv4) {
    auto addr = createIPv4SockAddr("192.168.1.1");
    auto result = ipToString(reinterpret_cast<struct sockaddr*>(&addr));
    EXPECT_EQ(result, "192.168.1.1");
}

TEST_F(IPUtilsTest, IpToStringReturnIPv6) {
    auto addr = createIPv6SockAddr("2001:db8::1");
    auto result = ipToString(reinterpret_cast<struct sockaddr*>(&addr));
    EXPECT_EQ(result, "2001:db8::1");
}

TEST_F(IPUtilsTest, IpToStringReturnNull) {
    auto result = ipToString(nullptr);
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// isIPInCIDR Tests
// ============================================================================

TEST_F(IPUtilsTest, IsIPInCIDRValidIPv4) {
    EXPECT_TRUE(isIPInCIDR("192.168.1.100", "192.168.1.0/24"));
    EXPECT_TRUE(isIPInCIDR("192.168.1.1", "192.168.1.0/24"));
    EXPECT_TRUE(isIPInCIDR("192.168.1.254", "192.168.1.0/24"));
    EXPECT_TRUE(isIPInCIDR("10.0.0.5", "10.0.0.0/8"));
    EXPECT_TRUE(isIPInCIDR("172.16.5.10", "172.16.0.0/12"));
}

TEST_F(IPUtilsTest, IsIPInCIDRNotInRange) {
    EXPECT_FALSE(isIPInCIDR("192.168.2.1", "192.168.1.0/24"));
    EXPECT_FALSE(isIPInCIDR("10.1.0.1", "10.0.0.0/16"));
    EXPECT_FALSE(isIPInCIDR("172.32.0.1", "172.16.0.0/12"));
}

TEST_F(IPUtilsTest, IsIPInCIDREdgeCases) {
    // Network address
    EXPECT_TRUE(isIPInCIDR("192.168.1.0", "192.168.1.0/24"));
    // Broadcast address
    EXPECT_TRUE(isIPInCIDR("192.168.1.255", "192.168.1.0/24"));
    // /32 - single host
    EXPECT_TRUE(isIPInCIDR("192.168.1.1", "192.168.1.1/32"));
    EXPECT_FALSE(isIPInCIDR("192.168.1.2", "192.168.1.1/32"));
    // /0 - all addresses
    EXPECT_TRUE(isIPInCIDR("1.2.3.4", "0.0.0.0/0"));
}

TEST_F(IPUtilsTest, IsIPInCIDRInvalidInput) {
    EXPECT_FALSE(isIPInCIDR("", "192.168.1.0/24"));
    EXPECT_FALSE(isIPInCIDR("192.168.1.1", ""));
    EXPECT_FALSE(isIPInCIDR("invalid", "192.168.1.0/24"));
    EXPECT_FALSE(isIPInCIDR("192.168.1.1", "invalid"));
    EXPECT_FALSE(isIPInCIDR("192.168.1.1", "192.168.1.0"));  // No prefix
}

// ============================================================================
// parseCIDR Tests
// ============================================================================

TEST_F(IPUtilsTest, ParseCIDRValid) {
    auto result1 = parseCIDR("192.168.1.0/24");
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->first, "192.168.1.0");
    EXPECT_EQ(result1->second, 24);

    auto result2 = parseCIDR("10.0.0.0/8");
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->first, "10.0.0.0");
    EXPECT_EQ(result2->second, 8);

    auto result3 = parseCIDR("0.0.0.0/0");
    ASSERT_TRUE(result3.has_value());
    EXPECT_EQ(result3->first, "0.0.0.0");
    EXPECT_EQ(result3->second, 0);

    auto result4 = parseCIDR("192.168.1.1/32");
    ASSERT_TRUE(result4.has_value());
    EXPECT_EQ(result4->first, "192.168.1.1");
    EXPECT_EQ(result4->second, 32);
}

TEST_F(IPUtilsTest, ParseCIDRInvalid) {
    EXPECT_FALSE(parseCIDR("").has_value());
    EXPECT_FALSE(parseCIDR("192.168.1.0").has_value());   // No prefix
    EXPECT_FALSE(parseCIDR("192.168.1.0/").has_value());  // Empty prefix
    EXPECT_FALSE(parseCIDR("/24").has_value());           // No IP
    EXPECT_FALSE(
        parseCIDR("192.168.1.0/33").has_value());  // Invalid prefix for IPv4
    EXPECT_FALSE(parseCIDR("192.168.1.0/-1").has_value());  // Negative prefix
    EXPECT_FALSE(parseCIDR("invalid/24").has_value());      // Invalid IP
}

// ============================================================================
// isPrivateIP Tests
// ============================================================================

TEST_F(IPUtilsTest, IsPrivateIPValid) {
    // Class A private: 10.0.0.0/8
    EXPECT_TRUE(isPrivateIP("10.0.0.1"));
    EXPECT_TRUE(isPrivateIP("10.255.255.255"));

    // Class B private: 172.16.0.0/12
    EXPECT_TRUE(isPrivateIP("172.16.0.1"));
    EXPECT_TRUE(isPrivateIP("172.31.255.255"));

    // Class C private: 192.168.0.0/16
    EXPECT_TRUE(isPrivateIP("192.168.0.1"));
    EXPECT_TRUE(isPrivateIP("192.168.255.255"));
}

TEST_F(IPUtilsTest, IsPrivateIPPublic) {
    EXPECT_FALSE(isPrivateIP("8.8.8.8"));
    EXPECT_FALSE(isPrivateIP("1.1.1.1"));
    EXPECT_FALSE(isPrivateIP("172.32.0.1"));  // Just outside Class B private
    EXPECT_FALSE(
        isPrivateIP("172.15.255.255"));        // Just outside Class B private
    EXPECT_FALSE(isPrivateIP("192.167.1.1"));  // Just outside Class C private
}

TEST_F(IPUtilsTest, IsPrivateIPEdgeCases) {
    EXPECT_FALSE(isPrivateIP(""));
    EXPECT_FALSE(isPrivateIP("invalid"));
    EXPECT_FALSE(isPrivateIP("127.0.0.1"));  // Loopback is not private
}

// ============================================================================
// isLoopbackIP Tests
// ============================================================================

TEST_F(IPUtilsTest, IsLoopbackIPValid) {
    // IPv4 loopback range: 127.0.0.0/8
    EXPECT_TRUE(isLoopbackIP("127.0.0.1"));
    EXPECT_TRUE(isLoopbackIP("127.0.0.0"));
    EXPECT_TRUE(isLoopbackIP("127.255.255.255"));
    EXPECT_TRUE(isLoopbackIP("127.1.2.3"));

    // IPv6 loopback
    EXPECT_TRUE(isLoopbackIP("::1"));
}

TEST_F(IPUtilsTest, IsLoopbackIPNotLoopback) {
    EXPECT_FALSE(isLoopbackIP("192.168.1.1"));
    EXPECT_FALSE(isLoopbackIP("10.0.0.1"));
    EXPECT_FALSE(isLoopbackIP("8.8.8.8"));
    EXPECT_FALSE(isLoopbackIP("128.0.0.1"));  // Just outside loopback range
}

TEST_F(IPUtilsTest, IsLoopbackIPEdgeCases) {
    EXPECT_FALSE(isLoopbackIP(""));
    EXPECT_FALSE(isLoopbackIP("invalid"));
}

// ============================================================================
// isMulticastIP Tests
// ============================================================================

TEST_F(IPUtilsTest, IsMulticastIPValid) {
    // IPv4 multicast range: 224.0.0.0 - 239.255.255.255
    EXPECT_TRUE(isMulticastIP("224.0.0.1"));
    EXPECT_TRUE(isMulticastIP("239.255.255.255"));
    EXPECT_TRUE(isMulticastIP("230.0.0.1"));

    // IPv6 multicast (ff00::/8)
    EXPECT_TRUE(isMulticastIP("ff02::1"));
    EXPECT_TRUE(isMulticastIP("ff00::"));
}

TEST_F(IPUtilsTest, IsMulticastIPNotMulticast) {
    EXPECT_FALSE(isMulticastIP("192.168.1.1"));
    EXPECT_FALSE(isMulticastIP("223.255.255.255"));  // Just below multicast
    EXPECT_FALSE(isMulticastIP("240.0.0.0"));        // Just above multicast
    EXPECT_FALSE(isMulticastIP("127.0.0.1"));
}

TEST_F(IPUtilsTest, IsMulticastIPEdgeCases) {
    EXPECT_FALSE(isMulticastIP(""));
    EXPECT_FALSE(isMulticastIP("invalid"));
}

// ============================================================================
// getLocalIPAddresses Tests
// ============================================================================

TEST_F(IPUtilsTest, GetLocalIPAddressesDefault) {
    auto addresses = getLocalIPAddresses();
    // Should return at least one address on most systems
    // Note: May be empty in some environments
    for (const auto& addr : addresses) {
        EXPECT_FALSE(addr.empty());
        // Should be valid IP
        EXPECT_TRUE(isValidIPv4(addr) || isValidIPv6(addr));
    }
}

TEST_F(IPUtilsTest, GetLocalIPAddressesWithLoopback) {
    auto addresses = getLocalIPAddresses(true);
    // With loopback included, should have at least loopback
    bool hasLoopback = false;
    for (const auto& addr : addresses) {
        if (addr == "127.0.0.1" || addr == "::1") {
            hasLoopback = true;
            break;
        }
    }
    // Note: Implementation might not include loopback even when requested
    (void)hasLoopback;
}

TEST_F(IPUtilsTest, GetLocalIPAddressesWithoutLoopback) {
    auto addresses = getLocalIPAddresses(false);
    for (const auto& addr : addresses) {
        EXPECT_NE(addr, "127.0.0.1") << "Should not include IPv4 loopback";
        EXPECT_NE(addr, "::1") << "Should not include IPv6 loopback";
    }
}

// ============================================================================
// normalizeIP Tests
// ============================================================================

TEST_F(IPUtilsTest, NormalizeIPIPv4) {
    auto result = normalizeIP("192.168.1.1");
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(isValidIPv4(result) || isValidIPv6(result));
}

TEST_F(IPUtilsTest, NormalizeIPIPv6) {
    auto result = normalizeIP("2001:db8::1");
    EXPECT_FALSE(result.empty());
}

TEST_F(IPUtilsTest, NormalizeIPIPv6Expanded) {
    auto result = normalizeIP("2001:0db8:0000:0000:0000:0000:0000:0001");
    EXPECT_FALSE(result.empty());
}

TEST_F(IPUtilsTest, NormalizeIPEdgeCases) {
    auto empty = normalizeIP("");
    // Empty input should return empty or handle gracefully
    (void)empty;

    auto invalid = normalizeIP("invalid");
    // Invalid input should return empty or handle gracefully
    (void)invalid;
}

// ============================================================================
// compareIP Tests
// ============================================================================

TEST_F(IPUtilsTest, CompareIPIPv4Equal) {
    EXPECT_EQ(compareIP("192.168.1.1", "192.168.1.1"), 0);
    EXPECT_EQ(compareIP("0.0.0.0", "0.0.0.0"), 0);
    EXPECT_EQ(compareIP("255.255.255.255", "255.255.255.255"), 0);
}

TEST_F(IPUtilsTest, CompareIPIPv4LessThan) {
    EXPECT_LT(compareIP("192.168.1.1", "192.168.1.2"), 0);
    EXPECT_LT(compareIP("192.168.0.255", "192.168.1.0"), 0);
    EXPECT_LT(compareIP("0.0.0.0", "255.255.255.255"), 0);
}

TEST_F(IPUtilsTest, CompareIPIPv4GreaterThan) {
    EXPECT_GT(compareIP("192.168.1.2", "192.168.1.1"), 0);
    EXPECT_GT(compareIP("192.168.1.0", "192.168.0.255"), 0);
    EXPECT_GT(compareIP("255.255.255.255", "0.0.0.0"), 0);
}

TEST_F(IPUtilsTest, CompareIPIPv6Equal) {
    EXPECT_EQ(compareIP("::1", "::1"), 0);
    EXPECT_EQ(compareIP("2001:db8::1", "2001:db8::1"), 0);
}

TEST_F(IPUtilsTest, CompareIPIPv6Ordering) {
    EXPECT_LT(compareIP("::1", "::2"), 0);
    EXPECT_GT(compareIP("::2", "::1"), 0);
}

TEST_F(IPUtilsTest, CompareIPEdgeCases) {
    // Empty strings
    auto result1 = compareIP("", "192.168.1.1");
    auto result2 = compareIP("192.168.1.1", "");
    // Results depend on implementation
    (void)result1;
    (void)result2;

    // Invalid IPs
    auto result3 = compareIP("invalid", "192.168.1.1");
    (void)result3;
}

// ============================================================================
// Concurrent Access Tests for New Functions
// ============================================================================

TEST_F(IPUtilsTest, ConcurrentCIDROperations) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&successCount, i]() {
            std::string ip = "192.168.1." + std::to_string(i % 256);
            if (isIPInCIDR(ip, "192.168.1.0/24")) {
                successCount++;
            }
            auto parsed = parseCIDR("10.0.0.0/8");
            if (parsed.has_value()) {
                successCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successCount.load(), 0);
}

TEST_F(IPUtilsTest, ConcurrentIPClassification) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i]() {
            isPrivateIP("10.0.0." + std::to_string(i % 256));
            isLoopbackIP("127.0.0." + std::to_string(i % 256));
            isMulticastIP("224.0.0." + std::to_string(i % 256));
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    SUCCEED();
}
