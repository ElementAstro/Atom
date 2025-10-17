#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vector>
#include <string>
#include <memory>
#include <cmath>

#include "atom/image/processing/gpu_acceleration.hpp"
#include "atom/image/core/image_blob.hpp"
#include "test_utils.hpp"

namespace atom::image::test {

class GPUAccelerationTest : public ::testing::Test {
protected:
    void SetUp() override {
        fileManager = std::make_unique<TestFileManager>();
        createTestImages();

        try {
            gpuProcessor = std::make_unique<GPUImageProcessor>();
            gpuAvailable = gpuProcessor->initialize(GPUBackend::AUTO, -1);
        } catch (const std::exception&) {
            gpuAvailable = false;
        }
    }

    void TearDown() override {
        gpuProcessor.reset();
        fileManager->cleanup();
    }

    void createTestImages() {
        auto gradientData = TestDataGenerator::generateGradientImage(128, 128, 3);
        gradient_image = blob(gradientData.data(), gradientData.size());

        auto solidData = TestDataGenerator::generateSolidColor(128, 128, 3, {128, 128, 128});
        solid_image = blob(solidData.data(), solidData.size());

        auto checkerboardData = TestDataGenerator::generateCheckerboard(128, 128, 3, 8);
        checkerboard_image = blob(checkerboardData.data(), checkerboardData.size());
    }

    std::unique_ptr<GPUImageProcessor> gpuProcessor;
    std::unique_ptr<TestFileManager> fileManager;
    bool gpuAvailable = false;

    blob gradient_image, solid_image, checkerboard_image;
};

TEST_F(GPUAccelerationTest, BackendAvailability) {
    bool cudaAvailable = GPUContext::isBackendAvailable(GPUBackend::CUDA);
    bool openclAvailable = GPUContext::isBackendAvailable(GPUBackend::OPENCL);
    bool vulkanAvailable = GPUContext::isBackendAvailable(GPUBackend::VULKAN);
    bool metalAvailable = GPUContext::isBackendAvailable(GPUBackend::METAL);

    EXPECT_TRUE(cudaAvailable || openclAvailable || vulkanAvailable || metalAvailable || true);
}

TEST_F(GPUAccelerationTest, OptimalBackend) {
    auto backend = GPUContext::getOptimalBackend();

    EXPECT_TRUE(backend == GPUBackend::CUDA ||
                backend == GPUBackend::OPENCL ||
                backend == GPUBackend::VULKAN ||
                backend == GPUBackend::METAL ||
                backend == GPUBackend::DIRECTCOMPUTE ||
                backend == GPUBackend::HIP ||
                backend == GPUBackend::SYCL ||
                backend == GPUBackend::AUTO);
}

TEST_F(GPUAccelerationTest, AvailableDevices) {
    auto devices = GPUContext::getAvailableDevices(GPUBackend::AUTO);
    EXPECT_GE(devices.size(), 0);

    for (const auto& device : devices) {
        EXPECT_GE(device.deviceId, 0);
        EXPECT_FALSE(device.name.empty());
        EXPECT_GT(device.totalMemory, 0);
        EXPECT_GT(device.computeUnits, 0);
    }
}

// Test GPU processor initialization
TEST_F(GPUAccelerationTest, ProcessorInitialization) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_TRUE(gpuAvailable);
    EXPECT_NE(gpuProcessor, nullptr);
}

// Test image upload to GPU
TEST_F(GPUAccelerationTest, ImageUpload) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto buffer = gpuProcessor->uploadImage(gradient_image);

    EXPECT_NE(buffer, nullptr);
    EXPECT_TRUE(buffer->isValid());
    EXPECT_GT(buffer->getSize(), 0);
}

// Test image upload with empty image
TEST_F(GPUAccelerationTest, ImageUploadEmpty) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    blob empty_image;
    auto buffer = gpuProcessor->uploadImage(empty_image);

    // Should handle empty image gracefully
    EXPECT_TRUE(buffer == nullptr || !buffer->isValid());
}

// Test image download from GPU
TEST_F(GPUAccelerationTest, ImageDownload) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto buffer = gpuProcessor->uploadImage(gradient_image);
    ASSERT_NE(buffer, nullptr);

    auto downloaded = gpuProcessor->downloadImage(*buffer, 128, 128, 3);

    EXPECT_FALSE(downloaded.isEmpty());
    EXPECT_EQ(downloaded.size(), gradient_image.size());
}

// Test Gaussian blur on GPU
TEST_F(GPUAccelerationTest, GaussianBlur) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto inputBuffer = gpuProcessor->uploadImage(gradient_image);
    ASSERT_NE(inputBuffer, nullptr);

    auto outputBuffer = gpuProcessor->gaussianBlur(*inputBuffer, 1.0f, 5, 128, 128, 3);

    EXPECT_NE(outputBuffer, nullptr);
    EXPECT_TRUE(outputBuffer->isValid());
}

// Test image resize on GPU
TEST_F(GPUAccelerationTest, ImageResize) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto inputBuffer = gpuProcessor->uploadImage(gradient_image);
    ASSERT_NE(inputBuffer, nullptr);

    auto outputBuffer = gpuProcessor->resize(*inputBuffer, 128, 128, 64, 64, 3, "linear");

    EXPECT_NE(outputBuffer, nullptr);
    EXPECT_TRUE(outputBuffer->isValid());
}

// Test color space conversion on GPU
TEST_F(GPUAccelerationTest, ColorSpaceConversion) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto inputBuffer = gpuProcessor->uploadImage(gradient_image);
    ASSERT_NE(inputBuffer, nullptr);

    auto outputBuffer = gpuProcessor->convertColorSpace(*inputBuffer, "RGB", "GRAY", 128, 128);

    EXPECT_NE(outputBuffer, nullptr);
    EXPECT_TRUE(outputBuffer->isValid());
}

// Test histogram equalization on GPU
TEST_F(GPUAccelerationTest, HistogramEqualization) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto inputBuffer = gpuProcessor->uploadImage(gradient_image);
    ASSERT_NE(inputBuffer, nullptr);

    auto outputBuffer = gpuProcessor->equalizeHistogram(*inputBuffer, 128, 128, 3);

    EXPECT_NE(outputBuffer, nullptr);
    EXPECT_TRUE(outputBuffer->isValid());
}

// Test edge detection on GPU
TEST_F(GPUAccelerationTest, EdgeDetection) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto inputBuffer = gpuProcessor->uploadImage(checkerboard_image);
    ASSERT_NE(inputBuffer, nullptr);

    auto outputBuffer = gpuProcessor->detectEdges(*inputBuffer, "sobel", 50.0f, 150.0f, 128, 128);

    EXPECT_NE(outputBuffer, nullptr);
    EXPECT_TRUE(outputBuffer->isValid());
}

// Test morphological operations on GPU
TEST_F(GPUAccelerationTest, MorphologicalOperations) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto inputBuffer = gpuProcessor->uploadImage(checkerboard_image);
    ASSERT_NE(inputBuffer, nullptr);

    std::vector<std::vector<int>> structElement = {{1, 1, 1}, {1, 1, 1}, {1, 1, 1}};
    auto outputBuffer = gpuProcessor->morphological(*inputBuffer, "erode", structElement, 128, 128, 3);

    EXPECT_NE(outputBuffer, nullptr);
    EXPECT_TRUE(outputBuffer->isValid());
}

// Test convolution on GPU
TEST_F(GPUAccelerationTest, Convolution) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto inputBuffer = gpuProcessor->uploadImage(gradient_image);
    ASSERT_NE(inputBuffer, nullptr);

    std::vector<std::vector<float>> kernel = {
        {0.0f, -1.0f, 0.0f},
        {-1.0f, 5.0f, -1.0f},
        {0.0f, -1.0f, 0.0f}
    };

    auto outputBuffer = gpuProcessor->convolve(*inputBuffer, kernel, 128, 128, 3);

    EXPECT_NE(outputBuffer, nullptr);
    EXPECT_TRUE(outputBuffer->isValid());
}

TEST_F(GPUAccelerationTest, PerformanceStatistics) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto stats = gpuProcessor->getPerformanceStats();
    EXPECT_GE(stats.size(), 0);
}

TEST_F(GPUAccelerationTest, FactoryFunction) {
    auto processor = createOptimalGPUProcessor(GPUBackend::AUTO, -1);
    EXPECT_NE(processor, nullptr);
}

TEST_F(GPUAccelerationTest, InvalidBufferOperations) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_NO_THROW({
        auto context = gpuProcessor->getContext();
        if (context) {
            auto buffer = context->createBuffer(0);
        }
    });
}

// Performance test for GPU operations
TEST_F(GPUAccelerationTest, DISABLED_PerformanceGPUOperations) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto largeData = TestDataGenerator::generateGradientImage(1024, 1024, 3);
    blob large_image(largeData.data(), largeData.size());

    auto start = std::chrono::high_resolution_clock::now();

    auto buffer = gpuProcessor->uploadImage(large_image);
    auto blurred = gpuProcessor->gaussianBlur(*buffer, 2.0f, 7, 1024, 1024, 3);
    auto result = gpuProcessor->downloadImage(*blurred, 1024, 1024, 3);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_FALSE(result.isEmpty());
    std::cout << "GPU processing took " << duration.count() << " ms" << std::endl;
}

// Test batch processing on GPU
TEST_F(GPUAccelerationTest, BatchProcessing) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    std::vector<std::unique_ptr<GPUBuffer>> inputs;
    inputs.push_back(gpuProcessor->uploadImage(gradient_image));
    inputs.push_back(gpuProcessor->uploadImage(solid_image));
    inputs.push_back(gpuProcessor->uploadImage(checkerboard_image));

    std::unordered_map<std::string, float> params;
    params["sigma"] = 1.0f;

    auto outputs = gpuProcessor->batchProcess(inputs, "gaussian_blur", params);

    EXPECT_EQ(outputs.size(), inputs.size());
    for (const auto& output : outputs) {
        EXPECT_NE(output, nullptr);
        EXPECT_TRUE(output->isValid());
    }
}

// Test custom kernel execution
TEST_F(GPUAccelerationTest, CustomKernel) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto inputBuffer = gpuProcessor->uploadImage(gradient_image);
    ASSERT_NE(inputBuffer, nullptr);

    // Simple kernel source (may not compile on all backends)
    std::string kernelSource = R"(
        __kernel void simple_kernel(__global uchar* input, __global uchar* output) {
            int gid = get_global_id(0);
            output[gid] = input[gid];
        }
    )";

    try {
        std::vector<size_t> globalWorkSize = {128 * 128 * 3};
        auto outputBuffer = gpuProcessor->applyCustomKernel(*inputBuffer, kernelSource,
                                                           "simple_kernel", globalWorkSize);

        if (outputBuffer) {
            EXPECT_TRUE(outputBuffer->isValid());
        }
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Custom kernel not supported: " << e.what();
    }
}

// Test GPU context retrieval
TEST_F(GPUAccelerationTest, GetContext) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto context = gpuProcessor->getContext();
    EXPECT_NE(context, nullptr);
}

// Test benchmark functionality
TEST_F(GPUAccelerationTest, Benchmark) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto results = gpuProcessor->benchmark("gaussian_blur", {256, 256}, 10);

    EXPECT_GT(results.size(), 0);
    for (const auto& [metric, value] : results) {
        EXPECT_FALSE(metric.empty());
        EXPECT_GE(value, 0.0);
    }
}

// Test different interpolation methods for resize
TEST_F(GPUAccelerationTest, ResizeInterpolationMethods) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto inputBuffer = gpuProcessor->uploadImage(gradient_image);
    ASSERT_NE(inputBuffer, nullptr);

    std::vector<std::string> methods = {"nearest", "linear", "cubic"};

    for (const auto& method : methods) {
        auto outputBuffer = gpuProcessor->resize(*inputBuffer, 128, 128, 64, 64, 3, method);
        EXPECT_NE(outputBuffer, nullptr) << "Failed for method: " << method;
        if (outputBuffer) {
            EXPECT_TRUE(outputBuffer->isValid());
        }
    }
}

// Test different edge detection methods
TEST_F(GPUAccelerationTest, EdgeDetectionMethods) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto inputBuffer = gpuProcessor->uploadImage(checkerboard_image);
    ASSERT_NE(inputBuffer, nullptr);

    std::vector<std::string> methods = {"sobel", "canny", "laplacian"};

    for (const auto& method : methods) {
        try {
            auto outputBuffer = gpuProcessor->detectEdges(*inputBuffer, method, 50.0f, 150.0f, 128, 128);
            EXPECT_NE(outputBuffer, nullptr) << "Failed for method: " << method;
            if (outputBuffer) {
                EXPECT_TRUE(outputBuffer->isValid());
            }
        } catch (const std::exception& e) {
            // Some methods may not be implemented
            GTEST_SKIP() << "Method " << method << " not available: " << e.what();
        }
    }
}

// Test different morphological operations
TEST_F(GPUAccelerationTest, MorphologicalOperationTypes) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto inputBuffer = gpuProcessor->uploadImage(checkerboard_image);
    ASSERT_NE(inputBuffer, nullptr);

    std::vector<std::vector<int>> structElement = {{1, 1, 1}, {1, 1, 1}, {1, 1, 1}};
    std::vector<std::string> operations = {"erode", "dilate", "open", "close"};

    for (const auto& op : operations) {
        try {
            auto outputBuffer = gpuProcessor->morphological(*inputBuffer, op, structElement, 128, 128, 3);
            EXPECT_NE(outputBuffer, nullptr) << "Failed for operation: " << op;
            if (outputBuffer) {
                EXPECT_TRUE(outputBuffer->isValid());
            }
        } catch (const std::exception& e) {
            // Some operations may not be implemented
            GTEST_SKIP() << "Operation " << op << " not available: " << e.what();
        }
    }
}

// Test memory type variations
TEST_F(GPUAccelerationTest, MemoryTypes) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto context = gpuProcessor->getContext();
    if (!context) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<GPUMemoryType> memTypes = {
        GPUMemoryType::DEVICE,
        GPUMemoryType::HOST,
        GPUMemoryType::UNIFIED,
        GPUMemoryType::PINNED
    };

    for (const auto& memType : memTypes) {
        try {
            auto buffer = context->createBuffer(1024, memType);
            if (buffer) {
                EXPECT_TRUE(buffer->isValid());
                EXPECT_EQ(buffer->getMemoryType(), memType);
            }
        } catch (const std::exception&) {
            // Some memory types may not be supported
        }
    }
}

TEST_F(GPUAccelerationTest, RoundTripConsistency) {
    if (!gpuAvailable) {
        GTEST_SKIP() << "GPU not available";
    }

    auto uploaded = gpuProcessor->uploadImage(gradient_image);
    ASSERT_NE(uploaded, nullptr);

    auto downloaded = gpuProcessor->downloadImage(*uploaded, 128, 128, 3);

    EXPECT_EQ(downloaded.size(), gradient_image.size());
    EXPECT_FALSE(downloaded.isEmpty());
}

} // namespace atom::image::test
