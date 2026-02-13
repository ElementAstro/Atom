#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <future>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include "atom/algorithm/crypto/tea.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

class TEAEnhancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }

        // Strong test keys
        strongKey1 = {0x12345678, 0x9ABCDEF0, 0xFEDCBA98, 0x76543210};
        strongKey2 = {0x11111111, 0x22222222, 0x33333333, 0x44444444};
        strongKey3 = {0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD};

        // Weak keys for testing
        zeroKey = {0, 0, 0, 0};
        oneKey = {1, 1, 1, 1};
        maxKey = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    }

    std::array<uint32_t, 4> strongKey1, strongKey2, strongKey3;
    std::array<uint32_t, 4> zeroKey, oneKey, maxKey;

    // Helper to generate random data
    std::vector<uint32_t> generateRandomU32Data(size_t count,
                                                uint32_t seed = 0) {
        std::vector<uint32_t> data(count);
        std::mt19937 gen(seed ? seed : std::random_device{}());
        std::uniform_int_distribution<uint32_t> dist;

        std::generate(data.begin(), data.end(), [&]() { return dist(gen); });
        return data;
    }

    // Helper to generate random byte data
    std::vector<uint8_t> generateRandomByteData(size_t size,
                                                uint32_t seed = 0) {
        std::vector<uint8_t> data(size);
        std::mt19937 gen(seed ? seed : std::random_device{}());
        std::uniform_int_distribution<> dist(0, 255);

        std::generate(data.begin(), data.end(),
                      [&]() { return static_cast<uint8_t>(dist(gen)); });
        return data;
    }

    // Helper to compare vectors
    void expectEqualVectors(const std::vector<uint32_t>& a,
                            const std::vector<uint32_t>& b) {
        ASSERT_EQ(a.size(), b.size());
        for (size_t i = 0; i < a.size(); ++i) {
            EXPECT_EQ(a[i], b[i]) << "Vectors differ at index " << i;
        }
    }

    // Helper to verify encryption changes data
    void verifyEncryptionChangesData(const std::vector<uint32_t>& original,
                                     const std::vector<uint32_t>& encrypted) {
        ASSERT_EQ(original.size(), encrypted.size());
        bool hasChanges = false;
        for (size_t i = 0; i < original.size(); ++i) {
            if (original[i] != encrypted[i]) {
                hasChanges = true;
                break;
            }
        }
        EXPECT_TRUE(hasChanges) << "Encryption should change the data";
    }
};

// Test container-based XXTEA encryption/decryption
TEST_F(TEAEnhancedTest, XXTEAContainerAPIs) {
    std::vector<uint32_t> data = {0x12345678, 0x9ABCDEF0, 0xFEDCBA98,
                                  0x76543210};

    // Test container-based encryption
    auto encrypted = xxteaEncrypt(data, strongKey1);
    verifyEncryptionChangesData(data, encrypted);

    // Test container-based decryption
    auto decrypted = xxteaDecrypt(encrypted, strongKey1);
    expectEqualVectors(data, decrypted);
}

// Test parallel XXTEA with different data sizes (ECB mode - self-consistency
// only)
TEST_F(TEAEnhancedTest, XXTEAParallelDifferentSizes) {
    std::vector<size_t> dataSizes = {100, 1000, 5000};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    for (size_t size : dataSizes) {
        auto data = generateRandomU32Data(size, 12345);

        // Verify parallel encrypt/decrypt self-consistency
        auto encryptedPar = xxteaEncryptParallel(data, strongKey1, 4);
        EXPECT_NE(encryptedPar, data);  // Data should be encrypted

        auto decryptedPar = xxteaDecryptParallel(encryptedPar, strongKey1, 4);
        expectEqualVectors(data, decryptedPar);

        // Verify sequential encrypt/decrypt self-consistency
        auto encryptedSeq = xxteaEncrypt(data, strongKey1);
        auto decryptedSeq = xxteaDecrypt(encryptedSeq, strongKey1);
        expectEqualVectors(data, decryptedSeq);
    }
#pragma GCC diagnostic pop
}

// Test parallel performance scaling
TEST_F(TEAEnhancedTest, ParallelPerformanceScaling) {
    const size_t dataSize = 100000;
    auto data = generateRandomU32Data(dataSize);

    std::vector<size_t> threadCounts = {1, 2, 4, 8};
    std::vector<double> encryptionTimes;

    for (size_t threads : threadCounts) {
        auto start = std::chrono::high_resolution_clock::now();
        auto encrypted = xxteaEncryptParallel(data, strongKey1, threads);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        encryptionTimes.push_back(duration.count());

        spdlog::info("XXTEA encryption with {} threads took {} ms", threads,
                     duration.count());

        // Verify correctness
        auto decrypted = xxteaDecryptParallel(encrypted, strongKey1, threads);
        expectEqualVectors(data, decrypted);
    }

    // Performance should generally improve with more threads (up to a point)
    EXPECT_GT(encryptionTimes[0],
              encryptionTimes.back() * 0.5);  // At least some improvement
}

// Test byte conversion with various sizes
TEST_F(TEAEnhancedTest, ByteConversionVariousSizes) {
    std::vector<size_t> byteSizes = {0,  1,  3,  4,    5,    7,    8,
                                     15, 16, 17, 1000, 1001, 1003, 1004};

    for (size_t size : byteSizes) {
        auto bytes = generateRandomByteData(size, size);

        // Convert to uint32_t and back
        auto uint32Vec = toUint32Vector(bytes);
        auto bytesResult = toByteArray(uint32Vec);

        // Original bytes should be preserved (with possible padding)
        EXPECT_GE(bytesResult.size(), bytes.size());
        EXPECT_EQ(bytesResult.size() % 4, 0);  // Should be multiple of 4

        for (size_t i = 0; i < bytes.size(); ++i) {
            EXPECT_EQ(bytesResult[i], bytes[i])
                << "Byte mismatch at index " << i;
        }

        // Padding bytes should be zero
        for (size_t i = bytes.size(); i < bytesResult.size(); ++i) {
            EXPECT_EQ(bytesResult[i], 0)
                << "Padding byte should be zero at index " << i;
        }
    }
}

// Test security: different keys produce different results
TEST_F(TEAEnhancedTest, SecurityDifferentKeys) {
    std::vector<uint32_t> data = {0x12345678, 0x9ABCDEF0, 0xFEDCBA98,
                                  0x76543210};

    auto encrypted1 = xxteaEncrypt(data, strongKey1);
    auto encrypted2 = xxteaEncrypt(data, strongKey2);
    auto encrypted3 = xxteaEncrypt(data, strongKey3);

    // All encrypted results should be different
    EXPECT_NE(encrypted1, encrypted2);
    EXPECT_NE(encrypted1, encrypted3);
    EXPECT_NE(encrypted2, encrypted3);

    // Each should decrypt correctly with its own key
    expectEqualVectors(data, xxteaDecrypt(encrypted1, strongKey1));
    expectEqualVectors(data, xxteaDecrypt(encrypted2, strongKey2));
    expectEqualVectors(data, xxteaDecrypt(encrypted3, strongKey3));

    // Wrong key should produce garbage
    auto wrongDecrypt = xxteaDecrypt(encrypted1, strongKey2);
    EXPECT_NE(wrongDecrypt, data);
}

// Test security: small changes in data produce different results
TEST_F(TEAEnhancedTest, SecurityAvalancheEffect) {
    std::vector<uint32_t> data1 = {0x12345678, 0x9ABCDEF0};
    std::vector<uint32_t> data2 = {0x12345679,
                                   0x9ABCDEF0};  // One bit different

    auto encrypted1 = xxteaEncrypt(data1, strongKey1);
    auto encrypted2 = xxteaEncrypt(data2, strongKey1);

    // Results should be significantly different (avalanche effect)
    EXPECT_NE(encrypted1, encrypted2);

    // Count different bits
    uint32_t diffBits = 0;
    for (size_t i = 0; i < encrypted1.size(); ++i) {
        uint32_t xorResult = encrypted1[i] ^ encrypted2[i];
        diffBits += __builtin_popcount(xorResult);
    }

    // Should have significant bit differences (avalanche effect)
    EXPECT_GT(diffBits,
              encrypted1.size() * 32 / 4);  // At least 25% bits different
}

// Test edge cases for TEA/XTEA
TEST_F(TEAEnhancedTest, TEAXTEAEdgeCases) {
    // Test with extreme values
    std::vector<std::pair<uint32_t, uint32_t>> testValues = {
        {0, 0},
        {UINT32_MAX, UINT32_MAX},
        {0, UINT32_MAX},
        {UINT32_MAX, 0},
        {0x80000000, 0x80000000}};

    for (auto [v0, v1] : testValues) {
        uint32_t original_v0 = v0, original_v1 = v1;

        // Test TEA
        teaEncrypt(v0, v1, strongKey1);
        EXPECT_TRUE(v0 != original_v0 || v1 != original_v1);  // Should change
        teaDecrypt(v0, v1, strongKey1);
        EXPECT_EQ(v0, original_v0);
        EXPECT_EQ(v1, original_v1);

        // Test XTEA
        v0 = original_v0;
        v1 = original_v1;
        xteaEncrypt(v0, v1, strongKey1);
        EXPECT_TRUE(v0 != original_v0 || v1 != original_v1);  // Should change
        xteaDecrypt(v0, v1, strongKey1);
        EXPECT_EQ(v0, original_v0);
        EXPECT_EQ(v1, original_v1);
    }
}

// Test weak key detection
TEST_F(TEAEnhancedTest, WeakKeyDetection) {
    uint32_t v0 = 0x12345678, v1 = 0x9ABCDEF0;

    // Zero key should be rejected
    EXPECT_THROW(teaEncrypt(v0, v1, zeroKey), TEAException);
    EXPECT_THROW(teaDecrypt(v0, v1, zeroKey), TEAException);
    EXPECT_THROW(xteaEncrypt(v0, v1, zeroKey), TEAException);
    EXPECT_THROW(xteaDecrypt(v0, v1, zeroKey), TEAException);
}

// Test thread safety with concurrent operations
TEST_F(TEAEnhancedTest, ThreadSafetyConcurrentOperations) {
    const int numThreads = 8;
    const size_t dataSize = 1000;
    auto testData = generateRandomU32Data(dataSize);

    // Test concurrent encryption with same data and key
    std::vector<std::future<std::vector<uint32_t>>> encryptFutures;
    for (int i = 0; i < numThreads; ++i) {
        encryptFutures.push_back(
            std::async(std::launch::async, [&testData, this]() {
                return xxteaEncrypt(testData, strongKey1);
            }));
    }

    // Collect results
    std::vector<std::vector<uint32_t>> encryptResults;
    for (auto& future : encryptFutures) {
        encryptResults.push_back(future.get());
    }

    // All encryption results should be identical
    for (size_t i = 1; i < encryptResults.size(); ++i) {
        expectEqualVectors(encryptResults[0], encryptResults[i]);
    }

    // Test concurrent decryption
    auto encrypted = encryptResults[0];
    std::vector<std::future<std::vector<uint32_t>>> decryptFutures;
    for (int i = 0; i < numThreads; ++i) {
        decryptFutures.push_back(
            std::async(std::launch::async, [&encrypted, this]() {
                return xxteaDecrypt(encrypted, strongKey1);
            }));
    }

    // All decryption results should match original data
    for (auto& future : decryptFutures) {
        auto decrypted = future.get();
        expectEqualVectors(testData, decrypted);
    }
}

// Test large data encryption/decryption
TEST_F(TEAEnhancedTest, LargeDataEncryption) {
    const size_t largeSize = 1000000;  // 1M uint32_t values
    auto largeData = generateRandomU32Data(largeSize);

    auto startTime = std::chrono::high_resolution_clock::now();
    auto encrypted = xxteaEncryptParallel(largeData, strongKey1, 8);
    auto midTime = std::chrono::high_resolution_clock::now();
    auto decrypted = xxteaDecryptParallel(encrypted, strongKey1, 8);
    auto endTime = std::chrono::high_resolution_clock::now();

    auto encryptTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        midTime - startTime);
    auto decryptTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - midTime);

    spdlog::info("Large data encryption: {} ms, decryption: {} ms",
                 encryptTime.count(), decryptTime.count());

    expectEqualVectors(largeData, decrypted);
}

// Test end-to-end encryption with real-world data patterns
TEST_F(TEAEnhancedTest, RealWorldDataPatterns) {
    // Test with different data patterns that might occur in real applications

    // Pattern 1: Mostly zeros with some data
    std::vector<uint32_t> sparseData(1000, 0);
    for (size_t i = 0; i < 10; ++i) {
        sparseData[i * 100] = 0x12345678 + i;
    }

    // Pattern 2: Incremental data
    std::vector<uint32_t> incrementalData(1000);
    std::iota(incrementalData.begin(), incrementalData.end(), 0);

    // Pattern 3: Repeating pattern
    std::vector<uint32_t> repeatingData;
    std::vector<uint32_t> pattern = {0x11111111, 0x22222222, 0x33333333,
                                     0x44444444};
    for (int i = 0; i < 250; ++i) {
        repeatingData.insert(repeatingData.end(), pattern.begin(),
                             pattern.end());
    }

    std::vector<std::vector<uint32_t>> testPatterns = {
        sparseData, incrementalData, repeatingData};

    for (size_t i = 0; i < testPatterns.size(); ++i) {
        auto& data = testPatterns[i];

        auto encrypted = xxteaEncrypt(data, strongKey1);
        verifyEncryptionChangesData(data, encrypted);

        auto decrypted = xxteaDecrypt(encrypted, strongKey1);
        expectEqualVectors(data, decrypted);

        spdlog::info("Pattern {} encryption/decryption successful", i + 1);
    }
}

// Test memory efficiency with repeated operations
TEST_F(TEAEnhancedTest, MemoryEfficiency) {
    const size_t dataSize = 10000;
    auto testData = generateRandomU32Data(dataSize);

    // Perform many encryption/decryption cycles
    for (int cycle = 0; cycle < 100; ++cycle) {
        auto encrypted = xxteaEncrypt(testData, strongKey1);
        auto decrypted = xxteaDecrypt(encrypted, strongKey1);

        expectEqualVectors(testData, decrypted);

        // Occasionally verify with different keys
        if (cycle % 10 == 0) {
            auto encrypted2 = xxteaEncrypt(testData, strongKey2);
            EXPECT_NE(encrypted, encrypted2);

            auto decrypted2 = xxteaDecrypt(encrypted2, strongKey2);
            expectEqualVectors(testData, decrypted2);
        }
    }

    // If we reach here without memory issues, the test passes
    SUCCEED();
}

// Test exception safety
TEST_F(TEAEnhancedTest, ExceptionSafety) {
    // Test that operations are exception-safe
    std::vector<uint32_t> data = {0x12345678, 0x9ABCDEF0};

    // These should not throw with valid inputs
    EXPECT_NO_THROW({
        auto encrypted = xxteaEncrypt(data, strongKey1);
        auto decrypted = xxteaDecrypt(encrypted, strongKey1);
    });

    // Test with empty data (should throw)
    std::vector<uint32_t> emptyData;
    EXPECT_THROW(xxteaEncrypt(emptyData, strongKey1), TEAException);
    EXPECT_THROW(xxteaEncryptParallel(emptyData, strongKey1), TEAException);
}
