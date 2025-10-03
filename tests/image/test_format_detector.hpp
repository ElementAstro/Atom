#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "atom/image/io/format_detector.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace atom::image::test {

class FormatDetectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        detector = std::make_unique<FormatDetector>();
        fileManager = std::make_unique<TestFileManager>();
        
        // Create test files with various formats
        createTestFiles();
    }

    void TearDown() override {
        fileManager->cleanup();
    }

    void createTestFiles() {
        // Create JPEG test file
        jpeg_file = createTestImageFile("test.jpg", {0xFF, 0xD8, 0xFF, 0xE0});
        fileManager->registerTempFile(jpeg_file);

        // Create PNG test file
        png_file = createTestImageFile("test.png", {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A});
        fileManager->registerTempFile(png_file);

        // Create BMP test file
        bmp_file = createTestImageFile("test.bmp", {0x42, 0x4D});
        fileManager->registerTempFile(bmp_file);

        // Create TIFF test file (little endian)
        tiff_file = createTestImageFile("test.tiff", {0x49, 0x49, 0x2A, 0x00});
        fileManager->registerTempFile(tiff_file);

        // Create GIF test file
        gif_file = createTestImageFile("test.gif", {0x47, 0x49, 0x46, 0x38, 0x39, 0x61});
        fileManager->registerTempFile(gif_file);

        // Create WEBP test file
        webp_file = createTestImageFile("test.webp", {0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50});
        fileManager->registerTempFile(webp_file);

        // Create FITS test file
        fits_file = FitsTestDataGenerator::createTempFitsFile(10, 10, 1, 32);
        fileManager->registerTempFile(fits_file);

        // Create unknown format file
        unknown_file = createTestImageFile("test.unknown", {0x00, 0x01, 0x02, 0x03});
        fileManager->registerTempFile(unknown_file);

        // Create empty file
        empty_file = "test_empty.dat";
        std::ofstream(empty_file).close();
        fileManager->registerTempFile(empty_file);
    }

    std::string createTestImageFile(const std::string& filename, const std::vector<uint8_t>& header) {
        std::ofstream file(filename, std::ios::binary);
        
        // Write magic number/header
        for (uint8_t byte : header) {
            file.put(static_cast<char>(byte));
        }
        
        // Add some dummy data
        std::vector<uint8_t> dummyData(100, 0x42);
        file.write(reinterpret_cast<const char*>(dummyData.data()), dummyData.size());
        
        file.close();
        return filename;
    }

    std::unique_ptr<FormatDetector> detector;
    std::unique_ptr<TestFileManager> fileManager;
    
    std::string jpeg_file, png_file, bmp_file, tiff_file, gif_file, webp_file, fits_file, unknown_file, empty_file;
};

// Test format detection by file extension
TEST_F(FormatDetectorTest, DetectFormatByExtension) {
    EXPECT_EQ(detector->detectFormatByExtension("image.jpg"), ImageFormat::JPEG);
    EXPECT_EQ(detector->detectFormatByExtension("image.jpeg"), ImageFormat::JPEG);
    EXPECT_EQ(detector->detectFormatByExtension("image.png"), ImageFormat::PNG);
    EXPECT_EQ(detector->detectFormatByExtension("image.bmp"), ImageFormat::BMP);
    EXPECT_EQ(detector->detectFormatByExtension("image.tiff"), ImageFormat::TIFF);
    EXPECT_EQ(detector->detectFormatByExtension("image.tif"), ImageFormat::TIFF);
    EXPECT_EQ(detector->detectFormatByExtension("image.gif"), ImageFormat::GIF);
    EXPECT_EQ(detector->detectFormatByExtension("image.webp"), ImageFormat::WEBP);
    EXPECT_EQ(detector->detectFormatByExtension("image.fits"), ImageFormat::FITS);
    EXPECT_EQ(detector->detectFormatByExtension("image.unknown"), ImageFormat::UNKNOWN);
}

// Test case insensitive extension detection
TEST_F(FormatDetectorTest, DetectFormatByExtensionCaseInsensitive) {
    EXPECT_EQ(detector->detectFormatByExtension("image.JPG"), ImageFormat::JPEG);
    EXPECT_EQ(detector->detectFormatByExtension("image.PNG"), ImageFormat::PNG);
    EXPECT_EQ(detector->detectFormatByExtension("image.BMP"), ImageFormat::BMP);
    EXPECT_EQ(detector->detectFormatByExtension("image.TIFF"), ImageFormat::TIFF);
    EXPECT_EQ(detector->detectFormatByExtension("image.GIF"), ImageFormat::GIF);
    EXPECT_EQ(detector->detectFormatByExtension("image.WEBP"), ImageFormat::WEBP);
    EXPECT_EQ(detector->detectFormatByExtension("image.FITS"), ImageFormat::FITS);
}

// Test format detection by magic number
TEST_F(FormatDetectorTest, DetectFormatByMagicNumber) {
    auto result = detector->detectFormat(jpeg_file);
    EXPECT_EQ(result.format, ImageFormat::JPEG);
    EXPECT_GE(result.confidence, DetectionConfidence::HIGH);

    result = detector->detectFormat(png_file);
    EXPECT_EQ(result.format, ImageFormat::PNG);
    EXPECT_GE(result.confidence, DetectionConfidence::HIGH);

    result = detector->detectFormat(bmp_file);
    EXPECT_EQ(result.format, ImageFormat::BMP);
    EXPECT_GE(result.confidence, DetectionConfidence::HIGH);

    result = detector->detectFormat(tiff_file);
    EXPECT_EQ(result.format, ImageFormat::TIFF);
    EXPECT_GE(result.confidence, DetectionConfidence::HIGH);

    result = detector->detectFormat(gif_file);
    EXPECT_EQ(result.format, ImageFormat::GIF);
    EXPECT_GE(result.confidence, DetectionConfidence::HIGH);

    result = detector->detectFormat(webp_file);
    EXPECT_EQ(result.format, ImageFormat::WEBP);
    EXPECT_GE(result.confidence, DetectionConfidence::HIGH);

    result = detector->detectFormat(fits_file);
    EXPECT_EQ(result.format, ImageFormat::FITS);
    EXPECT_GE(result.confidence, DetectionConfidence::HIGH);
}

// Test detection of unknown format
TEST_F(FormatDetectorTest, DetectUnknownFormat) {
    auto result = detector->detectFormat(unknown_file);
    EXPECT_EQ(result.format, ImageFormat::UNKNOWN);
    EXPECT_EQ(result.confidence, DetectionConfidence::NONE);
}

// Test detection with empty file
TEST_F(FormatDetectorTest, DetectFormatEmptyFile) {
    auto result = detector->detectFormat(empty_file);
    EXPECT_EQ(result.format, ImageFormat::UNKNOWN);
    EXPECT_EQ(result.confidence, DetectionConfidence::NONE);
}

// Test detection with non-existent file
TEST_F(FormatDetectorTest, DetectFormatNonExistentFile) {
    EXPECT_THROW(detector->detectFormat("non_existent_file.jpg"), std::runtime_error);
}

// Test format detection from memory buffer
TEST_F(FormatDetectorTest, DetectFormatFromBuffer) {
    // JPEG magic number
    std::vector<uint8_t> jpegBuffer = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46};
    auto result = detector->detectFormatFromBuffer(jpegBuffer.data(), jpegBuffer.size());
    EXPECT_EQ(result.format, ImageFormat::JPEG);
    EXPECT_GE(result.confidence, DetectionConfidence::HIGH);

    // PNG magic number
    std::vector<uint8_t> pngBuffer = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    result = detector->detectFormatFromBuffer(pngBuffer.data(), pngBuffer.size());
    EXPECT_EQ(result.format, ImageFormat::PNG);
    EXPECT_GE(result.confidence, DetectionConfidence::HIGH);

    // Unknown buffer
    std::vector<uint8_t> unknownBuffer = {0x00, 0x01, 0x02, 0x03};
    result = detector->detectFormatFromBuffer(unknownBuffer.data(), unknownBuffer.size());
    EXPECT_EQ(result.format, ImageFormat::UNKNOWN);
}

// Test format detection with insufficient data
TEST_F(FormatDetectorTest, DetectFormatInsufficientData) {
    std::vector<uint8_t> smallBuffer = {0xFF};
    auto result = detector->detectFormatFromBuffer(smallBuffer.data(), smallBuffer.size());
    EXPECT_EQ(result.format, ImageFormat::UNKNOWN);
    EXPECT_EQ(result.confidence, DetectionConfidence::NONE);
}

// Test MIME type mapping
TEST_F(FormatDetectorTest, GetMimeType) {
    EXPECT_EQ(detector->getMimeType(ImageFormat::JPEG), "image/jpeg");
    EXPECT_EQ(detector->getMimeType(ImageFormat::PNG), "image/png");
    EXPECT_EQ(detector->getMimeType(ImageFormat::BMP), "image/bmp");
    EXPECT_EQ(detector->getMimeType(ImageFormat::TIFF), "image/tiff");
    EXPECT_EQ(detector->getMimeType(ImageFormat::GIF), "image/gif");
    EXPECT_EQ(detector->getMimeType(ImageFormat::WEBP), "image/webp");
    EXPECT_EQ(detector->getMimeType(ImageFormat::FITS), "image/fits");
    EXPECT_EQ(detector->getMimeType(ImageFormat::UNKNOWN), "application/octet-stream");
}

// Test file extension mapping
TEST_F(FormatDetectorTest, GetFileExtensions) {
    auto extensions = detector->getFileExtensions(ImageFormat::JPEG);
    EXPECT_THAT(extensions, ::testing::Contains("jpg"));
    EXPECT_THAT(extensions, ::testing::Contains("jpeg"));

    extensions = detector->getFileExtensions(ImageFormat::PNG);
    EXPECT_THAT(extensions, ::testing::Contains("png"));

    extensions = detector->getFileExtensions(ImageFormat::TIFF);
    EXPECT_THAT(extensions, ::testing::Contains("tiff"));
    EXPECT_THAT(extensions, ::testing::Contains("tif"));

    extensions = detector->getFileExtensions(ImageFormat::UNKNOWN);
    EXPECT_TRUE(extensions.empty());
}

// Test format description
TEST_F(FormatDetectorTest, GetFormatDescription) {
    EXPECT_FALSE(detector->getFormatDescription(ImageFormat::JPEG).empty());
    EXPECT_FALSE(detector->getFormatDescription(ImageFormat::PNG).empty());
    EXPECT_FALSE(detector->getFormatDescription(ImageFormat::BMP).empty());
    EXPECT_FALSE(detector->getFormatDescription(ImageFormat::TIFF).empty());
    EXPECT_FALSE(detector->getFormatDescription(ImageFormat::GIF).empty());
    EXPECT_FALSE(detector->getFormatDescription(ImageFormat::WEBP).empty());
    EXPECT_FALSE(detector->getFormatDescription(ImageFormat::FITS).empty());
}

// Test format support checking
TEST_F(FormatDetectorTest, IsFormatSupported) {
    EXPECT_TRUE(detector->isFormatSupported(ImageFormat::JPEG));
    EXPECT_TRUE(detector->isFormatSupported(ImageFormat::PNG));
    EXPECT_TRUE(detector->isFormatSupported(ImageFormat::BMP));
    EXPECT_TRUE(detector->isFormatSupported(ImageFormat::TIFF));
    EXPECT_TRUE(detector->isFormatSupported(ImageFormat::GIF));
    EXPECT_FALSE(detector->isFormatSupported(ImageFormat::UNKNOWN));
}

// Test getting all supported formats
TEST_F(FormatDetectorTest, GetSupportedFormats) {
    auto formats = detector->getSupportedFormats();
    EXPECT_THAT(formats, ::testing::Contains(ImageFormat::JPEG));
    EXPECT_THAT(formats, ::testing::Contains(ImageFormat::PNG));
    EXPECT_THAT(formats, ::testing::Contains(ImageFormat::BMP));
    EXPECT_THAT(formats, ::testing::Contains(ImageFormat::TIFF));
    EXPECT_THAT(formats, ::testing::Not(::testing::Contains(ImageFormat::UNKNOWN)));
}

// Test custom format registration
TEST_F(FormatDetectorTest, RegisterCustomFormat) {
    // Register a custom format
    MagicSignature customSignature;
    customSignature.signature = {0xCA, 0xFE, 0xBA, 0xBE};
    customSignature.offset = 0;
    customSignature.format = ImageFormat::CUSTOM;
    customSignature.confidence = DetectionConfidence::HIGH;
    customSignature.description = "Custom test format";

    detector->registerCustomFormat(customSignature);

    // Create test file with custom signature
    std::string customFile = createTestImageFile("test.custom", {0xCA, 0xFE, 0xBA, 0xBE});
    fileManager->registerTempFile(customFile);

    // Test detection
    auto result = detector->detectFormat(customFile);
    EXPECT_EQ(result.format, ImageFormat::CUSTOM);
    EXPECT_EQ(result.confidence, DetectionConfidence::HIGH);
}

// Test format detection with offset magic numbers
TEST_F(FormatDetectorTest, DetectFormatWithOffset) {
    // Create file with magic number at offset 4
    std::string offsetFile = "test_offset.dat";
    std::ofstream file(offsetFile, std::ios::binary);

    // Write 4 bytes of padding
    file.write("\x00\x00\x00\x00", 4);
    // Write JPEG magic number
    file.write("\xFF\xD8\xFF\xE0", 4);
    // Write some data
    std::vector<uint8_t> data(100, 0x42);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();

    fileManager->registerTempFile(offsetFile);

    // Register custom signature with offset
    MagicSignature offsetSignature;
    offsetSignature.signature = {0xFF, 0xD8, 0xFF, 0xE0};
    offsetSignature.offset = 4;
    offsetSignature.format = ImageFormat::JPEG;
    offsetSignature.confidence = DetectionConfidence::HIGH;
    offsetSignature.description = "JPEG with offset";

    detector->registerCustomFormat(offsetSignature);

    auto result = detector->detectFormat(offsetFile);
    EXPECT_EQ(result.format, ImageFormat::JPEG);
}

// Test batch format detection
TEST_F(FormatDetectorTest, BatchFormatDetection) {
    std::vector<std::string> files = {jpeg_file, png_file, bmp_file, unknown_file};
    auto results = detector->detectFormats(files);

    EXPECT_EQ(results.size(), files.size());
    EXPECT_EQ(results[0].format, ImageFormat::JPEG);
    EXPECT_EQ(results[1].format, ImageFormat::PNG);
    EXPECT_EQ(results[2].format, ImageFormat::BMP);
    EXPECT_EQ(results[3].format, ImageFormat::UNKNOWN);
}

// Test format detection with corrupted files
TEST_F(FormatDetectorTest, DetectFormatCorruptedFile) {
    // Create file with partial JPEG header
    std::string corruptedFile = "test_corrupted.jpg";
    std::ofstream file(corruptedFile, std::ios::binary);
    file.write("\xFF\xD8", 2); // Incomplete JPEG header
    file.close();

    fileManager->registerTempFile(corruptedFile);

    auto result = detector->detectFormat(corruptedFile);
    // Should still detect as JPEG based on partial signature
    EXPECT_EQ(result.format, ImageFormat::JPEG);
    EXPECT_LT(result.confidence, DetectionConfidence::HIGH); // But with lower confidence
}

// Test format detection performance
TEST_F(FormatDetectorTest, DISABLED_DetectionPerformance) {
    const int iterations = 1000;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        detector->detectFormat(jpeg_file);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avgTime = static_cast<double>(duration.count()) / iterations;

    // Should be fast (less than 100 microseconds per detection on average)
    EXPECT_LT(avgTime, 100.0);

    std::cout << "Average detection time: " << avgTime << " microseconds" << std::endl;
}

// Test thread safety
TEST_F(FormatDetectorTest, ThreadSafety) {
    const int numThreads = 4;
    const int operationsPerThread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, operationsPerThread, &successCount, &errorCount]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                try {
                    auto result = detector->detectFormat(jpeg_file);
                    if (result.format == ImageFormat::JPEG) {
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

    EXPECT_EQ(successCount.load(), numThreads * operationsPerThread);
    EXPECT_EQ(errorCount.load(), 0);
}

// Test format detection with very large files
TEST_F(FormatDetectorTest, DetectFormatLargeFile) {
    std::string largeFile = "test_large.jpg";
    std::ofstream file(largeFile, std::ios::binary);

    // Write JPEG header
    file.write("\xFF\xD8\xFF\xE0", 4);

    // Write large amount of data (1MB)
    std::vector<uint8_t> largeData(1024 * 1024, 0x42);
    file.write(reinterpret_cast<const char*>(largeData.data()), largeData.size());
    file.close();

    fileManager->registerTempFile(largeFile);

    // Detection should still work efficiently
    auto start = std::chrono::high_resolution_clock::now();
    auto result = detector->detectFormat(largeFile);
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(result.format, ImageFormat::JPEG);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // Should be fast even for large files (detection only reads header)
    EXPECT_LT(duration.count(), 100);
}

} // namespace atom::image::test
