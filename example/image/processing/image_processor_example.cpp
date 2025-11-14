/**
 * @file image_processor_example.cpp
 * @brief Example demonstrating image processing operations
 *
 * This example covers:
 * - Basic image transformations (resize, rotate, crop)
 * - Filtering operations (blur, sharpen, median)
 * - Image enhancement (brightness, contrast, gamma)
 * - Edge detection and denoising
 * - Batch processing
 * - Image statistics and quality metrics
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/processing/image_processor.hpp"

using namespace atom::image;
using namespace std;

/**
 * @brief Create a test image
 */
blob createTestImage() {
    blob img(480, 640, 3);
    return img;
}

/**
 * @brief Demonstrate basic transformations
 */
void demonstrateBasicTransformations() {
    cout << "\n=== Basic Transformations ===\n";

    auto processor = createOptimalProcessor();
    auto img = createTestImage();

    // Resize with different algorithms
    auto resized_nearest = processor->resize(img, 800, 600, "nearest");
    auto resized_linear = processor->resize(img, 800, 600, "linear");
    auto resized_cubic = processor->resize(img, 800, 600, "cubic");
    auto resized_lanczos = processor->resize(img, 800, 600, "lanczos");

    cout << "Resized with different algorithms:\n";
    cout << "  Nearest: " << resized_nearest.width() << "x"
         << resized_nearest.height() << "\n";
    cout << "  Linear: " << resized_linear.width() << "x"
         << resized_linear.height() << "\n";
    cout << "  Cubic: " << resized_cubic.width() << "x"
         << resized_cubic.height() << "\n";
    cout << "  Lanczos: " << resized_lanczos.width() << "x"
         << resized_lanczos.height() << "\n";

    // Rotate
    auto rotated = processor->rotate(img, 45.0, true);
    cout << "Rotated 45 degrees: " << rotated.width() << "x" << rotated.height()
         << "\n";

    // Crop
    auto cropped = processor->crop(img, 100, 100, 200, 150);
    cout << "Cropped: " << cropped.width() << "x" << cropped.height() << "\n";
}

/**
 * @brief Demonstrate filtering operations
 */
void demonstrateFilteringOperations() {
    cout << "\n=== Filtering Operations ===\n";

    auto processor = createOptimalProcessor();
    auto img = createTestImage();

    // Gaussian blur
    auto blurred = processor->applyFilter(img, FilterType::GAUSSIAN_BLUR,
                                          {{"sigma", 2.0}});
    cout << "Applied Gaussian blur\n";

    // Sharpen
    auto sharpened =
        processor->applyFilter(img, FilterType::SHARPEN, {{"strength", 1.5}});
    cout << "Applied sharpening\n";

    // Median filter
    auto median =
        processor->applyFilter(img, FilterType::MEDIAN, {{"kernelSize", 5}});
    cout << "Applied median filter\n";
}

/**
 * @brief Demonstrate image enhancement
 */
void demonstrateImageEnhancement() {
    cout << "\n=== Image Enhancement ===\n";

    auto processor = createOptimalProcessor();
    auto img = createTestImage();

    // Brightness and contrast
    auto enhanced = processor->adjustBrightnessContrast(img, 20, 10);
    cout << "Adjusted brightness (+20) and contrast (+10)\n";

    // Gamma correction
    auto gamma_corrected = processor->adjustGamma(img, 1.2);
    cout << "Applied gamma correction (1.2)\n";

    // Histogram equalization
    auto equalized = processor->enhanceHistogram(img, false);
    cout << "Applied histogram equalization\n";

    // Adaptive histogram equalization
    auto adaptive = processor->enhanceHistogram(img, true);
    cout << "Applied adaptive histogram equalization (CLAHE)\n";
}

/**
 * @brief Demonstrate edge detection
 */
void demonstrateEdgeDetection() {
    cout << "\n=== Edge Detection ===\n";

    auto processor = createOptimalProcessor();
    auto img = createTestImage();

    // Canny edge detection
    auto canny = processor->detectEdges(img, "canny", {50.0, 150.0});
    cout << "Applied Canny edge detection\n";

    // Sobel edge detection
    auto sobel = processor->detectEdges(img, "sobel");
    cout << "Applied Sobel edge detection\n";

    // Laplacian edge detection
    auto laplacian = processor->detectEdges(img, "laplacian");
    cout << "Applied Laplacian edge detection\n";
}

/**
 * @brief Demonstrate denoising
 */
void demonstrateDenoising() {
    cout << "\n=== Denoising ===\n";

    auto processor = createOptimalProcessor();
    auto img = createTestImage();

    // Gaussian denoising
    auto gaussian = processor->denoise(img, "gaussian", 0.5);
    cout << "Applied Gaussian denoising\n";

    // Median denoising
    auto median = processor->denoise(img, "median", 0.5);
    cout << "Applied median denoising\n";

    // Bilateral denoising
    auto bilateral = processor->denoise(img, "bilateral", 0.5);
    cout << "Applied bilateral denoising\n";

    // Non-local means denoising
    auto nlmeans = processor->denoise(img, "nlmeans", 0.5);
    cout << "Applied non-local means denoising\n";
}

/**
 * @brief Demonstrate batch processing
 */
void demonstrateBatchProcessing() {
    cout << "\n=== Batch Processing ===\n";

    auto processor = createOptimalProcessor();

    // Create multiple test images
    vector<blob> images;
    for (int i = 0; i < 5; ++i) {
        images.push_back(createTestImage());
    }

    // Process batch
    auto results = processor->processBatch(images, [&](const blob& img) {
        return processor->resize(img, 256, 256);
    });

    cout << "Processed " << results.size() << " images in batch\n";
    for (size_t i = 0; i < results.size(); ++i) {
        cout << "  Image " << i << ": " << results[i].width() << "x"
             << results[i].height() << "\n";
    }
}

/**
 * @brief Demonstrate image statistics
 */
void demonstrateImageStatistics() {
    cout << "\n=== Image Statistics ===\n";

    auto processor = createOptimalProcessor();
    auto img = createTestImage();

    auto stats = processor->getStatistics(img);

    cout << "Image statistics:\n";
    for (const auto& [key, value] : stats) {
        cout << "  " << key << ": " << value << "\n";
    }
}

int main() {
    demonstrateBasicTransformations();
    demonstrateFilteringOperations();
    demonstrateImageEnhancement();
    demonstrateEdgeDetection();
    demonstrateDenoising();
    demonstrateBatchProcessing();
    demonstrateImageStatistics();

    return 0;
}
