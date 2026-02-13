/**
 * @file fits_utils_example.cpp
 * @brief Example demonstrating FITS utility functions
 *
 * This example covers:
 * - FitsImage wrapper class
 * - Loading and saving FITS images
 * - Converting between FITS and OpenCV formats
 * - Image manipulation utilities
 * - Data type conversions
 * - Metadata operations
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>

#ifdef ATOM_IMAGE_HAS_CFITSIO
#include "atom/image/formats/fits_utils.hpp"

#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/opencv.hpp>
#endif

using namespace atom::image;
using namespace std;

/**
 * @brief Demonstrate FitsImage creation
 */
void demonstrateFitsImageCreation() {
    cout << "\n=== FitsImage Creation ===\n";

    try {
        // Create from dimensions
        FitsImage img(1920, 1080, 3, DataType::FLOAT);
        cout << "Created FITS image: 1920x1080x3\n";

        // Load from file
        FitsImage loaded("test.fits");
        cout << "Loaded FITS image from file\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate loading and saving
 */
void demonstrateLoadingSaving() {
    cout << "\n=== Loading and Saving ===\n";

    try {
        FitsImage img;

        // Load FITS file
        img.load("input.fits");
        cout << "Loaded FITS file\n";

        // Save FITS file
        img.save("output.fits");
        cout << "Saved FITS file\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

#ifdef ATOM_IMAGE_HAS_OPENCV
/**
 * @brief Demonstrate OpenCV conversion
 */
void demonstrateOpenCVConversion() {
    cout << "\n=== OpenCV Conversion ===\n";

    try {
        // Create from cv::Mat
        cv::Mat mat(480, 640, CV_32FC3);
        mat = cv::Scalar(100.0f, 150.0f, 200.0f);

        FitsImage img(mat, DataType::FLOAT);
        cout << "Created FitsImage from cv::Mat\n";

        // Convert to cv::Mat
        cv::Mat converted = img.toMat();
        cout << "Converted FitsImage to cv::Mat: " << converted.cols << "x"
             << converted.rows << "\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}
#endif

/**
 * @brief Demonstrate image access
 */
void demonstrateImageAccess() {
    cout << "\n=== Image Access ===\n";

    try {
        FitsImage img(640, 480, 1, DataType::FLOAT);

        // Get dimensions
        int width = img.getWidth();
        int height = img.getHeight();
        int channels = img.getChannels();

        cout << "Image dimensions: " << width << "x" << height << " ("
             << channels << " channels)\n";

        // Access HDU
        auto& hdu = img.getImageHDU();
        cout << "Accessed image HDU\n";

        // Access FITS file
        auto& fits = img.getFITSFile();
        cout << "Accessed FITS file\n";

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
        FitsImage img(640, 480, 1, DataType::FLOAT);

        // Set metadata
        auto& hdu = img.getImageHDU();
        hdu.setHeaderKeyword("OBJECT", "Test Image");
        hdu.setHeaderKeyword("TELESCOP", "VLT");
        hdu.setHeaderKeyword("EXPTIME", "60.0");

        cout << "Set metadata keywords\n";

        // Get metadata
        auto object = hdu.getHeaderKeyword<string>("OBJECT");
        auto exptime = hdu.getHeaderKeyword<double>("EXPTIME");

        cout << "OBJECT: " << object << "\n";
        cout << "EXPTIME: " << exptime << " seconds\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate data type handling
 */
void demonstrateDataTypeHandling() {
    cout << "\n=== Data Type Handling ===\n";

    try {
        // Create images with different data types
        FitsImage byteImg(640, 480, 3, DataType::BYTE);
        FitsImage shortImg(640, 480, 3, DataType::SHORT);
        FitsImage floatImg(640, 480, 3, DataType::FLOAT);
        FitsImage doubleImg(640, 480, 3, DataType::DOUBLE);

        cout << "Created images with different data types:\n";
        cout << "  BYTE (8-bit)\n";
        cout << "  SHORT (16-bit)\n";
        cout << "  FLOAT (32-bit)\n";
        cout << "  DOUBLE (64-bit)\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate utility functions
 */
void demonstrateUtilityFunctions() {
    cout << "\n=== Utility Functions ===\n";

    try {
        FitsImage img(640, 480, 1, DataType::FLOAT);

        // Check if valid
        bool valid = img.isValid();
        cout << "Image is valid: " << (valid ? "Yes" : "No") << "\n";

        // Get data type
        DataType type = img.getDataType();
        cout << "Data type: " << static_cast<int>(type) << "\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

int main() {
    demonstrateFitsImageCreation();
    demonstrateLoadingSaving();
#ifdef ATOM_IMAGE_HAS_OPENCV
    demonstrateOpenCVConversion();
#endif
    demonstrateImageAccess();
    demonstrateMetadataOperations();
    demonstrateDataTypeHandling();
    demonstrateUtilityFunctions();

    return 0;
}

#elseint main() {
std::cout << "CFITSIO support not enabled. Rebuild with "
             "ATOM_IMAGE_HAS_CFITSIO=ON\n";
return 0;
}
#endif
