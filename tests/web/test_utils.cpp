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
