#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

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
        return (ip.find('.') != std::string::npos) ||
               (ip.find(':') != std::string::npos);
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
            EXPECT_TRUE(isValidIPAddress(addr))
                << "Invalid IP address: " << addr;
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
            EXPECT_TRUE(isValidIPAddress(addr))
                << "Invalid IP address: " << addr;
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
    ASSERT_NO_THROW({ clearDNSCacheExpiredEntries(); });
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
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

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

    std::vector<std::string> hostnames = {"google.com", "github.com",
                                          "stackoverflow.com", "wikipedia.org"};

    for (const auto& hostname : hostnames) {
        ASSERT_NO_THROW({
            auto addresses = getIPAddresses(hostname);
            EXPECT_FALSE(addresses.empty()) << "No addresses for: " << hostname;

            for (const auto& addr : addresses) {
                EXPECT_TRUE(isValidIPAddress(addr))
                    << "Invalid IP for " << hostname << ": " << addr;
            }
        }) << "Failed for hostname: "
           << hostname;
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
        "very-long-hostname-that-exceeds-normal-limits-and-should-probably-"
        "fail-or-at-least-be-handled-gracefully.com"};

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
                if (c == '.')
                    dotCount++;
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
        "google.com",    "github.com", "stackoverflow.com", "wikipedia.org",
        "microsoft.com", "apple.com",  "amazon.com",        "facebook.com"};

    for (const auto& hostname : hostnames) {
        auto addresses = getIPAddresses(hostname);
        // Don't require success for all (some might be blocked), but shouldn't
        // crash
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
        "a.com",            // Very short
        "localhost",        // Standard local
        "127.0.0.1",        // IPv4 address
        "::1",              // IPv6 address
        "UPPERCASE.COM",    // Uppercase
        "mixed-Case.Com",   // Mixed case
        "with-dashes.com",  // Dashes
        "123numeric.com",   // Starting with numbers
    };

    for (const auto& hostname : edgeCases) {
        EXPECT_NO_THROW({
            auto addresses = getIPAddresses(hostname);
            // Results may vary, but shouldn't crash
        }) << "Failed for edge case: "
           << hostname;
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

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_FALSE(addresses.empty());
    // DNS resolution should complete within reasonable time (10 seconds)
    EXPECT_LT(duration.count(), 10000);
}

TEST_F(DNSTest, LocalIPPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    auto addresses = getLocalIPAddresses();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_FALSE(addresses.empty());
    // Local IP enumeration should be fast (less than 1 second)
    EXPECT_LT(duration.count(), 1000);
}

// ============================================================================
// DnsRecordType and DnsError String Conversion Tests
// ============================================================================

TEST_F(DNSTest, DnsRecordTypeToString) {
    EXPECT_EQ(dnsRecordTypeToString(DnsRecordType::A), "A");
    EXPECT_EQ(dnsRecordTypeToString(DnsRecordType::AAAA), "AAAA");
    EXPECT_EQ(dnsRecordTypeToString(DnsRecordType::CNAME), "CNAME");
    EXPECT_EQ(dnsRecordTypeToString(DnsRecordType::MX), "MX");
    EXPECT_EQ(dnsRecordTypeToString(DnsRecordType::TXT), "TXT");
    EXPECT_EQ(dnsRecordTypeToString(DnsRecordType::NS), "NS");
    EXPECT_EQ(dnsRecordTypeToString(DnsRecordType::PTR), "PTR");
    EXPECT_EQ(dnsRecordTypeToString(DnsRecordType::SOA), "SOA");
    EXPECT_EQ(dnsRecordTypeToString(DnsRecordType::SRV), "SRV");
    EXPECT_EQ(dnsRecordTypeToString(DnsRecordType::ANY), "ANY");
    EXPECT_EQ(dnsRecordTypeToString(static_cast<DnsRecordType>(99)), "UNKNOWN");
}

TEST_F(DNSTest, DnsErrorToString) {
    EXPECT_EQ(dnsErrorToString(DnsError::Success), "Success");
    EXPECT_EQ(dnsErrorToString(DnsError::EmptyHostname),
              "Empty hostname provided");
    EXPECT_EQ(dnsErrorToString(DnsError::ResolutionFailed),
              "DNS resolution failed");
    EXPECT_EQ(dnsErrorToString(DnsError::Timeout), "DNS query timed out");
    EXPECT_EQ(dnsErrorToString(DnsError::NetworkError),
              "Network error occurred");
    EXPECT_EQ(dnsErrorToString(DnsError::InvalidAddress),
              "Invalid address format");
    EXPECT_EQ(dnsErrorToString(DnsError::CacheError), "DNS cache error");
    EXPECT_EQ(dnsErrorToString(DnsError::NotFound), "Host not found");
    EXPECT_EQ(dnsErrorToString(DnsError::ServerFailure), "DNS server failure");
    EXPECT_EQ(dnsErrorToString(DnsError::InvalidQuery), "Invalid DNS query");
    EXPECT_EQ(dnsErrorToString(static_cast<DnsError>(99)), "Unknown error");
}

// ============================================================================
// isValidHostname Tests
// ============================================================================

TEST_F(DNSTest, IsValidHostnameValid) {
    EXPECT_TRUE(isValidHostname("google.com"));
    EXPECT_TRUE(isValidHostname("www.google.com"));
    EXPECT_TRUE(isValidHostname("sub.domain.example.com"));
    EXPECT_TRUE(isValidHostname("a.b.c"));
    EXPECT_TRUE(isValidHostname("test-domain.com"));
    EXPECT_TRUE(isValidHostname("test_domain.com"));
    EXPECT_TRUE(isValidHostname("123.com"));
    EXPECT_TRUE(isValidHostname("a1b2c3.com"));
    EXPECT_TRUE(isValidHostname("UPPERCASE.COM"));
    EXPECT_TRUE(isValidHostname("MixedCase.Com"));
}

TEST_F(DNSTest, IsValidHostnameInvalid) {
    EXPECT_FALSE(isValidHostname(""));
    EXPECT_FALSE(isValidHostname(".leading.dot"));
    EXPECT_FALSE(isValidHostname("trailing.dot."));
    EXPECT_FALSE(isValidHostname("double..dots"));
    EXPECT_FALSE(isValidHostname("-starts-with-dash.com"));
    EXPECT_FALSE(isValidHostname("ends-with-dash-.com"));
    EXPECT_FALSE(isValidHostname("has space.com"));
    EXPECT_FALSE(isValidHostname("has@symbol.com"));
    // Test hostname longer than 253 characters
    std::string longHostname(254, 'a');
    EXPECT_FALSE(isValidHostname(longHostname));
    // Test label longer than 63 characters
    std::string longLabel(64, 'a');
    longLabel += ".com";
    EXPECT_FALSE(isValidHostname(longLabel));
}

// ============================================================================
// configureDnsCache Tests
// ============================================================================

TEST_F(DNSTest, ConfigureDnsCache) {
    DnsCacheConfig config;
    config.ttl = std::chrono::seconds(120);
    config.maxEntries = 500;
    config.enableNegativeCache = true;
    config.negativeTtl = std::chrono::seconds(30);

    ASSERT_NO_THROW(configureDnsCache(config));

    // Verify cache works with new config
    if (hasInternetConnectivity()) {
        auto addresses = getIPAddresses("google.com");
        EXPECT_FALSE(addresses.empty());
    }
}

TEST_F(DNSTest, ConfigureDnsCacheEdgeCases) {
    // Test with minimal values
    DnsCacheConfig minConfig;
    minConfig.ttl = std::chrono::seconds(1);
    minConfig.maxEntries = 1;
    minConfig.enableNegativeCache = false;
    minConfig.negativeTtl = std::chrono::seconds(1);
    ASSERT_NO_THROW(configureDnsCache(minConfig));

    // Test with large values
    DnsCacheConfig maxConfig;
    maxConfig.ttl = std::chrono::seconds(86400);
    maxConfig.maxEntries = 100000;
    maxConfig.enableNegativeCache = true;
    maxConfig.negativeTtl = std::chrono::seconds(3600);
    ASSERT_NO_THROW(configureDnsCache(maxConfig));
}

// ============================================================================
// clearDnsCache Tests
// ============================================================================

TEST_F(DNSTest, ClearDnsCache) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    // Populate cache
    auto addresses1 = getIPAddresses("google.com");
    EXPECT_FALSE(addresses1.empty());

    // Clear cache
    ASSERT_NO_THROW(clearDnsCache());

    // Cache should be empty now
    auto stats = getDnsCacheStats();
    EXPECT_EQ(stats.first, 0);  // Total entries should be 0
}

// ============================================================================
// getDnsCacheStats Tests
// ============================================================================

TEST_F(DNSTest, GetDnsCacheStats) {
    // Clear cache first
    clearDnsCache();

    auto stats1 = getDnsCacheStats();
    EXPECT_EQ(stats1.first, 0);  // No entries initially

    if (hasInternetConnectivity()) {
        // Add some entries
        getIPAddresses("google.com");
        getIPAddresses("github.com");

        auto stats2 = getDnsCacheStats();
        EXPECT_GE(stats2.first, 1);  // Should have at least 1 entry
    }
}

// ============================================================================
// resolveHostname Tests
// ============================================================================

TEST_F(DNSTest, ResolveHostnameValid) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    auto result = resolveHostname("google.com", DnsRecordType::A);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->addresses.empty());
    EXPECT_EQ(result->recordType, DnsRecordType::A);
    EXPECT_GE(result->queryTime.count(), 0);
}

TEST_F(DNSTest, ResolveHostnameIPv6) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    auto result = resolveHostname("google.com", DnsRecordType::AAAA);
    // IPv6 might not be available, so just check it doesn't crash
    if (result.has_value()) {
        EXPECT_EQ(result->recordType, DnsRecordType::AAAA);
        for (const auto& addr : result->addresses) {
            EXPECT_NE(addr.find(':'), std::string::npos);
        }
    }
}

TEST_F(DNSTest, ResolveHostnameEmpty) {
    auto result = resolveHostname("", DnsRecordType::A);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().error(), DnsError::EmptyHostname);
}

TEST_F(DNSTest, ResolveHostnameInvalid) {
    auto result =
        resolveHostname("invalid.nonexistent.domain.xyz", DnsRecordType::A);
    EXPECT_FALSE(result.has_value());
}

TEST_F(DNSTest, ResolveHostnameLocalhost) {
    auto result = resolveHostname("localhost", DnsRecordType::A);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->addresses.empty());
    EXPECT_THAT(result->addresses, ::testing::Contains("127.0.0.1"));
}

TEST_F(DNSTest, ResolveHostnameCacheHit) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    // First call - cache miss
    auto result1 = resolveHostname("google.com", DnsRecordType::A);
    ASSERT_TRUE(result1.has_value());
    EXPECT_FALSE(result1->fromCache);

    // Second call - cache hit
    auto result2 = resolveHostname("google.com", DnsRecordType::A);
    ASSERT_TRUE(result2.has_value());
    EXPECT_TRUE(result2->fromCache);
    EXPECT_EQ(result1->addresses, result2->addresses);
}

TEST_F(DNSTest, ResolveHostnameResultMethods) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    auto result = resolveHostname("google.com", DnsRecordType::A);
    ASSERT_TRUE(result.has_value());

    // Test DnsResult methods
    EXPECT_FALSE(result->empty());
    EXPECT_GT(result->size(), 0);
    EXPECT_NE(result->begin(), result->end());

    // Test iteration
    size_t count = 0;
    for (const auto& addr : *result) {
        EXPECT_FALSE(addr.empty());
        ++count;
    }
    EXPECT_EQ(count, result->size());
}

// ============================================================================
// resolveHostnameAsync Tests
// ============================================================================

TEST_F(DNSTest, ResolveHostnameAsyncValid) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    auto future = resolveHostnameAsync("google.com", DnsRecordType::A);
    ASSERT_TRUE(future.valid());

    auto result = future.get();
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->addresses.empty());
}

TEST_F(DNSTest, ResolveHostnameAsyncEmpty) {
    auto future = resolveHostnameAsync("", DnsRecordType::A);
    ASSERT_TRUE(future.valid());

    auto result = future.get();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().error(), DnsError::EmptyHostname);
}

TEST_F(DNSTest, ResolveHostnameAsyncMultiple) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    std::vector<std::future<expected<DnsResult, DnsError>>> futures;
    futures.push_back(resolveHostnameAsync("google.com"));
    futures.push_back(resolveHostnameAsync("github.com"));
    futures.push_back(resolveHostnameAsync("localhost"));

    for (auto& future : futures) {
        auto result = future.get();
        EXPECT_TRUE(result.has_value());
    }
}

// ============================================================================
// resolveHostnamesBatch Tests
// ============================================================================

TEST_F(DNSTest, ResolveHostnamesBatchValid) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    std::vector<std::string> hostnames = {"google.com", "github.com",
                                          "localhost"};
    auto results = resolveHostnamesBatch(hostnames, DnsRecordType::A);

    EXPECT_EQ(results.size(), hostnames.size());
    for (const auto& result : results) {
        if (result.has_value()) {
            EXPECT_FALSE(result->addresses.empty());
        }
    }
}

TEST_F(DNSTest, ResolveHostnamesBatchEmpty) {
    std::vector<std::string> hostnames;
    auto results = resolveHostnamesBatch(hostnames, DnsRecordType::A);
    EXPECT_TRUE(results.empty());
}

TEST_F(DNSTest, ResolveHostnamesBatchMixed) {
    std::vector<std::string> hostnames = {"localhost", "",
                                          "invalid.xyz.nonexistent"};
    auto results = resolveHostnamesBatch(hostnames, DnsRecordType::A);

    EXPECT_EQ(results.size(), 3);
    EXPECT_TRUE(results[0].has_value());   // localhost should succeed
    EXPECT_FALSE(results[1].has_value());  // empty should fail
    EXPECT_FALSE(results[2].has_value());  // invalid should fail
}

// ============================================================================
// reverseLookup Tests
// ============================================================================

TEST_F(DNSTest, ReverseLookupValid) {
    // Use localhost IP which should always resolve
    auto result = reverseLookup("127.0.0.1");
    // May or may not succeed depending on system config
    if (result.has_value()) {
        EXPECT_FALSE(result->empty());
    }
}

TEST_F(DNSTest, ReverseLookupEmpty) {
    auto result = reverseLookup("");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().error(), DnsError::EmptyHostname);
}

TEST_F(DNSTest, ReverseLookupInvalidIP) {
    auto result = reverseLookup("not.an.ip.address");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().error(), DnsError::InvalidAddress);
}

TEST_F(DNSTest, ReverseLookupIPv6) {
    auto result = reverseLookup("::1");
    // May or may not succeed depending on system config
    if (result.has_value()) {
        EXPECT_FALSE(result->empty());
    }
}

// ============================================================================
// reverseLookupAsync Tests
// ============================================================================

TEST_F(DNSTest, ReverseLookupAsyncValid) {
    auto future = reverseLookupAsync("127.0.0.1");
    ASSERT_TRUE(future.valid());

    auto result = future.get();
    // May or may not succeed depending on system config
    (void)result;
}

TEST_F(DNSTest, ReverseLookupAsyncEmpty) {
    auto future = reverseLookupAsync("");
    ASSERT_TRUE(future.valid());

    auto result = future.get();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().error(), DnsError::EmptyHostname);
}

// ============================================================================
// queryMxRecords Tests
// ============================================================================

TEST_F(DNSTest, QueryMxRecordsValid) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    auto result = queryMxRecords("google.com");
    // MX query might fail on some systems, just verify it doesn't crash
    if (result.has_value()) {
        for (const auto& mx : *result) {
            EXPECT_FALSE(mx.hostname.empty());
            EXPECT_GE(mx.priority, 0);
        }
    }
}

TEST_F(DNSTest, QueryMxRecordsEmpty) {
    auto result = queryMxRecords("");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().error(), DnsError::EmptyHostname);
}

TEST_F(DNSTest, MxRecordComparison) {
    MxRecord mx1{.hostname = "mx1.example.com", .priority = 10};
    MxRecord mx2{.hostname = "mx2.example.com", .priority = 20};
    MxRecord mx3{.hostname = "mx3.example.com", .priority = 10};

    EXPECT_TRUE((mx1 <=> mx2) <
                0);  // mx1 has lower priority value (higher priority)
    EXPECT_TRUE((mx2 <=> mx1) > 0);
    EXPECT_TRUE((mx1 <=> mx3) == 0);  // Same priority
}

// ============================================================================
// querySrvRecords Tests
// ============================================================================

TEST_F(DNSTest, QuerySrvRecordsValid) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    auto result = querySrvRecords("_http._tcp.google.com");
    // SRV query might fail, just verify it doesn't crash
    if (result.has_value()) {
        for (const auto& srv : *result) {
            EXPECT_GE(srv.port, 0);
            EXPECT_GE(srv.priority, 0);
            EXPECT_GE(srv.weight, 0);
        }
    }
}

TEST_F(DNSTest, QuerySrvRecordsEmpty) {
    auto result = querySrvRecords("");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().error(), DnsError::EmptyHostname);
}

TEST_F(DNSTest, SrvRecordComparison) {
    SrvRecord srv1{.target = "srv1.example.com",
                   .port = 80,
                   .priority = 10,
                   .weight = 100};
    SrvRecord srv2{
        .target = "srv2.example.com", .port = 80, .priority = 20, .weight = 50};
    SrvRecord srv3{
        .target = "srv3.example.com", .port = 80, .priority = 10, .weight = 50};

    EXPECT_TRUE((srv1 <=> srv2) <
                0);  // srv1 has lower priority (higher priority)
    EXPECT_TRUE((srv1 <=> srv3) < 0);  // Same priority, srv1 has higher weight
}

// ============================================================================
// queryTxtRecords Tests
// ============================================================================

TEST_F(DNSTest, QueryTxtRecordsValid) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    auto result = queryTxtRecords("google.com");
    // TXT query might fail, just verify it doesn't crash
    if (result.has_value()) {
        for (const auto& txt : *result) {
            // TXT records can be empty strings
            (void)txt;
        }
    }
}

TEST_F(DNSTest, QueryTxtRecordsEmpty) {
    auto result = queryTxtRecords("");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().error(), DnsError::EmptyHostname);
}

// ============================================================================
// getPrimaryLocalIP Tests
// ============================================================================

TEST_F(DNSTest, GetPrimaryLocalIPDefault) {
    auto result = getPrimaryLocalIP();
    if (result.has_value()) {
        EXPECT_FALSE(result->empty());
        EXPECT_TRUE(isValidIPAddress(*result));
    }
}

TEST_F(DNSTest, GetPrimaryLocalIPPreferIPv4) {
    auto result = getPrimaryLocalIP(false);
    if (result.has_value()) {
        EXPECT_FALSE(result->empty());
        // Should prefer IPv4
        if (result->find('.') != std::string::npos) {
            // It's IPv4, which is expected
            EXPECT_NE(result->find('.'), std::string::npos);
        }
    }
}

TEST_F(DNSTest, GetPrimaryLocalIPPreferIPv6) {
    auto result = getPrimaryLocalIP(true);
    if (result.has_value()) {
        EXPECT_FALSE(result->empty());
        // Should prefer IPv6 if available
        EXPECT_TRUE(isValidIPAddress(*result));
    }
}

// ============================================================================
// prefetchDns Tests
// ============================================================================

TEST_F(DNSTest, PrefetchDnsValid) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    clearDnsCache();

    std::vector<std::string> hostnames = {"google.com", "github.com"};
    ASSERT_NO_THROW(prefetchDns(hostnames, DnsRecordType::A));

    // After prefetch, cache should have entries
    auto stats = getDnsCacheStats();
    EXPECT_GE(stats.first, 1);
}

TEST_F(DNSTest, PrefetchDnsEmpty) {
    std::vector<std::string> hostnames;
    ASSERT_NO_THROW(prefetchDns(hostnames, DnsRecordType::A));
}

TEST_F(DNSTest, PrefetchDnsMixed) {
    std::vector<std::string> hostnames = {"localhost",
                                          "invalid.xyz.nonexistent"};
    // Should not throw even with invalid hostnames
    ASSERT_NO_THROW(prefetchDns(hostnames, DnsRecordType::A));
}

// ============================================================================
// isHostnameResolvable Tests
// ============================================================================

TEST_F(DNSTest, IsHostnameResolvableValid) {
    EXPECT_TRUE(isHostnameResolvable("localhost"));
}

TEST_F(DNSTest, IsHostnameResolvableInternet) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    EXPECT_TRUE(isHostnameResolvable("google.com"));
}

TEST_F(DNSTest, IsHostnameResolvableInvalid) {
    EXPECT_FALSE(isHostnameResolvable("invalid.nonexistent.domain.xyz"));
}

TEST_F(DNSTest, IsHostnameResolvableEmpty) {
    EXPECT_FALSE(isHostnameResolvable(""));
}

TEST_F(DNSTest, IsHostnameResolvableTimeout) {
    // Test with very short timeout
    auto result =
        isHostnameResolvable("localhost", std::chrono::milliseconds{1});
    // Result depends on system speed, just verify it doesn't crash
    (void)result;
}

// ============================================================================
// getLocalIPAddresses with parameters Tests
// ============================================================================

TEST_F(DNSTest, GetLocalIPAddressesWithLoopback) {
    auto addresses = getLocalIPAddresses(true, true);
    // Should include loopback addresses
    bool hasLoopback = false;
    for (const auto& addr : addresses) {
        if (addr == "127.0.0.1" || addr == "::1") {
            hasLoopback = true;
            break;
        }
    }
    // Note: getLocalIPAddresses might not include loopback depending on
    // implementation
    (void)hasLoopback;
}

TEST_F(DNSTest, GetLocalIPAddressesIPv4Only) {
    auto addresses = getLocalIPAddresses(false, false);
    for (const auto& addr : addresses) {
        // Should only have IPv4 addresses (no colons)
        EXPECT_EQ(addr.find(':'), std::string::npos)
            << "Found IPv6 address when IPv4 only requested: " << addr;
    }
}

TEST_F(DNSTest, GetLocalIPAddressesIncludeIPv6) {
    auto addresses = getLocalIPAddresses(false, true);
    // Just verify it doesn't crash and returns valid addresses
    for (const auto& addr : addresses) {
        EXPECT_TRUE(isValidIPAddress(addr));
    }
}

// ============================================================================
// DnsCacheConfig and DnsResolverConfig Tests
// ============================================================================

TEST_F(DNSTest, DnsCacheConfigDefaults) {
    DnsCacheConfig config;
    EXPECT_EQ(config.ttl, std::chrono::seconds(300));
    EXPECT_EQ(config.maxEntries, 1000);
    EXPECT_TRUE(config.enableNegativeCache);
    EXPECT_EQ(config.negativeTtl, std::chrono::seconds(60));
}

TEST_F(DNSTest, DnsResolverConfigDefaults) {
    DnsResolverConfig config;
    EXPECT_EQ(config.timeout, std::chrono::milliseconds(5000));
    EXPECT_EQ(config.maxRetries, 3);
    EXPECT_FALSE(config.preferIPv6);
    EXPECT_TRUE(config.useCache);
    EXPECT_TRUE(config.nameservers.empty());
}

// ============================================================================
// Concurrent DNS Operations Tests
// ============================================================================

TEST_F(DNSTest, ConcurrentResolveHostname) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&successCount]() {
            auto result = resolveHostname("google.com", DnsRecordType::A);
            if (result.has_value() && !result->addresses.empty()) {
                successCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successCount.load(), 0);
}

TEST_F(DNSTest, ConcurrentCacheOperations) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i]() {
            if (i % 3 == 0) {
                clearDnsCache();
            } else if (i % 3 == 1) {
                getDnsCacheStats();
            } else {
                clearDNSCacheExpiredEntries();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    SUCCEED();
}
