/**
 * @file advanced_formats_example.cpp
 * @brief Example demonstrating advanced format operations
 *
 * This example covers:
 * - Working with specialized image formats
 * - Format-specific features and optimizations
 * - Advanced compression techniques
 * - Multi-format workflows
 * - Format conversion strategies
 * - Performance considerations
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>
#include <vector>

#ifdef ATOM_IMAGE_HAS_CFITSIO
#include "atom/image/formats/advanced_formats.hpp"

using namespace atom::image;
using namespace std;

/**
 * @brief Demonstrate advanced format detection
 */
void demonstrateAdvancedFormatDetection() {
    cout << "\n=== Advanced Format Detection ===\n";

    try {
        // Detect format with confidence
        vector<string> files = {"image.fits", "data.ser", "photo.jpg"};

        for (const auto& file : files) {
            cout << "Detecting format for: " << file << "\n";
            // Format detection logic would go here
        }

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate format-specific optimizations
 */
void demonstrateFormatOptimizations() {
    cout << "\n=== Format-Specific Optimizations ===\n";

    try {
        cout << "FITS optimizations:\n";
        cout << "  - Memory-mapped I/O for large files\n";
        cout << "  - Tiled access for huge images\n";
        cout << "  - Compression (Rice, GZIP, HCOMPRESS)\n";

        cout << "\nSER optimizations:\n";
        cout << "  - Streaming access for video sequences\n";
        cout << "  - Frame caching for repeated access\n";
        cout << "  - Timestamp-based indexing\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate compression techniques
 */
void demonstrateCompressionTechniques() {
    cout << "\n=== Compression Techniques ===\n";

    try {
        cout << "Lossless compression:\n";
        cout << "  - Rice (astronomical data)\n";
        cout << "  - GZIP (general purpose)\n";
        cout << "  - PNG (standard images)\n";

        cout << "\nLossy compression:\n";
        cout << "  - JPEG (photos)\n";
        cout << "  - HCOMPRESS (astronomical)\n";

        cout << "\nCompression selection criteria:\n";
        cout << "  - Data type (integer vs. float)\n";
        cout << "  - Image content (smooth vs. detailed)\n";
        cout << "  - Required quality\n";
        cout << "  - File size constraints\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate multi-format workflows
 */
void demonstrateMultiFormatWorkflows() {
    cout << "\n=== Multi-Format Workflows ===\n";

    try {
        cout << "Workflow 1: FITS to standard formats\n";
        cout << "  1. Load FITS file\n";
        cout << "  2. Extract image data\n";
        cout << "  3. Normalize for display\n";
        cout << "  4. Save as PNG/JPEG\n";

        cout << "\nWorkflow 2: SER to FITS\n";
        cout << "  1. Load SER video\n";
        cout << "  2. Select best frames\n";
        cout << "  3. Stack frames\n";
        cout << "  4. Save as FITS\n";

        cout << "\nWorkflow 3: Batch conversion\n";
        cout << "  1. Scan directory\n";
        cout << "  2. Detect formats\n";
        cout << "  3. Convert in parallel\n";
        cout << "  4. Preserve metadata\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate format conversion strategies
 */
void demonstrateConversionStrategies() {
    cout << "\n=== Format Conversion Strategies ===\n";

    try {
        cout << "Strategy 1: Preserve precision\n";
        cout << "  - Use floating-point formats\n";
        cout << "  - Avoid lossy compression\n";
        cout << "  - Maintain bit depth\n";

        cout << "\nStrategy 2: Optimize for size\n";
        cout << "  - Use appropriate compression\n";
        cout << "  - Reduce bit depth if acceptable\n";
        cout << "  - Remove unnecessary metadata\n";

        cout << "\nStrategy 3: Optimize for speed\n";
        cout << "  - Use memory mapping\n";
        cout << "  - Parallel processing\n";
        cout << "  - Minimize format conversions\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate performance considerations
 */
void demonstratePerformanceConsiderations() {
    cout << "\n=== Performance Considerations ===\n";

    try {
        cout << "Large file handling:\n";
        cout << "  - Use memory mapping for files > 100MB\n";
        cout << "  - Enable tiled access for images > 4K\n";
        cout << "  - Consider streaming for sequences\n";

        cout << "\nMemory management:\n";
        cout << "  - Cache frequently accessed data\n";
        cout << "  - Use lazy loading when possible\n";
        cout << "  - Release resources promptly\n";

        cout << "\nParallel processing:\n";
        cout << "  - Process multiple files concurrently\n";
        cout << "  - Use async I/O operations\n";
        cout << "  - Leverage multi-core CPUs\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate metadata preservation
 */
void demonstrateMetadataPreservation() {
    cout << "\n=== Metadata Preservation ===\n";

    try {
        cout << "Critical metadata:\n";
        cout << "  - Observation details (telescope, instrument)\n";
        cout << "  - Timing information (exposure, timestamp)\n";
        cout << "  - Calibration data (dark, flat, bias)\n";
        cout << "  - Processing history\n";

        cout << "\nMetadata mapping:\n";
        cout << "  - FITS keywords to EXIF tags\n";
        cout << "  - Custom keywords to XMP\n";
        cout << "  - Preserve original format metadata\n";

    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

int main() {
    demonstrateAdvancedFormatDetection();
    demonstrateFormatOptimizations();
    demonstrateCompressionTechniques();
    demonstrateMultiFormatWorkflows();
    demonstrateConversionStrategies();
    demonstratePerformanceConsiderations();
    demonstrateMetadataPreservation();

    return 0;
}

#elseint main() {
std::cout << "CFITSIO support not enabled. Rebuild with "
             "ATOM_IMAGE_HAS_CFITSIO=ON\n";
return 0;
}
#endif
