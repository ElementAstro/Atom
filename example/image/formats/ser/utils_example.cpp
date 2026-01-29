/**
 * @file utils_example.cpp
 * @brief Example demonstrating SER utility functions
 *
 * This example covers:
 * - Bit depth conversion
 * - Color space conversion
 * - Image normalization
 * - File utilities
 * - Image statistics
 * - Bad pixel detection and correction
 * - Quality metrics (PSNR, SSIM)
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iomanip>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "atom/image/formats/ser/ser.hpp"

using namespace serastro;
using namespace serastro::utils;
using namespace std;

/**
 * @brief Demonstrate bit depth conversion
 */
void demonstrateBitDepthConversion() {
    cout << "\n=== Bit Depth Conversion ===\n";

    try {
        // Create 8-bit test image
        cv::Mat img8(480, 640, CV_8UC1, cv::Scalar(128));

        cout << "Original: " << img8.depth() << " bit\n";

        // Convert to 16-bit
        cv::Mat img16 = convertBitDepth(img8, 16, true);
        cout << "Converted to 16-bit: " << img16.depth() << "\n";

        // Convert back to 8-bit
        cv::Mat img8_back = convertBitDepth(img16, 8, true);
        cout << "Converted back to 8-bit: " << img8_back.depth() << "\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate normalization methods
 */
void demonstrateNormalization() {
    cout << "\n=== Image Normalization ===\n";

    try {
        // Create test image with varying intensities
        cv::Mat img(480, 640, CV_8UC1);
        cv::randu(img, 50, 200);

        // Calculate statistics before normalization
        auto statsBefore = calculateImageStatistics(img);
        cout << "Before normalization:\n";
        cout << "  Mean: " << statsBefore.mean << "\n";
        cout << "  Min: " << statsBefore.min << "\n";
        cout << "  Max: " << statsBefore.max << "\n";

        // Normalize to 0-1 range
        cv::Mat normalized = normalize(img, 0.0, 1.0);
        auto statsAfter = calculateImageStatistics(normalized);
        cout << "\nAfter normalization (0-1):\n";
        cout << "  Mean: " << statsAfter.mean << "\n";
        cout << "  Min: " << statsAfter.min << "\n";
        cout << "  Max: " << statsAfter.max << "\n";

        // Percentile normalization
        cv::Mat percentileNorm = normalizePercentile(img, 1.0, 99.0);
        cout << "\nPercentile normalization applied\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate image statistics
 */
void demonstrateImageStatistics() {
    cout << "\n=== Image Statistics ===\n";

    try {
        cv::Mat img(480, 640, CV_8UC1);
        cv::randn(img, 128, 30);

        auto stats = calculateImageStatistics(img);

        cout << fixed << setprecision(2);
        cout << "Image Statistics:\n";
        cout << "  Mean: " << stats.mean << "\n";
        cout << "  Std Dev: " << stats.stdDev << "\n";
        cout << "  Min: " << stats.min << "\n";
        cout << "  Max: " << stats.max << "\n";
        cout << "  Median: " << stats.median << "\n";
        cout << "  5th percentile: " << stats.percentile05 << "\n";
        cout << "  95th percentile: " << stats.percentile95 << "\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate bad pixel detection
 */
void demonstrateBadPixelDetection() {
    cout << "\n=== Bad Pixel Detection ===\n";

    try {
        // Create image with some bad pixels
        cv::Mat img(480, 640, CV_8UC1, cv::Scalar(128));

        // Add hot pixels
        img.at<uint8_t>(100, 100) = 255;
        img.at<uint8_t>(200, 300) = 255;

        // Add cold pixels
        img.at<uint8_t>(150, 150) = 0;
        img.at<uint8_t>(250, 350) = 0;

        // Detect bad pixels
        auto hotPixels = detectHotPixels(img, 0.95);
        auto coldPixels = detectColdPixels(img, 0.05);

        cout << "Detected " << hotPixels.size() << " hot pixels\n";
        cout << "Detected " << coldPixels.size() << " cold pixels\n";

        // Create bad pixel mask
        cv::Mat badPixelMask = createBadPixelMask(img, 0.95, 0.05);
        cout << "Created bad pixel mask\n";

        // Fix bad pixels
        cv::Mat fixed = fixBadPixels(img, badPixelMask);
        cout << "Fixed bad pixels\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate quality metrics
 */
void demonstrateQualityMetrics() {
    cout << "\n=== Quality Metrics ===\n";

    try {
        // Create reference image
        cv::Mat reference(480, 640, CV_8UC1, cv::Scalar(128));

        // Create slightly degraded version
        cv::Mat degraded = reference.clone();
        cv::Mat noise(degraded.size(), degraded.type());
        cv::randn(noise, 0, 5);
        degraded += noise;

        // Calculate PSNR
        double psnr = calculatePSNR(reference, degraded);
        cout << "PSNR: " << fixed << setprecision(2) << psnr << " dB\n";

        // Calculate SSIM
        double ssim = calculateSSIM(reference, degraded);
        cout << "SSIM: " << fixed << setprecision(4) << ssim << "\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

int main() {
    demonstrateBitDepthConversion();
    demonstrateNormalization();
    demonstrateImageStatistics();
    demonstrateBadPixelDetection();
    demonstrateQualityMetrics();

    return 0;
}
