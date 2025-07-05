// filepath: atom/image/test_hdu.hpp

#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include <tuple>

#include "hdu.hpp"
#include "fits_header.hpp"
#include "fits_data.hpp"

namespace fs = std::filesystem;

// Helper function to create a temporary FITS file for testing
inline std::string createTempFitsFile(int width = 10, int height = 10, int channels = 1) {
    // Create a temporary file
    std::string tempFilePath = (fs::temp_directory_path() /
                               fs::path("test_hdu_temp_" + std::to_string(std::random_device{}()) + ".fits")).string();

    // Create a simple FITS file with basic header
    std::ofstream outFile(tempFilePath, std::ios::binary);

    // Write FITS header
    outFile << "SIMPLE  =                    T / Standard FITS format" << std::string(80-44, ' ') << std::endl;
    outFile << "BITPIX  =                   32 / Bits per pixel" << std::string(80-42, ' ') << std::endl;
    outFile << "NAXIS   =                    2 / Number of axes" << std::string(80-42, ' ') << std::endl;
    outFile << "NAXIS1  =                   " << std::setw(2) << width << " / Width" << std::string(80-38, ' ') << std::endl;
    outFile << "NAXIS2  =                   " << std::setw(2) << height << " / Height" << std::string(80-39, ' ') << std::endl;
    if (channels > 1) {
        outFile << "NAXIS3  =                   " << std::setw(2) << channels << " / Channels" << std::string(80-41, ' ') << std::endl;
    }
    outFile << "END" << std::string(80-3, ' ') << std::endl;

    // Pad header to multiple of 2880 bytes
    int headerBlocks = 1; // Start with one for the header we've already written
    int bytesWritten = headerBlocks * 2880;
    int paddingRequired = bytesWritten - (7 * 80); // 7 header cards written so far
    outFile << std::string(paddingRequired, ' ');

    // Write simple data (all zeros)
    int dataSize = width * height * channels * sizeof(int32_t);
    std::vector<int32_t> dummyData(width * height * channels, 0);
    outFile.write(reinterpret_cast<const char*>(dummyData.data()), dataSize);

    // Pad data to multiple of 2880 bytes
    int dataPaddingRequired = (2880 - (dataSize % 2880)) % 2880;
    outFile << std::string(dataPaddingRequired, '\0');

    outFile.close();
    return tempFilePath;
}

class ImageHDUTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp files with different dimensions
        tempFilePaths.push_back(createTempFitsFile(10, 10, 1));  // Grayscale image
        tempFilePaths.push_back(createTempFitsFile(10, 10, 3));  // RGB image
        tempFilePaths.push_back(createTempFitsFile(100, 100, 1)); // Larger image
    }

    void TearDown() override {
        // Clean up all created temp files
        for (const auto& path : tempFilePaths) {
            std::remove(path.c_str());
        }
    }

    // Fill an ImageHDU with test data
    template <FitsNumeric T>
    void fillTestData(ImageHDU& hdu, int width, int height, int channels) {
        hdu.setImageSize(width, height, channels);

        // Fill with test pattern
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < channels; ++c) {
                    T value = static_cast<T>((x + y * 2) % 255);
                    hdu.setPixel<T>(x, y, value, c);
                }
            }
        }
    }

    // Create an ImageHDU with test pattern and specific data type
    template <FitsNumeric T>
    std::unique_ptr<ImageHDU> createTestImageHDU(int width, int height, int channels = 1) {
        auto hdu = std::make_unique<ImageHDU>();
        fillTestData<T>(*hdu, width, height, channels);
        return hdu;
    }

    std::vector<std::string> tempFilePaths;
};

// Test basic reading from a file
TEST_F(ImageHDUTest, ReadHDUFromFile) {
    ImageHDU hdu;
    std::ifstream file(tempFilePaths[0], std::ios::binary);

    ASSERT_NO_THROW(hdu.readHDU(file));

    auto [width, height, channels] = hdu.getImageSize();
    EXPECT_EQ(width, 10);
    EXPECT_EQ(height, 10);
    EXPECT_EQ(channels, 1);
}

// Test reading a multi-channel image
TEST_F(ImageHDUTest, ReadMultiChannelHDU) {
    ImageHDU hdu;
    std::ifstream file(tempFilePaths[1], std::ios::binary);

    ASSERT_NO_THROW(hdu.readHDU(file));

    auto [width, height, channels] = hdu.getImageSize();
    EXPECT_EQ(width, 10);
    EXPECT_EQ(height, 10);
    EXPECT_EQ(channels, 3);
    EXPECT_TRUE(hdu.isColor());
}

// Test writing an HDU to a file
TEST_F(ImageHDUTest, WriteHDUToFile) {
    // Create a test HDU with data
    auto hdu = createTestImageHDU<int32_t>(20, 15, 1);

    // Write to a new file
    std::string outputPath = (fs::temp_directory_path() / "test_hdu_write.fits").string();
    std::ofstream outputFile(outputPath, std::ios::binary);

    ASSERT_NO_THROW(hdu->writeHDU(outputFile));
    outputFile.close();

    // Read it back to verify
    ImageHDU readHdu;
    std::ifstream inputFile(outputPath, std::ios::binary);
    ASSERT_NO_THROW(readHdu.readHDU(inputFile));

    auto [width, height, channels] = readHdu.getImageSize();
    EXPECT_EQ(width, 20);
    EXPECT_EQ(height, 15);
    EXPECT_EQ(channels, 1);

    // Clean up
    std::remove(outputPath.c_str());
}

// Test header keyword operations
TEST_F(ImageHDUTest, HeaderKeywords) {
    ImageHDU hdu;
    std::ifstream file(tempFilePaths[0], std::ios::binary);
    hdu.readHDU(file);

    // Set and get a keyword
    hdu.setHeaderKeyword("OBSERVER", "Test User");
    EXPECT_EQ(hdu.getHeaderKeyword("OBSERVER"), "Test User");

    // Should have standard FITS keywords
    EXPECT_EQ(hdu.getHeaderKeyword("SIMPLE"), "T");
    EXPECT_EQ(hdu.getHeaderKeyword("BITPIX"), "32");
    EXPECT_EQ(hdu.getHeaderKeyword("NAXIS"), "2");
}

// Test setting and getting image dimensions
TEST_F(ImageHDUTest, ImageDimensions) {
    ImageHDU hdu;

    ASSERT_NO_THROW(hdu.setImageSize(30, 40, 2));

    auto [width, height, channels] = hdu.getImageSize();
    EXPECT_EQ(width, 30);
    EXPECT_EQ(height, 40);
    EXPECT_EQ(channels, 2);
    EXPECT_TRUE(hdu.isColor());
    EXPECT_EQ(hdu.getChannelCount(), 2);

    // Test invalid dimensions
    EXPECT_THROW(hdu.setImageSize(-5, 40), std::invalid_argument);
    EXPECT_THROW(hdu.setImageSize(30, 0), std::invalid_argument);
    EXPECT_THROW(hdu.setImageSize(10, 10, -1), std::invalid_argument);
}

// Test pixel access operations for different data types
TEST_F(ImageHDUTest, PixelAccess_Int32) {
    auto hdu = createTestImageHDU<int32_t>(15, 10);

    // Check a few pixels
    EXPECT_EQ(hdu->getPixel<int32_t>(5, 5), (5 + 5 * 2) % 255);
    EXPECT_EQ(hdu->getPixel<int32_t>(0, 0), 0);
    EXPECT_EQ(hdu->getPixel<int32_t>(9, 9), (9 + 9 * 2) % 255);

    // Modify a pixel
    hdu->setPixel<int32_t>(5, 5, 123);
    EXPECT_EQ(hdu->getPixel<int32_t>(5, 5), 123);

    // Out of bounds access
    EXPECT_THROW(hdu->getPixel<int32_t>(15, 5), std::out_of_range);
    EXPECT_THROW(hdu->getPixel<int32_t>(5, 15), std::out_of_range);
    EXPECT_THROW(hdu->setPixel<int32_t>(20, 5, 100), std::out_of_range);
}

TEST_F(ImageHDUTest, PixelAccess_Float) {
    auto hdu = createTestImageHDU<float>(15, 10);

    // Check a few pixels
    EXPECT_FLOAT_EQ(hdu->getPixel<float>(5, 5), static_cast<float>((5 + 5 * 2) % 255));
    EXPECT_FLOAT_EQ(hdu->getPixel<float>(0, 0), 0.0f);

    // Modify a pixel
    hdu->setPixel<float>(5, 5, 123.45f);
    EXPECT_FLOAT_EQ(hdu->getPixel<float>(5, 5), 123.45f);

    // Invalid channel access
    EXPECT_THROW(hdu->getPixel<float>(5, 5, 1), std::out_of_range);
}

TEST_F(ImageHDUTest, PixelAccess_Double) {
    auto hdu = createTestImageHDU<double>(15, 10, 3);

    // Check multi-channel access
    for (int c = 0; c < 3; ++c) {
        EXPECT_DOUBLE_EQ(hdu->getPixel<double>(5, 5, c), static_cast<double>((5 + 5 * 2) % 255));
    }

    // Modify different channels
    hdu->setPixel<double>(5, 5, 100.5, 0);
    hdu->setPixel<double>(5, 5, 200.5, 1);
    hdu->setPixel<double>(5, 5, 300.5, 2);

    EXPECT_DOUBLE_EQ(hdu->getPixel<double>(5, 5, 0), 100.5);
    EXPECT_DOUBLE_EQ(hdu->getPixel<double>(5, 5, 1), 200.5);
    EXPECT_DOUBLE_EQ(hdu->getPixel<double>(5, 5, 2), 300.5);
}

// Test image statistics computation
TEST_F(ImageHDUTest, ComputeImageStats_Int) {
    auto hdu = createTestImageHDU<int32_t>(20, 10);

    auto stats = hdu->computeImageStats<int32_t>();

    // Check basic stats properties
    EXPECT_LE(stats.min, stats.max);
    EXPECT_GE(stats.mean, static_cast<double>(stats.min));
    EXPECT_LE(stats.mean, static_cast<double>(stats.max));
    EXPECT_GE(stats.stddev, 0.0);

    // For our pattern, we know some properties
    EXPECT_EQ(stats.min, 0);
    EXPECT_EQ(stats.max, 57); // (19 + 19*2) % 255 = 57
}

TEST_F(ImageHDUTest, ComputeImageStats_Float) {
    auto hdu = createTestImageHDU<float>(20, 10, 2);

    // Check stats for each channel
    for (int channel = 0; channel < 2; ++channel) {
        auto stats = hdu->computeImageStats<float>(channel);

        EXPECT_FLOAT_EQ(stats.min, 0.0f);
        EXPECT_FLOAT_EQ(stats.max, 57.0f);  // (19 + 19*2) % 255 = 57
        EXPECT_GT(stats.mean, 0.0);
        EXPECT_GT(stats.stddev, 0.0);
    }

    // Invalid channel
    EXPECT_THROW(hdu->computeImageStats<float>(2), std::out_of_range);
}

// Test convolution filtering
TEST_F(ImageHDUTest, ApplyFilter) {
    auto hdu = createTestImageHDU<float>(20, 10);

    // Create a simple box blur kernel (3x3)
    std::vector<double> kernelData = {
        1.0/9.0, 1.0/9.0, 1.0/9.0,
        1.0/9.0, 1.0/9.0, 1.0/9.0,
        1.0/9.0, 1.0/9.0, 1.0/9.0
    };

    std::vector<std::span<const double>> kernel;
    for (int i = 0; i < 3; ++i) {
        kernel.push_back(std::span<const double>(&kernelData[i*3], 3));
    }

    // Store original value for comparison
    float originalValue = hdu->getPixel<float>(5, 5);

    // Apply filter
    ASSERT_NO_THROW(hdu->applyFilter<float>(kernel));

    // After box blur, center pixels should be the average of their neighborhood
    // But exact equality can be affected by boundary conditions, so we just verify it changed
    EXPECT_NE(hdu->getPixel<float>(5, 5), originalValue);
}

// Test parallel filtering
TEST_F(ImageHDUTest, ApplyFilterParallel) {
    auto hdu = createTestImageHDU<float>(50, 50); // Larger image for parallel processing

    // Create a simple box blur kernel (3x3)
    std::vector<double> kernelData = {
        1.0/9.0, 1.0/9.0, 1.0/9.0,
        1.0/9.0, 1.0/9.0, 1.0/9.0,
        1.0/9.0, 1.0/9.0, 1.0/9.0
    };

    std::vector<std::span<const double>> kernel;
    for (int i = 0; i < 3; ++i) {
        kernel.push_back(std::span<const double>(&kernelData[i*3], 3));
    }

    // Store original values at several positions
    std::vector<float> originalValues;
    for (int y = 10; y < 40; y += 10) {
        for (int x = 10; x < 40; x += 10) {
            originalValues.push_back(hdu->getPixel<float>(x, y));
        }
    }

    // Apply parallel filter
    ASSERT_NO_THROW(hdu->applyFilterParallel<float>(kernel));

    // Check that values have changed
    int idx = 0;
    for (int y = 10; y < 40; y += 10) {
        for (int x = 10; x < 40; x += 10) {
            EXPECT_NE(hdu->getPixel<float>(x, y), originalValues[idx++]);
        }
    }
}

// Test image resizing
TEST_F(ImageHDUTest, Resize) {
    auto hdu = createTestImageHDU<float>(20, 10);

    // Resize to larger dimensions
    ASSERT_NO_THROW(hdu->resize<float>(40, 20));

    auto [width, height, channels] = hdu->getImageSize();
    EXPECT_EQ(width, 40);
    EXPECT_EQ(height, 20);

    // Resize to smaller dimensions
    ASSERT_NO_THROW(hdu->resize<float>(10, 5));

    std::tie(width, height, channels) = hdu->getImageSize();
    EXPECT_EQ(width, 10);
    EXPECT_EQ(height, 5);

    // Invalid dimensions
    EXPECT_THROW(hdu->resize<float>(0, 20), std::invalid_argument);
    EXPECT_THROW(hdu->resize<float>(10, -5), std::invalid_argument);
}

// Test thumbnail creation
TEST_F(ImageHDUTest, CreateThumbnail) {
    auto hdu = createTestImageHDU<float>(100, 50);

    // Create a thumbnail with max size 20
    auto thumbnail = hdu->createThumbnail<float>(20);
    ASSERT_NE(thumbnail, nullptr);

    auto [width, height, channels] = thumbnail->getImageSize();

    // The width should be 20 and height should be proportionally scaled
    EXPECT_EQ(width, 20);
    EXPECT_EQ(height, 10); // 50/100 * 20 = 10

    // Test with invalid size
    EXPECT_THROW(hdu->createThumbnail<float>(0), std::invalid_argument);
}

// Test ROI extraction
TEST_F(ImageHDUTest, ExtractROI) {
    auto hdu = createTestImageHDU<int32_t>(30, 20);

    // Extract a region
    auto roi = hdu->extractROI<int32_t>(5, 5, 10, 8);
    ASSERT_NE(roi, nullptr);

    auto [width, height, channels] = roi->getImageSize();
    EXPECT_EQ(width, 10);
    EXPECT_EQ(height, 8);

    // Check that the ROI data matches the original in that region
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 10; ++x) {
            EXPECT_EQ(roi->getPixel<int32_t>(x, y),
                      hdu->getPixel<int32_t>(x + 5, y + 5));
        }
    }

    // Test invalid ROI parameters
    EXPECT_THROW(hdu->extractROI<int32_t>(-1, 5, 10, 8), std::out_of_range);
    EXPECT_THROW(hdu->extractROI<int32_t>(5, 5, 50, 8), std::out_of_range);
    EXPECT_THROW(hdu->extractROI<int32_t>(5, 5, 10, 0), std::invalid_argument);
}

// Test async statistics computation
TEST_F(ImageHDUTest, ComputeImageStatsAsync) {
    auto hdu = createTestImageHDU<float>(100, 100); // Larger image for async test

    // Compute stats asynchronously
    auto statsTask = hdu->computeImageStatsAsync<float>();

    // Get the result
    auto stats = statsTask.get_result();

    // Check basic stats properties
    EXPECT_LE(stats.min, stats.max);
    EXPECT_GE(stats.mean, static_cast<double>(stats.min));
    EXPECT_LE(stats.mean, static_cast<double>(stats.max));
    EXPECT_GE(stats.stddev, 0.0);
}

// Test image blending
TEST_F(ImageHDUTest, BlendImage) {
    auto hdu1 = createTestImageHDU<float>(20, 10);
    auto hdu2 = createTestImageHDU<float>(20, 10);

    // Modify hdu2 to have different values
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 20; ++x) {
            hdu2->setPixel<float>(x, y, 200.0f);
        }
    }

    // Blend with 50% of each
    ASSERT_NO_THROW(hdu1->blendImage<float>(*hdu2, 0.5));

    // Check a sample point - should be halfway between original and 200
    float originalValue = static_cast<float>((5 + 5 * 2) % 255);
    float expectedValue = originalValue * 0.5f + 200.0f * 0.5f;
    EXPECT_FLOAT_EQ(hdu1->getPixel<float>(5, 5), expectedValue);

    // Test invalid alpha
    EXPECT_THROW(hdu1->blendImage<float>(*hdu2, -0.1), std::invalid_argument);
    EXPECT_THROW(hdu1->blendImage<float>(*hdu2, 1.5), std::invalid_argument);

    // Test incompatible images
    auto hdu3 = createTestImageHDU<float>(30, 10);
    EXPECT_THROW(hdu1->blendImage<float>(*hdu3, 0.5), ImageProcessingException);
}

// Test mathematical operations
TEST_F(ImageHDUTest, ApplyMathOperation) {
    auto hdu = createTestImageHDU<float>(20, 10);

    // Apply a multiply-by-2 operation
    ASSERT_NO_THROW(hdu->applyMathOperation<float>([](float val) { return val * 2.0f; }));

    // Check a sample point
    float originalValue = static_cast<float>((5 + 5 * 2) % 255);
    EXPECT_FLOAT_EQ(hdu->getPixel<float>(5, 5), originalValue * 2.0f);

    // Apply a complex operation
    ASSERT_NO_THROW(hdu->applyMathOperation<float>([](float val) {
        return std::sin(val) * 100.0f;
    }));

    // Check the result is changed
    EXPECT_NE(hdu->getPixel<float>(5, 5), originalValue * 2.0f);
}

// Test histogram computation
TEST_F(ImageHDUTest, ComputeHistogram) {
    auto hdu = createTestImageHDU<uint8_t>(50, 50);

    // Compute a histogram with 10 bins
    auto histogram = hdu->computeHistogram<uint8_t>(10);

    // Check basic properties
    EXPECT_EQ(histogram.size(), 10);

    // The sum of all bins should equal the number of pixels
    double sum = 0.0;
    for (double binCount : histogram) {
        sum += binCount;
        EXPECT_GE(binCount, 0.0); // Bin counts should be non-negative
    }
    EXPECT_EQ(sum, 50 * 50);

    // Test invalid bin count
    EXPECT_THROW(hdu->computeHistogram<uint8_t>(0), std::invalid_argument);
}

// Test histogram equalization
TEST_F(ImageHDUTest, EqualizeHistogram) {
    auto hdu = createTestImageHDU<uint8_t>(50, 50);

    // Calculate histogram before equalization
    auto histBefore = hdu->computeHistogram<uint8_t>(256);

    // Perform equalization
    ASSERT_NO_THROW(hdu->equalizeHistogram<uint8_t>());

    // Calculate histogram after equalization
    auto histAfter = hdu->computeHistogram<uint8_t>(256);

    // Histograms should be different after equalization
    bool histogramChanged = false;
    for (size_t i = 0; i < histBefore.size(); ++i) {
        if (std::abs(histBefore[i] - histAfter[i]) > 1e-6) {
            histogramChanged = true;
            break;
        }
    }
    EXPECT_TRUE(histogramChanged);
}

// Test edge detection
TEST_F(ImageHDUTest, DetectEdges) {
    auto hdu = createTestImageHDU<float>(50, 50);

    // Store original value
    float originalValue = hdu->getPixel<float>(25, 25);

    // Apply Sobel edge detection
    ASSERT_NO_THROW(hdu->detectEdges<float>("sobel"));

    // Values should change after edge detection
    EXPECT_NE(hdu->getPixel<float>(25, 25), originalValue);

    // Test invalid method
    EXPECT_THROW(hdu->detectEdges<float>("invalid_method"), std::invalid_argument);
}

// Test compression functions
TEST_F(ImageHDUTest, CompressionDecompression) {
    auto hdu = createTestImageHDU<float>(50, 50);

    // Store original data
    std::vector<float> originalData;
    for (int y = 0; y < 50; ++y) {
        for (int x = 0; x < 50; ++x) {
            originalData.push_back(hdu->getPixel<float>(x, y));
        }
    }

    // Compress with RLE
    ASSERT_NO_THROW(hdu->compressData<float>("rle"));

    // Check compression ratio
    double ratio = hdu->computeCompressionRatio();
    EXPECT_GT(ratio, 1.0); // Should achieve some compression

    // Decompress
    ASSERT_NO_THROW(hdu->decompressData<float>());

    // Verify data is preserved
    int idx = 0;
    for (int y = 0; y < 50; ++y) {
        for (int x = 0; x < 50; ++x) {
            EXPECT_FLOAT_EQ(hdu->getPixel<float>(x, y), originalData[idx++]);
        }
    }

    // Test invalid algorithm
    EXPECT_THROW(hdu->compressData<float>("invalid_algorithm"), std::invalid_argument);
}

// Test noise addition and removal
TEST_F(ImageHDUTest, NoiseAdditionAndRemoval) {
    auto hdu = createTestImageHDU<float>(30, 30);

    // Store original data
    std::vector<float> originalData;
    for (int y = 0; y < 30; ++y) {
        for (int x = 0; x < 30; ++x) {
            originalData.push_back(hdu->getPixel<float>(x, y));
        }
    }

    // Add Gaussian noise
    ASSERT_NO_THROW(hdu->addNoise<float>("gaussian", 10.0));

    // Verify data changed
    bool dataChanged = false;
    int idx = 0;
    for (int y = 0; y < 30 && !dataChanged; ++y) {
        for (int x = 0; x < 30 && !dataChanged; ++x) {
            if (std::abs(hdu->getPixel<float>(x, y) - originalData[idx++]) > 1e-6) {
                dataChanged = true;
            }
        }
    }
    EXPECT_TRUE(dataChanged);

    // Remove noise with median filter
    ASSERT_NO_THROW(hdu->removeNoise<float>("median", 3));

    // Test invalid parameters
    EXPECT_THROW(hdu->addNoise<float>("invalid_noise", 10.0), std::invalid_argument);
    EXPECT_THROW(hdu->removeNoise<float>("median", 0), std::invalid_argument);
}

// Test Fourier transform and filtering
TEST_F(ImageHDUTest, FourierTransformAndFiltering) {
    auto hdu = createTestImageHDU<float>(32, 32); // Power of 2 size for FFT

    // Apply forward FFT
    ASSERT_NO_THROW(hdu->applyFourierTransform<float>(false));

    // Apply lowpass filter in frequency domain
    ASSERT_NO_THROW(hdu->applyFrequencyFilter<float>("lowpass", 0.5));

    // Apply inverse FFT to get back to spatial domain
    ASSERT_NO_THROW(hdu->applyFourierTransform<float>(true));

    // Test invalid parameters
    EXPECT_THROW(hdu->applyFrequencyFilter<float>("invalid_filter", 0.5), std::invalid_argument);
}

// Test auto-levels adjustment
TEST_F(ImageHDUTest, AutoLevels) {
    auto hdu = createTestImageHDU<uint8_t>(50, 50);

    // Apply auto-levels with custom black and white points
    ASSERT_NO_THROW(hdu->autoLevels<uint8_t>(0.1, 0.9));

    // Test invalid parameters
    EXPECT_THROW(hdu->autoLevels<uint8_t>(-0.1, 0.9), std::invalid_argument);
    EXPECT_THROW(hdu->autoLevels<uint8_t>(0.1, 1.1), std::invalid_argument);
    EXPECT_THROW(hdu->autoLevels<uint8_t>(0.6, 0.4), std::invalid_argument);
}

// Test morphological operations
TEST_F(ImageHDUTest, ApplyMorphology) {
    auto hdu = createTestImageHDU<uint8_t>(50, 50);

    // Apply dilation
    ASSERT_NO_THROW(hdu->applyMorphology<uint8_t>("dilate", 3));

    // Apply erosion
    ASSERT_NO_THROW(hdu->applyMorphology<uint8_t>("erode", 3));

    // Test invalid parameters
    EXPECT_THROW(hdu->applyMorphology<uint8_t>("invalid_op", 3), std::invalid_argument);
    EXPECT_THROW(hdu->applyMorphology<uint8_t>("dilate", 4), std::invalid_argument); // Kernel size should be odd
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
