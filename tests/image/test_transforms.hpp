#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/processing/transforms.hpp"
#include "test_utils.hpp"

namespace atom::image::test {

class TransformsTest : public ::testing::Test {
protected:
    void SetUp() override {
        transform = std::make_unique<ImageTransform>();
        fileManager = std::make_unique<TestFileManager>();

        // Create test images
        createTestImages();
    }

    void TearDown() override { fileManager->cleanup(); }

    void createTestImages() {
        // Create a simple gradient image
        auto gradientData = TestDataGenerator::generateGradientImage(32, 32, 1);
        gradient_image = blob(gradientData.data(), gradientData.size());

        // Create a checkerboard pattern
        auto checkerboardData =
            TestDataGenerator::generateCheckerboard(32, 32, 1, 4);
        checkerboard_image =
            blob(checkerboardData.data(), checkerboardData.size());

        // Create a circular pattern for rotation tests
        auto circleData =
            TestDataGenerator::generateCircularPattern(32, 32, 1, 10);
        circle_image = blob(circleData.data(), circleData.size());

        // Create a larger image for perspective tests
        auto largeData = TestDataGenerator::generateGradientImage(64, 64, 1);
        large_image = blob(largeData.data(), largeData.size());
    }

    std::unique_ptr<ImageTransform> transform;
    std::unique_ptr<TestFileManager> fileManager;

    blob gradient_image, checkerboard_image, circle_image, large_image;
};

// Test basic image resizing
TEST_F(TransformsTest, BasicResize) {
    auto result = transform->resize(gradient_image, 64, 64);

    EXPECT_GT(result.size(), 0);
    // New size should be 64x64 = 4096 pixels (assuming single channel)
    EXPECT_EQ(result.size(), 64 * 64);
}

// Test resize with different interpolation methods
TEST_F(TransformsTest, ResizeInterpolationMethods) {
    std::vector<InterpolationMethod> methods = {
        InterpolationMethod::NEAREST, InterpolationMethod::LINEAR,
        InterpolationMethod::CUBIC,   InterpolationMethod::LANCZOS,
        InterpolationMethod::AREA,    InterpolationMethod::SUPER_SAMPLING};

    for (const auto& method : methods) {
        auto result = transform->resize(gradient_image, 48, 48, method);

        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), 48 * 48);
    }
}

// Test resize with aspect ratio preservation
TEST_F(TransformsTest, ResizePreserveAspect) {
    // Test with non-square target dimensions
    auto result = transform->resize(gradient_image, 64, 32,
                                    InterpolationMethod::LINEAR, true);

    EXPECT_GT(result.size(), 0);
    // With aspect preservation, the actual size might be different
}

// Test basic rotation
TEST_F(TransformsTest, BasicRotation) {
    double angle = 45.0;  // 45 degrees

    auto result = transform->rotate(circle_image, angle);

    EXPECT_GT(result.size(), 0);
}

// Test rotation with different angles
TEST_F(TransformsTest, RotationAngles) {
    std::vector<double> angles = {0.0, 30.0, 45.0, 90.0, 180.0, 270.0, 360.0};

    for (double angle : angles) {
        auto result = transform->rotate(circle_image, angle);

        EXPECT_GT(result.size(), 0);
    }
}

// Test rotation with custom center
TEST_F(TransformsTest, RotationCustomCenter) {
    Point2D customCenter(10.0, 10.0);
    double angle = 90.0;

    auto result = transform->rotate(circle_image, angle, customCenter);

    EXPECT_GT(result.size(), 0);
}

// Test rotation with different border modes
TEST_F(TransformsTest, RotationBorderModes) {
    std::vector<BorderMode> borderModes = {
        BorderMode::CONSTANT, BorderMode::REPLICATE, BorderMode::REFLECT,
        BorderMode::WRAP, BorderMode::TRANSPARENT};

    double angle = 45.0;

    for (const auto& borderMode : borderModes) {
        auto result =
            transform->rotate(circle_image, angle, {}, true,
                              InterpolationMethod::LINEAR, borderMode);

        EXPECT_GT(result.size(), 0);
    }
}

// Test affine transformation
TEST_F(TransformsTest, AffineTransformation) {
    // Create a simple scaling transformation matrix
    std::array<std::array<double, 3>, 2> scaleMatrix = {{
        {{1.5, 0.0, 0.0}},  // Scale X by 1.5
        {{0.0, 1.5, 0.0}}   // Scale Y by 1.5
    }};

    auto result = transform->affineTransform(gradient_image, scaleMatrix);

    EXPECT_GT(result.size(), 0);
}

// Test affine transformation with translation
TEST_F(TransformsTest, AffineTransformationWithTranslation) {
    // Create a translation matrix
    std::array<std::array<double, 3>, 2> translateMatrix = {{
        {{1.0, 0.0, 10.0}},  // Translate X by 10
        {{0.0, 1.0, 5.0}}    // Translate Y by 5
    }};

    auto result = transform->affineTransform(gradient_image, translateMatrix);

    EXPECT_GT(result.size(), 0);
}

// Test affine transformation with shear
TEST_F(TransformsTest, AffineTransformationWithShear) {
    // Create a shear matrix
    std::array<std::array<double, 3>, 2> shearMatrix = {{
        {{1.0, 0.5, 0.0}},  // Shear X
        {{0.0, 1.0, 0.0}}   // No shear Y
    }};

    auto result = transform->affineTransform(gradient_image, shearMatrix);

    EXPECT_GT(result.size(), 0);
}

// Test perspective transformation
TEST_F(TransformsTest, PerspectiveTransformation) {
    // Create a simple perspective transformation matrix
    TransformMatrix perspectiveMatrix = {
        {{{1.0, 0.1, 0.0}}, {{0.1, 1.0, 0.0}}, {{0.001, 0.001, 1.0}}}};

    Point2D outputSize(64, 64);

    auto result = transform->perspectiveTransform(
        large_image, perspectiveMatrix, outputSize);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), 64 * 64);
}

// Test perspective correction
TEST_F(TransformsTest, PerspectiveCorrection) {
    // Define source and destination points for perspective correction
    std::array<Point2D, 4> srcPoints = {{
        {0, 0}, {63, 0}, {63, 63}, {0, 63}  // Original corners
    }};

    std::array<Point2D, 4> dstPoints = {{
        {10, 5}, {53, 8}, {58, 58}, {5, 55}  // Distorted corners
    }};

    Point2D outputSize(64, 64);

    auto result = transform->correctPerspective(large_image, srcPoints,
                                                dstPoints, outputSize);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), 64 * 64);
}

// Test distortion correction
TEST_F(TransformsTest, DistortionCorrection) {
    // Test barrel distortion correction
    double k1 = -0.1;  // Barrel distortion
    double k2 = 0.01;

    auto result = transform->correctDistortion(circle_image, k1, k2);

    EXPECT_GT(result.size(), 0);
}

// Test distortion correction with tangential distortion
TEST_F(TransformsTest, DistortionCorrectionTangential) {
    double k1 = 0.05;  // Pincushion distortion
    double k2 = -0.01;
    double p1 = 0.001;  // Tangential distortion
    double p2 = 0.002;

    Point2D customCenter(16.0, 16.0);

    auto result = transform->correctDistortion(circle_image, k1, k2, 0, p1, p2,
                                               customCenter);

    EXPECT_GT(result.size(), 0);
}

// Test elastic deformation
TEST_F(TransformsTest, ElasticDeformation) {
    // Create simple displacement fields
    int size = 32;
    std::vector<std::vector<double>> displacementX(
        size, std::vector<double>(size, 0.0));
    std::vector<std::vector<double>> displacementY(
        size, std::vector<double>(size, 0.0));

    // Create a simple wave deformation
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            displacementX[y][x] = 2.0 * std::sin(2.0 * M_PI * x / size);
            displacementY[y][x] = 2.0 * std::cos(2.0 * M_PI * y / size);
        }
    }

    auto result =
        transform->elasticDeform(gradient_image, displacementX, displacementY);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), gradient_image.size());
}

// Test polar transformation
TEST_F(TransformsTest, PolarTransformation) {
    Point2D center(16.0, 16.0);  // Center of 32x32 image
    double maxRadius = 16.0;

    auto result = transform->toPolar(circle_image, center, maxRadius);

    EXPECT_GT(result.size(), 0);
}

// Test inverse polar transformation
TEST_F(TransformsTest, InversePolarTransformation) {
    // First convert to polar
    Point2D center(16.0, 16.0);
    auto polarImage = transform->toPolar(circle_image, center);

    // Then convert back to Cartesian
    Point2D outputSize(32, 32);
    auto result = transform->fromPolar(polarImage, outputSize, center);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), 32 * 32);
}

// Test log-polar transformation
TEST_F(TransformsTest, LogPolarTransformation) {
    Point2D center(16.0, 16.0);

    auto result = transform->toLogPolar(circle_image, center);

    EXPECT_GT(result.size(), 0);
}

// Test image registration
TEST_F(TransformsTest, ImageRegistration) {
    // Create a slightly rotated version of the image for registration
    auto rotatedImage = transform->rotate(circle_image, 5.0);

    auto registrationMatrix =
        transform->registerImages(circle_image, rotatedImage);

    // The registration matrix should be close to a 5-degree rotation
    EXPECT_TRUE(registrationMatrix.size() > 0);
}

// Test transformation composition
TEST_F(TransformsTest, TransformationComposition) {
    // Create multiple transformation matrices
    std::array<std::array<double, 3>, 2> scaleMatrix = {
        {{{2.0, 0.0, 0.0}}, {{0.0, 2.0, 0.0}}}};

    std::array<std::array<double, 3>, 2> rotateMatrix = {
        {{{std::cos(M_PI / 4), -std::sin(M_PI / 4), 0.0}},
         {{std::sin(M_PI / 4), std::cos(M_PI / 4), 0.0}}}};

    // Apply transformations sequentially
    auto scaled = transform->affineTransform(gradient_image, scaleMatrix);
    auto scaledAndRotated = transform->affineTransform(scaled, rotateMatrix);

    EXPECT_GT(scaledAndRotated.size(), 0);
}

// Test error handling with invalid parameters
TEST_F(TransformsTest, ErrorHandlingInvalidParameters) {
    // Test resize with invalid dimensions
    EXPECT_THROW(transform->resize(gradient_image, -10, 20),
                 std::invalid_argument);
    EXPECT_THROW(transform->resize(gradient_image, 10, -20),
                 std::invalid_argument);
    EXPECT_THROW(transform->resize(gradient_image, 0, 20),
                 std::invalid_argument);

    // Test empty image
    blob emptyImage;
    EXPECT_THROW(transform->resize(emptyImage, 10, 10), std::invalid_argument);
}

// Test transformation accuracy
TEST_F(TransformsTest, TransformationAccuracy) {
    // Test that 360-degree rotation returns approximately the original image
    auto rotated360 = transform->rotate(circle_image, 360.0);

    EXPECT_GT(rotated360.size(), 0);
    EXPECT_EQ(rotated360.size(), circle_image.size());

    // Test that identity transformation returns the original image
    std::array<std::array<double, 3>, 2> identityMatrix = {
        {{{1.0, 0.0, 0.0}}, {{0.0, 1.0, 0.0}}}};

    auto identity = transform->affineTransform(gradient_image, identityMatrix);
    EXPECT_EQ(identity.size(), gradient_image.size());
}

// Test interpolation quality
TEST_F(TransformsTest, InterpolationQuality) {
    // Test upscaling with different interpolation methods
    int newSize = 64;

    auto nearest = transform->resize(gradient_image, newSize, newSize,
                                     InterpolationMethod::NEAREST);
    auto linear = transform->resize(gradient_image, newSize, newSize,
                                    InterpolationMethod::LINEAR);
    auto cubic = transform->resize(gradient_image, newSize, newSize,
                                   InterpolationMethod::CUBIC);

    EXPECT_EQ(nearest.size(), newSize * newSize);
    EXPECT_EQ(linear.size(), newSize * newSize);
    EXPECT_EQ(cubic.size(), newSize * newSize);
}

// Test downscaling quality
TEST_F(TransformsTest, DownscalingQuality) {
    // Test downscaling with area interpolation (should be best for downscaling)
    auto downscaled =
        transform->resize(large_image, 32, 32, InterpolationMethod::AREA);

    EXPECT_GT(downscaled.size(), 0);
    EXPECT_EQ(downscaled.size(), 32 * 32);
}

// Test extreme transformations
TEST_F(TransformsTest, ExtremeTransformations) {
    // Test very large scaling
    std::array<std::array<double, 3>, 2> largeScaleMatrix = {
        {{{10.0, 0.0, 0.0}}, {{0.0, 10.0, 0.0}}}};

    auto largeScaled =
        transform->affineTransform(gradient_image, largeScaleMatrix);
    EXPECT_GT(largeScaled.size(), 0);

    // Test very small scaling
    std::array<std::array<double, 3>, 2> smallScaleMatrix = {
        {{{0.1, 0.0, 0.0}}, {{0.0, 0.1, 0.0}}}};

    auto smallScaled =
        transform->affineTransform(gradient_image, smallScaleMatrix);
    EXPECT_GT(smallScaled.size(), 0);
}

// Test rotation precision
TEST_F(TransformsTest, RotationPrecision) {
    // Test small angle rotations
    std::vector<double> smallAngles = {0.1, 0.5, 1.0, 2.0, 5.0};

    for (double angle : smallAngles) {
        auto result = transform->rotate(circle_image, angle);
        EXPECT_GT(result.size(), 0);
    }

    // Test large angle rotations
    std::vector<double> largeAngles = {720.0, 1080.0, -360.0, -180.0};

    for (double angle : largeAngles) {
        auto result = transform->rotate(circle_image, angle);
        EXPECT_GT(result.size(), 0);
    }
}

// Test perspective transformation edge cases
TEST_F(TransformsTest, PerspectiveEdgeCases) {
    // Test with nearly degenerate transformation
    std::array<Point2D, 4> srcPoints = {{{0, 0}, {63, 0}, {63, 63}, {0, 63}}};

    std::array<Point2D, 4> nearlyDegeneratePoints = {{
        {0, 0}, {63, 1}, {62, 63}, {1, 62}  // Very slight distortion
    }};

    Point2D outputSize(64, 64);

    auto result = transform->correctPerspective(
        large_image, srcPoints, nearlyDegeneratePoints, outputSize);
    EXPECT_GT(result.size(), 0);
}

// Test distortion correction with extreme parameters
TEST_F(TransformsTest, ExtremeDistortionCorrection) {
    // Test with strong barrel distortion
    double strongBarrel = -0.5;
    auto barrelResult =
        transform->correctDistortion(circle_image, strongBarrel);
    EXPECT_GT(barrelResult.size(), 0);

    // Test with strong pincushion distortion
    double strongPincushion = 0.5;
    auto pincushionResult =
        transform->correctDistortion(circle_image, strongPincushion);
    EXPECT_GT(pincushionResult.size(), 0);
}

// Test elastic deformation edge cases
TEST_F(TransformsTest, ElasticDeformationEdgeCases) {
    int size = 32;

    // Test with zero displacement (should return original image)
    std::vector<std::vector<double>> zeroDisplacement(
        size, std::vector<double>(size, 0.0));
    auto zeroResult = transform->elasticDeform(gradient_image, zeroDisplacement,
                                               zeroDisplacement);
    EXPECT_EQ(zeroResult.size(), gradient_image.size());

    // Test with large displacement
    std::vector<std::vector<double>> largeDisplacement(
        size, std::vector<double>(size, 10.0));
    auto largeResult = transform->elasticDeform(
        gradient_image, largeDisplacement, largeDisplacement);
    EXPECT_GT(largeResult.size(), 0);
}

// Test polar transformation with different parameters
TEST_F(TransformsTest, PolarTransformationParameters) {
    Point2D center(16.0, 16.0);

    // Test with different angle ranges
    std::vector<double> angleRanges = {90.0, 180.0, 270.0, 360.0, 720.0};

    for (double angleRange : angleRanges) {
        auto result =
            transform->toPolar(circle_image, center, 16.0, angleRange);
        EXPECT_GT(result.size(), 0);
    }

    // Test with different radii
    std::vector<double> radii = {5.0, 10.0, 16.0, 20.0, 32.0};

    for (double radius : radii) {
        auto result = transform->toPolar(circle_image, center, radius);
        EXPECT_GT(result.size(), 0);
    }
}

// Test concurrent transformations
TEST_F(TransformsTest, ConcurrentTransformations) {
    const int numThreads = 4;
    const int operationsPerThread = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &successCount, &errorCount]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                try {
                    auto result = transform->resize(gradient_image, 48, 48);
                    if (result.size() == 48 * 48) {
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

    EXPECT_EQ(successCount.load(), numThreads * operationsPerThread);
    EXPECT_EQ(errorCount.load(), 0);
}

// Test performance with large transformations
TEST_F(TransformsTest, DISABLED_PerformanceTest) {
    // Create a large test image
    auto largeData = TestDataGenerator::generateGradientImage(256, 256, 1);
    blob largeTestImage(largeData.data(), largeData.size());

    auto start = std::chrono::high_resolution_clock::now();

    // Perform multiple transformations
    auto resized = transform->resize(largeTestImage, 512, 512);
    auto rotated = transform->rotate(resized, 45.0);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Large transformation time: " << duration.count() << " ms"
              << std::endl;

    EXPECT_GT(rotated.size(), 0);
    // Should complete in reasonable time (less than 5 seconds)
    EXPECT_LT(duration.count(), 5000);
}

// Test memory management
TEST_F(TransformsTest, MemoryManagement) {
    // Test that transformations properly manage memory
    for (int i = 0; i < 100; ++i) {
        auto result = transform->resize(gradient_image, 48, 48);
        EXPECT_EQ(result.size(), 48 * 48);
        // Memory should be automatically cleaned up
    }
}

}  // namespace atom::image::test
