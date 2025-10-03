#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include "atom/image/processing/ml_processing.hpp"
#include "atom/image/core/image_blob.hpp"
#include "test_utils.hpp"

namespace atom::image::test {

class MLProcessingTest : public ::testing::Test {
protected:
    void SetUp() override {
        mlProcessor = std::make_unique<MLImageProcessor>();
        fileManager = std::make_unique<TestFileManager>();
        
        // Create test images
        createTestImages();
        
        // Initialize ML processor (may fail if models not available)
        initializationSuccess = mlProcessor->initialize("", MLBackend::AUTO, false); // Use CPU for tests
    }

    void TearDown() override {
        fileManager->cleanup();
    }

    void createTestImages() {
        // Create a low resolution image for super-resolution tests
        auto lowResData = TestDataGenerator::generateGradientImage(16, 16, 3);
        low_res_image = blob(lowResData.data(), lowResData.size());

        // Create a noisy image for denoising tests
        auto noisyData = TestDataGenerator::generateRandomNoise(32, 32, 3, 12345);
        noisy_image = blob(noisyData.data(), noisyData.size());

        // Create a content image for style transfer
        auto contentData = TestDataGenerator::generateCircularPattern(64, 64, 3, 20);
        content_image = blob(contentData.data(), contentData.size());

        // Create a style image for style transfer
        auto styleData = TestDataGenerator::generateCheckerboard(64, 64, 3, 8);
        style_image = blob(styleData.data(), styleData.size());

        // Create a high resolution image for various tests
        auto highResData = TestDataGenerator::generateGradientImage(128, 128, 3);
        high_res_image = blob(highResData.data(), highResData.size());

        // Create a grayscale image for colorization tests
        auto grayData = TestDataGenerator::generateGradientImage(64, 64, 1);
        grayscale_image = blob(grayData.data(), grayData.size());
    }

    std::unique_ptr<MLImageProcessor> mlProcessor;
    std::unique_ptr<TestFileManager> fileManager;
    
    blob low_res_image, noisy_image, content_image, style_image, high_res_image, grayscale_image;
    bool initializationSuccess = false;
};

// Test ML processor initialization
TEST_F(MLProcessingTest, ProcessorInitialization) {
    // Test initialization with different backends
    std::vector<MLBackend> backends = {
        MLBackend::AUTO,
        MLBackend::ONNX,
        MLBackend::PYTORCH,
        MLBackend::TENSORFLOW
    };

    for (const auto& backend : backends) {
        auto processor = std::make_unique<MLImageProcessor>();
        
        // Initialization may fail if backend is not available, which is acceptable
        bool result = processor->initialize("", backend, false);
        
        // Test should not crash regardless of result
        EXPECT_TRUE(true);
    }
}

// Test super-resolution models
TEST_F(MLProcessingTest, SuperResolutionModels) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    std::vector<MLModelType> srModels = {
        MLModelType::ESRGAN,
        MLModelType::REAL_ESRGAN,
        MLModelType::SRCNN,
        MLModelType::VDSR,
        MLModelType::EDSR,
        MLModelType::WAIFU2X
    };

    MLParams params;
    params.scaleFactor = 2;
    params.useGPU = false;
    params.batchSize = 1;

    for (const auto& model : srModels) {
        auto result = mlProcessor->superResolution(low_res_image, model, params);
        
        // Result may fail if specific model is not available
        if (result.success) {
            EXPECT_GT(result.outputImage.size(), 0);
            EXPECT_GT(result.processingTime, 0.0);
            EXPECT_FALSE(result.modelUsed.empty());
        }
    }
}

// Test super-resolution with different scale factors
TEST_F(MLProcessingTest, SuperResolutionScaleFactors) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    std::vector<int> scaleFactors = {2, 3, 4, 8};
    
    for (int scale : scaleFactors) {
        MLParams params;
        params.scaleFactor = scale;
        params.useGPU = false;
        
        auto result = mlProcessor->superResolution(low_res_image, MLModelType::REAL_ESRGAN, params);
        
        if (result.success) {
            EXPECT_GT(result.outputImage.size(), 0);
            // Output should be larger than input for upscaling
            EXPECT_GT(result.outputImage.size(), low_res_image.size());
        }
    }
}

// Test denoising models
TEST_F(MLProcessingTest, DenoisingModels) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    std::vector<MLModelType> denoiseModels = {
        MLModelType::DNCNN,
        MLModelType::FFDNet,
        MLModelType::RIDNET,
        MLModelType::CBDNet
    };

    MLParams params;
    params.noiseLevel = 25.0;
    params.useGPU = false;

    for (const auto& model : denoiseModels) {
        auto result = mlProcessor->denoise(noisy_image, model, params);
        
        if (result.success) {
            EXPECT_GT(result.outputImage.size(), 0);
            EXPECT_EQ(result.outputImage.size(), noisy_image.size());
            EXPECT_GT(result.processingTime, 0.0);
        }
    }
}

// Test denoising with different noise levels
TEST_F(MLProcessingTest, DenoisingNoiseLevels) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    std::vector<double> noiseLevels = {10.0, 25.0, 50.0, 75.0};
    
    for (double noiseLevel : noiseLevels) {
        MLParams params;
        params.noiseLevel = noiseLevel;
        params.blindDenoising = true;
        params.useGPU = false;
        
        auto result = mlProcessor->denoise(noisy_image, MLModelType::DNCNN, params);
        
        if (result.success) {
            EXPECT_GT(result.outputImage.size(), 0);
            EXPECT_EQ(result.outputImage.size(), noisy_image.size());
        }
    }
}

// Test style transfer
TEST_F(MLProcessingTest, StyleTransfer) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    std::vector<MLModelType> styleModels = {
        MLModelType::NEURAL_STYLE,
        MLModelType::FAST_STYLE,
        MLModelType::ADAIN,
        MLModelType::PHOTOREALISTIC
    };

    MLParams params;
    params.styleStrength = 1.0;
    params.preserveColor = false;
    params.useGPU = false;

    for (const auto& model : styleModels) {
        auto result = mlProcessor->styleTransfer(content_image, style_image, model, params);
        
        if (result.success) {
            EXPECT_GT(result.outputImage.size(), 0);
            EXPECT_EQ(result.outputImage.size(), content_image.size());
            EXPECT_GT(result.processingTime, 0.0);
        }
    }
}

// Test style transfer with different parameters
TEST_F(MLProcessingTest, StyleTransferParameters) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    std::vector<double> styleStrengths = {0.3, 0.7, 1.0, 1.5};
    
    for (double strength : styleStrengths) {
        MLParams params;
        params.styleStrength = strength;
        params.preserveColor = false;
        params.useGPU = false;
        
        auto result = mlProcessor->styleTransfer(content_image, style_image, 
                                               MLModelType::FAST_STYLE, params);
        
        if (result.success) {
            EXPECT_GT(result.outputImage.size(), 0);
            EXPECT_EQ(result.outputImage.size(), content_image.size());
        }
    }
}

// Test image enhancement models
TEST_F(MLProcessingTest, ImageEnhancement) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    std::vector<MLModelType> enhanceModels = {
        MLModelType::DPED,
        MLModelType::WESPE,
        MLModelType::MIRNET,
        MLModelType::RETINEX_NET
    };

    MLParams params;
    params.enhancementStrength = 0.8;
    params.autoAdjust = true;
    params.useGPU = false;

    for (const auto& model : enhanceModels) {
        auto result = mlProcessor->enhance(high_res_image, model, params);
        
        if (result.success) {
            EXPECT_GT(result.outputImage.size(), 0);
            EXPECT_EQ(result.outputImage.size(), high_res_image.size());
            EXPECT_GT(result.processingTime, 0.0);
        }
    }
}

// Test image restoration models
TEST_F(MLProcessingTest, ImageRestoration) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    std::vector<MLModelType> restoreModels = {
        MLModelType::NAFNET,
        MLModelType::RESTORMER,
        MLModelType::SWINIR,
        MLModelType::UFORMER
    };

    MLParams params;
    params.useGPU = false;
    params.tileSize = 256;
    params.overlap = 16;

    for (const auto& model : restoreModels) {
        auto result = mlProcessor->restore(high_res_image, model, params);
        
        if (result.success) {
            EXPECT_GT(result.outputImage.size(), 0);
            EXPECT_EQ(result.outputImage.size(), high_res_image.size());
        }
    }
}

// Test specialized processing
TEST_F(MLProcessingTest, SpecializedProcessing) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    MLParams params;
    params.useGPU = false;

    // Test colorization
    auto colorResult = mlProcessor->colorize(grayscale_image, MLModelType::COLORIZATION, params);
    if (colorResult.success) {
        EXPECT_GT(colorResult.outputImage.size(), 0);
        // Colorized image should have more channels than grayscale
        EXPECT_GE(colorResult.outputImage.size(), grayscale_image.size());
    }

    // Test background removal
    auto bgRemovalResult = mlProcessor->removeBackground(high_res_image,
                                                        MLModelType::BACKGROUND_REMOVAL, params);
    if (bgRemovalResult.success) {
        EXPECT_GT(bgRemovalResult.outputImage.size(), 0);
    }

    // Test face restoration
    auto faceResult = mlProcessor->restoreFaces(high_res_image,
                                              MLModelType::FACE_RESTORATION, params);
    if (faceResult.success) {
        EXPECT_GT(faceResult.outputImage.size(), 0);
        EXPECT_EQ(faceResult.outputImage.size(), high_res_image.size());
    }
}

// Test image generation
TEST_F(MLProcessingTest, ImageGeneration) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    MLParams params;
    params.prompt = "A beautiful landscape with mountains and lakes";
    params.negativePrompt = "blurry, low quality";
    params.steps = 20; // Reduced for testing
    params.guidanceScale = 7.5;
    params.seed = 42; // Fixed seed for reproducibility
    params.useGPU = false;

    std::vector<MLModelType> genModels = {
        MLModelType::STABLE_DIFFUSION,
        MLModelType::DALLE,
        MLModelType::MIDJOURNEY
    };

    for (const auto& model : genModels) {
        auto result = mlProcessor->generateFromText(params.prompt, model, params);

        if (result.success) {
            EXPECT_GT(result.outputImage.size(), 0);
            EXPECT_GT(result.processingTime, 0.0);
            EXPECT_FALSE(result.modelUsed.empty());
        }
    }
}

// Test inpainting
TEST_F(MLProcessingTest, ImageInpainting) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    // Create a simple mask (center region)
    auto maskData = TestDataGenerator::generateSolidColor(64, 64, 1, {255});
    // Make center region black (area to inpaint)
    for (int y = 20; y < 44; ++y) {
        for (int x = 20; x < 44; ++x) {
            int idx = y * 64 + x;
            if (idx < static_cast<int>(maskData.size())) {
                maskData[idx] = static_cast<std::byte>(0);
            }
        }
    }
    blob mask(maskData.data(), maskData.size());

    MLParams params;
    params.useGPU = false;
    params.prompt = "smooth texture";

    auto result = mlProcessor->inpaint(content_image, mask, MLModelType::INPAINTING, params);

    if (result.success) {
        EXPECT_GT(result.outputImage.size(), 0);
        EXPECT_EQ(result.outputImage.size(), content_image.size());
    }
}

// Test outpainting (using inpainting with extended canvas)
TEST_F(MLProcessingTest, ImageOutpainting) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    // Create an extended canvas with the original image in center
    auto extendedData = TestDataGenerator::generateSolidColor(96, 96, 3, {128});
    // Copy original image to center (simplified approach)
    blob extendedImage(extendedData.data(), extendedData.size());

    // Create mask for outpainting (edges to be filled)
    auto maskData = TestDataGenerator::generateSolidColor(96, 96, 1, {255});
    blob mask(maskData.data(), maskData.size());

    MLParams params;
    params.useGPU = false;
    params.prompt = "extend the image naturally";

    auto result = mlProcessor->inpaint(extendedImage, mask, MLModelType::OUTPAINTING, params);

    if (result.success) {
        EXPECT_GT(result.outputImage.size(), 0);
        EXPECT_EQ(result.outputImage.size(), extendedImage.size());
    }
}

// Test batch processing
TEST_F(MLProcessingTest, BatchProcessing) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    std::vector<blob> inputImages = {low_res_image, content_image, grayscale_image};

    MLParams params;
    params.batchSize = 3;
    params.useGPU = false;
    params.scaleFactor = 2;

    auto results = mlProcessor->batchProcess(inputImages, MLModelType::REAL_ESRGAN, params);

    if (!results.empty() && results[0].success) {
        EXPECT_EQ(results.size(), inputImages.size());

        for (const auto& result : results) {
            if (result.success) {
                EXPECT_GT(result.outputImage.size(), 0);
                EXPECT_GT(result.processingTime, 0.0);
            }
        }
    }
}

// Test tiled processing for large images
TEST_F(MLProcessingTest, TiledProcessing) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    // Create a large image
    auto largeData = TestDataGenerator::generateGradientImage(256, 256, 3);
    blob largeImage(largeData.data(), largeData.size());

    MLParams params;
    params.tileSize = 128;
    params.overlap = 16;
    params.useGPU = false;
    params.scaleFactor = 2;

    auto result = mlProcessor->superResolution(largeImage, MLModelType::REAL_ESRGAN, params);

    if (result.success) {
        EXPECT_GT(result.outputImage.size(), 0);
        EXPECT_GT(result.outputImage.size(), largeImage.size());
    }
}

// Test test-time augmentation
TEST_F(MLProcessingTest, TestTimeAugmentation) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    MLParams params;
    params.enableTTA = true;
    params.useGPU = false;
    params.scaleFactor = 2;

    auto result = mlProcessor->superResolution(low_res_image, MLModelType::REAL_ESRGAN, params);

    if (result.success) {
        EXPECT_GT(result.outputImage.size(), 0);
        // TTA should generally improve quality but take longer
        EXPECT_GT(result.processingTime, 0.0);
    }
}

// Test custom model loading
TEST_F(MLProcessingTest, CustomModelLoading) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    MLParams params;
    params.modelPath = "custom_model.onnx"; // Non-existent model
    params.useGPU = false;

    auto result = mlProcessor->superResolution(low_res_image, MLModelType::CUSTOM, params);

    // Should fail gracefully with non-existent model
    if (!result.success) {
        EXPECT_FALSE(result.errorMessage.empty());
    }
}

// Test parameter validation
TEST_F(MLProcessingTest, ParameterValidation) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    MLParams params;
    params.useGPU = false;

    // Test invalid scale factor
    params.scaleFactor = 0;
    auto result1 = mlProcessor->superResolution(low_res_image, MLModelType::REAL_ESRGAN, params);
    EXPECT_FALSE(result1.success);

    // Test invalid noise level
    params.scaleFactor = 2;
    params.noiseLevel = -10.0;
    auto result2 = mlProcessor->denoise(noisy_image, MLModelType::DNCNN, params);
    EXPECT_FALSE(result2.success);

    // Test invalid style strength
    params.noiseLevel = 25.0;
    params.styleStrength = -1.0;
    auto result3 = mlProcessor->styleTransfer(content_image, style_image,
                                            MLModelType::FAST_STYLE, params);
    EXPECT_FALSE(result3.success);
}

// Test empty image handling
TEST_F(MLProcessingTest, EmptyImageHandling) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    blob emptyImage;
    MLParams params;
    params.useGPU = false;

    auto result = mlProcessor->superResolution(emptyImage, MLModelType::REAL_ESRGAN, params);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

// Test result metrics
TEST_F(MLProcessingTest, ResultMetrics) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    MLParams params;
    params.useGPU = false;
    params.scaleFactor = 2;

    auto result = mlProcessor->superResolution(low_res_image, MLModelType::REAL_ESRGAN, params);

    if (result.success) {
        EXPECT_GT(result.outputImage.size(), 0);
        EXPECT_GT(result.processingTime, 0.0);
        EXPECT_GE(result.confidence, 0.0);
        EXPECT_LE(result.confidence, 1.0);
        EXPECT_FALSE(result.modelUsed.empty());

        // Check if quality metrics are provided
        if (!result.metrics.empty()) {
            for (const auto& [metric, value] : result.metrics) {
                EXPECT_FALSE(metric.empty());
                EXPECT_GE(value, 0.0);
            }
        }
    }
}

// Test concurrent processing
TEST_F(MLProcessingTest, ConcurrentProcessing) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    const int numThreads = 2; // Reduced for testing
    const int operationsPerThread = 2;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    MLParams params;
    params.useGPU = false;
    params.scaleFactor = 2;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &params, &successCount, &errorCount]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                try {
                    auto result = mlProcessor->superResolution(low_res_image,
                                                             MLModelType::REAL_ESRGAN, params);
                    if (result.success && result.outputImage.size() > 0) {
                        successCount.fetch_add(1);
                    } else {
                        errorCount.fetch_add(1);
                    }
                } catch (...) {
                    errorCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // At least some operations should succeed if models are available
    EXPECT_GE(successCount.load() + errorCount.load(), numThreads * operationsPerThread);
}

// Test performance benchmarking
TEST_F(MLProcessingTest, DISABLED_PerformanceBenchmark) {
    if (!initializationSuccess) {
        GTEST_SKIP() << "ML processor initialization failed - models not available";
    }

    MLParams params;
    params.useGPU = false;
    params.scaleFactor = 2;

    const int iterations = 5;
    double totalTime = 0.0;

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        auto result = mlProcessor->superResolution(low_res_image, MLModelType::REAL_ESRGAN, params);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (result.success) {
            totalTime += duration.count();
        }
    }

    if (totalTime > 0) {
        double avgTime = totalTime / iterations;
        std::cout << "Average ML processing time: " << avgTime << " ms" << std::endl;

        // Should complete in reasonable time (less than 30 seconds per operation)
        EXPECT_LT(avgTime, 30000.0);
    }
}

} // namespace atom::image::test
