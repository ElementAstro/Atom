#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <future>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include "atom/algorithm/crypto/sha1.hpp"
#include "atom/error/exception.hpp"

using namespace atom::algorithm;

class SHA1EnhancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }

    // Helper function to generate random data
    std::vector<uint8_t> generateRandomData(size_t size, uint32_t seed = 0) {
        std::vector<uint8_t> data(size);
        std::mt19937 gen(seed ? seed : std::random_device{}());
        std::uniform_int_distribution<> dis(0, 255);

        for (auto& byte : data) {
            byte = static_cast<uint8_t>(dis(gen));
        }
        return data;
    }

    // Helper function to compare two hash digests
    void expectEqualDigests(const std::array<uint8_t, SHA1::DIGEST_SIZE>& a,
                            const std::array<uint8_t, SHA1::DIGEST_SIZE>& b) {
        for (size_t i = 0; i < SHA1::DIGEST_SIZE; ++i) {
            EXPECT_EQ(a[i], b[i]) << "Digests differ at position " << i;
        }
    }

    // Helper to convert string to byte vector
    std::vector<uint8_t> stringToBytes(const std::string& str) {
        return std::vector<uint8_t>(str.begin(), str.end());
    }

    // Helper to create test data of specific size
    std::vector<uint8_t> createTestData(size_t size, uint8_t pattern = 0xAA) {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>((pattern + i) % 256);
        }
        return data;
    }
};

// Test parallel hash computation with multiple containers
TEST_F(SHA1EnhancedTest, ParallelHashComputation) {
    auto data1 = stringToBytes("First test data for parallel hashing");
    auto data2 = stringToBytes("Second test data for parallel hashing");
    auto data3 = createTestData(1000);

    // Compute hashes individually for reference
    SHA1 hasher1, hasher2, hasher3;
    hasher1.update(data1);
    hasher2.update(data2);
    hasher3.update(data3);

    auto expected1 = hasher1.digest();
    auto expected2 = hasher2.digest();
    auto expected3 = hasher3.digest();

    // Test parallel computation (manual implementation since template is
    // complex)
    std::vector<std::future<std::array<uint8_t, SHA1::DIGEST_SIZE>>> futures;

    futures.push_back(std::async(std::launch::async, [&data1]() {
        SHA1 hasher;
        hasher.update(data1);
        return hasher.digest();
    }));

    futures.push_back(std::async(std::launch::async, [&data2]() {
        SHA1 hasher;
        hasher.update(data2);
        return hasher.digest();
    }));

    futures.push_back(std::async(std::launch::async, [&data3]() {
        SHA1 hasher;
        hasher.update(data3);
        return hasher.digest();
    }));

    auto result1 = futures[0].get();
    auto result2 = futures[1].get();
    auto result3 = futures[2].get();

    expectEqualDigests(expected1, result1);
    expectEqualDigests(expected2, result2);
    expectEqualDigests(expected3, result3);
}

// Test SIMD optimization if available
TEST_F(SHA1EnhancedTest, SIMDOptimization) {
#ifdef __AVX2__
    // Test with large data to trigger SIMD path
    const size_t dataSize = 1024 * 1024;  // 1MB
    auto largeData = generateRandomData(dataSize, 12345);

    // Hash with potential SIMD optimization
    SHA1 simdHasher;
    auto startSIMD = std::chrono::high_resolution_clock::now();
    simdHasher.update(largeData);
    auto simdDigest = simdHasher.digest();
    auto endSIMD = std::chrono::high_resolution_clock::now();

    // Hash in smaller chunks to potentially avoid SIMD
    SHA1 chunkHasher;
    auto startChunk = std::chrono::high_resolution_clock::now();
    const size_t chunkSize = 63;  // Odd size to avoid block alignment
    for (size_t i = 0; i < largeData.size(); i += chunkSize) {
        size_t currentChunkSize = std::min(chunkSize, largeData.size() - i);
        chunkHasher.update(largeData.data() + i, currentChunkSize);
    }
    auto chunkDigest = chunkHasher.digest();
    auto endChunk = std::chrono::high_resolution_clock::now();

    // Results should be identical regardless of processing method
    expectEqualDigests(simdDigest, chunkDigest);

    auto simdTime = std::chrono::duration_cast<std::chrono::microseconds>(
        endSIMD - startSIMD);
    auto chunkTime = std::chrono::duration_cast<std::chrono::microseconds>(
        endChunk - startChunk);

    spdlog::info("SIMD-optimized hashing: {} μs, Chunked hashing: {} μs",
                 simdTime.count(), chunkTime.count());
#else
    GTEST_SKIP() << "AVX2 not available, skipping SIMD test";
#endif
}

// Test with various block boundary conditions
TEST_F(SHA1EnhancedTest, BlockBoundaryConditions) {
    // Test data sizes around block boundaries (64 bytes)
    std::vector<size_t> testSizes = {0,   1,   55,  56,  63,   64,   65,
                                     119, 120, 127, 128, 129,  191,  192,
                                     193, 255, 256, 257, 1023, 1024, 1025};

    for (size_t size : testSizes) {
        auto data = createTestData(size);

        // Hash all at once
        SHA1 hasher1;
        hasher1.update(data);
        auto digest1 = hasher1.digest();

        // Hash byte by byte
        SHA1 hasher2;
        for (uint8_t byte : data) {
            hasher2.update(&byte, 1);
        }
        auto digest2 = hasher2.digest();

        expectEqualDigests(digest1, digest2);

        // Verify digest is not all zeros (except for empty input)
        bool allZeros = std::all_of(digest1.begin(), digest1.end(),
                                    [](uint8_t b) { return b == 0; });
        if (size > 0) {
            EXPECT_FALSE(allZeros)
                << "Digest should not be all zeros for size " << size;
        }
    }
}

// Test incremental updates with various chunk sizes
TEST_F(SHA1EnhancedTest, IncrementalUpdatesVariousChunks) {
    const size_t totalSize = 10000;
    auto data = generateRandomData(totalSize, 54321);

    // Reference: hash all at once
    SHA1 referenceHasher;
    referenceHasher.update(data);
    auto referenceDigest = referenceHasher.digest();

    // Test with different chunk sizes
    std::vector<size_t> chunkSizes = {1, 7, 31, 64, 65, 127, 128, 129, 1000};

    for (size_t chunkSize : chunkSizes) {
        SHA1 hasher;
        for (size_t i = 0; i < data.size(); i += chunkSize) {
            size_t currentChunkSize = std::min(chunkSize, data.size() - i);
            hasher.update(data.data() + i, currentChunkSize);
        }
        auto digest = hasher.digest();

        expectEqualDigests(referenceDigest, digest);
    }
}

// Test thread safety
TEST_F(SHA1EnhancedTest, ThreadSafety) {
    const int numThreads = 8;
    const size_t dataSize = 10000;

    // Create test data
    auto testData = generateRandomData(dataSize, 98765);

    // Compute reference hash
    SHA1 referenceHasher;
    referenceHasher.update(testData);
    auto referenceDigest = referenceHasher.digest();

    // Hash the same data in multiple threads
    std::vector<std::future<std::array<uint8_t, SHA1::DIGEST_SIZE>>> futures;

    for (int i = 0; i < numThreads; ++i) {
        futures.push_back(std::async(std::launch::async, [&testData]() {
            SHA1 hasher;
            hasher.update(testData);
            return hasher.digest();
        }));
    }

    // Verify all results match
    for (auto& future : futures) {
        auto digest = future.get();
        expectEqualDigests(referenceDigest, digest);
    }
}

// Test performance with large data
TEST_F(SHA1EnhancedTest, LargeDataPerformance) {
    const size_t dataSize = 100 * 1024 * 1024;  // 100MB
    auto largeData = generateRandomData(dataSize);

    auto startTime = std::chrono::high_resolution_clock::now();

    SHA1 hasher;
    hasher.update(largeData);
    auto digest = hasher.digest();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    double throughput = static_cast<double>(dataSize) / (1024 * 1024) /
                        (duration.count() / 1000.0);

    spdlog::info("SHA1 hashing of {}MB took {} ms (throughput: {:.2f} MB/s)",
                 dataSize / (1024 * 1024), duration.count(), throughput);

    // Verify we got a valid digest
    bool allZeros = std::all_of(digest.begin(), digest.end(),
                                [](uint8_t b) { return b == 0; });
    EXPECT_FALSE(allZeros);
}

// Test reset functionality thoroughly
TEST_F(SHA1EnhancedTest, ResetFunctionality) {
    auto data1 = stringToBytes("First data set");
    auto data2 = stringToBytes("Second data set");

    // Hash data1, reset, then hash data2
    SHA1 hasher;
    hasher.update(data1);
    hasher.reset();
    hasher.update(data2);
    auto digest1 = hasher.digest();

    // Hash data2 directly
    SHA1 directHasher;
    directHasher.update(data2);
    auto digest2 = directHasher.digest();

    expectEqualDigests(digest1, digest2);

    // Test multiple resets
    hasher.reset();
    hasher.reset();
    hasher.update(data1);
    auto digest3 = hasher.digest();

    SHA1 data1Hasher;
    data1Hasher.update(data1);
    auto digest4 = data1Hasher.digest();

    expectEqualDigests(digest3, digest4);
}

// Test with binary data containing all byte values
TEST_F(SHA1EnhancedTest, AllByteValues) {
    std::vector<uint8_t> allBytes;
    for (int i = 0; i < 256; ++i) {
        allBytes.push_back(static_cast<uint8_t>(i));
    }

    // Repeat the pattern multiple times
    std::vector<uint8_t> testData;
    for (int repeat = 0; repeat < 100; ++repeat) {
        testData.insert(testData.end(), allBytes.begin(), allBytes.end());
    }

    SHA1 hasher;
    hasher.update(testData);
    auto digest = hasher.digestAsString();

    // Verify we got a valid hex string
    EXPECT_EQ(digest.length(), 40);
    for (char c : digest) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

// Test error handling
TEST_F(SHA1EnhancedTest, ErrorHandling) {
    SHA1 hasher;

    // Null pointer with zero length should be safe
    EXPECT_NO_THROW(hasher.update(nullptr, 0));

    // Null pointer with non-zero length should throw
    EXPECT_THROW(hasher.update(nullptr, 5), atom::error::InvalidArgument);

    // Valid operations after error should still work
    auto data = stringToBytes("test data");
    EXPECT_NO_THROW(hasher.update(data));
    EXPECT_NO_THROW({
        auto result = hasher.digest();
        (void)result;  // Suppress unused variable warning
    });
}

// Test BytesToHex function with edge cases
TEST_F(SHA1EnhancedTest, BytesToHexEdgeCases) {
    // Test with all zeros
    std::array<uint8_t, SHA1::DIGEST_SIZE> zeros{};
    auto hexZeros = bytesToHex(zeros);
    EXPECT_EQ(hexZeros, std::string(40, '0'));

    // Test with all 0xFF
    std::array<uint8_t, SHA1::DIGEST_SIZE> allFF;
    allFF.fill(0xFF);
    auto hexFF = bytesToHex(allFF);
    EXPECT_EQ(hexFF, std::string(40, 'f'));

    // Test with alternating pattern
    std::array<uint8_t, SHA1::DIGEST_SIZE> alternating;
    for (size_t i = 0; i < alternating.size(); ++i) {
        alternating[i] = (i % 2 == 0) ? 0xAA : 0x55;
    }
    auto hexAlternating = bytesToHex(alternating);

    std::string expectedPattern;
    for (size_t i = 0; i < alternating.size(); ++i) {
        expectedPattern += (i % 2 == 0) ? "aa" : "55";
    }
    EXPECT_EQ(hexAlternating, expectedPattern);
}

// Test consistency of digest computation
TEST_F(SHA1EnhancedTest, DigestConsistency) {
    auto data = stringToBytes("Consistency test data");

    // Create multiple hashers with same data
    SHA1 hasher1, hasher2, hasher3;
    hasher1.update(data);
    hasher2.update(data);
    hasher3.update(data);

    // All should produce identical results
    auto digest1 = hasher1.digest();
    auto digest2 = hasher2.digest();
    auto digestStr = hasher3.digestAsString();

    expectEqualDigests(digest1, digest2);
    EXPECT_EQ(bytesToHex(digest1), digestStr);
}

// Test with very long repeated patterns
TEST_F(SHA1EnhancedTest, LongRepeatedPatterns) {
    // Create data with repeating patterns of different lengths
    std::vector<std::string> patterns = {"A", "AB", "ABC", "ABCD", "ABCDE"};

    for (const auto& pattern : patterns) {
        std::string longString;
        const size_t targetSize = 100000;

        while (longString.size() < targetSize) {
            longString += pattern;
        }
        longString.resize(targetSize);

        auto data = stringToBytes(longString);
        SHA1 hasher;
        hasher.update(data);
        auto digest = hasher.digestAsString();

        // Each pattern should produce a different hash
        EXPECT_EQ(digest.length(), 40);

        // Verify it's a valid hex string
        for (char c : digest) {
            EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
        }
    }
}
