#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "atom/image/formats/fits_utils.hpp"
#include "test_utils.hpp"

#ifdef ATOM_ENABLE_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

namespace atom::image::test {

class FitsUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        fileManager = std::make_unique<TestFileManager>();

        // Create test FITS files
        createTestFitsFiles();
    }

    void TearDown() override { fileManager->cleanup(); }

    void createTestFitsFiles() {
        // Create a simple FITS file for testing
        test_fits_file =
            FitsTestDataGenerator::createTempFitsFile(32, 32, 1, 16);
        fileManager->registerTempFile(test_fits_file);

        // Create a multi-channel FITS file
        multi_channel_fits_file =
            FitsTestDataGenerator::createTempFitsFile(64, 64, 3, 16);
        fileManager->registerTempFile(multi_channel_fits_file);

        // Create a large FITS file
        large_fits_file =
            FitsTestDataGenerator::createTempFitsFile(256, 256, 1, 32);
        fileManager->registerTempFile(large_fits_file);
    }

    std::unique_ptr<TestFileManager> fileManager;
    std::string test_fits_file, multi_channel_fits_file, large_fits_file;
};

// Test FitsImage default constructor
TEST_F(FitsUtilsTest, DefaultConstructor) {
    FitsImage image;

    auto [width, height, channels] = image.getSize();
    // Default constructor should create empty image
    EXPECT_EQ(width, 0);
    EXPECT_EQ(height, 0);
    EXPECT_EQ(channels, 0);
}

// Test FitsImage constructor with file
TEST_F(FitsUtilsTest, ConstructorWithFile) {
    FitsImage image(test_fits_file);

    auto [width, height, channels] = image.getSize();
    EXPECT_EQ(width, 32);
    EXPECT_EQ(height, 32);
    EXPECT_EQ(channels, 1);
}

// Test FitsImage constructor with dimensions
TEST_F(FitsUtilsTest, ConstructorWithDimensions) {
    FitsImage image(100, 200, 3, DataType::FLOAT);

    auto [width, height, channels] = image.getSize();
    EXPECT_EQ(width, 100);
    EXPECT_EQ(height, 200);
    EXPECT_EQ(channels, 3);
}

// Test image loading and saving
TEST_F(FitsUtilsTest, LoadAndSave) {
    FitsImage image(test_fits_file);

    std::string outputFile = "test_output.fits";
    fileManager->registerTempFile(outputFile);

    EXPECT_NO_THROW(image.save(outputFile));
    EXPECT_TRUE(std::filesystem::exists(outputFile));

    // Load the saved image and verify
    FitsImage loadedImage(outputFile);
    auto [width1, height1, channels1] = image.getSize();
    auto [width2, height2, channels2] = loadedImage.getSize();

    EXPECT_EQ(width1, width2);
    EXPECT_EQ(height1, height2);
    EXPECT_EQ(channels1, channels2);
}

// Test image resizing
TEST_F(FitsUtilsTest, ImageResizing) {
    FitsImage image(test_fits_file);

    auto [originalWidth, originalHeight, channels] = image.getSize();
    EXPECT_EQ(originalWidth, 32);
    EXPECT_EQ(originalHeight, 32);

    // Resize image
    image.resize(64, 64);

    auto [newWidth, newHeight, newChannels] = image.getSize();
    EXPECT_EQ(newWidth, 64);
    EXPECT_EQ(newHeight, 64);
    EXPECT_EQ(newChannels, channels);  // Channels should remain the same
}

// Test image cropping
TEST_F(FitsUtilsTest, ImageCropping) {
    FitsImage image(test_fits_file);

    // Crop a 16x16 region from the center
    image.crop(8, 8, 16, 16);

    auto [width, height, channels] = image.getSize();
    EXPECT_EQ(width, 16);
    EXPECT_EQ(height, 16);
}

// Test pixel value access
TEST_F(FitsUtilsTest, PixelValueAccess) {
    FitsImage image(10, 10, 1, DataType::FLOAT);

    // Set a pixel value
    image.setPixel(5, 5, 0, 42.5f);

    // Get the pixel value
    float value = image.getPixel(5, 5, 0);
    EXPECT_FLOAT_EQ(value, 42.5f);
}

// Test image statistics
TEST_F(FitsUtilsTest, ImageStatistics) {
    FitsImage image(test_fits_file);

    auto stats = image.getStatistics();

    // Basic validation of statistics
    EXPECT_GE(stats.min, 0.0);
    EXPECT_LE(stats.max, 65535.0);  // Assuming 16-bit data
    EXPECT_GE(stats.mean, stats.min);
    EXPECT_LE(stats.mean, stats.max);
    EXPECT_GE(stats.stddev, 0.0);
}

// Test histogram calculation
TEST_F(FitsUtilsTest, HistogramCalculation) {
    FitsImage image(test_fits_file);

    auto histogram = image.getHistogram(256);

    EXPECT_EQ(histogram.size(), 256);

    // Sum of histogram should equal total pixels
    int totalPixels = 0;
    for (const auto& count : histogram) {
        totalPixels += count;
    }

    auto [width, height, channels] = image.getSize();
    EXPECT_EQ(totalPixels, width * height * channels);
}

// Test image normalization
TEST_F(FitsUtilsTest, ImageNormalization) {
    FitsImage image(test_fits_file);

    // Normalize to 0-1 range
    image.normalize(0.0, 1.0);

    auto stats = image.getStatistics();
    EXPECT_GE(stats.min, 0.0);
    EXPECT_LE(stats.max, 1.0);
}

// Test image arithmetic operations
TEST_F(FitsUtilsTest, ArithmeticOperations) {
    FitsImage image1(10, 10, 1, DataType::FLOAT);
    FitsImage image2(10, 10, 1, DataType::FLOAT);

    // Fill images with test values
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            image1.setPixel(x, y, 0, 10.0f);
            image2.setPixel(x, y, 0, 5.0f);
        }
    }

    // Test addition
    auto result = image1.add(image2);
    EXPECT_FLOAT_EQ(result.getPixel(0, 0, 0), 15.0f);

    // Test subtraction
    result = image1.subtract(image2);
    EXPECT_FLOAT_EQ(result.getPixel(0, 0, 0), 5.0f);

    // Test multiplication
    result = image1.multiply(image2);
    EXPECT_FLOAT_EQ(result.getPixel(0, 0, 0), 50.0f);

    // Test division
    result = image1.divide(image2);
    EXPECT_FLOAT_EQ(result.getPixel(0, 0, 0), 2.0f);
}

#ifdef ATOM_ENABLE_OPENCV
// Test OpenCV integration
TEST_F(FitsUtilsTest, OpenCVIntegration) {
    // Create OpenCV Mat
    cv::Mat mat = cv::Mat::ones(50, 50, CV_32F) * 100.0f;

    // Create FitsImage from Mat
    FitsImage image(mat);

    auto [width, height, channels] = image.getSize();
    EXPECT_EQ(width, 50);
    EXPECT_EQ(height, 50);
    EXPECT_EQ(channels, 1);

    // Convert back to Mat
    cv::Mat resultMat = image.toMat();
    EXPECT_EQ(resultMat.rows, 50);
    EXPECT_EQ(resultMat.cols, 50);

    // Verify pixel values
    EXPECT_FLOAT_EQ(resultMat.at<float>(0, 0), 100.0f);
}

// Test OpenCV filter application
TEST_F(FitsUtilsTest, OpenCVFilterApplication) {
    FitsImage image(test_fits_file);

    // Apply Gaussian blur filter
    auto gaussianFilter = [](const cv::Mat& input) -> cv::Mat {
        cv::Mat output;
        cv::GaussianBlur(input, output, cv::Size(5, 5), 1.0);
        return output;
    };

    EXPECT_NO_THROW(image.applyOpenCVFilter(gaussianFilter));

    // Image should still have the same dimensions
    auto [width, height, channels] = image.getSize();
    EXPECT_EQ(width, 32);
    EXPECT_EQ(height, 32);
}

// Test OpenCV processing functions
TEST_F(FitsUtilsTest, OpenCVProcessing) {
    FitsImage image(test_fits_file);

    // Test edge detection
    std::map<std::string, double> params = {{"threshold1", 100.0},
                                            {"threshold2", 200.0}};
    EXPECT_NO_THROW(image.processWithOpenCV("Canny", params));

    // Test morphological operations
    params = {{"kernelSize", 3.0}, {"iterations", 1.0}};
    EXPECT_NO_THROW(image.processWithOpenCV("erode", params));
}
#endif

// Test error handling
TEST_F(FitsUtilsTest, ErrorHandling) {
    // Test loading non-existent file
    EXPECT_THROW(FitsImage image("non_existent_file.fits"), std::exception);

    // Test invalid dimensions
    EXPECT_THROW(FitsImage image(-10, 20), std::exception);
    EXPECT_THROW(FitsImage image(10, -20), std::exception);

    // Test invalid pixel access
    FitsImage image(10, 10, 1);
    EXPECT_THROW(image.getPixel(15, 5, 0), std::exception);
    EXPECT_THROW(image.setPixel(5, 15, 0, 1.0f), std::exception);
}

// Test multi-channel operations
TEST_F(FitsUtilsTest, MultiChannelOperations) {
    FitsImage image(multi_channel_fits_file);

    auto [width, height, channels] = image.getSize();
    EXPECT_EQ(width, 64);
    EXPECT_EQ(height, 64);
    EXPECT_EQ(channels, 3);

    // Test channel extraction
    auto redChannel = image.extractChannel(0);
    auto [redWidth, redHeight, redChannels] = redChannel.getSize();
    EXPECT_EQ(redWidth, width);
    EXPECT_EQ(redHeight, height);
    EXPECT_EQ(redChannels, 1);

    // Test channel combination
    std::vector<FitsImage> channelImages = {image.extractChannel(0),
                                            image.extractChannel(1),
                                            image.extractChannel(2)};

    auto combinedImage = FitsImage::combineChannels(channelImages);
    auto [combWidth, combHeight, combChannels] = combinedImage.getSize();
    EXPECT_EQ(combWidth, width);
    EXPECT_EQ(combHeight, height);
    EXPECT_EQ(combChannels, 3);
}

// Test performance with large images
TEST_F(FitsUtilsTest, DISABLED_PerformanceTest) {
    FitsImage image(large_fits_file);

    auto start = std::chrono::high_resolution_clock::now();

    // Perform some operations
    image.resize(128, 128);
    auto stats = image.getStatistics();
    image.normalize(0.0, 1.0);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Large image processing time: " << duration.count() << " ms"
              << std::endl;

    // Should complete in reasonable time (less than 1 second)
    EXPECT_LT(duration.count(), 1000);
}

// Test memory management
TEST_F(FitsUtilsTest, MemoryManagement) {
    // Test that images are properly cleaned up
    {
        FitsImage image(large_fits_file);
        auto [width, height, channels] = image.getSize();
        EXPECT_GT(width * height * channels, 0);
        // Image should be automatically cleaned up when going out of scope
    }

    // Test multiple image operations
    for (int i = 0; i < 10; ++i) {
        FitsImage image(test_fits_file);
        image.resize(64, 64);
        auto stats = image.getStatistics();
        EXPECT_GE(stats.mean, 0.0);
    }
}

}  // namespace atom::image::test
