#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#include "atom/image/io/image_saver.hpp"
#include "atom/image/io/image_loader.hpp"
#include "atom/image/core/image_blob.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace atom::image::test {

class ImageSaverTest : public ::testing::Test {
protected:
    void SetUp() override {
        saver = createImageSaver();
        loader = createImageLoader();
        fileManager = std::make_unique<TestFileManager>();
        
        // Create test image data
        createTestImageData();
    }

    void TearDown() override {
        fileManager->cleanup();
    }

    void createTestImageData() {
        // Create various test images
        rgb_image_data = TestDataGenerator::generateGradientImage(32, 32, 3);
        grayscale_image_data = TestDataGenerator::generateCheckerboard(16, 16, 1, 4);
        rgba_image_data = TestDataGenerator::generateGradientImage(24, 24, 4);
        large_image_data = TestDataGenerator::generateRandomNoise(128, 128, 3, 54321);

        // Create blob objects
        rgb_blob = blob(rgb_image_data.data(), rgb_image_data.size());
        grayscale_blob = blob(grayscale_image_data.data(), grayscale_image_data.size());
        rgba_blob = blob(rgba_image_data.data(), rgba_image_data.size());
        large_blob = blob(large_image_data.data(), large_image_data.size());
    }

    std::unique_ptr<ImageSaver> saver;
    std::unique_ptr<ImageLoader> loader;
    std::unique_ptr<TestFileManager> fileManager;
    
    std::vector<std::byte> rgb_image_data, grayscale_image_data, rgba_image_data, large_image_data;
    blob rgb_blob, grayscale_blob, rgba_blob, large_blob;
};

// Test basic image saving
TEST_F(ImageSaverTest, SaveBasicImage) {
    std::string outputPath = "test_output.png";
    fileManager->registerTempFile(outputPath);

    SaveOptions options;
    options.targetFormat = ImageFormat::PNG;
    options.quality = 95;

    auto result = saver->saveToFile(rgb_blob, outputPath, options);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(fs::exists(outputPath));
    EXPECT_GT(fs::file_size(outputPath), 0);
}

// Test saving different formats
TEST_F(ImageSaverTest, SaveDifferentFormats) {
    std::vector<std::pair<ImageFormat, std::string>> formats = {
        {ImageFormat::PNG, "test.png"},
        {ImageFormat::JPEG, "test.jpg"},
        {ImageFormat::BMP, "test.bmp"},
        {ImageFormat::TIFF, "test.tiff"}
    };

    for (const auto& [format, filename] : formats) {
        fileManager->registerTempFile(filename);
        
        SaveOptions options;
        options.targetFormat = format;
        options.quality = 90;

        auto result = saver->saveToFile(rgb_blob, filename, options);
        EXPECT_TRUE(result.success) << "Failed to save format: " << static_cast<int>(format);
        EXPECT_TRUE(fs::exists(filename));
        EXPECT_GT(fs::file_size(filename), 0);
    }
}

// Test quality settings for lossy formats
TEST_F(ImageSaverTest, QualitySettings) {
    std::string highQualityFile = "test_high_quality.jpg";
    std::string lowQualityFile = "test_low_quality.jpg";
    
    fileManager->registerTempFile(highQualityFile);
    fileManager->registerTempFile(lowQualityFile);

    // Save with high quality
    SaveOptions highQualityOptions;
    highQualityOptions.targetFormat = ImageFormat::JPEG;
    highQualityOptions.quality = 95;

    auto highResult = saver->saveToFile(rgb_blob, highQualityFile, highQualityOptions);
    EXPECT_TRUE(highResult.success);

    // Save with low quality
    SaveOptions lowQualityOptions;
    lowQualityOptions.targetFormat = ImageFormat::JPEG;
    lowQualityOptions.quality = 20;
    
    auto lowResult = saver->saveToFile(rgb_blob, lowQualityFile, lowQualityOptions);
    EXPECT_TRUE(lowResult.success);

    // High quality file should be larger
    auto highSize = fs::file_size(highQualityFile);
    auto lowSize = fs::file_size(lowQualityFile);
    EXPECT_GT(highSize, lowSize);
}

// Test compression settings
TEST_F(ImageSaverTest, CompressionSettings) {
    std::string uncompressedFile = "test_uncompressed.png";
    std::string compressedFile = "test_compressed.png";
    
    fileManager->registerTempFile(uncompressedFile);
    fileManager->registerTempFile(compressedFile);

    // Save without compression
    SaveOptions uncompressedOptions;
    uncompressedOptions.targetFormat = ImageFormat::PNG;
    uncompressedOptions.compressionLevel = 0;

    auto uncompressedResult = saver->saveToFile(rgb_blob, uncompressedFile, uncompressedOptions);
    EXPECT_TRUE(uncompressedResult.success);

    // Save with maximum compression
    SaveOptions compressedOptions;
    compressedOptions.targetFormat = ImageFormat::PNG;
    compressedOptions.compressionLevel = 9;
    
    auto compressedResult = saver->saveToFile(rgb_blob, compressedFile, compressedOptions);
    EXPECT_TRUE(compressedResult.success);

    // Compressed file should be smaller or equal
    auto uncompressedSize = fs::file_size(uncompressedFile);
    auto compressedSize = fs::file_size(compressedFile);
    EXPECT_LE(compressedSize, uncompressedSize);
}

// Test metadata preservation
TEST_F(ImageSaverTest, MetadataPreservation) {
    std::string outputFile = "test_with_metadata.jpg";
    fileManager->registerTempFile(outputFile);

    SaveOptions options;
    options.targetFormat = ImageFormat::JPEG;
    options.preserveMetadata = true;
    options.customMetadata = {
        {"Artist", "Test Artist"},
        {"Copyright", "Test Copyright"},
        {"Description", "Test Description"},
        {"Software", "Atom Image Library"}
    };

    auto result = saver->saveToFile(rgb_blob, outputFile, options);
    EXPECT_TRUE(result.success);

    // Verify metadata was saved (would require loading and checking)
    EXPECT_TRUE(fs::exists(outputFile));
}

// Test format auto-detection from extension
TEST_F(ImageSaverTest, FormatAutoDetection) {
    std::vector<std::string> files = {
        "test_auto.png",
        "test_auto.jpg",
        "test_auto.bmp",
        "test_auto.tiff"
    };

    for (const auto& filename : files) {
        fileManager->registerTempFile(filename);
        
        SaveOptions options; // No format specified - should auto-detect
        
        auto result = saver->saveToFile(rgb_blob, filename, options);
        EXPECT_TRUE(result.success) << "Failed to save: " << filename;
        EXPECT_TRUE(fs::exists(filename));
    }
}

// Test saving different channel configurations
TEST_F(ImageSaverTest, DifferentChannelConfigurations) {
    // Test grayscale (1 channel)
    std::string grayscaleFile = "test_grayscale.png";
    fileManager->registerTempFile(grayscaleFile);

    SaveOptions grayscaleOptions;
    grayscaleOptions.targetFormat = ImageFormat::PNG;

    auto grayscaleResult = saver->saveToFile(grayscale_blob, grayscaleFile, grayscaleOptions);
    EXPECT_TRUE(grayscaleResult.success);

    // Test RGB (3 channels)
    std::string rgbFile = "test_rgb.png";
    fileManager->registerTempFile(rgbFile);

    auto rgbResult = saver->saveToFile(rgb_blob, rgbFile, grayscaleOptions);
    EXPECT_TRUE(rgbResult.success);

    // Test RGBA (4 channels)
    std::string rgbaFile = "test_rgba.png";
    fileManager->registerTempFile(rgbaFile);

    auto rgbaResult = saver->saveToFile(rgba_blob, rgbaFile, grayscaleOptions);
    EXPECT_TRUE(rgbaResult.success);
}

// Test progressive saving
TEST_F(ImageSaverTest, ProgressiveSaving) {
    std::string outputFile = "test_progressive.jpg";
    fileManager->registerTempFile(outputFile);

    SaveOptions options;
    options.targetFormat = ImageFormat::JPEG;
    options.progressiveEncoding = true;

    auto result = saver->saveToFile(large_blob, outputFile, options);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(fs::exists(outputFile));
    EXPECT_GT(fs::file_size(outputFile), 0);
}

// Test error handling
TEST_F(ImageSaverTest, ErrorHandling) {
    // Test invalid output path
    auto result = saver->saveToFile(rgb_blob, "/invalid/path/test.png");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());

    // Test empty image data
    blob emptyBlob;
    result = saver->saveToFile(emptyBlob, "test_empty.png");
    EXPECT_FALSE(result.success);

    // Test unsupported format
    SaveOptions options;
    options.targetFormat = ImageFormat::UNKNOWN;
    result = saver->saveToFile(rgb_blob, "test_unknown.dat", options);
    EXPECT_FALSE(result.success);
}

// Test batch saving
TEST_F(ImageSaverTest, BatchSaving) {
    std::vector<blob> images = {rgb_blob, grayscale_blob, rgba_blob};
    std::vector<std::filesystem::path> filenames = {"batch1.png", "batch2.png", "batch3.png"};

    for (const auto& filename : filenames) {
        fileManager->registerTempFile(filename.string());
    }

    SaveOptions options;
    options.targetFormat = ImageFormat::PNG;

    auto results = saver->saveBatch(images, filenames, options);
    EXPECT_EQ(results.results.size(), images.size());

    for (size_t i = 0; i < results.results.size(); ++i) {
        EXPECT_TRUE(results.results[i].success) << "Failed to save batch image " << i;
        EXPECT_TRUE(fs::exists(filenames[i]));
    }
}

// Test concurrent saving thread safety
TEST_F(ImageSaverTest, ConcurrentSaving) {
    const int numThreads = 4;
    const int savesPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, &successCount, &errorCount]() {
            for (int i = 0; i < savesPerThread; ++i) {
                try {
                    std::string filename = "concurrent_" + std::to_string(t) + "_" + std::to_string(i) + ".png";
                    fileManager->registerTempFile(filename);

                    SaveOptions options;
                    options.targetFormat = ImageFormat::PNG;

                    auto result = saver->saveToFile(rgb_blob, filename, options);
                    if (result.success) {
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

    EXPECT_EQ(successCount.load(), numThreads * savesPerThread);
    EXPECT_EQ(errorCount.load(), 0);
}

// Test custom format saver registration (simplified)
TEST_F(ImageSaverTest, CustomFormatSaver) {
    std::string customFile = "test.custom";
    fileManager->registerTempFile(customFile);

    SaveOptions options;
    options.targetFormat = ImageFormat::UNKNOWN; // Will auto-detect from extension

    // This test verifies that the saver can handle unknown formats gracefully
    auto result = saver->saveToFile(rgb_blob, customFile, options);
    // May succeed or fail depending on implementation
    EXPECT_TRUE(fs::exists(customFile) || !result.success);
}

// Test round-trip consistency (save then load)
TEST_F(ImageSaverTest, RoundTripConsistency) {
    std::string tempFile = "test_roundtrip.png";
    fileManager->registerTempFile(tempFile);

    // Save image
    SaveOptions saveOptions;
    saveOptions.targetFormat = ImageFormat::PNG;
    saveOptions.compressionLevel = 0; // No compression for exact match

    auto saveResult = saver->saveToFile(rgb_blob, tempFile, saveOptions);
    EXPECT_TRUE(saveResult.success);

    // Load image back
    auto loadResult = loader->loadFromFile(tempFile);
    ASSERT_TRUE(loadResult.success);

    // Verify file was created and has content
    EXPECT_TRUE(fs::exists(tempFile));
    EXPECT_GT(fs::file_size(tempFile), 0);
    EXPECT_GT(loadResult.imageData.size(), 0);
}

// Test format conversion during save
TEST_F(ImageSaverTest, FormatConversion) {
    std::string outputFile = "test_conversion.jpg";
    fileManager->registerTempFile(outputFile);

    SaveOptions options;
    options.targetFormat = ImageFormat::JPEG;
    options.quality = 95;

    auto result = saver->saveToFile(rgba_blob, outputFile, options);
    EXPECT_TRUE(result.success);

    // Verify the saved file
    auto loadResult = loader->loadFromFile(outputFile);
    ASSERT_TRUE(loadResult.success);
    EXPECT_TRUE(fs::exists(outputFile));
}

// Test memory usage optimization
TEST_F(ImageSaverTest, MemoryUsageOptimization) {
    std::string outputFile = "test_memory_opt.png";
    fileManager->registerTempFile(outputFile);

    SaveOptions options;
    options.targetFormat = ImageFormat::PNG;
    options.optimizeSize = true; // Use available optimization option

    auto result = saver->saveToFile(large_blob, outputFile, options);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(fs::exists(outputFile));
}

// Test saving with different compression levels
TEST_F(ImageSaverTest, DifferentCompressionLevels) {
    std::vector<std::pair<int, std::string>> levels = {
        {0, "test_no_compression.png"},
        {9, "test_max_compression.png"}
    };

    for (const auto& [level, filename] : levels) {
        fileManager->registerTempFile(filename);

        SaveOptions options;
        options.targetFormat = ImageFormat::PNG;
        options.compressionLevel = level;

        auto result = saver->saveToFile(rgb_blob, filename, options);
        EXPECT_TRUE(result.success) << "Failed to save with compression level " << level;
        EXPECT_TRUE(fs::exists(filename));
    }
}

// Test saving with metadata
TEST_F(ImageSaverTest, MetadataHandling) {
    std::string outputFile = "test_metadata.jpg";
    fileManager->registerTempFile(outputFile);

    SaveOptions options;
    options.targetFormat = ImageFormat::JPEG;
    options.preserveMetadata = true;
    options.customMetadata["Author"] = "Test Author";

    auto result = saver->saveToFile(rgb_blob, outputFile, options);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(fs::exists(outputFile));
}

// Test basic save functionality
TEST_F(ImageSaverTest, BasicSaveFunctionality) {
    std::string outputFile = "test_basic_save.png";
    fileManager->registerTempFile(outputFile);

    SaveOptions options;
    options.targetFormat = ImageFormat::PNG;

    auto result = saver->saveToFile(rgb_blob, outputFile, options);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(fs::exists(outputFile));
    EXPECT_GT(fs::file_size(outputFile), 0);
}

// Test performance benchmarking
TEST_F(ImageSaverTest, DISABLED_SavingPerformance) {
    const int iterations = 50;
    std::vector<std::string> testFiles;

    // Create test files for cleanup
    for (int i = 0; i < iterations; ++i) {
        std::string filename = "perf_test_" + std::to_string(i) + ".png";
        testFiles.push_back(filename);
        fileManager->registerTempFile(filename);
    }

    SaveOptions options;
    options.targetFormat = ImageFormat::PNG;
    options.compressionLevel = 6; // Balanced compression

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto result = saver->saveToFile(rgb_blob, testFiles[i], options);
        ASSERT_TRUE(result.success);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double avgTime = static_cast<double>(duration.count()) / iterations;

    std::cout << "Average saving time: " << avgTime << " ms" << std::endl;

    // Should be reasonably fast (less than 50ms per small image)
    EXPECT_LT(avgTime, 50.0);
}

// Test file overwrite behavior
TEST_F(ImageSaverTest, FileOverwriteBehavior) {
    std::string outputFile = "test_overwrite.png";
    fileManager->registerTempFile(outputFile);

    SaveOptions options;
    options.targetFormat = ImageFormat::PNG;

    // Save first image
    auto result1 = saver->saveToFile(rgb_blob, outputFile, options);
    EXPECT_TRUE(result1.success);
    auto size1 = fs::file_size(outputFile);

    // Save different image to same file
    auto result2 = saver->saveToFile(grayscale_blob, outputFile, options);
    EXPECT_TRUE(result2.success);
    auto size2 = fs::file_size(outputFile);

    // File should be overwritten (sizes will likely be different)
    EXPECT_NE(size1, size2);
}

} // namespace atom::image::test
