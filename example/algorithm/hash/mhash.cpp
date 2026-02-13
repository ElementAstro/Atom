/**
 * @file mhash.cpp
 * @brief Comprehensive example demonstrating multi-hash utilities and MinHash
 *
 * This example shows how to:
 * - Use MinHash for similarity estimation and Jaccard index calculation
 * - Perform hexstring conversions for data encoding
 * - Use Keccak-256 hash function for cryptographic hashing
 * - Demonstrate practical applications of similarity estimation
 * - Compare different sets and analyze their relationships
 * - Handle large datasets efficiently with MinHash
 *
 * @author Atom Framework
 * @date 2024-12-19
 */

#include "atom/algorithm/hash/mhash.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace atom::algorithm;

/**
 * @brief Helper function to print section headers
 */
void printHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

/**
 * @brief Demonstrates hexstring conversion utilities
 */
void demonstrateHexstringConversion() {
    printHeader("Hexstring Conversion Utilities");

    try {
        // Test various data types
        std::vector<std::string> testStrings = {
            "Hello, World!", "Binary data: \x00\x01\x02\x03\xFF\xFE\xFD",
            "",  // Empty string
            "Unicode: 你好世界 🌍",
            "Special chars: !@#$%^&*()_+-=[]{}|;:,.<>?"};

        for (const auto& original : testStrings) {
            std::cout << "Original: \"" << original << "\"\n";

            // Convert to hexstring
            std::string hexString = hexstringFromData(original);
            std::cout << "Hexstring: " << hexString << "\n";

            // Convert back
            std::string recovered = dataFromHexstring(hexString);
            std::cout << "Recovered: \"" << recovered << "\"\n";

            // Verify round-trip
            bool success = (original == recovered);
            std::cout << "Round-trip: " << (success ? "✓ SUCCESS" : "✗ FAILED")
                      << "\n\n";
        }

        // Test binary data
        std::vector<uint8_t> binaryData = {0x00, 0x01, 0x02, 0x03,
                                           0xFF, 0xFE, 0xFD, 0xFC};
        std::string binaryStr(binaryData.begin(), binaryData.end());

        std::cout << "Binary data test:\n";
        std::cout << "Original bytes: ";
        for (uint8_t byte : binaryData) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(byte) << " ";
        }
        std::cout << std::dec << "\n";

        std::string hexFromBinary = hexstringFromData(binaryStr);
        std::cout << "Hexstring: " << hexFromBinary << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in hexstring conversion: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates basic MinHash functionality
 */
void demonstrateBasicMinHash() {
    printHeader("Basic MinHash Similarity Estimation");

    try {
        // Create MinHash objects with different numbers of hash functions
        MinHash minHash64(64);    // 64 hash functions for faster computation
        MinHash minHash256(256);  // 256 hash functions for higher accuracy

        // Example sets with known overlap
        std::vector<int> set1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        std::vector<int> set2 = {6,  7,  8,  9,  10,
                                 11, 12, 13, 14, 15};  // 50% overlap
        std::vector<int> set3 = {1, 2, 3, 4, 5};       // 50% of set1
        std::vector<int> set4 = {20, 21, 22, 23, 24};  // No overlap

        std::cout << "Test sets:\n";
        std::cout << "  Set 1: {1,2,3,4,5,6,7,8,9,10}\n";
        std::cout << "  Set 2: {6,7,8,9,10,11,12,13,14,15} (50% overlap with "
                     "Set 1)\n";
        std::cout << "  Set 3: {1,2,3,4,5} (subset of Set 1)\n";
        std::cout << "  Set 4: {20,21,22,23,24} (no overlap)\n\n";

        // Compute signatures
        auto sig1_64 = minHash64.computeSignature(set1);
        auto sig2_64 = minHash64.computeSignature(set2);
        auto sig3_64 = minHash64.computeSignature(set3);
        auto sig4_64 = minHash64.computeSignature(set4);

        auto sig1_256 = minHash256.computeSignature(set1);
        auto sig2_256 = minHash256.computeSignature(set2);

        // Calculate actual Jaccard indices for comparison
        auto calculateActualJaccard = [](const std::vector<int>& a,
                                         const std::vector<int>& b) {
            std::set<int> setA(a.begin(), a.end());
            std::set<int> setB(b.begin(), b.end());
            std::set<int> intersection, unionSet;

            std::set_intersection(
                setA.begin(), setA.end(), setB.begin(), setB.end(),
                std::inserter(intersection, intersection.begin()));
            std::set_union(setA.begin(), setA.end(), setB.begin(), setB.end(),
                           std::inserter(unionSet, unionSet.begin()));

            return static_cast<double>(intersection.size()) / unionSet.size();
        };

        // Compare estimated vs actual Jaccard indices
        std::cout << "Jaccard similarity comparison (64 hash functions):\n";

        double actual12 = calculateActualJaccard(set1, set2);
        double estimated12_64 = MinHash::jaccardIndex(sig1_64, sig2_64);
        std::cout << "  Set 1 vs Set 2: Actual=" << std::fixed
                  << std::setprecision(3) << actual12
                  << ", Estimated=" << estimated12_64
                  << " (error: " << std::abs(actual12 - estimated12_64)
                  << ")\n";

        double actual13 = calculateActualJaccard(set1, set3);
        double estimated13_64 = MinHash::jaccardIndex(sig1_64, sig3_64);
        std::cout << "  Set 1 vs Set 3: Actual=" << actual13
                  << ", Estimated=" << estimated13_64
                  << " (error: " << std::abs(actual13 - estimated13_64)
                  << ")\n";

        double actual14 = calculateActualJaccard(set1, set4);
        double estimated14_64 = MinHash::jaccardIndex(sig1_64, sig4_64);
        std::cout << "  Set 1 vs Set 4: Actual=" << actual14
                  << ", Estimated=" << estimated14_64
                  << " (error: " << std::abs(actual14 - estimated14_64)
                  << ")\n";

        // Compare accuracy with different numbers of hash functions
        std::cout << "\nAccuracy comparison (Set 1 vs Set 2):\n";
        double estimated12_256 = MinHash::jaccardIndex(sig1_256, sig2_256);
        std::cout << "  64 hash functions:  " << estimated12_64
                  << " (error: " << std::abs(actual12 - estimated12_64)
                  << ")\n";
        std::cout << "  256 hash functions: " << estimated12_256
                  << " (error: " << std::abs(actual12 - estimated12_256)
                  << ")\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic MinHash demonstration: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates Keccak-256 hash function
 */
void demonstrateKeccak256() {
    printHeader("Keccak-256 Cryptographic Hash Function");

    try {
        // Test various inputs
        std::vector<std::string> testInputs = {
            "Hello, World!",
            "",  // Empty string
            "The quick brown fox jumps over the lazy dog",
            "Keccak-256 is used in Ethereum blockchain",
            std::string(1000, 'A')  // Long string
        };

        for (const auto& input : testInputs) {
            const uint8_t* data =
                reinterpret_cast<const uint8_t*>(input.c_str());
            size_t length = input.length();

            // Compute Keccak-256 hash
            std::array<uint8_t, K_HASH_SIZE> hash =
                keccak256(std::span<const uint8_t>(data, length));

            std::cout << "Input: \""
                      << (input.length() > 50 ? input.substr(0, 47) + "..."
                                              : input)
                      << "\"\n";
            std::cout << "Keccak-256: ";
            for (const auto& byte : hash) {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(byte);
            }
            std::cout << std::dec << "\n";
            std::cout << "Length: " << K_HASH_SIZE << " bytes (256 bits)\n\n";
        }

        // Test binary data
        std::vector<uint8_t> binaryData = {0x00, 0x01, 0x02, 0x03,
                                           0xFF, 0xFE, 0xFD, 0xFC};
        std::array<uint8_t, K_HASH_SIZE> binaryHash = keccak256(
            std::span<const uint8_t>(binaryData.data(), binaryData.size()));

        std::cout << "Binary data test:\n";
        std::cout << "Input bytes: ";
        for (uint8_t byte : binaryData) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(byte) << " ";
        }
        std::cout << "\nKeccak-256: ";
        for (const auto& byte : binaryHash) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(byte);
        }
        std::cout << std::dec << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in Keccak-256 demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates performance characteristics of MinHash
 */
void demonstrateMinHashPerformance() {
    printHeader("MinHash Performance Analysis");

    try {
        // Test with different set sizes and hash function counts
        std::vector<size_t> setSizes = {100, 1000, 10000};
        std::vector<size_t> hashCounts = {64, 128, 256};

        std::random_device rd;
        std::mt19937 gen(rd());

        for (size_t setSize : setSizes) {
            std::cout << "Set size: " << setSize << " elements\n";

            // Generate random set
            std::uniform_int_distribution<> dis(1, setSize * 10);
            std::vector<int> testSet;
            for (size_t i = 0; i < setSize; ++i) {
                testSet.push_back(dis(gen));
            }

            for (size_t hashCount : hashCounts) {
                MinHash minHash(hashCount);

                // Measure signature computation time
                auto start = std::chrono::high_resolution_clock::now();
                auto signature = minHash.computeSignature(testSet);
                auto end = std::chrono::high_resolution_clock::now();

                auto duration =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        end - start);

                std::cout << "  " << hashCount
                          << " hash functions: " << duration.count() << " μs\n";
            }
            std::cout << "\n";
        }

        // Demonstrate space efficiency
        std::cout << "Space efficiency analysis:\n";
        size_t largeSetSize = 100000;
        MinHash minHash128(128);

        std::vector<int> largeSet;
        std::uniform_int_distribution<> largeDis(1, 1000000);
        for (size_t i = 0; i < largeSetSize; ++i) {
            largeSet.push_back(largeDis(gen));
        }

        auto signature = minHash128.computeSignature(largeSet);

        std::cout << "  Original set: " << largeSetSize << " integers ("
                  << (largeSetSize * sizeof(int)) << " bytes)\n";
        std::cout << "  MinHash signature: " << signature.size()
                  << " hash values ("
                  << (signature.size() * sizeof(signature[0])) << " bytes)\n";
        std::cout << "  Compression ratio: " << std::fixed
                  << std::setprecision(1)
                  << (static_cast<double>(largeSetSize * sizeof(int)) /
                      (signature.size() * sizeof(signature[0])))
                  << ":1\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in MinHash performance analysis: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive multi-hash utilities
 */
int main() {
    std::cout << "=== Atom Multi-Hash Utilities Comprehensive Example ===\n";
    std::cout
        << "Demonstrating MinHash, Keccak-256, and utility functions...\n";

    try {
        // Run all demonstration functions
        demonstrateHexstringConversion();
        demonstrateBasicMinHash();
        demonstrateKeccak256();
        demonstrateMinHashPerformance();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "All Multi-Hash Examples Completed Successfully\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "The multi-hash utilities provide:\n";
        std::cout << "  ✓ MinHash for efficient similarity estimation\n";
        std::cout << "  ✓ Keccak-256 cryptographic hash function\n";
        std::cout << "  ✓ Hexstring conversion utilities\n";
        std::cout << "  ✓ Jaccard index calculation for set similarity\n";
        std::cout << "  ✓ High performance with large datasets\n";
        std::cout << "  ✓ Space-efficient representation of large sets\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in multi-hash example: " << e.what()
                  << "\n";
        return 1;
    }
}
