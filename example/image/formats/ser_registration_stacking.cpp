/**
 * @file ser_registration_stacking.cpp
 * @brief SER frame registration and stacking operations
 *
 * This example demonstrates:
 * - Frame registration algorithms
 * - Alignment transformation calculation
 * - Multiple stacking methods
 * - Quality-weighted stacking
 * - Astronomical image processing workflow
 * - Performance optimization techniques
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Note: SER format support requires OpenCV
#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/opencv.hpp>
#include "atom/image/formats/ser/quality.h"
#include "atom/image/formats/ser/registration.h"
#include "atom/image/formats/ser/ser.hpp"
#include "atom/image/formats/ser/stacking.h"
#endif

using namespace std;

#ifdef ATOM_IMAGE_HAS_OPENCVusing namespace serastro;

/**
 * @brief Generate test frames with simulated motion
 */
vector<cv::Mat> generateRegistrationTestFrames() {
    vector<cv::Mat> frames;
    const int width = 640;
    const int height = 480;

    // Create base frame with features
    cv::Mat baseFrame(height, width, CV_8UC1);
    baseFrame.setTo(30);  // Dark background

    // Add "stars" and features
    vector<cv::Point2f> starPositions = {
        cv::Point2f(100, 100), cv::Point2f(200, 150), cv::Point2f(300, 200),
        cv::Point2f(400, 120), cv::Point2f(150, 300), cv::Point2f(350, 350),
        cv::Point2f(500, 250), cv::Point2f(250, 400)};

    for (const auto& pos : starPositions) {
        cv::circle(baseFrame, pos, 3, cv::Scalar(200), -1);
        cv::circle(baseFrame, pos, 5, cv::Scalar(150), 1);
    }

    // Generate frames with different transformations
    vector<cv::Mat> transformMatrices;

    // Frame 0: Reference (no transformation)
    cv::Mat identity = cv::Mat::eye(2, 3, CV_32F);
    transformMatrices.push_back(identity);

    // Frame 1: Small translation
    cv::Mat trans1 = cv::Mat::eye(2, 3, CV_32F);
    trans1.at<float>(0, 2) = 5.0f;  // dx
    trans1.at<float>(1, 2) = 3.0f;  // dy
    transformMatrices.push_back(trans1);

    // Frame 2: Larger translation
    cv::Mat trans2 = cv::Mat::eye(2, 3, CV_32F);
    trans2.at<float>(0, 2) = -8.0f;
    trans2.at<float>(1, 2) = 6.0f;
    transformMatrices.push_back(trans2);

    // Frame 3: Translation with slight rotation
    cv::Point2f center(width / 2.0f, height / 2.0f);
    cv::Mat rot3 = cv::getRotationMatrix2D(center, 1.5, 1.0);
    rot3.at<double>(0, 2) += 4.0;
    rot3.at<double>(1, 2) += -2.0;
    transformMatrices.push_back(rot3);

    // Frame 4: Larger motion
    cv::Mat trans4 = cv::Mat::eye(2, 3, CV_32F);
    trans4.at<float>(0, 2) = 12.0f;
    trans4.at<float>(1, 2) = -10.0f;
    transformMatrices.push_back(trans4);

    // Apply transformations and add noise
    for (size_t i = 0; i < transformMatrices.size(); ++i) {
        cv::Mat transformedFrame;
        cv::warpAffine(baseFrame, transformedFrame, transformMatrices[i],
                       cv::Size(width, height));

        // Add noise (varying amounts)
        cv::Mat noise(height, width, CV_8UC1);
        int noiseLevel = 10 + static_cast<int>(i * 5);  // Increasing noise
        cv::randu(noise, 0, noiseLevel);
        transformedFrame += noise;

        frames.push_back(transformedFrame);
    }

    return frames;
}

/**
 * @brief Demonstrate frame registration methods
 */
void demonstrateFrameRegistration() {
    cout << "\n=== Frame Registration Methods ===\n";

    try {
        auto frames = generateRegistrationTestFrames();
        cout << "Generated " << frames.size()
             << " test frames with simulated motion\n";

        // Test different registration methods
        vector<pair<RegistrationMethod, string>> methods = {
            {RegistrationMethod::PhaseCorrelation, "Phase Correlation"},
            {RegistrationMethod::FeatureMatching, "Feature Matching"},
            {RegistrationMethod::ECC, "Enhanced Correlation Coefficient"},
            {RegistrationMethod::Template, "Template Matching"}};

        for (const auto& [method, name] : methods) {
            cout << "\nTesting " << name << " method:\n";

            try {
                RegistrationParameters params;
                params.method = method;
                params.subPixelAccuracy = true;
                params.maxIterations = 50;
                params.terminationEpsilon = 0.01;

                FrameRegistrar registrar(params);
                registrar.setReferenceFrame(
                    frames[0]);  // Use first frame as reference

                cout << "  Reference frame set (frame 0)\n";

                // Register each frame against reference
                for (size_t i = 1; i < frames.size(); ++i) {
                    auto start = chrono::high_resolution_clock::now();

                    FrameTransformation transform =
                        registrar.calculateTransformation(frames[i]);

                    auto end = chrono::high_resolution_clock::now();
                    auto duration = chrono::duration_cast<chrono::microseconds>(
                        end - start);

                    cout << "  Frame " << i << ": ";
                    cout << "dx=" << fixed << setprecision(2)
                         << transform.translation.x;
                    cout << ", dy=" << transform.translation.y;
                    cout << ", angle=" << transform.rotation;
                    cout << ", scale=" << transform.scale;
                    cout << ", confidence=" << transform.confidence;
                    cout << " (" << duration.count() << "μs)\n";
                }

            } catch (const exception& e) {
                cout << "  Error with " << name << ": " << e.what() << "\n";
            }
        }

    } catch (const exception& e) {
        cerr << "Error in frame registration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate frame stacking methods
 */
void demonstrateFrameStacking() {
    cout << "\n=== Frame Stacking Methods ===\n";

    try {
        auto frames = generateRegistrationTestFrames();

        // First register all frames
        RegistrationParameters regParams;
        regParams.method = RegistrationMethod::PhaseCorrelation;
        FrameRegistrar registrar(regParams);
        registrar.setReferenceFrame(frames[0]);

        vector<cv::Mat> registeredFrames;
        registeredFrames.push_back(frames[0]);  // Reference frame

        cout << "Registering frames for stacking...\n";
        for (size_t i = 1; i < frames.size(); ++i) {
            auto [registeredFrame, transform] =
                registrar.registerFrame(frames[i]);
            registeredFrames.push_back(registeredFrame);
        }

        // Test different stacking methods
        vector<pair<StackingMethod, string>> methods = {
            {StackingMethod::Mean, "Mean (Average)"},
            {StackingMethod::Median, "Median"},
            {StackingMethod::MaximumValue, "Maximum Value"},
            {StackingMethod::SigmaClipping, "Sigma Clipping"},
            {StackingMethod::WeightedAverage, "Weighted Average"}};

        for (const auto& [method, name] : methods) {
            cout << "\nTesting " << name << " stacking:\n";

            try {
                StackingParameters params;
                params.method = method;
                params.normalizeBeforeStacking = true;
                params.normalizeResult = true;

                if (method == StackingMethod::SigmaClipping) {
                    params.sigmaLow = 2.0;
                    params.sigmaHigh = 2.0;
                    params.iterations = 3;
                }

                FrameStacker stacker(params);

                auto start = chrono::high_resolution_clock::now();

                cv::Mat stackedResult;
                if (method == StackingMethod::WeightedAverage) {
                    // Create quality-based weights
                    QualityAssessor assessor;
                    vector<double> weights;
                    for (const auto& frame : registeredFrames) {
                        QualityMetrics metrics = assessor.assessFrame(frame);
                        weights.push_back(metrics.overallScore);
                    }
                    stackedResult = stacker.stackFramesWithWeights(
                        registeredFrames, weights);
                } else {
                    stackedResult = stacker.stackFrames(registeredFrames);
                }

                auto end = chrono::high_resolution_clock::now();
                auto duration =
                    chrono::duration_cast<chrono::milliseconds>(end - start);

                // Calculate statistics
                cv::Scalar meanVal = cv::mean(stackedResult);
                double minVal, maxVal;
                cv::minMaxLoc(stackedResult, &minVal, &maxVal);

                cout << "  Stacking time: " << duration.count() << "ms\n";
                cout << "  Result size: " << stackedResult.cols << "x"
                     << stackedResult.rows << "\n";
                cout << "  Mean value: " << fixed << setprecision(2)
                     << meanVal[0] << "\n";
                cout << "  Value range: [" << minVal << ", " << maxVal << "]\n";

                // Calculate noise reduction estimate
                double noiseReduction =
                    sqrt(static_cast<double>(registeredFrames.size()));
                cout << "  Theoretical noise reduction: " << setprecision(1)
                     << noiseReduction << "x\n";

            } catch (const exception& e) {
                cout << "  Error with " << name << ": " << e.what() << "\n";
            }
        }

    } catch (const exception& e) {
        cerr << "Error in frame stacking: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate complete astronomical processing workflow
 */
void demonstrateAstronomicalWorkflow() {
    cout << "\n=== Complete Astronomical Processing Workflow ===\n";

    try {
        auto frames = generateRegistrationTestFrames();
        cout << "Starting with " << frames.size() << " raw frames\n";

        // Step 1: Quality assessment and frame selection
        cout << "\nStep 1: Quality Assessment\n";
        QualityAssessor assessor;
        vector<pair<int, double>> frameQualities;

        for (size_t i = 0; i < frames.size(); ++i) {
            QualityMetrics metrics = assessor.assessFrame(frames[i]);
            frameQualities.push_back(
                {static_cast<int>(i), metrics.overallScore});
            cout << "  Frame " << i << ": quality = " << fixed
                 << setprecision(3) << metrics.overallScore << "\n";
        }

        // Select frames above quality threshold
        double qualityThreshold = 0.3;
        vector<cv::Mat> selectedFrames;
        vector<int> selectedIndices;

        for (const auto& [index, quality] : frameQualities) {
            if (quality > qualityThreshold) {
                selectedFrames.push_back(frames[index]);
                selectedIndices.push_back(index);
            }
        }

        cout << "  Selected " << selectedFrames.size()
             << " frames above quality threshold " << qualityThreshold << "\n";
        cout << "  Selected frame indices: ";
        for (int idx : selectedIndices)
            cout << idx << " ";
        cout << "\n";

        // Step 2: Frame registration
        cout << "\nStep 2: Frame Registration\n";
        if (selectedFrames.empty()) {
            cout << "  No frames selected for registration\n";
            return;
        }

        RegistrationParameters regParams;
        regParams.method = RegistrationMethod::PhaseCorrelation;
        regParams.subPixelAccuracy = true;

        FrameRegistrar registrar(regParams);
        registrar.setReferenceFrame(selectedFrames[0]);

        vector<cv::Mat> alignedFrames;
        vector<FrameTransformation> transformations;

        alignedFrames.push_back(selectedFrames[0]);  // Reference frame

        for (size_t i = 1; i < selectedFrames.size(); ++i) {
            auto [alignedFrame, transform] =
                registrar.registerFrame(selectedFrames[i]);
            alignedFrames.push_back(alignedFrame);
            transformations.push_back(transform);

            cout << "  Frame " << selectedIndices[i] << " aligned: ";
            cout << "offset=(" << fixed << setprecision(2)
                 << transform.translation.x;
            cout << ", " << transform.translation.y << "), ";
            cout << "confidence=" << transform.confidence << "\n";
        }

        // Step 3: Quality-weighted stacking
        cout << "\nStep 3: Quality-Weighted Stacking\n";

        // Calculate weights based on quality and registration confidence
        vector<double> stackingWeights;
        stackingWeights.push_back(
            frameQualities[selectedIndices[0]].second);  // Reference frame

        for (size_t i = 0; i < transformations.size(); ++i) {
            double qualityWeight =
                frameQualities[selectedIndices[i + 1]].second;
            double confidenceWeight = transformations[i].confidence;
            double combinedWeight = qualityWeight * confidenceWeight;
            stackingWeights.push_back(combinedWeight);
        }

        cout << "  Stacking weights: ";
        for (size_t i = 0; i < stackingWeights.size(); ++i) {
            cout << fixed << setprecision(3) << stackingWeights[i] << " ";
        }
        cout << "\n";

        StackingParameters stackParams;
        stackParams.method = StackingMethod::WeightedAverage;
        stackParams.normalizeBeforeStacking = true;
        stackParams.normalizeResult = true;

        FrameStacker stacker(stackParams);

        auto start = chrono::high_resolution_clock::now();
        cv::Mat finalResult =
            stacker.stackFramesWithWeights(alignedFrames, stackingWeights);
        auto end = chrono::high_resolution_clock::now();
        auto duration =
            chrono::duration_cast<chrono::milliseconds>(end - start);

        // Step 4: Final result analysis
        cout << "\nStep 4: Final Result Analysis\n";
        cout << "  Processing time: " << duration.count() << "ms\n";
        cout << "  Final image size: " << finalResult.cols << "x"
             << finalResult.rows << "\n";

        // Compare with simple average
        cv::Mat simpleAverage;
        cv::Mat stackedFrames;
        cv::merge(alignedFrames, stackedFrames);
        cv::reduce(stackedFrames, simpleAverage, 2, cv::REDUCE_AVG);

        // Calculate improvement metrics
        QualityMetrics finalQuality = assessor.assessFrame(finalResult);
        QualityMetrics averageQuality = assessor.assessFrame(simpleAverage);

        cout << "  Weighted stacking quality: " << fixed << setprecision(3)
             << finalQuality.overallScore << "\n";
        cout << "  Simple average quality: " << averageQuality.overallScore
             << "\n";
        cout << "  Quality improvement: "
             << ((finalQuality.overallScore / averageQuality.overallScore -
                  1.0) *
                 100)
             << "%\n";

        double theoreticalSNRImprovement =
            sqrt(static_cast<double>(alignedFrames.size()));
        cout << "  Theoretical SNR improvement: " << setprecision(1)
             << theoreticalSNRImprovement << "x\n";
        cout << "  Actual SNR improvement: "
             << (finalQuality.snr / averageQuality.snr) << "x\n";

    } catch (const exception& e) {
        cerr << "Error in astronomical workflow: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate performance optimization techniques
 */
void demonstratePerformanceOptimization() {
    cout << "\n=== Performance Optimization ===\n";

    try {
        // Test with different frame counts
        vector<int> frameCounts = {5, 10, 20};

        for (int frameCount : frameCounts) {
            cout << "\nTesting with " << frameCount << " frames:\n";

            // Generate frames
            auto allFrames = generateRegistrationTestFrames();
            vector<cv::Mat> testFrames(
                allFrames.begin(),
                allFrames.begin() +
                    min(frameCount, static_cast<int>(allFrames.size())));

            // Measure registration time
            auto start = chrono::high_resolution_clock::now();

            FrameRegistrar registrar;
            registrar.setReferenceFrame(testFrames[0]);

            vector<cv::Mat> registeredFrames;
            registeredFrames.push_back(testFrames[0]);

            for (size_t i = 1; i < testFrames.size(); ++i) {
                auto [registered, transform] =
                    registrar.registerFrame(testFrames[i]);
                registeredFrames.push_back(registered);
            }

            auto regEnd = chrono::high_resolution_clock::now();
            auto regDuration =
                chrono::duration_cast<chrono::milliseconds>(regEnd - start);

            // Measure stacking time
            FrameStacker stacker;
            cv::Mat stacked = stacker.stackFrames(registeredFrames);

            auto stackEnd = chrono::high_resolution_clock::now();
            auto stackDuration =
                chrono::duration_cast<chrono::milliseconds>(stackEnd - regEnd);
            auto totalDuration =
                chrono::duration_cast<chrono::milliseconds>(stackEnd - start);

            cout << "  Registration time: " << regDuration.count() << "ms\n";
            cout << "  Stacking time: " << stackDuration.count() << "ms\n";
            cout << "  Total time: " << totalDuration.count() << "ms\n";
            cout << "  Time per frame: "
                 << (totalDuration.count() / testFrames.size()) << "ms\n";

            // Memory usage estimate
            size_t frameSize = testFrames[0].total() * testFrames[0].elemSize();
            size_t totalMemory =
                frameSize * testFrames.size() * 2;  // Original + registered
            cout << "  Memory usage: ~" << (totalMemory / (1024 * 1024))
                 << "MB\n";
        }

    } catch (const exception& e) {
        cerr << "Error in performance optimization: " << e.what() << "\n";
    }
}

#endif  // ATOM_IMAGE_HAS_OPENCV

int main() {
    cout << "=== Atom Image SER Registration and Stacking Example ===\n";
    cout << "This example demonstrates SER frame registration and stacking "
            "operations\n";

#ifdef ATOM_IMAGE_HAS_OPENCV
    // Run all demonstrations
    demonstrateFrameRegistration();
    demonstrateFrameStacking();
    demonstrateAstronomicalWorkflow();
    demonstratePerformanceOptimization();

    cout << "\n=== SER registration and stacking example completed ===\n";
    cout << "\nKey techniques demonstrated:\n";
    cout << "- Multiple registration algorithms (phase correlation, feature "
            "matching, etc.)\n";
    cout << "- Various stacking methods (mean, median, sigma clipping, "
            "weighted)\n";
    cout << "- Complete astronomical processing workflow\n";
    cout << "- Quality-based frame selection and weighting\n";
    cout << "- Performance optimization for batch processing\n";
#else
    cout << "\nNote: This example requires OpenCV support.\n";
    cout << "Please build with: cmake -DATOM_IMAGE_HAS_OPENCV=ON\n";
#endif

    return 0;
}
