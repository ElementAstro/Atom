/**
 * @file huffman.cpp
 * @brief Comprehensive example demonstrating Huffman compression algorithm
 * usage
 *
 * This example shows how to:
 * - Build Huffman trees from frequency data
 * - Generate optimal Huffman codes for compression
 * - Compress and decompress data using Huffman coding
 * - Serialize and deserialize Huffman trees
 * - Visualize tree structure and analyze compression efficiency
 * - Handle different data types and edge cases
 *
 * @author Atom Framework
 * @date 2024-12-19
 */

#include "atom/algorithm/huffman.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

using namespace atom::algorithm;

/**
 * @brief Demonstrates basic Huffman compression and decompression
 */
void demonstrateBasicHuffmanCompression() {
    std::cout << "\n=== Basic Huffman Compression Examples ===\n";

    try {
        // Example data with varying frequencies
        std::vector<unsigned char> data = {'a', 'b', 'a', 'c', 'a', 'b',
                                           'a', 'd', 'e', 'e', 'e', 'e',
                                           'a', 'a', 'a', 'a'};

        std::cout << "Original data: ";
        for (unsigned char byte : data) {
            std::cout << byte << " ";
        }
        std::cout << "\nOriginal size: " << data.size() << " bytes\n";

        // Step 1: Calculate frequencies
        std::unordered_map<unsigned char, int> frequencies;
        for (unsigned char byte : data) {
            frequencies[byte]++;
        }

        std::cout << "\nCharacter frequencies:\n";
        for (const auto& [byte, freq] : frequencies) {
            std::cout << "  '" << byte << "': " << freq << " times\n";
        }

        // Step 2: Create Huffman tree
        auto huffmanTreeRoot = createHuffmanTree(frequencies);
        if (!huffmanTreeRoot) {
            std::cerr << "Failed to create Huffman tree.\n";
            return;
        }

        // Step 3: Generate Huffman codes
        std::unordered_map<unsigned char, std::string> huffmanCodes;
        generateHuffmanCodes(huffmanTreeRoot.get(), "", huffmanCodes);

        std::cout << "\nGenerated Huffman codes:\n";
        for (const auto& [byte, code] : huffmanCodes) {
            std::cout << "  '" << byte << "': " << code << "\n";
        }

        // Step 4: Compress data
        std::string compressedData = compressData(data, huffmanCodes);
        std::cout << "\nCompressed data (binary): " << compressedData << "\n";
        std::cout << "Compressed size: " << compressedData.length()
                  << " bits\n";

        // Calculate compression ratio
        double originalBits = data.size() * 8;
        double compressedBits = compressedData.length();
        double compressionRatio =
            (originalBits - compressedBits) / originalBits * 100;
        std::cout << "Compression ratio: " << std::fixed << std::setprecision(2)
                  << compressionRatio << "%\n";

        // Step 5: Decompress data
        std::vector<unsigned char> decompressedData =
            decompressData(compressedData, huffmanTreeRoot.get());

        std::cout << "\nDecompressed data: ";
        for (unsigned char byte : decompressedData) {
            std::cout << byte << " ";
        }
        std::cout << "\n";

        // Verify integrity
        bool isIdentical = (data == decompressedData);
        std::cout << "Data integrity check: "
                  << (isIdentical ? "✓ PASSED" : "✗ FAILED") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic Huffman compression: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates Huffman tree serialization and deserialization
 */
void demonstrateTreeSerialization() {
    std::cout << "\n=== Huffman Tree Serialization ===\n";

    try {
        // Create test data
        std::vector<unsigned char> data = {'x', 'y', 'z', 'x', 'y', 'x'};

        // Build frequency map
        std::unordered_map<unsigned char, int> frequencies;
        for (unsigned char byte : data) {
            frequencies[byte]++;
        }

        // Create Huffman tree
        auto originalTree = createHuffmanTree(frequencies);
        if (!originalTree) {
            std::cerr << "Failed to create original Huffman tree.\n";
            return;
        }

        std::cout << "Original Huffman tree structure:\n";
        visualizeHuffmanTree(originalTree.get());

        // Serialize the tree
        std::string serializedTree = serializeTree(originalTree.get());
        std::cout << "\nSerialized tree: " << serializedTree << "\n";
        std::cout << "Serialized size: " << serializedTree.length()
                  << " characters\n";

        // Deserialize the tree
        size_t index = 0;
        auto deserializedTree = deserializeTree(serializedTree, index);
        if (!deserializedTree) {
            std::cerr << "Failed to deserialize Huffman tree.\n";
            return;
        }

        std::cout << "\nDeserialized Huffman tree structure:\n";
        visualizeHuffmanTree(deserializedTree.get());

        // Test that both trees produce the same codes
        std::unordered_map<unsigned char, std::string> originalCodes;
        std::unordered_map<unsigned char, std::string> deserializedCodes;

        generateHuffmanCodes(originalTree.get(), "", originalCodes);
        generateHuffmanCodes(deserializedTree.get(), "", deserializedCodes);

        std::cout << "\nCode comparison:\n";
        bool codesMatch = true;
        for (const auto& [byte, code] : originalCodes) {
            std::string deserializedCode = deserializedCodes[byte];
            std::cout << "  '" << byte << "': " << code << " vs "
                      << deserializedCode;
            if (code == deserializedCode) {
                std::cout << " ✓\n";
            } else {
                std::cout << " ✗\n";
                codesMatch = false;
            }
        }

        std::cout << "Serialization integrity: "
                  << (codesMatch ? "✓ PASSED" : "✗ FAILED") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in tree serialization: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates compression efficiency with different data patterns
 */
void demonstrateCompressionEfficiency() {
    std::cout << "\n=== Compression Efficiency Analysis ===\n";

    try {
        // Test different data patterns
        std::vector<std::pair<std::string, std::vector<unsigned char>>>
            testCases = {
                {"Uniform distribution",
                 {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'}},
                {"Skewed distribution",
                 {'a', 'a', 'a', 'a', 'a', 'b', 'c', 'd'}},
                {"Highly repetitive", {'x', 'x', 'x', 'x', 'x', 'x', 'y', 'z'}},
                {"Single character", {'m', 'm', 'm', 'm', 'm', 'm', 'm', 'm'}}};

        for (const auto& [description, data] : testCases) {
            std::cout << "\nTesting: " << description << "\n";
            std::cout << "Data: ";
            for (unsigned char byte : data) {
                std::cout << byte << " ";
            }
            std::cout << "\n";

            // Calculate frequencies
            std::unordered_map<unsigned char, int> frequencies;
            for (unsigned char byte : data) {
                frequencies[byte]++;
            }

            // Handle single character case
            if (frequencies.size() == 1) {
                std::cout
                    << "Single character detected - no compression needed\n";
                std::cout << "Compression ratio: 0% (no benefit)\n";
                continue;
            }

            // Create Huffman tree and codes
            auto tree = createHuffmanTree(frequencies);
            if (!tree) {
                std::cout << "Failed to create tree\n";
                continue;
            }

            std::unordered_map<unsigned char, std::string> codes;
            generateHuffmanCodes(tree.get(), "", codes);

            // Calculate compression metrics
            double originalBits = data.size() * 8;
            double compressedBits = 0;
            for (unsigned char byte : data) {
                compressedBits += codes[byte].length();
            }

            double compressionRatio =
                (originalBits - compressedBits) / originalBits * 100;
            double spaceSavings = originalBits - compressedBits;

            std::cout << "Original size: " << originalBits << " bits\n";
            std::cout << "Compressed size: " << compressedBits << " bits\n";
            std::cout << "Space savings: " << spaceSavings << " bits\n";
            std::cout << "Compression ratio: " << std::fixed
                      << std::setprecision(2) << compressionRatio << "%\n";

            // Show code lengths
            std::cout << "Code lengths: ";
            for (const auto& [byte, code] : codes) {
                std::cout << "'" << byte << "':" << code.length() << " ";
            }
            std::cout << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in compression efficiency analysis: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates performance characteristics
 */
void demonstratePerformanceCharacteristics() {
    std::cout << "\n=== Performance Characteristics ===\n";

    try {
        // Generate test data of different sizes
        std::vector<size_t> dataSizes = {100, 1000, 10000};
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis('a', 'z');

        for (size_t size : dataSizes) {
            std::cout << "\nTesting with " << size << " bytes:\n";

            // Generate random data
            std::vector<unsigned char> data;
            data.reserve(size);
            for (size_t i = 0; i < size; ++i) {
                data.push_back(static_cast<unsigned char>(dis(gen)));
            }

            // Measure tree construction time
            auto start = std::chrono::high_resolution_clock::now();

            std::unordered_map<unsigned char, int> frequencies;
            for (unsigned char byte : data) {
                frequencies[byte]++;
            }

            auto tree = createHuffmanTree(frequencies);
            auto end = std::chrono::high_resolution_clock::now();
            auto treeTime =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            if (!tree) {
                std::cout << "Failed to create tree\n";
                continue;
            }

            // Measure code generation time
            start = std::chrono::high_resolution_clock::now();
            std::unordered_map<unsigned char, std::string> codes;
            generateHuffmanCodes(tree.get(), "", codes);
            end = std::chrono::high_resolution_clock::now();
            auto codeTime =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            // Measure compression time
            start = std::chrono::high_resolution_clock::now();
            std::string compressed = compressData(data, codes);
            end = std::chrono::high_resolution_clock::now();
            auto compressTime =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            // Measure decompression time
            start = std::chrono::high_resolution_clock::now();
            auto decompressed = decompressData(compressed, tree.get());
            end = std::chrono::high_resolution_clock::now();
            auto decompressTime =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            std::cout << "  Tree construction: " << treeTime.count() << " μs\n";
            std::cout << "  Code generation: " << codeTime.count() << " μs\n";
            std::cout << "  Compression: " << compressTime.count() << " μs\n";
            std::cout << "  Decompression: " << decompressTime.count()
                      << " μs\n";
            std::cout
                << "  Total time: "
                << (treeTime + codeTime + compressTime + decompressTime).count()
                << " μs\n";

            // Verify integrity
            bool isIdentical = (data == decompressed);
            std::cout << "  Integrity check: "
                      << (isIdentical ? "✓ PASSED" : "✗ FAILED") << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in performance testing: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive Huffman usage
 */
int main() {
    std::cout
        << "=== Atom Huffman Compression Algorithm Comprehensive Example ===\n";
    std::cout << "Demonstrating Huffman coding compression capabilities...\n";

    try {
        // Run all demonstration functions
        demonstrateBasicHuffmanCompression();
        demonstrateTreeSerialization();
        demonstrateCompressionEfficiency();
        demonstratePerformanceCharacteristics();

        std::cout << "\n=== All Huffman Examples Completed Successfully ===\n";
        std::cout << "The Huffman algorithm provides:\n";
        std::cout
            << "  ✓ Optimal prefix-free compression for known frequencies\n";
        std::cout << "  ✓ Lossless compression and decompression\n";
        std::cout << "  ✓ Tree serialization for storage and transmission\n";
        std::cout
            << "  ✓ Efficient compression for skewed data distributions\n";
        std::cout << "  ✓ Fast compression and decompression operations\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in Huffman example: " << e.what()
                  << "\n";
        return 1;
    }
}
