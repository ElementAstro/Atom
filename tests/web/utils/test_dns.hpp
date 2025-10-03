// filepath: tests/web/utils/test_dns.hpp
#ifndef TEST_DNS_HPP
#define TEST_DNS_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include <spdlog/spdlog.h>

#include "atom/web/utils/dns.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
#endif

using namespace atom::web;

class DNSTest : public ::testing::Test {
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

        // Set a reasonable TTL for testing
        setDNSCacheTTL(std::chrono::seconds(60));
    }

    void TearDown() override {
        // Clear any expired entries after each test
        clearDNSCacheExpiredEntries();

#ifdef _WIN32
        WSACleanup();
#endif
    }

    // Helper method to check internet connectivity
    bool hasInternetConnectivity() {
        try {
            auto addresses = getIPAddresses("8.8.8.8");
            return !addresses.empty();
        } catch (const std::exception&) {
            return false;
        }
    }

    // Helper method to validate IP address format
    bool isValidIPAddress(const std::string& ip) {
        // Simple validation - check if it contains dots (IPv4) or colons (IPv6)
        return (ip.find('.') != std::string::npos) || (ip.find(':') != std::string::npos);
    }
};

// Basic getIPAddresses Tests
TEST_F(DNSTest, GetIPAddressesValidHostname) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    ASSERT_NO_THROW({
        auto addresses = getIPAddresses("google.com");
        EXPECT_FALSE(addresses.empty());
        
        for (const auto& addr : addresses) {
            EXPECT_TRUE(isValidIPAddress(addr)) << "Invalid IP address: " << addr;
        }
    });
}

TEST_F(DNSTest, GetIPAddressesLocalhost) {
    ASSERT_NO_THROW({
        auto addresses = getIPAddresses("localhost");
        EXPECT_FALSE(addresses.empty());
        
        // Should contain at least 127.0.0.1 or ::1
        bool hasLoopback = false;
        for (const auto& addr : addresses) {
            if (addr == "127.0.0.1" || addr == "::1") {
                hasLoopback = true;
                break;
            }
        }
        EXPECT_TRUE(hasLoopback);
    });
}

TEST_F(DNSTest, GetIPAddressesIPAddress) {
    ASSERT_NO_THROW({
        auto addresses = getIPAddresses("127.0.0.1");
        EXPECT_FALSE(addresses.empty());
        EXPECT_THAT(addresses, ::testing::Contains("127.0.0.1"));
    });
}

TEST_F(DNSTest, GetIPAddressesEmptyHostname) {
    auto addresses = getIPAddresses("");
    EXPECT_TRUE(addresses.empty());
}

TEST_F(DNSTest, GetIPAddressesInvalidHostname) {
    auto addresses = getIPAddresses("invalid.nonexistent.domain.xyz.invalid");
    EXPECT_TRUE(addresses.empty());
}

// getLocalIPAddresses Tests
TEST_F(DNSTest, GetLocalIPAddresses) {
    ASSERT_NO_THROW({
        auto addresses = getLocalIPAddresses();
        // Should have at least one local IP address
        EXPECT_FALSE(addresses.empty());
        
        for (const auto& addr : addresses) {
            EXPECT_TRUE(isValidIPAddress(addr)) << "Invalid IP address: " << addr;
            // Should not contain loopback addresses
            EXPECT_NE(addr, "127.0.0.1");
            EXPECT_NE(addr, "::1");
        }
    });
}

// DNS Cache Tests
TEST_F(DNSTest, DNSCacheTTLSetting) {
    ASSERT_NO_THROW({
        setDNSCacheTTL(std::chrono::seconds(30));
        setDNSCacheTTL(std::chrono::seconds(300));
        setDNSCacheTTL(std::chrono::seconds(3600));
    });
}

TEST_F(DNSTest, DNSCacheExpiredEntriesClearing) {
    ASSERT_NO_THROW({
        clearDNSCacheExpiredEntries();
    });
}

TEST_F(DNSTest, DNSCacheFunctionality) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    // Set a short TTL for testing
    setDNSCacheTTL(std::chrono::seconds(2));

    // First call should populate cache
    auto addresses1 = getIPAddresses("google.com");
    EXPECT_FALSE(addresses1.empty());

    // Second call should use cache (should be faster)
    auto start = std::chrono::high_resolution_clock::now();
    auto addresses2 = getIPAddresses("google.com");
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(addresses1, addresses2);
    // Cached lookup should be very fast (less than 10ms)
    EXPECT_LT(duration.count(), 10);

    // Wait for cache to expire
    std::this_thread::sleep_for(std::chrono::seconds(3));
    clearDNSCacheExpiredEntries();

    // Third call should repopulate cache
    auto addresses3 = getIPAddresses("google.com");
    EXPECT_FALSE(addresses3.empty());
}

// Edge Cases and Error Handling Tests
TEST_F(DNSTest, GetIPAddressesWithDifferentHostnames) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    std::vector<std::string> hostnames = {
        "google.com", "github.com", "stackoverflow.com", "wikipedia.org"
    };

    for (const auto& hostname : hostnames) {
        ASSERT_NO_THROW({
            auto addresses = getIPAddresses(hostname);
            EXPECT_FALSE(addresses.empty()) << "No addresses for: " << hostname;
            
            for (const auto& addr : addresses) {
                EXPECT_TRUE(isValidIPAddress(addr)) 
                    << "Invalid IP for " << hostname << ": " << addr;
            }
        }) << "Failed for hostname: " << hostname;
    }
}

TEST_F(DNSTest, GetIPAddressesSpecialCharacters) {
    // Test with various invalid characters
    std::vector<std::string> invalidHostnames = {
        "host with spaces.com",
        "host@with#symbols.com",
        "host.with..double.dots.com",
        ".leading.dot.com",
        "trailing.dot.com.",
        "very-long-hostname-that-exceeds-normal-limits-and-should-probably-fail-or-at-least-be-handled-gracefully.com"
    };

    for (const auto& hostname : invalidHostnames) {
        auto addresses = getIPAddresses(hostname);
        // These should either return empty or handle gracefully
        // We don't expect them to succeed, but they shouldn't crash
        EXPECT_NO_THROW(getIPAddresses(hostname)) << "Crashed on: " << hostname;
    }
}

// Performance and Stress Tests
TEST_F(DNSTest, ConcurrentDNSResolution) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    constexpr int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&successCount, &errorCount]() {
            try {
                auto addresses = getIPAddresses("google.com");
                if (!addresses.empty()) {
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

    EXPECT_GT(successCount.load(), 0);
    EXPECT_EQ(successCount.load() + errorCount.load(), numThreads);
}

TEST_F(DNSTest, ConcurrentLocalIPAddresses) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&successCount]() {
            try {
                auto addresses = getLocalIPAddresses();
                if (!addresses.empty()) {
                    successCount++;
                }
            } catch (const std::exception&) {
                // Error is acceptable but shouldn't crash
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successCount.load(), 0);
}

// Memory Management Tests
TEST_F(DNSTest, MemoryLeakPrevention) {
    // Test that multiple operations don't cause memory leaks
    for (int i = 0; i < 50; ++i) {
        auto addresses = getIPAddresses("localhost");
        auto localAddresses = getLocalIPAddresses();
        clearDNSCacheExpiredEntries();
    }
    // If we reach here without crashes, memory management is likely correct
    SUCCEED();
}

// IPv4 vs IPv6 Resolution Tests
TEST_F(DNSTest, IPv4AndIPv6Resolution) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    auto addresses = getIPAddresses("google.com");
    if (addresses.empty()) {
        GTEST_SKIP() << "Could not resolve google.com";
    }

    bool hasIPv4 = false;
    bool hasIPv6 = false;

    for (const auto& addr : addresses) {
        if (addr.find('.') != std::string::npos) {
            hasIPv4 = true;
        } else if (addr.find(':') != std::string::npos) {
            hasIPv6 = true;
        }
    }

    // Should have at least one type of address
    EXPECT_TRUE(hasIPv4 || hasIPv6);
}

// Cache Behavior Tests
TEST_F(DNSTest, CacheConsistency) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    // Multiple calls should return consistent results from cache
    auto addresses1 = getIPAddresses("google.com");
    auto addresses2 = getIPAddresses("google.com");
    auto addresses3 = getIPAddresses("google.com");

    EXPECT_EQ(addresses1, addresses2);
    EXPECT_EQ(addresses2, addresses3);
}

TEST_F(DNSTest, CacheExpiration) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    // Set very short TTL
    setDNSCacheTTL(std::chrono::seconds(1));

    auto addresses1 = getIPAddresses("google.com");
    EXPECT_FALSE(addresses1.empty());

    // Wait for cache to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));
    clearDNSCacheExpiredEntries();

    // Should still work after cache expiration
    auto addresses2 = getIPAddresses("google.com");
    EXPECT_FALSE(addresses2.empty());
}

// Local IP Address Validation Tests
TEST_F(DNSTest, LocalIPAddressValidation) {
    auto addresses = getLocalIPAddresses();

    for (const auto& addr : addresses) {
        // Should be valid IP addresses
        EXPECT_TRUE(isValidIPAddress(addr));

        // Should not be loopback addresses
        EXPECT_NE(addr, "127.0.0.1");
        EXPECT_NE(addr, "::1");

        // Should not be empty
        EXPECT_FALSE(addr.empty());

        // IPv4 addresses should have proper format
        if (addr.find('.') != std::string::npos) {
            size_t dotCount = 0;
            for (char c : addr) {
                if (c == '.') dotCount++;
            }
            EXPECT_EQ(dotCount, 3) << "Invalid IPv4 format: " << addr;
        }

        // IPv6 addresses should have proper format
        if (addr.find(':') != std::string::npos) {
            EXPECT_GT(addr.length(), 2) << "Invalid IPv6 format: " << addr;
        }
    }
}

// Error Recovery Tests
TEST_F(DNSTest, ErrorRecoveryAfterFailure) {
    // Try to resolve invalid hostname
    auto invalidAddresses = getIPAddresses("invalid.nonexistent.domain.xyz");
    EXPECT_TRUE(invalidAddresses.empty());

    // Should still work for valid hostnames after failure
    auto validAddresses = getIPAddresses("localhost");
    EXPECT_FALSE(validAddresses.empty());
}

// Platform-Specific Tests
TEST_F(DNSTest, PlatformSpecificBehavior) {
#ifdef _WIN32
    // Windows-specific tests
    EXPECT_NO_THROW({
        auto addresses = getLocalIPAddresses();
        // Windows should return at least one address
        EXPECT_FALSE(addresses.empty());
    });
#else
    // Unix-like systems tests
    EXPECT_NO_THROW({
        auto addresses = getLocalIPAddresses();
        // Unix systems should return at least one address
        EXPECT_FALSE(addresses.empty());
    });
#endif
}

// Stress Testing
TEST_F(DNSTest, HighVolumeResolution) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    // Test resolving many different hostnames
    std::vector<std::string> hostnames = {
        "google.com", "github.com", "stackoverflow.com", "wikipedia.org",
        "microsoft.com", "apple.com", "amazon.com", "facebook.com"
    };

    for (const auto& hostname : hostnames) {
        auto addresses = getIPAddresses(hostname);
        // Don't require success for all (some might be blocked), but shouldn't crash
        EXPECT_NO_THROW(getIPAddresses(hostname));
    }
}

TEST_F(DNSTest, RapidCacheOperations) {
    // Rapidly set different TTL values
    for (int i = 1; i <= 100; ++i) {
        setDNSCacheTTL(std::chrono::seconds(i));
        clearDNSCacheExpiredEntries();
    }
    SUCCEED();
}

// Edge Case Hostname Tests
TEST_F(DNSTest, EdgeCaseHostnames) {
    std::vector<std::string> edgeCases = {
        "a.com",                    // Very short
        "localhost",                // Standard local
        "127.0.0.1",               // IPv4 address
        "::1",                     // IPv6 address
        "UPPERCASE.COM",           // Uppercase
        "mixed-Case.Com",          // Mixed case
        "with-dashes.com",         // Dashes
        "123numeric.com",          // Starting with numbers
    };

    for (const auto& hostname : edgeCases) {
        EXPECT_NO_THROW({
            auto addresses = getIPAddresses(hostname);
            // Results may vary, but shouldn't crash
        }) << "Failed for edge case: " << hostname;
    }
}

// Timeout and Performance Tests
TEST_F(DNSTest, ResolutionPerformance) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto addresses = getIPAddresses("google.com");
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_FALSE(addresses.empty());
    // DNS resolution should complete within reasonable time (10 seconds)
    EXPECT_LT(duration.count(), 10000);
}

TEST_F(DNSTest, LocalIPPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    auto addresses = getLocalIPAddresses();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_FALSE(addresses.empty());
    // Local IP enumeration should be fast (less than 1 second)
    EXPECT_LT(duration.count(), 1000);
}

#endif  // TEST_DNS_HPP
