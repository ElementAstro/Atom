/**
 * @file backend_integration.cpp
 * @brief Example demonstrating integration with different image processing
 * backends
 *
 * This example covers:
 * - OpenCV backend integration
 * - CImg backend integration
 * - stb_image backend integration
 * - Backend-specific operations
 * - Performance comparisons
 * - Conditional compilation for optional backends
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <chrono>
#include <iostream>
#include <memory>

#include "atom/image/core/image_blob.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Demonstrate OpenCV backend integration
 */
void demonstrateOpenCVBackend() {
    std::cout << "\n=== OpenCV Backend Integration ===\n";

#if __has_include(<opencv2/core.hpp>)
    try {
        // Create a blob and convert to OpenCV Mat
        blob<uint8_t> img(200, 300, 3);

        // Fill with test pattern
        for (int y = 0; y < img.rows(); ++y) {
            for (int x = 0; x < img.cols(); ++x) {
                img.at(y, x, 0) = static_cast<uint8_t>((x + y) % 256);
                img.at(y, x, 1) = static_cast<uint8_t>(x % 256);
                img.at(y, x, 2) = static_cast<uint8_t>(y % 256);
            }
        }

        std::cout << "Created blob: " << img.cols() << "x" << img.rows()
                  << "\n";

        // Convert to OpenCV Mat
        auto start = high_resolution_clock::now();
        cv::Mat cvMat = img.to_mat();
        auto end = high_resolution_clock::now();

        std::cout << "Converted to OpenCV Mat: " << cvMat.cols << "x"
                  << cvMat.rows << " (type: " << cvMat.type() << ")\n";
        std::cout << "Conversion time: "
                  << duration_cast<microseconds>(end - start).count()
                  << " microseconds\n";

        // Apply OpenCV-specific operations
        std::cout << "Applying OpenCV Gaussian blur...\n";
        cv::Mat kernel = cv::getGaussianKernel(5, 1.0, CV_32F);
        img.apply_filter(kernel);

        std::cout << "Applying OpenCV resize...\n";
        img.resize(150, 100);
        std::cout << "After resize: " << img.cols() << "x" << img.rows()
                  << "\n";

        // Color space conversion
        std::cout << "Applying color space conversion (BGR to HSV)...\n";
        img.convert_color(cv::COLOR_BGR2HSV);
        std::cout << "Converted to HSV color space\n";

        // Create blob from existing OpenCV Mat
        cv::Mat testMat(100, 150, CV_8UC3);
        testMat.setTo(cv::Scalar(128, 64, 192));

        blob<uint8_t> fromMat(testMat);
        std::cout << "Created blob from OpenCV Mat: " << fromMat.cols() << "x"
                  << fromMat.rows() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in OpenCV backend: " << e.what() << "\n";
    }
#else
    std::cout << "OpenCV backend not available (OpenCV headers not found)\n";
    std::cout << "To enable OpenCV support, install OpenCV and ensure headers "
                 "are in include path\n";
#endif
}

/**
 * @brief Demonstrate CImg backend integration
 */
void demonstrateCImgBackend() {
    std::cout << "\n=== CImg Backend Integration ===\n";

#if __has_include(<CImg.h>)
    try {
        // Create CImg image
        cimg_library::CImg<unsigned char> cimg(150, 100, 1, 3);

        // Fill with gradient pattern
        cimg_forXYC(cimg, x, y, c) {
            cimg(x, y, c) = static_cast<unsigned char>((x + y + c * 50) % 256);
        }

        std::cout << "Created CImg: " << cimg.width() << "x" << cimg.height()
                  << "x" << cimg.spectrum() << "\n";

        // Convert CImg to blob
        auto start = high_resolution_clock::now();
        blob<uint8_t> img(cimg);
        auto end = high_resolution_clock::now();

        std::cout << "Converted to blob: " << img.cols() << "x" << img.rows()
                  << " with " << img.channels() << " channels\n";
        std::cout << "Conversion time: "
                  << duration_cast<microseconds>(end - start).count()
                  << " microseconds\n";

        // Perform some operations
        img.resize(200, 150);
        img.rotate(45.0);

        // Convert back to CImg
        start = high_resolution_clock::now();
        auto resultCImg = img.to_cimg();
        end = high_resolution_clock::now();

        std::cout << "Converted back to CImg: " << resultCImg.width() << "x"
                  << resultCImg.height() << "\n";
        std::cout << "Back-conversion time: "
                  << duration_cast<microseconds>(end - start).count()
                  << " microseconds\n";

        // CImg-specific operations could be applied here
        std::cout << "CImg backend integration successful\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in CImg backend: " << e.what() << "\n";
    }
#else
    std::cout << "CImg backend not available (CImg.h not found)\n";
    std::cout << "To enable CImg support, install CImg library and ensure "
                 "CImg.h is in include path\n";
#endif
}

/**
 * @brief Demonstrate stb_image backend integration
 */
void demonstrateStbImageBackend() {
    std::cout << "\n=== stb_image Backend Integration ===\n";

    try {
        // stb_image is typically used for loading/saving images from files
        // Since we don't have actual image files in this example, we'll
        // demonstrate the concept of how stb_image integration would work

        std::cout
            << "stb_image backend is primarily used for file I/O operations\n";
        std::cout << "It provides lightweight image loading/saving without "
                     "external dependencies\n";

        // Create a blob that could be saved using stb_image
        blob<uint8_t> img(128, 128, 3);

        // Fill with a simple pattern
        for (int y = 0; y < img.rows(); ++y) {
            for (int x = 0; x < img.cols(); ++x) {
                img.at(y, x, 0) = static_cast<uint8_t>((x * 2) % 256);
                img.at(y, x, 1) = static_cast<uint8_t>((y * 2) % 256);
                img.at(y, x, 2) = static_cast<uint8_t>((x + y) % 256);
            }
        }

        std::cout << "Created image suitable for stb_image: " << img.cols()
                  << "x" << img.rows() << "\n";

        // In a real implementation, you would use stb_image functions like:
        // stbi_write_png("output.png", img.cols(), img.rows(), img.channels(),
        // img.data(), 0); unsigned char* data = stbi_load("input.jpg", &width,
        // &height, &channels, 0);

        std::cout
            << "stb_image provides functions for PNG, JPEG, BMP, TGA formats\n";
        std::cout
            << "It's ideal for applications that need minimal dependencies\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in stb_image backend demonstration: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Compare performance across different backends
 */
void compareBackendPerformance() {
    std::cout << "\n=== Backend Performance Comparison ===\n";

    const int width = 512;
    const int height = 512;
    const int channels = 3;
    const int iterations = 100;

    try {
        // Create test data
        std::vector<uint8_t> testData(width * height * channels);
        for (size_t i = 0; i < testData.size(); ++i) {
            testData[i] = static_cast<uint8_t>(i % 256);
        }

        std::cout << "Performance test with " << width << "x" << height << "x"
                  << channels << " image, " << iterations << " iterations\n\n";

        // Test blob creation performance
        auto start = high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            blob<uint8_t> img(testData.data(), height, width, channels);
            // Prevent optimization
            volatile auto size = img.size();
        }
        auto end = high_resolution_clock::now();
        auto blobTime = duration_cast<microseconds>(end - start);

        std::cout << "Blob creation (with copy): "
                  << blobTime.count() / iterations << " μs per operation\n";

        // Test fast blob creation performance
        start = high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            fast_blob<uint8_t> img(testData.data(), height, width, channels);
            // Prevent optimization
            volatile auto size = img.size();
        }
        end = high_resolution_clock::now();
        auto fastBlobTime = duration_cast<microseconds>(end - start);

        std::cout << "Fast blob creation (view-only): "
                  << fastBlobTime.count() / iterations << " μs per operation\n";

        std::cout << "Speed improvement: "
                  << static_cast<double>(blobTime.count()) /
                         fastBlobTime.count()
                  << "x faster\n";

#if __has_include(<opencv2/core.hpp>)
        // Test OpenCV Mat creation
        start = high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            cv::Mat mat(height, width, CV_8UC3, testData.data());
            // Prevent optimization
            volatile auto size = mat.total();
        }
        end = high_resolution_clock::now();
        auto matTime = duration_cast<microseconds>(end - start);

        std::cout << "OpenCV Mat creation: " << matTime.count() / iterations
                  << " μs per operation\n";
#endif

    } catch (const std::exception& e) {
        std::cerr << "Error in performance comparison: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate backend feature matrix
 */
void demonstrateFeatureMatrix() {
    std::cout << "\n=== Backend Feature Matrix ===\n";

    std::cout << "Feature comparison across backends:\n\n";

    std::cout
        << "| Feature                | Blob | OpenCV | CImg | stb_image |\n";
    std::cout
        << "|------------------------|------|--------|------|----------|\n";
    std::cout
        << "| Basic operations       |  ✓   |   ✓    |  ✓   |    -     |\n";
    std::cout
        << "| Memory efficiency      |  ✓   |   ✓    |  ✓   |    ✓     |\n";
    std::cout
        << "| File I/O               |  -   |   ✓    |  ✓   |    ✓     |\n";
    std::cout
        << "| Advanced filters       |  -   |   ✓    |  ✓   |    -     |\n";
    std::cout
        << "| Computer vision        |  -   |   ✓    |  -   |    -     |\n";
    std::cout
        << "| No dependencies        |  ✓   |   -    |  -   |    ✓     |\n";
    std::cout
        << "| Template-based         |  ✓   |   -    |  ✓   |    -     |\n";
    std::cout
        << "| GPU acceleration       |  -   |   ✓    |  -   |    -     |\n";
    std::cout
        << "| Serialization          |  ✓   |   -    |  -   |    -     |\n";
    std::cout
        << "| View-only mode         |  ✓   |   ✓    |  -   |    -     |\n";

    std::cout << "\nRecommendations:\n";
    std::cout << "- Use Blob for: Core operations, serialization, memory "
                 "efficiency\n";
    std::cout << "- Use OpenCV for: Computer vision, advanced processing, GPU "
                 "acceleration\n";
    std::cout
        << "- Use CImg for: Scientific computing, template-based processing\n";
    std::cout
        << "- Use stb_image for: Lightweight file I/O, minimal dependencies\n";
}

int main() {
    std::cout << "=== Atom Image Backend Integration Example ===\n";
    std::cout << "This example demonstrates integration with different image "
                 "processing backends\n";

    // Demonstrate each backend
    demonstrateOpenCVBackend();
    demonstrateCImgBackend();
    demonstrateStbImageBackend();

    // Performance and feature comparisons
    compareBackendPerformance();
    demonstrateFeatureMatrix();

    std::cout << "\n=== Backend integration example completed ===\n";
    return 0;
}
