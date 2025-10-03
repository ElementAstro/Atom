// filepath: tests/web/utils/test_addr_info.hpp
#ifndef TEST_ADDR_INFO_HPP
#define TEST_ADDR_INFO_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include <spdlog/spdlog.h>

#include "atom/web/utils/addr_info.hpp"

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
#if defined(__linux__) || defined(__APPLE__)
#include <netdb.h>
#include <sys/socket.h>
#elif defined(_WIN32)
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
#endif
#endif

using namespace atom::web;

class AddrInfoTest : public ::testing::Test {
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

    // Helper method to check internet connectivity
    bool hasInternetConnectivity() {
        try {
            auto addrInfo = getAddrInfo("8.8.8.8", "53");
            return addrInfo != nullptr;
        } catch (const std::exception&) {
            return false;
        }
    }

    // Helper method to create a simple addrinfo structure for testing
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> createTestAddrInfo() {
        try {
            return getAddrInfo("127.0.0.1", "80");
        } catch (const std::exception&) {
            return {nullptr, ::freeaddrinfo};
        }
    }
};

// Basic getAddrInfo Tests
TEST_F(AddrInfoTest, GetAddrInfoValidHostname) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    ASSERT_NO_THROW({
        auto addrInfo = getAddrInfo("google.com", "80");
        EXPECT_NE(addrInfo, nullptr);
        EXPECT_NE(addrInfo.get(), nullptr);
    });
}

TEST_F(AddrInfoTest, GetAddrInfoLocalhost) {
    ASSERT_NO_THROW({
        auto addrInfo = getAddrInfo("localhost", "80");
        EXPECT_NE(addrInfo, nullptr);
        EXPECT_NE(addrInfo.get(), nullptr);
    });
}

TEST_F(AddrInfoTest, GetAddrInfoIPAddress) {
    ASSERT_NO_THROW({
        auto addrInfo = getAddrInfo("127.0.0.1", "80");
        EXPECT_NE(addrInfo, nullptr);
        EXPECT_NE(addrInfo.get(), nullptr);
    });
}

TEST_F(AddrInfoTest, GetAddrInfoEmptyHostname) {
    EXPECT_THROW(getAddrInfo("", "80"), std::invalid_argument);
}

TEST_F(AddrInfoTest, GetAddrInfoInvalidHostname) {
    EXPECT_THROW(getAddrInfo("invalid.nonexistent.domain.xyz", "80"), std::runtime_error);
}

TEST_F(AddrInfoTest, GetAddrInfoEmptyService) {
    ASSERT_NO_THROW({
        auto addrInfo = getAddrInfo("127.0.0.1", "");
        EXPECT_NE(addrInfo, nullptr);
    });
}

TEST_F(AddrInfoTest, GetAddrInfoValidService) {
    ASSERT_NO_THROW({
        auto addrInfo = getAddrInfo("127.0.0.1", "http");
        EXPECT_NE(addrInfo, nullptr);
    });
}

// addrInfoToString Tests
TEST_F(AddrInfoTest, AddrInfoToStringTextFormat) {
    auto addrInfo = createTestAddrInfo();
    if (!addrInfo) {
        GTEST_SKIP() << "Could not create test addrinfo";
    }

    ASSERT_NO_THROW({
        std::string result = addrInfoToString(addrInfo.get(), false);
        EXPECT_FALSE(result.empty());
        EXPECT_THAT(result, ::testing::HasSubstr("addrinfo"));
        EXPECT_THAT(result, ::testing::HasSubstr("Family"));
        EXPECT_THAT(result, ::testing::HasSubstr("Socktype"));
        EXPECT_THAT(result, ::testing::HasSubstr("Protocol"));
    });
}

TEST_F(AddrInfoTest, AddrInfoToStringJsonFormat) {
    auto addrInfo = createTestAddrInfo();
    if (!addrInfo) {
        GTEST_SKIP() << "Could not create test addrinfo";
    }

    ASSERT_NO_THROW({
        std::string result = addrInfoToString(addrInfo.get(), true);
        EXPECT_FALSE(result.empty());
        EXPECT_THAT(result, ::testing::HasSubstr("["));
        EXPECT_THAT(result, ::testing::HasSubstr("]"));
        EXPECT_THAT(result, ::testing::HasSubstr("family"));
        EXPECT_THAT(result, ::testing::HasSubstr("socktype"));
        EXPECT_THAT(result, ::testing::HasSubstr("protocol"));
    });
}

TEST_F(AddrInfoTest, AddrInfoToStringNullPointer) {
    EXPECT_THROW(addrInfoToString(nullptr, false), std::invalid_argument);
    EXPECT_THROW(addrInfoToString(nullptr, true), std::invalid_argument);
}

// dumpAddrInfo Tests
TEST_F(AddrInfoTest, DumpAddrInfoValid) {
    auto sourceAddrInfo = createTestAddrInfo();
    if (!sourceAddrInfo) {
        GTEST_SKIP() << "Could not create test addrinfo";
    }

    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> dstAddrInfo(nullptr, ::freeaddrinfo);
    
    int result = dumpAddrInfo(dstAddrInfo, sourceAddrInfo.get());
    EXPECT_EQ(result, 0);
    EXPECT_NE(dstAddrInfo, nullptr);
    EXPECT_NE(dstAddrInfo.get(), nullptr);
}

TEST_F(AddrInfoTest, DumpAddrInfoNullSource) {
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> dstAddrInfo(nullptr, ::freeaddrinfo);
    
    int result = dumpAddrInfo(dstAddrInfo, nullptr);
    EXPECT_EQ(result, -1);
}

// compareAddrInfo Tests
TEST_F(AddrInfoTest, CompareAddrInfoEqual) {
    auto addrInfo1 = createTestAddrInfo();
    auto addrInfo2 = createTestAddrInfo();
    
    if (!addrInfo1 || !addrInfo2) {
        GTEST_SKIP() << "Could not create test addrinfo";
    }

    ASSERT_NO_THROW({
        bool result = compareAddrInfo(addrInfo1.get(), addrInfo2.get());
        EXPECT_TRUE(result);
    });
}

TEST_F(AddrInfoTest, CompareAddrInfoNullPointers) {
    auto addrInfo = createTestAddrInfo();
    if (!addrInfo) {
        GTEST_SKIP() << "Could not create test addrinfo";
    }

    EXPECT_THROW(compareAddrInfo(nullptr, addrInfo.get()), std::invalid_argument);
    EXPECT_THROW(compareAddrInfo(addrInfo.get(), nullptr), std::invalid_argument);
    EXPECT_THROW(compareAddrInfo(nullptr, nullptr), std::invalid_argument);
}

// filterAddrInfo Tests
TEST_F(AddrInfoTest, FilterAddrInfoIPv4) {
    auto addrInfo = createTestAddrInfo();
    if (!addrInfo) {
        GTEST_SKIP() << "Could not create test addrinfo";
    }

    ASSERT_NO_THROW({
        auto filtered = filterAddrInfo(addrInfo.get(), AF_INET);
        // Result might be null if no IPv4 addresses found, which is valid
    });
}

TEST_F(AddrInfoTest, FilterAddrInfoIPv6) {
    auto addrInfo = createTestAddrInfo();
    if (!addrInfo) {
        GTEST_SKIP() << "Could not create test addrinfo";
    }

    ASSERT_NO_THROW({
        auto filtered = filterAddrInfo(addrInfo.get(), AF_INET6);
        // Result might be null if no IPv6 addresses found, which is valid
    });
}

TEST_F(AddrInfoTest, FilterAddrInfoNullPointer) {
    EXPECT_THROW(filterAddrInfo(nullptr, AF_INET), std::invalid_argument);
}

// sortAddrInfo Tests
TEST_F(AddrInfoTest, SortAddrInfoValid) {
    auto addrInfo = createTestAddrInfo();
    if (!addrInfo) {
        GTEST_SKIP() << "Could not create test addrinfo";
    }

    ASSERT_NO_THROW({
        auto sorted = sortAddrInfo(addrInfo.get());
        EXPECT_NE(sorted, nullptr);
    });
}

TEST_F(AddrInfoTest, SortAddrInfoNullPointer) {
    EXPECT_THROW(sortAddrInfo(nullptr), std::invalid_argument);
}

// Edge Cases and Error Handling Tests
TEST_F(AddrInfoTest, GetAddrInfoWithDifferentServices) {
    std::vector<std::string> services = {"80", "443", "22", "21", "25", "53"};

    for (const auto& service : services) {
        ASSERT_NO_THROW({
            auto addrInfo = getAddrInfo("127.0.0.1", service);
            EXPECT_NE(addrInfo, nullptr);
        }) << "Failed for service: " << service;
    }
}

TEST_F(AddrInfoTest, GetAddrInfoWithNamedServices) {
    std::vector<std::string> services = {"http", "https", "ssh", "ftp", "smtp", "dns"};

    for (const auto& service : services) {
        ASSERT_NO_THROW({
            auto addrInfo = getAddrInfo("127.0.0.1", service);
            EXPECT_NE(addrInfo, nullptr);
        }) << "Failed for service: " << service;
    }
}

TEST_F(AddrInfoTest, AddrInfoToStringMultipleEntries) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    try {
        auto addrInfo = getAddrInfo("google.com", "80");
        if (!addrInfo) {
            GTEST_SKIP() << "Could not resolve google.com";
        }

        // Test text format
        std::string textResult = addrInfoToString(addrInfo.get(), false);
        EXPECT_FALSE(textResult.empty());

        // Should contain multiple entries
        size_t count = 0;
        size_t pos = 0;
        while ((pos = textResult.find("addrinfo[", pos)) != std::string::npos) {
            count++;
            pos++;
        }
        EXPECT_GT(count, 0);

        // Test JSON format
        std::string jsonResult = addrInfoToString(addrInfo.get(), true);
        EXPECT_FALSE(jsonResult.empty());
        EXPECT_THAT(jsonResult, ::testing::StartsWith("["));
        EXPECT_THAT(jsonResult, ::testing::EndsWith("]"));

    } catch (const std::exception& e) {
        GTEST_SKIP() << "Network resolution failed: " << e.what();
    }
}

TEST_F(AddrInfoTest, DumpAddrInfoPreservesData) {
    auto sourceAddrInfo = createTestAddrInfo();
    if (!sourceAddrInfo) {
        GTEST_SKIP() << "Could not create test addrinfo";
    }

    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> dstAddrInfo(nullptr, ::freeaddrinfo);

    int result = dumpAddrInfo(dstAddrInfo, sourceAddrInfo.get());
    EXPECT_EQ(result, 0);
    EXPECT_NE(dstAddrInfo, nullptr);

    // Compare the original and dumped data
    EXPECT_TRUE(compareAddrInfo(sourceAddrInfo.get(), dstAddrInfo.get()));
}

TEST_F(AddrInfoTest, FilterAddrInfoPreservesCorrectFamily) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    try {
        auto addrInfo = getAddrInfo("google.com", "80");
        if (!addrInfo) {
            GTEST_SKIP() << "Could not resolve google.com";
        }

        // Filter for IPv4
        auto ipv4Filtered = filterAddrInfo(addrInfo.get(), AF_INET);
        if (ipv4Filtered) {
            for (const struct addrinfo* current = ipv4Filtered.get();
                 current != nullptr; current = current->ai_next) {
                EXPECT_EQ(current->ai_family, AF_INET);
            }
        }

        // Filter for IPv6
        auto ipv6Filtered = filterAddrInfo(addrInfo.get(), AF_INET6);
        if (ipv6Filtered) {
            for (const struct addrinfo* current = ipv6Filtered.get();
                 current != nullptr; current = current->ai_next) {
                EXPECT_EQ(current->ai_family, AF_INET6);
            }
        }

    } catch (const std::exception& e) {
        GTEST_SKIP() << "Network resolution failed: " << e.what();
    }
}

TEST_F(AddrInfoTest, SortAddrInfoOrdersByFamily) {
    if (!hasInternetConnectivity()) {
        GTEST_SKIP() << "No internet connectivity available";
    }

    try {
        auto addrInfo = getAddrInfo("google.com", "80");
        if (!addrInfo) {
            GTEST_SKIP() << "Could not resolve google.com";
        }

        auto sorted = sortAddrInfo(addrInfo.get());
        EXPECT_NE(sorted, nullptr);

        // Verify sorting by family
        int lastFamily = -1;
        for (const struct addrinfo* current = sorted.get();
             current != nullptr; current = current->ai_next) {
            if (lastFamily != -1) {
                EXPECT_GE(current->ai_family, lastFamily);
            }
            lastFamily = current->ai_family;
        }

    } catch (const std::exception& e) {
        GTEST_SKIP() << "Network resolution failed: " << e.what();
    }
}

// Performance and Stress Tests
TEST_F(AddrInfoTest, ConcurrentGetAddrInfo) {
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
                auto addrInfo = getAddrInfo("google.com", "80");
                if (addrInfo) {
                    successCount++;
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

TEST_F(AddrInfoTest, ConcurrentDumpAddrInfo) {
    auto sourceAddrInfo = createTestAddrInfo();
    if (!sourceAddrInfo) {
        GTEST_SKIP() << "Could not create test addrinfo";
    }

    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::vector<std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>> results;
    std::vector<int> returnCodes(numThreads);

    // Initialize results vector
    for (int i = 0; i < numThreads; ++i) {
        results.emplace_back(nullptr, ::freeaddrinfo);
    }

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            returnCodes[i] = dumpAddrInfo(results[i], sourceAddrInfo.get());
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (int i = 0; i < numThreads; ++i) {
        EXPECT_EQ(returnCodes[i], 0) << "Thread " << i << " failed";
        EXPECT_NE(results[i], nullptr) << "Thread " << i << " result is null";
    }
}

// Memory Management Tests
TEST_F(AddrInfoTest, MemoryLeakPrevention) {
    // Test that multiple operations don't cause memory leaks
    for (int i = 0; i < 100; ++i) {
        auto addrInfo = createTestAddrInfo();
        if (addrInfo) {
            // Perform various operations
            std::string textStr = addrInfoToString(addrInfo.get(), false);
            std::string jsonStr = addrInfoToString(addrInfo.get(), true);

            auto filtered = filterAddrInfo(addrInfo.get(), AF_INET);
            auto sorted = sortAddrInfo(addrInfo.get());

            std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> dumped(nullptr, ::freeaddrinfo);
            dumpAddrInfo(dumped, addrInfo.get());
        }
    }
    // If we reach here without crashes, memory management is likely correct
    SUCCEED();
}

// Platform-Specific Tests
TEST_F(AddrInfoTest, PlatformSpecificBehavior) {
#ifdef _WIN32
    // Windows-specific tests
    EXPECT_NO_THROW({
        auto addrInfo = getAddrInfo("localhost", "80");
        EXPECT_NE(addrInfo, nullptr);
    });
#else
    // Unix-like systems tests
    EXPECT_NO_THROW({
        auto addrInfo = getAddrInfo("localhost", "80");
        EXPECT_NE(addrInfo, nullptr);
    });
#endif
}

#endif  // TEST_ADDR_INFO_HPP
