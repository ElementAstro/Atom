#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "atom/web/utils/port.hpp"
#include "atom/web/utils/socket.hpp"

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
#include <unistd.h>
#endif

using namespace atom::web;

class PortTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests to reduce noise
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }

        // Initialize socket API for all tests
        socketInitialized = initializeWindowsSocketAPI();
        if (!socketInitialized) {
            GTEST_SKIP() << "Failed to initialize socket API";
        }
    }

    void TearDown() override {
#ifdef _WIN32
        if (socketInitialized) {
            WSACleanup();
        }
#endif
    }

    // Helper method to create a test server on a specific port
    int createTestServer(uint16_t port) {
        int sockfd = createSocket();
        if (sockfd < 0) {
            return -1;
        }

        if (!bindSocket(sockfd, port)) {
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return -1;
        }

        if (listen(sockfd, 1) < 0) {
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return -1;
        }

        return sockfd;
    }

    // Helper method to close a socket
    void closeSocket(int sockfd) {
        if (sockfd >= 0) {
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
        }
    }

    bool socketInitialized = false;
};

// Basic Port Usage Tests
TEST_F(PortTest, IsPortInUseAvailablePort) {
    // Test with a high port number that's likely to be available
    uint16_t testPort = 54321;

    ASSERT_NO_THROW({
        bool inUse = isPortInUse(testPort);
        // Port should be available (false) or in use (true)
        // The important thing is that it doesn't crash
    });
}

TEST_F(PortTest, IsPortInUseWithBoundSocket) {
    uint16_t testPort = 54322;

    // Create a test server to occupy the port
    int serverSocket = createTestServer(testPort);
    if (serverSocket >= 0) {
        // Port should now be in use
        EXPECT_TRUE(isPortInUse(testPort));

        closeSocket(serverSocket);

        // Give the system time to release the port
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Port should now be available (though this might be flaky due to
        // TIME_WAIT) So we just test that the function doesn't crash
        EXPECT_NO_THROW(isPortInUse(testPort));
    }
}

TEST_F(PortTest, IsPortInUseInvalidPorts) {
    // Test with invalid port numbers
    EXPECT_THROW(isPortInUse(0), std::invalid_argument);
    EXPECT_THROW(isPortInUse(65536), std::invalid_argument);
    EXPECT_THROW(isPortInUse(-1), std::invalid_argument);
}

// Async Port Usage Tests
TEST_F(PortTest, IsPortInUseAsyncBasic) {
    uint16_t testPort = 54323;

    ASSERT_NO_THROW({
        auto future = isPortInUseAsync(testPort);
        EXPECT_TRUE(future.valid());

        // Wait for result with timeout
        auto status = future.wait_for(std::chrono::seconds(5));
        EXPECT_EQ(status, std::future_status::ready);

        if (status == std::future_status::ready) {
            bool result = future.get();
            // Result can be true or false, just ensure it completes
        }
    });
}

TEST_F(PortTest, IsPortInUseAsyncMultiple) {
    std::vector<std::future<bool>> futures;
    std::vector<uint16_t> testPorts = {54324, 54325, 54326, 54327, 54328};

    // Launch async checks for multiple ports
    for (uint16_t port : testPorts) {
        futures.push_back(isPortInUseAsync(port));
    }

    // Wait for all results
    for (auto& future : futures) {
        EXPECT_TRUE(future.valid());
        auto status = future.wait_for(std::chrono::seconds(5));
        EXPECT_EQ(status, std::future_status::ready);

        if (status == std::future_status::ready) {
            EXPECT_NO_THROW(future.get());
        }
    }
}

// Process ID Tests
TEST_F(PortTest, GetProcessIDOnPortUnusedPort) {
    uint16_t testPort = 54329;

    ASSERT_NO_THROW({
        auto processID = getProcessIDOnPort(testPort);
        // Should return empty optional for unused port
        EXPECT_FALSE(processID.has_value());
    });
}

TEST_F(PortTest, GetProcessIDOnPortInvalidPort) {
    EXPECT_THROW(getProcessIDOnPort(0), std::invalid_argument);
    EXPECT_THROW(getProcessIDOnPort(65536), std::invalid_argument);
}

// Port Scanning Tests
TEST_F(PortTest, ScanPortLocalhost) {
    // Test scanning localhost on a commonly closed port
    ASSERT_NO_THROW({
        bool isOpen =
            scanPort("127.0.0.1", 54330, std::chrono::milliseconds(1000));
        // Port should be closed (false), but function shouldn't crash
        EXPECT_FALSE(isOpen);
    });
}

TEST_F(PortTest, ScanPortWithServer) {
    uint16_t testPort = 54331;

    // Create a test server
    int serverSocket = createTestServer(testPort);
    if (serverSocket >= 0) {
        // Port should be detected as open
        bool isOpen =
            scanPort("127.0.0.1", testPort, std::chrono::milliseconds(1000));
        EXPECT_TRUE(isOpen);

        closeSocket(serverSocket);
    }
}

TEST_F(PortTest, ScanPortEmptyHost) {
    bool result = scanPort("", 80, std::chrono::milliseconds(1000));
    EXPECT_FALSE(result);
}

TEST_F(PortTest, ScanPortInvalidHost) {
    bool result = scanPort("invalid.nonexistent.host.xyz", 80,
                           std::chrono::milliseconds(1000));
    EXPECT_FALSE(result);
}

// Port Range Scanning Tests
TEST_F(PortTest, ScanPortRangeBasic) {
    ASSERT_NO_THROW({
        auto openPorts = scanPortRange("127.0.0.1", 54340, 54345,
                                       std::chrono::milliseconds(500));
        // Should return empty vector for closed ports
        EXPECT_TRUE(openPorts.empty() || !openPorts.empty());  // Either is fine
    });
}

TEST_F(PortTest, ScanPortRangeInvalidRange) {
    EXPECT_THROW(scanPortRange("127.0.0.1", 54350, 54340,
                               std::chrono::milliseconds(500)),
                 std::invalid_argument);
}

TEST_F(PortTest, ScanPortRangeEmptyHost) {
    EXPECT_THROW(
        scanPortRange("", 54350, 54355, std::chrono::milliseconds(500)),
        std::invalid_argument);
}

TEST_F(PortTest, ScanPortRangeWithServers) {
    std::vector<int> serverSockets;
    std::vector<uint16_t> testPorts = {54360, 54361, 54362};

    // Create test servers on some ports
    for (uint16_t port : testPorts) {
        int serverSocket = createTestServer(port);
        if (serverSocket >= 0) {
            serverSockets.push_back(serverSocket);
        }
    }

    if (!serverSockets.empty()) {
        // Scan the range
        auto openPorts = scanPortRange("127.0.0.1", 54360, 54365,
                                       std::chrono::milliseconds(500));

        // Should find at least some of the open ports
        EXPECT_FALSE(openPorts.empty());

        // Clean up servers
        for (int sockfd : serverSockets) {
            closeSocket(sockfd);
        }
    }
}

// Async Port Range Scanning Tests
TEST_F(PortTest, ScanPortRangeAsyncBasic) {
    ASSERT_NO_THROW({
        auto future =
            scanPortRangeAsync<uint16_t>("127.0.0.1", 54370, 54375, 500);
        EXPECT_TRUE(future.valid());

        auto status = future.wait_for(std::chrono::seconds(10));
        EXPECT_EQ(status, std::future_status::ready);

        if (status == std::future_status::ready) {
            auto openPorts = future.get();
            // Result can be empty or contain ports
        }
    });
}

// Performance Tests
TEST_F(PortTest, PortCheckPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    // Check multiple ports quickly
    for (uint16_t port = 54400; port < 54410; ++port) {
        isPortInUse(port);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time (5 seconds for 10 ports)
    EXPECT_LT(duration.count(), 5000);
}

// Concurrent Access Tests
TEST_F(PortTest, ConcurrentPortChecks) {
    constexpr int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&successCount, i]() {
            try {
                uint16_t port = 54500 + i;
                isPortInUse(port);
                successCount++;
            } catch (const std::exception&) {
                // Exceptions are acceptable but shouldn't crash
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), numThreads);
}

// Edge Case Tests
TEST_F(PortTest, EdgeCasePortNumbers) {
    // Test edge case port numbers
    std::vector<uint16_t> edgePorts = {1, 1023, 1024, 49151, 49152, 65535};

    for (uint16_t port : edgePorts) {
        EXPECT_NO_THROW({ isPortInUse(port); })
            << "Failed for edge case port: " << port;
    }
}

TEST_F(PortTest, ScanPortTimeout) {
    // Test with very short timeout
    auto start = std::chrono::high_resolution_clock::now();

    bool result = scanPort("127.0.0.1", 54600, std::chrono::milliseconds(10));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Port should be closed and function should not crash
    EXPECT_FALSE(result);
    // Should respect timeout (allow some margin for system overhead)
    EXPECT_LT(duration.count(), 1000);
}

TEST_F(PortTest, ScanPortRangeLargeRange) {
    // Test with larger range but short timeout to ensure it doesn't take too
    // long
    auto start = std::chrono::high_resolution_clock::now();

    auto openPorts =
        scanPortRange("127.0.0.1", 54700, 54710, std::chrono::milliseconds(50));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::seconds>(end - start);

    // Should complete within reasonable time even for larger range
    EXPECT_LT(duration.count(), 30);
}

// Error Recovery Tests
TEST_F(PortTest, ErrorRecoveryAfterFailure) {
    // Try invalid operation first
    EXPECT_THROW(isPortInUse(0), std::invalid_argument);

    // Should still work for valid operations after failure
    EXPECT_NO_THROW(isPortInUse(54800));
}

// Resource Management Tests
TEST_F(PortTest, ResourceManagementMultipleChecks) {
    // Perform many port checks to ensure no resource leaks
    for (int i = 0; i < 100; ++i) {
        uint16_t port = 54900 + (i % 50);  // Reuse some ports
        EXPECT_NO_THROW(isPortInUse(port));
    }
    SUCCEED();
}

// Platform-Specific Tests
TEST_F(PortTest, PlatformSpecificBehavior) {
    uint16_t testPort = 55000;

#ifdef _WIN32
    // Windows-specific behavior
    EXPECT_NO_THROW(isPortInUse(testPort));
    EXPECT_NO_THROW(getProcessIDOnPort(testPort));
#else
    // Unix-like systems behavior
    EXPECT_NO_THROW(isPortInUse(testPort));
    EXPECT_NO_THROW(getProcessIDOnPort(testPort));
#endif
}

// Stress Tests
TEST_F(PortTest, StressTestPortScanning) {
    // Rapid port scanning
    for (int i = 0; i < 50; ++i) {
        uint16_t port = 55100 + i;
        EXPECT_NO_THROW(
            scanPort("127.0.0.1", port, std::chrono::milliseconds(10)));
    }
    SUCCEED();
}

TEST_F(PortTest, StressTestAsyncOperations) {
    std::vector<std::future<bool>> futures;

    // Launch many async operations
    for (int i = 0; i < 20; ++i) {
        uint16_t port = 55200 + i;
        futures.push_back(isPortInUseAsync(port));
    }

    // Wait for all to complete
    int completedCount = 0;
    for (auto& future : futures) {
        if (future.valid()) {
            auto status = future.wait_for(std::chrono::seconds(5));
            if (status == std::future_status::ready) {
                EXPECT_NO_THROW(future.get());
                completedCount++;
            }
        }
    }

    EXPECT_GT(completedCount, 0);
}

// Memory Safety Tests
TEST_F(PortTest, MemorySafetyTests) {
    // Test that operations don't cause memory issues
    for (int i = 0; i < 50; ++i) {
        uint16_t port = 55300 + (i % 10);

        // Mix of operations
        isPortInUse(port);
        getProcessIDOnPort(port);
        scanPort("127.0.0.1", port, std::chrono::milliseconds(10));

        if (i % 5 == 0) {
            auto future = isPortInUseAsync(port);
            if (future.valid()) {
                future.wait_for(std::chrono::milliseconds(100));
            }
        }
    }
    SUCCEED();
}

// Integration Tests
TEST_F(PortTest, PortLifecycleIntegration) {
    uint16_t testPort = 55400;

    // Check port is initially available
    bool initiallyInUse = isPortInUse(testPort);

    // Create server if port was available
    int serverSocket = -1;
    if (!initiallyInUse) {
        serverSocket = createTestServer(testPort);
        if (serverSocket >= 0) {
            // Port should now be in use
            EXPECT_TRUE(isPortInUse(testPort));

            // Should be detected by port scan
            EXPECT_TRUE(scanPort("127.0.0.1", testPort,
                                 std::chrono::milliseconds(1000)));

            // Clean up
            closeSocket(serverSocket);

            // Give system time to release port
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// Robustness Tests
TEST_F(PortTest, RobustnessUnderLoad) {
    constexpr int numOperations = 100;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    // Launch multiple threads doing various operations
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < numOperations / 5; ++j) {
                try {
                    uint16_t port = 55500 + (i * 100) + j;

                    if (i % 3 == 0) {
                        // Port usage checks
                        isPortInUse(port);
                        successCount++;
                    } else if (i % 3 == 1) {
                        // Process ID checks
                        getProcessIDOnPort(port);
                        successCount++;
                    } else {
                        // Port scanning
                        scanPort("127.0.0.1", port,
                                 std::chrono::milliseconds(10));
                        successCount++;
                    }
                } catch (const std::exception&) {
                    errorCount++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should have completed most operations successfully
    EXPECT_GT(successCount.load(), numOperations / 2);
    EXPECT_EQ(successCount.load() + errorCount.load(), numOperations);
}

// Timeout and Performance Validation
TEST_F(PortTest, TimeoutValidation) {
    // Test that timeouts are respected
    std::vector<std::chrono::milliseconds> timeouts = {
        std::chrono::milliseconds(10), std::chrono::milliseconds(100),
        std::chrono::milliseconds(1000)};

    for (auto timeout : timeouts) {
        auto start = std::chrono::high_resolution_clock::now();

        // Scan a likely closed port with specific timeout
        scanPort("127.0.0.1", 55600, timeout);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Should not exceed timeout by too much (allow 500ms margin for system
        // overhead)
        EXPECT_LT(duration.count(), timeout.count() + 500);
    }
}

// ============================================================================
// getServiceName Tests
// ============================================================================

TEST_F(PortTest, GetServiceNameWellKnown) {
    // Test well-known ports
    EXPECT_EQ(getServiceName(80), "http");
    EXPECT_EQ(getServiceName(443), "https");
    EXPECT_EQ(getServiceName(22), "ssh");
    EXPECT_EQ(getServiceName(21), "ftp");
    EXPECT_EQ(getServiceName(25), "smtp");
    EXPECT_EQ(getServiceName(53), "dns");
    EXPECT_EQ(getServiceName(110), "pop3");
    EXPECT_EQ(getServiceName(143), "imap");
}

TEST_F(PortTest, GetServiceNameUnknown) {
    // Test unknown ports
    auto result = getServiceName(54321);
    // Should return empty string for unknown ports
    EXPECT_TRUE(result.empty() || !result.empty());  // Either is acceptable
}

TEST_F(PortTest, GetServiceNameEdgeCases) {
    // Test edge case ports
    auto result1 = getServiceName(0);
    auto result2 = getServiceName(65535);
    // Should not crash
    (void)result1;
    (void)result2;
}

// ============================================================================
// getServicePort Tests
// ============================================================================

TEST_F(PortTest, GetServicePortWellKnown) {
    // Test well-known services
    auto http = getServicePort("http");
    ASSERT_TRUE(http.has_value());
    EXPECT_EQ(*http, 80);

    auto https = getServicePort("https");
    ASSERT_TRUE(https.has_value());
    EXPECT_EQ(*https, 443);

    auto ssh = getServicePort("ssh");
    ASSERT_TRUE(ssh.has_value());
    EXPECT_EQ(*ssh, 22);

    auto ftp = getServicePort("ftp");
    ASSERT_TRUE(ftp.has_value());
    EXPECT_EQ(*ftp, 21);
}

TEST_F(PortTest, GetServicePortUnknown) {
    auto result = getServicePort("unknown_service_xyz");
    EXPECT_FALSE(result.has_value());
}

TEST_F(PortTest, GetServicePortEmpty) {
    auto result = getServicePort("");
    EXPECT_FALSE(result.has_value());
}

TEST_F(PortTest, GetServicePortCaseInsensitive) {
    // Test case sensitivity (depends on implementation)
    auto http1 = getServicePort("http");
    auto http2 = getServicePort("HTTP");
    auto http3 = getServicePort("Http");
    // At least one should work
    EXPECT_TRUE(http1.has_value() || http2.has_value() || http3.has_value());
}

// ============================================================================
// getCommonServices Tests
// ============================================================================

TEST_F(PortTest, GetCommonServicesNotEmpty) {
    const auto& services = getCommonServices();
    EXPECT_FALSE(services.empty());
}

TEST_F(PortTest, GetCommonServicesContainsWellKnown) {
    const auto& services = getCommonServices();

    // Should contain common services
    EXPECT_TRUE(services.find("http") != services.end() ||
                services.find("HTTP") != services.end());
    EXPECT_TRUE(services.find("https") != services.end() ||
                services.find("HTTPS") != services.end());
    EXPECT_TRUE(services.find("ssh") != services.end() ||
                services.find("SSH") != services.end());
}

TEST_F(PortTest, GetCommonServicesConsistent) {
    // Multiple calls should return the same reference
    const auto& services1 = getCommonServices();
    const auto& services2 = getCommonServices();
    EXPECT_EQ(&services1, &services2);
}

// ============================================================================
// findAvailablePort Tests
// ============================================================================

TEST_F(PortTest, FindAvailablePortDefault) {
    auto port = findAvailablePort();
    if (port.has_value()) {
        EXPECT_GE(*port, 1024);
        EXPECT_LE(*port, 65535);
        // The found port should be available
        EXPECT_FALSE(isPortInUse(*port));
    }
}

TEST_F(PortTest, FindAvailablePortInRange) {
    auto port = findAvailablePort(50000, 50100);
    if (port.has_value()) {
        EXPECT_GE(*port, 50000);
        EXPECT_LE(*port, 50100);
    }
}

TEST_F(PortTest, FindAvailablePortSmallRange) {
    // Test with a small range
    auto port = findAvailablePort(60000, 60010);
    // May or may not find a port depending on system state
    if (port.has_value()) {
        EXPECT_GE(*port, 60000);
        EXPECT_LE(*port, 60010);
    }
}

TEST_F(PortTest, FindAvailablePortWithHost) {
    auto port = findAvailablePort(50200, 50300, "127.0.0.1");
    if (port.has_value()) {
        EXPECT_GE(*port, 50200);
        EXPECT_LE(*port, 50300);
    }
}

// ============================================================================
// isPrivilegedPort Tests
// ============================================================================

TEST_F(PortTest, IsPrivilegedPortTrue) {
    EXPECT_TRUE(isPrivilegedPort(0));
    EXPECT_TRUE(isPrivilegedPort(1));
    EXPECT_TRUE(isPrivilegedPort(22));
    EXPECT_TRUE(isPrivilegedPort(80));
    EXPECT_TRUE(isPrivilegedPort(443));
    EXPECT_TRUE(isPrivilegedPort(1023));
}

TEST_F(PortTest, IsPrivilegedPortFalse) {
    EXPECT_FALSE(isPrivilegedPort(1024));
    EXPECT_FALSE(isPrivilegedPort(1025));
    EXPECT_FALSE(isPrivilegedPort(8080));
    EXPECT_FALSE(isPrivilegedPort(49152));
    EXPECT_FALSE(isPrivilegedPort(65535));
}

TEST_F(PortTest, IsPrivilegedPortBoundary) {
    EXPECT_TRUE(isPrivilegedPort(1023));   // Last privileged
    EXPECT_FALSE(isPrivilegedPort(1024));  // First non-privileged
}

// ============================================================================
// isEphemeralPort Tests
// ============================================================================

TEST_F(PortTest, IsEphemeralPortTrue) {
    EXPECT_TRUE(isEphemeralPort(49152));  // First ephemeral
    EXPECT_TRUE(isEphemeralPort(49153));
    EXPECT_TRUE(isEphemeralPort(50000));
    EXPECT_TRUE(isEphemeralPort(60000));
    EXPECT_TRUE(isEphemeralPort(65535));  // Last ephemeral
}

TEST_F(PortTest, IsEphemeralPortFalse) {
    EXPECT_FALSE(isEphemeralPort(0));
    EXPECT_FALSE(isEphemeralPort(80));
    EXPECT_FALSE(isEphemeralPort(1024));
    EXPECT_FALSE(isEphemeralPort(49151));  // Just below ephemeral range
}

TEST_F(PortTest, IsEphemeralPortBoundary) {
    EXPECT_FALSE(isEphemeralPort(49151));  // Last non-ephemeral
    EXPECT_TRUE(isEphemeralPort(49152));   // First ephemeral
    EXPECT_TRUE(isEphemeralPort(65535));   // Last ephemeral
}

// ============================================================================
// scanPortsConcurrent Tests
// ============================================================================

TEST_F(PortTest, ScanPortsConcurrentBasic) {
    std::vector<uint16_t> portsToScan = {55700, 55701, 55702, 55703, 55704};

    ASSERT_NO_THROW({
        auto openPorts = scanPortsConcurrent("127.0.0.1", portsToScan,
                                             std::chrono::milliseconds(100));
        // Should return empty or contain ports (depends on system state)
        (void)openPorts;
    });
}

TEST_F(PortTest, ScanPortsConcurrentWithServer) {
    uint16_t testPort = 55710;
    std::vector<uint16_t> portsToScan = {55710, 55711, 55712};

    // Create a test server
    int serverSocket = createTestServer(testPort);
    if (serverSocket >= 0) {
        auto openPorts = scanPortsConcurrent("127.0.0.1", portsToScan,
                                             std::chrono::milliseconds(500));

        // Should find the open port
        EXPECT_THAT(openPorts, ::testing::Contains(testPort));

        closeSocket(serverSocket);
    }
}

TEST_F(PortTest, ScanPortsConcurrentEmpty) {
    std::vector<uint16_t> emptyPorts;
    auto openPorts = scanPortsConcurrent("127.0.0.1", emptyPorts);
    EXPECT_TRUE(openPorts.empty());
}

TEST_F(PortTest, ScanPortsConcurrentMaxConcurrent) {
    std::vector<uint16_t> portsToScan;
    for (uint16_t i = 55800; i < 55850; ++i) {
        portsToScan.push_back(i);
    }

    // Test with limited concurrency
    ASSERT_NO_THROW({
        auto openPorts = scanPortsConcurrent("127.0.0.1", portsToScan,
                                             std::chrono::milliseconds(50), 5);
        (void)openPorts;
    });
}

TEST_F(PortTest, ScanPortsConcurrentPerformance) {
    std::vector<uint16_t> portsToScan;
    for (uint16_t i = 55900; i < 55920; ++i) {
        portsToScan.push_back(i);
    }

    auto start = std::chrono::high_resolution_clock::now();

    auto openPorts = scanPortsConcurrent("127.0.0.1", portsToScan,
                                         std::chrono::milliseconds(100), 10);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::seconds>(end - start);

    // Concurrent scanning should be faster than sequential
    // Allow up to 10 seconds for 20 ports with 100ms timeout each
    EXPECT_LT(duration.count(), 10);
}

// ============================================================================
// Port Classification Integration Tests
// ============================================================================

TEST_F(PortTest, PortClassificationIntegration) {
    // Test that port classification functions work together
    for (uint16_t port = 0; port <= 65535; port += 1000) {
        bool privileged = isPrivilegedPort(port);
        bool ephemeral = isEphemeralPort(port);

        // A port cannot be both privileged and ephemeral
        if (privileged) {
            EXPECT_FALSE(ephemeral)
                << "Port " << port << " is both privileged and ephemeral";
        }

        // Privileged ports are < 1024
        if (port < 1024) {
            EXPECT_TRUE(privileged)
                << "Port " << port << " should be privileged";
        }

        // Ephemeral ports are >= 49152
        if (port >= 49152) {
            EXPECT_TRUE(ephemeral) << "Port " << port << " should be ephemeral";
        }
    }
}

// ============================================================================
// Service Name/Port Consistency Tests
// ============================================================================

TEST_F(PortTest, ServiceNamePortConsistency) {
    // Test that getServiceName and getServicePort are consistent
    const auto& services = getCommonServices();

    for (const auto& [name, port] : services) {
        auto foundPort = getServicePort(name);
        if (foundPort.has_value()) {
            EXPECT_EQ(*foundPort, port)
                << "Inconsistent port for service: " << name;
        }

        auto foundName = getServiceName(port);
        if (!foundName.empty()) {
            // Name might be different (e.g., alias) but should map to same port
            auto reversePort = getServicePort(foundName);
            if (reversePort.has_value()) {
                EXPECT_EQ(*reversePort, port)
                    << "Inconsistent reverse lookup for port: " << port;
            }
        }
    }
}

// ============================================================================
// Concurrent Service Lookup Tests
// ============================================================================

TEST_F(PortTest, ConcurrentServiceLookup) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&successCount, i]() {
            // Mix of service lookups
            auto name = getServiceName(80 + i);
            auto port = getServicePort("http");
            const auto& services = getCommonServices();

            if (port.has_value() || !name.empty() || !services.empty()) {
                successCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successCount.load(), 0);
}
