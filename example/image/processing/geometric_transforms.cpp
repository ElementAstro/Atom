/**
 * @file geometric_transforms.cpp
 * @brief Example demonstrating geometric transformation operations
 *
 * This example covers:
 * - Resize operations with different interpolation methods
 * - Rotation with various angles and options
 * - Cropping and region extraction
 * - Flipping and mirroring
 * - Affine transformations
 * - Perspective transformations
 * - Image registration and alignment
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/processing/image_processor.hpp"
#include "atom/image/processing/transforms.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Create a test image with geometric features
 */
blob createGeometricTestImage() {
    const int width = 300;
    const int height = 200;
    // Create raw data buffer
    std::vector<uint8_t> data(height * width * 3);

    // Create a pattern with geometric features
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Grid pattern
            bool grid_x = (x % 20) < 2;
            bool grid_y = (y % 20) < 2;

            // Diagonal lines
            bool diag1 = ((x + y) % 40) < 2;
            bool diag2 = ((x - y + height) % 40) < 2;

            // Circle
            int center_x = width / 2;
            int center_y = height / 2;
            double distance = std::sqrt((x - center_x) * (x - center_x) +
                                        (y - center_y) * (y - center_y));
            bool circle = (distance > 30 && distance < 35);

            // Rectangle
            bool rect = (x > width / 4 && x < 3 * width / 4 && y > height / 4 &&
                         y < 3 * height / 4 &&
                         (x < width / 3 || x > 2 * width / 3 ||
                          y < height / 3 || y > 2 * height / 3));

            uint8_t r = 128, g = 128, b = 128;  // Gray background

            if (grid_x || grid_y) {
                r = 255;
                g = 0;
                b = 0;  // Red grid
            } else if (diag1) {
                r = 0;
                g = 255;
                b = 0;  // Green diagonal
            } else if (diag2) {
                r = 0;
                g = 0;
                b = 255;  // Blue diagonal
            } else if (circle) {
                r = 255;
                g = 255;
                b = 0;  // Yellow circle
            } else if (rect) {
                r = 255;
                g = 0;
                b = 255;  // Magenta rectangle
            }

            // Direct access to data vector
            int pixel_idx = (y * width + x) * 3;
            data[pixel_idx] = r;
            data[pixel_idx + 1] = g;
            data[pixel_idx + 2] = b;
        }
    }

    return blob(reinterpret_cast<std::byte*>(data.data()), data.size(), height,
                width, 3, DEFAULT_DEPTH);
}

/**
 * @brief Demonstrate resize operations
 */
void demonstrateResize() {
    std::cout << "\n=== Resize Operations ===\n";

    try {
        auto original = createGeometricTestImage();
        std::cout << "Original image blob size: " << original.size()
                  << " bytes\n";

        ImageTransform transform;

        // Test different interpolation methods
        std::vector<std::pair<InterpolationMethod, std::string>> methods = {
            {InterpolationMethod::NEAREST, "Nearest Neighbor"},
            {InterpolationMethod::LINEAR, "Bilinear"},
            {InterpolationMethod::CUBIC, "Bicubic"},
            {InterpolationMethod::LANCZOS, "Lanczos"}};

        std::vector<std::pair<int, int>> target_sizes = {
            {150, 100},  // Downscale
            {600, 400},  // Upscale
            {300, 300},  // Aspect ratio change
            {450, 150}   // Extreme aspect ratio
        };

        for (const auto& [method, name] : methods) {
            std::cout << "\n" << name << " interpolation:\n";

            for (const auto& [width, height] : target_sizes) {
                auto start = high_resolution_clock::now();

                auto resized =
                    transform.resize(original, width, height, method, false);

                auto end = high_resolution_clock::now();
                auto duration = duration_cast<microseconds>(end - start);

                // Skip scale calculation due to lack of blob dimension methods
                std::cout << "  Resized to " << width << "x" << height << " in "
                          << duration.count() << " μs\n";
            }
        }

        // Demonstrate aspect ratio preservation
        std::cout << "\nAspect ratio preservation:\n";
        auto start = high_resolution_clock::now();
        auto resized_preserve = transform.resize(
            original, 400, 300, InterpolationMethod::LINEAR, true);
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);

        std::cout << "  Preserve aspect resize completed in "
                  << duration.count() << " μs\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in resize operations: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate rotation operations
 */
void demonstrateRotation() {
    std::cout << "\n=== Rotation Operations ===\n";

    try {
        auto original = createGeometricTestImage();
        ImageTransform transform;

        // Test different rotation angles
        std::vector<double> angles = {15, 30, 45, 90, 135, 180, -45, -90};

        for (double angle : angles) {
            auto start = high_resolution_clock::now();

            // Rotate with canvas expansion
            auto rotated = transform.rotate(original, angle, {}, true,
                                            InterpolationMethod::LINEAR,
                                            BorderMode::CONSTANT, 0);

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "  Rotate " << std::setw(4) << angle
                      << "° completed in " << duration.count() << " μs\n";
        }

        // Demonstrate rotation around custom center
        std::cout << "\nRotation around custom center:\n";
        Point2D custom_center = {150, 100};  // Center point
        auto start = high_resolution_clock::now();
        auto custom_rotated = transform.rotate(
            original, 45, custom_center, false, InterpolationMethod::LINEAR,
            BorderMode::CONSTANT, 0);
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);

        std::cout << "  Custom center (" << custom_center.x << ", "
                  << custom_center.y << "): " << duration.count() << " μs\n";

        // Demonstrate different border modes
        std::cout << "\nBorder handling modes (45° rotation):\n";
        std::vector<std::pair<BorderMode, std::string>> border_modes = {
            {BorderMode::CONSTANT, "Constant (black)"},
            {BorderMode::REFLECT, "Reflect"},
            {BorderMode::WRAP, "Wrap"},
            {BorderMode::REPLICATE, "Replicate"}};

        for (const auto& [mode, name] : border_modes) {
            auto start = high_resolution_clock::now();
            auto rotated =
                transform.rotate(original, 45, {}, false,
                                 InterpolationMethod::LINEAR, mode, 128);
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "  " << name << ": " << duration.count() << " μs\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in rotation operations: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate cropping operations
 */
void demonstrateCropping() {
    std::cout << "\n=== Cropping Operations ===\n";

    try {
        auto original = createGeometricTestImage();
        ImageTransform transform;

        // Test different crop regions
        std::vector<std::tuple<int, int, int, int, std::string>> crop_regions =
            {{50, 25, 200, 150, "Center crop"},
             {0, 0, 150, 100, "Top-left crop"},
             {150, 100, 150, 100, "Bottom-right crop"},
             {75, 50, 150, 100, "Custom region"},
             {10, 10, 280, 180, "Large crop"}};

        for (const auto& [x, y, width, height, description] : crop_regions) {
            auto start = high_resolution_clock::now();

            auto cropped = original.crop(x, y, width, height);

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "  " << description << ": "
                      << "(" << x << "," << y << "," << width << "," << height
                      << ") -> " << cropped.getCols() << "x"
                      << cropped.getRows() << " in " << duration.count()
                      << " μs\n";
        }

        // Demonstrate smart cropping (content-aware)
        std::cout << "\nSmart cropping simulation:\n";

        // Simulate finding the most interesting region
        int best_x = original.getCols() / 4;
        int best_y = original.getRows() / 4;
        int crop_width = original.getCols() / 2;
        int crop_height = original.getRows() / 2;

        auto start = high_resolution_clock::now();
        auto smart_crop =
            original.crop(best_x, best_y, crop_width, crop_height);
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);

        std::cout << "  Content-aware crop: " << duration.count() << " μs\n";
        std::cout << "  Region: (" << best_x << "," << best_y << ","
                  << crop_width << "," << crop_height << ")\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in cropping operations: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate flipping and mirroring
 */
void demonstrateFlipping() {
    std::cout << "\n=== Flipping and Mirroring ===\n";

    try {
        auto original = createGeometricTestImage();
        ImageTransform transform;

        std::vector<std::pair<int, std::string>> flip_modes = {
            {0, "Horizontal flip"},
            {1, "Vertical flip"},
            {-1, "Both directions"}};

        for (const auto& [mode, description] : flip_modes) {
            auto start = high_resolution_clock::now();

            blob flipped = original.clone();
            // Use Blob's flip via OpenCV wrapper when available
            flipped.flip(mode);

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "  " << description << ": " << duration.count()
                      << " μs\n";
        }

        // Demonstrate transpose operations
        // Note: transpose() method not available in current API
        // std::cout << "\nTranspose operations:\n";

        // auto start = high_resolution_clock::now();
        // auto transposed = transform.transpose(original);
        // auto end = high_resolution_clock::now();
        // auto duration = duration_cast<microseconds>(end - start);

        // std::cout << "  Transpose: " << original.getCols() << "x"
        //           << original.getRows() << " -> " << transposed.getCols() <<
        //           "x"
        //           << transposed.getRows() << " in " << duration.count() << "
        //           μs\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in flipping operations: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate affine transformations
 */
void demonstrateAffineTransforms() {
    std::cout << "\n=== Affine Transformations ===\n";

    try {
        auto original = createGeometricTestImage();
        ImageTransform transform;

        // Create different affine transformation matrices
        std::vector<std::pair<TransformMatrix, std::string>> transforms = {
            {{{1.5, 0, 0}, {0, 1.5, 0}, {0, 0, 1}}, "Scale 1.5x"},
            {{{1, 0.3, 0}, {0, 1, 0}, {0, 0, 1}}, "Shear X"},
            {{{1, 0, 0}, {0.3, 1, 0}, {0, 0, 1}}, "Shear Y"},
            {{{1, 0, 50}, {0, 1, 25}, {0, 0, 1}}, "Translation"},
            {{{0.8, 0.2, 0}, {-0.2, 0.8, 0}, {0, 0, 1}}, "Combined transform"}};

        for (const auto& [matrix, description] : transforms) {
            auto start = high_resolution_clock::now();

            auto transformed = transform.applyAffineTransform(
                original, matrix, InterpolationMethod::LINEAR,
                BorderMode::CONSTANT, 0);

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "  " << description << ": " << duration.count()
                      << " μs\n";
            std::cout << "    Result size: " << transformed.getCols() << "x"
                      << transformed.getRows() << "\n";
        }

        // Demonstrate transformation composition
        std::cout << "\nTransformation composition:\n";

        // Scale then rotate
        TransformMatrix scale = {{1.2, 0, 0}, {0, 1.2, 0}, {0, 0, 1}};
        TransformMatrix rotate = {
            {0.707, -0.707, 0}, {0.707, 0.707, 0}, {0, 0, 1}};  // 45° rotation

        auto start = high_resolution_clock::now();
        auto scaled = transform.applyAffineTransform(original, scale);
        auto final = transform.applyAffineTransform(scaled, rotate);
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);

        std::cout << "  Scale + Rotate: " << duration.count() << " μs\n";
        std::cout << "  Final size: " << final.getCols() << "x"
                  << final.getRows() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in affine transformations: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate perspective transformations
 */
void demonstratePerspectiveTransforms() {
    std::cout << "\n=== Perspective Transformations ===\n";

    try {
        auto original = createGeometricTestImage();
        ImageTransform transform;

        // Define source and destination points for perspective correction
        std::vector<Point2D> src_points = {
            {0, 0},
            {original.getCols() - 1, 0},
            {original.getCols() - 1, original.getRows() - 1},
            {0, original.getRows() - 1}};

        // Different perspective effects
        std::vector<std::pair<std::vector<Point2D>, std::string>>
            perspective_transforms = {
                {{{20, 10},
                  {original.getCols() - 21, 10},
                  {original.getCols() - 1, original.getRows() - 1},
                  {0, original.getRows() - 1}},
                 "Keystone correction"},

                {{{0, 20},
                  {original.getCols() - 1, 0},
                  {original.getCols() - 21, original.getRows() - 21},
                  {20, original.getRows() - 1}},
                 "Perspective tilt"},

                {{{30, 30},
                  {original.getCols() - 31, 20},
                  {original.getCols() - 21, original.getRows() - 31},
                  {20, original.getRows() - 21}},
                 "Complex perspective"}};

        for (const auto& [dst_points, description] : perspective_transforms) {
            auto start = high_resolution_clock::now();

            auto transformed = transform.applyPerspectiveTransform(
                original, src_points, dst_points, InterpolationMethod::LINEAR);

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "  " << description << ": " << duration.count()
                      << " μs\n";
            std::cout << "    Result size: " << transformed.getCols() << "x"
                      << transformed.getRows() << "\n";
        }

        // Demonstrate document rectification simulation
        std::cout << "\nDocument rectification simulation:\n";

        // Simulate detected document corners (skewed)
        std::vector<Point2D> document_corners = {
            {45, 30}, {250, 20}, {270, 170}, {25, 180}};

        // Target rectangle
        std::vector<Point2D> target_rect = {
            {0, 0}, {200, 0}, {200, 150}, {0, 150}};

        auto start = high_resolution_clock::now();
        auto rectified = transform.applyPerspectiveTransform(
            original, document_corners, target_rect,
            InterpolationMethod::CUBIC);
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);

        std::cout << "  Document rectification: " << duration.count()
                  << " μs\n";
        std::cout << "  Rectified size: " << rectified.getCols() << "x"
                  << rectified.getRows() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in perspective transformations: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrate performance comparison
 */
void demonstratePerformanceComparison() {
    std::cout << "\n=== Performance Comparison ===\n";

    try {
        auto original = createGeometricTestImage();
        ImageTransform transform;

        const int iterations = 100;

        std::cout << "Performance test (" << iterations << " iterations):\n";

        // Resize performance
        auto start = high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto resized = transform.resize(original, 150, 100,
                                            InterpolationMethod::LINEAR);
        }
        auto end = high_resolution_clock::now();
        auto resize_time = duration_cast<microseconds>(end - start);

        // Rotation performance
        start = high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto rotated = transform.rotate(original, 45);
        }
        end = high_resolution_clock::now();
        auto rotate_time = duration_cast<microseconds>(end - start);

        // Crop performance
        start = high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto cropped = transform.crop(original, 50, 25, 200, 150);
        }
        end = high_resolution_clock::now();
        auto crop_time = duration_cast<microseconds>(end - start);

        // Flip performance
        start = high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto flipped = transform.flip(original, FlipMode::HORIZONTAL);
        }
        end = high_resolution_clock::now();
        auto flip_time = duration_cast<microseconds>(end - start);

        std::cout << "Operation    | Total Time | Avg Time | Ops/sec\n";
        std::cout << "-------------|------------|----------|--------\n";
        std::cout << "Resize       | " << std::setw(8) << resize_time.count()
                  << " μs | " << std::setw(6)
                  << resize_time.count() / iterations << " μs | "
                  << std::setw(6)
                  << (1000000 * iterations) / resize_time.count() << "\n";
        std::cout << "Rotate       | " << std::setw(8) << rotate_time.count()
                  << " μs | " << std::setw(6)
                  << rotate_time.count() / iterations << " μs | "
                  << std::setw(6)
                  << (1000000 * iterations) / rotate_time.count() << "\n";
        std::cout << "Crop         | " << std::setw(8) << crop_time.count()
                  << " μs | " << std::setw(6) << crop_time.count() / iterations
                  << " μs | " << std::setw(6)
                  << (1000000 * iterations) / crop_time.count() << "\n";
        std::cout << "Flip         | " << std::setw(8) << flip_time.count()
                  << " μs | " << std::setw(6) << flip_time.count() / iterations
                  << " μs | " << std::setw(6)
                  << (1000000 * iterations) / flip_time.count() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in performance comparison: " << e.what() << "\n";
    }
}

int main() {
    std::cout << "=== Atom Image Geometric Transforms Example ===\n";
    std::cout << "This example demonstrates various geometric transformation "
                 "operations\n";

    // Run all demonstrations
    demonstrateResize();
    demonstrateRotation();
    demonstrateCropping();
    demonstrateFlipping();
    demonstrateAffineTransforms();
    demonstratePerspectiveTransforms();
    demonstratePerformanceComparison();

    std::cout << "\n=== Geometric transforms example completed ===\n";
    return 0;
}
