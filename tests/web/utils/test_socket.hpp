// filepath: tests/web/utils/test_socket.hpp
#ifndef TEST_SOCKET_HPP
#define TEST_SOCKET_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "atom/web/utils/socket.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
#elif defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace atom::web;

class SocketTest : public ::testing::Test {
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

    // Helper method to create a sockaddr_in structure
    struct sockaddr_in createSockAddr(const std::string& ip, uint16_t port) {
        struct sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
        return addr;
    }

    bool socketInitialized = false;
};

// Socket API Initialization Tests
TEST_F(SocketTest, InitializeWindowsSocketAPIBasic) {
    // Should succeed (already initialized in SetUp)
    EXPECT_TRUE(initializeWindowsSocketAPI());
}

TEST_F(SocketTest, InitializeWindowsSocketAPIMultipleCalls) {
    // Multiple calls should be safe and return true
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(initializeWindowsSocketAPI());
    }
}

TEST_F(SocketTest, InitializeWindowsSocketAPIConcurrent) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&successCount]() {
            if (initializeWindowsSocketAPI()) {
                successCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), numThreads);
}

// Socket Creation Tests
TEST_F(SocketTest, CreateSocketBasic) {
    int sockfd = createSocket();
    EXPECT_GE(sockfd, 0);

    if (sockfd >= 0) {
        closeSocket(sockfd);
    }
}

TEST_F(SocketTest, CreateMultipleSockets) {
    std::vector<int> sockets;

    // Create multiple sockets
    for (int i = 0; i < 10; ++i) {
        int sockfd = createSocket();
        EXPECT_GE(sockfd, 0) << "Failed to create socket " << i;
        if (sockfd >= 0) {
            sockets.push_back(sockfd);
        }
    }

    EXPECT_FALSE(sockets.empty());

    // Clean up all sockets
    for (int sockfd : sockets) {
        closeSocket(sockfd);
    }
}

TEST_F(SocketTest, CreateSocketResourceManagement) {
    // Create and immediately close many sockets to test resource management
    for (int i = 0; i < 100; ++i) {
        int sockfd = createSocket();
        EXPECT_GE(sockfd, 0);
        if (sockfd >= 0) {
            closeSocket(sockfd);
        }
    }
    SUCCEED();
}

// Socket Binding Tests
TEST_F(SocketTest, BindSocketValidPort) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);

    // Try to bind to a high port number (less likely to be in use)
    bool bindResult = bindSocket(sockfd, 56789);

    // Result depends on whether port is available
    // The important thing is that it doesn't crash
    EXPECT_NO_THROW(bindSocket(sockfd, 56789));

    closeSocket(sockfd);
}

TEST_F(SocketTest, BindSocketPortZero) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);

    // Port 0 should let the system choose a port
    bool result = bindSocket(sockfd, 0);
    EXPECT_TRUE(result);  // Should succeed with system-chosen port

    closeSocket(sockfd);
}

TEST_F(SocketTest, BindSocketInvalidSocket) {
    // Test with invalid socket descriptor
    bool result = bindSocket(-1, 12345);
    EXPECT_FALSE(result);
}

TEST_F(SocketTest, BindSocketSamePortTwice) {
    uint16_t testPort = 56790;

    int sockfd1 = createSocket();
    int sockfd2 = createSocket();
    ASSERT_GE(sockfd1, 0);
    ASSERT_GE(sockfd2, 0);

    // First bind should succeed
    bool result1 = bindSocket(sockfd1, testPort);

    if (result1) {
        // Second bind to same port should fail
        bool result2 = bindSocket(sockfd2, testPort);
        EXPECT_FALSE(result2);
    }

    closeSocket(sockfd1);
    closeSocket(sockfd2);
}

// Non-blocking Socket Tests
TEST_F(SocketTest, SetSocketNonBlockingValid) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);

    bool result = setSocketNonBlocking(sockfd);
    EXPECT_TRUE(result);

    closeSocket(sockfd);
}

TEST_F(SocketTest, SetSocketNonBlockingInvalid) {
    // Test with invalid socket descriptor
    bool result = setSocketNonBlocking(-1);
    EXPECT_FALSE(result);
}

TEST_F(SocketTest, SetSocketNonBlockingMultipleCalls) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);

    // Multiple calls should be safe
    EXPECT_TRUE(setSocketNonBlocking(sockfd));
    EXPECT_TRUE(setSocketNonBlocking(sockfd));
    EXPECT_TRUE(setSocketNonBlocking(sockfd));

    closeSocket(sockfd);
}

// Connection Timeout Tests
TEST_F(SocketTest, ConnectWithTimeoutInvalidSocket) {
    auto addr = createSockAddr("127.0.0.1", 80);

    bool result =
        connectWithTimeout(-1, reinterpret_cast<struct sockaddr*>(&addr),
                           sizeof(addr), std::chrono::milliseconds(1000));
    EXPECT_FALSE(result);
}

TEST_F(SocketTest, ConnectWithTimeoutNullAddress) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);

    bool result =
        connectWithTimeout(sockfd, nullptr, 0, std::chrono::milliseconds(1000));
    EXPECT_FALSE(result);

    closeSocket(sockfd);
}

TEST_F(SocketTest, ConnectWithTimeoutUnreachableAddress) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);

    // Set socket to non-blocking first
    EXPECT_TRUE(setSocketNonBlocking(sockfd));

    // Try to connect to a non-routable address
    auto addr = createSockAddr("10.255.255.1", 12345);

    auto start = std::chrono::high_resolution_clock::now();

    bool result =
        connectWithTimeout(sockfd, reinterpret_cast<struct sockaddr*>(&addr),
                           sizeof(addr), std::chrono::milliseconds(100));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should fail and respect timeout
    EXPECT_FALSE(result);
    EXPECT_LT(duration.count(), 1000);  // Should timeout quickly

    closeSocket(sockfd);
}

TEST_F(SocketTest, ConnectWithTimeoutZeroTimeout) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);

    setSocketNonBlocking(sockfd);

    auto addr = createSockAddr("127.0.0.1", 56791);

    bool result =
        connectWithTimeout(sockfd, reinterpret_cast<struct sockaddr*>(&addr),
                           sizeof(addr), std::chrono::milliseconds(0));

    // Should fail immediately with zero timeout
    EXPECT_FALSE(result);

    closeSocket(sockfd);
}

// Performance Tests
TEST_F(SocketTest, SocketCreationPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<int> sockets;
    for (int i = 0; i < 100; ++i) {
        int sockfd = createSocket();
        if (sockfd >= 0) {
            sockets.push_back(sockfd);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Clean up
    for (int sockfd : sockets) {
        closeSocket(sockfd);
    }

    // Should complete within reasonable time (1 second)
    EXPECT_LT(duration.count(), 1000);
    EXPECT_FALSE(sockets.empty());
}

TEST_F(SocketTest, SocketBindingPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<int> sockets;
    for (int i = 0; i < 50; ++i) {
        int sockfd = createSocket();
        if (sockfd >= 0) {
            bindSocket(sockfd, 0);  // Let system choose port
            sockets.push_back(sockfd);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Clean up
    for (int sockfd : sockets) {
        closeSocket(sockfd);
    }

    // Should complete within reasonable time (2 seconds)
    EXPECT_LT(duration.count(), 2000);
}

// Concurrent Access Tests
TEST_F(SocketTest, ConcurrentSocketCreation) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::vector<std::vector<int>> threadSockets(numThreads);

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                for (int j = 0; j < 5; ++j) {
                    int sockfd = createSocket();
                    if (sockfd >= 0) {
                        threadSockets[i].push_back(sockfd);
                        successCount++;
                    }
                }
            } catch (const std::exception&) {
                // Exceptions are acceptable but shouldn't crash
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Clean up all sockets
    for (auto& sockets : threadSockets) {
        for (int sockfd : sockets) {
            closeSocket(sockfd);
        }
    }

    EXPECT_GT(successCount.load(), 0);
}

TEST_F(SocketTest, ConcurrentSocketBinding) {
    constexpr int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::vector<std::vector<int>> threadSockets(numThreads);

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                int sockfd = createSocket();
                if (sockfd >= 0) {
                    threadSockets[i].push_back(sockfd);
                    if (bindSocket(sockfd, 0)) {  // Let system choose port
                        successCount++;
                    }
                }
            } catch (const std::exception&) {
                // Exceptions are acceptable but shouldn't crash
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Clean up all sockets
    for (auto& sockets : threadSockets) {
        for (int sockfd : sockets) {
            closeSocket(sockfd);
        }
    }

    EXPECT_GT(successCount.load(), 0);
}

// Error Handling Tests
TEST_F(SocketTest, ErrorHandlingInvalidOperations) {
    // Test various invalid operations
    EXPECT_FALSE(bindSocket(-1, 80));
    EXPECT_FALSE(setSocketNonBlocking(-1));

    struct sockaddr_in addr {};
    EXPECT_FALSE(
        connectWithTimeout(-1, reinterpret_cast<struct sockaddr*>(&addr),
                           sizeof(addr), std::chrono::milliseconds(100)));
}

// Platform-Specific Tests
TEST_F(SocketTest, PlatformSpecificBehavior) {
#ifdef _WIN32
    // Windows-specific tests
    EXPECT_TRUE(initializeWindowsSocketAPI());

    int sockfd = createSocket();
    EXPECT_GE(sockfd, 0);
    if (sockfd >= 0) {
        EXPECT_TRUE(setSocketNonBlocking(sockfd));
        closesocket(sockfd);
    }
#else
    // Unix-like systems tests
    EXPECT_TRUE(
        initializeWindowsSocketAPI());  // Should return true on non-Windows

    int sockfd = createSocket();
    EXPECT_GE(sockfd, 0);
    if (sockfd >= 0) {
        EXPECT_TRUE(setSocketNonBlocking(sockfd));
        close(sockfd);
    }
#endif
}

// Resource Management Tests
TEST_F(SocketTest, ResourceManagementStressTest) {
    // Create and destroy many sockets rapidly
    for (int i = 0; i < 500; ++i) {
        int sockfd = createSocket();
        if (sockfd >= 0) {
            if (i % 2 == 0) {
                setSocketNonBlocking(sockfd);
            }
            if (i % 3 == 0) {
                bindSocket(sockfd, 0);
            }
            closeSocket(sockfd);
        }
    }
    SUCCEED();
}

// Edge Case Tests
TEST_F(SocketTest, EdgeCasePortNumbers) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);

    // Test edge case port numbers
    std::vector<uint16_t> testPorts = {0, 1, 1023, 1024, 49151, 49152, 65535};

    for (uint16_t port : testPorts) {
        // These may succeed or fail depending on system permissions
        // The important thing is they don't crash
        EXPECT_NO_THROW(bindSocket(sockfd, port));
    }

    closeSocket(sockfd);
}

// Memory Safety Tests
TEST_F(SocketTest, MemorySafetyTests) {
    // Test that operations don't cause memory issues
    for (int i = 0; i < 100; ++i) {
        // Mix of operations
        initializeWindowsSocketAPI();

        int sockfd = createSocket();
        if (sockfd >= 0) {
            setSocketNonBlocking(sockfd);
            bindSocket(sockfd, 0);

            // Test connection with invalid address (should fail gracefully)
            struct sockaddr_in addr {};
            addr.sin_family = AF_INET;
            connectWithTimeout(sockfd,
                               reinterpret_cast<struct sockaddr*>(&addr),
                               sizeof(addr), std::chrono::milliseconds(1));

            closeSocket(sockfd);
        }
    }
    SUCCEED();
}

// Integration Tests
TEST_F(SocketTest, SocketLifecycleIntegration) {
    // Test complete socket lifecycle
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);

    // Set non-blocking
    EXPECT_TRUE(setSocketNonBlocking(sockfd));

    // Bind to any available port
    EXPECT_TRUE(bindSocket(sockfd, 0));

    // Try to connect to localhost (should fail but not crash)
    auto addr = createSockAddr("127.0.0.1", 56792);
    bool connectResult =
        connectWithTimeout(sockfd, reinterpret_cast<struct sockaddr*>(&addr),
                           sizeof(addr), std::chrono::milliseconds(10));
    // Connection will likely fail, but that's expected
    EXPECT_FALSE(connectResult);  // Should fail to connect to closed port

    // Clean up
    closeSocket(sockfd);
    SUCCEED();
}

// Timeout Behavior Tests
TEST_F(SocketTest, TimeoutBehaviorValidation) {
    std::vector<std::chrono::milliseconds> timeouts = {
        std::chrono::milliseconds(1), std::chrono::milliseconds(10),
        std::chrono::milliseconds(100), std::chrono::milliseconds(1000)};

    for (auto timeout : timeouts) {
        int sockfd = createSocket();
        ASSERT_GE(sockfd, 0);

        setSocketNonBlocking(sockfd);

        auto start = std::chrono::high_resolution_clock::now();

        // Try to connect to non-routable address
        auto addr = createSockAddr("10.255.255.1", 56793);
        bool result = connectWithTimeout(
            sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr),
            timeout);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Should fail and respect timeout (allow some margin)
        EXPECT_FALSE(result);
        EXPECT_LT(duration.count(), timeout.count() + 500);

        closeSocket(sockfd);
    }
}

// Robustness Tests
TEST_F(SocketTest, RobustnessUnderLoad) {
    constexpr int numOperations = 200;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    // Launch multiple threads doing various operations
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < numOperations / 5; ++j) {
                try {
                    if (i % 4 == 0) {
                        // Socket creation and destruction
                        int sockfd = createSocket();
                        if (sockfd >= 0) {
                            closeSocket(sockfd);
                            successCount++;
                        } else {
                            errorCount++;
                        }
                    } else if (i % 4 == 1) {
                        // Socket binding
                        int sockfd = createSocket();
                        if (sockfd >= 0) {
                            bindSocket(sockfd, 0);
                            closeSocket(sockfd);
                            successCount++;
                        } else {
                            errorCount++;
                        }
                    } else if (i % 4 == 2) {
                        // Non-blocking mode
                        int sockfd = createSocket();
                        if (sockfd >= 0) {
                            setSocketNonBlocking(sockfd);
                            closeSocket(sockfd);
                            successCount++;
                        } else {
                            errorCount++;
                        }
                    } else {
                        // Initialization
                        if (initializeWindowsSocketAPI()) {
                            successCount++;
                        } else {
                            errorCount++;
                        }
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

#endif  // TEST_SOCKET_HPP
