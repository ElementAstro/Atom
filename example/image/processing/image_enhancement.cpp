/**
 * @file image_enhancement.cpp
 * @brief Example demonstrating advanced image enhancement techniques
 *
 * This example covers:
 * - Histogram equalization (global and adaptive)
 * - HDR tone mapping operators
 * - Color correction and white balance
 * - Brightness and contrast adjustment
 * - Gamma correction
 * - Saturation and vibrance adjustment
 * - Clarity and structure enhancement
 * - Automatic enhancement algorithms
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/processing/enhancement.hpp"
#include "atom/image/processing/image_processor.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Create a test image with various lighting conditions
 */
blob createEnhancementTestImage() {
    const int width = 400;
    const int height = 300;
    // Create raw data buffer
    std::vector<uint8_t> data(height * width * 3);

    blob img(reinterpret_cast<std::byte*>(data.data()), data.size());

    // Create regions with different brightness levels
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double brightness_factor = 1.0;

            // Dark region (left side)
            if (x < width / 3) {
                brightness_factor = 0.3;
            }
            // Bright region (right side)
            else if (x > 2 * width / 3) {
                brightness_factor = 1.8;
            }
            // Normal region (middle)
            else {
                brightness_factor = 1.0;
            }

            // Add some texture
            double texture = 0.5 + 0.3 * std::sin(x * 0.1) * std::cos(y * 0.1);

            // Color gradients
            uint8_t base_r = static_cast<uint8_t>(std::clamp(
                (x * 255.0 / width) * brightness_factor * texture, 0.0, 255.0));
            uint8_t base_g = static_cast<uint8_t>(
                std::clamp((y * 255.0 / height) * brightness_factor * texture,
                           0.0, 255.0));
            uint8_t base_b = static_cast<uint8_t>(
                std::clamp(((x + y) * 255.0 / (width + height)) *
                               brightness_factor * texture,
                           0.0, 255.0));

            // Add some geometric features
            int center_x = width / 2;
            int center_y = height / 2;
            double distance = std::sqrt((x - center_x) * (x - center_x) +
                                        (y - center_y) * (y - center_y));

            if (distance < 30) {
                base_r = std::min(255, base_r + 50);
                base_g = std::min(255, base_g + 50);
                base_b = std::min(255, base_b + 50);
            }

            // Direct access to data vector
            int pixel_idx = (y * width + x) * 3;
            data[pixel_idx] = base_r;
            data[pixel_idx + 1] = base_g;
            data[pixel_idx + 2] = base_b;
        }
    }

    return blob(reinterpret_cast<std::byte*>(data.data()), data.size());
}

/**
 * @brief Calculate and display histogram statistics
 */
void displayHistogramStats(const blob& img, const std::string& description) {
    std::vector<int> histogram(256, 0);

    // Simplified histogram calculation
    const uint8_t* data = reinterpret_cast<const uint8_t*>(img.data());
    size_t pixel_count = img.size() / 3;  // Assume 3-channel image

    for (size_t i = 0; i < pixel_count; ++i) {
        histogram[data[i * 3]]++;  // Only use first channel
    }

    // Find statistics
    int min_val = 0, max_val = 255;
    for (int i = 0; i < 256; ++i) {
        if (histogram[i] > 0) {
            min_val = i;
            break;
        }
    }
    for (int i = 255; i >= 0; --i) {
        if (histogram[i] > 0) {
            max_val = i;
            break;
        }
    }

    // Calculate mean
    long long sum = 0, count = 0;
    for (int i = 0; i < 256; ++i) {
        sum += i * histogram[i];
        count += histogram[i];
    }
    double mean = static_cast<double>(sum) / count;

    std::cout << "  " << description << " - Range: [" << min_val << ", "
              << max_val << "], Mean: " << std::fixed << std::setprecision(1)
              << mean << "\n";
}

/**
 * @brief Demonstrate histogram equalization
 */
void demonstrateHistogramEqualization() {
    std::cout << "\n=== Histogram Equalization ===\n";

    try {
        auto original = createEnhancementTestImage();
        std::cout << "Original image blob size: " << original.size()
                  << " bytes\n";

        displayHistogramStats(original, "Original");

        ImageEnhancement enhancer;

        // Test different histogram equalization methods
        std::vector<std::pair<HistogramMethod, std::string>> methods = {
            {HistogramMethod::GLOBAL, "Global Histogram Equalization"},
            {HistogramMethod::CLAHE, "CLAHE (Contrast Limited Adaptive)"},
            {HistogramMethod::ADAPTIVE, "Adaptive Histogram Equalization"}};

        for (const auto& [method, name] : methods) {
            auto start = high_resolution_clock::now();

            EnhancementParams params;
            if (method == HistogramMethod::CLAHE) {
                params.clipLimit = 2.0;
                params.tileSize = 8;
            } else if (method == HistogramMethod::ADAPTIVE) {
                params.windowSize = 64;
            }

            auto enhanced =
                enhancer.equalizeHistogram(original, method, params);

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "\n" << name << ": " << duration.count() << " μs\n";
            displayHistogramStats(enhanced, "Enhanced");

            if (method == HistogramMethod::CLAHE) {
                std::cout << "  Clip limit: " << params.clipLimit
                          << ", Tile size: " << params.tileSize << "\n";
            }
        }

        // Compare different CLAHE parameters
        std::cout << "\nCLAHE parameter comparison:\n";
        std::vector<double> clip_limits = {1.0, 2.0, 4.0, 8.0};

        for (double clip_limit : clip_limits) {
            EnhancementParams params;
            params.clipLimit = clip_limit;
            params.tileSize = 8;

            auto start = high_resolution_clock::now();
            auto enhanced = enhancer.equalizeHistogram(
                original, HistogramMethod::CLAHE, params);
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "  Clip limit " << clip_limit << ": "
                      << duration.count() << " μs\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in histogram equalization: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate tone mapping for HDR images
 */
void demonstrateToneMapping() {
    std::cout << "\n=== HDR Tone Mapping ===\n";

    try {
        // Create HDR-like test image with high dynamic range
        blob<uint8_t> hdr_image(200, 300, 3);

        for (int y = 0; y < hdr_image.rows(); ++y) {
            for (int x = 0; x < hdr_image.cols(); ++x) {
                // Simulate HDR content with extreme brightness variations
                double brightness = 1.0;

                // Very bright sun-like region
                int sun_x = hdr_image.cols() / 4;
                int sun_y = hdr_image.rows() / 4;
                double sun_dist = std::sqrt((x - sun_x) * (x - sun_x) +
                                            (y - sun_y) * (y - sun_y));
                if (sun_dist < 20) {
                    brightness = 5.0;  // Simulate very bright region
                }

                // Shadow region
                if (x > 2 * hdr_image.cols() / 3 &&
                    y > 2 * hdr_image.rows() / 3) {
                    brightness = 0.1;  // Very dark region
                }

                uint8_t r = static_cast<uint8_t>(
                    std::clamp(128 * brightness, 0.0, 255.0));
                uint8_t g = static_cast<uint8_t>(
                    std::clamp(100 * brightness, 0.0, 255.0));
                uint8_t b = static_cast<uint8_t>(
                    std::clamp(80 * brightness, 0.0, 255.0));

                hdr_image.at(y, x, 0) = r;
                hdr_image.at(y, x, 1) = g;
                hdr_image.at(y, x, 2) = b;
            }
        }

        std::cout << "Created HDR test image: " << hdr_image.cols() << "x"
                  << hdr_image.rows() << "\n";
        displayHistogramStats(hdr_image, "HDR Original");

        ImageEnhancement enhancer;

        // Test different tone mapping operators
        std::vector<std::pair<ToneMappingOperator, std::string>> operators = {
            {ToneMappingOperator::REINHARD, "Reinhard Tone Mapping"},
            {ToneMappingOperator::DRAGO, "Drago Tone Mapping"},
            {ToneMappingOperator::MANTIUK, "Mantiuk Tone Mapping"},
            {ToneMappingOperator::FATTAL, "Fattal Tone Mapping"}};

        for (const auto& [op, name] : operators) {
            auto start = high_resolution_clock::now();

            EnhancementParams params;
            if (op == ToneMappingOperator::REINHARD) {
                params.key = 0.18;
                params.white = 1.0;
            } else if (op == ToneMappingOperator::DRAGO) {
                params.bias = 0.85;
            } else if (op == ToneMappingOperator::MANTIUK) {
                params.colorSaturation = 1.0;
                params.contrastEnhancement = 1.0;
            } else if (op == ToneMappingOperator::FATTAL) {
                params.alpha = 0.1;
                params.beta = 0.8;
            }

            auto tone_mapped = enhancer.toneMapping(hdr_image, op, params);

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "\n" << name << ": " << duration.count() << " μs\n";
            displayHistogramStats(tone_mapped, "Tone Mapped");
        }

        // Parameter sensitivity analysis for Reinhard
        std::cout << "\nReinhard parameter sensitivity:\n";
        std::vector<double> key_values = {0.09, 0.18, 0.36, 0.72};

        for (double key : key_values) {
            EnhancementParams params;
            params.key = key;
            params.white = 1.0;

            auto start = high_resolution_clock::now();
            auto tone_mapped = enhancer.toneMapping(
                hdr_image, ToneMappingOperator::REINHARD, params);
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "  Key value " << key << ": " << duration.count()
                      << " μs\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in tone mapping: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate color correction techniques
 */
void demonstrateColorCorrection() {
    std::cout << "\n=== Color Correction ===\n";

    try {
        // Create image with color cast
        blob<uint8_t> color_cast_image(200, 300, 3);

        for (int y = 0; y < color_cast_image.rows(); ++y) {
            for (int x = 0; x < color_cast_image.cols(); ++x) {
                uint8_t base_r =
                    static_cast<uint8_t>((x * 255) / color_cast_image.cols());
                uint8_t base_g =
                    static_cast<uint8_t>((y * 255) / color_cast_image.rows());
                uint8_t base_b = static_cast<uint8_t>(
                    ((x + y) * 255) /
                    (color_cast_image.cols() + color_cast_image.rows()));

                // Add blue color cast (simulating tungsten lighting)
                color_cast_image.at(y, x, 0) =
                    static_cast<uint8_t>(std::clamp(base_r * 0.8, 0.0, 255.0));
                color_cast_image.at(y, x, 1) =
                    static_cast<uint8_t>(std::clamp(base_g * 0.9, 0.0, 255.0));
                color_cast_image.at(y, x, 2) =
                    static_cast<uint8_t>(std::clamp(base_b * 1.3, 0.0, 255.0));
            }
        }

        std::cout << "Created color cast test image: "
                  << color_cast_image.cols() << "x" << color_cast_image.rows()
                  << "\n";

        ImageEnhancement enhancer;

        // Test different color correction methods
        std::vector<std::pair<ColorCorrectionMethod, std::string>> methods = {
            {ColorCorrectionMethod::AUTO_COLOR, "Auto Color Correction"},
            {ColorCorrectionMethod::WHITE_BALANCE, "White Balance"},
            {ColorCorrectionMethod::GRAY_WORLD, "Gray World Assumption"},
            {ColorCorrectionMethod::PERFECT_REFLECTOR, "Perfect Reflector"},
            {ColorCorrectionMethod::COLOR_CONSTANCY, "Color Constancy"}};

        for (const auto& [method, name] : methods) {
            auto start = high_resolution_clock::now();

            EnhancementParams params;
            if (method == ColorCorrectionMethod::WHITE_BALANCE) {
                params.temperature = 3200;  // Tungsten temperature
                params.tint = 0;
            }

            auto corrected =
                enhancer.colorCorrection(color_cast_image, method, params);

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "  " << name << ": " << duration.count() << " μs\n";
        }

        // White balance temperature adjustment
        std::cout << "\nWhite balance temperature adjustment:\n";
        std::vector<int> temperatures = {2700, 3200, 4000, 5500, 6500, 7500};

        for (int temp : temperatures) {
            EnhancementParams params;
            params.temperature = temp;
            params.tint = 0;

            auto start = high_resolution_clock::now();
            auto corrected = enhancer.colorCorrection(
                color_cast_image, ColorCorrectionMethod::WHITE_BALANCE, params);
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::string light_type;
            if (temp < 3000)
                light_type = "Tungsten";
            else if (temp < 4000)
                light_type = "Warm White";
            else if (temp < 5000)
                light_type = "Neutral";
            else if (temp < 6000)
                light_type = "Cool White";
            else
                light_type = "Daylight";

            std::cout << "  " << temp << "K (" << light_type
                      << "): " << duration.count() << " μs\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in color correction: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate brightness and contrast adjustment
 */
void demonstrateBrightnessContrast() {
    std::cout << "\n=== Brightness and Contrast Adjustment ===\n";

    try {
        auto original = createEnhancementTestImage();
        ImageEnhancement enhancer;

        // Test different brightness and contrast combinations
        std::vector<std::tuple<double, double, std::string>> adjustments = {
            {20, 1.2, "Brighter, More Contrast"},
            {-20, 0.8, "Darker, Less Contrast"},
            {0, 1.5, "Same Brightness, High Contrast"},
            {30, 1.0, "Brighter, Same Contrast"},
            {-10, 1.3, "Slightly Darker, More Contrast"}};

        for (const auto& [brightness, contrast, description] : adjustments) {
            auto start = high_resolution_clock::now();

            auto adjusted = enhancer.adjustBrightnessContrast(
                original, brightness, contrast, true);

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "  " << description << " (B:" << brightness
                      << ", C:" << contrast << "): " << duration.count()
                      << " μs\n";
            displayHistogramStats(adjusted, "Adjusted");
        }

        // Gamma correction
        std::cout << "\nGamma correction:\n";
        std::vector<double> gamma_values = {0.5, 0.8, 1.0, 1.2, 1.8, 2.2};

        for (double gamma : gamma_values) {
            auto start = high_resolution_clock::now();
            auto corrected =
                enhancer.gammaCorrection(original, gamma, ColorSpace::RGB);
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::string effect;
            if (gamma < 1.0)
                effect = "Brighter midtones";
            else if (gamma > 1.0)
                effect = "Darker midtones";
            else
                effect = "No change";

            std::cout << "  Gamma " << gamma << " (" << effect
                      << "): " << duration.count() << " μs\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in brightness/contrast adjustment: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrate saturation and vibrance adjustment
 */
void demonstrateSaturationVibrance() {
    std::cout << "\n=== Saturation and Vibrance Adjustment ===\n";

    try {
        auto original = createEnhancementTestImage();
        ImageEnhancement enhancer;

        // Test saturation adjustment
        std::cout << "Saturation adjustment:\n";
        std::vector<double> saturation_values = {-50, -25, 0, 25, 50, 100};

        for (double saturation : saturation_values) {
            auto start = high_resolution_clock::now();
            auto adjusted =
                enhancer.adjustSaturation(original, saturation, true);
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::string effect;
            if (saturation < 0)
                effect = "Desaturated";
            else if (saturation > 0)
                effect = "More saturated";
            else
                effect = "No change";

            std::cout << "  Saturation " << std::showpos << saturation
                      << std::noshowpos << " (" << effect
                      << "): " << duration.count() << " μs\n";
        }

        // Test vibrance adjustment
        std::cout << "\nVibrance adjustment:\n";
        std::vector<double> vibrance_values = {-30, -15, 0, 15, 30, 50};

        for (double vibrance : vibrance_values) {
            auto start = high_resolution_clock::now();
            auto adjusted = enhancer.adjustVibrance(original, vibrance);
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "  Vibrance " << std::showpos << vibrance
                      << std::noshowpos << ": " << duration.count() << " μs\n";
        }

        std::cout << "\nSaturation vs Vibrance:\n";
        std::cout << "  Saturation: Affects all colors equally\n";
        std::cout
            << "  Vibrance: Protects skin tones, affects muted colors more\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in saturation/vibrance adjustment: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrate automatic enhancement
 */
void demonstrateAutoEnhancement() {
    std::cout << "\n=== Automatic Enhancement ===\n";

    try {
        auto original = createEnhancementTestImage();
        ImageEnhancement enhancer;

        // Test different auto enhancement modes
        std::vector<std::pair<std::string, std::string>> modes = {
            {"auto", "General Auto Enhancement"},
            {"portrait", "Portrait Optimization"},
            {"landscape", "Landscape Optimization"},
            {"night", "Night Scene Enhancement"},
            {"macro", "Macro Photography"},
            {"sports", "Sports/Action Enhancement"}};

        for (const auto& [mode, description] : modes) {
            auto start = high_resolution_clock::now();

            auto enhanced = enhancer.autoEnhance(original, mode, 0.8);

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "  " << description << ": " << duration.count()
                      << " μs\n";
            displayHistogramStats(enhanced, mode + " enhanced");
        }

        // Test different enhancement strengths
        std::cout << "\nEnhancement strength comparison:\n";
        std::vector<double> strengths = {0.2, 0.5, 0.8, 1.0};

        for (double strength : strengths) {
            auto start = high_resolution_clock::now();
            auto enhanced = enhancer.autoEnhance(original, "auto", strength);
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "  Strength " << strength << ": " << duration.count()
                      << " μs\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in auto enhancement: " << e.what() << "\n";
    }
}

int main() {
    std::cout << "=== Atom Image Enhancement Example ===\n";
    std::cout
        << "This example demonstrates advanced image enhancement techniques\n";

    // Run all demonstrations
    demonstrateHistogramEqualization();
    demonstrateToneMapping();
    demonstrateColorCorrection();
    demonstrateBrightnessContrast();
    demonstrateSaturationVibrance();
    demonstrateAutoEnhancement();

    std::cout << "\n=== Image enhancement example completed ===\n";
    return 0;
}
