/**
 * @file basic_blob_operations.cpp
 * @brief Comprehensive example demonstrating basic image blob operations
 *
 * This example covers:
 * - Creating image blobs from different sources
 * - Basic blob operations (resize, rotate, flip, crop)
 * - Memory management with normal and fast blob modes
 * - Serialization and deserialization
 * - Error handling and validation
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
        // Create a simple 3-channel RGB image (100x100)
        blob<uint8_t> img(100, 100, 3);
        std::cout << "Created RGB blob: " << img.cols() << "x" << img.rows()
                  << " with " << img.channels() << " channels\n";

        // Fill with gradient pattern
        for (int y = 0; y < img.rows(); ++y) {
            for (int x = 0; x < img.cols(); ++x) {
                img.at(y, x, 0) =
                    static_cast<uint8_t>(x * 255 / img.cols());  // Red gradient
                img.at(y, x, 1) = static_cast<uint8_t>(
                    y * 255 / img.rows());  // Green gradient
                img.at(y, x, 2) = static_cast<uint8_t>(
                    (x + y) * 255 /
                    (img.cols() + img.rows()));  // Blue gradient
            }
        }

        // Basic operations
        std::cout << "Original size: " << img.cols() << "x" << img.rows()
                  << "\n";

        // Resize operation
        img.resize(150, 120);
        std::cout << "After resize: " << img.cols() << "x" << img.rows()
                  << "\n";

        // Rotation (90 degrees)
        img.rotate(90.0);
        std::cout << "After 90° rotation: " << img.cols() << "x" << img.rows()
                  << "\n";

        // Flip operations
        img.flip(0);  // Vertical flip
        std::cout << "Applied vertical flip\n";

        img.flip(1);  // Horizontal flip
        std::cout << "Applied horizontal flip\n";

        // Crop operation
        auto cropped = img.crop(10, 10, 50, 50);
        std::cout << "Cropped region: " << cropped.cols() << "x"
                  << cropped.rows() << "\n";

        // Memory usage information
        std::cout << "Original image memory: " << img.size() << " bytes\n";
        std::cout << "Cropped image memory: " << cropped.size() << " bytes\n";

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
        fast_blob<uint8_t> fastImg(imageData.data(), 480, 640, 3);
        std::cout << "Created fast blob view: " << fastImg.cols() << "x"
                  << fastImg.rows() << " with " << fastImg.channels()
                  << " channels\n";

        // Fast blob operations (read-only)
        std::cout << "Fast blob memory size: " << fastImg.size() << " bytes\n";
        std::cout << "Sample pixel at (100, 100): R="
                  << static_cast<int>(fastImg.at(100, 100, 0))
                  << " G=" << static_cast<int>(fastImg.at(100, 100, 1))
                  << " B=" << static_cast<int>(fastImg.at(100, 100, 2)) << "\n";

        // Performance comparison
        auto start = high_resolution_clock::now();

        // Create normal blob (with memory copy)
        blob<uint8_t> normalImg(imageData.data(), 480, 640, 3);

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        std::cout << "Normal blob creation time: " << duration.count()
                  << " microseconds\n";

        start = high_resolution_clock::now();

        // Create fast blob (no memory copy)
        fast_blob<uint8_t> fastImg2(imageData.data(), 480, 640, 3);

        end = high_resolution_clock::now();
        duration = duration_cast<microseconds>(end - start);
        std::cout << "Fast blob creation time: " << duration.count()
                  << " microseconds\n";

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
        // Create and populate an image
        blob<uint8_t> originalImg(64, 64, 3);

        // Create a simple pattern
        for (int y = 0; y < originalImg.rows(); ++y) {
            for (int x = 0; x < originalImg.cols(); ++x) {
                originalImg.at(y, x, 0) = static_cast<uint8_t>((x + y) % 256);
                originalImg.at(y, x, 1) = static_cast<uint8_t>((x * y) % 256);
                originalImg.at(y, x, 2) = static_cast<uint8_t>((x ^ y) % 256);
            }
        }

        std::cout << "Original image: " << originalImg.cols() << "x"
                  << originalImg.rows() << " with " << originalImg.channels()
                  << " channels\n";

        // Serialize the image
        auto serializedData = originalImg.serialize();
        std::cout << "Serialized data size: " << serializedData.size()
                  << " bytes\n";

        // Deserialize the image
        auto restoredImg = blob<uint8_t>::deserialize(serializedData);
        std::cout << "Restored image: " << restoredImg.cols() << "x"
                  << restoredImg.rows() << " with " << restoredImg.channels()
                  << " channels\n";

        // Verify data integrity
        bool dataMatches = true;
        for (int y = 0; y < originalImg.rows() && dataMatches; ++y) {
            for (int x = 0; x < originalImg.cols() && dataMatches; ++x) {
                for (int c = 0; c < originalImg.channels() && dataMatches;
                     ++c) {
                    if (originalImg.at(y, x, c) != restoredImg.at(y, x, c)) {
                        dataMatches = false;
                    }
                }
            }
        }

        std::cout << "Data integrity check: "
                  << (dataMatches ? "PASSED" : "FAILED") << "\n";

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
