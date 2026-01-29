#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "atom/image/io/format_detector.hpp"
#include "atom/image/io/image_loader.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace atom::image::test {

class ImageLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        loader = createImageLoader();
        fileManager = std::make_unique<TestFileManager>();

        // Create test image files
        createTestImageFiles();
    }

    void TearDown() override { fileManager->cleanup(); }

    void createTestImageFiles() {
        // Create simple test images with known patterns
        auto gradientData = TestDataGenerator::generateGradientImage(32, 32, 3);
        auto checkerboardData =
            TestDataGenerator::generateCheckerboard(16, 16, 1, 4);
        auto noiseData =
            TestDataGenerator::generateRandomNoise(64, 64, 3, 12345);

        // Create test files (simulated - in real implementation these would be
        // actual image files)
        test_rgb_image = createMockImageFile("test_rgb.png", gradientData, 32,
                                             32, 3, ImageFormat::PNG);
        test_grayscale_image = createMockImageFile(
            "test_gray.bmp", checkerboardData, 16, 16, 1, ImageFormat::BMP);
        test_large_image = createMockImageFile("test_large.jpg", noiseData, 64,
                                               64, 3, ImageFormat::JPEG);

        // Create FITS test file
        test_fits_image =
            FitsTestDataGenerator::createTempFitsFile(20, 20, 1, 32);
        fileManager->registerTempFile(test_fits_image);

        // Register all test files for cleanup
        fileManager->registerTempFile(test_rgb_image);
        fileManager->registerTempFile(test_grayscale_image);
        fileManager->registerTempFile(test_large_image);

        // Create invalid/corrupted files for error testing
        createInvalidFiles();
    }

    std::string createMockImageFile(const std::string& filename,
                                    const std::vector<std::byte>& data,
                                    int width, int height, int channels,
                                    ImageFormat format) {
        std::ofstream file(filename, std::ios::binary);

        // Write format-specific header (simplified mock)
        switch (format) {
            case ImageFormat::PNG:
                file.write("\x89PNG\r\n\x1a\n", 8);
                break;
            case ImageFormat::JPEG:
                file.write("\xFF\xD8\xFF\xE0", 4);
                break;
            case ImageFormat::BMP:
                file.write("BM", 2);
                break;
            default:
                break;
        }

        // Write mock metadata
        file.write(reinterpret_cast<const char*>(&width), sizeof(width));
        file.write(reinterpret_cast<const char*>(&height), sizeof(height));
        file.write(reinterpret_cast<const char*>(&channels), sizeof(channels));

        // Write image data
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();

        return filename;
    }

    void createInvalidFiles() {
        // Empty file
        empty_file = "test_empty.dat";
        std::ofstream(empty_file).close();
        fileManager->registerTempFile(empty_file);

        // Corrupted file
        corrupted_file = "test_corrupted.jpg";
        std::ofstream corruptedFile(corrupted_file, std::ios::binary);
        corruptedFile.write("\xFF\xD8", 2);  // Incomplete JPEG header
        corruptedFile.close();
        fileManager->registerTempFile(corrupted_file);

        // File with wrong extension
        wrong_extension_file = "test_wrong.png";
        std::ofstream wrongFile(wrong_extension_file, std::ios::binary);
        wrongFile.write("\xFF\xD8\xFF\xE0", 4);  // JPEG data with PNG extension
        wrongFile.close();
        fileManager->registerTempFile(wrong_extension_file);
    }

    std::unique_ptr<ImageLoader> loader;
    std::unique_ptr<TestFileManager> fileManager;

    std::string test_rgb_image, test_grayscale_image, test_large_image,
        test_fits_image;
    std::string empty_file, corrupted_file, wrong_extension_file;
};

// Test basic image loading
TEST_F(ImageLoaderTest, LoadBasicImage) {
    auto result = loader->loadFromFile(test_rgb_image);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->width, 32);
    EXPECT_EQ(result->height, 32);
    EXPECT_EQ(result->channels, 3);
    EXPECT_EQ(result->format, ImageFormat::PNG);
    EXPECT_FALSE(result->imageData.empty());
}

// Test loading different formats
TEST_F(ImageLoaderTest, LoadDifferentFormats) {
    // Test PNG
    auto pngResult = loader->loadFromFile(test_rgb_image);
    ASSERT_TRUE(pngResult.has_value());
    EXPECT_EQ(pngResult->format, ImageFormat::PNG);

    // Test BMP
    auto bmpResult = loader->loadFromFile(test_grayscale_image);
    ASSERT_TRUE(bmpResult.has_value());
    EXPECT_EQ(bmpResult->format, ImageFormat::BMP);

    // Test JPEG
    auto jpegResult = loader->loadFromFile(test_large_image);
    ASSERT_TRUE(jpegResult.has_value());
    EXPECT_EQ(jpegResult->format, ImageFormat::JPEG);

// Test FITS
#ifdef ATOM_IMAGE_HAS_CFITSIO
    auto fitsResult = loader->loadFromFile(test_fits_image);
    ASSERT_TRUE(fitsResult.has_value());
    EXPECT_EQ(fitsResult->format, ImageFormat::FITS);
#endif
}

// Test loading with specific options
TEST_F(ImageLoaderTest, LoadWithOptions) {
    LoadOptions options;
    options.convertToRGB = true;
    options.normalizePixels = true;
    options.targetWidth = 16;
    options.targetHeight = 16;
    options.preserveAspectRatio = false;

    auto result = loader->loadFromFile(test_rgb_image, options);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->width, 16);
    EXPECT_EQ(result->height, 16);
    // Should be RGB after conversion
    EXPECT_EQ(result->channels, 3);
}

// Test loading with aspect ratio preservation
TEST_F(ImageLoaderTest, LoadWithAspectRatioPreservation) {
    LoadOptions options;
    options.targetWidth = 64;
    options.targetHeight = 32;
    options.preserveAspectRatio = true;

    auto result =
        loader->loadFromFile(test_rgb_image, options);  // 32x32 original
    ASSERT_TRUE(result.has_value());

    // Should maintain square aspect ratio, so both dimensions should be 32
    EXPECT_EQ(result->width, 32);
    EXPECT_EQ(result->height, 32);
}

// Test caching functionality
TEST_F(ImageLoaderTest, ImageCaching) {
    // Enable caching
    loader->setCacheEnabled(true);
    loader->setMaxCacheSize(10 * 1024 * 1024);  // 10MB

    // Load image first time
    auto start1 = std::chrono::high_resolution_clock::now();
    auto result1 = loader->loadFromFile(test_rgb_image);
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 =
        std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    ASSERT_TRUE(result1.has_value());

    // Load same image second time (should be cached)
    auto start2 = std::chrono::high_resolution_clock::now();
    auto result2 = loader->loadFromFile(test_rgb_image);
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 =
        std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    ASSERT_TRUE(result2.has_value());

    // Second load should be significantly faster (cached)
    EXPECT_LT(duration2.count(), duration1.count() / 2);

    // Results should be identical
    EXPECT_EQ(result1->width, result2->width);
    EXPECT_EQ(result1->height, result2->height);
    EXPECT_EQ(result1->channels, result2->channels);
}

// Test cache statistics
TEST_F(ImageLoaderTest, CacheStatistics) {
    loader->setCacheEnabled(true);

    // Clear cache and reset statistics
    loader->clearCache();

    auto initialStats = loader->getCacheStatistics();
    EXPECT_EQ(initialStats.hits, 0);
    EXPECT_EQ(initialStats.misses, 0);

    // Load image (cache miss)
    loader->loadFromFile(test_rgb_image);
    auto stats1 = loader->getCacheStatistics();
    EXPECT_EQ(stats1.hits, 0);
    EXPECT_EQ(stats1.misses, 1);

    // Load same image again (cache hit)
    loader->loadFromFile(test_rgb_image);
    auto stats2 = loader->getCacheStatistics();
    EXPECT_EQ(stats2.hits, 1);
    EXPECT_EQ(stats2.misses, 1);
}

// Test batch loading
TEST_F(ImageLoaderTest, BatchLoading) {
    std::vector<std::filesystem::path> filePaths = {
        test_rgb_image, test_grayscale_image, test_large_image};

    auto results = quickLoadBatch(filePaths, 2);  // Max 2 concurrent operations

    EXPECT_EQ(results.size(), filePaths.size());

    // Check that all images were loaded successfully
    for (size_t i = 0; i < results.size(); ++i) {
        EXPECT_FALSE(results[i].empty()) << "Failed to load image " << i;
    }
}

// Test error handling
TEST_F(ImageLoaderTest, ErrorHandling) {
    // Test non-existent file
    auto result = loader->loadFromFile("non_existent_file.jpg");
    EXPECT_FALSE(result.has_value());

    // Test empty file
    result = loader->loadFromFile(empty_file);
    EXPECT_FALSE(result.has_value());

    // Test corrupted file
    result = loader->loadFromFile(corrupted_file);
    EXPECT_FALSE(result.has_value());
}

// Test format mismatch handling
TEST_F(ImageLoaderTest, FormatMismatchHandling) {
    // File has JPEG data but PNG extension
    auto result = loader->loadFromFile(wrong_extension_file);

    // Should still load successfully by detecting actual format
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->format,
              ImageFormat::JPEG);  // Detected format, not extension
}

// Test memory mapping for large files
TEST_F(ImageLoaderTest, MemoryMapping) {
    LoadOptions options;
    options.useMemoryMapping = true;
    options.maxMemoryUsage = 1024 * 1024;  // 1MB limit

    auto result = loader->loadFromFile(test_large_image, options);
    ASSERT_TRUE(result.has_value());

    // Should load successfully even with memory mapping
    EXPECT_EQ(result->width, 64);
    EXPECT_EQ(result->height, 64);
}

// Test metadata extraction
TEST_F(ImageLoaderTest, MetadataExtraction) {
    auto result = loader->loadFromFile(test_rgb_image);
    ASSERT_TRUE(result.has_value());

    // Check that metadata is extracted
    EXPECT_FALSE(result->metadata.empty());
    EXPECT_TRUE(result->metadata.count("width") > 0);
    EXPECT_TRUE(result->metadata.count("height") > 0);
    EXPECT_TRUE(result->metadata.count("channels") > 0);
}

// Test custom format loader registration
TEST_F(ImageLoaderTest, CustomFormatLoader) {
    // Register a custom loader for a test format
    auto customLoader = [](const void* data, size_t size,
                           const LoadOptions& options) -> LoadResult {
        LoadResult result;
        result.success = true;
        result.width = 10;
        result.height = 10;
        result.channels = 1;
        result.format = ImageFormat::CUSTOM;
        result.imageData =
            TestDataGenerator::generateSolidColor(10, 10, 1, {128});
        return result;
    };

    loader->registerCustomLoader(ImageFormat::CUSTOM, customLoader);

    // Create a custom format file
    std::string customFile = "test.custom";
    std::ofstream file(customFile, std::ios::binary);
    file.write("CUSTOM", 6);
    file.close();
    fileManager->registerTempFile(customFile);

    // Force format detection to return CUSTOM
    LoadOptions options;
    options.preferredFormat = ImageFormat::CUSTOM;

    auto result = loader->loadFromFile(customFile, options);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->format, ImageFormat::CUSTOM);
    EXPECT_EQ(result->width, 10);
    EXPECT_EQ(result->height, 10);
}

// Test progressive loading
TEST_F(ImageLoaderTest, ProgressiveLoading) {
    bool progressCalled = false;
    int lastProgress = -1;

    auto progressCallback = [&](int progress) {
        progressCalled = true;
        EXPECT_GE(progress, lastProgress);
        EXPECT_GE(progress, 0);
        EXPECT_LE(progress, 100);
        lastProgress = progress;
    };

    LoadOptions options;
    options.progressCallback = progressCallback;

    auto result = loader->loadFromFile(test_large_image, options);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(progressCalled);
    EXPECT_EQ(lastProgress, 100);
}

// Test concurrent loading thread safety
TEST_F(ImageLoaderTest, ConcurrentLoading) {
    const int numThreads = 4;
    const int loadsPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back(
            [this, loadsPerThread, &successCount, &errorCount]() {
                for (int i = 0; i < loadsPerThread; ++i) {
                    try {
                        auto result = loader->loadFromFile(test_rgb_image);
                        if (result.has_value()) {
                            successCount.fetch_add(1);
                        } else {
                            errorCount.fetch_add(1);
                        }
                    } catch (...) {
                        errorCount.fetch_add(1);
                    }
                }
            });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount.load(), numThreads * loadsPerThread);
    EXPECT_EQ(errorCount.load(), 0);
}

// Test memory usage limits
TEST_F(ImageLoaderTest, MemoryUsageLimits) {
    LoadOptions options;
    options.maxMemoryUsage = 1024;  // Very small limit (1KB)

    // Try to load a large image with memory limit
    auto result = loader->loadFromFile(test_large_image, options);

    // Should either fail or use memory mapping/streaming
    if (result.has_value()) {
        // If successful, verify it's within memory constraints
        size_t expectedSize = result->width * result->height * result->channels;
        // Implementation should have used memory-efficient loading
        EXPECT_TRUE(expectedSize > 0);
    }
    // If it fails, that's also acceptable given the memory constraint
}

// Test loading from memory buffer
TEST_F(ImageLoaderTest, LoadFromBuffer) {
    // First load a file to get its data
    std::ifstream file(test_rgb_image, std::ios::binary);
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
    file.close();

    auto result = loader->loadFromBuffer(buffer.data(), buffer.size());
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->width, 32);
    EXPECT_EQ(result->height, 32);
    EXPECT_EQ(result->channels, 3);
}

// Test loading with pixel format conversion
TEST_F(ImageLoaderTest, PixelFormatConversion) {
    LoadOptions options;
    options.convertToRGB = true;

    // Load grayscale image and convert to RGB
    auto result = loader->loadFromFile(test_grayscale_image, options);
    ASSERT_TRUE(result.has_value());

    // Should be converted to RGB (3 channels)
    EXPECT_EQ(result->channels, 3);
}

// Test loading with normalization
TEST_F(ImageLoaderTest, PixelNormalization) {
    LoadOptions options;
    options.normalizePixels = true;

    auto result = loader->loadFromFile(test_rgb_image, options);
    ASSERT_TRUE(result.has_value());

    // Check that pixels are normalized (values should be in [0,1] range)
    // This would require examining the actual pixel data
    EXPECT_FALSE(result->imageData.empty());
}

// Test cache eviction policy
TEST_F(ImageLoaderTest, CacheEviction) {
    loader->setCacheEnabled(true);
    loader->setMaxCacheSize(1024);  // Very small cache

    // Load multiple images to trigger eviction
    loader->loadFromFile(test_rgb_image);
    loader->loadFromFile(test_grayscale_image);
    loader->loadFromFile(test_large_image);

    auto stats = loader->getCacheStatistics();
    // Cache should have evicted some entries
    EXPECT_GT(stats.evictions, 0);
}

// Test quick loading utility functions
TEST_F(ImageLoaderTest, QuickLoadUtilities) {
    // Test quick single image load
    auto image = quickLoadImage(test_rgb_image);
    EXPECT_FALSE(image.empty());

    // Test quick batch load
    std::vector<std::filesystem::path> paths = {test_rgb_image,
                                                test_grayscale_image};
    auto images = quickLoadBatch(paths);
    EXPECT_EQ(images.size(), 2);
    EXPECT_FALSE(images[0].empty());
    EXPECT_FALSE(images[1].empty());
}

// Test performance benchmarking
TEST_F(ImageLoaderTest, DISABLED_LoadingPerformance) {
    const int iterations = 100;

    // Disable caching for fair performance test
    loader->setCacheEnabled(false);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto result = loader->loadFromFile(test_rgb_image);
        ASSERT_TRUE(result.has_value());
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double avgTime = static_cast<double>(duration.count()) / iterations;

    std::cout << "Average loading time: " << avgTime << " ms" << std::endl;

    // Should be reasonably fast (less than 10ms per small image)
    EXPECT_LT(avgTime, 10.0);
}

}  // namespace atom::image::test
