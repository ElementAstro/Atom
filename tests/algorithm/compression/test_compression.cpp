/*
 * test_compression.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Algorithm Compression Library
Tests Huffman coding, matrix compression, and other compression algorithms.

**************************************************/

#include <gtest/gtest.h>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "atom/algorithm/huffman.hpp"

namespace atom::algorithm::compression::test {

// ============================================================================
// Huffman Coding Tests
// ============================================================================

class HuffmanTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test data
        test_string_ = "This is a test string for Huffman compression testing";
        test_data_ =
            std::vector<uint8_t>(test_string_.begin(), test_string_.end());
    }

    void TearDown() override {
        // Cleanup
    }

    std::string test_string_;
    std::vector<uint8_t> test_data_;
};

TEST_F(HuffmanTest, BasicCompression) {
    // Test basic Huffman compression with simple data
    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : test_data_) {
        frequencies[byte]++;
    }

    auto tree = atom::algorithm::createHuffmanTree(frequencies);
    ASSERT_NE(tree, nullptr);

    std::unordered_map<unsigned char, std::string> huffmanCodes;
    atom::algorithm::generateHuffmanCodes(tree.get(), "", huffmanCodes);

    EXPECT_FALSE(huffmanCodes.empty());

    // Verify all bytes in test data have codes
    for (unsigned char byte : test_data_) {
        EXPECT_TRUE(huffmanCodes.find(byte) != huffmanCodes.end());
    }
}

TEST_F(HuffmanTest, CompressionDecompression) {
    // Test complete compression and decompression roundtrip
    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : test_data_) {
        frequencies[byte]++;
    }

    auto tree = atom::algorithm::createHuffmanTree(frequencies);
    ASSERT_NE(tree, nullptr);

    std::unordered_map<unsigned char, std::string> huffmanCodes;
    atom::algorithm::generateHuffmanCodes(tree.get(), "", huffmanCodes);

    auto compressed = atom::algorithm::compressData(test_data_, huffmanCodes);
    EXPECT_FALSE(compressed.empty());

    auto decompressed = atom::algorithm::decompressData(compressed, tree.get());

    ASSERT_EQ(test_data_.size(), decompressed.size());
    for (size_t i = 0; i < test_data_.size(); ++i) {
        EXPECT_EQ(test_data_[i], decompressed[i]) << "Mismatch at index " << i;
    }
}

TEST_F(HuffmanTest, EmptyDataHandling) {
    // Test handling of empty data
    std::vector<unsigned char> empty_data;
    std::unordered_map<unsigned char, int> empty_frequencies;

    // Empty frequency map should throw
    EXPECT_THROW(atom::algorithm::createHuffmanTree(empty_frequencies),
                 atom::algorithm::HuffmanException);
}

TEST_F(HuffmanTest, SingleCharacterData) {
    // Test compression of single character repeated data
    std::vector<unsigned char> single_char_data(100, 'A');
    std::unordered_map<unsigned char, int> frequencies;
    frequencies['A'] = 100;

    auto tree = atom::algorithm::createHuffmanTree(frequencies);
    ASSERT_NE(tree, nullptr);

    std::unordered_map<unsigned char, std::string> huffmanCodes;
    atom::algorithm::generateHuffmanCodes(tree.get(), "", huffmanCodes);

    ASSERT_EQ(huffmanCodes.size(), 1);
    EXPECT_EQ(huffmanCodes['A'], "0");  // Single character gets code "0"

    auto compressed =
        atom::algorithm::compressData(single_char_data, huffmanCodes);
    auto decompressed = atom::algorithm::decompressData(compressed, tree.get());

    EXPECT_EQ(single_char_data, decompressed);
}

TEST_F(HuffmanTest, CompressionRatio) {
    // Test compression ratio for different types of data
    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : test_data_) {
        frequencies[byte]++;
    }

    auto tree = atom::algorithm::createHuffmanTree(frequencies);
    std::unordered_map<unsigned char, std::string> huffmanCodes;
    atom::algorithm::generateHuffmanCodes(tree.get(), "", huffmanCodes);

    auto compressed = atom::algorithm::compressData(test_data_, huffmanCodes);

    // Compressed data should be a string of '0' and '1' characters
    size_t compressed_bits = compressed.size();
    size_t original_bits = test_data_.size() * 8;

    // For text data, compression should reduce size
    EXPECT_LT(compressed_bits, original_bits);
}

TEST_F(HuffmanTest, LargeDataCompression) {
    // Test with larger data set
    std::vector<unsigned char> large_data;
    large_data.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        large_data.push_back(static_cast<unsigned char>('A' + (i % 26)));
    }

    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : large_data) {
        frequencies[byte]++;
    }

    auto tree = atom::algorithm::createHuffmanTree(frequencies);
    std::unordered_map<unsigned char, std::string> huffmanCodes;
    atom::algorithm::generateHuffmanCodes(tree.get(), "", huffmanCodes);

    auto compressed = atom::algorithm::compressData(large_data, huffmanCodes);
    auto decompressed = atom::algorithm::decompressData(compressed, tree.get());

    EXPECT_EQ(large_data, decompressed);
}

TEST_F(HuffmanTest, TreeSerialization) {
    // Test tree serialization and deserialization
    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : test_data_) {
        frequencies[byte]++;
    }

    auto tree = atom::algorithm::createHuffmanTree(frequencies);
    ASSERT_NE(tree, nullptr);

    // Serialize the tree
    auto serialized = atom::algorithm::serializeTree(tree.get());
    EXPECT_FALSE(serialized.empty());

    // Deserialize the tree
    size_t index = 0;
    auto deserialized_tree =
        atom::algorithm::deserializeTree(serialized, index);
    ASSERT_NE(deserialized_tree, nullptr);

    // Verify both trees produce same codes
    std::unordered_map<unsigned char, std::string> codes1, codes2;
    atom::algorithm::generateHuffmanCodes(tree.get(), "", codes1);
    atom::algorithm::generateHuffmanCodes(deserialized_tree.get(), "", codes2);

    EXPECT_EQ(codes1, codes2);
}

TEST_F(HuffmanTest, NullTreeHandling) {
    // Test error handling with null tree
    std::unordered_map<unsigned char, std::string> codes;
    EXPECT_THROW(atom::algorithm::generateHuffmanCodes(nullptr, "", codes),
                 atom::algorithm::HuffmanException);

    EXPECT_THROW(
        {
            auto result = atom::algorithm::decompressData("0101", nullptr);
            (void)result;
        },
        atom::algorithm::HuffmanException);

    EXPECT_THROW(
        {
            auto result = atom::algorithm::serializeTree(nullptr);
            (void)result;
        },
        atom::algorithm::HuffmanException);
}

TEST_F(HuffmanTest, InvalidCompressedData) {
    // Test decompression with invalid data
    std::unordered_map<unsigned char, int> frequencies;
    frequencies['A'] = 10;
    frequencies['B'] = 5;

    auto tree = atom::algorithm::createHuffmanTree(frequencies);

    // Invalid bit characters
    EXPECT_THROW(
        {
            auto result = atom::algorithm::decompressData("01X01", tree.get());
            (void)result;
        },
        atom::algorithm::HuffmanException);

    // Incomplete compressed data (doesn't end at leaf)
    EXPECT_THROW(
        {
            auto result = atom::algorithm::decompressData("0", tree.get());
            (void)result;
        },
        atom::algorithm::HuffmanException);
}

TEST_F(HuffmanTest, MissingHuffmanCode) {
    // Test compression with missing code
    std::vector<unsigned char> data = {'A', 'B', 'C'};
    std::unordered_map<unsigned char, std::string> incomplete_codes;
    incomplete_codes['A'] = "0";
    incomplete_codes['B'] = "10";
    // Missing code for 'C'

    EXPECT_THROW(
        {
            auto result = atom::algorithm::compressData(data, incomplete_codes);
            (void)result;
        },
        atom::algorithm::HuffmanException);
}

TEST_F(HuffmanTest, BinaryDataCompression) {
    // Test with binary data including null bytes
    std::vector<unsigned char> binary_data = {0x00, 0xFF, 0x7F,
                                              0x80, 0x00, 0xFF};

    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : binary_data) {
        frequencies[byte]++;
    }

    auto tree = atom::algorithm::createHuffmanTree(frequencies);
    std::unordered_map<unsigned char, std::string> huffmanCodes;
    atom::algorithm::generateHuffmanCodes(tree.get(), "", huffmanCodes);

    auto compressed = atom::algorithm::compressData(binary_data, huffmanCodes);
    auto decompressed = atom::algorithm::decompressData(compressed, tree.get());

    EXPECT_EQ(binary_data, decompressed);
}

TEST_F(HuffmanTest, ParallelFrequencyCount) {
    // Test parallel frequency counting
    std::vector<unsigned char> large_data;
    large_data.reserve(100000);
    for (int i = 0; i < 100000; ++i) {
        large_data.push_back(static_cast<unsigned char>('A' + (i % 26)));
    }

    auto frequencies = huffman_optimized::parallelFrequencyCount<unsigned char>(
        std::span<const unsigned char>(large_data), 4);

    EXPECT_EQ(frequencies.size(), 26);

    // Verify total count
    size_t total = 0;
    for (const auto& [byte, count] : frequencies) {
        total += count;
    }
    EXPECT_EQ(total, large_data.size());
}

TEST_F(HuffmanTest, ParallelCompression) {
    // Test parallel compression
    std::vector<unsigned char> data;
    data.reserve(50000);
    for (int i = 0; i < 50000; ++i) {
        data.push_back(static_cast<unsigned char>('A' + (i % 10)));
    }

    auto frequencies = huffman_optimized::parallelFrequencyCount<unsigned char>(
        std::span<const unsigned char>(data), 4);

    auto tree = huffman_optimized::createTreeParallel(frequencies);
    ASSERT_NE(tree, nullptr);

    std::unordered_map<unsigned char, std::string> huffmanCodes;
    atom::algorithm::generateHuffmanCodes(tree.get(), "", huffmanCodes);

    auto compressed = huffman_optimized::compressParallel(
        std::span<const unsigned char>(data), huffmanCodes, 4);

    EXPECT_FALSE(compressed.empty());

    // Verify decompression works
    auto decompressed = atom::algorithm::decompressData(compressed, tree.get());
    EXPECT_EQ(data, decompressed);
}

TEST_F(HuffmanTest, SimdCompression) {
    // Test SIMD-optimized compression
    std::vector<unsigned char> data;
    data.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        data.push_back(static_cast<unsigned char>('A' + (i % 5)));
    }

    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : data) {
        frequencies[byte]++;
    }

    auto tree = atom::algorithm::createHuffmanTree(frequencies);
    std::unordered_map<unsigned char, std::string> huffmanCodes;
    atom::algorithm::generateHuffmanCodes(tree.get(), "", huffmanCodes);

    auto compressed = huffman_optimized::compressSimd(
        std::span<const unsigned char>(data), huffmanCodes);

    EXPECT_FALSE(compressed.empty());

    auto decompressed = atom::algorithm::decompressData(compressed, tree.get());
    EXPECT_EQ(data, decompressed);
}

TEST_F(HuffmanTest, InputValidation) {
    // Test input validation
    std::vector<unsigned char> data = {'A', 'B', 'C'};
    std::unordered_map<unsigned char, std::string> codes;
    codes['A'] = "0";
    codes['B'] = "10";
    codes['C'] = "11";

    EXPECT_NO_THROW(huffman_optimized::validateInput(
        std::span<const unsigned char>(data), codes));

    // Empty data should throw
    std::vector<unsigned char> empty_data;
    EXPECT_THROW(huffman_optimized::validateInput(
                     std::span<const unsigned char>(empty_data), codes),
                 atom::algorithm::HuffmanException);

    // Empty codes should throw
    std::unordered_map<unsigned char, std::string> empty_codes;
    EXPECT_THROW(huffman_optimized::validateInput(
                     std::span<const unsigned char>(data), empty_codes),
                 atom::algorithm::HuffmanException);
}

// ============================================================================
// General Compression Tests
// ============================================================================

class GeneralCompressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup general compression tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(GeneralCompressionTest, CompressionAlgorithmComparison) {
    // Compare Huffman compression performance on different data types
    std::string text_data =
        "This is a test string with repeated characters aaaaabbbbcccc";
    std::vector<unsigned char> text_bytes(text_data.begin(), text_data.end());

    // Huffman compression on text data
    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : text_bytes) {
        frequencies[byte]++;
    }
    auto huffman_tree = atom::algorithm::createHuffmanTree(frequencies);
    std::unordered_map<unsigned char, std::string> huffman_codes;
    atom::algorithm::generateHuffmanCodes(huffman_tree.get(), "",
                                          huffman_codes);
    auto huffman_compressed =
        atom::algorithm::compressData(text_bytes, huffman_codes);

    // Test on binary data
    std::vector<unsigned char> binary_data;
    for (int i = 0; i < 1000; ++i) {
        binary_data.push_back(static_cast<unsigned char>(i % 256));
    }

    std::unordered_map<unsigned char, int> binary_frequencies;
    for (unsigned char byte : binary_data) {
        binary_frequencies[byte]++;
    }
    auto binary_tree = atom::algorithm::createHuffmanTree(binary_frequencies);
    std::unordered_map<unsigned char, std::string> binary_codes;
    atom::algorithm::generateHuffmanCodes(binary_tree.get(), "", binary_codes);
    auto binary_compressed =
        atom::algorithm::compressData(binary_data, binary_codes);

    // Both should successfully compress
    EXPECT_FALSE(huffman_compressed.empty());
    EXPECT_FALSE(binary_compressed.empty());

    // Text data should compress better than random binary data
    double text_compression_ratio =
        static_cast<double>(huffman_compressed.size()) /
        static_cast<double>(text_bytes.size() * 8);
    double binary_compression_ratio =
        static_cast<double>(binary_compressed.size()) /
        static_cast<double>(binary_data.size() * 8);

    EXPECT_LT(text_compression_ratio, binary_compression_ratio);
}

TEST_F(GeneralCompressionTest, PerformanceBenchmarks) {
    // Benchmark compression performance
    std::vector<unsigned char> large_data;
    large_data.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        large_data.push_back(static_cast<unsigned char>('A' + (i % 10)));
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : large_data) {
        frequencies[byte]++;
    }
    auto tree = atom::algorithm::createHuffmanTree(frequencies);
    std::unordered_map<unsigned char, std::string> codes;
    atom::algorithm::generateHuffmanCodes(tree.get(), "", codes);
    auto compressed = atom::algorithm::compressData(large_data, codes);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (less than 1 second for 10k bytes)
    EXPECT_LT(duration.count(), 1000);
}

TEST_F(GeneralCompressionTest, MemoryUsage) {
    // Test memory usage during compression
    // Create data that will require significant memory
    std::vector<unsigned char> data;
    data.reserve(100000);
    for (int i = 0; i < 100000; ++i) {
        data.push_back(static_cast<unsigned char>(i % 256));
    }

    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : data) {
        frequencies[byte]++;
    }

    // Should not throw out of memory
    bool success = false;
    try {
        auto tree = atom::algorithm::createHuffmanTree(frequencies);
        std::unordered_map<unsigned char, std::string> codes;
        atom::algorithm::generateHuffmanCodes(tree.get(), "", codes);
        auto compressed = atom::algorithm::compressData(data, codes);
        auto decompressed =
            atom::algorithm::decompressData(compressed, tree.get());
        success = true;
    } catch (...) {
        success = false;
    }
    EXPECT_TRUE(success);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

class CompressionErrorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup error condition tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(CompressionErrorTest, CorruptedDataHandling) {
    // Test handling of corrupted compressed data
    std::unordered_map<unsigned char, int> frequencies;
    frequencies['A'] = 10;
    frequencies['B'] = 5;

    auto tree = atom::algorithm::createHuffmanTree(frequencies);

    // Corrupted data with invalid bits
    EXPECT_THROW(
        {
            auto result = atom::algorithm::decompressData("01X01", tree.get());
            (void)result;
        },
        atom::algorithm::HuffmanException);

    // Data that doesn't end at a leaf node
    EXPECT_THROW(
        {
            auto result = atom::algorithm::decompressData("0", tree.get());
            (void)result;
        },
        atom::algorithm::HuffmanException);
}

TEST_F(CompressionErrorTest, InvalidInputHandling) {
    // Test handling of invalid input data

    // Empty frequency map
    std::unordered_map<unsigned char, int> empty_freq;
    EXPECT_THROW(
        {
            auto result = atom::algorithm::createHuffmanTree(empty_freq);
            (void)result;
        },
        atom::algorithm::HuffmanException);

    // Null tree for code generation
    std::unordered_map<unsigned char, std::string> codes;
    EXPECT_THROW(atom::algorithm::generateHuffmanCodes(nullptr, "", codes),
                 atom::algorithm::HuffmanException);

    // Invalid compressed data format
    EXPECT_THROW(
        {
            auto result = atom::algorithm::decompressData("", nullptr);
            (void)result;
        },
        atom::algorithm::HuffmanException);
}

TEST_F(CompressionErrorTest, MemoryLimitHandling) {
    // Test behavior under memory constraints
    // Create very large frequency map
    std::unordered_map<unsigned char, int> large_freq;
    for (int i = 0; i < 256; ++i) {
        large_freq[static_cast<unsigned char>(i)] = i + 1;
    }

    // Should handle large frequency maps
    auto tree = atom::algorithm::createHuffmanTree(large_freq);
    EXPECT_NE(tree, nullptr);

    std::unordered_map<unsigned char, std::string> codes;
    EXPECT_NO_THROW(
        atom::algorithm::generateHuffmanCodes(tree.get(), "", codes));

    EXPECT_EQ(codes.size(), 256);
}

// ============================================================================
// Integration Tests
// ============================================================================

class CompressionIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup integration test environment
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(CompressionIntegrationTest, EndToEndCompression) {
    // Test complete compression workflow from data to compressed to
    // decompressed
    std::string original_text =
        "The quick brown fox jumps over the lazy dog. "
        "This is a test of the Huffman compression algorithm. "
        "It should compress repeated characters efficiently.";
    std::vector<unsigned char> original_data(original_text.begin(),
                                             original_text.end());

    // Step 1: Build frequency map
    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : original_data) {
        frequencies[byte]++;
    }

    // Step 2: Create Huffman tree
    auto tree = atom::algorithm::createHuffmanTree(frequencies);
    ASSERT_NE(tree, nullptr);

    // Step 3: Generate codes
    std::unordered_map<unsigned char, std::string> huffman_codes;
    atom::algorithm::generateHuffmanCodes(tree.get(), "", huffman_codes);

    // Step 4: Compress
    auto compressed =
        atom::algorithm::compressData(original_data, huffman_codes);
    EXPECT_FALSE(compressed.empty());

    // Step 5: Serialize tree for storage/transmission
    auto serialized_tree = atom::algorithm::serializeTree(tree.get());
    EXPECT_FALSE(serialized_tree.empty());

    // Step 6: Deserialize tree
    size_t index = 0;
    auto deserialized_tree =
        atom::algorithm::deserializeTree(serialized_tree, index);
    ASSERT_NE(deserialized_tree, nullptr);

    // Step 7: Decompress
    auto decompressed =
        atom::algorithm::decompressData(compressed, deserialized_tree.get());

    // Step 8: Verify
    ASSERT_EQ(original_data.size(), decompressed.size());
    EXPECT_EQ(original_data, decompressed);
}

TEST_F(CompressionIntegrationTest, MultipleDataFormatCompression) {
    // Test Huffman compression on different data formats
    std::string text = "AAAABBBBCCCCDDDD";
    std::vector<unsigned char> data(text.begin(), text.end());

    // Build frequency map manually (buildFrequencyMap doesn't exist)
    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : data) {
        frequencies[byte]++;
    }

    // Test with string input
    auto tree = atom::algorithm::createHuffmanTree(frequencies);
    std::unordered_map<unsigned char, std::string> codes;
    atom::algorithm::generateHuffmanCodes(tree.get(), "", codes);
    auto compressed_from_string = atom::algorithm::compressData(data, codes);

    // Test with vector input
    std::vector<unsigned char> vector_data(data.begin(), data.end());
    auto compressed_from_vector =
        atom::algorithm::compressData(vector_data, codes);

    // Both should produce same result
    EXPECT_EQ(compressed_from_string, compressed_from_vector);

    // Verify decompression works for both
    auto decompressed_from_string =
        atom::algorithm::decompressData(compressed_from_string, tree.get());
    auto decompressed_from_vector =
        atom::algorithm::decompressData(compressed_from_vector, tree.get());

    EXPECT_EQ(data, decompressed_from_string);
    EXPECT_EQ(data, decompressed_from_vector);
}

TEST_F(CompressionIntegrationTest, RealWorldDataCompression) {
    // Test compression on real-world-like data samples

    // Simulate JSON data
    std::string json_like = R"({
        "name": "test",
        "value": 123,
        "items": ["a", "b", "c"],
        "nested": {"key": "value"}
    })";
    std::vector<unsigned char> json_data(json_like.begin(), json_like.end());

    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : json_data) {
        frequencies[byte]++;
    }

    auto tree = atom::algorithm::createHuffmanTree(frequencies);
    std::unordered_map<unsigned char, std::string> codes;
    atom::algorithm::generateHuffmanCodes(tree.get(), "", codes);

    auto compressed = atom::algorithm::compressData(json_data, codes);
    auto decompressed = atom::algorithm::decompressData(compressed, tree.get());

    EXPECT_EQ(json_data, decompressed);

    // Calculate compression ratio
    double ratio = static_cast<double>(compressed.size()) /
                   static_cast<double>(json_data.size() * 8);

    // JSON has repeated characters, should achieve some compression
    EXPECT_LT(ratio, 1.0);
}

}  // namespace atom::algorithm::compression::test

// Main function removed - using gtest_main
