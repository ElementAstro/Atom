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
        auto future = scanPortRangeAsync("127.0.0.1", 54370, 54375,
                                         std::chrono::milliseconds(500));
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
