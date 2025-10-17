/**
 * @file fast_blob_performance.cpp
 * @brief Performance comparison and usage patterns for fast_blob
 *
 * This example demonstrates:
 * - fast_blob vs normal blob performance comparison
 * - Memory usage patterns and optimization
 * - Read-only operation benefits
 * - Zero-copy operation examples
 * - Large data handling with fast_blob
 * - Best practices for fast_blob usage
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <chrono>
#include <iostream>
#include <memory>
#include <vector>
#include <random>
#include <algorithm>
#include <iomanip>

#include "atom/image/core/image_blob.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Performance measurement utility
 */
class PerformanceMeasurer {
public:
    void start() {
        start_time_ = high_resolution_clock::now();
    }

    double stop() {
        auto end_time = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end_time - start_time_);
        return duration.count();
    }

private:
    high_resolution_clock::time_point start_time_;
};

/**
 * @brief Create test data of specified size
 */
std::vector<uint8_t> createTestData(size_t size) {
    std::vector<uint8_t> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);

    for (size_t i = 0; i < size; ++i) {
        data[i] = dis(gen);
    }

    return data;
}

/**
 * @brief Compare creation performance between blob types
 */
void compareCreationPerformance() {
    std::cout << "\n=== Creation Performance Comparison ===\n";

    std::vector<size_t> sizes = {
        1024,           // 1KB
        1024 * 1024,    // 1MB
        10 * 1024 * 1024 // 10MB
    };

    for (size_t size : sizes) {
        std::cout << "\nTesting with " << (size / 1024) << "KB data:\n";

        auto testData = createTestData(size);
        PerformanceMeasurer measurer;

        // Test normal blob creation (with memory copy)
        measurer.start();
        for (int i = 0; i < 100; ++i) {
            blob normalBlob(testData.data(), testData.size());
        }
        double normalTime = measurer.stop();

        // Test fast blob creation (zero-copy)
        measurer.start();
        for (int i = 0; i < 100; ++i) {
            fast_blob fastBlob(testData.data(), testData.size());
        }
        double fastTime = measurer.stop();

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Normal blob (100 iterations): " << normalTime << " μs\n";
        std::cout << "  Fast blob (100 iterations): " << fastTime << " μs\n";
        std::cout << "  Speed improvement: " << (normalTime / fastTime) << "x\n";
        std::cout << "  Memory saved: " << (size / (1024 * 1024)) << "MB per fast_blob\n";
    }
}

/**
 * @brief Demonstrate read-only operations with fast_blob
 */
void demonstrateReadOnlyOperations() {
    std::cout << "\n=== Read-Only Operations with fast_blob ===\n";

    // Create test data
    auto testData = createTestData(1024 * 1024); // 1MB
    fast_blob fastBlob(testData.data(), testData.size());

    std::cout << "Created fast_blob with " << fastBlob.size() << " bytes\n";

    // Basic read operations
    std::cout << "Dimensions: " << fastBlob.getWidth() << "x" << fastBlob.getHeight() << "\n";
    std::cout << "Channels: " << fastBlob.getChannels() << "\n";
    std::cout << "Empty check: " << (fastBlob.isEmpty() ? "empty" : "not empty") << "\n";

    // Slice operations (zero-copy sub-views)
    PerformanceMeasurer measurer;

    measurer.start();
    for (int i = 0; i < 1000; ++i) {
        auto slice = fastBlob.slice(100, 500);
    }
    double sliceTime = measurer.stop();

    std::cout << "Slice operations (1000 iterations): " << sliceTime << " μs\n";
    std::cout << "Average slice time: " << (sliceTime / 1000.0) << " μs\n";

    // Data access patterns
    std::cout << "\nData access patterns:\n";

    // Sequential access
    measurer.start();
    uint64_t checksum = 0;
    for (size_t i = 0; i < std::min(fastBlob.size(), size_t(10000)); ++i) {
        checksum += static_cast<uint8_t>(fastBlob.data()[i]);
    }
    double sequentialTime = measurer.stop();

    std::cout << "Sequential access (10K elements): " << sequentialTime << " μs\n";
    std::cout << "Checksum: " << checksum << "\n";

    // Random access
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, fastBlob.size() - 1);

    measurer.start();
    checksum = 0;
    for (int i = 0; i < 10000; ++i) {
        size_t index = dis(gen);
        checksum += static_cast<uint8_t>(fastBlob.data()[index]);
    }
    double randomTime = measurer.stop();

    std::cout << "Random access (10K elements): " << randomTime << " μs\n";
    std::cout << "Checksum: " << checksum << "\n";
}

/**
 * @brief Show memory usage patterns
 */
void demonstrateMemoryUsage() {
    std::cout << "\n=== Memory Usage Patterns ===\n";

    size_t dataSize = 50 * 1024 * 1024; // 50MB
    auto largeData = createTestData(dataSize);

    std::cout << "Original data size: " << (dataSize / (1024 * 1024)) << "MB\n";

    // Normal blob - copies data
    {
        std::cout << "\nNormal blob usage:\n";
        blob normalBlob(largeData.data(), largeData.size());
        std::cout << "  Memory usage: ~" << (dataSize * 2 / (1024 * 1024)) << "MB (original + copy)\n";
        std::cout << "  Blob size: " << normalBlob.size() << " bytes\n";
    }

    // Fast blob - references data
    {
        std::cout << "\nFast blob usage:\n";
        fast_blob fastBlob(largeData.data(), largeData.size());
        std::cout << "  Memory usage: ~" << (dataSize / (1024 * 1024)) << "MB (original only)\n";
        std::cout << "  Blob size: " << fastBlob.size() << " bytes\n";
        std::cout << "  Memory saved: " << (dataSize / (1024 * 1024)) << "MB\n";
    }

    // Multiple fast blobs from same data
    std::cout << "\nMultiple fast blobs from same data:\n";
    std::vector<fast_blob> fastBlobs;

    for (int i = 0; i < 10; ++i) {
        size_t offset = i * (dataSize / 10);
        size_t size = dataSize / 10;
        if (offset + size <= dataSize) {
            fastBlobs.emplace_back(largeData.data() + offset, size);
        }
    }

    std::cout << "  Created " << fastBlobs.size() << " fast blobs\n";
    std::cout << "  Total memory usage: ~" << (dataSize / (1024 * 1024)) << "MB (shared)\n";
    std::cout << "  Equivalent normal blobs would use: ~" << (dataSize * fastBlobs.size() / (1024 * 1024)) << "MB\n";
}

/**
 * @brief Demonstrate best practices for fast_blob usage
 */
void demonstrateBestPractices() {
    std::cout << "\n=== Best Practices for fast_blob Usage ===\n";

    auto testData = createTestData(1024 * 1024);

    std::cout << "1. Use fast_blob for read-only operations:\n";
    {
        fast_blob readOnlyBlob(testData.data(), testData.size());

        // Good: Read operations
        std::cout << "   Size: " << readOnlyBlob.size() << " bytes\n";
        std::cout << "   Dimensions: " << readOnlyBlob.getWidth() << "x" << readOnlyBlob.getHeight() << "\n";

        // Good: Creating slices
        auto slice = readOnlyBlob.slice(100, 500);
        std::cout << "   Slice size: " << slice.size() << " bytes\n";
    }

    std::cout << "\n2. Ensure data lifetime exceeds fast_blob lifetime:\n";
    {
        // Good: Data outlives fast_blob
        auto localData = createTestData(1024);
        {
            fast_blob safeBlob(localData.data(), localData.size());
            std::cout << "   Safe usage: data available during fast_blob lifetime\n";
        }
        // fast_blob destroyed before localData
    }

    std::cout << "\n3. Use for large data processing pipelines:\n";
    {
        // Simulate processing pipeline
        fast_blob inputBlob(testData.data(), testData.size());

        PerformanceMeasurer measurer;
        measurer.start();

        // Stage 1: Analysis
        uint64_t sum = 0;
        for (size_t i = 0; i < inputBlob.size(); i += 100) {
            sum += static_cast<uint8_t>(inputBlob.data()[i]);
        }

        // Stage 2: Create multiple views
        std::vector<fast_blob> views;
        for (int i = 0; i < 10; ++i) {
            size_t offset = i * (inputBlob.size() / 10);
            size_t size = inputBlob.size() / 10;
            if (offset + size <= inputBlob.size()) {
                views.emplace_back(inputBlob.data() + offset, size);
            }
        }

        double pipelineTime = measurer.stop();

        std::cout << "   Pipeline processing time: " << pipelineTime << " μs\n";
        std::cout << "   Created " << views.size() << " views with zero memory copy\n";
        std::cout << "   Analysis result: " << sum << "\n";
    }

    std::cout << "\n4. Performance considerations:\n";
    std::cout << "   - Use fast_blob for temporary processing\n";
    std::cout << "   - Ideal for read-heavy workloads\n";
    std::cout << "   - Perfect for data analysis and inspection\n";
    std::cout << "   - Excellent for creating multiple views of same data\n";
}

int main() {
    std::cout << "=== Atom Image fast_blob Performance Example ===\n";
    std::cout << "This example demonstrates fast_blob usage patterns and performance benefits\n";

    // Run all demonstrations
    compareCreationPerformance();
    demonstrateReadOnlyOperations();
    demonstrateMemoryUsage();
    demonstrateBestPractices();

    std::cout << "\n=== fast_blob performance example completed ===\n";
    std::cout << "\nKey takeaways:\n";
    std::cout << "- fast_blob provides zero-copy access to existing data\n";
    std::cout << "- Significant performance and memory benefits for read-only operations\n";
    std::cout << "- Ideal for data analysis, inspection, and temporary processing\n";
    std::cout << "- Ensure data lifetime exceeds fast_blob lifetime\n";

    return 0;
}
