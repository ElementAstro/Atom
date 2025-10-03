#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vector>
#include <memory>
#include <string>

#include "atom/image/processing/enhancement.hpp"
#include "atom/image/core/image_blob.hpp"
#include "test_utils.hpp"

namespace atom::image::test {

class EnhancementTest : public ::testing::Test {
protected:
    void SetUp() override {
        enhancement = std::make_unique<ImageEnhancement>();
        fileManager = std::make_unique<TestFileManager>();
        
        // Create test images
        createTestImages();
    }

    void TearDown() override {
        fileManager->cleanup();
    }

    void createTestImages() {
        // Create a low contrast gradient image
        auto lowContrastData = TestDataGenerator::generateGradientImage(32, 32, 1);
        // Reduce contrast by scaling values to middle range
        for (auto& pixel : lowContrastData) {
            uint8_t value = static_cast<uint8_t>(pixel);
            pixel = static_cast<std::byte>(64 + value / 4);
        }
        low_contrast_image = blob(lowContrastData.data(), lowContrastData.size());

        // Create a dark image
        auto darkData = TestDataGenerator::generateGradientImage(32, 32, 1);
        for (auto& pixel : darkData) {
            uint8_t value = static_cast<uint8_t>(pixel);
            pixel = static_cast<std::byte>(value / 4); // Make it darker
        }
        dark_image = blob(darkData.data(), darkData.size());

        // Create a bright image
        auto brightData = TestDataGenerator::generateGradientImage(32, 32, 1);
        for (auto& pixel : brightData) {
            uint8_t value = static_cast<uint8_t>(pixel);
            pixel = static_cast<std::byte>(std::min(255, static_cast<int>(value) + 128));
        }
        bright_image = blob(brightData.data(), brightData.size());

        // Create a noisy image
        auto noisyData = TestDataGenerator::generateRandomNoise(32, 32, 1, 12345);
        noisy_image = blob(noisyData.data(), noisyData.size());

        // Create a color image (RGB)
        auto colorData = TestDataGenerator::generateGradientImage(32, 32, 3);
        color_image = blob(colorData.data(), colorData.size());

        // Create an HDR-like image (high dynamic range simulation)
        auto hdrData = TestDataGenerator::generateGradientImage(32, 32, 1);
        for (auto& pixel : hdrData) {
            // Simulate HDR by expanding dynamic range
            uint8_t value = static_cast<uint8_t>(pixel);
            pixel = static_cast<std::byte>(std::min(255, static_cast<int>(value) * 2));
        }
        hdr_image = blob(hdrData.data(), hdrData.size());
    }

    std::unique_ptr<ImageEnhancement> enhancement;
    std::unique_ptr<TestFileManager> fileManager;
    
    blob low_contrast_image, dark_image, bright_image, noisy_image, color_image, hdr_image;
};

// Test global histogram equalization
TEST_F(EnhancementTest, GlobalHistogramEqualization) {
    auto result = enhancement->equalizeHistogram(low_contrast_image, HistogramMethod::GLOBAL);
    
    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), low_contrast_image.size());
}

// Test CLAHE (Contrast Limited Adaptive Histogram Equalization)
TEST_F(EnhancementTest, CLAHEHistogramEqualization) {
    EnhancementParams params;
    params.clipLimit = 2.0;
    params.tileGridSize = 8;
    
    auto result = enhancement->equalizeHistogram(low_contrast_image, HistogramMethod::CLAHE, params);
    
    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), low_contrast_image.size());
}

// Test adaptive histogram equalization
TEST_F(EnhancementTest, AdaptiveHistogramEqualization) {
    auto result = enhancement->equalizeHistogram(low_contrast_image, HistogramMethod::ADAPTIVE);
    
    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), low_contrast_image.size());
}

// Test local histogram equalization
TEST_F(EnhancementTest, LocalHistogramEqualization) {
    auto result = enhancement->equalizeHistogram(low_contrast_image, HistogramMethod::LOCAL);
    
    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), low_contrast_image.size());
}

// Test multi-scale histogram equalization
TEST_F(EnhancementTest, MultiScaleHistogramEqualization) {
    auto result = enhancement->equalizeHistogram(low_contrast_image, HistogramMethod::MULTI_SCALE);
    
    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), low_contrast_image.size());
}

// Test Reinhard tone mapping
TEST_F(EnhancementTest, ReinhardToneMapping) {
    EnhancementParams params;
    params.gamma = 2.2;
    params.exposure = 0.0;
    params.intensity = 1.0;
    
    auto result = enhancement->toneMapping(hdr_image, ToneMappingOperator::REINHARD, params);
    
    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), hdr_image.size());
}

// Test different tone mapping operators
TEST_F(EnhancementTest, ToneMappingOperators) {
    std::vector<ToneMappingOperator> operators = {
        ToneMappingOperator::REINHARD,
        ToneMappingOperator::DRAGO,
        ToneMappingOperator::MANTIUK,
        ToneMappingOperator::FATTAL,
        ToneMappingOperator::DURAND,
        ToneMappingOperator::GAMMA,
        ToneMappingOperator::LINEAR,
        ToneMappingOperator::LOGARITHMIC
    };

    for (const auto& op : operators) {
        auto result = enhancement->toneMapping(hdr_image, op);
        
        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), hdr_image.size());
    }
}

// Test brightness and contrast adjustment
TEST_F(EnhancementTest, BrightnessContrastAdjustment) {
    // Test brightness increase
    auto brighter = enhancement->adjustBrightnessContrast(dark_image, 50.0, 1.0);
    EXPECT_GT(brighter.size(), 0);
    EXPECT_EQ(brighter.size(), dark_image.size());
    
    // Test contrast increase
    auto higherContrast = enhancement->adjustBrightnessContrast(low_contrast_image, 0.0, 1.5);
    EXPECT_GT(higherContrast.size(), 0);
    EXPECT_EQ(higherContrast.size(), low_contrast_image.size());
    
    // Test combined adjustment
    auto combined = enhancement->adjustBrightnessContrast(dark_image, 25.0, 1.2);
    EXPECT_GT(combined.size(), 0);
    EXPECT_EQ(combined.size(), dark_image.size());
}

// Test gamma correction
TEST_F(EnhancementTest, GammaCorrection) {
    std::vector<double> gammaValues = {0.5, 1.0, 1.8, 2.2, 2.8};
    
    for (double gamma : gammaValues) {
        auto result = enhancement->gammaCorrection(dark_image, gamma);
        
        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), dark_image.size());
    }
}

// Test gamma correction with different color spaces
TEST_F(EnhancementTest, GammaCorrectionColorSpaces) {
    std::vector<ColorSpace> colorSpaces = {
        ColorSpace::RGB,
        ColorSpace::HSV,
        ColorSpace::HSL,
        ColorSpace::LAB,
        ColorSpace::YUV,
        ColorSpace::GRAY
    };

    for (const auto& colorSpace : colorSpaces) {
        auto result = enhancement->gammaCorrection(color_image, 2.2, colorSpace);
        
        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), color_image.size());
    }
}

// Test image sharpening
TEST_F(EnhancementTest, ImageSharpening) {
    std::vector<std::string> sharpenMethods = {
        "unsharp_mask",
        "high_pass",
        "clarity"
    };

    for (const auto& method : sharpenMethods) {
        auto result = enhancement->sharpen(low_contrast_image, 1.0, 1.0, 0.0, method);
        
        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), low_contrast_image.size());
    }
}

// Test sharpening with different parameters
TEST_F(EnhancementTest, SharpeningParameters) {
    // Test different strength values
    std::vector<double> strengths = {0.5, 1.0, 1.5, 2.0};
    
    for (double strength : strengths) {
        auto result = enhancement->sharpen(low_contrast_image, strength);
        EXPECT_GT(result.size(), 0);
    }
    
    // Test different radius values
    std::vector<double> radii = {0.5, 1.0, 1.5, 2.0};
    
    for (double radius : radii) {
        auto result = enhancement->sharpen(low_contrast_image, 1.0, radius);
        EXPECT_GT(result.size(), 0);
    }
}

// Test noise reduction
TEST_F(EnhancementTest, NoiseReduction) {
    std::vector<std::string> denoiseMethods = {
        "bilateral",
        "nlm",
        "bm3d",
        "dct"
    };

    for (const auto& method : denoiseMethods) {
        auto result = enhancement->denoise(noisy_image, 0.5, method);
        
        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), noisy_image.size());
    }
}

// Test color correction methods
TEST_F(EnhancementTest, ColorCorrectionMethods) {
    std::vector<ColorCorrectionMethod> methods = {
        ColorCorrectionMethod::WHITE_BALANCE,
        ColorCorrectionMethod::COLOR_CAST,
        ColorCorrectionMethod::GAMMA_CORRECTION,
        ColorCorrectionMethod::CURVES,
        ColorCorrectionMethod::LEVELS,
        ColorCorrectionMethod::COLOR_GRADING,
        ColorCorrectionMethod::AUTO_LEVELS,
        ColorCorrectionMethod::AUTO_COLOR
    };

    for (const auto& method : methods) {
        auto result = enhancement->colorCorrection(color_image, method);
        
        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), color_image.size());
    }
}

// Test white balance correction
TEST_F(EnhancementTest, WhiteBalanceCorrection) {
    EnhancementParams params;
    params.temperature = 5500.0;  // Daylight
    params.tint = 0.0;
    
    auto result = enhancement->colorCorrection(color_image, 
                                             ColorCorrectionMethod::WHITE_BALANCE, params);
    
    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), color_image.size());
}

// Test color temperature adjustment
TEST_F(EnhancementTest, ColorTemperatureAdjustment) {
    std::vector<double> temperatures = {3000.0, 4000.0, 5500.0, 6500.0, 8000.0};
    
    for (double temp : temperatures) {
        EnhancementParams params;
        params.temperature = temp;
        
        auto result = enhancement->colorCorrection(color_image, 
                                                 ColorCorrectionMethod::WHITE_BALANCE, params);
        EXPECT_GT(result.size(), 0);
    }
}

// Test saturation and vibrance adjustment
TEST_F(EnhancementTest, SaturationVibranceAdjustment) {
    std::vector<std::pair<double, double>> values = {
        {0.0, 0.0},    // No adjustment
        {20.0, 10.0},  // Increase both
        {-20.0, -10.0}, // Decrease both
        {50.0, 0.0},   // Only saturation
        {0.0, 30.0}    // Only vibrance
    };

    for (const auto& [saturation, vibrance] : values) {
        auto result = enhancement->vibranceSaturation(color_image, vibrance, saturation);

        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), color_image.size());
    }
}

// Test clarity enhancement
TEST_F(EnhancementTest, ClarityEnhancement) {
    double clarityAmount = 30.0;  // 30% clarity
    double radius = 20.0;

    auto result = enhancement->clarity(low_contrast_image, clarityAmount, radius);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), low_contrast_image.size());
}

// Test dehaze operation
TEST_F(EnhancementTest, DehazeOperation) {
    double strength = 0.4;
    bool preserveColors = true;

    auto result = enhancement->dehaze(low_contrast_image, strength, preserveColors);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), low_contrast_image.size());
}

// Test highlights and shadows adjustment
TEST_F(EnhancementTest, HighlightsShadowsAdjustment) {
    double shadows = 30.0;      // Lift shadows
    double highlights = -30.0;  // Reduce highlights
    double radius = 30.0;

    auto result = enhancement->shadowHighlight(bright_image, shadows, highlights, radius);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), bright_image.size());
}

// Test levels adjustment
TEST_F(EnhancementTest, LevelsAdjustment) {
    double blackPoint = 10.0;   // Lift black point
    double whitePoint = 245.0;  // Lower white point
    double gamma = 1.2;
    double outputBlack = 0.0;
    double outputWhite = 255.0;

    auto result = enhancement->adjustLevels(color_image, blackPoint, whitePoint,
                                          gamma, outputBlack, outputWhite);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), color_image.size());
}

// Test curves adjustment
TEST_F(EnhancementTest, CurvesAdjustment) {
    // Create a simple S-curve for contrast enhancement
    std::vector<std::pair<double, double>> curve = {
        {0.0, 0.0},
        {0.25, 0.2},
        {0.5, 0.5},
        {0.75, 0.8},
        {1.0, 1.0}
    };

    auto result = enhancement->applyCurve(low_contrast_image, curve);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), low_contrast_image.size());
}

// Test color grading
TEST_F(EnhancementTest, ColorGrading) {
    EnhancementParams params;
    params.temperature = 6000.0;
    params.tint = 0.1;
    params.saturation = 1.2;
    params.vibrance = 0.2;

    auto result = enhancement->colorCorrection(color_image,
                                             ColorCorrectionMethod::COLOR_GRADING, params);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), color_image.size());
}

// Test auto levels
TEST_F(EnhancementTest, AutoLevels) {
    auto result = enhancement->colorCorrection(low_contrast_image,
                                             ColorCorrectionMethod::AUTO_LEVELS);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), low_contrast_image.size());
}

// Test auto color correction
TEST_F(EnhancementTest, AutoColorCorrection) {
    auto result = enhancement->colorCorrection(color_image,
                                             ColorCorrectionMethod::AUTO_COLOR);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), color_image.size());
}

// Test enhancement parameter validation
TEST_F(EnhancementTest, ParameterValidation) {
    // Test invalid gamma values
    EXPECT_THROW(enhancement->gammaCorrection(dark_image, -1.0), std::invalid_argument);
    EXPECT_THROW(enhancement->gammaCorrection(dark_image, 0.0), std::invalid_argument);

    // Test invalid brightness values
    EXPECT_THROW(enhancement->adjustBrightnessContrast(dark_image, -200.0), std::invalid_argument);
    EXPECT_THROW(enhancement->adjustBrightnessContrast(dark_image, 200.0), std::invalid_argument);

    // Test invalid contrast values
    EXPECT_THROW(enhancement->adjustBrightnessContrast(dark_image, 0.0, -1.0), std::invalid_argument);
}

// Test empty image handling
TEST_F(EnhancementTest, EmptyImageHandling) {
    blob emptyImage;

    EXPECT_THROW(enhancement->equalizeHistogram(emptyImage), std::invalid_argument);
    EXPECT_THROW(enhancement->adjustBrightnessContrast(emptyImage, 0.0, 1.0), std::invalid_argument);
    EXPECT_THROW(enhancement->gammaCorrection(emptyImage, 2.2), std::invalid_argument);
}

// Test enhancement chaining
TEST_F(EnhancementTest, EnhancementChaining) {
    // Apply multiple enhancements in sequence
    auto step1 = enhancement->adjustBrightnessContrast(dark_image, 20.0, 1.2);
    auto step2 = enhancement->gammaCorrection(step1, 1.8);
    auto step3 = enhancement->sharpen(step2, 0.8);
    auto final = enhancement->equalizeHistogram(step3, HistogramMethod::CLAHE);

    EXPECT_GT(final.size(), 0);
    EXPECT_EQ(final.size(), dark_image.size());
}

// Test preservation of image properties
TEST_F(EnhancementTest, ImagePropertiesPreservation) {
    // Test that enhancement preserves image dimensions
    auto enhanced = enhancement->adjustBrightnessContrast(color_image, 10.0, 1.1);

    EXPECT_EQ(enhanced.size(), color_image.size());

    // Test with different enhancement operations
    auto sharpened = enhancement->sharpen(enhanced, 1.0);
    EXPECT_EQ(sharpened.size(), color_image.size());

    auto denoised = enhancement->denoise(sharpened, 0.3);
    EXPECT_EQ(denoised.size(), color_image.size());
}

// Test extreme parameter values
TEST_F(EnhancementTest, ExtremeParameterValues) {
    // Test with extreme but valid values
    auto extremeBright = enhancement->adjustBrightnessContrast(dark_image, 99.0, 1.0);
    EXPECT_GT(extremeBright.size(), 0);

    auto extremeContrast = enhancement->adjustBrightnessContrast(low_contrast_image, 0.0, 2.9);
    EXPECT_GT(extremeContrast.size(), 0);

    auto extremeGamma = enhancement->gammaCorrection(dark_image, 0.1);
    EXPECT_GT(extremeGamma.size(), 0);

    auto extremeSharpening = enhancement->sharpen(low_contrast_image, 2.0);
    EXPECT_GT(extremeSharpening.size(), 0);
}

// Test different CLAHE parameters
TEST_F(EnhancementTest, CLAHEParameterVariations) {
    std::vector<double> clipLimits = {1.0, 2.0, 4.0, 8.0};
    std::vector<int> tileSizes = {4, 8, 16};

    for (double clipLimit : clipLimits) {
        for (int tileSize : tileSizes) {
            EnhancementParams params;
            params.clipLimit = clipLimit;
            params.tileGridSize = tileSize;

            auto result = enhancement->equalizeHistogram(low_contrast_image,
                                                       HistogramMethod::CLAHE, params);
            EXPECT_GT(result.size(), 0);
        }
    }
}

// Test tone mapping parameter variations
TEST_F(EnhancementTest, ToneMappingParameterVariations) {
    std::vector<double> gammaValues = {1.0, 1.8, 2.2, 2.8};
    std::vector<double> exposureValues = {-2.0, -1.0, 0.0, 1.0, 2.0};

    for (double gamma : gammaValues) {
        for (double exposure : exposureValues) {
            EnhancementParams params;
            params.gamma = gamma;
            params.exposure = exposure;

            auto result = enhancement->toneMapping(hdr_image,
                                                 ToneMappingOperator::REINHARD, params);
            EXPECT_GT(result.size(), 0);
        }
    }
}

} // namespace atom::image::test
