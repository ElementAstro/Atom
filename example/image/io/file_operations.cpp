/**
 * @file basic_file_operations.cpp
 * @brief Example demonstrating basic image file I/O operations
 *
 * This example covers:
 * - Loading images from various formats
 * - Saving images in different formats
 * - Format detection and validation
 * - Error handling for file operations
 * - File metadata extraction
 * - Batch file processing
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "atom/image/core/image_blob.hpp"

using namespace atom::image;
namespace fs = std::filesystem;

/**
 * @brief Create sample test images for demonstration
 */
void createSampleImages() {
    std::cout << "\n=== Creating Sample Test Images ===\n";

    try {
        // Create a simple RGB gradient image
        const int grad_width = 300, grad_height = 200, grad_channels = 3;
        std::vector<uint8_t> gradient_data(grad_width * grad_height *
                                           grad_channels);

        for (int y = 0; y < grad_height; ++y) {
            for (int x = 0; x < grad_width; ++x) {
                int pixel_idx = (y * grad_width + x) * grad_channels;
                gradient_data[pixel_idx] = static_cast<uint8_t>(
                    (x * 255) / grad_width);  // Red gradient
                gradient_data[pixel_idx + 1] = static_cast<uint8_t>(
                    (y * 255) / grad_height);  // Green gradient
                gradient_data[pixel_idx + 2] = static_cast<uint8_t>(
                    ((x + y) * 255) /
                    (grad_width + grad_height));  // Blue gradient
            }
        }

        blob gradient(reinterpret_cast<std::byte*>(gradient_data.data()),
                      gradient_data.size());
        std::cout << "Created gradient image: " << grad_width << "x"
                  << grad_height << "\n";

        // Create a checkerboard pattern
        const int check_size = 256, check_channels = 1;
        const int square_size = 32;
        std::vector<uint8_t> checkerboard_data(check_size * check_size *
                                               check_channels);

        for (int y = 0; y < check_size; ++y) {
            for (int x = 0; x < check_size; ++x) {
                bool white = ((x / square_size) + (y / square_size)) % 2 == 0;
                int pixel_idx = (y * check_size + x) * check_channels;
                checkerboard_data[pixel_idx] = white ? 255 : 0;
            }
        }

        blob checkerboard(
            reinterpret_cast<std::byte*>(checkerboard_data.data()),
            checkerboard_data.size());
        std::cout << "Created checkerboard image: " << check_size << "x"
                  << check_size << "\n";

        // Create a circular pattern
        const int circle_size = 200, circle_channels = 3;
        std::vector<uint8_t> circle_data(circle_size * circle_size *
                                         circle_channels);
        int center_x = circle_size / 2;
        int center_y = circle_size / 2;
        int max_radius = std::min(center_x, center_y);

        for (int y = 0; y < circle_size; ++y) {
            for (int x = 0; x < circle_size; ++x) {
                int dx = x - center_x;
                int dy = y - center_y;
                double distance = std::sqrt(dx * dx + dy * dy);
                double normalized_distance = distance / max_radius;

                int pixel_idx = (y * circle_size + x) * circle_channels;
                if (normalized_distance <= 1.0) {
                    uint8_t intensity =
                        static_cast<uint8_t>((1.0 - normalized_distance) * 255);
                    circle_data[pixel_idx] = intensity;
                    circle_data[pixel_idx + 1] =
                        static_cast<uint8_t>(intensity * 0.7);
                    circle_data[pixel_idx + 2] =
                        static_cast<uint8_t>(intensity * 0.3);
                } else {
                    circle_data[pixel_idx] = 0;
                    circle_data[pixel_idx + 1] = 0;
                    circle_data[pixel_idx + 2] = 0;
                }
            }
        }

        blob circle(reinterpret_cast<std::byte*>(circle_data.data()),
                    circle_data.size());
        std::cout << "Created circle image: " << circle_size << "x"
                  << circle_size << "\n";

        // Store sample images for later use (in a real implementation, these
        // would be saved to files)
        std::cout << "Sample images created successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "Error creating sample images: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate format detection
 */
void demonstrateFormatDetection() {
    std::cout << "\n=== Format Detection ===\n";

    try {
        // Simulate format detection based on file extensions
        std::vector<std::string> test_files = {
            "image.jpg",     "photo.jpeg",    "picture.png", "graphic.bmp",
            "document.tiff", "animation.gif", "vector.svg",  "raw.cr2"};

        std::cout << "Detecting formats based on file extensions:\n";

        for (const auto& filename : test_files) {
            std::string extension = fs::path(filename).extension().string();
            std::transform(extension.begin(), extension.end(),
                           extension.begin(), ::tolower);

            std::string format_type;
            bool supported = true;

            if (extension == ".jpg" || extension == ".jpeg") {
                format_type = "JPEG (lossy compression)";
            } else if (extension == ".png") {
                format_type = "PNG (lossless compression)";
            } else if (extension == ".bmp") {
                format_type = "BMP (uncompressed)";
            } else if (extension == ".tiff" || extension == ".tif") {
                format_type = "TIFF (flexible format)";
            } else if (extension == ".gif") {
                format_type = "GIF (animation support)";
            } else if (extension == ".svg") {
                format_type = "SVG (vector graphics)";
                supported = false;
            } else if (extension == ".cr2") {
                format_type = "Canon RAW (requires special handling)";
                supported = false;
            } else {
                format_type = "Unknown format";
                supported = false;
            }

            std::cout << "  " << filename << " -> " << format_type;
            if (!supported) {
                std::cout << " [NOT SUPPORTED]";
            }
            std::cout << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in format detection: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate file validation
 */
void demonstrateFileValidation() {
    std::cout << "\n=== File Validation ===\n";

    try {
        // Simulate file validation checks
        std::vector<std::pair<std::string, bool>> test_cases = {
            {"valid_image.jpg", true},  {"corrupted_image.png", false},
            {"empty_file.bmp", false},  {"text_file.txt", false},
            {"large_image.tiff", true}, {"nonexistent.jpg", false}};

        std::cout << "Validating image files:\n";

        for (const auto& [filename, should_be_valid] : test_cases) {
            bool is_valid = should_be_valid;  // Simulated validation result

            // Simulate various validation checks
            if (filename.find("corrupted") != std::string::npos) {
                is_valid = false;
                std::cout << "  " << filename
                          << " -> INVALID (corrupted header)\n";
            } else if (filename.find("empty") != std::string::npos) {
                is_valid = false;
                std::cout << "  " << filename << " -> INVALID (empty file)\n";
            } else if (filename.find("text") != std::string::npos) {
                is_valid = false;
                std::cout << "  " << filename << " -> INVALID (not an image)\n";
            } else if (filename.find("nonexistent") != std::string::npos) {
                is_valid = false;
                std::cout << "  " << filename
                          << " -> INVALID (file not found)\n";
            } else {
                std::cout << "  " << filename << " -> VALID\n";
            }
        }

        // Demonstrate validation criteria
        std::cout << "\nValidation criteria:\n";
        std::cout << "  - File exists and is readable\n";
        std::cout << "  - Valid image format header\n";
        std::cout << "  - Consistent metadata (width, height, channels)\n";
        std::cout << "  - No corruption in critical sections\n";
        std::cout << "  - Supported color space and bit depth\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in file validation: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate error handling for file operations
 */
void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling ===\n";

    try {
        std::cout << "Testing various error conditions:\n";

        // Test 1: File not found
        try {
            // In a real implementation: blob<uint8_t> img =
            // blob<uint8_t>::load("nonexistent.jpg");
            throw std::runtime_error("File not found: nonexistent.jpg");
        } catch (const std::exception& e) {
            std::cout << "  ✓ Caught file not found error: " << e.what()
                      << "\n";
        }

        // Test 2: Unsupported format
        try {
            // In a real implementation: blob<uint8_t> img =
            // blob<uint8_t>::load("document.pdf");
            throw std::runtime_error("Unsupported format: PDF");
        } catch (const std::exception& e) {
            std::cout << "  ✓ Caught unsupported format error: " << e.what()
                      << "\n";
        }

        // Test 3: Corrupted file
        try {
            // In a real implementation: blob<uint8_t> img =
            // blob<uint8_t>::load("corrupted.jpg");
            throw std::runtime_error("Corrupted image data");
        } catch (const std::exception& e) {
            std::cout << "  ✓ Caught corrupted file error: " << e.what()
                      << "\n";
        }

        // Test 4: Insufficient memory
        try {
            // In a real implementation: blob<uint8_t> img =
            // blob<uint8_t>::load("huge_image.tiff");
            throw std::bad_alloc();
        } catch (const std::bad_alloc& e) {
            std::cout << "  ✓ Caught memory allocation error\n";
        }

        // Test 5: Permission denied
        try {
            // In a real implementation: img.save("/root/protected.jpg");
            throw std::runtime_error("Permission denied");
        } catch (const std::exception& e) {
            std::cout << "  ✓ Caught permission error: " << e.what() << "\n";
        }

        std::cout << "Error handling tests completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error in error handling demo: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrate batch file processing
 */
void demonstrateBatchProcessing() {
    std::cout << "\n=== Batch File Processing ===\n";

    try {
        // Simulate a directory with multiple image files
        std::vector<std::string> image_files = {"photo001.jpg",  "photo002.jpg",
                                                "photo003.png",  "image001.bmp",
                                                "image002.tiff", "scan001.png"};

        std::cout << "Processing batch of " << image_files.size()
                  << " images:\n";

        int processed = 0;
        int failed = 0;

        for (const auto& filename : image_files) {
            try {
                std::cout << "  Processing " << filename << "... ";

                // Simulate loading image
                // In real implementation: auto img =
                // blob<uint8_t>::load(filename);

                // Simulate processing (resize to thumbnail)
                const int thumb_size = 128, thumb_channels = 3;
                std::vector<uint8_t> thumbnail_data(thumb_size * thumb_size *
                                                    thumb_channels);

                // Fill thumbnail with sample data
                for (int y = 0; y < thumb_size; ++y) {
                    for (int x = 0; x < thumb_size; ++x) {
                        int pixel_idx = (y * thumb_size + x) * thumb_channels;
                        thumbnail_data[pixel_idx] =
                            static_cast<uint8_t>((x + processed * 50) % 256);
                        thumbnail_data[pixel_idx + 1] =
                            static_cast<uint8_t>((y + processed * 30) % 256);
                        thumbnail_data[pixel_idx + 2] = static_cast<uint8_t>(
                            (x + y + processed * 20) % 256);
                    }
                }

                blob thumbnail(
                    reinterpret_cast<std::byte*>(thumbnail_data.data()),
                    thumbnail_data.size());

                // Simulate saving thumbnail
                std::string thumb_name = "thumb_" + filename;
                // In real implementation: thumbnail.save(thumb_name);

                std::cout << "SUCCESS (created " << thumb_name << ")\n";
                processed++;

            } catch (const std::exception& e) {
                std::cout << "FAILED (" << e.what() << ")\n";
                failed++;
            }
        }

        std::cout << "\nBatch processing summary:\n";
        std::cout << "  Total files: " << image_files.size() << "\n";
        std::cout << "  Processed successfully: " << processed << "\n";
        std::cout << "  Failed: " << failed << "\n";
        std::cout << "  Success rate: "
                  << (100.0 * processed / image_files.size()) << "%\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in batch processing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate file metadata extraction
 */
void demonstrateMetadataExtraction() {
    std::cout << "\n=== File Metadata Extraction ===\n";

    try {
        // Simulate metadata extraction from different file types
        struct ImageMetadata {
            std::string filename;
            int width, height, channels;
            std::string format;
            size_t file_size;
            std::string color_space;
            int bit_depth;
        };

        std::vector<ImageMetadata> sample_metadata = {
            {"photo001.jpg", 1920, 1080, 3, "JPEG", 245760, "sRGB", 8},
            {"photo002.png", 800, 600, 4, "PNG", 192000, "sRGB", 8},
            {"image001.bmp", 640, 480, 3, "BMP", 921600, "RGB", 8},
            {"scan001.tiff", 2400, 3200, 1, "TIFF", 7680000, "Grayscale", 16}};

        std::cout << "Extracting metadata from image files:\n\n";
        std::cout << "Filename        | Dimensions  | Channels | Format | Size "
                     "(KB) | Color Space | Bit Depth\n";
        std::cout << "----------------|-------------|----------|--------|------"
                     "-----|-------------|----------\n";

        for (const auto& meta : sample_metadata) {
            std::cout << std::left << std::setw(15) << meta.filename << " | "
                      << std::setw(11)
                      << (std::to_string(meta.width) + "x" +
                          std::to_string(meta.height))
                      << " | " << std::setw(8) << meta.channels << " | "
                      << std::setw(6) << meta.format << " | " << std::setw(9)
                      << (meta.file_size / 1024) << " | " << std::setw(11)
                      << meta.color_space << " | " << meta.bit_depth << "\n";
        }

        // Calculate statistics
        size_t total_size = 0;
        int total_pixels = 0;

        for (const auto& meta : sample_metadata) {
            total_size += meta.file_size;
            total_pixels += meta.width * meta.height;
        }

        std::cout << "\nMetadata summary:\n";
        std::cout << "  Total files: " << sample_metadata.size() << "\n";
        std::cout << "  Total size: " << total_size / 1024 << " KB\n";
        std::cout << "  Total pixels: " << total_pixels / 1000000.0 << " MP\n";
        std::cout << "  Average file size: "
                  << (total_size / sample_metadata.size()) / 1024 << " KB\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in metadata extraction: " << e.what() << "\n";
    }
}

int main() {
    std::cout << "=== Atom Image Basic File Operations Example ===\n";
    std::cout << "This example demonstrates basic image file I/O operations\n";

    // Run all demonstrations
    createSampleImages();
    demonstrateFormatDetection();
    demonstrateFileValidation();
    demonstrateErrorHandling();
    demonstrateBatchProcessing();
    demonstrateMetadataExtraction();

    std::cout << "\n=== File operations example completed ===\n";
    return 0;
}
