/**
 * @file sha1.cpp
 * @brief Comprehensive example demonstrating SHA-1 hash algorithm usage
 *
 * This example shows how to:
 * - Compute SHA-1 hashes for various types of input
 * - Handle different data formats (strings, binary data, streaming)
 * - Demonstrate incremental hashing capabilities
 * - Show performance characteristics and SIMD optimizations
 * - Handle errors and edge cases gracefully
 * - Compare with other hash algorithms
 *
 * @author Atom Framework
 * @date 2024-12-19
 */

#include "atom/algorithm/crypto/sha1.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace atom::algorithm;

/**
 * @brief Demonstrates basic SHA-1 hashing with various string inputs
 */
void demonstrateBasicSHA1Hashing() {
    std::cout << "\n=== Basic SHA-1 Hashing Examples ===\n";

    try {
        // Test various string inputs
        std::vector<std::string> testInputs = {
            "Hello, World!",
            "",  // Empty string
            "The quick brown fox jumps over the lazy dog",
            "SHA-1 produces a 160-bit hash value known as a message digest",
            "1234567890",
            "Special characters: !@#$%^&*()_+-=[]{}|;:,.<>?",
            "Unicode: 你好世界 🌍 🚀",
            "A very long string that exceeds the typical block size to test "
            "multi-block processing capabilities of the SHA-1 algorithm "
            "implementation"};

        for (const auto& input : testInputs) {
            SHA1 sha1;
            sha1.update(reinterpret_cast<const uint8_t*>(input.c_str()),
                        input.size());
            auto hash = sha1.digest();
            std::string hashHex = bytesToHex(hash);

            std::cout << "Input: \""
                      << (input.length() > 50 ? input.substr(0, 47) + "..."
                                              : input)
                      << "\"\n";
            std::cout << "SHA-1: " << hashHex << "\n";
            std::cout << "Length: " << hashHex.length()
                      << " characters (160 bits)\n\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in basic SHA-1 hashing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates incremental hashing capabilities
 */
void demonstrateIncrementalHashing() {
    std::cout << "\n=== Incremental SHA-1 Hashing ===\n";

    try {
        std::string fullMessage =
            "This is a long message that will be processed incrementally to "
            "demonstrate the streaming capabilities of SHA-1";

        // Hash the full message at once
        SHA1 sha1_full;
        sha1_full.update(reinterpret_cast<const uint8_t*>(fullMessage.c_str()),
                         fullMessage.size());
        auto fullHash = sha1_full.digest();
        std::string fullHashHex = bytesToHex(fullHash);

        std::cout << "Full message hash: " << fullHashHex << "\n";

        // Hash the same message incrementally
        SHA1 sha1_incremental;
        size_t chunkSize = 10;

        std::cout << "\nProcessing incrementally in chunks of " << chunkSize
                  << " bytes:\n";
        for (size_t i = 0; i < fullMessage.length(); i += chunkSize) {
            size_t currentChunkSize =
                std::min(chunkSize, fullMessage.length() - i);
            std::string chunk = fullMessage.substr(i, currentChunkSize);

            sha1_incremental.update(
                reinterpret_cast<const uint8_t*>(chunk.c_str()), chunk.size());
            std::cout << "  Chunk " << (i / chunkSize + 1) << ": \"" << chunk
                      << "\"\n";
        }

        auto incrementalHash = sha1_incremental.digest();
        std::string incrementalHashHex = bytesToHex(incrementalHash);

        std::cout << "\nIncremental hash: " << incrementalHashHex << "\n";
        std::cout << "Hashes match: "
                  << (fullHashHex == incrementalHashHex ? "✓ YES" : "✗ NO")
                  << "\n";

        // Demonstrate reset functionality
        std::cout << "\nDemonstrating reset functionality:\n";
        sha1_incremental.reset();
        sha1_incremental.update(reinterpret_cast<const uint8_t*>("New message"),
                                11);
        auto newHash = sha1_incremental.digest();
        std::string newHashHex = bytesToHex(newHash);
        std::cout << "Hash after reset: " << newHashHex << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in incremental hashing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates binary data hashing
 */
void demonstrateBinaryDataHashing() {
    std::cout << "\n=== Binary Data SHA-1 Hashing ===\n";

    try {
        // Create binary test data
        std::vector<uint8_t> binaryData = {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
            0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA,
            0xF9, 0xF8, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};

        SHA1 sha1;
        sha1.update(binaryData.data(), binaryData.size());
        auto hash = sha1.digest();
        std::string hashHex = bytesToHex(hash);

        std::cout << "Binary data (hex): ";
        for (uint8_t byte : binaryData) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(byte) << " ";
        }
        std::cout << std::dec << "\n";
        std::cout << "SHA-1 hash: " << hashHex << "\n";

        // Test with different binary patterns
        std::vector<std::vector<uint8_t>> patterns = {
            std::vector<uint8_t>(64, 0x00),  // All zeros
            std::vector<uint8_t>(64, 0xFF),  // All ones
            std::vector<uint8_t>()           // Sequential pattern
        };

        // Create sequential pattern
        for (int i = 0; i < 64; ++i) {
            patterns[2].push_back(static_cast<uint8_t>(i));
        }

        std::vector<std::string> patternNames = {
            "All zeros (64 bytes)", "All ones (64 bytes)", "Sequential (0-63)"};

        std::cout << "\nBinary pattern hashes:\n";
        for (size_t i = 0; i < patterns.size(); ++i) {
            SHA1 patternSha1;
            patternSha1.update(patterns[i].data(), patterns[i].size());
            auto patternHash = patternSha1.digest();
            std::string patternHashHex = bytesToHex(patternHash);
            std::cout << patternNames[i] << ": " << patternHashHex << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in binary data hashing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates performance characteristics and SIMD optimizations
 */
void demonstratePerformanceCharacteristics() {
    std::cout << "\n=== Performance Characteristics ===\n";

    try {
        // Test performance with different data sizes
        std::vector<size_t> dataSizes = {1024, 10240, 102400,
                                         1024000};  // 1KB, 10KB, 100KB, 1MB
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        std::cout << "Performance test with different data sizes:\n";

        for (size_t size : dataSizes) {
            // Generate random data
            std::vector<uint8_t> data(size);
            for (size_t i = 0; i < size; ++i) {
                data[i] = static_cast<uint8_t>(dis(gen));
            }

            // Measure hashing time
            auto start = std::chrono::high_resolution_clock::now();
            SHA1 sha1;
            sha1.update(data.data(), data.size());
            [[maybe_unused]] auto hash = sha1.digest();
            auto end = std::chrono::high_resolution_clock::now();

            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);
            double throughput = (static_cast<double>(size) / 1024.0 / 1024.0) /
                                (duration.count() / 1000000.0);  // MB/s

            std::cout << "  " << size << " bytes: " << duration.count()
                      << " μs";
            std::cout << " (throughput: " << std::fixed << std::setprecision(2)
                      << throughput << " MB/s)\n";
        }

        // Security warnings and recommendations
        std::cout << "\n⚠️  SECURITY WARNINGS:\n";
        std::cout << "   • SHA-1 is cryptographically broken and unsuitable "
                     "for security purposes\n";
        std::cout << "   • SHA-1 is vulnerable to collision attacks "
                     "(demonstrated in 2017)\n";
        std::cout << "   • Use SHA-256, SHA-3, or BLAKE2 for cryptographic "
                     "applications\n";
        std::cout << "   • SHA-1 is acceptable for non-cryptographic checksums "
                     "only\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in performance demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates practical use cases and comparisons
 */
void demonstratePracticalUseCases() {
    std::cout << "\n=== Practical Use Cases ===\n";

    try {
        // Use case 1: File integrity verification
        std::cout << "1. File Integrity Verification (educational only):\n";
        std::string fileContent =
            "This is the content of a file that needs integrity verification.";

        SHA1 sha1;
        sha1.update(reinterpret_cast<const uint8_t*>(fileContent.c_str()),
                    fileContent.size());
        auto originalHash = sha1.digest();
        std::string originalHashHex = bytesToHex(originalHash);

        std::cout << "   Original file hash: " << originalHashHex << "\n";

        // Simulate file modification
        std::string modifiedContent = fileContent + " [MODIFIED]";
        sha1.reset();
        sha1.update(reinterpret_cast<const uint8_t*>(modifiedContent.c_str()),
                    modifiedContent.size());
        auto modifiedHash = sha1.digest();
        std::string modifiedHashHex = bytesToHex(modifiedHash);

        std::cout << "   Modified file hash: " << modifiedHashHex << "\n";
        std::cout << "   Integrity check: "
                  << (originalHashHex == modifiedHashHex ? "✓ INTACT"
                                                         : "✗ MODIFIED")
                  << "\n";

        // Use case 2: Data deduplication (demonstration)
        std::cout << "\n2. Data Deduplication Simulation:\n";
        std::vector<std::string> dataBlocks = {
            "Block 1: Unique content", "Block 2: Different content",
            "Block 1: Unique content",  // Duplicate
            "Block 3: Another unique block"};

        std::unordered_map<std::string, std::vector<size_t>> hashToBlocks;

        for (size_t i = 0; i < dataBlocks.size(); ++i) {
            SHA1 blockSha1;
            blockSha1.update(
                reinterpret_cast<const uint8_t*>(dataBlocks[i].c_str()),
                dataBlocks[i].size());
            auto blockHash = blockSha1.digest();
            std::string blockHashHex = bytesToHex(blockHash);

            hashToBlocks[blockHashHex].push_back(i);
            std::cout << "   Block " << i << " hash: " << blockHashHex << "\n";
        }

        std::cout << "\n   Deduplication results:\n";
        for (const auto& [hash, blocks] : hashToBlocks) {
            if (blocks.size() > 1) {
                std::cout << "     Duplicate blocks found: ";
                for (size_t blockIndex : blocks) {
                    std::cout << blockIndex << " ";
                }
                std::cout << "(hash: " << hash.substr(0, 16) << "...)\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in practical use cases: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive SHA-1 usage
 */
int main() {
    std::cout << "=== Atom SHA-1 Algorithm Comprehensive Example ===\n";
    std::cout << "Demonstrating SHA-1 hash computation capabilities...\n";

    try {
        // Run all demonstration functions
        demonstrateBasicSHA1Hashing();
        demonstrateIncrementalHashing();
        demonstrateBinaryDataHashing();
        demonstratePerformanceCharacteristics();
        demonstratePracticalUseCases();

        std::cout << "\n=== All SHA-1 Examples Completed Successfully ===\n";
        std::cout << "The SHA-1 algorithm provides:\n";
        std::cout << "  ✓ 160-bit hash output (40 hex characters)\n";
        std::cout << "  ✓ Incremental/streaming hash computation\n";
        std::cout << "  ✓ Fast performance with SIMD optimizations\n";
        std::cout << "  ✓ Suitable for non-cryptographic checksums\n";
        std::cout << "  ⚠️  NOT suitable for cryptographic security\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in SHA-1 example: " << e.what()
                  << "\n";
        return 1;
    }
}
