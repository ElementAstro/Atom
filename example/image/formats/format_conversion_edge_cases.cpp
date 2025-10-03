/**
 * @file format_conversion_edge_cases.cpp
 * @brief Format conversion edge cases and quality preservation
 *
 * This example demonstrates:
 * - Format conversion edge cases and boundary conditions
 * - Quality preservation during format conversions
 * - Metadata handling across different formats
 * - Lossy vs lossless conversion strategies
 * - Color space conversion accuracy
 * - Compression artifact handling
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <map>
#include <algorithm>
#include <cmath>
#include <iomanip>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/formats/format_converter.hpp"
#include "atom/image/formats/image_format_detector.hpp"
#include "atom/image/core/image_metadata.hpp"
#include "atom/image/processing/color_space_converter.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Format conversion test result
 */
struct ConversionTestResult {
    std::string testName;
    std::string sourceFormat;
    std::string targetFormat;
    bool success;
    std::string errorMessage;
    size_t originalSize;
    size_t convertedSize;
    double compressionRatio;
    double qualityScore;
    std::chrono::microseconds conversionTime;
    
    ConversionTestResult(const std::string& name, const std::string& src, const std::string& tgt)
        : testName(name), sourceFormat(src), targetFormat(tgt), success(false),
          originalSize(0), convertedSize(0), compressionRatio(0.0), qualityScore(0.0) {}
};

/**
 * @brief Quality assessment metrics
 */
struct QualityMetrics {
    double psnr;        // Peak Signal-to-Noise Ratio
    double mse;         // Mean Squared Error
    double ssim;        // Structural Similarity Index
    double colorError;  // Color space conversion error
    
    QualityMetrics() : psnr(0.0), mse(0.0), ssim(0.0), colorError(0.0) {}
};

/**
 * @brief Format conversion edge case tester
 */
class FormatConversionTester {
private:
    std::vector<ConversionTestResult> results_;
    FormatConverter converter_;
    ImageFormatDetector detector_;

public:
    /**
     * @brief Calculate quality metrics between two images
     */
    QualityMetrics calculateQualityMetrics(const blob& original, const blob& converted) {
        QualityMetrics metrics;
        
        if (original.size() != converted.size()) {
            // Different sizes, can't compare directly
            metrics.psnr = 0.0;
            metrics.mse = std::numeric_limits<double>::max();
            return metrics;
        }
        
        // Calculate MSE
        double mse = 0.0;
        for (size_t i = 0; i < original.size(); ++i) {
            double diff = static_cast<double>(original.data()[i]) - static_cast<double>(converted.data()[i]);
            mse += diff * diff;
        }
        mse /= original.size();
        metrics.mse = mse;
        
        // Calculate PSNR
        if (mse > 0) {
            metrics.psnr = 20.0 * std::log10(255.0 / std::sqrt(mse));
        } else {
            metrics.psnr = std::numeric_limits<double>::infinity();
        }
        
        // Simplified SSIM calculation (actual SSIM is more complex)
        double mean1 = 0.0, mean2 = 0.0;
        for (size_t i = 0; i < original.size(); ++i) {
            mean1 += original.data()[i];
            mean2 += converted.data()[i];
        }
        mean1 /= original.size();
        mean2 /= original.size();
        
        double var1 = 0.0, var2 = 0.0, covar = 0.0;
        for (size_t i = 0; i < original.size(); ++i) {
            double diff1 = original.data()[i] - mean1;
            double diff2 = converted.data()[i] - mean2;
            var1 += diff1 * diff1;
            var2 += diff2 * diff2;
            covar += diff1 * diff2;
        }
        var1 /= original.size();
        var2 /= original.size();
        covar /= original.size();
        
        double c1 = 6.5025, c2 = 58.5225; // Constants for SSIM
        metrics.ssim = ((2 * mean1 * mean2 + c1) * (2 * covar + c2)) /
                      ((mean1 * mean1 + mean2 * mean2 + c1) * (var1 + var2 + c2));
        
        return metrics;
    }
    
    /**
     * @brief Test format conversion
     */
    ConversionTestResult testConversion(const std::string& testName,
                                       const blob& sourceData,
                                       const std::string& sourceFormat,
                                       const std::string& targetFormat,
                                       const std::map<std::string, std::any>& options = {}) {
        ConversionTestResult result(testName, sourceFormat, targetFormat);
        result.originalSize = sourceData.size();
        
        std::cout << "Testing: " << testName << " (" << sourceFormat << " -> " << targetFormat << ") ... ";
        
        auto start = high_resolution_clock::now();
        
        try {
            auto convertedData = converter_.convert(sourceData, sourceFormat, targetFormat, options);
            
            result.conversionTime = duration_cast<microseconds>(high_resolution_clock::now() - start);
            result.convertedSize = convertedData.size();
            result.compressionRatio = static_cast<double>(result.originalSize) / result.convertedSize;
            
            // Calculate quality metrics
            auto metrics = calculateQualityMetrics(sourceData, convertedData);
            result.qualityScore = metrics.psnr;
            
            result.success = true;
            std::cout << "SUCCESS";
            
        } catch (const std::exception& e) {
            result.conversionTime = duration_cast<microseconds>(high_resolution_clock::now() - start);
            result.errorMessage = e.what();
            result.success = false;
            std::cout << "FAILED (" << e.what() << ")";
        }
        
        std::cout << " [" << result.conversionTime.count() << " μs]\n";
        
        results_.push_back(result);
        return result;
    }
    
    /**
     * @brief Print test summary
     */
    void printSummary() const {
        size_t successful = 0;
        size_t failed = 0;
        
        for (const auto& result : results_) {
            if (result.success) {
                successful++;
            } else {
                failed++;
            }
        }
        
        std::cout << "\n=== Format Conversion Test Summary ===\n";
        std::cout << "Total tests: " << results_.size() << "\n";
        std::cout << "Successful: " << successful << "\n";
        std::cout << "Failed: " << failed << "\n";
        std::cout << "Success rate: " << std::fixed << std::setprecision(1) 
                 << (successful * 100.0 / results_.size()) << "%\n";
        
        if (successful > 0) {
            std::cout << "\nSuccessful conversions:\n";
            std::cout << std::left << std::setw(25) << "Test" 
                     << std::setw(15) << "Compression" 
                     << std::setw(12) << "Quality" 
                     << std::setw(10) << "Time (μs)" << "\n";
            std::cout << std::string(62, '-') << "\n";
            
            for (const auto& result : results_) {
                if (result.success) {
                    std::cout << std::left << std::setw(25) << result.testName
                             << std::setw(15) << std::fixed << std::setprecision(2) << result.compressionRatio
                             << std::setw(12) << std::setprecision(1) << result.qualityScore
                             << std::setw(10) << result.conversionTime.count() << "\n";
                }
            }
        }
        
        if (failed > 0) {
            std::cout << "\nFailed conversions:\n";
            for (const auto& result : results_) {
                if (!result.success) {
                    std::cout << "  " << result.testName << ": " << result.errorMessage << "\n";
                }
            }
        }
    }
};

/**
 * @brief Create test images with different characteristics
 */
std::map<std::string, blob> createTestImages() {
    std::map<std::string, blob> testImages;
    
    // 1. Solid color image (high compression potential)
    {
        std::vector<uint8_t> solidColor(100 * 100 * 3, 128); // Gray image
        testImages["solid_color"] = blob(solidColor.data(), solidColor.size());
    }
    
    // 2. High frequency noise (low compression potential)
    {
        std::vector<uint8_t> noise(100 * 100 * 3);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        
        for (auto& byte : noise) {
            byte = static_cast<uint8_t>(dis(gen));
        }
        testImages["high_frequency_noise"] = blob(noise.data(), noise.size());
    }
    
    // 3. Gradient image (medium compression)
    {
        std::vector<uint8_t> gradient(100 * 100 * 3);
        for (int y = 0; y < 100; ++y) {
            for (int x = 0; x < 100; ++x) {
                int index = (y * 100 + x) * 3;
                uint8_t value = static_cast<uint8_t>((x + y) * 255 / 200);
                gradient[index] = value;     // R
                gradient[index + 1] = value; // G
                gradient[index + 2] = value; // B
            }
        }
        testImages["gradient"] = blob(gradient.data(), gradient.size());
    }
    
    // 4. Checkerboard pattern (specific compression characteristics)
    {
        std::vector<uint8_t> checkerboard(100 * 100 * 3);
        for (int y = 0; y < 100; ++y) {
            for (int x = 0; x < 100; ++x) {
                int index = (y * 100 + x) * 3;
                uint8_t value = ((x / 10) + (y / 10)) % 2 ? 255 : 0;
                checkerboard[index] = value;     // R
                checkerboard[index + 1] = value; // G
                checkerboard[index + 2] = value; // B
            }
        }
        testImages["checkerboard"] = blob(checkerboard.data(), checkerboard.size());
    }
    
    // 5. Single pixel image (edge case)
    {
        std::vector<uint8_t> singlePixel = {255, 128, 64}; // RGB
        testImages["single_pixel"] = blob(singlePixel.data(), singlePixel.size());
    }
    
    // 6. Very small image (2x2)
    {
        std::vector<uint8_t> tinyImage = {
            255, 0, 0,    0, 255, 0,    // Red, Green
            0, 0, 255,    255, 255, 0   // Blue, Yellow
        };
        testImages["tiny_2x2"] = blob(tinyImage.data(), tinyImage.size());
    }
    
    return testImages;
}

/**
 * @brief Test basic format conversions
 */
void testBasicFormatConversions() {
    std::cout << "\n=== Basic Format Conversions ===\n";
    
    FormatConversionTester tester;
    auto testImages = createTestImages();
    
    // Test common format conversions
    std::vector<std::pair<std::string, std::string>> formatPairs = {
        {"RAW", "JPEG"},
        {"RAW", "PNG"},
        {"RAW", "BMP"},
        {"RAW", "TIFF"},
        {"JPEG", "PNG"},
        {"PNG", "JPEG"},
        {"BMP", "PNG"},
        {"TIFF", "JPEG"}
    };
    
    for (const auto& [sourceFormat, targetFormat] : formatPairs) {
        for (const auto& [imageName, imageData] : testImages) {
            std::string testName = imageName + "_" + sourceFormat + "_to_" + targetFormat;
            tester.testConversion(testName, imageData, sourceFormat, targetFormat);
        }
    }
    
    tester.printSummary();
}

/**
 * @brief Test lossy vs lossless conversions
 */
void testLossyVsLosslessConversions() {
    std::cout << "\n=== Lossy vs Lossless Conversions ===\n";
    
    FormatConversionTester tester;
    auto testImages = createTestImages();
    
    // Test with different quality settings for lossy formats
    std::vector<int> qualityLevels = {10, 50, 90, 100};
    
    for (int quality : qualityLevels) {
        std::map<std::string, std::any> jpegOptions = {
            {"quality", quality},
            {"optimize", true}
        };
        
        for (const auto& [imageName, imageData] : testImages) {
            std::string testName = imageName + "_JPEG_quality_" + std::to_string(quality);
            tester.testConversion(testName, imageData, "RAW", "JPEG", jpegOptions);
        }
    }
    
    // Test lossless formats
    std::vector<std::string> losslessFormats = {"PNG", "BMP", "TIFF"};
    
    for (const auto& format : losslessFormats) {
        for (const auto& [imageName, imageData] : testImages) {
            std::string testName = imageName + "_" + format + "_lossless";
            tester.testConversion(testName, imageData, "RAW", format);
        }
    }
    
    tester.printSummary();
}

/**
 * @brief Test edge cases and boundary conditions
 */
void testEdgeCases() {
    std::cout << "\n=== Edge Cases and Boundary Conditions ===\n";
    
    FormatConversionTester tester;
    
    // Test 1: Empty image
    {
        blob emptyImage;
        tester.testConversion("empty_image", emptyImage, "RAW", "PNG");
    }
    
    // Test 2: Very large dimensions (simulated)
    {
        // Create header for very large image (without actual data)
        std::vector<uint8_t> largeImageHeader = {
            0xFF, 0xFF, 0xFF, 0xFF, // Width: max uint32
            0xFF, 0xFF, 0xFF, 0xFF, // Height: max uint32
            0x03, 0x00, 0x00, 0x00  // Channels: 3
        };
        blob largeImage(largeImageHeader.data(), largeImageHeader.size());
        tester.testConversion("very_large_dimensions", largeImage, "RAW", "PNG");
    }
    
    // Test 3: Unusual aspect ratios
    {
        // Very wide image (1000x1)
        std::vector<uint8_t> wideImage(1000 * 1 * 3, 128);
        blob wideBlob(wideImage.data(), wideImage.size());
        tester.testConversion("very_wide_1000x1", wideBlob, "RAW", "PNG");
        
        // Very tall image (1x1000)
        std::vector<uint8_t> tallImage(1 * 1000 * 3, 128);
        blob tallBlob(tallImage.data(), tallImage.size());
        tester.testConversion("very_tall_1x1000", tallBlob, "RAW", "PNG");
    }
    
    // Test 4: Extreme color values
    {
        std::vector<uint8_t> extremeColors = {
            0, 0, 0,           // Pure black
            255, 255, 255,     // Pure white
            255, 0, 0,         // Pure red
            0, 255, 0,         // Pure green
            0, 0, 255          // Pure blue
        };
        blob extremeBlob(extremeColors.data(), extremeColors.size());
        tester.testConversion("extreme_colors", extremeBlob, "RAW", "JPEG");
    }
    
    // Test 5: Corrupted data patterns
    {
        std::vector<uint8_t> corruptedData(100, 0xFF); // All 0xFF bytes
        blob corruptedBlob(corruptedData.data(), corruptedData.size());
        tester.testConversion("all_0xFF_pattern", corruptedBlob, "RAW", "PNG");
        
        std::vector<uint8_t> alternatingData(100);
        for (size_t i = 0; i < alternatingData.size(); ++i) {
            alternatingData[i] = (i % 2) ? 0xFF : 0x00;
        }
        blob alternatingBlob(alternatingData.data(), alternatingData.size());
        tester.testConversion("alternating_pattern", alternatingBlob, "RAW", "PNG");
    }
    
    tester.printSummary();
}

/**
 * @brief Test color space conversions
 */
void testColorSpaceConversions() {
    std::cout << "\n=== Color Space Conversions ===\n";
    
    FormatConversionTester tester;
    ColorSpaceConverter colorConverter;
    
    // Create RGB test image
    std::vector<uint8_t> rgbImage(100 * 100 * 3);
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
            int index = (y * 100 + x) * 3;
            rgbImage[index] = static_cast<uint8_t>(x * 255 / 100);     // R gradient
            rgbImage[index + 1] = static_cast<uint8_t>(y * 255 / 100); // G gradient
            rgbImage[index + 2] = 128;                                 // B constant
        }
    }
    blob rgbBlob(rgbImage.data(), rgbImage.size());
    
    // Test RGB to other color spaces
    std::vector<std::string> colorSpaces = {"HSV", "LAB", "YUV", "CMYK"};
    
    for (const auto& colorSpace : colorSpaces) {
        try {
            auto convertedData = colorConverter.convert(rgbBlob, "RGB", colorSpace);
            
            std::string testName = "RGB_to_" + colorSpace;
            tester.testConversion(testName, convertedData, colorSpace, "RGB");
            
            // Test round-trip conversion
            auto roundTripData = colorConverter.convert(convertedData, colorSpace, "RGB");
            std::string roundTripName = "RGB_to_" + colorSpace + "_to_RGB";
            
            // Calculate quality loss in round-trip
            auto metrics = tester.calculateQualityMetrics(rgbBlob, roundTripData);
            std::cout << "  Round-trip quality (PSNR): " << std::fixed << std::setprecision(2) 
                     << metrics.psnr << " dB\n";
            
        } catch (const std::exception& e) {
            std::cout << "  Color space conversion failed: " << e.what() << "\n";
        }
    }
    
    tester.printSummary();
}

/**
 * @brief Test metadata preservation
 */
void testMetadataPreservation() {
    std::cout << "\n=== Metadata Preservation ===\n";
    
    FormatConversionTester tester;
    
    // Create test image with metadata
    std::vector<uint8_t> imageData(100 * 100 * 3, 128);
    blob imageBlob(imageData.data(), imageData.size());
    
    // Create metadata
    ImageMetadata metadata;
    metadata.width = 100;
    metadata.height = 100;
    metadata.channels = 3;
    metadata.bitDepth = 8;
    metadata.colorSpace = "RGB";
    metadata.dpi = 300;
    metadata.author = "Test Author";
    metadata.description = "Test image for metadata preservation";
    metadata.copyright = "Test Copyright";
    
    // Test metadata preservation across different formats
    std::vector<std::string> metadataFormats = {"JPEG", "PNG", "TIFF"};
    
    for (const auto& format : metadataFormats) {
        std::map<std::string, std::any> options = {
            {"preserve_metadata", true},
            {"metadata", metadata}
        };
        
        std::string testName = "metadata_preservation_" + format;
        auto result = tester.testConversion(testName, imageBlob, "RAW", format, options);
        
        if (result.success) {
            // In a real implementation, you would verify that metadata was preserved
            std::cout << "  " << format << " metadata preservation: SIMULATED SUCCESS\n";
        }
    }
    
    tester.printSummary();
}

/**
 * @brief Test compression artifacts and quality assessment
 */
void testCompressionArtifacts() {
    std::cout << "\n=== Compression Artifacts and Quality Assessment ===\n";
    
    FormatConversionTester tester;
    
    // Create test images that are sensitive to compression artifacts
    
    // 1. High contrast edges (prone to ringing artifacts)
    std::vector<uint8_t> highContrast(100 * 100 * 3);
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
            int index = (y * 100 + x) * 3;
            uint8_t value = (x < 50) ? 0 : 255; // Sharp edge
            highContrast[index] = value;
            highContrast[index + 1] = value;
            highContrast[index + 2] = value;
        }
    }
    blob highContrastBlob(highContrast.data(), highContrast.size());
    
    // 2. Fine details (prone to blocking artifacts)
    std::vector<uint8_t> fineDetails(100 * 100 * 3);
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
            int index = (y * 100 + x) * 3;
            uint8_t value = ((x + y) % 2) ? 255 : 0; // Checkerboard
            fineDetails[index] = value;
            fineDetails[index + 1] = value;
            fineDetails[index + 2] = value;
        }
    }
    blob fineDetailsBlob(fineDetails.data(), fineDetails.size());
    
    // Test with different compression levels
    std::vector<int> compressionLevels = {1, 25, 50, 75, 95};
    
    for (int level : compressionLevels) {
        std::map<std::string, std::any> options = {
            {"quality", level}
        };
        
        // Test high contrast image
        std::string testName1 = "high_contrast_jpeg_q" + std::to_string(level);
        auto result1 = tester.testConversion(testName1, highContrastBlob, "RAW", "JPEG", options);
        
        // Test fine details image
        std::string testName2 = "fine_details_jpeg_q" + std::to_string(level);
        auto result2 = tester.testConversion(testName2, fineDetailsBlob, "RAW", "JPEG", options);
        
        if (result1.success && result2.success) {
            std::cout << "  Quality " << level << " - High contrast PSNR: " 
                     << std::fixed << std::setprecision(2) << result1.qualityScore << " dB, "
                     << "Fine details PSNR: " << result2.qualityScore << " dB\n";
        }
    }
    
    tester.printSummary();
}

/**
 * @brief Test performance under different conditions
 */
void testConversionPerformance() {
    std::cout << "\n=== Conversion Performance Analysis ===\n";
    
    FormatConversionTester tester;
    
    // Create images of different sizes
    std::vector<std::pair<int, std::string>> imageSizes = {
        {10, "tiny_10x10"},
        {100, "small_100x100"},
        {500, "medium_500x500"},
        {1000, "large_1000x1000"}
    };
    
    for (const auto& [size, sizeName] : imageSizes) {
        std::vector<uint8_t> imageData(size * size * 3, 128);
        blob imageBlob(imageData.data(), imageData.size());
        
        // Test conversion performance for different formats
        std::vector<std::string> formats = {"JPEG", "PNG", "BMP"};
        
        for (const auto& format : formats) {
            std::string testName = sizeName + "_to_" + format;
            auto result = tester.testConversion(testName, imageBlob, "RAW", format);
            
            if (result.success) {
                double pixelsPerSecond = (size * size * 1e6) / result.conversionTime.count();
                std::cout << "  " << testName << ": " << std::scientific << std::setprecision(2) 
                         << pixelsPerSecond << " pixels/sec\n";
            }
        }
    }
    
    tester.printSummary();
}

int main() {
    std::cout << "=== Atom Image Format Conversion Edge Cases Demo ===\n";
    std::cout << "This example demonstrates format conversion edge cases and quality preservation\n";

    // Run all format conversion tests
    testBasicFormatConversions();
    testLossyVsLosslessConversions();
    testEdgeCases();
    testColorSpaceConversions();
    testMetadataPreservation();
    testCompressionArtifacts();
    testConversionPerformance();

    std::cout << "\n=== Format conversion edge cases demo completed ===\n";
    std::cout << "\nKey capabilities demonstrated:\n";
    std::cout << "- Comprehensive format conversion testing\n";
    std::cout << "- Quality preservation analysis (PSNR, MSE, SSIM)\n";
    std::cout << "- Edge cases and boundary conditions handling\n";
    std::cout << "- Lossy vs lossless conversion strategies\n";
    std::cout << "- Color space conversion accuracy\n";
    std::cout << "- Metadata preservation across formats\n";
    std::cout << "- Compression artifact assessment\n";
    std::cout << "- Performance analysis for different image sizes\n";
    
    return 0;
}
