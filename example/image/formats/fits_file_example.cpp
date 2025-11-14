/**
 * @file fits_file_example.cpp
 * @brief Example demonstrating FITS file operations
 *
 * This example covers:
 * - Reading and writing FITS files
 * - Managing multiple HDUs (Header Data Units)
 * - Asynchronous file operations
 * - Memory-mapped I/O for large files
 * - Progress tracking
 * - File validation
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <future>
#include <iostream>

#ifdef ATOM_IMAGE_HAS_CFITSIO
#include "atom/image/formats/fits_file.hpp"

using namespace atom::image;
using namespace std;

/**
 * @brief Demonstrate basic FITS file operations
 */
void demonstrateBasicFITSOperations() {
    cout << "\n=== Basic FITS File Operations ===\n";

    try {
        // Read FITS file
        FITSFile fits;
        fits.readFITS("test.fits");

        cout << "FITS file loaded successfully\n";
        cout << "Number of HDUs: " << fits.getHDUCount() << "\n";
        cout << "Is empty: " << (fits.isEmpty() ? "Yes" : "No") << "\n";

        // Access primary HDU
        auto& primaryHDU = fits.getHDU(0);
        cout << "Primary HDU type: " << static_cast<int>(primaryHDU.getType())
             << "\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate HDU management
 */
void demonstrateHDUManagement() {
    cout << "\n=== HDU Management ===\n";

    try {
        FITSFile fits;

        // Create image HDU
        fits.createImageHDU(1920, 1080, 3);
        cout << "Created image HDU\n";

        // Add extension HDU
        fits.addHDU(std::make_unique<ImageHDU>());
        cout << "Added extension HDU\n";

        cout << "Total HDUs: " << fits.getHDUCount() << "\n";

        // Access specific HDU
        auto& hdu = fits.getHDU(0);
        cout << "HDU 0 type: " << static_cast<int>(hdu.getType()) << "\n";

        // Remove HDU
        fits.removeHDU(1);
        cout << "Removed HDU 1\n";
        cout << "Remaining HDUs: " << fits.getHDUCount() << "\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate writing FITS files
 */
void demonstrateWritingFITS() {
    cout << "\n=== Writing FITS Files ===\n";

    try {
        FITSFile fits;

        // Create image HDU with data
        fits.createImageHDU(640, 480, 1);

        auto& hdu = fits.getImageHDU(0);
        // Set some header keywords
        hdu.setHeaderKeyword("OBJECT", "Test Image");
        hdu.setHeaderKeyword("EXPTIME", "10.0");

        // Write to file
        fits.writeFITS("output.fits");
        cout << "FITS file written successfully\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate asynchronous operations
 */
void demonstrateAsyncOperations() {
    cout << "\n=== Asynchronous Operations ===\n";

    try {
        FITSFile fits;

        // Async read
        auto readFuture = fits.readFITSAsync("test.fits");
        cout << "Reading FITS file asynchronously...\n";

        // Do other work while reading
        cout << "Doing other work...\n";

        // Wait for completion
        readFuture.get();
        cout << "Async read completed\n";
        cout << "HDU count: " << fits.getHDUCount() << "\n";

        // Async write
        auto writeFuture = fits.writeFITSAsync("output_async.fits");
        cout << "Writing FITS file asynchronously...\n";
        writeFuture.get();
        cout << "Async write completed\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate memory-mapped I/O
 */
void demonstrateMemoryMappedIO() {
    cout << "\n=== Memory-Mapped I/O ===\n";

    try {
        FITSFile fits;

        // Read large file with memory mapping
        fits.readFITS("large_file.fits", true, true);
        cout << "Large file loaded with memory mapping\n";
        cout << "HDU count: " << fits.getHDUCount() << "\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate progress tracking
 */
void demonstrateProgressTracking() {
    cout << "\n=== Progress Tracking ===\n";

    try {
        FITSFile fits;

        // Set progress callback
        fits.setProgressCallback([](float progress, const string& status) {
            cout << "Progress: " << int(progress * 100) << "% - " << status
                 << "\n";
        });

        fits.readFITS("test.fits");
        cout << "File loaded with progress tracking\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

int main() {
    demonstrateBasicFITSOperations();
    demonstrateHDUManagement();
    demonstrateWritingFITS();
    demonstrateAsyncOperations();
    demonstrateMemoryMappedIO();
    demonstrateProgressTracking();

    return 0;
}

#else
int main() {
    std::cout << "CFITSIO support not enabled. Rebuild with "
                 "ATOM_IMAGE_HAS_CFITSIO=ON\n";
    return 0;
}
#endif
