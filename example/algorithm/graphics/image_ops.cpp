/*
 * image_ops.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * Example demonstrating image operations from
 * atom/algorithm/graphics/image_ops.hpp
 */

#include "atom/algorithm/graphics/image_ops.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace atom::algorithm;

// Helper function to print a small image as ASCII
template <typename T>
void printImage(const std::vector<T>& image, i32 width, i32 height,
                const std::string& title) {
    std::cout << "\n" << title << " (" << width << "x" << height << "):\n";

    T min_val = *std::min_element(image.begin(), image.end());
    T max_val = *std::max_element(image.begin(), image.end());
    T range = max_val - min_val;

    if (range == 0)
        range = 1;

    const char* gradient = " .:-=+*#%@";

    for (i32 y = 0; y < height; ++y) {
        for (i32 x = 0; x < width; ++x) {
            T val = image[y * width + x];
            int idx = static_cast<int>((val - min_val) / range * 9);
            idx = std::clamp(idx, 0, 9);
            std::cout << gradient[idx];
        }
        std::cout << "\n";
    }
}

// Generate a simple test image
std::vector<u8> generateTestImage(i32 width, i32 height) {
    std::vector<u8> image(width * height);

    for (i32 y = 0; y < height; ++y) {
        for (i32 x = 0; x < width; ++x) {
            // Create a pattern with gradients and shapes
            f32 cx = static_cast<f32>(x) - width / 2.0f;
            f32 cy = static_cast<f32>(y) - height / 2.0f;
            f32 dist = std::sqrt(cx * cx + cy * cy);

            // Circular gradient with some variation
            f32 value = 255.0f * (1.0f - dist / (width / 2.0f));
            value = std::clamp(value, 0.0f, 255.0f);

            // Add some horizontal stripes
            if ((y / 3) % 2 == 0) {
                value *= 0.8f;
            }

            image[y * width + x] = static_cast<u8>(value);
        }
    }

    return image;
}

// Generate noisy image
std::vector<u8> generateNoisyImage(i32 width, i32 height) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 255);

    std::vector<u8> image(width * height);
    for (auto& pixel : image) {
        pixel = static_cast<u8>(dis(gen));
    }
    return image;
}

// Demonstrate convolution with custom kernel
void demonstrateConvolution() {
    std::cout << "\n=== Custom Convolution ===\n";

    constexpr i32 WIDTH = 20;
    constexpr i32 HEIGHT = 15;

    auto image = generateTestImage(WIDTH, HEIGHT);
    printImage(image, WIDTH, HEIGHT, "Original Image");

    // Sharpen kernel
    std::vector<f32> sharpen_kernel = {0, -1, 0, -1, 5, -1, 0, -1, 0};

    auto sharpened =
        ImageOps::convolve<u8>(image, WIDTH, HEIGHT, sharpen_kernel, 3);
    printImage(sharpened, WIDTH, HEIGHT, "Sharpened Image");

    // Box blur kernel
    std::vector<f32> blur_kernel(9, 1.0f / 9.0f);

    auto blurred = ImageOps::convolve<u8>(image, WIDTH, HEIGHT, blur_kernel, 3);
    printImage(blurred, WIDTH, HEIGHT, "Box Blurred Image");
}

// Demonstrate Gaussian blur
void demonstrateGaussianBlur() {
    std::cout << "\n=== Gaussian Blur ===\n";

    constexpr i32 WIDTH = 20;
    constexpr i32 HEIGHT = 15;

    auto image = generateTestImage(WIDTH, HEIGHT);
    printImage(image, WIDTH, HEIGHT, "Original Image");

    // Different sigma values
    for (f32 sigma : {0.5f, 1.0f, 2.0f}) {
        auto blurred = ImageOps::gaussianBlur<u8>(image, WIDTH, HEIGHT, sigma);
        std::cout << "\nGaussian Blur (sigma=" << sigma << "):\n";
        printImage(blurred, WIDTH, HEIGHT,
                   "Blurred (sigma=" + std::to_string(sigma) + ")");
    }
}

// Demonstrate Sobel edge detection
void demonstrateSobelEdgeDetection() {
    std::cout << "\n=== Sobel Edge Detection ===\n";

    constexpr i32 WIDTH = 25;
    constexpr i32 HEIGHT = 15;

    auto image = generateTestImage(WIDTH, HEIGHT);
    printImage(image, WIDTH, HEIGHT, "Original Image");

    auto edges = ImageOps::sobelEdgeDetection<u8>(image, WIDTH, HEIGHT);
    printImage(edges, WIDTH, HEIGHT, "Sobel Edges");
}

// Demonstrate Laplacian edge detection
void demonstrateLaplacianEdgeDetection() {
    std::cout << "\n=== Laplacian Edge Detection ===\n";

    constexpr i32 WIDTH = 25;
    constexpr i32 HEIGHT = 15;

    auto image = generateTestImage(WIDTH, HEIGHT);
    printImage(image, WIDTH, HEIGHT, "Original Image");

    auto edges = ImageOps::laplacianEdgeDetection<u8>(image, WIDTH, HEIGHT);
    printImage(edges, WIDTH, HEIGHT, "Laplacian Edges");
}

// Demonstrate brightness adjustment
void demonstrateBrightnessAdjustment() {
    std::cout << "\n=== Brightness Adjustment ===\n";

    constexpr i32 WIDTH = 20;
    constexpr i32 HEIGHT = 10;

    auto image = generateTestImage(WIDTH, HEIGHT);
    printImage(image, WIDTH, HEIGHT, "Original Image");

    // Increase brightness
    auto brighter = ImageOps::adjustBrightness<u8>(image, 50);
    printImage(brighter, WIDTH, HEIGHT, "Brighter (+50)");

    // Decrease brightness
    auto darker = ImageOps::adjustBrightness<u8>(image, -50);
    printImage(darker, WIDTH, HEIGHT, "Darker (-50)");
}

// Demonstrate contrast adjustment
void demonstrateContrastAdjustment() {
    std::cout << "\n=== Contrast Adjustment ===\n";

    constexpr i32 WIDTH = 20;
    constexpr i32 HEIGHT = 10;

    auto image = generateTestImage(WIDTH, HEIGHT);
    printImage(image, WIDTH, HEIGHT, "Original Image");

    // Increase contrast
    auto high_contrast = ImageOps::adjustContrast<u8>(image, 1.5f);
    printImage(high_contrast, WIDTH, HEIGHT, "High Contrast (1.5x)");

    // Decrease contrast
    auto low_contrast = ImageOps::adjustContrast<u8>(image, 0.5f);
    printImage(low_contrast, WIDTH, HEIGHT, "Low Contrast (0.5x)");
}

// Demonstrate histogram equalization
void demonstrateHistogramEqualization() {
    std::cout << "\n=== Histogram Equalization ===\n";

    constexpr i32 WIDTH = 20;
    constexpr i32 HEIGHT = 15;

    // Create a low-contrast image
    std::vector<u8> low_contrast_image(WIDTH * HEIGHT);
    for (i32 y = 0; y < HEIGHT; ++y) {
        for (i32 x = 0; x < WIDTH; ++x) {
            // Values concentrated in middle range
            f32 val = 100.0f + 50.0f * std::sin(x * 0.5f) * std::cos(y * 0.5f);
            low_contrast_image[y * WIDTH + x] = static_cast<u8>(val);
        }
    }

    printImage(low_contrast_image, WIDTH, HEIGHT, "Low Contrast Image");

    auto equalized = ImageOps::histogramEqualization<u8>(low_contrast_image);
    printImage(equalized, WIDTH, HEIGHT, "Histogram Equalized");
}

// Demonstrate image thresholding
void demonstrateThresholding() {
    std::cout << "\n=== Image Thresholding ===\n";

    constexpr i32 WIDTH = 20;
    constexpr i32 HEIGHT = 15;

    auto image = generateTestImage(WIDTH, HEIGHT);
    printImage(image, WIDTH, HEIGHT, "Original Image");

    // Binary threshold
    auto binary = ImageOps::threshold<u8>(image, 128);
    printImage(binary, WIDTH, HEIGHT, "Binary Threshold (128)");
}

// Demonstrate image inversion
void demonstrateInversion() {
    std::cout << "\n=== Image Inversion ===\n";

    constexpr i32 WIDTH = 20;
    constexpr i32 HEIGHT = 10;

    auto image = generateTestImage(WIDTH, HEIGHT);
    printImage(image, WIDTH, HEIGHT, "Original Image");

    auto inverted = ImageOps::invert<u8>(image);
    printImage(inverted, WIDTH, HEIGHT, "Inverted Image");
}

// Benchmark image operations
void benchmarkImageOps() {
    std::cout << "\n=== Image Operations Benchmark ===\n";

    constexpr i32 WIDTH = 256;
    constexpr i32 HEIGHT = 256;
    constexpr int ITERATIONS = 10;

    auto image = generateTestImage(WIDTH, HEIGHT);

    // Benchmark Gaussian blur
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        auto result = ImageOps::gaussianBlur<u8>(image, WIDTH, HEIGHT, 1.0f);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Gaussian blur (" << WIDTH << "x" << HEIGHT
              << "): " << duration.count() / ITERATIONS << " ms/iteration\n";

    // Benchmark Sobel edge detection
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        auto result = ImageOps::sobelEdgeDetection<u8>(image, WIDTH, HEIGHT);
    }
    end = std::chrono::high_resolution_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Sobel edge detection (" << WIDTH << "x" << HEIGHT
              << "): " << duration.count() / ITERATIONS << " ms/iteration\n";

    // Benchmark histogram equalization
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        auto result = ImageOps::histogramEqualization<u8>(image);
    }
    end = std::chrono::high_resolution_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Histogram equalization (" << WIDTH << "x" << HEIGHT
              << "): " << duration.count() / ITERATIONS << " ms/iteration\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   Image Operations Example\n";
    std::cout << "========================================\n";

    try {
        demonstrateConvolution();
        demonstrateGaussianBlur();
        demonstrateSobelEdgeDetection();
        demonstrateLaplacianEdgeDetection();
        demonstrateBrightnessAdjustment();
        demonstrateContrastAdjustment();
        demonstrateHistogramEqualization();
        demonstrateThresholding();
        demonstrateInversion();
        benchmarkImageOps();

        std::cout << "\n========================================\n";
        std::cout << "   All examples completed successfully!\n";
        std::cout << "========================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
