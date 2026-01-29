/**
 * @file exceptions_example.cpp
 * @brief Example demonstrating exception handling in image module
 *
 * This example covers:
 * - Different exception types
 * - Error handling patterns
 * - Exception catching and recovery
 * - Custom error messages
 * - Stack trace information
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>
#include <stdexcept>

#include "atom/image/core/exceptions.hpp"
#include "atom/image/core/image_blob.hpp"

using namespace atom::image;
using namespace std;

/**
 * @brief Demonstrate basic exception handling
 */
void demonstrateBasicExceptions() {
    cout << "\n=== Basic Exception Handling ===\n";

    try {
        // Attempt to load non-existent file
        blob img = blob::load("nonexistent_file.jpg");
    } catch (const ImageLoadException& e) {
        cout << "Caught ImageLoadException: " << e.what() << "\n";
    } catch (const exception& e) {
        cout << "Caught generic exception: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate format exceptions
 */
void demonstrateFormatExceptions() {
    cout << "\n=== Format Exceptions ===\n";

    try {
        // Try to load invalid format
        blob img(100, 100, 3);
        img.save("test.invalid_format");
    } catch (const UnsupportedFormatException& e) {
        cout << "Caught UnsupportedFormatException: " << e.what() << "\n";
    } catch (const exception& e) {
        cout << "Caught exception: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate dimension exceptions
 */
void demonstrateDimensionExceptions() {
    cout << "\n=== Dimension Exceptions ===\n";

    try {
        // Create blob with invalid dimensions
        blob img(0, 0, 3);  // Invalid: zero dimensions
    } catch (const InvalidDimensionsException& e) {
        cout << "Caught InvalidDimensionsException: " << e.what() << "\n";
    } catch (const exception& e) {
        cout << "Caught exception: " << e.what() << "\n";
    }

    try {
        // Try to resize to invalid dimensions
        blob img(100, 100, 3);
        img.resize(0, 100);  // Invalid: zero width
    } catch (const InvalidDimensionsException& e) {
        cout << "Caught InvalidDimensionsException: " << e.what() << "\n";
    } catch (const exception& e) {
        cout << "Caught exception: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate memory exceptions
 */
void demonstrateMemoryExceptions() {
    cout << "\n=== Memory Exceptions ===\n";

    try {
        // Try to allocate extremely large blob
        blob img(100000, 100000, 3);  // May throw bad_alloc
    } catch (const bad_alloc& e) {
        cout << "Caught bad_alloc: " << e.what() << "\n";
    } catch (const exception& e) {
        cout << "Caught exception: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate exception recovery
 */
void demonstrateExceptionRecovery() {
    cout << "\n=== Exception Recovery ===\n";

    vector<string> filenames = {"image1.jpg", "nonexistent.jpg", "image2.jpg"};

    int successCount = 0;
    int failureCount = 0;

    for (const auto& filename : filenames) {
        try {
            blob img = blob::load(filename);
            successCount++;
            cout << "Successfully loaded: " << filename << "\n";
        } catch (const ImageLoadException& e) {
            failureCount++;
            cout << "Failed to load: " << filename << " - " << e.what() << "\n";
            // Continue processing other files
        }
    }

    cout << "\nResults: " << successCount << " succeeded, " << failureCount
         << " failed\n";
}

/**
 * @brief Demonstrate nested exception handling
 */
void demonstrateNestedExceptions() {
    cout << "\n=== Nested Exception Handling ===\n";

    try {
        try {
            // Inner operation that might fail
            blob img = blob::load("test.jpg");
            img.resize(0, 0);  // This will fail
        } catch (const InvalidDimensionsException& e) {
            cout << "Inner exception caught: " << e.what() << "\n";
            // Re-throw or handle
            throw runtime_error(
                "Failed to process image due to invalid dimensions");
        }
    } catch (const runtime_error& e) {
        cout << "Outer exception caught: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate exception safety
 */
void demonstrateExceptionSafety() {
    cout << "\n=== Exception Safety ===\n";

    blob* img = nullptr;

    try {
        img = new blob(100, 100, 3);

        // Operation that might throw
        img->resize(0, 100);  // Will throw

        delete img;  // Won't be reached if exception thrown
    } catch (const exception& e) {
        cout << "Exception caught: " << e.what() << "\n";

        // Clean up resources
        if (img != nullptr) {
            delete img;
            cout << "Resources cleaned up\n";
        }
    }

    // Better approach: use RAII
    try {
        auto img_smart = make_unique<blob>(100, 100, 3);
        img_smart->resize(0, 100);  // Will throw
        // Automatic cleanup via unique_ptr
    } catch (const exception& e) {
        cout << "Exception caught (RAII): " << e.what() << "\n";
        cout << "Resources automatically cleaned up\n";
    }
}

int main() {
    demonstrateBasicExceptions();
    demonstrateFormatExceptions();
    demonstrateDimensionExceptions();
    demonstrateMemoryExceptions();
    demonstrateExceptionRecovery();
    demonstrateNestedExceptions();
    demonstrateExceptionSafety();

    return 0;
}
