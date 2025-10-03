/**
 * @file blob_edge_cases.cpp
 * @brief Comprehensive edge case testing for image blob operations
 *
 * This example demonstrates:
 * - Empty blob handling and validation
 * - Large blob memory management
 * - Memory constraint scenarios
 * - Error condition handling
 * - Boundary value testing
 * - Invalid input handling
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <limits>
#include <random>

#include "atom/image/core/image_blob.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Test empty blob operations and edge cases
 */
void testEmptyBlobOperations() {
    std::cout << "\n=== Empty Blob Operations ===\n";

    try {
        // Test default constructor
        blob emptyBlob;
        std::cout << "Empty blob created successfully\n";
        std::cout << "Empty check: " << (emptyBlob.isEmpty() ? "empty" : "not empty") << "\n";
        std::cout << "Size: " << emptyBlob.size() << " bytes\n";
        std::cout << "Dimensions: " << emptyBlob.getWidth() << "x" << emptyBlob.getHeight() << "\n";

        // Test operations on empty blob
        try {
            auto cloned = emptyBlob.clone();
            std::cout << "Empty blob cloning: SUCCESS\n";
        } catch (const std::exception& e) {
            std::cout << "Empty blob cloning failed: " << e.what() << "\n";
        }

        // Test serialization of empty blob
        try {
            auto serialized = emptyBlob.serialize();
            std::cout << "Empty blob serialization: SUCCESS (size: " << serialized.size() << ")\n";
            
            auto deserialized = blob::deserialize(serialized);
            std::cout << "Empty blob deserialization: SUCCESS\n";
        } catch (const std::exception& e) {
            std::cout << "Empty blob serialization/deserialization failed: " << e.what() << "\n";
        }

        // Test zero-size data creation
        std::vector<uint8_t> zeroData;
        try {
            blob zeroBlob(zeroData.data(), zeroData.size());
            std::cout << "Zero-size blob creation: SUCCESS\n";
        } catch (const std::exception& e) {
            std::cout << "Zero-size blob creation failed: " << e.what() << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in empty blob operations: " << e.what() << "\n";
    }
}

/**
 * @brief Test large blob memory management
 */
void testLargeBlobOperations() {
    std::cout << "\n=== Large Blob Operations ===\n";

    try {
        // Test progressively larger blobs
        std::vector<size_t> sizes = {
            1024 * 1024,        // 1MB
            10 * 1024 * 1024,   // 10MB
            50 * 1024 * 1024,   // 50MB
        };

        for (size_t size : sizes) {
            std::cout << "Testing blob size: " << (size / (1024 * 1024)) << "MB\n";
            
            auto start = high_resolution_clock::now();
            
            try {
                // Create large data buffer
                std::vector<uint8_t> largeData(size);
                
                // Fill with pattern to ensure memory is actually allocated
                for (size_t i = 0; i < size; ++i) {
                    largeData[i] = static_cast<uint8_t>(i % 256);
                }

                // Create blob from large data
                blob largeBlob(largeData.data(), largeData.size());
                
                auto end = high_resolution_clock::now();
                auto duration = duration_cast<milliseconds>(end - start);
                
                std::cout << "  Creation time: " << duration.count() << "ms\n";
                std::cout << "  Blob size: " << largeBlob.size() << " bytes\n";
                std::cout << "  Memory usage: " << (largeBlob.size() / (1024 * 1024)) << "MB\n";

                // Test operations on large blob
                start = high_resolution_clock::now();
                auto cloned = largeBlob.clone();
                end = high_resolution_clock::now();
                duration = duration_cast<milliseconds>(end - start);
                
                std::cout << "  Clone time: " << duration.count() << "ms\n";

            } catch (const std::bad_alloc& e) {
                std::cout << "  Memory allocation failed for " << (size / (1024 * 1024)) << "MB: " << e.what() << "\n";
                break;
            } catch (const std::exception& e) {
                std::cout << "  Error with " << (size / (1024 * 1024)) << "MB blob: " << e.what() << "\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in large blob operations: " << e.what() << "\n";
    }
}

/**
 * @brief Test memory constraint scenarios
 */
void testMemoryConstraints() {
    std::cout << "\n=== Memory Constraint Testing ===\n";

    try {
        // Test maximum size limits
        std::cout << "Testing maximum size limits...\n";
        
        // Test with maximum possible size_t value (will likely fail)
        try {
            size_t maxSize = std::numeric_limits<size_t>::max();
            std::cout << "Attempting to create blob with max size_t: " << maxSize << "\n";
            
            std::vector<uint8_t> impossibleData;
            impossibleData.reserve(maxSize);  // This should throw
            
            std::cout << "ERROR: Should not reach here!\n";
        } catch (const std::bad_alloc& e) {
            std::cout << "Expected failure for max size_t: " << e.what() << "\n";
        } catch (const std::exception& e) {
            std::cout << "Expected failure for max size_t: " << e.what() << "\n";
        }

        // Test memory alignment edge cases
        std::cout << "Testing memory alignment...\n";
        
        std::vector<uint8_t> testData(1024);
        blob alignmentBlob(testData.data(), testData.size());
        
        try {
            alignmentBlob.alignMemory(64);
            std::cout << "Memory alignment (64 bytes): SUCCESS\n";
        } catch (const std::exception& e) {
            std::cout << "Memory alignment failed: " << e.what() << "\n";
        }

        // Test with unusual alignment values
        std::vector<size_t> alignments = {1, 2, 4, 8, 16, 32, 128, 256, 512};
        for (size_t alignment : alignments) {
            try {
                blob testBlob(testData.data(), testData.size());
                testBlob.alignMemory(alignment);
                std::cout << "Alignment " << alignment << ": SUCCESS\n";
            } catch (const std::exception& e) {
                std::cout << "Alignment " << alignment << " failed: " << e.what() << "\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in memory constraint testing: " << e.what() << "\n";
    }
}

/**
 * @brief Test error condition handling
 */
void testErrorConditions() {
    std::cout << "\n=== Error Condition Testing ===\n";

    try {
        // Test null pointer handling
        std::cout << "Testing null pointer handling...\n";
        
        try {
            blob nullBlob(nullptr, 100);
            std::cout << "ERROR: Null pointer should have failed!\n";
        } catch (const std::exception& e) {
            std::cout << "Null pointer correctly rejected: " << e.what() << "\n";
        }

        // Test invalid size combinations
        std::cout << "Testing invalid size combinations...\n";
        
        std::vector<uint8_t> testData(100);
        try {
            blob invalidBlob(testData.data(), SIZE_MAX);
            std::cout << "ERROR: Invalid size should have failed!\n";
        } catch (const std::exception& e) {
            std::cout << "Invalid size correctly rejected: " << e.what() << "\n";
        }

        // Test corrupted serialization data
        std::cout << "Testing corrupted serialization data...\n";
        
        std::vector<uint8_t> corruptedData = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00};
        try {
            auto corrupted = blob::deserialize(corruptedData);
            std::cout << "ERROR: Corrupted data should have failed!\n";
        } catch (const std::exception& e) {
            std::cout << "Corrupted data correctly rejected: " << e.what() << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in error condition testing: " << e.what() << "\n";
    }
}

/**
 * @brief Test boundary value scenarios
 */
void testBoundaryValues() {
    std::cout << "\n=== Boundary Value Testing ===\n";

    try {
        // Test minimum valid sizes
        std::vector<size_t> minSizes = {1, 2, 3, 4, 8, 16};
        
        for (size_t size : minSizes) {
            try {
                std::vector<uint8_t> minData(size, 0x42);
                blob minBlob(minData.data(), minData.size());
                std::cout << "Minimum size " << size << ": SUCCESS\n";
            } catch (const std::exception& e) {
                std::cout << "Minimum size " << size << " failed: " << e.what() << "\n";
            }
        }

        // Test edge case dimensions
        std::cout << "Testing edge case dimensions...\n";
        
        // Single pixel images
        std::vector<uint8_t> singlePixel(3);  // RGB
        try {
            blob singlePixelBlob(singlePixel.data(), singlePixel.size());
            std::cout << "Single pixel blob: SUCCESS\n";
        } catch (const std::exception& e) {
            std::cout << "Single pixel blob failed: " << e.what() << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in boundary value testing: " << e.what() << "\n";
    }
}

int main() {
    std::cout << "=== Atom Image Blob Edge Cases Example ===\n";
    std::cout << "This example demonstrates edge case handling for image blob operations\n";

    // Run all edge case tests
    testEmptyBlobOperations();
    testLargeBlobOperations();
    testMemoryConstraints();
    testErrorConditions();
    testBoundaryValues();

    std::cout << "\n=== Blob edge cases example completed ===\n";
    std::cout << "\nNote: Some failures are expected and demonstrate proper error handling.\n";
    
    return 0;
}
