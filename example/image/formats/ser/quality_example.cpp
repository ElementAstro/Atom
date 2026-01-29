/**
 * @file quality_example.cpp
 * @brief Example demonstrating SER frame quality assessment
 *
 * This example covers:
 * - Assessing frame quality
 * - Using different quality metrics
 * - Sorting frames by quality
 * - Selecting best frames
 * - Custom quality metrics
 * - Detailed metric analysis
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iomanip>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

#include "atom/image/formats/ser/ser.hpp"

using namespace serastro;
using namespace std;

/**
 * @brief Generate test frames with varying quality
 */
vector<cv::Mat> generateTestFrames() {
    vector<cv::Mat> frames;

    for (int i = 0; i < 5; ++i) {
        cv::Mat frame(480, 640, CV_8UC1);

        // Add varying levels of noise and sharpness
        frame = cv::Scalar(100);

        // Add some features
        cv::circle(frame, cv::Point(320, 240), 50 - i * 5, cv::Scalar(200), -1);

        // Add noise (more noise = lower quality)
        cv::Mat noise(frame.size(), frame.type());
        cv::randn(noise, 0, 10 + i * 5);
        frame += noise;

        frames.push_back(frame);
    }

    return frames;
}

/**
 * @brief Demonstrate basic quality assessment
 */
void demonstrateBasicQuality() {
    cout << "\n=== Basic Quality Assessment ===\n";

    try {
        QualityAssessor assessor;
        auto frames = generateTestFrames();

        cout << "Assessing " << frames.size() << " frames:\n";

        for (size_t i = 0; i < frames.size(); ++i) {
            double quality = assessor.assessQuality(frames[i]);
            cout << "Frame " << i << " quality: " << fixed << setprecision(4)
                 << quality << "\n";
        }

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate different quality metrics
 */
void demonstrateDifferentMetrics() {
    cout << "\n=== Different Quality Metrics ===\n";

    try {
        QualityAssessor assessor;
        cv::Mat frame = generateTestFrames()[0];

        // Test different metrics
        vector<QualityMetric> metrics = {
            QualityMetric::Sharpness, QualityMetric::SNR,
            QualityMetric::Entropy,   QualityMetric::Brightness,
            QualityMetric::Contrast,  QualityMetric::Composite};

        vector<string> metricNames = {"Sharpness",  "SNR",      "Entropy",
                                      "Brightness", "Contrast", "Composite"};

        for (size_t i = 0; i < metrics.size(); ++i) {
            double value = assessor.getMetricValue(frame, metrics[i]);
            cout << metricNames[i] << ": " << fixed << setprecision(4) << value
                 << "\n";
        }

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate frame sorting and selection
 */
void demonstrateFrameSelection() {
    cout << "\n=== Frame Sorting and Selection ===\n";

    try {
        QualityAssessor assessor;
        auto frames = generateTestFrames();

        // Sort frames by quality
        auto sortedIndices = assessor.sortFramesByQuality(frames);

        cout << "Frames sorted by quality (best to worst):\n";
        for (size_t i = 0; i < sortedIndices.size(); ++i) {
            cout << "  Rank " << (i + 1) << ": Frame " << sortedIndices[i]
                 << "\n";
        }

        // Select best frames
        size_t bestCount = 3;
        auto bestFrames = assessor.selectBestFrames(frames, bestCount);

        cout << "\nSelected " << bestFrames.size() << " best frames\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate custom quality parameters
 */
void demonstrateCustomParameters() {
    cout << "\n=== Custom Quality Parameters ===\n";

    try {
        QualityParameters params;
        params.primaryMetric = QualityMetric::Sharpness;
        params.normalizeMetrics = true;
        params.roiSize = 0.5;  // Use center 50% of frame

        QualityAssessor assessor(params);
        auto frames = generateTestFrames();

        cout << "Using custom parameters (Sharpness metric, 50% ROI):\n";

        for (size_t i = 0; i < frames.size(); ++i) {
            double quality = assessor.assessQuality(frames[i]);
            cout << "Frame " << i << " quality: " << fixed << setprecision(4)
                 << quality << "\n";
        }

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

int main() {
    demonstrateBasicQuality();
    demonstrateDifferentMetrics();
    demonstrateFrameSelection();
    demonstrateCustomParameters();

    return 0;
}
