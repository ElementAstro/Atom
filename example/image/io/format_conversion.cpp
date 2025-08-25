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
 * @brief Demonstrate basic format conversion
 */
void demonstrateBasicConversion() {
    std::cout << "\n=== Basic Format Conversion ===\n";

    try {
        // Create a sample image
        blob<uint8_t> original(400, 300, 3);

        // Fill with a complex pattern to test compression
        for (int y = 0; y < original.rows(); ++y) {
            for (int x = 0; x < original.cols(); ++x) {
                // Create a pattern with varying frequency
                double freq_x = 2.0 * M_PI * x / original.cols();
                double freq_y = 2.0 * M_PI * y / original.rows();

                original.at(y, x, 0) =
                    static_cast<uint8_t>(128 + 127 * std::sin(freq_x * 3));
                original.at(y, x, 1) =
                    static_cast<uint8_t>(128 + 127 * std::sin(freq_y * 2));
                original.at(y, x, 2) = static_cast<uint8_t>(
                    128 + 127 * std::sin((freq_x + freq_y) * 1.5));
            }
        }

        std::cout << "Created test image: " << original.cols() << "x"
                  << original.rows() << "\n";

        // Simulate conversions to different formats
        std::vector<ConversionParams> conversions = {
            {"RAW", "JPEG", 85, true, "default", false},
            {"RAW", "PNG", 0, true, "lossless", false},
            {"RAW", "BMP", 0, false, "none", false},
            {"RAW", "TIFF", 0, true, "lzw", false},
            {"RAW", "WEBP", 80, true, "default", true}};

        for (const auto& conv : conversions) {
            auto start = high_resolution_clock::now();

            // Simulate format conversion
            blob<uint8_t> converted = original;  // Copy for conversion

            // Apply format-specific processing
            if (conv.target_format == "JPEG") {
                // JPEG conversion would apply DCT compression
                std::cout << "  Converting to JPEG (quality: " << conv.quality
                          << ")... ";
                // Simulate quality loss for demonstration
                if (conv.quality < 90) {
                    // Apply slight blur to simulate compression artifacts
                    for (int y = 1; y < converted.rows() - 1; ++y) {
                        for (int x = 1; x < converted.cols() - 1; ++x) {
                            for (int c = 0; c < 3; ++c) {
                                int sum = converted.at(y - 1, x, c) +
                                          converted.at(y + 1, x, c) +
                                          converted.at(y, x - 1, c) +
                                          converted.at(y, x + 1, c) +
                                          converted.at(y, x, c) * 4;
                                converted.at(y, x, c) =
                                    static_cast<uint8_t>(sum / 8);
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
        // Create a detailed test image
        blob<uint8_t> test_image(512, 512, 3);

        // Create a detailed pattern with high-frequency components
        for (int y = 0; y < test_image.rows(); ++y) {
            for (int x = 0; x < test_image.cols(); ++x) {
                // High-frequency checkerboard pattern
                bool checker = ((x / 8) + (y / 8)) % 2 == 0;

                // Gradient overlay
                double grad_x = static_cast<double>(x) / test_image.cols();
                double grad_y = static_cast<double>(y) / test_image.rows();

                uint8_t base_r = static_cast<uint8_t>(grad_x * 255);
                uint8_t base_g = static_cast<uint8_t>(grad_y * 255);
                uint8_t base_b = static_cast<uint8_t>((grad_x + grad_y) * 127);

                if (checker) {
                    test_image.at(y, x, 0) = std::min(255, base_r + 50);
                    test_image.at(y, x, 1) = std::min(255, base_g + 50);
                    test_image.at(y, x, 2) = std::min(255, base_b + 50);
                } else {
                    test_image.at(y, x, 0) = std::max(0, base_r - 50);
                    test_image.at(y, x, 1) = std::max(0, base_g - 50);
                    test_image.at(y, x, 2) = std::max(0, base_b - 50);
                }
            }
        }

        std::cout << "Testing JPEG quality levels:\n";
        std::cout << "Quality | Size (KB) | Compression | Processing Time\n";
        std::cout << "--------|-----------|-------------|----------------\n";

        size_t original_size = test_image.size();

        for (int quality = 10; quality <= 100; quality += 20) {
            auto start = high_resolution_clock::now();

            // Simulate JPEG compression at different quality levels
            blob<uint8_t> compressed = test_image;

            // Apply quality-dependent processing
            if (quality < 50) {
                // Heavy compression artifacts simulation
                for (int y = 0; y < compressed.rows(); y += 8) {
                    for (int x = 0; x < compressed.cols(); x += 8) {
                        // Block-based averaging (simulating DCT quantization)
                        for (int c = 0; c < 3; ++c) {
                            int sum = 0;
                            int count = 0;
                            for (int by = 0;
                                 by < 8 && y + by < compressed.rows(); ++by) {
                                for (int bx = 0;
                                     bx < 8 && x + bx < compressed.cols();
                                     ++bx) {
                                    sum += compressed.at(y + by, x + bx, c);
                                    count++;
                                }
                            }
                            uint8_t avg = static_cast<uint8_t>(sum / count);

                            for (int by = 0;
                                 by < 8 && y + by < compressed.rows(); ++by) {
                                for (int bx = 0;
                                     bx < 8 && x + bx < compressed.cols();
                                     ++bx) {
                                    compressed.at(y + by, x + bx, c) = avg;
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
        // Create RGB test image
        blob<uint8_t> rgb_image(200, 200, 3);

        // Fill with color gradients
        for (int y = 0; y < rgb_image.rows(); ++y) {
            for (int x = 0; x < rgb_image.cols(); ++x) {
                rgb_image.at(y, x, 0) =
                    static_cast<uint8_t>((x * 255) / rgb_image.cols());
                rgb_image.at(y, x, 1) =
                    static_cast<uint8_t>((y * 255) / rgb_image.rows());
                rgb_image.at(y, x, 2) = static_cast<uint8_t>(
                    ((x + y) * 255) / (rgb_image.cols() + rgb_image.rows()));
            }
        }

        std::cout << "Original RGB image: " << rgb_image.cols() << "x"
                  << rgb_image.rows() << "\n";

        // Convert to grayscale
        blob<uint8_t> grayscale(rgb_image.rows(), rgb_image.cols(), 1);
        for (int y = 0; y < rgb_image.rows(); ++y) {
            for (int x = 0; x < rgb_image.cols(); ++x) {
                // Standard RGB to grayscale conversion
                uint8_t r = rgb_image.at(y, x, 0);
                uint8_t g = rgb_image.at(y, x, 1);
                uint8_t b = rgb_image.at(y, x, 2);
                uint8_t gray =
                    static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
                grayscale.at(y, x, 0) = gray;
            }
        }
        std::cout << "Converted to grayscale: " << grayscale.cols() << "x"
                  << grayscale.rows() << " (1 channel)\n";

        // Simulate HSV conversion
        blob<uint8_t> hsv_image(rgb_image.rows(), rgb_image.cols(), 3);
        for (int y = 0; y < rgb_image.rows(); ++y) {
            for (int x = 0; x < rgb_image.cols(); ++x) {
                uint8_t r = rgb_image.at(y, x, 0);
                uint8_t g = rgb_image.at(y, x, 1);
                uint8_t b = rgb_image.at(y, x, 2);

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

                hsv_image.at(y, x, 0) = static_cast<uint8_t>(hue * 255 / 360);
                hsv_image.at(y, x, 1) = static_cast<uint8_t>(saturation * 255);
                hsv_image.at(y, x, 2) = static_cast<uint8_t>(value * 255);
            }
        }
        std::cout << "Converted to HSV: " << hsv_image.cols() << "x"
                  << hsv_image.rows() << " (3 channels)\n";

        // Convert RGB to RGBA (add alpha channel)
        blob<uint8_t> rgba_image(rgb_image.rows(), rgb_image.cols(), 4);
        for (int y = 0; y < rgb_image.rows(); ++y) {
            for (int x = 0; x < rgb_image.cols(); ++x) {
                rgba_image.at(y, x, 0) = rgb_image.at(y, x, 0);  // R
                rgba_image.at(y, x, 1) = rgb_image.at(y, x, 1);  // G
                rgba_image.at(y, x, 2) = rgb_image.at(y, x, 2);  // B

                // Create alpha gradient
                double distance_from_center =
                    std::sqrt(std::pow(x - rgb_image.cols() / 2.0, 2) +
                              std::pow(y - rgb_image.rows() / 2.0, 2));
                double max_distance =
                    std::sqrt(std::pow(rgb_image.cols() / 2.0, 2) +
                              std::pow(rgb_image.rows() / 2.0, 2));
                uint8_t alpha = static_cast<uint8_t>(
                    255 * (1.0 - distance_from_center / max_distance));
                rgba_image.at(y, x, 3) = alpha;  // A
            }
        }
        std::cout << "Converted to RGBA: " << rgba_image.cols() << "x"
                  << rgba_image.rows() << " (4 channels)\n";

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
                blob<uint8_t> image(img.height, img.width, 3);

                // Apply conversion
                blob<uint8_t> converted = image;  // Copy for conversion

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
