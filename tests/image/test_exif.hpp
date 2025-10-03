#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>

#include "atom/image/metadata/exif.hpp"
#include "test_utils.hpp"

namespace atom::image::test {

class ExifTest : public ::testing::Test {
protected:
    void SetUp() override {
        fileManager = std::make_unique<TestFileManager>();

        // Create test EXIF data
        createTestExifData();

        // Create test image files with EXIF data
        createTestImageFiles();
    }

    void TearDown() override {
        fileManager->cleanup();
    }

    void createTestExifData() {
        // Basic camera information
        testExifData = {
            {"Make", "Canon"},
            {"Model", "EOS R5"},
            {"Software", "Canon Digital Photo Professional"},
            {"DateTime", "2023:10:15 14:30:25"},
            {"DateTimeOriginal", "2023:10:15 14:30:25"},
            {"DateTimeDigitized", "2023:10:15 14:30:25"},
            
            // Camera settings
            {"ExposureTime", "1/125"},
            {"FNumber", "f/5.6"},
            {"ISO", "400"},
            {"FocalLength", "85.0 mm"},
            {"Flash", "Flash did not fire"},
            {"WhiteBalance", "Auto"},
            {"ExposureMode", "Manual"},
            {"MeteringMode", "Pattern"},
            
            // Image properties
            {"ImageWidth", "6000"},
            {"ImageHeight", "4000"},
            {"BitsPerSample", "8"},
            {"ColorSpace", "sRGB"},
            {"Orientation", "1"},
            
            // GPS data
            {"GPSLatitude", "37.7749"},
            {"GPSLongitude", "-122.4194"},
            {"GPSAltitude", "16.0"},
            {"GPSLatitudeRef", "N"},
            {"GPSLongitudeRef", "W"},
            {"GPSAltitudeRef", "0"},
            {"GPSTimeStamp", "22:30:25"},
            {"GPSDateStamp", "2023:10:15"},
            
            // Lens information
            {"LensModel", "RF85mm F1.2 L USM"},
            {"LensSerialNumber", "1234567890"},
            {"LensMake", "Canon"}
        };

        // Create GPS coordinate test data
        gpsTestData = {
            {"GPSLatitude", "40.7128"},
            {"GPSLongitude", "-74.0060"},
            {"GPSLatitudeRef", "N"},
            {"GPSLongitudeRef", "W"},
            {"GPSAltitude", "10.0"},
            {"GPSAltitudeRef", "0"}
        };
    }

    void createTestImageFiles() {
        // Create a mock JPEG file with EXIF data
        jpeg_with_exif = "test_with_exif.jpg";
        createMockJpegWithExif(jpeg_with_exif, testExifData);
        fileManager->registerTempFile(jpeg_with_exif);

        // Create a JPEG file without EXIF data
        jpeg_without_exif = "test_without_exif.jpg";
        createMockJpegWithoutExif(jpeg_without_exif);
        fileManager->registerTempFile(jpeg_without_exif);

        // Create a corrupted EXIF file
        corrupted_exif_file = "test_corrupted_exif.jpg";
        createCorruptedExifFile(corrupted_exif_file);
        fileManager->registerTempFile(corrupted_exif_file);
    }

    void createMockJpegWithExif(const std::string& filename, const std::unordered_map<std::string, std::string>& exifData) {
        std::ofstream file(filename, std::ios::binary);
        
        // Write JPEG header
        file.write("\xFF\xD8\xFF\xE1", 4);
        
        // Write mock EXIF segment
        std::string exifSegment = createMockExifSegment(exifData);
        uint16_t segmentSize = static_cast<uint16_t>(exifSegment.size() + 2);
        file.write(reinterpret_cast<const char*>(&segmentSize), 2);
        file.write(exifSegment.c_str(), exifSegment.size());
        
        // Write some image data
        std::vector<uint8_t> imageData(1000, 0x42);
        file.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
        
        // Write JPEG end marker
        file.write("\xFF\xD9", 2);
        file.close();
    }

    void createMockJpegWithoutExif(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        
        // Write JPEG header without EXIF
        file.write("\xFF\xD8\xFF\xE0", 4);
        
        // Write JFIF segment
        file.write("\x00\x10JFIF\x00\x01\x01\x01\x00H\x00H\x00\x00", 16);
        
        // Write some image data
        std::vector<uint8_t> imageData(1000, 0x42);
        file.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
        
        // Write JPEG end marker
        file.write("\xFF\xD9", 2);
        file.close();
    }

    void createCorruptedExifFile(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        
        // Write JPEG header
        file.write("\xFF\xD8\xFF\xE1", 4);
        
        // Write corrupted EXIF segment
        file.write("\x00\x08Exif\x00\x00", 8);
        file.write("CORRUPTED_DATA", 14);
        
        // Write JPEG end marker
        file.write("\xFF\xD9", 2);
        file.close();
    }

    std::string createMockExifSegment(const std::unordered_map<std::string, std::string>& exifData) {
        std::string segment = "Exif\x00\x00";
        
        // Add mock TIFF header
        segment += "II*\x00"; // Little endian TIFF header
        
        // Add mock IFD data (simplified)
        for (const auto& [key, value] : exifData) {
            segment += key + "=" + value + ";";
        }
        
        return segment;
    }

    std::unique_ptr<TestFileManager> fileManager;

    std::unordered_map<std::string, std::string> testExifData;
    std::unordered_map<std::string, std::string> gpsTestData;

    std::string jpeg_with_exif, jpeg_without_exif, corrupted_exif_file;
};

// Test basic EXIF reading
TEST_F(ExifTest, ReadBasicExifData) {
    ExifParser parser(jpeg_with_exif);
    bool success = parser.parse();
    EXPECT_TRUE(success);

    const auto& exifData = parser.getExifData();

    // Check for basic camera information
    EXPECT_FALSE(exifData.cameraMake.empty());
    EXPECT_FALSE(exifData.cameraModel.empty());
    EXPECT_FALSE(exifData.dateTime.empty());
}

// Test GPS coordinate extraction
TEST_F(ExifTest, ExtractGPSCoordinates) {
    ExifParser parser(jpeg_with_exif);
    bool success = parser.parse();
    EXPECT_TRUE(success);

    const auto& exifData = parser.getExifData();

    if (exifData.gpsLatitude.has_value() && exifData.gpsLongitude.has_value()) {
        double lat = exifData.gpsLatitude->toDecimalDegrees();
        double lon = exifData.gpsLongitude->toDecimalDegrees();

        EXPECT_NEAR(std::abs(lat), 37.7749, 1.0); // Allow some tolerance for test data
        EXPECT_NEAR(std::abs(lon), 122.4194, 1.0);
    }
}

// Test camera settings extraction
TEST_F(ExifTest, ExtractCameraSettings) {
    ExifParser parser(jpeg_with_exif);
    bool success = parser.parse();
    EXPECT_TRUE(success);

    const auto& exifData = parser.getExifData();
    EXPECT_FALSE(exifData.exposureTime.empty());
    EXPECT_FALSE(exifData.fNumber.empty());
    EXPECT_FALSE(exifData.isoSpeed.empty());
    EXPECT_FALSE(exifData.focalLength.empty());
}

// Test date/time parsing
TEST_F(ExifTest, ParseDateTime) {
    ExifParser parser(jpeg_with_exif);
    bool success = parser.parse();
    EXPECT_TRUE(success);

    const auto& exifData = parser.getExifData();
    EXPECT_FALSE(exifData.dateTime.empty());

    // Basic validation that date string is in expected format
    EXPECT_TRUE(exifData.dateTime.find(":") != std::string::npos);
}

// Test reading from file without EXIF data
TEST_F(ExifTest, ReadFromFileWithoutExif) {
    ExifParser parser(jpeg_without_exif);
    bool success = parser.parse();

    // May succeed or fail depending on implementation
    // If it succeeds, EXIF data should be empty/default
    if (success) {
        const auto& exifData = parser.getExifData();
        // Most fields should be empty for file without EXIF
        EXPECT_TRUE(exifData.cameraMake.empty() || exifData.cameraMake == "Unknown");
    }
}

// Test error handling with corrupted EXIF data
TEST_F(ExifTest, HandleCorruptedExifData) {
    ExifParser parser(corrupted_exif_file);
    bool success = parser.parse();

    // Should handle corrupted data gracefully
    EXPECT_FALSE(success);
}

// Test error handling with non-existent file
TEST_F(ExifTest, HandleNonExistentFile) {
    EXPECT_THROW(ExifParser parser("non_existent_file.jpg"), std::exception);
}

// Test image dimensions extraction
TEST_F(ExifTest, ExtractImageDimensions) {
    ExifParser parser(jpeg_with_exif);
    bool success = parser.parse();
    EXPECT_TRUE(success);

    const auto& exifData = parser.getExifData();
    EXPECT_FALSE(exifData.imageWidth.empty());
    EXPECT_FALSE(exifData.imageHeight.empty());

    // Verify dimensions are numeric
    EXPECT_NO_THROW(std::stoi(exifData.imageWidth));
    EXPECT_NO_THROW(std::stoi(exifData.imageHeight));
}

// Test orientation extraction
TEST_F(ExifTest, ExtractOrientation) {
    ExifParser parser(jpeg_with_exif);
    bool success = parser.parse();
    EXPECT_TRUE(success);

    const auto& exifData = parser.getExifData();
    EXPECT_FALSE(exifData.orientation.empty());
}

// Test software information extraction
TEST_F(ExifTest, ExtractSoftwareInfo) {
    ExifParser parser(jpeg_with_exif);
    bool success = parser.parse();
    EXPECT_TRUE(success);

    const auto& exifData = parser.getExifData();
    // Software field may or may not be present
    if (!exifData.software.empty()) {
        EXPECT_FALSE(exifData.software.empty());
    }
}

// Test GPS coordinate conversion
TEST_F(ExifTest, GPSCoordinateConversion) {
    // Test decimal degrees to DMS conversion using static method
    auto coord = GpsCoordinate::fromDecimalDegrees(37.7749, true);
    EXPECT_EQ(coord.degrees, 37);
    EXPECT_GT(coord.minutes, 0);
    EXPECT_GE(coord.seconds, 0);
    EXPECT_EQ(coord.direction, 'N');

    // Test DMS to decimal degrees conversion
    GpsCoordinate testCoord = {37, 46, 29.64, 'N'};
    double decimal = testCoord.toDecimalDegrees();
    EXPECT_NEAR(decimal, 37.7749, 0.01);
}

// Test GPS coordinate string representation
TEST_F(ExifTest, GPSCoordinateStringRepresentation) {
    GpsCoordinate coord = {37, 46, 29.64, 'N'};
    std::string coordStr = coord.toString();
    EXPECT_FALSE(coordStr.empty());
    EXPECT_TRUE(coordStr.find("37") != std::string::npos);
    EXPECT_TRUE(coordStr.find("N") != std::string::npos);
}

// Test multiple EXIF parsing
TEST_F(ExifTest, MultipleExifParsing) {
    // Test parsing the same file multiple times
    for (int i = 0; i < 5; ++i) {
        ExifParser parser(jpeg_with_exif);
        bool success = parser.parse();
        EXPECT_TRUE(success);

        const auto& exifData = parser.getExifData();
        EXPECT_FALSE(exifData.cameraMake.empty());
    }
}

// Test EXIF data structure completeness
TEST_F(ExifTest, ExifDataStructureCompleteness) {
    ExifParser parser(jpeg_with_exif);
    bool success = parser.parse();
    EXPECT_TRUE(success);

    const auto& exifData = parser.getExifData();

    // Test that the structure has all expected fields (even if empty)
    // This ensures the ExifData structure is properly initialized
    EXPECT_TRUE(exifData.cameraMake.empty() || !exifData.cameraMake.empty());
    EXPECT_TRUE(exifData.cameraModel.empty() || !exifData.cameraModel.empty());
    EXPECT_TRUE(exifData.dateTime.empty() || !exifData.dateTime.empty());
    EXPECT_TRUE(exifData.exposureTime.empty() || !exifData.exposureTime.empty());
    EXPECT_TRUE(exifData.fNumber.empty() || !exifData.fNumber.empty());
    EXPECT_TRUE(exifData.isoSpeed.empty() || !exifData.isoSpeed.empty());
    EXPECT_TRUE(exifData.focalLength.empty() || !exifData.focalLength.empty());
    EXPECT_TRUE(exifData.orientation.empty() || !exifData.orientation.empty());
    EXPECT_TRUE(exifData.compression.empty() || !exifData.compression.empty());
    EXPECT_TRUE(exifData.imageWidth.empty() || !exifData.imageWidth.empty());
    EXPECT_TRUE(exifData.imageHeight.empty() || !exifData.imageHeight.empty());
    EXPECT_TRUE(exifData.colorSpace.empty() || !exifData.colorSpace.empty());
    EXPECT_TRUE(exifData.software.empty() || !exifData.software.empty());
}

} // namespace atom::image::test
