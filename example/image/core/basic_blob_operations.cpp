/**
 * @file basic_blob_operations.cpp
 * @brief Basic demonstration of image blob operations
 *
 * This example demonstrates:
 * - Creating and using image blobs
 * - Basic operations with available API
 * - Memory management
 * - Serialization
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include "atom/image/core/image_blob.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Demonstrate basic blob creation and operations
 */
void demonstrateBasicOperations() {
    std::cout << "\n=== Basic Blob Operations ===\n";

    try {
        // Create a basic blob
        blob img;
        std::cout << "Created blob: " << img.getCols() << "x" << img.getRows()
                  << " with " << img.getChannels() << " channels\n";

        // Create test data
        std::vector<uint8_t> testData(100 * 100 * 3);
        for (size_t i = 0; i < testData.size(); i += 3) {
            testData[i] = static_cast<uint8_t>(i % 256);     // Red
            testData[i + 1] = static_cast<uint8_t>((i / 2) % 256); // Green
            testData[i + 2] = static_cast<uint8_t>((i / 3) % 256); // Blue
        }

        // Create blob from data
        blob dataImg(testData.data(), testData.size());
        std::cout << "Created blob from data with " << dataImg.size() << " bytes\n";

        // Basic blob operations
        std::cout << "Empty check: " << (dataImg.isEmpty() ? "empty" : "not empty") << "\n";
        std::cout << "Width: " << dataImg.getWidth() << "\n";
        std::cout << "Height: " << dataImg.getHeight() << "\n";

        // Fill operation
        blob fillBlob;
        fillBlob.fill(std::byte{128});
        std::cout << "Filled blob with value 128\n";

        // Memory alignment
        dataImg.alignMemory(64);
        std::cout << "Applied memory alignment\n";

        // Clone operation
        auto clonedImg = dataImg.clone();
        std::cout << "Cloned blob: " << clonedImg.size() << " bytes\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic operations: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate fast blob for view-only operations
 */
void demonstrateFastBlob() {
    std::cout << "\n=== Fast Blob Operations ===\n";

    try {
        // Create original data
        std::vector<uint8_t> imageData(640 * 480 * 3);

        // Fill with test pattern
        for (size_t i = 0; i < imageData.size(); i += 3) {
            imageData[i] = static_cast<uint8_t>(i % 256);            // Red
            imageData[i + 1] = static_cast<uint8_t>((i / 3) % 256);  // Green
            imageData[i + 2] = static_cast<uint8_t>((i / 6) % 256);  // Blue
        }

        // Create fast blob (view-only, no memory copy)
        fast_blob fastImg(imageData.data(), imageData.size());
        std::cout << "Created fast blob view with " << fastImg.size() << " bytes\n";

        // Basic fast blob operations (read-only)
        std::cout << "Fast blob memory size: " << fastImg.size() << " bytes\n";
        std::cout << "Fast blob dimensions: " << fastImg.getWidth() << "x" << fastImg.getHeight() << "\n";
        std::cout << "Fast blob channels: " << fastImg.getChannels() << "\n";
        std::cout << "Empty check: " << (fastImg.isEmpty() ? "empty" : "not empty") << "\n";

        // Performance comparison
        auto start = high_resolution_clock::now();

        // Create normal blob (with memory copy)
        blob normalImg(imageData.data(), imageData.size());

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        std::cout << "Normal blob creation time: " << duration.count()
                  << " microseconds\n";

        start = high_resolution_clock::now();

        // Create another fast blob (no memory copy)
        fast_blob fastImg2(imageData.data(), imageData.size());

        end = high_resolution_clock::now();
        duration = duration_cast<microseconds>(end - start);
        std::cout << "Fast blob creation time: " << duration.count()
                  << " microseconds\n";

        // Demonstrate slice operation
        auto slice = fastImg.slice(100, 200);
        std::cout << "Created slice with " << slice.size() << " bytes\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in fast blob operations: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate serialization and deserialization
 */
void demonstrateSerialization() {
    std::cout << "\n=== Serialization Operations ===\n";

    try {
        // Create test data
        std::vector<uint8_t> testData(64 * 64 * 3);
        for (size_t i = 0; i < testData.size(); ++i) {
            testData[i] = static_cast<uint8_t>((i + i/3 + i/64) % 256);
        }

        // Create and populate an image blob
        blob originalImg(testData.data(), testData.size());

        std::cout << "Original image blob: " << originalImg.getCols() << "x"
                  << originalImg.getRows() << " with " << originalImg.getChannels()
                  << " channels\n";
        std::cout << "Original size: " << originalImg.size() << " bytes\n";

        // Serialize the image
        auto serializedData = originalImg.serialize();
        std::cout << "Serialized data size: " << serializedData.size()
                  << " bytes\n";

        // Deserialize the image
        auto restoredImg = blob::deserialize(serializedData);
        std::cout << "Restored image blob: " << restoredImg.getCols() << "x"
                  << restoredImg.getRows() << " with " << restoredImg.getChannels()
                  << " channels\n";
        std::cout << "Restored size: " << restoredImg.size() << " bytes\n";

        // Verify data integrity by comparing sizes and a few bytes
        bool dataMatches = (originalImg.size() == restoredImg.size());
        if (dataMatches && originalImg.size() > 0) {
            // Compare first, middle, and last bytes
            size_t checkPoints[] = {0, originalImg.size() / 2, originalImg.size() - 1};
            for (size_t point : checkPoints) {
                if (originalImg[point] != restoredImg[point]) {
                    dataMatches = false;
                    break;
                }
            }
        }

        std::cout << "Data integrity check: "
                  << (dataMatches ? "PASSED" : "FAILED") << "\n";

        // Demonstrate compression
        auto compressed = originalImg.compress();
        std::cout << "Compressed size: " << compressed.size() << " bytes\n";

        auto decompressed = compressed.decompress();
        std::cout << "Decompressed size: " << decompressed.size() << " bytes\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in serialization: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate different data types and memory alignment
 */
void demonstrateDataTypes() {
    std::cout << "\n=== Different Data Types ===\n";

    try {
        // 8-bit unsigned integer (most common)
        blob<uint8_t> img8u(100, 100, 3);
        std::cout << "8-bit unsigned blob: " << sizeof(uint8_t)
                  << " bytes per pixel component\n";

        // 16-bit unsigned integer (for high dynamic range)
        blob<uint16_t> img16u(100, 100, 3);
        std::cout << "16-bit unsigned blob: " << sizeof(uint16_t)
                  << " bytes per pixel component\n";

        // 32-bit floating point (for scientific applications)
        blob<float> imgFloat(100, 100, 3);
        std::cout << "32-bit float blob: " << sizeof(float)
                  << " bytes per pixel component\n";

        // Fill float image with normalized values
        for (int y = 0; y < imgFloat.rows(); ++y) {
            for (int x = 0; x < imgFloat.cols(); ++x) {
                imgFloat.at(y, x, 0) = static_cast<float>(x) / imgFloat.cols();
                imgFloat.at(y, x, 1) = static_cast<float>(y) / imgFloat.rows();
                imgFloat.at(y, x, 2) = 0.5f;
            }
        }

        // Memory usage comparison
        std::cout << "Memory usage comparison for 100x100x3 image:\n";
        std::cout << "  8-bit:  " << img8u.size() << " bytes\n";
        std::cout << "  16-bit: " << img16u.size() << " bytes\n";
        std::cout << "  Float:  " << imgFloat.size() << " bytes\n";

        // Demonstrate type conversion (conceptual)
        std::cout << "Sample float pixel values: ";
        std::cout << "R=" << imgFloat.at(50, 50, 0)
                  << " G=" << imgFloat.at(50, 50, 1)
                  << " B=" << imgFloat.at(50, 50, 2) << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in data type demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate error handling and edge cases
 */
void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling ===\n";

    try {
        // Test invalid dimensions
        try {
            blob<uint8_t> invalidImg(-10, 100, 3);
            std::cout << "ERROR: Should have thrown exception for negative "
                         "dimensions\n";
        } catch (const std::exception& e) {
            std::cout << "Correctly caught invalid dimensions: " << e.what()
                      << "\n";
        }

        // Test out-of-bounds access
        blob<uint8_t> testImg(10, 10, 3);
        try {
            auto pixel = testImg.at(15, 15, 0);  // Out of bounds
            std::cout << "ERROR: Should have thrown exception for "
                         "out-of-bounds access\n";
        } catch (const std::exception& e) {
            std::cout << "Correctly caught out-of-bounds access: " << e.what()
                      << "\n";
        }

        // Test invalid crop parameters
        try {
            auto cropped = testImg.crop(-5, -5, 20, 20);  // Invalid crop region
            std::cout
                << "ERROR: Should have thrown exception for invalid crop\n";
        } catch (const std::exception& e) {
            std::cout << "Correctly caught invalid crop parameters: "
                      << e.what() << "\n";
        }

        std::cout << "Error handling tests completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error in error handling demo: " << e.what()
                  << "\n";
    }
}

int main() {
    std::cout << "=== Atom Image Core Blob Operations Example ===\n";
    std::cout
        << "This example demonstrates comprehensive image blob functionality\n";

    // Run all demonstrations
    demonstrateBasicOperations();
    demonstrateFastBlob();
    demonstrateSerialization();
    demonstrateDataTypes();
    demonstrateErrorHandling();

    std::cout << "\n=== Example completed successfully ===\n";
    return 0;
}
