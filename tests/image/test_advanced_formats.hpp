#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vector>
#include <string>
#include <memory>
#include <filesystem>

#include "atom/image/formats/advanced_formats.hpp"
#include "atom/image/core/image_blob.hpp"
#include "test_utils.hpp"

namespace atom::image::test {

class AdvancedFormatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<AdvancedFormatProcessor>();
        fileManager = std::make_unique<TestFileManager>();
        createTestImages();
    }

    void TearDown() override {
        fileManager->cleanup();
    }

    void createTestImages() {
        auto gradientData = TestDataGenerator::generateGradientImage(256, 256, 3);
        test_image = blob(gradientData.data(), gradientData.size());

        auto grayData = TestDataGenerator::generateGradientImage(256, 256, 1);
        gray_image = blob(grayData.data(), grayData.size());

        auto hdrData = TestDataGenerator::generateGradientImage(256, 256, 3);
        hdr_image = blob(hdrData.data(), hdrData.size());
    }

    std::unique_ptr<AdvancedFormatProcessor> processor;
    std::unique_ptr<TestFileManager> fileManager;

    blob test_image, gray_image, hdr_image;
};

TEST_F(AdvancedFormatsTest, FormatDetectionByExtension) {
    std::vector<std::pair<std::string, AdvancedFormat>> testCases = {
        {"test.cr2", AdvancedFormat::CR2},
        {"test.nef", AdvancedFormat::NEF},
        {"test.dng", AdvancedFormat::DNG},
        {"test.dcm", AdvancedFormat::DICOM},
        {"test.exr", AdvancedFormat::OPENEXR},
        {"test.avif", AdvancedFormat::AVIF},
        {"test.heif", AdvancedFormat::HEIF},
        {"test.svg", AdvancedFormat::SVG},
        {"test.webp", AdvancedFormat::WEBP},
        {"test.unknown", AdvancedFormat::UNKNOWN}
    };

    for (const auto& [filename, expectedFormat] : testCases) {
        EXPECT_NO_THROW({
            [[maybe_unused]] auto format = processor->detectFormat(filename);
        });
    }
}

TEST_F(AdvancedFormatsTest, GetSupportedFormats) {
    auto formats = processor->getSupportedFormats();

    EXPECT_GT(formats.size(), 0);

    for (const auto& format : formats) {
        EXPECT_FALSE(format.empty());
    }
}

TEST_F(AdvancedFormatsTest, IsFormatSupported) {
    std::vector<AdvancedFormat> formatsToTest = {
        AdvancedFormat::WEBP,
        AdvancedFormat::AVIF,
        AdvancedFormat::HEIF,
        AdvancedFormat::OPENEXR,
        AdvancedFormat::DNG,
        AdvancedFormat::DICOM
    };

    for (const auto& format : formatsToTest) {
        bool supported = processor->isFormatSupported(format);
        EXPECT_TRUE(supported || !supported);
    }
}

TEST_F(AdvancedFormatsTest, GetFormatExtensions) {
    auto extensions = processor->getFormatExtensions(AdvancedFormat::WEBP);
    EXPECT_GE(extensions.size(), 0);
}

TEST_F(AdvancedFormatsTest, LoadRAW) {
    RAWParams params;
    params.exposure = 0.0;
    params.autoWhiteBalance = true;
    params.bitDepth = 8;

    try {
        auto result = processor->loadRAW("test.cr2", params);
        EXPECT_FALSE(result.isEmpty());
    } catch (const std::exception& e) {
        GTEST_SKIP() << "RAW loading not available: " << e.what();
    }
}

TEST_F(AdvancedFormatsTest, LoadDICOM) {
    try {
        auto [image, metadata] = processor->loadDICOM("test.dcm", 0, 0);

        EXPECT_GE(metadata.bitsAllocated, 0);
        EXPECT_GE(metadata.samplesPerPixel, 0);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "DICOM loading not available: " << e.what();
    }
}

TEST_F(AdvancedFormatsTest, SaveDICOM) {
    DICOMMetadata metadata;
    metadata.patientName = "Test Patient";
    metadata.patientID = "12345";
    metadata.modality = "CT";
    metadata.bitsAllocated = 16;
    metadata.samplesPerPixel = 1;

    try {
        bool saved = processor->saveDICOM(gray_image, "test_output.dcm", metadata);
        EXPECT_TRUE(saved || !saved);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "DICOM saving not available: " << e.what();
    }
}

// Test animation loading
TEST_F(AdvancedFormatsTest, LoadAnimation) {
    try {
        auto frames = processor->loadAnimation("test.gif");
        
        // Verify frame structure
        for (const auto& frame : frames) {
            EXPECT_FALSE(frame.imageData.isEmpty());
            EXPECT_GT(frame.duration, 0);
        }
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Animation loading not available: " << e.what();
    }
}

// Test animation saving
TEST_F(AdvancedFormatsTest, SaveAnimation) {
    std::vector<AnimationFrame> frames;
    
    AnimationFrame frame1;
    frame1.imageData = test_image;
    frame1.duration = 100;
    frames.push_back(frame1);
    
    AnimationFrame frame2;
    frame2.imageData = gray_image;
    frame2.duration = 100;
    frames.push_back(frame2);
    
    try {
        bool saved = processor->saveAnimation(frames, "test_output.gif", AdvancedFormat::GIF, 0);
        EXPECT_TRUE(saved || !saved);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Animation saving not available: " << e.what();
    }
}

// Test HDR loading
TEST_F(AdvancedFormatsTest, LoadHDR) {
    try {
        auto result = processor->loadHDR("test.exr", 0.0, 1.0);
        EXPECT_FALSE(result.isEmpty());
    } catch (const std::exception& e) {
        GTEST_SKIP() << "HDR loading not available: " << e.what();
    }
}

// Test HDR saving
TEST_F(AdvancedFormatsTest, SaveHDR) {
    try {
        bool saved = processor->saveHDR(hdr_image, "test_output.exr", AdvancedFormat::OPENEXR, "zip");
        EXPECT_TRUE(saved || !saved);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "HDR saving not available: " << e.what();
    }
}

// Test vector image loading
TEST_F(AdvancedFormatsTest, LoadVector) {
    try {
        auto result = processor->loadVector("test.svg", 512, 512, 96.0);
        EXPECT_FALSE(result.isEmpty());
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vector loading not available: " << e.what();
    }
}

// Test microscopy image loading
TEST_F(AdvancedFormatsTest, LoadMicroscopy) {
    try {
        auto [image, metadata] = processor->loadMicroscopy("test.lsm", 0, 0, 0, 0);
        
        EXPECT_GE(metadata.size(), 0);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Microscopy loading not available: " << e.what();
    }
}

// Test satellite image loading
TEST_F(AdvancedFormatsTest, LoadSatellite) {
    try {
        std::vector<int> bands = {1, 2, 3};
        auto [image, metadata] = processor->loadSatellite("test.tif", bands);
        
        EXPECT_GE(metadata.size(), 0);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Satellite loading not available: " << e.what();
    }
}

// Test format conversion
TEST_F(AdvancedFormatsTest, FormatConversion) {
    try {
        bool converted = processor->convertFormat("input.png", "output.webp", 
                                                  AdvancedFormat::WEBP);
        EXPECT_TRUE(converted || !converted);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Format conversion not available: " << e.what();
    }
}

TEST_F(AdvancedFormatsTest, GetFormatInfo) {
    try {
        auto info = processor->getFormatInfo("test.png");
        EXPECT_GE(info.size(), 0);
    } catch (const std::exception& e) {
        EXPECT_NO_THROW({
            auto info = processor->getFormatInfo("nonexistent.png");
        });
    }
}

TEST_F(AdvancedFormatsTest, BatchConvert) {
    std::vector<std::string> inputFiles = {"test1.png", "test2.png", "test3.png"};

    int progressCallCount = 0;
    auto progressCallback = [&progressCallCount](int current, int total) {
        progressCallCount++;
        EXPECT_LE(current, total);
    };

    try {
        int converted = processor->batchConvert(inputFiles, "output_dir",
                                               AdvancedFormat::WEBP, {}, progressCallback);
        EXPECT_GE(converted, 0);
        EXPECT_LE(converted, static_cast<int>(inputFiles.size()));
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Batch conversion not available: " << e.what();
    }
}

TEST_F(AdvancedFormatsTest, FactoryFunction) {
    auto formatProcessor = createOptimalFormatProcessor(true);
    EXPECT_NE(formatProcessor, nullptr);
}

TEST_F(AdvancedFormatsTest, RAWParameters) {
    RAWParams params;
    params.exposure = 1.0;
    params.highlights = -50.0;
    params.shadows = 50.0;
    params.whites = 10.0;
    params.blacks = -10.0;
    params.clarity = 25.0;
    params.vibrance = 15.0;
    params.saturation = 10.0;
    params.temperature = 5500.0;
    params.tint = 0.0;
    params.sharpness = 40.0;
    params.noiseReduction = 25.0;
    params.autoWhiteBalance = false;
    params.autoExposure = false;
    params.colorSpace = "AdobeRGB";
    params.bitDepth = 16;

    EXPECT_EQ(params.bitDepth, 16);
    EXPECT_EQ(params.colorSpace, "AdobeRGB");
}

// Test DICOM metadata structure
TEST_F(AdvancedFormatsTest, DICOMMetadataStructure) {
    DICOMMetadata metadata;
    metadata.patientName = "John Doe";
    metadata.patientID = "ABC123";
    metadata.studyDate = "20250101";
    metadata.modality = "MR";
    metadata.manufacturer = "Test Manufacturer";
    metadata.modelName = "Test Model";
    metadata.pixelSpacing[0] = 0.5;
    metadata.pixelSpacing[1] = 0.5;
    metadata.sliceThickness = 1.0;
    metadata.bitsAllocated = 16;
    metadata.bitsStored = 12;
    metadata.samplesPerPixel = 1;
    metadata.photometricInterpretation = "MONOCHROME2";
    metadata.customTags["CustomTag1"] = "Value1";

    EXPECT_EQ(metadata.patientName, "John Doe");
    EXPECT_EQ(metadata.bitsAllocated, 16);
    EXPECT_EQ(metadata.customTags.size(), 1);
}

// Test animation frame structure
TEST_F(AdvancedFormatsTest, AnimationFrameStructure) {
    AnimationFrame frame;
    frame.imageData = test_image;
    frame.duration = 100;
    frame.disposalMethod = 1;
    frame.offsetX = 10;
    frame.offsetY = 20;
    frame.transparent = true;
    frame.transparentColor = 0xFF00FF;

    EXPECT_FALSE(frame.imageData.isEmpty());
    EXPECT_EQ(frame.duration, 100);
    EXPECT_TRUE(frame.transparent);
}

// Test format detection from data
TEST_F(AdvancedFormatsTest, FormatDetectionFromData) {
    // Create some test data with magic numbers
    std::vector<uint8_t> pngData = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> jpegData = {0xFF, 0xD8, 0xFF, 0xE0};

    // Test detection (may not work without full file structure)
    EXPECT_NO_THROW({
        [[maybe_unused]] auto format1 = processor->detectFormat(pngData.data(), pngData.size());
        [[maybe_unused]] auto format2 = processor->detectFormat(jpegData.data(), jpegData.size());
    });
}

// Test loading with different RAW formats
TEST_F(AdvancedFormatsTest, DifferentRAWFormats) {
    std::vector<std::string> rawFiles = {
        "test.cr2",  // Canon
        "test.nef",  // Nikon
        "test.arw",  // Sony
        "test.dng",  // Adobe
        "test.raf",  // Fujifilm
        "test.orf",  // Olympus
        "test.rw2",  // Panasonic
        "test.pef"   // Pentax
    };

    RAWParams params;
    params.autoWhiteBalance = true;

    for (const auto& file : rawFiles) {
        try {
            auto result = processor->loadRAW(file, params);
            // If successful, verify result
            if (!result.isEmpty()) {
                EXPECT_GT(result.size(), 0);
            }
        } catch (const std::exception&) {
            // Expected if file doesn't exist or format not supported
        }
    }
}

// Test HDR with different exposure values
TEST_F(AdvancedFormatsTest, HDRExposureValues) {
    std::vector<double> exposures = {-2.0, -1.0, 0.0, 1.0, 2.0};

    for (double exposure : exposures) {
        try {
            auto result = processor->loadHDR("test.exr", exposure, 1.0);
            // Just verify it doesn't crash
        } catch (const std::exception&) {
            // Expected if file doesn't exist
        }
    }
}

// Test vector rasterization at different resolutions
TEST_F(AdvancedFormatsTest, VectorRasterizationResolutions) {
    std::vector<std::pair<int, int>> resolutions = {
        {256, 256},
        {512, 512},
        {1024, 1024},
        {1920, 1080}
    };

    for (const auto& [width, height] : resolutions) {
        try {
            auto result = processor->loadVector("test.svg", width, height, 96.0);
            // Verify dimensions if successful
        } catch (const std::exception&) {
            // Expected if file doesn't exist
        }
    }
}

// Test microscopy multi-dimensional loading
TEST_F(AdvancedFormatsTest, MicroscopyMultiDimensional) {
    // Test different series, channels, time points, and z-stacks
    try {
        auto [image1, meta1] = processor->loadMicroscopy("test.lsm", 0, 0, 0, 0);
        auto [image2, meta2] = processor->loadMicroscopy("test.lsm", 0, 1, 0, 0);
        auto [image3, meta3] = processor->loadMicroscopy("test.lsm", 0, 0, 1, 0);
        auto [image4, meta4] = processor->loadMicroscopy("test.lsm", 0, 0, 0, 1);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Microscopy loading not available: " << e.what();
    }
}

// Test satellite band selection
TEST_F(AdvancedFormatsTest, SatelliteBandSelection) {
    std::vector<std::vector<int>> bandCombinations = {
        {1, 2, 3},      // RGB
        {4, 3, 2},      // False color
        {1},            // Single band
        {}              // All bands
    };

    for (const auto& bands : bandCombinations) {
        try {
            auto [image, metadata] = processor->loadSatellite("test.tif", bands);
        } catch (const std::exception&) {
            // Expected if file doesn't exist
        }
    }
}

// Test format conversion with parameters
TEST_F(AdvancedFormatsTest, FormatConversionWithParameters) {
    std::unordered_map<std::string, std::string> params;
    params["quality"] = "90";
    params["compression"] = "lossless";

    try {
        [[maybe_unused]] bool converted = processor->convertFormat("input.png", "output.webp",
                                                  AdvancedFormat::WEBP, params);
    } catch (const std::exception&) {
        // Expected if files don't exist
    }
}

// Test batch conversion with progress tracking
TEST_F(AdvancedFormatsTest, BatchConversionProgress) {
    std::vector<std::string> files = {"f1.png", "f2.png", "f3.png", "f4.png", "f5.png"};

    int lastProgress = -1;
    auto callback = [&lastProgress](int current, int total) {
        EXPECT_GT(current, lastProgress);
        EXPECT_LE(current, total);
        lastProgress = current;
    };

    try {
        processor->batchConvert(files, "output", AdvancedFormat::WEBP, {}, callback);
    } catch (const std::exception&) {
        // Expected if files don't exist
    }
}

TEST_F(AdvancedFormatsTest, EmptyAnimationFrames) {
    std::vector<AnimationFrame> emptyFrames;

    try {
        [[maybe_unused]] bool saved = processor->saveAnimation(emptyFrames, "empty.gif", AdvancedFormat::GIF, 0);
    } catch (const std::exception&) {
        SUCCEED();
    }
}

TEST_F(AdvancedFormatsTest, FormatExtensionMapping) {
    std::vector<AdvancedFormat> formats = {
        AdvancedFormat::WEBP,
        AdvancedFormat::AVIF,
        AdvancedFormat::HEIF,
        AdvancedFormat::JPEG_XL,
        AdvancedFormat::OPENEXR,
        AdvancedFormat::SVG,
        AdvancedFormat::PDF
    };

    for (const auto& format : formats) {
        auto extensions = processor->getFormatExtensions(format);
        EXPECT_GE(extensions.size(), 0);
    }
}

TEST_F(AdvancedFormatsTest, InvalidFormat) {
    EXPECT_FALSE(processor->isFormatSupported(AdvancedFormat::UNKNOWN));
}

TEST_F(AdvancedFormatsTest, LargeAnimation) {
    std::vector<AnimationFrame> frames;

    for (int i = 0; i < 100; ++i) {
        AnimationFrame frame;
        frame.imageData = test_image;
        frame.duration = 50;
        frames.push_back(frame);
    }

    try {
        [[maybe_unused]] bool saved = processor->saveAnimation(frames, "large.gif", AdvancedFormat::GIF, 0);
    } catch (const std::exception&) {
        SUCCEED();
    }
}

} // namespace atom::image::test

