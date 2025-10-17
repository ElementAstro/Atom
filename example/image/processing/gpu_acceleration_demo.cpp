/**
 * @file gpu_acceleration_demo.cpp
 * @brief GPU acceleration demonstration with fallback mechanisms
 *
 * This example demonstrates:
 * - GPU context initialization and device detection
 * - GPU-accelerated image processing operations
 * - Performance comparison between GPU and CPU
 * - Fallback mechanisms for systems without GPU support
 * - Multi-backend GPU support (CUDA, OpenCL, etc.)
 * - Memory management for GPU operations
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/processing/gpu_acceleration.hpp"
#include "atom/image/processing/image_processor.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Create test image data for GPU processing
 */
blob createTestImage(int width, int height, int channels = 3) {
    std::vector<uint8_t> data(width * height * channels);

    // Create a test pattern
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                int index = (y * width + x) * channels + c;
                // Create a gradient pattern with some variation
                int value = ((x + y + c * 50) % 256);
                data[index] = static_cast<uint8_t>(value);
            }
        }
    }

    return blob(data.data(), data.size());
}

/**
 * @brief Demonstrate GPU device detection and capabilities
 */
void demonstrateGPUDetection() {
    std::cout << "\n=== GPU Device Detection ===\n";

    try {
        // Check available GPU backends
        std::vector<GPUBackend> backends = {
            GPUBackend::CUDA, GPUBackend::OPENCL, GPUBackend::VULKAN,
            GPUBackend::METAL, GPUBackend::AUTO};

        std::vector<std::string> backendNames = {"CUDA", "OpenCL", "Vulkan",
                                                 "Metal", "Auto-select"};

        for (size_t i = 0; i < backends.size(); ++i) {
            std::cout << "Checking " << backendNames[i] << " support:\n";

            bool isAvailable = GPUContext::isBackendAvailable(backends[i]);
            std::cout << "  Available: " << (isAvailable ? "YES" : "NO")
                      << "\n";

            if (isAvailable) {
                try {
                    auto devices = GPUContext::getAvailableDevices(backends[i]);
                    std::cout << "  Devices found: " << devices.size() << "\n";

                    for (size_t j = 0; j < devices.size(); ++j) {
                        const auto& device = devices[j];
                        std::cout << "    Device " << j << ":\n";
                        std::cout << "      Name: " << device.name << "\n";
                        std::cout << "      Vendor: " << device.vendor << "\n";
                        std::cout << "      Memory: "
                                  << (device.memorySize / (1024 * 1024))
                                  << " MB\n";
                        std::cout
                            << "      Compute Units: " << device.computeUnits
                            << "\n";
                        std::cout << "      Max Work Group Size: "
                                  << device.maxWorkGroupSize << "\n";
                        std::cout << "      Double Precision: "
                                  << (device.supportsDouble ? "YES" : "NO")
                                  << "\n";
                        std::cout << "      Half Precision: "
                                  << (device.supportsHalf ? "YES" : "NO")
                                  << "\n";
                    }
                } catch (const std::exception& e) {
                    std::cout << "  Error getting device info: " << e.what()
                              << "\n";
                }
            }
        }

        // Get optimal backend
        GPUBackend optimal = GPUContext::getOptimalBackend();
        std::cout << "\nOptimal backend: ";
        switch (optimal) {
            case GPUBackend::CUDA:
                std::cout << "CUDA";
                break;
            case GPUBackend::OPENCL:
                std::cout << "OpenCL";
                break;
            case GPUBackend::VULKAN:
                std::cout << "Vulkan";
                break;
            case GPUBackend::METAL:
                std::cout << "Metal";
                break;
            case GPUBackend::AUTO:
                std::cout << "Auto-select";
                break;
            default:
                std::cout << "Unknown";
                break;
        }
        std::cout << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in GPU detection: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate GPU-accelerated image processing
 */
void demonstrateGPUProcessing() {
    std::cout << "\n=== GPU-Accelerated Processing ===\n";

    try {
        // Create GPU processor
        GPUImageProcessor gpuProcessor;

        // Try to initialize with optimal backend
        bool gpuInitialized = gpuProcessor.initialize();

        if (!gpuInitialized) {
            std::cout << "GPU initialization failed, using CPU fallback\n";
            return;
        }

        std::cout << "GPU processor initialized successfully\n";

        // Create test image
        auto testImage = createTestImage(1024, 768, 3);
        std::cout << "Created test image: " << testImage.getWidth() << "x"
                  << testImage.getHeight() << "\n";

        // Upload image to GPU
        auto start = high_resolution_clock::now();
        auto gpuBuffer = gpuProcessor.uploadImage(testImage);
        auto uploadTime =
            duration_cast<microseconds>(high_resolution_clock::now() - start);

        if (!gpuBuffer) {
            std::cout << "Failed to upload image to GPU\n";
            return;
        }

        std::cout << "Image uploaded to GPU in " << uploadTime.count()
                  << " μs\n";

        // Test different GPU operations
        std::vector<std::string> operations = {"gaussian_blur", "resize",
                                               "histogram_equalization",
                                               "edge_detection"};

        for (const auto& operation : operations) {
            std::cout << "\nTesting " << operation << ":\n";

            try {
                start = high_resolution_clock::now();

                std::unique_ptr<GPUBuffer> result;
                if (operation == "gaussian_blur") {
                    result = gpuProcessor.gaussianBlur(
                        *gpuBuffer, 2.0f, 7, testImage.getWidth(),
                        testImage.getHeight(), 3);
                } else if (operation == "resize") {
                    result =
                        gpuProcessor.resize(*gpuBuffer, testImage.getWidth(),
                                            testImage.getHeight(), 512, 384, 3);
                } else if (operation == "histogram_equalization") {
                    result = gpuProcessor.equalizeHistogram(
                        *gpuBuffer, testImage.getWidth(), testImage.getHeight(),
                        3);
                } else if (operation == "edge_detection") {
                    std::vector<std::vector<float>> sobelKernel = {
                        {-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
                    result = gpuProcessor.convolve(*gpuBuffer, sobelKernel,
                                                   testImage.getWidth(),
                                                   testImage.getHeight(), 3);
                }

                auto processingTime = duration_cast<microseconds>(
                    high_resolution_clock::now() - start);

                if (result) {
                    std::cout << "  Processing time: " << processingTime.count()
                              << " μs\n";
                    std::cout << "  Result buffer size: " << result->getSize()
                              << " bytes\n";

                    // Download result for verification
                    start = high_resolution_clock::now();
                    std::vector<uint8_t> resultData(result->getSize());
                    bool downloaded =
                        result->download(resultData.data(), resultData.size());
                    auto downloadTime = duration_cast<microseconds>(
                        high_resolution_clock::now() - start);

                    if (downloaded) {
                        std::cout << "  Download time: " << downloadTime.count()
                                  << " μs\n";
                        std::cout
                            << "  Total GPU time: "
                            << (processingTime.count() + downloadTime.count())
                            << " μs\n";
                    } else {
                        std::cout << "  Failed to download result\n";
                    }
                } else {
                    std::cout << "  Operation failed\n";
                }

            } catch (const std::exception& e) {
                std::cout << "  Error: " << e.what() << "\n";
            }
        }

        // Get performance statistics
        auto stats = gpuProcessor.getPerformanceStats();
        if (!stats.empty()) {
            std::cout << "\nGPU Performance Statistics:\n";
            for (const auto& [key, value] : stats) {
                std::cout << "  " << key << ": " << std::fixed
                          << std::setprecision(2) << value << "\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in GPU processing: " << e.what() << "\n";
    }
}

/**
 * @brief Compare GPU vs CPU performance
 */
void compareGPUvsCPU() {
    std::cout << "\n=== GPU vs CPU Performance Comparison ===\n";

    try {
        // Create test images of different sizes
        std::vector<std::pair<int, int>> imageSizes = {
            {512, 384},    // Small
            {1024, 768},   // Medium
            {2048, 1536},  // Large
            {4096, 3072}   // Very Large
        };

        // Initialize processors
        GPUImageProcessor gpuProcessor;
        bool gpuAvailable = gpuProcessor.initialize();

        ImageProcessor cpuProcessor;

        for (const auto& [width, height] : imageSizes) {
            std::cout << "\nTesting " << width << "x" << height << " image:\n";

            auto testImage = createTestImage(width, height, 3);

            // Test Gaussian blur operation
            const int iterations = 5;

            // CPU processing
            auto start = high_resolution_clock::now();
            for (int i = 0; i < iterations; ++i) {
                auto result = cpuProcessor.applyFilter(
                    testImage, FilterType::GAUSSIAN_BLUR,
                    {{"sigma", 2.0}, {"kernel_size", 7}});
            }
            auto cpuTime = duration_cast<milliseconds>(
                high_resolution_clock::now() - start);
            double avgCpuTime =
                static_cast<double>(cpuTime.count()) / iterations;

            std::cout << "  CPU average time: " << std::fixed
                      << std::setprecision(2) << avgCpuTime << " ms\n";

            // GPU processing (if available)
            if (gpuAvailable) {
                try {
                    auto gpuBuffer = gpuProcessor.uploadImage(testImage);
                    if (gpuBuffer) {
                        start = high_resolution_clock::now();
                        for (int i = 0; i < iterations; ++i) {
                            auto result = gpuProcessor.gaussianBlur(
                                *gpuBuffer, 2.0f, 7, width, height, 3);
                        }
                        auto gpuTime = duration_cast<milliseconds>(
                            high_resolution_clock::now() - start);
                        double avgGpuTime =
                            static_cast<double>(gpuTime.count()) / iterations;

                        std::cout << "  GPU average time: " << avgGpuTime
                                  << " ms\n";

                        if (avgGpuTime > 0) {
                            double speedup = avgCpuTime / avgGpuTime;
                            std::cout << "  GPU speedup: " << speedup << "x\n";
                        }
                    } else {
                        std::cout << "  GPU: Failed to upload image\n";
                    }
                } catch (const std::exception& e) {
                    std::cout << "  GPU error: " << e.what() << "\n";
                }
            } else {
                std::cout << "  GPU: Not available\n";
            }

            // Calculate throughput
            size_t pixels = width * height;
            double cpuThroughput = (pixels * iterations) /
                                   (cpuTime.count() / 1000.0);  // pixels/second
            std::cout << "  CPU throughput: " << (cpuThroughput / 1000000.0)
                      << " Mpixels/s\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in performance comparison: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate fallback mechanisms
 */
void demonstrateFallbackMechanisms() {
    std::cout << "\n=== Fallback Mechanisms ===\n";

    try {
        std::cout << "Testing graceful fallback when GPU is unavailable:\n";

        // Create a processing function that tries GPU first, then falls back to
        // CPU
        auto processImageWithFallback =
            [](const blob& input, const std::string& operation) -> blob {
            // Try GPU first
            GPUImageProcessor gpuProcessor;
            if (gpuProcessor.initialize()) {
                std::cout << "  Attempting GPU processing...\n";

                try {
                    auto gpuBuffer = gpuProcessor.uploadImage(input);
                    if (gpuBuffer) {
                        std::unique_ptr<GPUBuffer> result;

                        if (operation == "blur") {
                            result = gpuProcessor.gaussianBlur(
                                *gpuBuffer, 2.0f, 7, input.getWidth(),
                                input.getHeight(), 3);
                        }

                        if (result) {
                            std::vector<uint8_t> resultData(result->getSize());
                            if (result->download(resultData.data(),
                                                 resultData.size())) {
                                std::cout << "  GPU processing successful\n";
                                return blob(resultData.data(),
                                            resultData.size());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cout << "  GPU processing failed: " << e.what()
                              << "\n";
                }
            }

            // Fallback to CPU
            std::cout << "  Falling back to CPU processing...\n";
            ImageProcessor cpuProcessor;

            if (operation == "blur") {
                return cpuProcessor.applyFilter(
                    input, FilterType::GAUSSIAN_BLUR,
                    {{"sigma", 2.0}, {"kernel_size", 7}});
            }

            return input;  // Return original if no processing possible
        };

        // Test the fallback mechanism
        auto testImage = createTestImage(512, 384, 3);

        auto start = high_resolution_clock::now();
        auto result = processImageWithFallback(testImage, "blur");
        auto totalTime =
            duration_cast<milliseconds>(high_resolution_clock::now() - start);

        std::cout << "  Total processing time: " << totalTime.count()
                  << " ms\n";
        std::cout << "  Result size: " << result.size() << " bytes\n";
        std::cout << "  Fallback mechanism completed successfully\n";

        // Test error recovery
        std::cout << "\nTesting error recovery:\n";

        try {
            // Simulate GPU error by using invalid parameters
            GPUImageProcessor errorProcessor;
            if (errorProcessor.initialize()) {
                auto buffer = errorProcessor.uploadImage(testImage);
                if (buffer) {
                    // This should fail gracefully
                    auto badResult =
                        errorProcessor.gaussianBlur(*buffer, -1.0f, 0, 0, 0, 0);
                    if (!badResult) {
                        std::cout << "  GPU error handled gracefully\n";
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cout << "  Caught and handled GPU error: " << e.what() << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in fallback demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate batch processing on GPU
 */
void demonstrateBatchProcessing() {
    std::cout << "\n=== GPU Batch Processing ===\n";

    try {
        GPUImageProcessor gpuProcessor;
        if (!gpuProcessor.initialize()) {
            std::cout << "GPU not available for batch processing\n";
            return;
        }

        // Create multiple test images
        std::vector<blob> inputImages;
        const int batchSize = 5;

        for (int i = 0; i < batchSize; ++i) {
            auto img = createTestImage(512, 384, 3);
            inputImages.push_back(img);
        }

        std::cout << "Created batch of " << batchSize << " images\n";

        // Upload all images to GPU
        std::vector<std::unique_ptr<GPUBuffer>> gpuBuffers;

        auto start = high_resolution_clock::now();
        for (const auto& img : inputImages) {
            auto buffer = gpuProcessor.uploadImage(img);
            if (buffer) {
                gpuBuffers.push_back(std::move(buffer));
            }
        }
        auto uploadTime =
            duration_cast<milliseconds>(high_resolution_clock::now() - start);

        std::cout << "Uploaded " << gpuBuffers.size() << " images in "
                  << uploadTime.count() << " ms\n";

        // Process batch
        start = high_resolution_clock::now();

        std::vector<std::unique_ptr<GPUBuffer>> results =
            gpuProcessor.batchProcess(gpuBuffers, "gaussian_blur",
                                      {{"sigma", 2.0f}, {"kernel_size", 7}});

        auto processingTime =
            duration_cast<milliseconds>(high_resolution_clock::now() - start);

        std::cout << "Processed " << results.size() << " images in "
                  << processingTime.count() << " ms\n";
        std::cout << "Average processing time per image: "
                  << (static_cast<double>(processingTime.count()) /
                      results.size())
                  << " ms\n";

        // Download results
        start = high_resolution_clock::now();
        std::vector<blob> outputImages;

        for (auto& result : results) {
            if (result) {
                std::vector<uint8_t> data(result->getSize());
                if (result->download(data.data(), data.size())) {
                    outputImages.emplace_back(data.data(), data.size());
                }
            }
        }

        auto downloadTime =
            duration_cast<milliseconds>(high_resolution_clock::now() - start);

        std::cout << "Downloaded " << outputImages.size() << " results in "
                  << downloadTime.count() << " ms\n";
        std::cout << "Total batch processing time: "
                  << (uploadTime.count() + processingTime.count() +
                      downloadTime.count())
                  << " ms\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in batch processing: " << e.what() << "\n";
    }
}

int main() {
    std::cout << "=== Atom Image GPU Acceleration Demo ===\n";
    std::cout << "This example demonstrates GPU-accelerated image processing "
                 "with fallbacks\n";

    // Run all demonstrations
    demonstrateGPUDetection();
    demonstrateGPUProcessing();
    compareGPUvsCPU();
    demonstrateFallbackMechanisms();
    demonstrateBatchProcessing();

    std::cout << "\n=== GPU acceleration demo completed ===\n";
    std::cout << "\nKey features demonstrated:\n";
    std::cout << "- GPU device detection and capability assessment\n";
    std::cout << "- GPU-accelerated image processing operations\n";
    std::cout << "- Performance comparison between GPU and CPU\n";
    std::cout << "- Graceful fallback mechanisms for systems without GPU\n";
    std::cout << "- Batch processing optimization for multiple images\n";
    std::cout << "- Error handling and recovery mechanisms\n";

    return 0;
}
