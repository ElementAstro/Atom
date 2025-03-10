#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <future>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "atom/algorithm/tea.hpp"
#include "atom/log/loguru.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

// Helper function to generate random data
std::vector<uint8_t> generateRandomBytes(size_t size) {
    std::vector<uint8_t> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 255);

    std::generate(data.begin(), data.end(),
                  [&]() { return static_cast<uint8_t>(dist(gen)); });
    return data;
}

// Helper function to generate a random key
std::array<uint32_t, 4> generateRandomKey() {
    std::array<uint32_t, 4> key;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(
        1, UINT32_MAX);  // Avoid all zeros

    for (auto& k : key) {
        k = dist(gen);
    }
    return key;
}

// Test fixture for TEA tests
class TEATest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing
        static bool initialized = false;
        if (!initialized) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            initialized = true;
        }

        // Default test key - non-zero values
        defaultKey = {0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210};

        // All zeros key (invalid)
        zeroKey = {0, 0, 0, 0};
    }

    std::array<uint32_t, 4> defaultKey;
    std::array<uint32_t, 4> zeroKey;

    // Helper to compare two vectors of bytes
    void expectEqualVectors(const std::vector<uint32_t>& a,
                            const std::vector<uint32_t>& b) {
        ASSERT_EQ(a.size(), b.size());
        for (size_t i = 0; i < a.size(); ++i) {
            EXPECT_EQ(a[i], b[i]) << "Vectors differ at index " << i;
        }
    }
};

// Basic TEA encryption/decryption tests
TEST_F(TEATest, BasicTEAEncryptDecrypt) {
    uint32_t v0 = 0x12345678;
    uint32_t v1 = 0x9ABCDEF0;

    // Make copies for later comparison
    uint32_t original_v0 = v0;
    uint32_t original_v1 = v1;

    // Encrypt
    teaEncrypt(v0, v1, defaultKey);

    // Ensure values were changed
    EXPECT_NE(v0, original_v0);
    EXPECT_NE(v1, original_v1);

    // Decrypt
    teaDecrypt(v0, v1, defaultKey);

    // Check that we got back the original values
    EXPECT_EQ(v0, original_v0);
    EXPECT_EQ(v1, original_v1);
}

// Test TEA with invalid key
TEST_F(TEATest, TEAWithInvalidKey) {
    uint32_t v0 = 0x12345678;
    uint32_t v1 = 0x9ABCDEF0;

    // Encrypt with zero key should throw
    EXPECT_THROW(teaEncrypt(v0, v1, zeroKey), TEAException);

    // Decrypt with zero key should throw
    EXPECT_THROW(teaDecrypt(v0, v1, zeroKey), TEAException);
}

// Basic XTEA encryption/decryption tests
TEST_F(TEATest, BasicXTEAEncryptDecrypt) {
    uint32_t v0 = 0x12345678;
    uint32_t v1 = 0x9ABCDEF0;

    // Make copies for later comparison
    uint32_t original_v0 = v0;
    uint32_t original_v1 = v1;

    // Encrypt
    xteaEncrypt(v0, v1, defaultKey);

    // Ensure values were changed
    EXPECT_NE(v0, original_v0);
    EXPECT_NE(v1, original_v1);

    // Decrypt
    xteaDecrypt(v0, v1, defaultKey);

    // Check that we got back the original values
    EXPECT_EQ(v0, original_v0);
    EXPECT_EQ(v1, original_v1);
}

// Test XTEA with invalid key
TEST_F(TEATest, XTEAWithInvalidKey) {
    uint32_t v0 = 0x12345678;
    uint32_t v1 = 0x9ABCDEF0;

    // Encrypt with zero key should throw
    EXPECT_THROW(xteaEncrypt(v0, v1, zeroKey), TEAException);

    // Decrypt with zero key should throw
    EXPECT_THROW(xteaDecrypt(v0, v1, zeroKey), TEAException);
}

// Basic XXTEA encryption/decryption tests
TEST_F(TEATest, BasicXXTEAEncryptDecrypt) {
    // Create some test data
    std::vector<uint32_t> data = {0x12345678, 0x9ABCDEF0, 0xFEDCBA98,
                                  0x76543210};

    // Encrypt
    auto encrypted = xxteaEncrypt(data, defaultKey);

    // Ensure data was changed
    EXPECT_NE(encrypted, data);

    // Decrypt
    auto decrypted = xxteaDecrypt(encrypted, defaultKey);

    // Check that we got back the original data
    expectEqualVectors(decrypted, data);
}

// Test XXTEA with different data sizes
TEST_F(TEATest, XXTEADifferentSizes) {
    // Test with a single element (should return copy)
    std::vector<uint32_t> single = {0x12345678};
    auto encrypted_single = xxteaEncrypt(single, defaultKey);
    expectEqualVectors(encrypted_single, single);

    // Test with empty vector (should throw)
    std::vector<uint32_t> empty;
    EXPECT_THROW(xxteaEncrypt(empty, defaultKey), TEAException);
    EXPECT_THROW(xxteaDecrypt(empty, defaultKey), TEAException);

    // Test with large data
    std::vector<uint32_t> large(100);
    std::iota(large.begin(), large.end(), 0);  // Fill with sequential values

    auto encrypted_large = xxteaEncrypt(large, defaultKey);
    auto decrypted_large = xxteaDecrypt(encrypted_large, defaultKey);

    expectEqualVectors(decrypted_large, large);
}

// Test parallel XXTEA implementation
TEST_F(TEATest, XXTEAParallel) {
    // Create large data for parallel processing
    std::vector<uint32_t> large_data(10000);
    std::iota(large_data.begin(), large_data.end(), 0);

    // Encrypt using both regular and parallel versions
    auto encrypted_regular = xxteaEncrypt(large_data, defaultKey);
    auto encrypted_parallel = xxteaEncryptParallel(large_data, defaultKey);

    // Results should match regardless of implementation
    expectEqualVectors(encrypted_parallel, encrypted_regular);

    // Decrypt using both regular and parallel versions
    auto decrypted_regular = xxteaDecrypt(encrypted_regular, defaultKey);
    auto decrypted_parallel =
        xxteaDecryptParallel(encrypted_parallel, defaultKey);

    // Decrypted results should match the original data
    expectEqualVectors(decrypted_regular, large_data);
    expectEqualVectors(decrypted_parallel, large_data);
}

// Test with custom thread count
TEST_F(TEATest, XXTEACustomThreadCount) {
    std::vector<uint32_t> data(5000);
    std::iota(data.begin(), data.end(), 0);

    // Try with different thread counts
    auto encrypted_2threads = xxteaEncryptParallel(data, defaultKey, 2);
    auto encrypted_4threads = xxteaEncryptParallel(data, defaultKey, 4);

    // Results should match regardless of thread count
    expectEqualVectors(encrypted_2threads, encrypted_4threads);

    // Decrypt with different thread counts
    auto decrypted_2threads =
        xxteaDecryptParallel(encrypted_2threads, defaultKey, 2);
    auto decrypted_4threads =
        xxteaDecryptParallel(encrypted_4threads, defaultKey, 4);

    // Decrypted results should match the original data
    expectEqualVectors(decrypted_2threads, data);
    expectEqualVectors(decrypted_4threads, data);
}

// Test byte conversion functions
TEST_F(TEATest, ByteConversionFunctions) {
    // Create test byte array
    std::vector<uint8_t> bytes = {
        0x01, 0x23, 0x45, 0x67,  // First uint32_t: 0x67452301
        0x89, 0xAB, 0xCD, 0xEF,  // Second uint32_t: 0xEFCDAB89
        0xFE, 0xDC               // Partial third uint32_t: 0x0000DCFE
    };

    // Convert to uint32_t vector
    auto uint32_vec = toUint32Vector(bytes);

    // Check size (should be ceil(10/4) = 3)
    EXPECT_EQ(uint32_vec.size(), 3);

    // Convert back to bytes
    auto bytes_result = toByteArray(uint32_vec);

    // Resulting vector should be a multiple of 4 in size
    EXPECT_EQ(bytes_result.size(), 12);

    // First 10 bytes should match original
    for (size_t i = 0; i < bytes.size(); ++i) {
        EXPECT_EQ(bytes_result[i], bytes[i]) << "Bytes differ at index " << i;
    }

    // Last 2 bytes should be zero (padding)
    EXPECT_EQ(bytes_result[10], 0);
    EXPECT_EQ(bytes_result[11], 0);
}

// Test byte conversion with empty data
TEST_F(TEATest, ByteConversionEmpty) {
    std::vector<uint8_t> empty_bytes;
    auto uint32_vec = toUint32Vector(empty_bytes);

    EXPECT_EQ(uint32_vec.size(), 0);

    std::vector<uint32_t> empty_uint32;
    auto bytes_result = toByteArray(empty_uint32);

    EXPECT_EQ(bytes_result.size(), 0);
}

// Test end-to-end encryption and decryption with byte conversion
TEST_F(TEATest, EndToEndEncryption) {
    // Create test data
    std::string message = "This is a secret message for XXTEA encryption test";
    std::vector<uint8_t> message_bytes(message.begin(), message.end());

    // Convert to uint32_t, encrypt, convert back to bytes
    auto uint32_data = toUint32Vector(message_bytes);
    auto encrypted = xxteaEncrypt(uint32_data, defaultKey);
    auto encrypted_bytes = toByteArray(encrypted);

    // The encrypted bytes should be different from the original
    EXPECT_NE(encrypted_bytes, message_bytes);

    // Convert encrypted bytes back to uint32_t, decrypt, convert to bytes
    auto encrypted_uint32 = toUint32Vector(encrypted_bytes);
    auto decrypted = xxteaDecrypt(encrypted_uint32, defaultKey);
    auto decrypted_bytes = toByteArray(decrypted);

    // Trim any padding zeros
    while (decrypted_bytes.size() > message_bytes.size()) {
        decrypted_bytes.pop_back();
    }

    // Check that decrypted message matches original
    EXPECT_EQ(decrypted_bytes, message_bytes);

    // Convert back to string and check
    std::string decrypted_message(decrypted_bytes.begin(),
                                  decrypted_bytes.end());
    EXPECT_EQ(decrypted_message, message);
}

// Test with different keys
TEST_F(TEATest, DifferentKeys) {
    std::vector<uint32_t> data = {0x12345678, 0x9ABCDEF0};

    auto key1 =
        std::array<uint32_t, 4>{0x11111111, 0x22222222, 0x33333333, 0x44444444};
    auto key2 =
        std::array<uint32_t, 4>{0x55555555, 0x66666666, 0x77777777, 0x88888888};

    // Encrypt with different keys
    auto encrypted1 = xxteaEncrypt(data, key1);
    auto encrypted2 = xxteaEncrypt(data, key2);

    // Results should be different
    EXPECT_NE(encrypted1, encrypted2);

    // Decrypt with correct keys
    auto decrypted1 = xxteaDecrypt(encrypted1, key1);
    auto decrypted2 = xxteaDecrypt(encrypted2, key2);

    // Both should decrypt to original data
    expectEqualVectors(decrypted1, data);
    expectEqualVectors(decrypted2, data);

    // Decrypt with wrong key should produce garbage (but shouldn't crash)
    auto wrong_decrypt = xxteaDecrypt(encrypted1, key2);
    EXPECT_NE(wrong_decrypt, data);
}

// Performance test
TEST_F(TEATest, PerformanceTest) {
    // Create large data for meaningful performance measurement
    std::vector<uint32_t> large_data(100000);
    std::iota(large_data.begin(), large_data.end(), 0);

    // Measure time for regular XXTEA
    auto start = std::chrono::high_resolution_clock::now();
    auto encrypted = xxteaEncrypt(large_data, defaultKey);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    std::cout << "Regular XXTEA encryption of 100,000 integers took "
              << duration << "ms" << std::endl;

    // Measure time for parallel XXTEA
    start = std::chrono::high_resolution_clock::now();
    auto encrypted_parallel = xxteaEncryptParallel(large_data, defaultKey);
    end = std::chrono::high_resolution_clock::now();
    auto duration_parallel =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    std::cout << "Parallel XXTEA encryption of 100,000 integers took "
              << duration_parallel << "ms" << std::endl;

    // On multi-core systems, parallel should be faster
    // This is not a strict test because performance depends on hardware
    std::cout << "Speedup factor: "
              << (duration > 0
                      ? static_cast<double>(duration) / duration_parallel
                      : 0)
              << "x" << std::endl;

    // Verify results match
    expectEqualVectors(encrypted, encrypted_parallel);
}

// Test with large keys (edge of uint32_t range)
TEST_F(TEATest, LargeKeyValues) {
    std::array<uint32_t, 4> large_key = {UINT32_MAX, UINT32_MAX - 1,
                                         UINT32_MAX - 2, UINT32_MAX - 3};

    uint32_t v0 = 0x12345678;
    uint32_t v1 = 0x9ABCDEF0;

    uint32_t original_v0 = v0;
    uint32_t original_v1 = v1;

    // Should not throw or overflow
    EXPECT_NO_THROW({
        teaEncrypt(v0, v1, large_key);
        teaDecrypt(v0, v1, large_key);
    });

    // Should round-trip correctly
    EXPECT_EQ(v0, original_v0);
    EXPECT_EQ(v1, original_v1);

    // Test with XXTEA as well
    std::vector<uint32_t> data = {0x12345678, 0x9ABCDEF0};

    auto encrypted = xxteaEncrypt(data, large_key);
    auto decrypted = xxteaDecrypt(encrypted, large_key);

    expectEqualVectors(decrypted, data);
}

// Test thread safety
TEST_F(TEATest, ThreadSafety) {
    // Shared data and key
    std::vector<uint32_t> data(1000);
    std::iota(data.begin(), data.end(), 0);

    // Encrypt in multiple threads
    std::vector<std::future<std::vector<uint32_t>>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(std::async(std::launch::async, [&data, this]() {
            return xxteaEncrypt(data, defaultKey);
        }));
    }

    // Collect results
    std::vector<std::vector<uint32_t>> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }

    // All results should be identical
    for (size_t i = 1; i < results.size(); ++i) {
        expectEqualVectors(results[0], results[i]);
    }
}

// Test exception handling
TEST_F(TEATest, ExceptionHandling) {
    // Test handling of invalid inputs

    // Empty data with XXTEA
    std::vector<uint32_t> empty_data;
    EXPECT_THROW(xxteaEncrypt(empty_data, defaultKey), TEAException);
    EXPECT_THROW(xxteaEncryptParallel(empty_data, defaultKey), TEAException);

    // Invalid key with TEA
    uint32_t v0 = 0x12345678;
    uint32_t v1 = 0x9ABCDEF0;
    EXPECT_THROW(teaEncrypt(v0, v1, zeroKey), TEAException);
    EXPECT_THROW(teaDecrypt(v0, v1, zeroKey), TEAException);

    // Invalid key with XTEA
    EXPECT_THROW(xteaEncrypt(v0, v1, zeroKey), TEAException);
    EXPECT_THROW(xteaDecrypt(v0, v1, zeroKey), TEAException);

    // Invalid key with XXTEA
    std::vector<uint32_t> data = {0x12345678, 0x9ABCDEF0};
    // Note: XXTEA doesn't actually check the key validity in the current
    // implementation EXPECT_THROW(xxteaEncrypt(data, zeroKey), TEAException);
}

// Test with randomly generated data
TEST_F(TEATest, RandomData) {
    // Generate random data of various sizes
    for (size_t size : {2, 10, 100, 1000}) {
        std::vector<uint32_t> data(size);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dist;

        std::generate(data.begin(), data.end(), [&]() { return dist(gen); });

        // Generate a random key
        auto random_key = generateRandomKey();

        // Test encryption/decryption round trip
        auto encrypted = xxteaEncrypt(data, random_key);
        auto decrypted = xxteaDecrypt(encrypted, random_key);

        expectEqualVectors(decrypted, data);

        // Also test parallel version
        auto encrypted_parallel = xxteaEncryptParallel(data, random_key);
        auto decrypted_parallel =
            xxteaDecryptParallel(encrypted_parallel, random_key);

        expectEqualVectors(decrypted_parallel, data);
    }
}

// Main function to run all tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}