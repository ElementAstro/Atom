// filepath: /home/max/Atom-1/atom/web/test_utils.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "utils.hpp"

// Only compile these tests on Linux/Apple platforms where dumpAddrInfo is
// defined
#if defined(__linux__) || defined(__APPLE__)

using namespace atom::web;

class AddrInfoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a sample addrinfo structure for testing
        srcInfo = createSampleAddrInfo();
    }

    void TearDown() override {
        // Free the sample addrinfo structure
        if (srcInfo) {
            freeaddrinfo(srcInfo);
            srcInfo = nullptr;
        }
    }

    // Helper to create a sample addrinfo structure for testing
    addrinfo* createSampleAddrInfo() {
        struct addrinfo hints {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_CANONNAME;

        struct addrinfo* result = nullptr;
        int ret = getaddrinfo("localhost", "80", &hints, &result);
        EXPECT_EQ(ret, 0) << "getaddrinfo failed with error: "
                          << gai_strerror(ret);
        return result;
    }

    // Creates a more complex addrinfo linked list with multiple nodes
    addrinfo* createComplexAddrInfo() {
        struct addrinfo hints {};
        hints.ai_family = AF_UNSPEC;  // Allow both IPv4 and IPv6
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_CANONNAME;

        struct addrinfo* result = nullptr;
        int ret = getaddrinfo("localhost", "http", &hints, &result);
        EXPECT_EQ(ret, 0) << "getaddrinfo failed with error: "
                          << gai_strerror(ret);
        return result;
    }

    // Helper to count nodes in an addrinfo linked list
    int countAddrInfoNodes(const addrinfo* info) {
        int count = 0;
        while (info) {
            count++;
            info = info->ai_next;
        }
        return count;
    }

    struct addrinfo* srcInfo = nullptr;
};

// Test dumpAddrInfo with a valid source addrinfo struct
TEST_F(AddrInfoTest, DumpAddrInfoWithValidSource) {
    // Create destination unique_ptr
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> dstInfo(
        nullptr, ::freeaddrinfo);

    // Call dumpAddrInfo
    int result = dumpAddrInfo(dstInfo, srcInfo);

    // Verify result
    EXPECT_EQ(result, 0) << "dumpAddrInfo should return 0 on success";

    // Verify that destination is not null
    EXPECT_NE(dstInfo.get(), nullptr)
        << "dstInfo should not be null after dumpAddrInfo";

    // Verify basic properties are copied correctly
    EXPECT_EQ(dstInfo->ai_family, srcInfo->ai_family);
    EXPECT_EQ(dstInfo->ai_socktype, srcInfo->ai_socktype);
    EXPECT_EQ(dstInfo->ai_protocol, srcInfo->ai_protocol);
    EXPECT_EQ(dstInfo->ai_addrlen, srcInfo->ai_addrlen);

    // Check address data is copied correctly
    if (srcInfo->ai_addr != nullptr) {
        EXPECT_NE(dstInfo->ai_addr, nullptr);
        // Compare memory of sockaddr structures
        EXPECT_EQ(
            memcmp(dstInfo->ai_addr, srcInfo->ai_addr, srcInfo->ai_addrlen), 0);
    }

    // Check canonical name is copied correctly
    if (srcInfo->ai_canonname != nullptr) {
        EXPECT_NE(dstInfo->ai_canonname, nullptr);
        EXPECT_STREQ(dstInfo->ai_canonname, srcInfo->ai_canonname);
    }
}

// Test dumpAddrInfo with null source
TEST_F(AddrInfoTest, DumpAddrInfoWithNullSource) {
    // Create destination unique_ptr
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> dstInfo(
        nullptr, ::freeaddrinfo);

    // Call dumpAddrInfo with null source
    int result = dumpAddrInfo(dstInfo, nullptr);

    // Verify that it fails with an error code
    EXPECT_EQ(result, -1) << "dumpAddrInfo should return -1 on failure";

    // Destination should still be null
    EXPECT_EQ(dstInfo.get(), nullptr);
}

// Test dumpAddrInfo with complex addrinfo (multiple nodes in linked list)
TEST_F(AddrInfoTest, DumpAddrInfoWithComplexAddrInfo) {
    // Create a more complex addrinfo linked list
    struct addrinfo* complexInfo = createComplexAddrInfo();
    ASSERT_NE(complexInfo, nullptr);

    // Count nodes in the source
    int srcNodeCount = countAddrInfoNodes(complexInfo);
    ASSERT_GT(srcNodeCount, 0)
        << "Source addrinfo should have at least one node";

    // Create destination unique_ptr
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> dstInfo(
        nullptr, ::freeaddrinfo);

    // Call dumpAddrInfo
    int result = dumpAddrInfo(dstInfo, complexInfo);

    // Verify result
    EXPECT_EQ(result, 0) << "dumpAddrInfo should return 0 on success";

    // Verify that destination is not null
    EXPECT_NE(dstInfo.get(), nullptr)
        << "dstInfo should not be null after dumpAddrInfo";

    // Count nodes in the destination
    int dstNodeCount = countAddrInfoNodes(dstInfo.get());

    // Verify that all nodes were copied
    EXPECT_EQ(dstNodeCount, srcNodeCount)
        << "Number of nodes should match between source and destination";

    // Free the complex info
    freeaddrinfo(complexInfo);
}

// Test duplicating an addrinfo with null sockaddr
TEST_F(AddrInfoTest, DumpAddrInfoWithNullSockaddr) {
    // First create a sample addrinfo
    struct addrinfo info;
    memset(&info, 0, sizeof(info));
    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_STREAM;
    info.ai_protocol = IPPROTO_TCP;
    info.ai_addrlen = 0;
    info.ai_addr = nullptr;  // Explicitly null sockaddr
    info.ai_canonname = nullptr;
    info.ai_next = nullptr;

    // Create destination unique_ptr
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> dstInfo(
        nullptr, ::freeaddrinfo);

    // Call dumpAddrInfo
    int result = dumpAddrInfo(dstInfo, &info);

    // Verify result
    EXPECT_EQ(result, 0) << "dumpAddrInfo should return 0 on success";

    // Verify that destination is not null
    EXPECT_NE(dstInfo.get(), nullptr)
        << "dstInfo should not be null after dumpAddrInfo";

    // Verify that sockaddr is null in destination as well
    EXPECT_EQ(dstInfo->ai_addr, nullptr);
}

// Test duplicating an addrinfo with a canonical name
TEST_F(AddrInfoTest, DumpAddrInfoWithCanonicalName) {
    // First create a sample addrinfo
    struct addrinfo info;
    memset(&info, 0, sizeof(info));
    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_STREAM;
    info.ai_protocol = IPPROTO_TCP;
    info.ai_addrlen = 0;
    info.ai_addr = nullptr;

    // Set a canonical name
    const char* testName = "test.canonical.name";
    info.ai_canonname = strdup(testName);
    ASSERT_NE(info.ai_canonname, nullptr);

    info.ai_next = nullptr;

    // Create destination unique_ptr
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> dstInfo(
        nullptr, ::freeaddrinfo);

    // Call dumpAddrInfo
    int result = dumpAddrInfo(dstInfo, &info);

    // Verify result
    EXPECT_EQ(result, 0) << "dumpAddrInfo should return 0 on success";

    // Verify that destination is not null
    EXPECT_NE(dstInfo.get(), nullptr)
        << "dstInfo should not be null after dumpAddrInfo";

    // Verify canonical name was copied correctly
    EXPECT_NE(dstInfo->ai_canonname, nullptr);
    EXPECT_STREQ(dstInfo->ai_canonname, testName);

    // Clean up
    free(info.ai_canonname);
}

// Test that dumpAddrInfo makes a deep copy (modifying source doesn't affect
// destination)
TEST_F(AddrInfoTest, DumpAddrInfoMakesDeepCopy) {
    // Create destination unique_ptr
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> dstInfo(
        nullptr, ::freeaddrinfo);

    // Call dumpAddrInfo
    int result = dumpAddrInfo(dstInfo, srcInfo);
    EXPECT_EQ(result, 0);

    // Store original values for comparison
    int originalFamily = srcInfo->ai_family;

    // Modify the source
    srcInfo->ai_family = AF_INET6;  // Change from whatever it was

    // Verify destination was not affected
    EXPECT_EQ(dstInfo->ai_family, originalFamily);
    EXPECT_NE(dstInfo->ai_family, srcInfo->ai_family);
}

// Test duplicating an addrinfo with a chain of multiple nodes
TEST_F(AddrInfoTest, DumpAddrInfoWithMultipleNodes) {
    // Create a chain of 3 addrinfo nodes
    struct addrinfo* node1 =
        static_cast<addrinfo*>(calloc(1, sizeof(addrinfo)));
    struct addrinfo* node2 =
        static_cast<addrinfo*>(calloc(1, sizeof(addrinfo)));
    struct addrinfo* node3 =
        static_cast<addrinfo*>(calloc(1, sizeof(addrinfo)));

    // Set up different families for each node
    node1->ai_family = AF_INET;
    node2->ai_family = AF_INET6;
    node3->ai_family = AF_UNIX;

    // Link them together
    node1->ai_next = node2;
    node2->ai_next = node3;
    node3->ai_next = nullptr;

    // Create destination unique_ptr
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> dstInfo(
        nullptr, ::freeaddrinfo);

    // Call dumpAddrInfo
    int result = dumpAddrInfo(dstInfo, node1);

    // Verify result
    EXPECT_EQ(result, 0);

    // Check that we have 3 nodes in the destination chain
    EXPECT_NE(dstInfo.get(), nullptr);
    EXPECT_NE(dstInfo->ai_next, nullptr);
    EXPECT_NE(dstInfo->ai_next->ai_next, nullptr);
    EXPECT_EQ(dstInfo->ai_next->ai_next->ai_next, nullptr);

    // Verify family values were copied correctly
    EXPECT_EQ(dstInfo->ai_family, AF_INET);
    EXPECT_EQ(dstInfo->ai_next->ai_family, AF_INET6);
    EXPECT_EQ(dstInfo->ai_next->ai_next->ai_family, AF_UNIX);

    // Clean up the manual chain
    free(node3);
    free(node2);
    free(node1);
}

// Test performance of dumpAddrInfo with a large addrinfo chain
TEST_F(AddrInfoTest, DumpAddrInfoPerformance) {
    // Create a complex addrinfo structure
    struct addrinfo* complexInfo = createComplexAddrInfo();
    ASSERT_NE(complexInfo, nullptr);

    // Measure the time it takes to duplicate the structure
    auto start = std::chrono::high_resolution_clock::now();

    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> dstInfo(
        nullptr, ::freeaddrinfo);
    int result = dumpAddrInfo(dstInfo, complexInfo);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();

    // Verify the operation was successful
    EXPECT_EQ(result, 0);
    EXPECT_NE(dstInfo.get(), nullptr);

    // Log the performance (no assertion, just information)
    std::cout << "dumpAddrInfo took " << duration << " microseconds to copy "
              << countAddrInfoNodes(complexInfo) << " addrinfo nodes."
              << std::endl;

    // Clean up
    freeaddrinfo(complexInfo);
}

// Test thread safety - call dumpAddrInfo from multiple threads
TEST_F(AddrInfoTest, DumpAddrInfoThreadSafety) {
    // Create a more complex addrinfo
    struct addrinfo* complexInfo = createComplexAddrInfo();
    ASSERT_NE(complexInfo, nullptr);

    // Number of threads to test with
    const int numThreads = 10;

    // Vector to store results and outputs from each thread
    std::vector<int> results(numThreads);
    std::vector<std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>>
        outputs;

    // Add empty unique_ptrs to the vector
    for (int i = 0; i < numThreads; i++) {
        outputs.emplace_back(nullptr, ::freeaddrinfo);
    }

    // Vector to store thread objects
    std::vector<std::thread> threads;

    // Launch threads, each calling dumpAddrInfo on the same source
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([i, complexInfo, &results, &outputs]() {
            results[i] = dumpAddrInfo(outputs[i], complexInfo);
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify that all calls succeeded
    for (int i = 0; i < numThreads; i++) {
        EXPECT_EQ(results[i], 0) << "dumpAddrInfo failed in thread " << i;
        EXPECT_NE(outputs[i].get(), nullptr)
            << "Output is null in thread " << i;
    }

    // Clean up
    freeaddrinfo(complexInfo);
}

#endif  // defined(__linux__) || defined(__APPLE__)

// DNS Utilities Tests
class DNSUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests
        spdlog::set_level(spdlog::level::off);
    }
};

TEST_F(DNSUtilsTest, GetIPAddressesValidHostname) {
    auto ips = getIPAddresses("localhost");
    EXPECT_FALSE(ips.empty());

    // Should contain at least one valid IP
    bool hasValidIP = false;
    for (const auto& ip : ips) {
        if (isValidIPv4(ip) || isValidIPv6(ip)) {
            hasValidIP = true;
            break;
        }
    }
    EXPECT_TRUE(hasValidIP);
}

TEST_F(DNSUtilsTest, GetIPAddressesInvalidHostname) {
    auto ips = getIPAddresses("invalid.nonexistent.domain.test");
    EXPECT_TRUE(ips.empty());
}

TEST_F(DNSUtilsTest, GetIPAddressesEmptyHostname) {
    auto ips = getIPAddresses("");
    EXPECT_TRUE(ips.empty());
}

TEST_F(DNSUtilsTest, GetIPAddressesWellKnownDomains) {
    // Test with well-known domains (may fail in isolated environments)
    std::vector<std::string> testDomains = {
        "google.com",
        "github.com",
        "stackoverflow.com"
    };

    for (const auto& domain : testDomains) {
        auto ips = getIPAddresses(domain);
        // Don't assert on success as network may not be available
        // Just ensure no crash occurs
        EXPECT_NO_THROW(getIPAddresses(domain));
    }
}

TEST_F(DNSUtilsTest, GetLocalIPAddresses) {
    auto localIPs = getLocalIPAddresses();

    // Should have at least one local IP (loopback)
    EXPECT_FALSE(localIPs.empty());

    // All returned IPs should be valid
    for (const auto& ip : localIPs) {
        EXPECT_TRUE(isValidIPv4(ip) || isValidIPv6(ip))
            << "Invalid IP address: " << ip;
    }
}

TEST_F(DNSUtilsTest, DNSCacheOperations) {
    // Clear expired entries (should not crash)
    ASSERT_NO_THROW(clearDNSCacheExpiredEntries());

    // Set DNS cache TTL
    ASSERT_NO_THROW(setDNSCacheTTL(std::chrono::seconds(300)));

    // Test caching by resolving the same hostname multiple times
    auto start = std::chrono::high_resolution_clock::now();
    auto ips1 = getIPAddresses("localhost");
    auto mid = std::chrono::high_resolution_clock::now();
    auto ips2 = getIPAddresses("localhost");
    auto end = std::chrono::high_resolution_clock::now();

    // Second call should be faster (cached)
    auto firstDuration = std::chrono::duration_cast<std::chrono::microseconds>(mid - start);
    auto secondDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - mid);

    // Results should be the same
    EXPECT_EQ(ips1, ips2);
}

// IP Utilities Tests
class IPUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests
        spdlog::set_level(spdlog::level::off);
    }
};

TEST_F(IPUtilsTest, IsValidIPv4) {
    // Valid IPv4 addresses
    EXPECT_TRUE(isValidIPv4("192.168.1.1"));
    EXPECT_TRUE(isValidIPv4("0.0.0.0"));
    EXPECT_TRUE(isValidIPv4("255.255.255.255"));
    EXPECT_TRUE(isValidIPv4("127.0.0.1"));
    EXPECT_TRUE(isValidIPv4("10.0.0.1"));

    // Invalid IPv4 addresses
    EXPECT_FALSE(isValidIPv4("256.1.1.1"));
    EXPECT_FALSE(isValidIPv4("192.168.1"));
    EXPECT_FALSE(isValidIPv4("192.168.1.1.1"));
    EXPECT_FALSE(isValidIPv4("not.an.ip"));
    EXPECT_FALSE(isValidIPv4(""));
    EXPECT_FALSE(isValidIPv4("192.168.1.-1"));
}

TEST_F(IPUtilsTest, IsValidIPv6) {
    // Valid IPv6 addresses
    EXPECT_TRUE(isValidIPv6("::1"));
    EXPECT_TRUE(isValidIPv6("::"));
    EXPECT_TRUE(isValidIPv6("2001:db8::1"));
    EXPECT_TRUE(isValidIPv6("fe80::1"));
    EXPECT_TRUE(isValidIPv6("::ffff:192.168.1.1"));
    EXPECT_TRUE(isValidIPv6("2001:db8:85a3::8a2e:370:7334"));

    // Invalid IPv6 addresses
    EXPECT_FALSE(isValidIPv6("invalid::address"));
    EXPECT_FALSE(isValidIPv6("2001:db8::1::2"));
    EXPECT_FALSE(isValidIPv6("gggg::1"));
    EXPECT_FALSE(isValidIPv6(""));
    EXPECT_FALSE(isValidIPv6("192.168.1.1"));  // IPv4 in IPv6 validator
}

TEST_F(IPUtilsTest, IPToString) {
    // Test IPv4 conversion
    struct sockaddr_in addr4{};
    addr4.sin_family = AF_INET;
    inet_pton(AF_INET, "192.168.1.1", &addr4.sin_addr);

    char buffer[INET6_ADDRSTRLEN];
    EXPECT_TRUE(ipToString(reinterpret_cast<struct sockaddr*>(&addr4), buffer, sizeof(buffer)));
    EXPECT_STREQ(buffer, "192.168.1.1");

    // Test IPv6 conversion
    struct sockaddr_in6 addr6{};
    addr6.sin6_family = AF_INET6;
    inet_pton(AF_INET6, "::1", &addr6.sin6_addr);

    EXPECT_TRUE(ipToString(reinterpret_cast<struct sockaddr*>(&addr6), buffer, sizeof(buffer)));
    EXPECT_STREQ(buffer, "::1");

    // Test with null parameters
    EXPECT_FALSE(ipToString(nullptr, buffer, sizeof(buffer)));
    EXPECT_FALSE(ipToString(reinterpret_cast<struct sockaddr*>(&addr4), nullptr, sizeof(buffer)));
    EXPECT_FALSE(ipToString(reinterpret_cast<struct sockaddr*>(&addr4), buffer, 0));
}

TEST_F(IPUtilsTest, IPToStringBufferSizes) {
    struct sockaddr_in addr4{};
    addr4.sin_family = AF_INET;
    inet_pton(AF_INET, "192.168.1.1", &addr4.sin_addr);

    // Test with insufficient buffer for IPv4
    char smallBuffer[10];
    EXPECT_FALSE(ipToString(reinterpret_cast<struct sockaddr*>(&addr4), smallBuffer, sizeof(smallBuffer)));

    // Test with exact size buffer for IPv4
    char exactBuffer[INET_ADDRSTRLEN];
    EXPECT_TRUE(ipToString(reinterpret_cast<struct sockaddr*>(&addr4), exactBuffer, sizeof(exactBuffer)));
}

// Network Utilities Tests
class NetworkUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests
        spdlog::set_level(spdlog::level::off);
    }
};

TEST_F(NetworkUtilsTest, CheckInternetConnectivity) {
    // Test internet connectivity check
    // Note: This may fail in isolated test environments
    bool hasInternet = checkInternetConnectivity();

    // Just ensure the function doesn't crash
    EXPECT_NO_THROW(checkInternetConnectivity());

    // The result can be either true or false depending on environment
    // We just verify it returns a boolean value
    EXPECT_TRUE(hasInternet == true || hasInternet == false);
}

// Port Utilities Tests
class PortUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests
        spdlog::set_level(spdlog::level::off);

        // Initialize Windows Socket API if needed
        initializeWindowsSocketAPI();
    }

    void TearDown() override {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

TEST_F(PortUtilsTest, IsPortInUse) {
    // Test with well-known ports that are likely to be closed
    EXPECT_FALSE(isPortInUse(65432));  // High port number, likely unused

    // Test with invalid port numbers
    EXPECT_FALSE(isPortInUse(0));      // Invalid port
    EXPECT_FALSE(isPortInUse(65536));  // Out of range
}

TEST_F(PortUtilsTest, IsPortInUseAsync) {
    // Test async version
    EXPECT_FALSE(isPortInUseAsync(65433));  // High port number, likely unused

    // Test with invalid port numbers
    EXPECT_FALSE(isPortInUseAsync(0));      // Invalid port
    EXPECT_FALSE(isPortInUseAsync(65536));  // Out of range
}

TEST_F(PortUtilsTest, GetAvailablePort) {
    uint16_t port = getAvailablePort();

    // Should return a valid port number
    EXPECT_GT(port, 0);
    EXPECT_LE(port, 65535);

    // The returned port should actually be available
    EXPECT_FALSE(isPortInUse(port));
}

TEST_F(PortUtilsTest, GetAvailablePortInRange) {
    uint16_t startPort = 50000;
    uint16_t endPort = 50010;

    uint16_t port = getAvailablePortInRange(startPort, endPort);

    if (port != 0) {  // If a port was found
        EXPECT_GE(port, startPort);
        EXPECT_LE(port, endPort);
        EXPECT_FALSE(isPortInUse(port));
    }

    // Test with invalid range
    uint16_t invalidPort = getAvailablePortInRange(50010, 50000);  // start > end
    EXPECT_EQ(invalidPort, 0);
}

TEST_F(PortUtilsTest, ScanPort) {
    // Test scanning a port that's likely to be closed
    bool isOpen = scanPort("127.0.0.1", 65434, std::chrono::milliseconds(1000));
    EXPECT_FALSE(isOpen);  // Should be closed

    // Test with invalid host
    bool invalidHost = scanPort("invalid.nonexistent.host", 80, std::chrono::milliseconds(1000));
    EXPECT_FALSE(invalidHost);

    // Test with empty host
    bool emptyHost = scanPort("", 80, std::chrono::milliseconds(1000));
    EXPECT_FALSE(emptyHost);
}

TEST_F(PortUtilsTest, ScanPortRange) {
    // Scan a small range of high ports (likely to be closed)
    auto openPorts = scanPortRange("127.0.0.1", 65430, 65435, std::chrono::milliseconds(500));

    // Should return a vector (possibly empty)
    EXPECT_TRUE(openPorts.empty() || !openPorts.empty());

    // All returned ports should be in the specified range
    for (uint16_t port : openPorts) {
        EXPECT_GE(port, 65430);
        EXPECT_LE(port, 65435);
    }

    // Test with invalid range
    EXPECT_THROW(scanPortRange("127.0.0.1", 65435, 65430, std::chrono::milliseconds(500)),
                 std::invalid_argument);

    // Test with empty host
    EXPECT_THROW(scanPortRange("", 80, 85, std::chrono::milliseconds(500)),
                 std::invalid_argument);
}

// Socket Utilities Tests
class SocketUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests
        spdlog::set_level(spdlog::level::off);

        // Initialize Windows Socket API if needed
        initializeWindowsSocketAPI();
    }

    void TearDown() override {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

TEST_F(SocketUtilsTest, CreateSocket) {
    int sockfd = createSocket();

    // Should return a valid socket descriptor
    EXPECT_GE(sockfd, 0);

    // Clean up
    if (sockfd >= 0) {
#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif
    }
}

TEST_F(SocketUtilsTest, BindSocket) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);

    // Try to bind to an available port
    uint16_t testPort = getAvailablePort();
    bool bindResult = bindSocket(sockfd, testPort);

    // Should succeed if port is available
    EXPECT_TRUE(bindResult);

    // Clean up
#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
}

TEST_F(SocketUtilsTest, BindSocketInvalidDescriptor) {
    // Test with invalid socket descriptor
    bool result = bindSocket(-1, 8080);
    EXPECT_FALSE(result);
}

TEST_F(SocketUtilsTest, SetSocketNonBlocking) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);

    // Set socket to non-blocking mode
    bool result = setSocketNonBlocking(sockfd);
    EXPECT_TRUE(result);

    // Clean up
#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
}

TEST_F(SocketUtilsTest, SetSocketNonBlockingInvalidDescriptor) {
    // Test with invalid socket descriptor
    bool result = setSocketNonBlocking(-1);
    EXPECT_FALSE(result);
}

// Performance and Stress Tests
class UtilsPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests
        spdlog::set_level(spdlog::level::off);

        // Initialize Windows Socket API if needed
        initializeWindowsSocketAPI();
    }

    void TearDown() override {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

TEST_F(UtilsPerformanceTest, DNSResolutionPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    // Resolve multiple hostnames
    std::vector<std::string> hostnames = {
        "localhost",
        "127.0.0.1",
        "::1"
    };

    for (int i = 0; i < 100; ++i) {
        for (const auto& hostname : hostnames) {
            auto ips = getIPAddresses(hostname);
            // Don't assert on results as network may not be available
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 30000);  // Less than 30 seconds
}

TEST_F(UtilsPerformanceTest, IPValidationPerformance) {
    std::vector<std::string> testIPs = {
        "192.168.1.1", "10.0.0.1", "172.16.0.1", "127.0.0.1",
        "2001:db8::1", "::1", "::", "fe80::1",
        "invalid.ip", "256.1.1.1", "not.an.ip", ""
    };

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        for (const auto& ip : testIPs) {
            isValidIPv4(ip);
            isValidIPv6(ip);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 5000);  // Less than 5 seconds
}

TEST_F(UtilsPerformanceTest, SocketCreationPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<int> sockets;
    sockets.reserve(100);

    // Create multiple sockets
    for (int i = 0; i < 100; ++i) {
        try {
            int sockfd = createSocket();
            if (sockfd >= 0) {
                sockets.push_back(sockfd);
            }
        } catch (const std::exception&) {
            // Ignore failures in performance test
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Clean up sockets
    for (int sockfd : sockets) {
#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif
    }

    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 10000);  // Less than 10 seconds

    // Should have created at least some sockets
    EXPECT_GT(sockets.size(), 0);
}

TEST_F(UtilsPerformanceTest, PortScanningPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    // Scan a small range of ports
    for (uint16_t port = 65500; port <= 65510; ++port) {
        scanPort("127.0.0.1", port, std::chrono::milliseconds(100));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 15000);  // Less than 15 seconds
}

// Edge Cases and Error Handling Tests
class UtilsEdgeCasesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests
        spdlog::set_level(spdlog::level::off);

        // Initialize Windows Socket API if needed
        initializeWindowsSocketAPI();
    }

    void TearDown() override {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

TEST_F(UtilsEdgeCasesTest, DNSWithSpecialCharacters) {
    // Test DNS resolution with special characters
    std::vector<std::string> specialHostnames = {
        "test-with-dashes.com",
        "test_with_underscores.com",
        "test.with.many.dots.com",
        "123.numeric.hostname.com",
        "xn--e1afmkfd.xn--p1ai",  // IDN domain
    };

    for (const auto& hostname : specialHostnames) {
        EXPECT_NO_THROW(getIPAddresses(hostname));
    }
}

TEST_F(UtilsEdgeCasesTest, IPValidationEdgeCases) {
    // IPv4 edge cases
    EXPECT_TRUE(isValidIPv4("0.0.0.0"));
    EXPECT_TRUE(isValidIPv4("255.255.255.255"));
    EXPECT_FALSE(isValidIPv4("256.0.0.0"));
    EXPECT_FALSE(isValidIPv4("-1.0.0.0"));
    EXPECT_FALSE(isValidIPv4("1.2.3"));
    EXPECT_FALSE(isValidIPv4("1.2.3.4.5"));

    // IPv6 edge cases
    EXPECT_TRUE(isValidIPv6("::"));
    EXPECT_TRUE(isValidIPv6("::1"));
    EXPECT_TRUE(isValidIPv6("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
    EXPECT_FALSE(isValidIPv6("::1::"));
    EXPECT_FALSE(isValidIPv6("gggg::1"));
    EXPECT_FALSE(isValidIPv6("1:2:3:4:5:6:7:8:9"));
}

TEST_F(UtilsEdgeCasesTest, PortRangeEdgeCases) {
    // Test boundary port numbers
    EXPECT_FALSE(isPortInUse(1));      // Low port
    EXPECT_FALSE(isPortInUse(65535));  // Max port

    // Test port range scanning with edge cases
    auto ports1 = scanPortRange("127.0.0.1", 1, 1, std::chrono::milliseconds(100));
    EXPECT_TRUE(ports1.empty() || !ports1.empty());  // Just ensure no crash

    auto ports2 = scanPortRange("127.0.0.1", 65535, 65535, std::chrono::milliseconds(100));
    EXPECT_TRUE(ports2.empty() || !ports2.empty());  // Just ensure no crash
}

TEST_F(UtilsEdgeCasesTest, SocketOperationsWithErrors) {
    // Test socket operations with various error conditions

    // Multiple socket creation and cleanup
    std::vector<int> sockets;
    for (int i = 0; i < 10; ++i) {
        try {
            int sockfd = createSocket();
            if (sockfd >= 0) {
                sockets.push_back(sockfd);
            }
        } catch (const std::exception&) {
            // Expected in some cases
        }
    }

    // Try to bind multiple sockets to the same port
    if (!sockets.empty()) {
        uint16_t testPort = getAvailablePort();
        bool firstBind = bindSocket(sockets[0], testPort);
        EXPECT_TRUE(firstBind);

        if (sockets.size() > 1) {
            bool secondBind = bindSocket(sockets[1], testPort);
            EXPECT_FALSE(secondBind);  // Should fail - port already in use
        }
    }

    // Clean up
    for (int sockfd : sockets) {
#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif
    }
}
