/**
 * @file ser_writer_example.cpp
 * @brief Example demonstrating SER file writing capabilities
 *
 * This example covers:
 * - Creating new SER files
 * - Configuring SER headers
 * - Writing individual frames
 * - Writing frames with timestamps
 * - Writing multiple frames efficiently
 * - Finalizing SER files
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

#include "atom/image/formats/ser/ser.hpp"

using namespace serastro;
using namespace std;

/**
 * @brief Generate a test frame with pattern
 */
cv::Mat generateTestFrame(int width, int height, int frameNumber) {
    cv::Mat frame(height, width, CV_8UC1);

    // Create a moving pattern
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int value = (x + y + frameNumber * 10) % 256;
            frame.at<uint8_t>(y, x) = static_cast<uint8_t>(value);
        }
    }

    return frame;
}

/**
 * @brief Demonstrate basic SER file writing
 */
void demonstrateBasicWriting() {
    cout << "\n=== Basic SER File Writing ===\n";

    try {
        // Configure header
        SERHeader header;
        header.imageWidth = 640;
        header.imageHeight = 480;
        header.pixelDepth = 8;
        header.colorID = static_cast<uint32_t>(SERColorID::Mono);
        header.frameCount = 0;  // Will be updated on finalize

        // Set metadata
        string observer = "Example Observer";
        string instrument = "Test Camera";
        string telescope = "Example Telescope";

        copy(observer.begin(), observer.end(), header.observer.begin());
        copy(instrument.begin(), instrument.end(), header.instrument.begin());
        copy(telescope.begin(), telescope.end(), header.telescope.begin());

        // Create writer
        SERWriter writer("output_basic.ser", header);

        // Write frames
        for (int i = 0; i < 10; ++i) {
            auto frame = generateTestFrame(640, 480, i);
            writer.writeFrame(frame);
            cout << "Written frame " << i << "\n";
        }

        // Finalize (updates frame count in header)
        writer.finalize();

        cout << "Successfully created output_basic.ser with "
             << writer.getFrameCount() << " frames\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate writing with timestamps
 */
void demonstrateTimestampWriting() {
    cout << "\n=== Writing with Timestamps ===\n";

    try {
        SERHeader header(640, 480, 8, SERColorID::Mono);

        SERWriter writer("output_timestamps.ser", header);

        WriteOptions options;
        options.appendTimestamps = true;

        // Write frames with timestamps
        for (int i = 0; i < 10; ++i) {
            auto frame = generateTestFrame(640, 480, i);

            // Generate timestamp (current time + offset)
            uint64_t timestamp =
                SERTimestamp::now().nanoseconds + i * 1000000;  // 1ms apart

            writer.writeFrameWithTimestamp(frame, timestamp, options);
            cout << "Written frame " << i << " with timestamp " << timestamp
                 << "\n";
        }

        writer.finalize();

        cout << "Successfully created output_timestamps.ser\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate batch frame writing
 */
void demonstrateBatchWriting() {
    cout << "\n=== Batch Frame Writing ===\n";

    try {
        SERHeader header(320, 240, 8, SERColorID::Mono);

        SERWriter writer("output_batch.ser", header);

        // Generate multiple frames
        vector<cv::Mat> frames;
        for (int i = 0; i < 20; ++i) {
            frames.push_back(generateTestFrame(320, 240, i));
        }

        // Write all frames at once
        writer.writeFrames(frames);

        writer.finalize();

        cout << "Successfully wrote " << frames.size() << " frames in batch\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

int main() {
    demonstrateBasicWriting();
    demonstrateTimestampWriting();
    demonstrateBatchWriting();

    return 0;
}
