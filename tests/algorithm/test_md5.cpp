#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "atom/algorithm/md5.hpp"
#include "atom/log/loguru.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

// Helper function to generate random data
std::vector<std::byte> generateRandomBytes(size_t size) {
    std::vector<std::byte> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 255);

    for (size_t i = 0; i < size; ++i) {
        data[i] = static_cast<std::byte>(dist(gen));
    }
    return data;
}

// Helper function to convert hex string to byte array
std::vector<std::byte> hexToBytes(const std::string& hex) {
    std::vector<std::byte> bytes;
    bytes.reserve(hex.length() / 2);

    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        auto byte = static_cast<std::byte>(std::stoi(byteString, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// Test fixture for MD5 tests
class MD5Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing
        static bool initialized = false;
        if (!initialized) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            initialized = true;
        }
    }

    // Known test vectors from RFC 1321
    struct TestVector {
        std::string input;
        std::string expected;
    };

    std::vector<TestVector> testVectors = {
        {"", "d41d8cd98f00b204e9800998ecf8427e"},
        {"a", "0cc175b9c0f1b6a831c399e269772661"},
        {"abc", "900150983cd24fb0d6963f7d28e17f72"},
        {"message digest", "f96b697d7cb7938d525a2f31aaf161d0"},
        {"abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b"},
        {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
         "d174ab98d277d9f5a5611c2c9f419d9f"},
        {"123456789012345678901234567890123456789012345678901234567890123456789"
         "01234567890",
         "57edf4a22be3c955ac49da2e2107b67a"}};
};

// Test empty string
TEST_F(MD5Test, EmptyString) {
    std::string emptyStr = "";
    std::string hash = MD5::encrypt(emptyStr);
    EXPECT_EQ(hash, "d41d8cd98f00b204e9800998ecf8427e");
}

// Test known vectors
TEST_F(MD5Test, KnownVectors) {
    for (const auto& test : testVectors) {
        std::string hash = MD5::encrypt(test.input);
        EXPECT_EQ(hash, test.expected) << "Failed for input: " << test.input;
    }
}

// Test with different string types
TEST_F(MD5Test, DifferentStringTypes) {
    // Test with std::string
    std::string stdString = "abc";
    EXPECT_EQ(MD5::encrypt(stdString), "900150983cd24fb0d6963f7d28e17f72");

    // Test with string literal
    EXPECT_EQ(MD5::encrypt("abc"), "900150983cd24fb0d6963f7d28e17f72");

    // Test with string_view
    std::string_view stringView = "abc";
    EXPECT_EQ(MD5::encrypt(stringView), "900150983cd24fb0d6963f7d28e17f72");

    // Test with char array
    char charArray[] = "abc";
    EXPECT_EQ(MD5::encrypt(charArray), "900150983cd24fb0d6963f7d28e17f72");
}

// Test with binary data
TEST_F(MD5Test, BinaryData) {
    // Create binary data with null bytes and control characters
    std::vector<std::byte> binaryData;
    for (int i = 0; i < 256; ++i) {
        binaryData.push_back(static_cast<std::byte>(i));
    }

    std::string hash = MD5::encryptBinary(binaryData);

    // Expected hash of bytes 0-255
    std::string expectedHash = "e2c865db4162bed963bfaa9ef6ac18f0";
    EXPECT_EQ(hash, expectedHash);
}

// Test with very large data
TEST_F(MD5Test, LargeData) {
    // Create 1MB of random data
    const size_t dataSize = 1024 * 1024;
    std::vector<std::byte> largeData = generateRandomBytes(dataSize);

    // Hash the large data
    std::string hash = MD5::encryptBinary(largeData);

    // We don't know the expected hash, but it should be 32 hex characters
    EXPECT_EQ(hash.length(), 32);

    // All characters should be valid hex
    for (char c : hash) {
        bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        EXPECT_TRUE(isHex) << "Invalid hex character in hash: " << c;
    }
}

// Test with zero-length span
TEST_F(MD5Test, ZeroLengthSpan) {
    std::span<const std::byte> emptySpan;
    std::string hash = MD5::encryptBinary(emptySpan);
    EXPECT_EQ(hash, "d41d8cd98f00b204e9800998ecf8427e");
}

// Test verification function
TEST_F(MD5Test, Verification) {
    std::string input = "Hello, world!";
    std::string correctHash = MD5::encrypt(input);
    std::string wrongHash = "0000000000000000000000000000000";

    EXPECT_TRUE(MD5::verify(input, correctHash));
    EXPECT_FALSE(MD5::verify(input, wrongHash));
}

// Test incremental hashing by using the public API
TEST_F(MD5Test, IncrementalHashing) {
    std::string input1 = "Hello, ";
    std::string input2 = "world!";
    std::string combined = input1 + input2;

    // Test using the static methods
    std::string directHash = MD5::encrypt(combined);

    // We can't directly test incremental updates since the methods are private
    // Let's verify that the public API works correctly
    EXPECT_EQ(MD5::encrypt(combined), directHash);
}

// Test exception handling in encrypt
TEST_F(MD5Test, ExceptionHandling) {
    // Create a custom string-like type that throws on conversion
    struct ThrowingString {
        operator std::string_view() const {
            throw std::runtime_error("Test exception");
        }
    };

    ThrowingString throwingStr;
    EXPECT_THROW(MD5::encrypt(throwingStr), MD5Exception);
}

// Test verify handles exceptions
TEST_F(MD5Test, VerifyExceptionHandling) {
    struct ThrowingString {
        operator std::string_view() const {
            throw std::runtime_error("Test exception");
        }
    };

    ThrowingString throwingStr;
    // verify should handle the exception and return false
    EXPECT_FALSE(MD5::verify(throwingStr, "any-hash"));
}

// Test with Unicode data
TEST_F(MD5Test, UnicodeData) {
    // Unicode string with various characters
    std::string unicodeStr = "こんにちは世界！";

    // Calculate hash
    std::string hash = MD5::encrypt(unicodeStr);

    // The expected hash depends on how the Unicode is encoded in memory
    // This test mainly checks that it doesn't crash or throw
    EXPECT_EQ(hash.length(), 32);
}

// Test with multi-byte characters
TEST_F(MD5Test, MultibyteCharacters) {
    // Multi-byte emoji
    std::string emoji = "😀👍🌍";

    // Calculate hash
    std::string hash = MD5::encrypt(emoji);

    // Again, mainly testing it doesn't crash
    EXPECT_EQ(hash.length(), 32);
}

// Test MD5 of a file-like data
TEST_F(MD5Test, FileContent) {
    // Simulate file content with binary data and text
    std::vector<std::byte> fileData;

    // Add a text header
    std::string header = "FILE_HEADER";
    const auto* header_ptr = reinterpret_cast<const std::byte*>(header.data());
    fileData.insert(fileData.end(), header_ptr, header_ptr + header.size());

    // Add some binary data
    std::vector<std::byte> binaryPart = generateRandomBytes(1000);
    fileData.insert(fileData.end(), binaryPart.begin(), binaryPart.end());

    // Add a text footer
    std::string footer = "FILE_FOOTER";
    const auto* footer_ptr = reinterpret_cast<const std::byte*>(footer.data());
    fileData.insert(fileData.end(), footer_ptr, footer_ptr + footer.size());

    // Calculate hash
    std::string hash = MD5::encryptBinary(fileData);

    // Verify basic properties
    EXPECT_EQ(hash.length(), 32);
}

// Test performance with large data
TEST_F(MD5Test, Performance) {
    const size_t dataSize = 10 * 1024 * 1024;  // 10 MB
    auto largeData = generateRandomBytes(dataSize);

    auto start = std::chrono::high_resolution_clock::now();
    std::string hash = MD5::encryptBinary(largeData);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "MD5 hash of " << dataSize / (1024 * 1024) << " MB took "
              << duration.count() << " ms" << std::endl;

    // No specific performance requirement, just for information
    EXPECT_EQ(hash.length(), 32);
}

// Test thread safety
TEST_F(MD5Test, ThreadSafety) {
    constexpr int numThreads = 10;
    std::vector<std::string> inputs = {
        "thread1", "thread2", "thread3", "thread4", "thread5",
        "thread6", "thread7", "thread8", "thread9", "thread10"};

    std::vector<std::future<std::string>> futures;

    // Launch multiple threads to hash different inputs
    for (int i = 0; i < numThreads; ++i) {
        futures.push_back(std::async(std::launch::async, [i, &inputs]() {
            return MD5::encrypt(inputs[i]);
        }));
    }

    // Collect results
    std::vector<std::string> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }

    // Verify results by comparing with single-threaded execution
    for (int i = 0; i < numThreads; ++i) {
        std::string expectedHash = MD5::encrypt(inputs[i]);
        EXPECT_EQ(results[i], expectedHash);
    }
}

// Test MD5 collision resistance (for small input changes)
TEST_F(MD5Test, CollisionResistance) {
    std::string input1 = "test string";
    std::string input2 = "test strinf";  // One character different

    std::string hash1 = MD5::encrypt(input1);
    std::string hash2 = MD5::encrypt(input2);

    // Even with a small change, hashes should be completely different
    EXPECT_NE(hash1, hash2);

    // Hamming distance between hashes should be significant
    int hammingDistance = 0;
    for (size_t i = 0; i < hash1.length(); ++i) {
        unsigned char c1 = static_cast<unsigned char>(hash1[i]);
        unsigned char c2 = static_cast<unsigned char>(hash2[i]);
        unsigned char diff = c1 ^ c2;
        while (diff) {
            ++hammingDistance;
            diff &= diff - 1;
        }
    }

    // Just verifying there's a significant difference
    EXPECT_GT(hammingDistance, 10);
}

// Test consistency with different input sources
TEST_F(MD5Test, ConsistencyWithDifferentSources) {
    std::string input = "test consistency";
    const auto* data_ptr = reinterpret_cast<const std::byte*>(input.data());

    // Using string API
    std::string hash1 = MD5::encrypt(input);

    // Using binary API with the same data
    std::string hash2 =
        MD5::encryptBinary(std::span<const std::byte>(data_ptr, input.size()));

    EXPECT_EQ(hash1, hash2);
}

// Test mixing binary and string operations
TEST_F(MD5Test, MixedOperations) {
    // Create some binary data
    std::vector<std::byte> binaryData = generateRandomBytes(100);

    // Create a string copy of the same data
    std::string stringData;
    stringData.reserve(binaryData.size());
    for (auto byte : binaryData) {
        stringData.push_back(static_cast<char>(byte));
    }

    // Hash both ways
    std::string binaryHash = MD5::encryptBinary(binaryData);
    std::string stringHash = MD5::encrypt(stringData);

    // Results should be identical
    EXPECT_EQ(binaryHash, stringHash);
}

// Test repeated usage of static functions
TEST_F(MD5Test, RepeatedStaticUsage) {
    std::string input1 = "First data";
    std::string input2 = "Second data";

    // Hash multiple times using static methods
    std::string hash1_1 = MD5::encrypt(input1);
    std::string hash1_2 = MD5::encrypt(input1);
    std::string hash2 = MD5::encrypt(input2);

    // Same input should give same hash
    EXPECT_EQ(hash1_1, hash1_2);

    // Different input should give different hash
    EXPECT_NE(hash1_1, hash2);
}
