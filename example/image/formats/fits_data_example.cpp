/**
 * @file fits_data_example.cpp
 * @brief Example demonstrating FITS data operations
 *
 * This example covers:
 * - Working with typed FITS data
 * - Data access and modification
 * - Data validation and recovery
 * - Data transformations
 * - Statistics calculation
 * - Data type conversions
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>

#ifdef ATOM_IMAGE_HAS_CFITSIO
#include "atom/image/formats/fits_data.hpp"

using namespace atom::image;
using namespace std;

/**
 * @brief Demonstrate basic data operations
 */
void demonstrateBasicDataOperations() {
    cout << "\n=== Basic Data Operations ===\n";

    try {
        // Create typed FITS data
        TypedFITSData<float> data(1920, 1080, 3);

        cout << "Created FITS data: " << data.getWidth() << "x"
             << data.getHeight() << " (" << data.getChannels()
             << " channels)\n";

        // Get data size
        size_t totalSize = data.size();
        cout << "Total data size: " << totalSize << " elements\n";

        // Check if empty
        cout << "Is empty: " << (data.empty() ? "Yes" : "No") << "\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate data access
 */
void demonstrateDataAccess() {
    cout << "\n=== Data Access ===\n";

    try {
        TypedFITSData<float> data(640, 480, 1);

        // Set pixel value
        data.setPixel(100, 100, 0, 255.0f);

        // Get pixel value
        float value = data.getPixel(100, 100, 0);
        cout << "Pixel at (100, 100): " << value << "\n";

        // Direct data access
        auto& rawData = data.getData();
        cout << "Raw data size: " << rawData.size() << "\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate data validation
 */
void demonstrateDataValidation() {
    cout << "\n=== Data Validation ===\n";

    try {
        TypedFITSData<float> data(640, 480, 1);

        // Validate data
        bool valid = data.validate();
        cout << "Data is valid: " << (valid ? "Yes" : "No") << "\n";

        // Try to recover from errors
        size_t fixed = data.tryRecover(true, true, 0.0f);
        cout << "Fixed " << fixed << " invalid values\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate data transformations
 */
void demonstrateDataTransformations() {
    cout << "\n=== Data Transformations ===\n";

    try {
        TypedFITSData<float> data(640, 480, 1);

        // Apply transformation function
        data.transform([](float value) {
            return value * 2.0f;  // Double all values
        });

        cout << "Applied transformation to all pixels\n";

        // Fill with value
        data.fill(0.0f);
        cout << "Filled data with zeros\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate statistics
 */
void demonstrateStatistics() {
    cout << "\n=== Statistics ===\n";

    try {
        TypedFITSData<float> data(640, 480, 1);

        // Fill with some test data
        for (int y = 0; y < 480; ++y) {
            for (int x = 0; x < 640; ++x) {
                data.setPixel(x, y, 0, static_cast<float>(x + y));
            }
        }

        // Calculate statistics
        auto stats = data.calculateStatistics();
        cout << "Mean: " << stats.mean << "\n";
        cout << "Std dev: " << stats.stddev << "\n";
        cout << "Min: " << stats.min << "\n";
        cout << "Max: " << stats.max << "\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate data type operations
 */
void demonstrateDataTypeOperations() {
    cout << "\n=== Data Type Operations ===\n";

    try {
        // Create data with different types
        TypedFITSData<uint8_t> byteData(640, 480, 3);
        TypedFITSData<uint16_t> shortData(640, 480, 3);
        TypedFITSData<float> floatData(640, 480, 3);
        TypedFITSData<double> doubleData(640, 480, 3);

        cout << "Created data with different types:\n";
        cout << "  uint8_t: " << byteData.size() << " elements\n";
        cout << "  uint16_t: " << shortData.size() << " elements\n";
        cout << "  float: " << floatData.size() << " elements\n";
        cout << "  double: " << doubleData.size() << " elements\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate data copying and moving
 */
void demonstrateDataCopyingMoving() {
    cout << "\n=== Data Copying and Moving ===\n";

    try {
        TypedFITSData<float> original(640, 480, 1);
        original.fill(100.0f);

        // Copy data
        TypedFITSData<float> copy = original;
        cout << "Copied data: " << copy.size() << " elements\n";

        // Move data
        TypedFITSData<float> moved = std::move(copy);
        cout << "Moved data: " << moved.size() << " elements\n";
        cout << "Original copy is empty: " << (copy.empty() ? "Yes" : "No")
             << "\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

int main() {
    demonstrateBasicDataOperations();
    demonstrateDataAccess();
    demonstrateDataValidation();
    demonstrateDataTransformations();
    demonstrateStatistics();
    demonstrateDataTypeOperations();
    demonstrateDataCopyingMoving();

    return 0;
}

#else
int main() {
    std::cout << "CFITSIO support not enabled. Rebuild with "
                 "ATOM_IMAGE_HAS_CFITSIO=ON\n";
    return 0;
}
#endif
