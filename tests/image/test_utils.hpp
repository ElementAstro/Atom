#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "atom/image/core/image_blob.hpp"

namespace atom::image::test {

/**
 * @brief Test data generation utilities
 */
class TestDataGenerator {
public:
    /**
     * @brief Generate synthetic image data with gradient pattern
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @return Vector of test image data
     */
    static std::vector<std::byte> generateGradientImage(int width, int height,
                                                        int channels = 3) {
        std::vector<std::byte> data;
        data.reserve(width * height * channels);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < channels; ++c) {
                    uint8_t value;
                    switch (c) {
                        case 0:  // Red channel
                            value =
                                static_cast<uint8_t>((x * 255) / (width - 1));
                            break;
                        case 1:  // Green channel
                            value =
                                static_cast<uint8_t>((y * 255) / (height - 1));
                            break;
                        case 2:  // Blue channel
                            value = static_cast<uint8_t>(((x + y) * 255) /
                                                         (width + height - 2));
                            break;
                        default:  // Additional channels
                            value = static_cast<uint8_t>(
                                (x * y * 255) / ((width - 1) * (height - 1)));
                            break;
                    }
                    data.push_back(std::byte{value});
                }
            }
        }
        return data;
    }

    /**
     * @brief Generate checkerboard pattern image
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @param blockSize Size of checkerboard blocks
     * @return Vector of test image data
     */
    static std::vector<std::byte> generateCheckerboard(int width, int height,
                                                       int channels = 1,
                                                       int blockSize = 8) {
        std::vector<std::byte> data;
        data.reserve(width * height * channels);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                bool isWhite = ((x / blockSize) + (y / blockSize)) % 2 == 0;
                uint8_t value = isWhite ? 255 : 0;

                for (int c = 0; c < channels; ++c) {
                    data.push_back(std::byte{value});
                }
            }
        }
        return data;
    }

    /**
     * @brief Generate random noise image
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @param seed Random seed for reproducibility
     * @return Vector of test image data
     */
    static std::vector<std::byte> generateRandomNoise(int width, int height,
                                                      int channels = 1,
                                                      uint32_t seed = 42) {
        std::vector<std::byte> data;
        data.reserve(width * height * channels);

        std::mt19937 gen(seed);
        std::uniform_int_distribution<> dis(0, 255);

        for (int i = 0; i < width * height * channels; ++i) {
            data.push_back(std::byte{static_cast<uint8_t>(dis(gen))});
        }
        return data;
    }

    /**
     * @brief Generate solid color image
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @param color Color values for each channel
     * @return Vector of test image data
     */
    static std::vector<std::byte> generateSolidColor(
        int width, int height, int channels,
        const std::vector<uint8_t>& color) {
        std::vector<std::byte> data;
        data.reserve(width * height * channels);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < channels; ++c) {
                    uint8_t value = (c < color.size()) ? color[c] : 128;
                    data.push_back(std::byte{value});
                }
            }
        }
        return data;
    }

    /**
     * @brief Generate circular pattern image
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @return Vector of test image data
     */
    static std::vector<std::byte> generateCircularPattern(int width, int height,
                                                          int channels = 1) {
        std::vector<std::byte> data;
        data.reserve(width * height * channels);

        int centerX = width / 2;
        int centerY = height / 2;
        double maxRadius = std::min(centerX, centerY);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                double distance = std::sqrt((x - centerX) * (x - centerX) +
                                            (y - centerY) * (y - centerY));
                uint8_t value = static_cast<uint8_t>(
                    (1.0 - std::min(distance / maxRadius, 1.0)) * 255);

                for (int c = 0; c < channels; ++c) {
                    data.push_back(std::byte{value});
                }
            }
        }
        return data;
    }
};

/**
 * @brief FITS file generation utilities
 */
class FitsTestDataGenerator {
public:
    /**
     * @brief Create a temporary FITS file with test data
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @param bitpix FITS BITPIX value
     * @return Path to created temporary file
     */
    static std::string createTempFitsFile(int width = 10, int height = 10,
                                          int channels = 1, int bitpix = 32) {
        namespace fs = std::filesystem;

        std::string tempFilePath =
            (fs::temp_directory_path() /
             fs::path("test_fits_temp_" +
                      std::to_string(std::random_device{}()) + ".fits"))
                .string();

        std::ofstream outFile(tempFilePath, std::ios::binary);

        // Write FITS header
        writeHeaderCard(outFile, "SIMPLE", "T", "Standard FITS format");
        writeHeaderCard(outFile, "BITPIX", std::to_string(bitpix),
                        "Bits per pixel");
        writeHeaderCard(outFile, "NAXIS", std::to_string(channels > 1 ? 3 : 2),
                        "Number of axes");
        writeHeaderCard(outFile, "NAXIS1", std::to_string(width), "Width");
        writeHeaderCard(outFile, "NAXIS2", std::to_string(height), "Height");
        if (channels > 1) {
            writeHeaderCard(outFile, "NAXIS3", std::to_string(channels),
                            "Channels");
        }
        writeHeaderCard(outFile, "END", "", "");

        // Pad header to multiple of 2880 bytes
        size_t headerSize = outFile.tellp();
        size_t paddingRequired = (2880 - (headerSize % 2880)) % 2880;
        outFile << std::string(paddingRequired, ' ');

        // Write test data
        int pixelSize = bitpix / 8;
        int dataSize = width * height * channels * pixelSize;

        if (bitpix == 32) {
            std::vector<int32_t> data(width * height * channels);
            for (int i = 0; i < width * height * channels; ++i) {
                data[i] = i % 256;  // Simple test pattern
            }
            outFile.write(reinterpret_cast<const char*>(data.data()), dataSize);
        } else if (bitpix == 16) {
            std::vector<int16_t> data(width * height * channels);
            for (int i = 0; i < width * height * channels; ++i) {
                data[i] = static_cast<int16_t>(i % 256);
            }
            outFile.write(reinterpret_cast<const char*>(data.data()), dataSize);
        } else if (bitpix == 8) {
            std::vector<uint8_t> data(width * height * channels);
            for (int i = 0; i < width * height * channels; ++i) {
                data[i] = static_cast<uint8_t>(i % 256);
            }
            outFile.write(reinterpret_cast<const char*>(data.data()), dataSize);
        }

        // Pad data to multiple of 2880 bytes
        size_t dataPaddingRequired = (2880 - (dataSize % 2880)) % 2880;
        outFile << std::string(dataPaddingRequired, '\0');

        outFile.close();
        return tempFilePath;
    }

private:
    static void writeHeaderCard(std::ofstream& file, const std::string& keyword,
                                const std::string& value,
                                const std::string& comment) {
        std::string card = keyword;
        card.resize(8, ' ');

        if (!value.empty()) {
            card += "= ";
            card += std::string(20 - value.length(), ' ') + value;
        }

        if (!comment.empty()) {
            card += " / " + comment;
        }

        card.resize(80, ' ');
        file << card;
    }
};

/**
 * @brief Test file management utilities
 */
class TestFileManager {
private:
    std::vector<std::string> tempFiles_;

public:
    /**
     * @brief Register a temporary file for cleanup
     * @param filePath Path to temporary file
     */
    void registerTempFile(const std::string& filePath) {
        tempFiles_.push_back(filePath);
    }

    /**
     * @brief Clean up all registered temporary files
     */
    void cleanup() {
        for (const auto& path : tempFiles_) {
            std::remove(path.c_str());
        }
        tempFiles_.clear();
    }

    /**
     * @brief Destructor - automatically clean up files
     */
    ~TestFileManager() { cleanup(); }
};

/**
 * @brief Common test assertions and utilities
 */
class TestAssertions {
public:
    /**
     * @brief Assert that two floating point values are approximately equal
     * @param expected Expected value
     * @param actual Actual value
     * @param tolerance Tolerance for comparison
     */
    static void assertFloatNear(double expected, double actual,
                                double tolerance = 1e-6) {
        EXPECT_NEAR(expected, actual, tolerance);
    }

    /**
     * @brief Assert that image dimensions are valid
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     */
    static void assertValidImageDimensions(int width, int height,
                                           int channels = 1) {
        EXPECT_GT(width, 0) << "Width must be positive";
        EXPECT_GT(height, 0) << "Height must be positive";
        EXPECT_GT(channels, 0) << "Channels must be positive";
        EXPECT_LE(channels, 4) << "Channels should be reasonable (≤4)";
    }

    /**
     * @brief Assert that a file exists
     * @param filePath Path to file
     */
    static void assertFileExists(const std::string& filePath) {
        EXPECT_TRUE(std::filesystem::exists(filePath))
            << "File should exist: " << filePath;
    }

    /**
     * @brief Assert that a file does not exist
     * @param filePath Path to file
     */
    static void assertFileNotExists(const std::string& filePath) {
        EXPECT_FALSE(std::filesystem::exists(filePath))
            << "File should not exist: " << filePath;
    }
};

}  // namespace atom::image::test
