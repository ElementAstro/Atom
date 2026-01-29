/**
 * @file format_detector_example.cpp
 * @brief Example demonstrating image format detection
 *
 * This example covers:
 * - Detecting image formats from files
 * - Detecting formats from memory buffers
 * - Format detection confidence levels
 * - Getting MIME types and extensions
 * - Checking format support
 * - Custom format registration
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <fstream>
#include <iostream>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/io/format_detector.hpp"

using namespace atom::image;
using namespace std;

/**
 * @brief Demonstrate basic format detection
 */
void demonstrateBasicDetection() {
    cout << "\n=== Basic Format Detection ===\n";

    FormatDetector detector;

    // Detect from file paths
    vector<string> testFiles = {"image.jpg", "photo.png", "data.fits",
                                "video.ser"};

    for (const auto& file : testFiles) {
        auto result = detector.detectFromFile(file);

        cout << "File: " << file << "\n";
        cout << "  Format: " << static_cast<int>(result.format) << "\n";
        cout << "  Confidence: " << static_cast<int>(result.confidence) << "\n";
        cout << "  MIME type: " << result.mimeType << "\n";
        cout << "  Description: " << result.description << "\n";
    }
}

/**
 * @brief Demonstrate extension-based detection
 */
void demonstrateExtensionDetection() {
    cout << "\n=== Extension-Based Detection ===\n";

    FormatDetector detector;

    vector<string> extensions = {".jpg", ".png", ".fits", ".tiff", ".bmp"};

    for (const auto& ext : extensions) {
        auto result = detector.detectFromExtension(ext);
        cout << "Extension " << ext << ": " << static_cast<int>(result.format)
             << "\n";
    }
}

/**
 * @brief Demonstrate format capabilities
 */
void demonstrateFormatCapabilities() {
    cout << "\n=== Format Capabilities ===\n";

    FormatDetector detector;

    vector<ImageFormat> formats = {ImageFormat::JPEG, ImageFormat::PNG,
                                   ImageFormat::FITS, ImageFormat::TIFF,
                                   ImageFormat::BMP};

    for (const auto& format : formats) {
        cout << "Format " << static_cast<int>(format) << ":\n";
        cout << "  Read support: "
             << (detector.isReadSupported(format) ? "Yes" : "No") << "\n";
        cout << "  Write support: "
             << (detector.isWriteSupported(format) ? "Yes" : "No") << "\n";
        cout << "  MIME type: " << detector.getMimeType(format) << "\n";

        auto extensions = detector.getExtensions(format);
        cout << "  Extensions: ";
        for (const auto& ext : extensions) {
            cout << ext << " ";
        }
        cout << "\n";
    }
}

/**
 * @brief Demonstrate memory buffer detection
 */
void demonstrateMemoryDetection() {
    cout << "\n=== Memory Buffer Detection ===\n";

    FormatDetector detector;

    // JPEG magic bytes
    vector<uint8_t> jpegData = {0xFF, 0xD8, 0xFF, 0xE0};
    auto jpegResult =
        detector.detectFromMemory(jpegData.data(), jpegData.size());
    cout << "JPEG magic bytes detected as: "
         << static_cast<int>(jpegResult.format) << "\n";

    // PNG magic bytes
    vector<uint8_t> pngData = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    auto pngResult = detector.detectFromMemory(pngData.data(), pngData.size());
    cout << "PNG magic bytes detected as: "
         << static_cast<int>(pngResult.format) << "\n";
}

/**
 * @brief Demonstrate quick detection functions
 */
void demonstrateQuickDetection() {
    cout << "\n=== Quick Detection Functions ===\n";

    // Quick file detection
    auto format1 = quickDetectFormat("image.jpg");
    cout << "Quick detect image.jpg: " << static_cast<int>(format1) << "\n";

    // Quick memory detection
    vector<uint8_t> data = {0xFF, 0xD8, 0xFF, 0xE0};
    auto format2 = quickDetectFormat(data.data(), data.size());
    cout << "Quick detect JPEG bytes: " << static_cast<int>(format2) << "\n";
}

/**
 * @brief Demonstrate format information
 */
void demonstrateFormatInfo() {
    cout << "\n=== Format Information ===\n";

    FormatDetector detector;

    // Get all supported formats
    auto formats = detector.getSupportedFormats();
    cout << "Total supported formats: " << formats.size() << "\n";

    // Show details for each format
    cout << "\nSupported formats:\n";
    for (const auto& format : formats) {
        if (format != ImageFormat::UNKNOWN) {
            cout << "  - " << detector.getMimeType(format) << "\n";
        }
    }
}

/**
 * @brief Demonstrate confidence levels
 */
void demonstrateConfidenceLevels() {
    cout << "\n=== Detection Confidence Levels ===\n";

    FormatDetector detector;

    // File with clear extension
    auto result1 = detector.detectFromFile("image.jpg");
    cout << "image.jpg confidence: " << static_cast<int>(result1.confidence)
         << "\n";

    // File with ambiguous extension
    auto result2 = detector.detectFromExtension(".dat");
    cout << ".dat extension confidence: "
         << static_cast<int>(result2.confidence) << "\n";
}

int main() {
    demonstrateBasicDetection();
    demonstrateExtensionDetection();
    demonstrateFormatCapabilities();
    demonstrateMemoryDetection();
    demonstrateQuickDetection();
    demonstrateFormatInfo();
    demonstrateConfidenceLevels();

    return 0;
}
