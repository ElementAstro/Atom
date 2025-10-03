// filepath: tests/web/utils/test_network.hpp
#ifndef TEST_NETWORK_HPP
#define TEST_NETWORK_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <spdlog/spdlog.h>

#include "atom/web/utils/network.hpp"
#include "atom/web/utils/socket.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

using namespace atom::web;

class NetworkTest : public ::testing::Test {
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

    bool socketInitialized = false;
};

// Basic Internet Connectivity Tests
TEST_F(NetworkTest, CheckInternetConnectivityBasic) {
    ASSERT_NO_THROW({
        bool hasInternet = checkInternetConnectivity();
        // Result can be true or false depending on environment
        // The important thing is that it doesn't crash
    });
}

TEST_F(NetworkTest, CheckInternetConnectivityRepeated) {
    // Test multiple calls to ensure consistency
    std::vector<bool> results;
    
    for (int i = 0; i < 5; ++i) {
        bool result = checkInternetConnectivity();
        results.push_back(result);
    }
    
    // Results should be consistent (all true or all false)
    bool firstResult = results[0];
    for (bool result : results) {
        EXPECT_EQ(result, firstResult) << "Inconsistent connectivity results";
    }
}

// Socket Initialization Tests
TEST_F(NetworkTest, InitializeWindowsSocketAPIMultipleCalls) {
    // Multiple calls should be safe
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(initializeWindowsSocketAPI());
    }
}

TEST_F(NetworkTest, InitializeWindowsSocketAPIConcurrent) {
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
TEST_F(NetworkTest, CreateSocketBasic) {
    ASSERT_NO_THROW({
        int sockfd = createSocket();
        EXPECT_GE(sockfd, 0);
        
        if (sockfd >= 0) {
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
        }
    });
}

TEST_F(NetworkTest, CreateMultipleSockets) {
    std::vector<int> sockets;
    
    // Create multiple sockets
    for (int i = 0; i < 10; ++i) {
        int sockfd = createSocket();
        EXPECT_GE(sockfd, 0) << "Failed to create socket " << i;
        if (sockfd >= 0) {
            sockets.push_back(sockfd);
        }
    }
    
    // Clean up all sockets
    for (int sockfd : sockets) {
#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif
    }
}

// Socket Binding Tests
TEST_F(NetworkTest, BindSocketValidPort) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);
    
    // Try to bind to a high port number (less likely to be in use)
    bool bindResult = bindSocket(sockfd, 12345);
    
    // Result depends on whether port is available
    // The important thing is that it doesn't crash
    EXPECT_NO_THROW(bindSocket(sockfd, 12345));
    
#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
}

TEST_F(NetworkTest, BindSocketInvalidSocket) {
    // Test with invalid socket descriptor
    bool result = bindSocket(-1, 12345);
    EXPECT_FALSE(result);
}

TEST_F(NetworkTest, BindSocketPortZero) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);
    
    // Port 0 should let the system choose a port
    bool result = bindSocket(sockfd, 0);
    // This might succeed or fail depending on system
    
#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
}

// Socket Non-blocking Mode Tests
TEST_F(NetworkTest, SetSocketNonBlockingValid) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);
    
    bool result = setSocketNonBlocking(sockfd);
    EXPECT_TRUE(result);
    
#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
}

TEST_F(NetworkTest, SetSocketNonBlockingInvalid) {
    // Test with invalid socket descriptor
    bool result = setSocketNonBlocking(-1);
    EXPECT_FALSE(result);
}

// Connection Timeout Tests
TEST_F(NetworkTest, ConnectWithTimeoutInvalidSocket) {
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    
    bool result = connectWithTimeout(-1, 
                                   reinterpret_cast<struct sockaddr*>(&addr),
                                   sizeof(addr),
                                   std::chrono::milliseconds(1000));
    EXPECT_FALSE(result);
}

TEST_F(NetworkTest, ConnectWithTimeoutNullAddress) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);
    
    bool result = connectWithTimeout(sockfd, nullptr, 0, 
                                   std::chrono::milliseconds(1000));
    EXPECT_FALSE(result);
    
#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
}

// Performance Tests
TEST_F(NetworkTest, InternetConnectivityPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    
    bool hasInternet = checkInternetConnectivity();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (10 seconds)
    EXPECT_LT(duration.count(), 10000);
}

TEST_F(NetworkTest, SocketCreationPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<int> sockets;
    for (int i = 0; i < 100; ++i) {
        int sockfd = createSocket();
        if (sockfd >= 0) {
            sockets.push_back(sockfd);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Clean up
    for (int sockfd : sockets) {
#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif
    }
    
    // Should complete within reasonable time (1 second)
    EXPECT_LT(duration.count(), 1000);
}

// Concurrent Access Tests
TEST_F(NetworkTest, ConcurrentInternetConnectivityCheck) {
    constexpr int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> completedCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&completedCount]() {
            try {
                checkInternetConnectivity();
                completedCount++;
            } catch (const std::exception&) {
                // Exceptions are acceptable but shouldn't crash
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completedCount.load(), numThreads);
}

TEST_F(NetworkTest, ConcurrentSocketCreation) {
    constexpr int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&successCount]() {
            try {
                int sockfd = createSocket();
                if (sockfd >= 0) {
                    successCount++;
#ifdef _WIN32
                    closesocket(sockfd);
#else
                    close(sockfd);
#endif
                }
            } catch (const std::exception&) {
                // Exceptions are acceptable but shouldn't crash
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successCount.load(), 0);
}

// Error Handling Tests
TEST_F(NetworkTest, SocketErrorHandling) {
    // Test various error conditions
    EXPECT_FALSE(bindSocket(-1, 80));
    EXPECT_FALSE(setSocketNonBlocking(-1));

    struct sockaddr_in addr{};
    EXPECT_FALSE(connectWithTimeout(-1,
                                   reinterpret_cast<struct sockaddr*>(&addr),
                                   sizeof(addr),
                                   std::chrono::milliseconds(100)));
}

// Platform-Specific Tests
TEST_F(NetworkTest, PlatformSpecificBehavior) {
#ifdef _WIN32
    // Windows-specific tests
    EXPECT_TRUE(initializeWindowsSocketAPI());

    // Test WSA error handling
    int sockfd = createSocket();
    EXPECT_GE(sockfd, 0);
    if (sockfd >= 0) {
        closesocket(sockfd);
    }
#else
    // Unix-like systems tests
    EXPECT_TRUE(initializeWindowsSocketAPI());  // Should return true on non-Windows

    int sockfd = createSocket();
    EXPECT_GE(sockfd, 0);
    if (sockfd >= 0) {
        close(sockfd);
    }
#endif
}

// Resource Management Tests
TEST_F(NetworkTest, SocketResourceManagement) {
    std::vector<int> sockets;

    // Create many sockets
    for (int i = 0; i < 50; ++i) {
        int sockfd = createSocket();
        if (sockfd >= 0) {
            sockets.push_back(sockfd);
        }
    }

    EXPECT_FALSE(sockets.empty());

    // Clean up all sockets
    for (int sockfd : sockets) {
#ifdef _WIN32
        int result = closesocket(sockfd);
        EXPECT_EQ(result, 0) << "Failed to close socket " << sockfd;
#else
        int result = close(sockfd);
        EXPECT_EQ(result, 0) << "Failed to close socket " << sockfd;
#endif
    }
}

// Timeout Behavior Tests
TEST_F(NetworkTest, ConnectTimeoutBehavior) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);

    // Set socket to non-blocking
    EXPECT_TRUE(setSocketNonBlocking(sockfd));

    // Try to connect to a non-routable address with short timeout
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    inet_pton(AF_INET, "10.255.255.1", &addr.sin_addr);  // Non-routable

    auto start = std::chrono::high_resolution_clock::now();

    bool result = connectWithTimeout(sockfd,
                                   reinterpret_cast<struct sockaddr*>(&addr),
                                   sizeof(addr),
                                   std::chrono::milliseconds(100));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Connection should fail (non-routable address) but not crash
    EXPECT_FALSE(result);
    // Should timeout within reasonable time
    EXPECT_LT(duration.count(), 1000);

#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
}

// Edge Case Tests
TEST_F(NetworkTest, EdgeCasePortNumbers) {
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);

    // Test edge case port numbers
    std::vector<uint16_t> testPorts = {0, 1, 65535};

    for (uint16_t port : testPorts) {
        // These may succeed or fail depending on system permissions
        // The important thing is they don't crash
        EXPECT_NO_THROW(bindSocket(sockfd, port));
    }

#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
}

// Stress Tests
TEST_F(NetworkTest, StressTestSocketOperations) {
    // Rapidly create and destroy sockets
    for (int i = 0; i < 1000; ++i) {
        int sockfd = createSocket();
        if (sockfd >= 0) {
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
        }
    }
    SUCCEED();
}

TEST_F(NetworkTest, StressTestConnectivityCheck) {
    // Multiple rapid connectivity checks
    for (int i = 0; i < 10; ++i) {
        EXPECT_NO_THROW(checkInternetConnectivity());
    }
    SUCCEED();
}

// Memory Safety Tests
TEST_F(NetworkTest, MemorySafetyTests) {
    // Test that operations don't cause memory leaks or corruption
    for (int i = 0; i < 100; ++i) {
        // Mix of operations
        initializeWindowsSocketAPI();

        int sockfd = createSocket();
        if (sockfd >= 0) {
            setSocketNonBlocking(sockfd);
            bindSocket(sockfd, 0);

#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
        }

        if (i % 10 == 0) {
            checkInternetConnectivity();
        }
    }
    SUCCEED();
}

// Integration Tests
TEST_F(NetworkTest, SocketLifecycleIntegration) {
    // Test complete socket lifecycle
    int sockfd = createSocket();
    ASSERT_GE(sockfd, 0);

    // Set non-blocking
    EXPECT_TRUE(setSocketNonBlocking(sockfd));

    // Bind to any available port
    EXPECT_TRUE(bindSocket(sockfd, 0));

    // Clean up
#ifdef _WIN32
    EXPECT_EQ(closesocket(sockfd), 0);
#else
    EXPECT_EQ(close(sockfd), 0);
#endif
}

// Robustness Tests
TEST_F(NetworkTest, RobustnessUnderLoad) {
    constexpr int numOperations = 100;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    // Launch multiple threads doing various operations
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < numOperations / 5; ++j) {
                try {
                    if (i % 3 == 0) {
                        // Socket operations
                        int sockfd = createSocket();
                        if (sockfd >= 0) {
                            setSocketNonBlocking(sockfd);
#ifdef _WIN32
                            closesocket(sockfd);
#else
                            close(sockfd);
#endif
                            successCount++;
                        } else {
                            errorCount++;
                        }
                    } else if (i % 3 == 1) {
                        // Initialization
                        if (initializeWindowsSocketAPI()) {
                            successCount++;
                        } else {
                            errorCount++;
                        }
                    } else {
                        // Connectivity check
                        checkInternetConnectivity();
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

#endif  // TEST_NETWORK_HPP
