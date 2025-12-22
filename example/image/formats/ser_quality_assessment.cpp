/**
 * @file ser_quality_assessment.cpp
 * @brief SER frame quality assessment and ranking
 *
 * This example demonstrates:
 * - Frame quality metrics calculation
 * - Quality-based frame ranking
 * - Automatic frame selection
 * - Quality threshold filtering
 * - Statistical quality analysis
 * - Custom quality metrics
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Note: SER format support requires OpenCV
#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/opencv.hpp>
#include "atom/image/formats/ser/quality.h"
#include "atom/image/formats/ser/ser.hpp"
#include "atom/image/formats/ser/ser_reader.h"
#endif

using namespace std;

#ifdef ATOM_IMAGE_HAS_OPENCVusing namespace serastro;

/**
 * @brief Generate test frames with varying quality
 */
vector<cv::Mat> generateQualityTestFrames() {
    vector<cv::Mat> frames;
    const int width = 640;
    const int height = 480;

    // Frame 1: High quality - sharp with low noise
    cv::Mat frame1(height, width, CV_8UC1);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Sharp geometric pattern
            int value = ((x / 20) % 2) * ((y / 20) % 2) * 255;
            frame1.at<uint8_t>(y, x) = static_cast<uint8_t>(value);
        }
    }
    // Add minimal noise
    cv::Mat noise1(height, width, CV_8UC1);
    cv::randu(noise1, 0, 10);
    frame1 += noise1;
    frames.push_back(frame1);

    // Frame 2: Medium quality - less sharp, moderate noise
    cv::Mat frame2(height, width, CV_8UC1);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Softer pattern
            double dx = x - width / 2.0;
            double dy = y - height / 2.0;
            double dist = sqrt(dx * dx + dy * dy);
            int value = static_cast<int>(128 + 100 * sin(dist / 20.0));
            frame2.at<uint8_t>(y, x) =
                static_cast<uint8_t>(max(0, min(255, value)));
        }
    }
    // Add moderate noise
    cv::Mat noise2(height, width, CV_8UC1);
    cv::randu(noise2, 0, 30);
    frame2 += noise2;
    frames.push_back(frame2);

    // Frame 3: Low quality - blurry with high noise
    cv::Mat frame3(height, width, CV_8UC1);
    frame3.setTo(128);  // Uniform background
    // Add some weak features
    cv::circle(frame3, cv::Point(width / 2, height / 2), 50, cv::Scalar(200),
               -1);
    cv::circle(frame3, cv::Point(width / 4, height / 4), 30, cv::Scalar(180),
               -1);
    // Blur the image
    cv::GaussianBlur(frame3, frame3, cv::Size(15, 15), 5.0);
    // Add high noise
    cv::Mat noise3(height, width, CV_8UC1);
    cv::randu(noise3, 0, 60);
    frame3 += noise3;
    frames.push_back(frame3);

    // Frame 4: Very low quality - mostly noise
    cv::Mat frame4(height, width, CV_8UC1);
    cv::randu(frame4, 50, 200);  // Random noise
    frames.push_back(frame4);

    // Frame 5: Good quality with stars (astronomical simulation)
    cv::Mat frame5(height, width, CV_8UC1);
    frame5.setTo(20);  // Dark sky background

    // Add "stars" at random positions
    cv::RNG rng(42);  // Fixed seed for reproducibility
    for (int i = 0; i < 50; ++i) {
        int x = rng.uniform(10, width - 10);
        int y = rng.uniform(10, height - 10);
        int brightness = rng.uniform(150, 255);

        // Create star with Gaussian profile
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                if (y + dy >= 0 && y + dy < height && x + dx >= 0 &&
                    x + dx < width) {
                    double dist = sqrt(dx * dx + dy * dy);
                    double intensity = brightness * exp(-dist * dist / 2.0);
                    uint8_t& pixel = frame5.at<uint8_t>(y + dy, x + dx);
                    pixel = static_cast<uint8_t>(min(255.0, pixel + intensity));
                }
            }
        }
    }

    // Add minimal noise to astronomical frame
    cv::Mat noise5(height, width, CV_8UC1);
    cv::randu(noise5, 0, 15);
    frame5 += noise5;
    frames.push_back(frame5);

    return frames;
}

/**
 * @brief Demonstrate basic quality assessment
 */
void demonstrateBasicQualityAssessment() {
    cout << "\n=== Basic Quality Assessment ===\n";

    try {
        // Create quality assessor with default parameters
        QualityAssessor assessor;

        // Generate test frames
        auto frames = generateQualityTestFrames();

        cout << "Assessing quality of " << frames.size() << " test frames:\n";

        vector<pair<int, QualityMetrics>> frameQualities;

        for (size_t i = 0; i < frames.size(); ++i) {
            cout << "\nFrame " << i << ":\n";

            QualityMetrics metrics = assessor.assessFrame(frames[i]);
            frameQualities.push_back({static_cast<int>(i), metrics});

            cout << "  Sharpness: " << fixed << setprecision(3)
                 << metrics.sharpness << "\n";
            cout << "  SNR: " << metrics.snr << "\n";
            cout << "  Entropy: " << metrics.entropy << "\n";
            cout << "  Brightness: " << metrics.brightness << "\n";
            cout << "  Contrast: " << metrics.contrast << "\n";
            cout << "  Star count: " << metrics.starCount << "\n";
            cout << "  Overall score: " << metrics.overallScore << "\n";

            // Classify quality
            string qualityClass;
            if (metrics.overallScore > 0.8)
                qualityClass = "Excellent";
            else if (metrics.overallScore > 0.6)
                qualityClass = "Good";
            else if (metrics.overallScore > 0.4)
                qualityClass = "Fair";
            else if (metrics.overallScore > 0.2)
                qualityClass = "Poor";
            else
                qualityClass = "Very Poor";

            cout << "  Quality class: " << qualityClass << "\n";
        }

        // Rank frames by quality
        cout << "\n=== Frame Ranking ===\n";

        auto rankedFrames = assessor.rankFramesByQuality(frames);

        cout << "Frames ranked by quality (best to worst):\n";
        for (size_t i = 0; i < rankedFrames.size(); ++i) {
            int frameIndex = rankedFrames[i].first;
            double score = rankedFrames[i].second;
            cout << "  Rank " << (i + 1) << ": Frame " << frameIndex
                 << " (score: " << fixed << setprecision(3) << score << ")\n";
        }

    } catch (const exception& e) {
        cerr << "Error in basic quality assessment: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate custom quality parameters
 */
void demonstrateCustomQualityParameters() {
    cout << "\n=== Custom Quality Parameters ===\n";

    try {
        // Create assessor with custom parameters
        QualityParameters params;
        params.enableSharpness = true;
        params.enableSNR = true;
        params.enableEntropy = true;
        params.enableBrightness =
            false;  // Disable brightness for dark astronomical images
        params.enableContrast = true;
        params.enableStarCount = true;  // Enable for astronomical images

        // Adjust weights for astronomical imaging
        params.sharpnessWeight = 0.3;
        params.snrWeight = 0.3;
        params.entropyWeight = 0.1;
        params.contrastWeight = 0.1;
        params.starCountWeight = 0.2;  // Higher weight for star count

        // Set ROI for quality assessment (center region)
        params.useROI = true;
        params.roiX = 0.25;      // 25% from left
        params.roiY = 0.25;      // 25% from top
        params.roiWidth = 0.5;   // 50% width
        params.roiHeight = 0.5;  // 50% height

        QualityAssessor customAssessor(params);

        auto frames = generateQualityTestFrames();

        cout << "Custom quality assessment (optimized for astronomy):\n";

        for (size_t i = 0; i < frames.size(); ++i) {
            QualityMetrics metrics = customAssessor.assessFrame(frames[i]);

            cout << "Frame " << i << ": ";
            cout << "Score=" << fixed << setprecision(3)
                 << metrics.overallScore;
            cout << ", Stars=" << metrics.starCount;
            cout << ", Sharpness=" << metrics.sharpness;
            cout << ", SNR=" << metrics.snr << "\n";
        }

        // Compare with default assessment
        cout << "\nComparison with default parameters:\n";
        QualityAssessor defaultAssessor;

        for (size_t i = 0; i < frames.size(); ++i) {
            QualityMetrics defaultMetrics =
                defaultAssessor.assessFrame(frames[i]);
            QualityMetrics customMetrics =
                customAssessor.assessFrame(frames[i]);

            cout << "Frame " << i << ": ";
            cout << "Default=" << fixed << setprecision(3)
                 << defaultMetrics.overallScore;
            cout << ", Custom=" << customMetrics.overallScore;
            cout << " (diff="
                 << (customMetrics.overallScore - defaultMetrics.overallScore)
                 << ")\n";
        }

    } catch (const exception& e) {
        cerr << "Error in custom quality parameters: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate quality-based frame selection
 */
void demonstrateFrameSelection() {
    cout << "\n=== Quality-Based Frame Selection ===\n";

    try {
        QualityAssessor assessor;
        auto frames = generateQualityTestFrames();

        // Select best frames with different criteria
        vector<double> thresholds = {0.8, 0.6, 0.4, 0.2};

        for (double threshold : thresholds) {
            cout << "Selecting frames with quality > " << threshold << ":\n";

            vector<int> selectedFrames;
            for (size_t i = 0; i < frames.size(); ++i) {
                QualityMetrics metrics = assessor.assessFrame(frames[i]);
                if (metrics.overallScore > threshold) {
                    selectedFrames.push_back(static_cast<int>(i));
                }
            }

            cout << "  Selected " << selectedFrames.size() << " frames: ";
            for (int frameIndex : selectedFrames) {
                cout << frameIndex << " ";
            }
            cout << "\n";
        }

        // Select top N frames
        vector<int> topCounts = {1, 2, 3};

        for (int topN : topCounts) {
            cout << "\nSelecting top " << topN << " frames:\n";

            auto rankedFrames = assessor.rankFramesByQuality(frames);

            cout << "  Selected frames: ";
            for (int i = 0;
                 i < min(topN, static_cast<int>(rankedFrames.size())); ++i) {
                cout << rankedFrames[i].first << " ";
            }
            cout << "\n";
        }

    } catch (const exception& e) {
        cerr << "Error in frame selection: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate statistical quality analysis
 */
void demonstrateStatisticalAnalysis() {
    cout << "\n=== Statistical Quality Analysis ===\n";

    try {
        QualityAssessor assessor;
        auto frames = generateQualityTestFrames();

        // Collect all quality metrics
        vector<QualityMetrics> allMetrics;
        for (const auto& frame : frames) {
            allMetrics.push_back(assessor.assessFrame(frame));
        }

        // Calculate statistics for each metric
        auto calculateStats = [](const vector<double>& values) {
            if (values.empty())
                return make_tuple(0.0, 0.0, 0.0, 0.0);

            double sum = 0.0;
            double minVal = values[0];
            double maxVal = values[0];

            for (double val : values) {
                sum += val;
                minVal = min(minVal, val);
                maxVal = max(maxVal, val);
            }

            double mean = sum / values.size();

            // Calculate standard deviation
            double variance = 0.0;
            for (double val : values) {
                variance += (val - mean) * (val - mean);
            }
            double stddev = sqrt(variance / values.size());

            return make_tuple(mean, stddev, minVal, maxVal);
        };

        // Extract individual metrics
        vector<double> sharpnessValues, snrValues, entropyValues, overallScores;
        for (const auto& metrics : allMetrics) {
            sharpnessValues.push_back(metrics.sharpness);
            snrValues.push_back(metrics.snr);
            entropyValues.push_back(metrics.entropy);
            overallScores.push_back(metrics.overallScore);
        }

        cout << "Quality Statistics Summary:\n";
        cout << fixed << setprecision(3);

        auto [sharpMean, sharpStd, sharpMin, sharpMax] =
            calculateStats(sharpnessValues);
        cout << "Sharpness: mean=" << sharpMean << ", std=" << sharpStd
             << ", range=[" << sharpMin << ", " << sharpMax << "]\n";

        auto [snrMean, snrStd, snrMin, snrMax] = calculateStats(snrValues);
        cout << "SNR: mean=" << snrMean << ", std=" << snrStd << ", range=["
             << snrMin << ", " << snrMax << "]\n";

        auto [entMean, entStd, entMin, entMax] = calculateStats(entropyValues);
        cout << "Entropy: mean=" << entMean << ", std=" << entStd << ", range=["
             << entMin << ", " << entMax << "]\n";

        auto [scoreMean, scoreStd, scoreMin, scoreMax] =
            calculateStats(overallScores);
        cout << "Overall Score: mean=" << scoreMean << ", std=" << scoreStd
             << ", range=[" << scoreMin << ", " << scoreMax << "]\n";

        // Quality distribution
        cout << "\nQuality Distribution:\n";
        map<string, int> qualityDistribution;
        for (double score : overallScores) {
            if (score > 0.8)
                qualityDistribution["Excellent"]++;
            else if (score > 0.6)
                qualityDistribution["Good"]++;
            else if (score > 0.4)
                qualityDistribution["Fair"]++;
            else if (score > 0.2)
                qualityDistribution["Poor"]++;
            else
                qualityDistribution["Very Poor"]++;
        }

        for (const auto& [quality, count] : qualityDistribution) {
            cout << "  " << quality << ": " << count << " frames\n";
        }

    } catch (const exception& e) {
        cerr << "Error in statistical analysis: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate performance characteristics
 */
void demonstratePerformanceCharacteristics() {
    cout << "\n=== Performance Characteristics ===\n";

    try {
        QualityAssessor assessor;
        auto frames = generateQualityTestFrames();

        // Measure assessment time for different frame sizes
        vector<cv::Size> testSizes = {
            cv::Size(320, 240),   // Small
            cv::Size(640, 480),   // Medium
            cv::Size(1280, 960),  // Large
            cv::Size(1920, 1080)  // HD
        };

        for (const auto& size : testSizes) {
            cout << "Testing " << size.width << "x" << size.height
                 << " frames:\n";

            // Create test frame of specified size
            cv::Mat testFrame(size.height, size.width, CV_8UC1);
            cv::randu(testFrame, 0, 255);

            // Measure assessment time
            const int iterations = 10;
            auto start = chrono::high_resolution_clock::now();

            for (int i = 0; i < iterations; ++i) {
                QualityMetrics metrics = assessor.assessFrame(testFrame);
            }

            auto end = chrono::high_resolution_clock::now();
            auto duration =
                chrono::duration_cast<chrono::microseconds>(end - start);

            double avgTime = static_cast<double>(duration.count()) / iterations;
            double pixelsPerSecond =
                (size.width * size.height * iterations * 1000000.0) /
                duration.count();

            cout << "  Average assessment time: " << fixed << setprecision(2)
                 << avgTime << " μs\n";
            cout << "  Processing rate: " << (pixelsPerSecond / 1000000.0)
                 << " Mpixels/s\n";
        }

    } catch (const exception& e) {
        cerr << "Error in performance testing: " << e.what() << "\n";
    }
}

#endif  // ATOM_IMAGE_HAS_OPENCV

int main() {
    cout << "=== Atom Image SER Quality Assessment Example ===\n";
    cout << "This example demonstrates SER frame quality assessment and "
            "ranking\n";

#ifdef ATOM_IMAGE_HAS_OPENCV
    // Run all demonstrations
    demonstrateBasicQualityAssessment();
    demonstrateCustomQualityParameters();
    demonstrateFrameSelection();
    demonstrateStatisticalAnalysis();
    demonstratePerformanceCharacteristics();

    cout << "\n=== SER quality assessment example completed ===\n";
    cout << "\nKey capabilities demonstrated:\n";
    cout << "- Multiple quality metrics (sharpness, SNR, entropy, etc.)\n";
    cout << "- Customizable quality parameters and weights\n";
    cout << "- Quality-based frame ranking and selection\n";
    cout << "- Statistical analysis of quality distributions\n";
    cout << "- Performance characteristics for different frame sizes\n";
#else
    cout << "\nNote: This example requires OpenCV support.\n";
    cout << "Please build with: cmake -DATOM_IMAGE_HAS_OPENCV=ON\n";
#endif

    return 0;
}
