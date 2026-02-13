/**
 * @file ser_reader_example.cpp
 * @brief Example demonstrating SER file reading capabilities
 *
 * This example covers:
 * - Opening and reading SER video files
 * - Accessing header information
 * - Reading individual frames
 * - Reading multiple frames efficiently
 * - Working with timestamps
 * - Frame caching and performance optimization
 * - Reading raw frame data
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

#include "atom/image/formats/ser/ser.hpp"

using namespace serastro;
using namespace std;

/**
 * @brief Demonstrate basic SER file reading
 */
void demonstrateBasicReading(const string& filename) {
    cout << "\n=== Basic SER File Reading ===\n";

    try {
        // Open SER file
        SERReader reader(filename);

        // Get header information
        const auto& header = reader.getHeader();

        cout << "File: " << filename << "\n";
        cout << "Dimensions: " << header.imageWidth << "x" << header.imageHeight
             << "\n";
        cout << "Frame count: " << header.frameCount << "\n";
        cout << "Pixel depth: " << header.pixelDepth << " bits\n";
        cout << "Color format: " << (reader.isColor() ? "Color" : "Mono")
             << "\n";

        // Read first frame
        auto frame = reader.readFrame(0);
        cout << "First frame size: " << frame.cols << "x" << frame.rows << "\n";
        cout << "Frame type: " << frame.type() << "\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate reading multiple frames with options
 */
void demonstrateAdvancedReading(const string& filename) {
    cout << "\n=== Advanced Frame Reading ===\n";

    try {
        SERReader reader(filename);

        // Configure read options
        ReadOptions options;
        options.convertToFloat = true;
        options.normalizeFrame = true;
        options.enableCache = true;
        options.maxCacheSize = 500;  // 500 MB cache

        // Read multiple frames
        vector<size_t> frameIndices = {0, 10, 20, 30, 40};
        auto frames = reader.readFrames(frameIndices, options);

        cout << "Read " << frames.size() << " frames\n";

        for (size_t i = 0; i < frames.size(); ++i) {
            cout << "Frame " << frameIndices[i] << ": " << frames[i].cols << "x"
                 << frames[i].rows << ", type=" << frames[i].type() << "\n";
        }

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate timestamp handling
 */
void demonstrateTimestamps(const string& filename) {
    cout << "\n=== Timestamp Handling ===\n";

    try {
        SERReader reader(filename);

        if (reader.hasTimestamps()) {
            cout << "File contains timestamps\n";

            // Get all timestamps
            auto timestamps = reader.getAllTimestamps();

            cout << "Total timestamps: " << timestamps.size() << "\n";

            // Display first few timestamps
            for (size_t i = 0; i < min(size_t(5), timestamps.size()); ++i) {
                cout << "Frame " << i
                     << " timestamp: " << timestamps[i].nanoseconds << " ns\n";
            }
        } else {
            cout << "File does not contain timestamps\n";
        }

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate performance with caching
 */
void demonstratePerformance(const string& filename) {
    cout << "\n=== Performance Demonstration ===\n";

    try {
        SERReader reader(filename);
        size_t frameCount = reader.getFrameCount();

        // Read with caching
        auto start = chrono::high_resolution_clock::now();

        ReadOptions options;
        options.enableCache = true;

        for (size_t i = 0; i < min(frameCount, size_t(100)); ++i) {
            auto frame = reader.readFrame(i, options);
        }

        auto end = chrono::high_resolution_clock::now();
        auto duration =
            chrono::duration_cast<chrono::milliseconds>(end - start);

        cout << "Read 100 frames in " << duration.count() << " ms\n";
        cout << "Average: " << (duration.count() / 100.0) << " ms per frame\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <ser_file>\n";
        return 1;
    }

    string filename = argv[1];

    demonstrateBasicReading(filename);
    demonstrateAdvancedReading(filename);
    demonstrateTimestamps(filename);
    demonstratePerformance(filename);

    return 0;
}
