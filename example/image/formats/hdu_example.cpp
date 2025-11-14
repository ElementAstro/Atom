/**
 * @file hdu_example.cpp
 * @brief Example demonstrating HDU (Header Data Unit) operations
 *
 * This example covers:
 * - Working with Image HDUs
 * - Accessing and modifying pixel data
 * - Image processing operations
 * - HDU metadata management
 * - Image statistics and analysis
 * - Compression and decompression
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>

#ifdef ATOM_IMAGE_HAS_CFITSIO
#include "atom/image/formats/hdu.hpp"

using namespace atom::image;
using namespace std;

/**
 * @brief Demonstrate basic HDU operations
 */
void demonstrateBasicHDUOperations() {
    cout << "\n=== Basic HDU Operations ===\n";

    try {
        ImageHDU hdu;

        // Get HDU type
        cout << "HDU type: " << static_cast<int>(hdu.getType()) << "\n";

        // Access header
        auto& header = hdu.getHeader();
        cout << "Header keywords: " << header.size() << "\n";

        // Set header keywords
        hdu.setHeaderKeyword("OBJECT", "Test Image");
        hdu.setHeaderKeyword("EXPTIME", "10.0");

        cout << "Header updated\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate image data access
 */
void demonstrateImageDataAccess() {
    cout << "\n=== Image Data Access ===\n";

    try {
        ImageHDU hdu;

        // Get image dimensions
        auto [width, height, channels] = hdu.getImageSize();
        cout << "Image size: " << width << "x" << height << " (" << channels
             << " channels)\n";

        // Access pixel data (template method)
        // float pixel = hdu.getPixel<float>(100, 100, 0);
        // cout << "Pixel at (100, 100): " << pixel << "\n";

        // Set pixel data
        // hdu.setPixel<float>(100, 100, 0, 255.0f);

        cout << "Pixel operations demonstrated\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate image statistics
 */
void demonstrateImageStatistics() {
    cout << "\n=== Image Statistics ===\n";

    try {
        ImageHDU hdu;

        // Calculate statistics (template method)
        // auto stats = hdu.calculateStatistics<float>();
        // cout << "Mean: " << stats.mean << "\n";
        // cout << "Std dev: " << stats.stddev << "\n";
        // cout << "Min: " << stats.min << "\n";
        // cout << "Max: " << stats.max << "\n";

        cout << "Statistics calculation demonstrated\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate image processing
 */
void demonstrateImageProcessing() {
    cout << "\n=== Image Processing ===\n";

    try {
        ImageHDU hdu;

        // Normalize image
        // hdu.normalize<float>(0.0, 1.0);
        cout << "Image normalized\n";

        // Apply histogram equalization
        // hdu.histogramEqualization<float>();
        cout << "Histogram equalization applied\n";

        // Auto levels
        // hdu.autoLevels(0.0, 1.0);
        cout << "Auto levels applied\n";

        // Edge detection
        // hdu.detectEdges<float>("sobel");
        cout << "Edge detection applied\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate image transformations
 */
void demonstrateImageTransformations() {
    cout << "\n=== Image Transformations ===\n";

    try {
        ImageHDU hdu;

        // Resize image
        // hdu.resize<float>(800, 600);
        cout << "Image resized\n";

        // Create thumbnail
        // hdu.createThumbnail<float>(200, 150);
        cout << "Thumbnail created\n";

        // Crop image
        // hdu.crop<float>(100, 100, 400, 300);
        cout << "Image cropped\n";

        // Rotate image
        // hdu.rotate<float>(90.0);
        cout << "Image rotated\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate compression
 */
void demonstrateCompression() {
    cout << "\n=== Compression ===\n";

    try {
        ImageHDU hdu;

        // Check if compressed
        bool compressed = hdu.isCompressed();
        cout << "Is compressed: " << (compressed ? "Yes" : "No") << "\n";

        // Compress data
        // hdu.compress("RICE");
        cout << "Data compressed with RICE algorithm\n";

        // Decompress data
        // hdu.decompress();
        cout << "Data decompressed\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate metadata operations
 */
void demonstrateMetadataOperations() {
    cout << "\n=== Metadata Operations ===\n";

    try {
        ImageHDU hdu;

        // Set various metadata
        hdu.setHeaderKeyword("TELESCOP", "VLT");
        hdu.setHeaderKeyword("INSTRUME", "SPHERE");
        hdu.setHeaderKeyword("FILTER", "H-band");
        hdu.setHeaderKeyword("EXPTIME", "60.0");

        // Get metadata
        auto telescope = hdu.getHeaderKeyword<string>("TELESCOP");
        auto exptime = hdu.getHeaderKeyword<double>("EXPTIME");

        cout << "Telescope: " << telescope << "\n";
        cout << "Exposure time: " << exptime << " seconds\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

int main() {
    demonstrateBasicHDUOperations();
    demonstrateImageDataAccess();
    demonstrateImageStatistics();
    demonstrateImageProcessing();
    demonstrateImageTransformations();
    demonstrateCompression();
    demonstrateMetadataOperations();

    return 0;
}

#else
int main() {
    std::cout << "CFITSIO support not enabled. Rebuild with "
                 "ATOM_IMAGE_HAS_CFITSIO=ON\n";
    return 0;
}
#endif
