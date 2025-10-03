/**
 * @file boundary_conditions_demo.cpp
 * @brief Boundary conditions and edge cases demonstration
 *
 * This example demonstrates:
 * - Zero-size images and empty data handling
 * - Maximum size limits and memory constraints
 * - Extreme parameter values and edge cases
 * - Numerical precision and overflow handling
 * - Invalid input validation and sanitization
 * - Performance under extreme conditions
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <limits>
#include <cmath>
#include <algorithm>
#include <random>
#include <iomanip>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/processing/image_processor.hpp"
#include "atom/image/core/image_metadata.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Test result structure for boundary condition tests
 */
struct BoundaryTestResult {
    std::string testName;
    bool passed;
    std::string errorMessage;
    std::chrono::microseconds executionTime;
    size_t memoryUsed;
    
    BoundaryTestResult(const std::string& name) 
        : testName(name), passed(false), memoryUsed(0) {}
};

/**
 * @brief Boundary condition test runner
 */
class BoundaryTester {
private:
    std::vector<BoundaryTestResult> results_;

public:
    template<typename TestFunc>
    void runTest(const std::string& testName, TestFunc&& testFunc) {
        BoundaryTestResult result(testName);
        
        std::cout << "Running test: " << testName << " ... ";
        
        auto start = steady_clock::now();
        
        try {
            testFunc();
            result.passed = true;
            std::cout << "PASSED";
            
        } catch (const std::exception& e) {
            result.passed = false;
            result.errorMessage = e.what();
            std::cout << "FAILED (" << e.what() << ")";
            
        } catch (...) {
            result.passed = false;
            result.errorMessage = "Unknown exception";
            std::cout << "FAILED (Unknown exception)";
        }
        
        result.executionTime = duration_cast<microseconds>(steady_clock::now() - start);
        std::cout << " [" << result.executionTime.count() << " μs]\n";
        
        results_.push_back(result);
    }
    
    void printSummary() const {
        size_t passed = 0;
        size_t failed = 0;
        
        for (const auto& result : results_) {
            if (result.passed) {
                passed++;
            } else {
                failed++;
            }
        }
        
        std::cout << "\n=== Test Summary ===\n";
        std::cout << "Total tests: " << results_.size() << "\n";
        std::cout << "Passed: " << passed << "\n";
        std::cout << "Failed: " << failed << "\n";
        std::cout << "Success rate: " << std::fixed << std::setprecision(1) 
                 << (passed * 100.0 / results_.size()) << "%\n";
        
        if (failed > 0) {
            std::cout << "\nFailed tests:\n";
            for (const auto& result : results_) {
                if (!result.passed) {
                    std::cout << "  " << result.testName << ": " << result.errorMessage << "\n";
                }
            }
        }
    }
};

/**
 * @brief Test zero-size and empty data conditions
 */
void testZeroSizeConditions() {
    std::cout << "\n=== Zero-Size and Empty Data Tests ===\n";
    
    BoundaryTester tester;
    
    // Test 1: Empty blob creation
    tester.runTest("Empty blob creation", []() {
        blob emptyBlob;
        if (!emptyBlob.empty()) {
            throw std::runtime_error("Empty blob should report as empty");
        }
        if (emptyBlob.size() != 0) {
            throw std::runtime_error("Empty blob size should be 0");
        }
        if (emptyBlob.data() != nullptr) {
            throw std::runtime_error("Empty blob data pointer should be null");
        }
    });
    
    // Test 2: Zero-size blob creation
    tester.runTest("Zero-size blob with null data", []() {
        blob zeroBlob(nullptr, 0);
        if (!zeroBlob.empty()) {
            throw std::runtime_error("Zero-size blob should be empty");
        }
    });
    
    // Test 3: Operations on empty blobs
    tester.runTest("Operations on empty blob", []() {
        blob emptyBlob;
        ImageProcessor processor;
        
        // These operations should handle empty input gracefully
        try {
            auto result = processor.applyFilter(emptyBlob, FilterType::GAUSSIAN_BLUR, {});
            if (!result.empty()) {
                throw std::runtime_error("Processing empty blob should return empty result");
            }
        } catch (const std::exception& e) {
            // This is also acceptable - operation should fail gracefully
            std::string msg = e.what();
            if (msg.find("empty") == std::string::npos && msg.find("invalid") == std::string::npos) {
                throw; // Re-throw if not a proper empty data error
            }
        }
    });
    
    // Test 4: Zero-width or zero-height images
    tester.runTest("Zero-width image", []() {
        std::vector<uint8_t> data(100, 128); // Some data
        
        // Try to create image with zero width
        ImageMetadata metadata;
        metadata.width = 0;
        metadata.height = 100;
        metadata.channels = 1;
        
        // This should either work (creating empty result) or fail gracefully
        blob image(data.data(), data.size());
        // The actual validation depends on the implementation
    });
    
    // Test 5: Single pixel image
    tester.runTest("Single pixel image", []() {
        std::vector<uint8_t> data = {255}; // Single white pixel
        blob singlePixel(data.data(), data.size());
        
        ImageProcessor processor;
        
        // Operations on single pixel should work or fail gracefully
        try {
            auto result = processor.applyFilter(singlePixel, FilterType::GAUSSIAN_BLUR, 
                                              {{"kernel_size", 1}});
            // Should either work or throw appropriate exception
        } catch (const std::exception& e) {
            // Acceptable if it's a proper size-related error
            std::string msg = e.what();
            if (msg.find("size") == std::string::npos && msg.find("dimension") == std::string::npos) {
                throw;
            }
        }
    });
    
    tester.printSummary();
}

/**
 * @brief Test maximum size limits and memory constraints
 */
void testMaximumSizeLimits() {
    std::cout << "\n=== Maximum Size Limits Tests ===\n";
    
    BoundaryTester tester;
    
    // Test 1: Large blob creation
    tester.runTest("Large blob creation (10MB)", []() {
        const size_t largeSize = 10 * 1024 * 1024; // 10MB
        std::vector<uint8_t> largeData(largeSize, 128);
        
        blob largeBlob(largeData.data(), largeData.size());
        
        if (largeBlob.size() != largeSize) {
            throw std::runtime_error("Large blob size mismatch");
        }
    });
    
    // Test 2: Very large blob creation (may fail due to memory)
    tester.runTest("Very large blob creation (100MB)", []() {
        const size_t veryLargeSize = 100 * 1024 * 1024; // 100MB
        
        try {
            std::vector<uint8_t> veryLargeData(veryLargeSize, 64);
            blob veryLargeBlob(veryLargeData.data(), veryLargeData.size());
            
            if (veryLargeBlob.size() != veryLargeSize) {
                throw std::runtime_error("Very large blob size mismatch");
            }
            
        } catch (const std::bad_alloc& e) {
            // This is acceptable - system may not have enough memory
            throw std::runtime_error("Insufficient memory for very large blob (expected)");
        }
    });
    
    // Test 3: Maximum theoretical size
    tester.runTest("Maximum size_t value", []() {
        const size_t maxSize = std::numeric_limits<size_t>::max();
        
        // This should fail gracefully
        try {
            std::vector<uint8_t> impossibleData(maxSize);
            throw std::runtime_error("Should not be able to allocate max size_t bytes");
            
        } catch (const std::bad_alloc& e) {
            // Expected behavior
        } catch (const std::length_error& e) {
            // Also acceptable
        }
    });
    
    // Test 4: Large image dimensions
    tester.runTest("Large image dimensions", []() {
        const uint32_t largeWidth = 10000;
        const uint32_t largeHeight = 10000;
        const uint32_t channels = 3;
        
        size_t requiredSize = static_cast<size_t>(largeWidth) * largeHeight * channels;
        
        if (requiredSize > 1024 * 1024 * 1024) { // > 1GB
            throw std::runtime_error("Image too large for test (expected)");
        }
        
        std::vector<uint8_t> imageData(requiredSize, 100);
        blob largeImage(imageData.data(), imageData.size());
        
        // Try basic operations
        ImageProcessor processor;
        // Most operations should handle large images or fail gracefully
    });
    
    // Test 5: Integer overflow in size calculations
    tester.runTest("Integer overflow in size calculation", []() {
        const uint32_t width = std::numeric_limits<uint32_t>::max() / 2;
        const uint32_t height = 3;
        const uint32_t channels = 4;
        
        // This multiplication should overflow
        uint64_t size64 = static_cast<uint64_t>(width) * height * channels;
        
        if (size64 > std::numeric_limits<size_t>::max()) {
            // Expected overflow condition
            return;
        }
        
        // If no overflow, the size is still too large
        if (size64 > 1024 * 1024 * 1024) { // > 1GB
            return; // Expected
        }
        
        throw std::runtime_error("Size calculation should have overflowed or been too large");
    });
    
    tester.printSummary();
}

/**
 * @brief Test extreme parameter values
 */
void testExtremeParameters() {
    std::cout << "\n=== Extreme Parameter Values Tests ===\n";
    
    BoundaryTester tester;
    
    // Create test image
    std::vector<uint8_t> testData(100 * 100 * 3, 128);
    blob testImage(testData.data(), testData.size());
    
    ImageProcessor processor;
    
    // Test 1: Extreme filter parameters
    tester.runTest("Extreme Gaussian blur sigma", [&]() {
        // Very large sigma
        try {
            auto result = processor.applyFilter(testImage, FilterType::GAUSSIAN_BLUR, 
                                              {{"sigma", 1000.0}});
            // Should either work or fail gracefully
        } catch (const std::exception& e) {
            std::string msg = e.what();
            if (msg.find("parameter") == std::string::npos && 
                msg.find("range") == std::string::npos &&
                msg.find("invalid") == std::string::npos) {
                throw;
            }
        }
        
        // Zero sigma
        try {
            auto result = processor.applyFilter(testImage, FilterType::GAUSSIAN_BLUR, 
                                              {{"sigma", 0.0}});
        } catch (const std::exception& e) {
            // Expected for zero sigma
        }
        
        // Negative sigma
        try {
            auto result = processor.applyFilter(testImage, FilterType::GAUSSIAN_BLUR, 
                                              {{"sigma", -1.0}});
        } catch (const std::exception& e) {
            // Expected for negative sigma
        }
    });
    
    // Test 2: Extreme kernel sizes
    tester.runTest("Extreme kernel sizes", [&]() {
        // Very large kernel
        try {
            auto result = processor.applyFilter(testImage, FilterType::GAUSSIAN_BLUR, 
                                              {{"kernel_size", 999}});
        } catch (const std::exception& e) {
            // May fail due to size constraints
        }
        
        // Zero kernel size
        try {
            auto result = processor.applyFilter(testImage, FilterType::GAUSSIAN_BLUR, 
                                              {{"kernel_size", 0}});
        } catch (const std::exception& e) {
            // Expected for zero kernel
        }
        
        // Even kernel size (should be odd)
        try {
            auto result = processor.applyFilter(testImage, FilterType::GAUSSIAN_BLUR, 
                                              {{"kernel_size", 4}});
        } catch (const std::exception& e) {
            // May be rejected for even size
        }
    });
    
    // Test 3: Extreme numerical values
    tester.runTest("Extreme numerical values", [&]() {
        // Test with infinity
        try {
            auto result = processor.applyFilter(testImage, FilterType::GAUSSIAN_BLUR, 
                                              {{"sigma", std::numeric_limits<double>::infinity()}});
        } catch (const std::exception& e) {
            // Expected to fail
        }
        
        // Test with NaN
        try {
            auto result = processor.applyFilter(testImage, FilterType::GAUSSIAN_BLUR, 
                                              {{"sigma", std::numeric_limits<double>::quiet_NaN()}});
        } catch (const std::exception& e) {
            // Expected to fail
        }
        
        // Test with very small values
        try {
            auto result = processor.applyFilter(testImage, FilterType::GAUSSIAN_BLUR, 
                                              {{"sigma", std::numeric_limits<double>::min()}});
        } catch (const std::exception& e) {
            // May fail due to precision issues
        }
    });
    
    // Test 4: Extreme brightness/contrast values
    tester.runTest("Extreme brightness/contrast", [&]() {
        // Maximum brightness
        try {
            auto result = processor.adjustBrightness(testImage, 255.0);
            // Should clamp to valid range
        } catch (const std::exception& e) {
            // May reject extreme values
        }
        
        // Minimum brightness
        try {
            auto result = processor.adjustBrightness(testImage, -255.0);
        } catch (const std::exception& e) {
            // May reject extreme values
        }
        
        // Extreme contrast
        try {
            auto result = processor.adjustContrast(testImage, 100.0);
        } catch (const std::exception& e) {
            // May reject extreme values
        }
    });
    
    // Test 5: Extreme rotation angles
    tester.runTest("Extreme rotation angles", [&]() {
        // Very large angle
        try {
            auto result = processor.rotate(testImage, 3600.0); // 10 full rotations
            // Should normalize angle
        } catch (const std::exception& e) {
            // May reject or normalize
        }
        
        // Negative angle
        try {
            auto result = processor.rotate(testImage, -720.0); // -2 full rotations
        } catch (const std::exception& e) {
            // May reject or normalize
        }
    });
    
    tester.printSummary();
}

/**
 * @brief Test numerical precision and overflow conditions
 */
void testNumericalPrecision() {
    std::cout << "\n=== Numerical Precision and Overflow Tests ===\n";
    
    BoundaryTester tester;
    
    // Test 1: Floating point precision
    tester.runTest("Floating point precision", []() {
        // Test very small differences
        double a = 1.0;
        double b = 1.0 + std::numeric_limits<double>::epsilon();
        
        if (a == b) {
            throw std::runtime_error("Should detect epsilon difference");
        }
        
        // Test precision loss
        double large = 1e15;
        double small = 1.0;
        double sum = large + small;
        
        if (sum - large != small) {
            // Expected precision loss with large numbers
        }
    });
    
    // Test 2: Integer overflow
    tester.runTest("Integer overflow detection", []() {
        // Test addition overflow
        uint32_t max32 = std::numeric_limits<uint32_t>::max();
        
        // This should overflow
        uint64_t result = static_cast<uint64_t>(max32) + 1;
        
        if (result <= max32) {
            throw std::runtime_error("Overflow detection failed");
        }
        
        // Test multiplication overflow
        uint32_t large = std::numeric_limits<uint32_t>::max() / 2;
        uint64_t product = static_cast<uint64_t>(large) * 3;
        
        if (product <= std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Multiplication overflow not detected");
        }
    });
    
    // Test 3: Division by zero and near-zero
    tester.runTest("Division by zero handling", []() {
        double zero = 0.0;
        double nearZero = std::numeric_limits<double>::min();
        
        // Division by zero should produce infinity
        double result = 1.0 / zero;
        if (!std::isinf(result)) {
            throw std::runtime_error("Division by zero should produce infinity");
        }
        
        // Division by near-zero should produce very large number
        double result2 = 1.0 / nearZero;
        if (!std::isfinite(result2) || result2 < 1e100) {
            throw std::runtime_error("Division by near-zero should produce large finite number");
        }
    });
    
    // Test 4: Trigonometric function edge cases
    tester.runTest("Trigonometric edge cases", []() {
        // Test with very large angles
        double largeAngle = 1e10;
        double sinResult = std::sin(largeAngle);
        
        if (!std::isfinite(sinResult) || std::abs(sinResult) > 1.0) {
            throw std::runtime_error("Sin of large angle should be finite and in [-1,1]");
        }
        
        // Test with infinity
        double infAngle = std::numeric_limits<double>::infinity();
        double sinInf = std::sin(infAngle);
        
        if (!std::isnan(sinInf)) {
            throw std::runtime_error("Sin of infinity should be NaN");
        }
    });
    
    // Test 5: Accumulation errors
    tester.runTest("Accumulation errors", []() {
        // Test summing many small values
        double sum = 0.0;
        double small = 1e-10;
        int iterations = 1000000;
        
        for (int i = 0; i < iterations; ++i) {
            sum += small;
        }
        
        double expected = small * iterations;
        double error = std::abs(sum - expected) / expected;
        
        if (error > 1e-6) { // Allow some accumulation error
            throw std::runtime_error("Accumulation error too large: " + std::to_string(error));
        }
    });
    
    tester.printSummary();
}

/**
 * @brief Test invalid input validation
 */
void testInvalidInputValidation() {
    std::cout << "\n=== Invalid Input Validation Tests ===\n";
    
    BoundaryTester tester;
    
    // Test 1: Null pointer handling
    tester.runTest("Null pointer handling", []() {
        try {
            blob nullBlob(nullptr, 100); // Non-zero size with null data
            throw std::runtime_error("Should reject null pointer with non-zero size");
        } catch (const std::exception& e) {
            // Expected to fail
            std::string msg = e.what();
            if (msg.find("null") == std::string::npos && msg.find("invalid") == std::string::npos) {
                throw std::runtime_error("Should provide meaningful error for null pointer");
            }
        }
    });
    
    // Test 2: Mismatched size and data
    tester.runTest("Mismatched size and data", []() {
        std::vector<uint8_t> data(100, 128);
        
        // Try to create blob with wrong size
        try {
            blob mismatchedBlob(data.data(), 200); // Size larger than actual data
            // This might work (accessing beyond bounds) or fail
            // The behavior depends on implementation
        } catch (const std::exception& e) {
            // Acceptable if validation is performed
        }
    });
    
    // Test 3: Invalid image dimensions
    tester.runTest("Invalid image dimensions", []() {
        std::vector<uint8_t> data(100, 128);
        blob testBlob(data.data(), data.size());
        
        ImageProcessor processor;
        
        // Try operations with invalid dimensions
        try {
            // This depends on how the processor validates dimensions
            auto result = processor.resize(testBlob, 0, 100); // Zero width
        } catch (const std::exception& e) {
            // Expected for invalid dimensions
        }
        
        try {
            auto result = processor.resize(testBlob, 100, 0); // Zero height
        } catch (const std::exception& e) {
            // Expected for invalid dimensions
        }
    });
    
    // Test 4: Invalid parameter combinations
    tester.runTest("Invalid parameter combinations", []() {
        std::vector<uint8_t> data(100 * 100 * 3, 128);
        blob testImage(data.data(), data.size());
        
        ImageProcessor processor;
        
        // Test conflicting parameters
        try {
            auto result = processor.applyFilter(testImage, FilterType::GAUSSIAN_BLUR, 
                                              {{"sigma", 5.0}, {"kernel_size", 3}}); // Conflicting params
            // May work (one parameter takes precedence) or fail
        } catch (const std::exception& e) {
            // Acceptable if validation rejects conflicting parameters
        }
    });
    
    // Test 5: Corrupted data patterns
    tester.runTest("Corrupted data patterns", []() {
        // Create data with specific corruption patterns
        std::vector<uint8_t> corruptedData(1000);
        
        // Pattern 1: All same value (may indicate corruption)
        std::fill(corruptedData.begin(), corruptedData.end(), 0xFF);
        blob pattern1(corruptedData.data(), corruptedData.size());
        
        // Pattern 2: Alternating pattern (may indicate corruption)
        for (size_t i = 0; i < corruptedData.size(); ++i) {
            corruptedData[i] = (i % 2) ? 0xFF : 0x00;
        }
        blob pattern2(corruptedData.data(), corruptedData.size());
        
        // Pattern 3: Random noise
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        
        for (auto& byte : corruptedData) {
            byte = static_cast<uint8_t>(dis(gen));
        }
        blob pattern3(corruptedData.data(), corruptedData.size());
        
        // These patterns should be processed without crashing
        ImageProcessor processor;
        
        try {
            auto result1 = processor.applyFilter(pattern1, FilterType::GAUSSIAN_BLUR, {{"sigma", 1.0}});
            auto result2 = processor.applyFilter(pattern2, FilterType::GAUSSIAN_BLUR, {{"sigma", 1.0}});
            auto result3 = processor.applyFilter(pattern3, FilterType::GAUSSIAN_BLUR, {{"sigma", 1.0}});
            
            // Should complete without crashing
        } catch (const std::exception& e) {
            // Acceptable if specific corruption is detected
        }
    });
    
    tester.printSummary();
}

/**
 * @brief Test performance under extreme conditions
 */
void testExtremePerformance() {
    std::cout << "\n=== Extreme Performance Conditions Tests ===\n";
    
    BoundaryTester tester;
    
    // Test 1: Performance with large data
    tester.runTest("Performance with large data", []() {
        const size_t largeSize = 5 * 1024 * 1024; // 5MB
        std::vector<uint8_t> largeData(largeSize, 128);
        blob largeBlob(largeData.data(), largeData.size());
        
        ImageProcessor processor;
        
        auto start = steady_clock::now();
        
        try {
            auto result = processor.applyFilter(largeBlob, FilterType::GAUSSIAN_BLUR, 
                                              {{"sigma", 2.0}});
            
            auto duration = duration_cast<milliseconds>(steady_clock::now() - start);
            
            if (duration.count() > 10000) { // > 10 seconds
                throw std::runtime_error("Processing too slow: " + std::to_string(duration.count()) + "ms");
            }
            
        } catch (const std::bad_alloc& e) {
            throw std::runtime_error("Insufficient memory for large data processing");
        }
    });
    
    // Test 2: Memory usage patterns
    tester.runTest("Memory usage patterns", []() {
        // Test multiple allocations and deallocations
        std::vector<blob> blobs;
        
        try {
            for (int i = 0; i < 100; ++i) {
                size_t size = 1024 * (i + 1); // Increasing sizes
                std::vector<uint8_t> data(size, static_cast<uint8_t>(i));
                blobs.emplace_back(data.data(), data.size());
            }
            
            // Process all blobs
            ImageProcessor processor;
            for (const auto& blob : blobs) {
                try {
                    auto result = processor.applyFilter(blob, FilterType::GAUSSIAN_BLUR, 
                                                      {{"sigma", 1.0}});
                } catch (const std::exception& e) {
                    // Some may fail due to size constraints
                }
            }
            
        } catch (const std::bad_alloc& e) {
            throw std::runtime_error("Memory allocation pattern failed");
        }
    });
    
    // Test 3: Rapid successive operations
    tester.runTest("Rapid successive operations", []() {
        std::vector<uint8_t> data(1000, 128);
        blob testBlob(data.data(), data.size());
        
        ImageProcessor processor;
        
        auto start = steady_clock::now();
        
        // Perform many rapid operations
        for (int i = 0; i < 1000; ++i) {
            try {
                auto result = processor.applyFilter(testBlob, FilterType::GAUSSIAN_BLUR, 
                                                  {{"sigma", 0.5}});
            } catch (const std::exception& e) {
                // Some operations may fail, but shouldn't crash
            }
        }
        
        auto duration = duration_cast<milliseconds>(steady_clock::now() - start);
        
        if (duration.count() > 5000) { // > 5 seconds for 1000 operations
            throw std::runtime_error("Rapid operations too slow: " + std::to_string(duration.count()) + "ms");
        }
    });
    
    tester.printSummary();
}

int main() {
    std::cout << "=== Atom Image Boundary Conditions Demo ===\n";
    std::cout << "This example demonstrates handling of boundary conditions and edge cases\n";

    // Run all boundary condition tests
    testZeroSizeConditions();
    testMaximumSizeLimits();
    testExtremeParameters();
    testNumericalPrecision();
    testInvalidInputValidation();
    testExtremePerformance();

    std::cout << "\n=== Boundary conditions demo completed ===\n";
    std::cout << "\nKey capabilities demonstrated:\n";
    std::cout << "- Zero-size and empty data handling\n";
    std::cout << "- Maximum size limits and memory constraints\n";
    std::cout << "- Extreme parameter values validation\n";
    std::cout << "- Numerical precision and overflow handling\n";
    std::cout << "- Invalid input validation and sanitization\n";
    std::cout << "- Performance under extreme conditions\n";
    std::cout << "- Comprehensive boundary testing framework\n";
    
    return 0;
}
