/**
 * @file basic_ser_operations.cpp
 * @brief Basic SER format file operations and header manipulation
 *
 * This example demonstrates:
 * - SER file reading and writing
 * - Header manipulation and metadata access
 * - Basic frame access and iteration
 * - SER format validation
 * - Color format handling
 * - Timestamp management
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Note: SER format support requires OpenCV
#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/opencv.hpp>
#include "atom/image/formats/ser/ser.hpp"
#include "atom/image/formats/ser/ser_format.h"
#include "atom/image/formats/ser/ser_reader.h"
#include "atom/image/formats/ser/ser_writer.h"
#endif

using namespace std;

#ifdef ATOM_IMAGE_HAS_OPENCVusing namespace serastro;

/**
 * @brief Create a sample SER header for testing
 */
SERHeader createSampleHeader() {
    SERHeader header;

    // Basic image properties
    header.imageWidth = 640;
    header.imageHeight = 480;
    header.pixelDepth = 8;
    header.colorID = static_cast<uint32_t>(SERColorID::Mono);
    header.frameCount = 100;

    // Metadata
    std::string observer = "Test Observer";
    std::string instrument = "Test Camera";
    std::string telescope = "Test Telescope";

    std::copy(observer.begin(), observer.end(), header.observer.begin());
    std::copy(instrument.begin(), instrument.end(), header.instrument.begin());
    std::copy(telescope.begin(), telescope.end(), header.telescope.begin());

    header.setCurrentDateTime();

    return header;
}

/**
 * @brief Generate test frame data
 */
cv::Mat generateTestFrame(int width, int height, int frameNumber) {
    cv::Mat frame(height, width, CV_8UC1);

    // Create a pattern that changes with frame number
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Create a moving pattern
            int value = (x + y + frameNumber * 2) % 256;
            frame.at<uint8_t>(y, x) = static_cast<uint8_t>(value);
        }
    }

    // Add some noise
    cv::Mat noise(height, width, CV_8UC1);
    cv::randu(noise, 0, 20);
    frame += noise;

    return frame;
}

/**
 * @brief Demonstrate basic SER header operations
 */
void demonstrateHeaderOperations() {
    cout << "\n=== SER Header Operations ===\n";

    try {
        // Create and display header
        SERHeader header = createSampleHeader();

        cout << "SER Header Information:\n";
        cout << "  File ID: " << string(header.fileID.data(), 14) << "\n";
        cout << "  Image dimensions: " << header.imageWidth << "x"
             << header.imageHeight << "\n";
        cout << "  Pixel depth: " << header.pixelDepth << " bits\n";
        cout << "  Color format: " << header.colorID << " (";

        switch (static_cast<SERColorID>(header.colorID)) {
            case SERColorID::Mono:
                cout << "Mono";
                break;
            case SERColorID::BayerRGGB:
                cout << "Bayer RGGB";
                break;
            case SERColorID::BayerGRBG:
                cout << "Bayer GRBG";
                break;
            case SERColorID::BayerGBRG:
                cout << "Bayer GBRG";
                break;
            case SERColorID::BayerBGGR:
                cout << "Bayer BGGR";
                break;
            case SERColorID::RGB:
                cout << "RGB";
                break;
            case SERColorID::BGR:
                cout << "BGR";
                break;
            default:
                cout << "Unknown";
                break;
        }
        cout << ")\n";

        cout << "  Frame count: " << header.frameCount << "\n";
        cout << "  Observer: " << string(header.observer.data()) << "\n";
        cout << "  Instrument: " << string(header.instrument.data()) << "\n";
        cout << "  Telescope: " << string(header.telescope.data()) << "\n";

        // Display timestamp
        auto timePoint = header.getDateTime();
        auto time_t = chrono::system_clock::to_time_t(timePoint);
        cout << "  Date/Time: "
             << put_time(localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\n";

        // Calculate file size information
        size_t frameSize = header.getFrameSize();
        size_t totalDataSize = frameSize * header.frameCount;

        cout << "  Frame size: " << frameSize << " bytes\n";
        cout << "  Total data size: " << (totalDataSize / (1024 * 1024))
             << " MB\n";

        // Test header validation
        bool isValid = header.isValid();
        cout << "  Header validation: " << (isValid ? "VALID" : "INVALID")
             << "\n";

    } catch (const exception& e) {
        cerr << "Error in header operations: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate SER file writing
 */
void demonstrateSERWriting() {
    cout << "\n=== SER File Writing ===\n";

    try {
        string filename = "test_output.ser";
        SERHeader header = createSampleHeader();

        // Reduce frame count for demo
        header.frameCount = 10;

        cout << "Creating SER file: " << filename << "\n";
        cout << "Writing " << header.frameCount << " frames...\n";

        SERWriter writer(filename);
        writer.writeHeader(header);

        // Write frames
        for (uint64_t i = 0; i < header.frameCount; ++i) {
            cv::Mat frame = generateTestFrame(
                header.imageWidth, header.imageHeight, static_cast<int>(i));

            // Create timestamp for this frame
            SERTimestamp timestamp = SERTimestamp::now();

            writer.writeFrame(frame, timestamp);

            if ((i + 1) % 5 == 0) {
                cout << "  Written " << (i + 1) << " frames\n";
            }
        }

        writer.close();
        cout << "SER file writing completed successfully\n";

        // Display file size
        ifstream file(filename, ios::binary | ios::ate);
        if (file) {
            size_t fileSize = file.tellg();
            cout << "Output file size: " << (fileSize / 1024) << " KB\n";
            file.close();
        }

    } catch (const exception& e) {
        cerr << "Error in SER writing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate SER file reading
 */
void demonstrateSERReading() {
    cout << "\n=== SER File Reading ===\n";

    try {
        string filename = "test_output.ser";

        cout << "Reading SER file: " << filename << "\n";

        SERReader reader(filename);
        SERHeader header = reader.getHeader();

        cout << "File header information:\n";
        cout << "  Dimensions: " << header.imageWidth << "x"
             << header.imageHeight << "\n";
        cout << "  Frame count: " << header.frameCount << "\n";
        cout << "  Pixel depth: " << header.pixelDepth << " bits\n";

        // Check if timestamps are available
        bool hasTimestamps = reader.hasTimestamps();
        cout << "  Timestamps available: " << (hasTimestamps ? "YES" : "NO")
             << "\n";

        // Read and analyze some frames
        cout << "\nReading frames:\n";

        vector<uint64_t> framesToRead = {0, header.frameCount / 4,
                                         header.frameCount / 2,
                                         header.frameCount - 1};

        for (uint64_t frameIndex : framesToRead) {
            if (frameIndex < header.frameCount) {
                cout << "  Frame " << frameIndex << ":\n";

                cv::Mat frame = reader.readFrame(frameIndex);

                cout << "    Size: " << frame.cols << "x" << frame.rows << "\n";
                cout << "    Type: " << frame.type() << "\n";
                cout << "    Channels: " << frame.channels() << "\n";

                // Calculate basic statistics
                cv::Scalar meanVal = cv::mean(frame);
                double minVal, maxVal;
                cv::minMaxLoc(frame, &minVal, &maxVal);

                cout << "    Mean value: " << fixed << setprecision(2)
                     << meanVal[0] << "\n";
                cout << "    Min/Max: " << minVal << "/" << maxVal << "\n";

                // Read timestamp if available
                if (hasTimestamps) {
                    auto timestamp = reader.getTimestamp(frameIndex);
                    if (timestamp.has_value()) {
                        auto timePoint = timestamp->toTimePoint();
                        auto time_t =
                            chrono::system_clock::to_time_t(timePoint);
                        cout << "    Timestamp: "
                             << put_time(localtime(&time_t), "%H:%M:%S")
                             << "\n";
                    }
                }
            }
        }

        // Test sequential reading performance
        cout << "\nPerformance test - sequential reading:\n";
        auto start = chrono::high_resolution_clock::now();

        for (uint64_t i = 0; i < min(header.frameCount, uint64_t(5)); ++i) {
            cv::Mat frame = reader.readFrame(i);
        }

        auto end = chrono::high_resolution_clock::now();
        auto duration =
            chrono::duration_cast<chrono::milliseconds>(end - start);

        cout << "  Read 5 frames in " << duration.count() << "ms\n";

        // Test random access performance
        cout << "\nPerformance test - random access:\n";
        start = chrono::high_resolution_clock::now();

        vector<uint64_t> randomIndices = {3, 1, 4, 0, 2};
        for (uint64_t index : randomIndices) {
            if (index < header.frameCount) {
                cv::Mat frame = reader.readFrame(index);
            }
        }

        end = chrono::high_resolution_clock::now();
        duration = chrono::duration_cast<chrono::milliseconds>(end - start);

        cout << "  Random access to 5 frames in " << duration.count() << "ms\n";

    } catch (const exception& e) {
        cerr << "Error in SER reading: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate different color formats
 */
void demonstrateColorFormats() {
    cout << "\n=== Color Format Handling ===\n";

    vector<pair<SERColorID, string>> colorFormats = {
        {SERColorID::Mono, "Monochrome"},
        {SERColorID::RGB, "RGB Color"},
        {SERColorID::BGR, "BGR Color"},
        {SERColorID::BayerRGGB, "Bayer RGGB"}};

    for (const auto& [colorId, name] : colorFormats) {
        cout << "Testing " << name << " format:\n";

        try {
            SERHeader header = createSampleHeader();
            header.colorID = static_cast<uint32_t>(colorId);
            header.frameCount = 3;

            // Adjust dimensions and create appropriate test frame
            cv::Mat testFrame;

            switch (colorId) {
                case SERColorID::Mono:
                    testFrame = generateTestFrame(header.imageWidth,
                                                  header.imageHeight, 0);
                    break;

                case SERColorID::RGB:
                case SERColorID::BGR:
                    testFrame =
                        cv::Mat(header.imageHeight, header.imageWidth, CV_8UC3);
                    for (int y = 0; y < header.imageHeight; ++y) {
                        for (int x = 0; x < header.imageWidth; ++x) {
                            cv::Vec3b& pixel = testFrame.at<cv::Vec3b>(y, x);
                            pixel[0] =
                                static_cast<uint8_t>((x + y) % 256);  // B or R
                            pixel[1] =
                                static_cast<uint8_t>((x * 2 + y) % 256);  // G
                            pixel[2] = static_cast<uint8_t>((x + y * 2) %
                                                            256);  // R or B
                        }
                    }
                    break;

                case SERColorID::BayerRGGB:
                    testFrame = generateTestFrame(header.imageWidth,
                                                  header.imageHeight, 0);
                    // Bayer pattern is stored as mono but interpreted
                    // differently
                    break;

                default:
                    continue;
            }

            cout << "  Frame type: " << testFrame.type() << "\n";
            cout << "  Channels: " << testFrame.channels() << "\n";
            cout << "  Frame size: " << header.getFrameSize() << " bytes\n";

            // Test if we can create a valid header
            bool isValid = header.isValid();
            cout << "  Header valid: " << (isValid ? "YES" : "NO") << "\n";

        } catch (const exception& e) {
            cout << "  Error: " << e.what() << "\n";
        }
    }
}

/**
 * @brief Demonstrate error handling and validation
 */
void demonstrateErrorHandling() {
    cout << "\n=== Error Handling and Validation ===\n";

    // Test invalid file reading
    cout << "Testing invalid file reading:\n";
    try {
        SERReader reader("nonexistent_file.ser");
        cout << "  ERROR: Should have failed!\n";
    } catch (const exception& e) {
        cout << "  Correctly caught error: " << e.what() << "\n";
    }

    // Test invalid header
    cout << "\nTesting invalid header:\n";
    try {
        SERHeader invalidHeader;
        invalidHeader.imageWidth = 0;   // Invalid
        invalidHeader.imageHeight = 0;  // Invalid
        invalidHeader.frameCount = 0;   // Invalid

        bool isValid = invalidHeader.isValid();
        cout << "  Invalid header validation: "
             << (isValid ? "FAILED" : "PASSED") << "\n";

    } catch (const exception& e) {
        cout << "  Error in validation: " << e.what() << "\n";
    }

    // Test writing with invalid parameters
    cout << "\nTesting writing with invalid parameters:\n";
    try {
        SERHeader header = createSampleHeader();
        header.imageWidth = 0;  // Make it invalid

        SERWriter writer("invalid_test.ser");
        writer.writeHeader(header);
        cout << "  ERROR: Should have failed!\n";
    } catch (const exception& e) {
        cout << "  Correctly caught error: " << e.what() << "\n";
    }
}

#endif  // ATOM_IMAGE_HAS_OPENCV

int main() {
    cout << "=== Atom Image Basic SER Operations Example ===\n";
    cout << "This example demonstrates basic SER format file operations\n";

#ifdef ATOM_IMAGE_HAS_OPENCV
    // Run all demonstrations
    demonstrateHeaderOperations();
    demonstrateSERWriting();
    demonstrateSERReading();
    demonstrateColorFormats();
    demonstrateErrorHandling();

    cout << "\n=== Basic SER operations example completed ===\n";
    cout << "\nNote: Test files created during this example:\n";
    cout << "- test_output.ser (can be deleted after testing)\n";
#else
    cout << "\nNote: This example requires OpenCV support.\n";
    cout << "Please build with: cmake -DATOM_IMAGE_HAS_OPENCV=ON\n";
#endif

    return 0;
}
