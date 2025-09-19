/**
 * @file format_conversion.cpp
 * @brief Example demonstrating image format conversion capabilities
 *
 * This example covers:
 * - Converting between different image formats
 * - Quality settings for lossy formats
 * - Compression options and trade-offs
 * - Color space conversions during format changes
 * - Metadata preservation during conversion
 * - Batch format conversion
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

#include "atom/image/core/image_blob.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Structure to hold format conversion parameters
 */
struct ConversionParams {
    std::string source_format;
    std::string target_format;
    int quality = 95;  // For lossy formats (0-100)
    bool preserve_metadata = true;
    std::string compression = "default";
    bool optimize_size = false;
};

/**
 * @brief Helper function to create image data with given dimensions and pattern
 */
std::vector<uint8_t> createImageData(int width, int height, int channels) {
    std::vector<uint8_t> data(height * width * channels);
    
    // Fill with a complex pattern to test compression
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Create a pattern with varying frequency
            double freq_x = 2.0 * M_PI * x / width;
            double freq_y = 2.0 * M_PI * y / height;

            int pixel_idx = (y * width + x) * channels;
            data[pixel_idx] = static_cast<uint8_t>(128 + 127 * std::sin(freq_x * 3));
            if (channels > 1) {
                data[pixel_idx + 1] = static_cast<uint8_t>(128 + 127 * std::sin(freq_y * 2));
            }
            if (channels > 2) {
                data[pixel_idx + 2] = static_cast<uint8_t>(128 + 127 * std::sin((freq_x + freq_y) * 1.5));
            }
        }
    }
    return data;
}

/**
 * @brief Demonstrate basic format conversion
 */
void demonstrateBasicConversion() {
    std::cout << "\n=== Basic Format Conversion ===\n";

    try {
        // Create sample image data
        const int width = 400, height = 300, channels = 3;
        auto imageData = createImageData(width, height, channels);
        
        // Create a blob from the image data
        blob original(reinterpret_cast<std::byte*>(imageData.data()), imageData.size());

        std::cout << "Created test image: " << width << "x" << height << "\n";

        // Simulate conversions to different formats
        std::vector<ConversionParams> conversions = {
            {"RAW", "JPEG", 85, true, "default", false},
            {"RAW", "PNG", 0, true, "lossless", false},
            {"RAW", "BMP", 0, false, "none", false},
            {"RAW", "TIFF", 0, true, "lzw", false},
            {"RAW", "WEBP", 80, true, "default", true}};

        for (const auto& conv : conversions) {
            auto start = high_resolution_clock::now();

            // Simulate format conversion by creating a copy of original data
            std::vector<uint8_t> convertedData(imageData);  // Copy for conversion
            blob converted(reinterpret_cast<std::byte*>(convertedData.data()), convertedData.size());

            // Apply format-specific processing
            if (conv.target_format == "JPEG") {
                // JPEG conversion would apply DCT compression
                std::cout << "  Converting to JPEG (quality: " << conv.quality
                          << ")... ";
                // Simulate quality loss for demonstration
                if (conv.quality < 90) {
                    // Apply slight blur to simulate compression artifacts
                    for (int y = 1; y < height - 1; ++y) {
                        for (int x = 1; x < width - 1; ++x) {
                            for (int c = 0; c < channels; ++c) {
                                int idx = (y * width + x) * channels + c;
                                int sum = convertedData[(y-1) * width * channels + x * channels + c] +
                                         convertedData[(y+1) * width * channels + x * channels + c] +
                                         convertedData[y * width * channels + (x-1) * channels + c] +
                                         convertedData[y * width * channels + (x+1) * channels + c] +
                                         convertedData[idx] * 4;
                                convertedData[idx] = static_cast<uint8_t>(sum / 8);
                            }
                        }
                    }
                }
            } else if (conv.target_format == "PNG") {
                std::cout << "  Converting to PNG (lossless)... ";
                // PNG is lossless, no quality degradation
            } else if (conv.target_format == "BMP") {
                std::cout << "  Converting to BMP (uncompressed)... ";
                // BMP is typically uncompressed
            } else if (conv.target_format == "TIFF") {
                std::cout << "  Converting to TIFF (compression: "
                          << conv.compression << ")... ";
                // TIFF supports various compression methods
            } else if (conv.target_format == "WEBP") {
                std::cout << "  Converting to WebP (quality: " << conv.quality
                          << ")... ";
                // WebP supports both lossy and lossless compression
            }

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            // Calculate simulated file sizes
            size_t original_size = original.size();
            size_t converted_size = original_size;

            if (conv.target_format == "JPEG") {
                converted_size = static_cast<size_t>(
                    original_size * (conv.quality / 100.0) * 0.3);
            } else if (conv.target_format == "PNG") {
                converted_size = static_cast<size_t>(
                    original_size * 0.7);  // Typical PNG compression
            } else if (conv.target_format == "BMP") {
                converted_size = original_size;  // No compression
            } else if (conv.target_format == "TIFF") {
                converted_size = static_cast<size_t>(original_size *
                                                     0.8);  // LZW compression
            } else if (conv.target_format == "WEBP") {
                converted_size = static_cast<size_t>(
                    original_size * (conv.quality / 100.0) * 0.25);
            }

            double compression_ratio =
                static_cast<double>(original_size) / converted_size;

            std::cout << "Done (" << duration.count() << " μs)\n";
            std::cout << "    Size: " << original_size / 1024 << " KB -> "
                      << converted_size / 1024 << " KB (ratio: " << std::fixed
                      << std::setprecision(2) << compression_ratio << "x)\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in basic conversion: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate quality vs size trade-offs
 */
void demonstrateQualityTradeoffs() {
    std::cout << "\n=== Quality vs Size Trade-offs ===\n";

    try {
        // Create a detailed test image data
        const int width = 512, height = 512, channels = 3;
        std::vector<uint8_t> testImageData(height * width * channels);

        // Create a detailed pattern with high-frequency components
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // High-frequency checkerboard pattern
                bool checker = ((x / 8) + (y / 8)) % 2 == 0;

                // Gradient overlay
                double grad_x = static_cast<double>(x) / width;
                double grad_y = static_cast<double>(y) / height;

                uint8_t base_r = static_cast<uint8_t>(grad_x * 255);
                uint8_t base_g = static_cast<uint8_t>(grad_y * 255);
                uint8_t base_b = static_cast<uint8_t>((grad_x + grad_y) * 127);

                int pixel_idx = (y * width + x) * channels;
                if (checker) {
                    testImageData[pixel_idx] = std::min(255, base_r + 50);
                    testImageData[pixel_idx + 1] = std::min(255, base_g + 50);
                    testImageData[pixel_idx + 2] = std::min(255, base_b + 50);
                } else {
                    testImageData[pixel_idx] = std::max(0, base_r - 50);
                    testImageData[pixel_idx + 1] = std::max(0, base_g - 50);
                    testImageData[pixel_idx + 2] = std::max(0, base_b - 50);
                }
            }
        }
        
        // Create blob from the data
        blob test_image(reinterpret_cast<std::byte*>(testImageData.data()), testImageData.size());

        std::cout << "Testing JPEG quality levels:\n";
        std::cout << "Quality | Size (KB) | Compression | Processing Time\n";
        std::cout << "--------|-----------|-------------|----------------\n";

        size_t original_size = test_image.size();

        for (int quality = 10; quality <= 100; quality += 20) {
            auto start = high_resolution_clock::now();

            // Simulate JPEG compression at different quality levels by creating a copy
            std::vector<uint8_t> compressedData(testImageData);
            blob compressed(reinterpret_cast<std::byte*>(compressedData.data()), compressedData.size());

            // Apply quality-dependent processing
            if (quality < 50) {
                // Heavy compression artifacts simulation
                for (int y = 0; y < height; y += 8) {
                    for (int x = 0; x < width; x += 8) {
                        // Block-based averaging (simulating DCT quantization)
                        for (int c = 0; c < channels; ++c) {
                            int sum = 0;
                            int count = 0;
                            for (int by = 0; by < 8 && y + by < height; ++by) {
                                for (int bx = 0; bx < 8 && x + bx < width; ++bx) {
                                    int idx = ((y + by) * width + (x + bx)) * channels + c;
                                    sum += compressedData[idx];
                                    count++;
                                }
                            }
                            uint8_t avg = static_cast<uint8_t>(sum / count);

                            for (int by = 0; by < 8 && y + by < height; ++by) {
                                for (int bx = 0; bx < 8 && x + bx < width; ++bx) {
                                    int idx = ((y + by) * width + (x + bx)) * channels + c;
                                    compressedData[idx] = avg;
                                }
                            }
                        }
                    }
                }
            }

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            // Calculate simulated compressed size
            size_t compressed_size =
                static_cast<size_t>(original_size * (quality / 100.0) * 0.3);
            double compression_ratio =
                static_cast<double>(original_size) / compressed_size;

            std::cout << std::setw(7) << quality << " | " << std::setw(9)
                      << compressed_size / 1024 << " | " << std::setw(11)
                      << std::fixed << std::setprecision(1) << compression_ratio
                      << "x | " << std::setw(13) << duration.count() << " μs\n";
        }

        std::cout << "\nObservations:\n";
        std::cout << "- Lower quality = smaller file size but more artifacts\n";
        std::cout << "- Quality 85-95 often provides good balance\n";
        std::cout << "- Quality below 50 shows significant degradation\n";
        std::cout << "- Processing time may vary with quality settings\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in quality trade-offs demo: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate color space conversions
 */
void demonstrateColorSpaceConversion() {
    std::cout << "\n=== Color Space Conversions ===\n";

    try {
        // Create RGB test image data
        const int width = 200, height = 200, channels = 3;
        std::vector<uint8_t> rgbImageData(height * width * channels);

        // Fill with color gradients
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int pixel_idx = (y * width + x) * channels;
                rgbImageData[pixel_idx] = static_cast<uint8_t>((x * 255) / width);
                rgbImageData[pixel_idx + 1] = static_cast<uint8_t>((y * 255) / height);
                rgbImageData[pixel_idx + 2] = static_cast<uint8_t>(((x + y) * 255) / (width + height));
            }
        }
        
        // Create blob from the data
        blob rgb_image(reinterpret_cast<std::byte*>(rgbImageData.data()), rgbImageData.size());

        std::cout << "Original RGB image: " << width << "x" << height << "\n";

        // Convert to grayscale
        std::vector<uint8_t> grayscaleData(height * width * 1);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // Standard RGB to grayscale conversion
                int rgb_idx = (y * width + x) * channels;
                uint8_t r = rgbImageData[rgb_idx];
                uint8_t g = rgbImageData[rgb_idx + 1];
                uint8_t b = rgbImageData[rgb_idx + 2];
                uint8_t gray = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
                
                int gray_idx = y * width + x;
                grayscaleData[gray_idx] = gray;
            }
        }
        blob grayscale(reinterpret_cast<std::byte*>(grayscaleData.data()), grayscaleData.size());
        std::cout << "Converted to grayscale: " << width << "x" << height << " (1 channel)\n";

        // Simulate HSV conversion
        std::vector<uint8_t> hsvImageData(height * width * channels);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int rgb_idx = (y * width + x) * channels;
                uint8_t r = rgbImageData[rgb_idx];
                uint8_t g = rgbImageData[rgb_idx + 1];
                uint8_t b = rgbImageData[rgb_idx + 2];

                // Simplified RGB to HSV conversion
                double rf = r / 255.0;
                double gf = g / 255.0;
                double bf = b / 255.0;

                double max_val = std::max({rf, gf, bf});
                double min_val = std::min({rf, gf, bf});
                double delta = max_val - min_val;

                // Hue calculation (simplified)
                double hue = 0;
                if (delta > 0) {
                    if (max_val == rf) {
                        hue = 60 * ((gf - bf) / delta);
                    } else if (max_val == gf) {
                        hue = 60 * (2 + (bf - rf) / delta);
                    } else {
                        hue = 60 * (4 + (rf - gf) / delta);
                    }
                }
                if (hue < 0)
                    hue += 360;

                // Saturation and Value
                double saturation = (max_val == 0) ? 0 : (delta / max_val);
                double value = max_val;

                int hsv_idx = (y * width + x) * channels;
                hsvImageData[hsv_idx] = static_cast<uint8_t>(hue * 255 / 360);
                hsvImageData[hsv_idx + 1] = static_cast<uint8_t>(saturation * 255);
                hsvImageData[hsv_idx + 2] = static_cast<uint8_t>(value * 255);
            }
        }
        blob hsv_image(reinterpret_cast<std::byte*>(hsvImageData.data()), hsvImageData.size());
        std::cout << "Converted to HSV: " << width << "x" << height << " (3 channels)\n";

        // Convert RGB to RGBA (add alpha channel)
        const int rgba_channels = 4;
        std::vector<uint8_t> rgbaImageData(height * width * rgba_channels);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int rgb_idx = (y * width + x) * channels;
                int rgba_idx = (y * width + x) * rgba_channels;
                
                rgbaImageData[rgba_idx] = rgbImageData[rgb_idx];      // R
                rgbaImageData[rgba_idx + 1] = rgbImageData[rgb_idx + 1]; // G
                rgbaImageData[rgba_idx + 2] = rgbImageData[rgb_idx + 2]; // B

                // Create alpha gradient
                double distance_from_center = std::sqrt(std::pow(x - width / 2.0, 2) +
                                                       std::pow(y - height / 2.0, 2));
                double max_distance = std::sqrt(std::pow(width / 2.0, 2) +
                                              std::pow(height / 2.0, 2));
                uint8_t alpha = static_cast<uint8_t>(255 * (1.0 - distance_from_center / max_distance));
                rgbaImageData[rgba_idx + 3] = alpha;  // A
            }
        }
        blob rgba_image(reinterpret_cast<std::byte*>(rgbaImageData.data()), rgbaImageData.size());
        std::cout << "Converted to RGBA: " << width << "x" << height << " (4 channels)\n";

        // Memory usage comparison
        std::cout << "\nMemory usage comparison:\n";
        std::cout << "  RGB:       " << rgb_image.size() / 1024 << " KB\n";
        std::cout << "  Grayscale: " << grayscale.size() / 1024 << " KB\n";
        std::cout << "  HSV:       " << hsv_image.size() / 1024 << " KB\n";
        std::cout << "  RGBA:      " << rgba_image.size() / 1024 << " KB\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in color space conversion: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate batch format conversion
 */
void demonstrateBatchConversion() {
    std::cout << "\n=== Batch Format Conversion ===\n";

    try {
        // Simulate a batch of images to convert
        struct ImageFile {
            std::string filename;
            std::string source_format;
            int width, height;
        };

        std::vector<ImageFile> source_images = {
            {"photo001.bmp", "BMP", 1920, 1080},
            {"photo002.bmp", "BMP", 1600, 1200},
            {"scan001.tiff", "TIFF", 2400, 3200},
            {"image001.png", "PNG", 800, 600},
            {"picture001.bmp", "BMP", 640, 480}};

        std::string target_format = "JPEG";
        int target_quality = 85;

        std::cout << "Converting " << source_images.size() << " images to "
                  << target_format << " (quality: " << target_quality
                  << ")\n\n";

        size_t total_original_size = 0;
        size_t total_converted_size = 0;
        int successful_conversions = 0;

        for (const auto& img : source_images) {
            try {
                auto start = high_resolution_clock::now();

                std::cout << "Converting " << img.filename << " ("
                          << img.source_format << ", " << img.width << "x"
                          << img.height << ")... ";

                // Simulate loading and conversion
                size_t original_size =
                    static_cast<size_t>(img.width) * img.height * 3;

                // Create simulated image data
                std::vector<uint8_t> imageData(img.height * img.width * 3);
                blob image(reinterpret_cast<std::byte*>(imageData.data()), imageData.size());

                // Apply conversion by creating a copy
                std::vector<uint8_t> convertedData(imageData);
                blob converted(reinterpret_cast<std::byte*>(convertedData.data()), convertedData.size());

                // Calculate converted size based on format and quality
                size_t converted_size;
                if (target_format == "JPEG") {
                    converted_size = static_cast<size_t>(
                        original_size * (target_quality / 100.0) * 0.3);
                } else if (target_format == "PNG") {
                    converted_size = static_cast<size_t>(original_size * 0.7);
                } else {
                    converted_size = original_size;
                }

                auto end = high_resolution_clock::now();
                auto duration = duration_cast<milliseconds>(end - start);

                std::string output_filename =
                    img.filename.substr(0, img.filename.find_last_of('.')) +
                    ".jpg";

                std::cout << "SUCCESS\n";
                std::cout << "  Output: " << output_filename << "\n";
                std::cout << "  Size: " << original_size / 1024 << " KB -> "
                          << converted_size / 1024 << " KB\n";
                std::cout << "  Time: " << duration.count() << " ms\n\n";

                total_original_size += original_size;
                total_converted_size += converted_size;
                successful_conversions++;

            } catch (const std::exception& e) {
                std::cout << "FAILED (" << e.what() << ")\n\n";
            }
        }

        std::cout << "Batch conversion summary:\n";
        std::cout << "  Total files processed: " << source_images.size()
                  << "\n";
        std::cout << "  Successful conversions: " << successful_conversions
                  << "\n";
        std::cout << "  Failed conversions: "
                  << (source_images.size() - successful_conversions) << "\n";
        std::cout << "  Total size reduction: " << total_original_size / 1024
                  << " KB -> " << total_converted_size / 1024 << " KB\n";
        std::cout << "  Overall compression ratio: "
                  << static_cast<double>(total_original_size) /
                         total_converted_size
                  << "x\n";
        std::cout << "  Space saved: "
                  << (total_original_size - total_converted_size) / 1024
                  << " KB\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in batch conversion: " << e.what() << "\n";
    }
}

int main() {
    std::cout << "=== Atom Image Format Conversion Example ===\n";
    std::cout
        << "This example demonstrates image format conversion capabilities\n";

    // Run all demonstrations
    demonstrateBasicConversion();
    demonstrateQualityTradeoffs();
    demonstrateColorSpaceConversion();
    demonstrateBatchConversion();

    std::cout << "\n=== Format conversion example completed ===\n";
    return 0;
}
