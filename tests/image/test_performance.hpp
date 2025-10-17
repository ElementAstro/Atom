#ifndef ATOM_IMAGE_TEST_PERFORMANCE_HPP
#define ATOM_IMAGE_TEST_PERFORMANCE_HPP

#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "test_utils.hpp"

#ifdef ATOM_IMAGE_HAS_OPENCV
#include "atom/image/processing/image_processor.hpp"
// Note: filters.hpp is included by image_processor.hpp, don't include it again
// to avoid duplicate enum definitions
#include "atom/image/processing/enhancement.hpp"
#include "atom/image/processing/transforms.hpp"
#endif

#ifdef ATOM_IMAGE_HAS_CFITSIO
#include "atom/image/formats/fits_file.hpp"
#include "atom/image/formats/fits_utils.hpp"
#endif

namespace atom::image::test {

class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test data of various sizes
        createTestData();

#ifdef ATOM_IMAGE_HAS_OPENCV
        processor = atom::image::createOptimalProcessor(false);
#endif

        // Initialize random number generator
        rng.seed(42);  // Fixed seed for reproducible tests
    }

    void TearDown() override {
        // Cleanup is automatic with RAII
    }

    void createTestData() {
        // Small image (64x64)
        auto small_data = TestDataGenerator::generateGradientImage(64, 64, 3);
        small_image = blob(small_data.data(), small_data.size(), 64, 64, 3);

        // Medium image (256x256)
        auto medium_data =
            TestDataGenerator::generateGradientImage(256, 256, 3);
        medium_image =
            blob(medium_data.data(), medium_data.size(), 256, 256, 3);

        // Large image (1024x1024) - only for stress tests
        auto large_data =
            TestDataGenerator::generateGradientImage(1024, 1024, 3);
        large_image = blob(large_data.data(), large_data.size(), 1024, 1024, 3);

        // Create batch of images for batch processing tests
        for (int i = 0; i < 10; ++i) {
            auto data = TestDataGenerator::generateGradientImage(128, 128, 3);
            image_batch.emplace_back(data.data(), data.size(), 128, 128, 3);
        }
    }

    // Helper function to measure execution time
    template <typename Func>
    double measureExecutionTime(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count() / 1000.0;  // Return milliseconds
    }

    // Test data
    blob small_image;
    blob medium_image;
    blob large_image;
    std::vector<blob> image_batch;

#ifdef ATOM_IMAGE_HAS_OPENCV
    std::unique_ptr<atom::image::ImageProcessor> processor;
#endif

    std::mt19937 rng;
};

// Test blob creation and copy performance
TEST_F(PerformanceTest, BlobCreationPerformance) {
    const int iterations = 1000;

    // Test small blob creation
    auto small_time = measureExecutionTime([]() {
        for (int i = 0; i < 1000; ++i) {
            auto data = TestDataGenerator::generateSolidColor(64, 64, 3, {128});
            blob test_blob(data.data(), data.size(), 64, 64, 3);
            (void)test_blob;  // Suppress unused variable warning
        }
    });

    // Test medium blob creation
    auto medium_time = measureExecutionTime([]() {
        for (int i = 0; i < 1000; ++i) {
            auto data =
                TestDataGenerator::generateSolidColor(256, 256, 3, {128});
            blob test_blob(data.data(), data.size(), 256, 256, 3);
            (void)test_blob;
        }
    });

    std::cout << "Small blob creation (" << iterations
              << " iterations): " << small_time << " ms" << std::endl;
    std::cout << "Medium blob creation (" << iterations
              << " iterations): " << medium_time << " ms" << std::endl;

    // Performance expectations (these are reasonable bounds)
    EXPECT_LT(small_time, 1000.0);   // Should complete in less than 1 second
    EXPECT_LT(medium_time, 5000.0);  // Should complete in less than 5 seconds
}

// Test blob copy performance
TEST_F(PerformanceTest, BlobCopyPerformance) {
    const int iterations = 100;

    auto copy_time = measureExecutionTime([this]() {
        for (int i = 0; i < 100; ++i) {
            blob copy(large_image);
            (void)copy;
        }
    });

    std::cout << "Large blob copy (" << iterations
              << " iterations): " << copy_time << " ms" << std::endl;

    EXPECT_LT(copy_time, 10000.0);  // Should complete in less than 10 seconds
}

// Test blob slice performance
TEST_F(PerformanceTest, BlobSlicePerformance) {
    const int iterations = 10000;

    auto slice_time = measureExecutionTime([this]() {
        for (int i = 0; i < 10000; ++i) {
            auto slice = large_image.slice(i % 1000, 1000);
            (void)slice;
        }
    });

    std::cout << "Blob slice operations (" << iterations
              << " iterations): " << slice_time << " ms" << std::endl;

    EXPECT_LT(slice_time, 5000.0);  // Should complete in less than 5 seconds
}

#ifdef ATOM_IMAGE_HAS_OPENCV
// Test image processing performance
TEST_F(PerformanceTest, ImageProcessingPerformance) {
    if (!processor) {
        GTEST_SKIP() << "Image processor not available";
    }

    const int iterations = 50;

    // Test resize performance
    auto resize_time = measureExecutionTime([this, iterations]() {
        for (int i = 0; i < iterations; ++i) {
            auto result = processor->resize(medium_image, 128, 128);
            (void)result;
        }
    });

    // Test rotation performance
    auto rotation_time = measureExecutionTime([this, iterations]() {
        for (int i = 0; i < iterations; ++i) {
            auto result =
                processor->rotate(medium_image, i * 7.2);  // Different angles
            (void)result;
        }
    });

    // Test filter performance
    auto filter_time = measureExecutionTime([this]() {
        for (int i = 0; i < 50; ++i) {
            auto result =
                processor->applyFilter(medium_image, FilterType::GAUSSIAN_BLUR);
            (void)result;
        }
    });

    std::cout << "Resize operations (" << iterations
              << " iterations): " << resize_time << " ms" << std::endl;
    std::cout << "Rotation operations (" << iterations
              << " iterations): " << rotation_time << " ms" << std::endl;
    std::cout << "Filter operations (" << iterations
              << " iterations): " << filter_time << " ms" << std::endl;

    // Performance expectations
    EXPECT_LT(resize_time, 30000.0);    // 30 seconds
    EXPECT_LT(rotation_time, 30000.0);  // 30 seconds
    EXPECT_LT(filter_time, 30000.0);    // 30 seconds
}

// Test batch processing performance
TEST_F(PerformanceTest, BatchProcessingPerformance) {
    if (!processor) {
        GTEST_SKIP() << "Image processor not available";
    }

    auto batch_operation = [this](const blob& img) {
        return processor->resize(img, 64, 64);
    };

    auto batch_time = measureExecutionTime([this, &batch_operation]() {
        auto results = processor->processBatch(image_batch, batch_operation);
        (void)results;
    });

    std::cout << "Batch processing (" << image_batch.size()
              << " images): " << batch_time << " ms" << std::endl;

    EXPECT_LT(batch_time, 15000.0);  // Should complete in less than 15 seconds
}
#endif

// Test memory stress with large allocations
TEST_F(PerformanceTest, DISABLED_MemoryStressTest) {
    const size_t large_size = 2048;  // 2048x2048 image
    const int iterations = 10;

    std::vector<blob> large_blobs;
    large_blobs.reserve(iterations);

    auto allocation_time =
        measureExecutionTime([&large_blobs, large_size, iterations]() {
            for (int i = 0; i < iterations; ++i) {
                auto data = TestDataGenerator::generateGradientImage(
                    large_size, large_size, 3);
                large_blobs.emplace_back(data.data(), data.size(), large_size,
                                         large_size, 3);
            }
        });

    std::cout << "Large memory allocation (" << iterations << " x "
              << large_size << "x" << large_size
              << " images): " << allocation_time << " ms" << std::endl;

    // Test should complete without memory errors
    EXPECT_EQ(large_blobs.size(), iterations);
    EXPECT_LT(allocation_time,
              60000.0);  // Should complete in less than 1 minute
}

// Test concurrent access performance
TEST_F(PerformanceTest, ConcurrentAccessPerformance) {
    const int num_threads = std::thread::hardware_concurrency();
    const int operations_per_thread = 100;

    std::atomic<int> completed_operations{0};
    std::vector<std::thread> threads;

    auto concurrent_time = measureExecutionTime([&]() {
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([this, operations_per_thread,
                                  &completed_operations]() {
                for (int i = 0; i < operations_per_thread; ++i) {
                    // Perform read operations on shared data
                    auto size = medium_image.size();
                    if (size > 0) {
                        auto slice =
                            medium_image.slice(0, std::min(size, size_t(1000)));
                        (void)slice;
                        completed_operations.fetch_add(1);
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    });

    std::cout << "Concurrent access (" << num_threads << " threads, "
              << operations_per_thread << " ops each): " << concurrent_time
              << " ms" << std::endl;

    EXPECT_EQ(completed_operations.load(), num_threads * operations_per_thread);
    EXPECT_LT(concurrent_time,
              10000.0);  // Should complete in less than 10 seconds
}

#ifdef ATOM_IMAGE_HAS_CFITSIO
// Test FITS file I/O performance
TEST_F(PerformanceTest, FITSFileIOPerformance) {
    const std::string test_fits_file = "performance_test.fits";
    TestFileManager file_manager;
    file_manager.addFile(test_fits_file);

    // Create test FITS data
    const int width = 512, height = 512;
    std::vector<float> fits_data(width * height);
    std::iota(fits_data.begin(), fits_data.end(), 0.0f);

    // Test FITS write performance
    auto write_time = measureExecutionTime([&]() {
        try {
            FITSFile fits_file(test_fits_file, FITSMode::WRITE);
            fits_file.writeImageData(fits_data.data(), width, height);
        } catch (...) {
            // Handle gracefully if FITS not available
        }
    });

    // Test FITS read performance
    auto read_time = measureExecutionTime([&]() {
        try {
            FITSFile fits_file(test_fits_file, FITSMode::READ);
            auto data = fits_file.readImageData<float>();
            (void)data;
        } catch (...) {
            // Handle gracefully if FITS not available
        }
    });

    std::cout << "FITS write (" << width << "x" << height << "): " << write_time
              << " ms" << std::endl;
    std::cout << "FITS read (" << width << "x" << height << "): " << read_time
              << " ms" << std::endl;

    // Performance expectations for FITS I/O
    EXPECT_LT(write_time, 5000.0);  // Should write in less than 5 seconds
    EXPECT_LT(read_time, 5000.0);   // Should read in less than 5 seconds
}
#endif

// Test fragmented memory performance
TEST_F(PerformanceTest, FragmentedMemoryPerformance) {
    const int num_allocations = 1000;
    std::vector<blob> blobs;
    blobs.reserve(num_allocations);

    // Create many small allocations to fragment memory
    auto fragmentation_time = measureExecutionTime([&]() {
        for (int i = 0; i < num_allocations; ++i) {
            // Vary sizes to create fragmentation
            int size = 64 + (i % 128);
            auto data = TestDataGenerator::generateSolidColor(
                size, size, 1, {static_cast<uint8_t>(i % 256)});
            blobs.emplace_back(data.data(), data.size(), size, size, 1);
        }
    });

    // Test access performance on fragmented memory
    auto access_time = measureExecutionTime([&]() {
        for (const auto& blob : blobs) {
            if (blob.size() > 0) {
                auto value = blob[0];
                (void)value;
            }
        }
    });

    std::cout << "Fragmented memory allocation (" << num_allocations
              << " blobs): " << fragmentation_time << " ms" << std::endl;
    std::cout << "Fragmented memory access (" << num_allocations
              << " blobs): " << access_time << " ms" << std::endl;

    EXPECT_EQ(blobs.size(), num_allocations);
    EXPECT_LT(fragmentation_time,
              15000.0);              // Should complete in less than 15 seconds
    EXPECT_LT(access_time, 1000.0);  // Access should be fast
}

// Test cache performance with repeated operations
TEST_F(PerformanceTest, CachePerformanceTest) {
    const int iterations = 1000;

    // Test repeated slice operations (should benefit from caching)
    auto repeated_slice_time = measureExecutionTime([this]() {
        for (int i = 0; i < 1000; ++i) {
            // Repeat the same slice operation
            auto slice = medium_image.slice(1000, 1000);
            (void)slice;
        }
    });

    // Test varied slice operations (less cache-friendly)
    auto varied_slice_time = measureExecutionTime([this]() {
        for (int i = 0; i < 1000; ++i) {
            // Vary the slice parameters
            auto slice = medium_image.slice(i % 10000, 1000);
            (void)slice;
        }
    });

    std::cout << "Repeated slice operations (" << iterations
              << " iterations): " << repeated_slice_time << " ms" << std::endl;
    std::cout << "Varied slice operations (" << iterations
              << " iterations): " << varied_slice_time << " ms" << std::endl;

    // Repeated operations might be faster due to caching
    EXPECT_LT(repeated_slice_time, 5000.0);
    EXPECT_LT(varied_slice_time, 10000.0);
}

// Test scalability with increasing data sizes
TEST_F(PerformanceTest, ScalabilityTest) {
    std::vector<int> sizes = {64, 128, 256, 512};
    std::vector<double> times;

    for (int size : sizes) {
        auto data = TestDataGenerator::generateGradientImage(size, size, 3);
        blob test_blob(data.data(), data.size(), size, size, 3);

        auto time = measureExecutionTime([&test_blob]() {
            // Perform a standard operation
            blob copy(test_blob);
            auto slice = copy.slice(0, std::min(copy.size(), size_t(1000)));
            (void)slice;
        });

        times.push_back(time);
        std::cout << "Size " << size << "x" << size << ": " << time << " ms"
                  << std::endl;
    }

    // Check that performance scales reasonably
    for (size_t i = 1; i < times.size(); ++i) {
        // Each doubling of size should not increase time by more than 8x
        EXPECT_LT(times[i], times[i - 1] * 8.0);
    }
}

// Test resource cleanup under stress
TEST_F(PerformanceTest, ResourceCleanupStressTest) {
    const int stress_iterations = 500;

    auto cleanup_time = measureExecutionTime([this]() {
        for (int i = 0; i < 500; ++i) {
            // Create temporary objects that need cleanup
            {
                auto data =
                    TestDataGenerator::generateGradientImage(128, 128, 3);
                blob temp_blob(data.data(), data.size(), 128, 128, 3);

                // Perform operations that create more temporary objects
                auto slice1 = temp_blob.slice(0, 1000);
                auto slice2 = temp_blob.slice(1000, 1000);
                blob copy(temp_blob);

                // Objects should be automatically cleaned up here
            }

            // Force some operations between iterations
            if (i % 100 == 0) {
                auto test_slice = small_image.slice(0, 100);
                (void)test_slice;
            }
        }
    });

    std::cout << "Resource cleanup stress test (" << stress_iterations
              << " iterations): " << cleanup_time << " ms" << std::endl;

    // Should complete without memory leaks or excessive time
    EXPECT_LT(cleanup_time,
              30000.0);  // Should complete in less than 30 seconds
}

// Test performance under memory pressure
TEST_F(PerformanceTest, DISABLED_MemoryPressureTest) {
    // This test is disabled by default as it uses significant memory
    const size_t pressure_size = 1024;  // 1024x1024 images
    const int num_images = 20;

    std::vector<blob> pressure_blobs;
    pressure_blobs.reserve(num_images);

    auto pressure_time = measureExecutionTime([&]() {
        for (int i = 0; i < num_images; ++i) {
            auto data = TestDataGenerator::generateGradientImage(
                pressure_size, pressure_size, 3);
            pressure_blobs.emplace_back(data.data(), data.size(), pressure_size,
                                        pressure_size, 3);

            // Perform operations on existing blobs to maintain memory pressure
            for (auto& existing_blob : pressure_blobs) {
                if (existing_blob.size() > 1000) {
                    auto slice = existing_blob.slice(0, 1000);
                    (void)slice;
                }
            }
        }
    });

    std::cout << "Memory pressure test (" << num_images << " x "
              << pressure_size << "x" << pressure_size
              << " images): " << pressure_time << " ms" << std::endl;

    EXPECT_EQ(pressure_blobs.size(), num_images);
    EXPECT_LT(pressure_time,
              120000.0);  // Should complete in less than 2 minutes
}

}  // namespace atom::image::test

#endif  // ATOM_IMAGE_TEST_PERFORMANCE_HPP
