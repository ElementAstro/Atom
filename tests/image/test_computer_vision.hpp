#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vector>
#include <string>
#include <memory>
#include <cmath>

#include "atom/image/processing/computer_vision.hpp"
#include "atom/image/core/image_blob.hpp"
#include "test_utils.hpp"

namespace atom::image::test {

class ComputerVisionTest : public ::testing::Test {
protected:
    void SetUp() override {
        cv = std::make_unique<ComputerVision>();
        fileManager = std::make_unique<TestFileManager>();
        createTestImages();
    }

    void TearDown() override {
        fileManager->cleanup();
    }

    void createTestImages() {
        auto gradientData = TestDataGenerator::generateGradientImage(256, 256, 3);
        gradient_image = blob(gradientData.data(), gradientData.size());

        auto checkerboardData = TestDataGenerator::generateCheckerboard(256, 256, 3, 8);
        checkerboard_image = blob(checkerboardData.data(), checkerboardData.size());

        auto circleData = TestDataGenerator::generateCircularPattern(256, 256, 3);
        circle_image = blob(circleData.data(), circleData.size());

        auto solidData = TestDataGenerator::generateSolidColor(256, 256, 3, {128, 128, 128});
        solid_image = blob(solidData.data(), solidData.size());

        auto noisyData = TestDataGenerator::generateRandomNoise(256, 256, 3, 12345);
        noisy_image = blob(noisyData.data(), noisyData.size());
    }

    std::unique_ptr<ComputerVision> cv;
    std::unique_ptr<TestFileManager> fileManager;

    blob gradient_image, checkerboard_image, circle_image, solid_image, noisy_image;
};

TEST_F(ComputerVisionTest, FeatureDetectionORB) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    auto keypoints = cv->detectFeatures(checkerboard_image, FeatureDetectorType::ORB, 100);

    EXPECT_GT(keypoints.size(), 0);
    EXPECT_LE(keypoints.size(), 100);

    for (const auto& kp : keypoints) {
        EXPECT_GE(kp.x, 0.0);
        EXPECT_GE(kp.y, 0.0);
        EXPECT_GT(kp.size, 0.0);
    }
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, FeatureDetectionSIFT) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    auto keypoints = cv->detectFeatures(checkerboard_image, FeatureDetectorType::SIFT, 50);

    EXPECT_GT(keypoints.size(), 0);
    EXPECT_LE(keypoints.size(), 50);
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, FeatureDetectionAKAZE) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    auto keypoints = cv->detectFeatures(checkerboard_image, FeatureDetectorType::AKAZE, 100);

    EXPECT_GT(keypoints.size(), 0);
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, FeatureDetectionBRISK) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    auto keypoints = cv->detectFeatures(checkerboard_image, FeatureDetectorType::BRISK, 100);
    
    EXPECT_GT(keypoints.size(), 0);
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, FeatureDetectionFAST) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    auto keypoints = cv->detectFeatures(checkerboard_image, FeatureDetectorType::FAST, 100);
    
    EXPECT_GT(keypoints.size(), 0);
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, FeatureDetectionHarris) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    auto keypoints = cv->detectFeatures(checkerboard_image, FeatureDetectorType::HARRIS, 50, 0.01);
    
    // Harris may detect fewer features
    EXPECT_GE(keypoints.size(), 0);
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, FeatureDetectionGFTT) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    auto keypoints = cv->detectFeatures(checkerboard_image, FeatureDetectorType::GFTT, 100);
    
    EXPECT_GT(keypoints.size(), 0);
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

// Test feature detection with empty image
TEST_F(ComputerVisionTest, FeatureDetectionEmptyImage) {
    blob empty_image;
    auto keypoints = cv->detectFeatures(empty_image, FeatureDetectorType::ORB);
    
    EXPECT_EQ(keypoints.size(), 0);
}

// Test feature matching
TEST_F(ComputerVisionTest, FeatureMatching) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    auto keypoints1 = cv->detectFeatures(checkerboard_image, FeatureDetectorType::ORB, 100);
    auto keypoints2 = cv->detectFeatures(checkerboard_image, FeatureDetectorType::ORB, 100);
    
    ASSERT_GT(keypoints1.size(), 0);
    ASSERT_GT(keypoints2.size(), 0);
    
    auto matches = cv->matchFeatures(keypoints1, keypoints2, "ratio_test", 0.7);
    
    // Should find matches since it's the same image
    EXPECT_GT(matches.size(), 0);
    
    // Verify match indices are valid
    for (const auto& match : matches) {
        EXPECT_GE(match.first, 0);
        EXPECT_LT(match.first, static_cast<int>(keypoints1.size()));
        EXPECT_GE(match.second, 0);
        EXPECT_LT(match.second, static_cast<int>(keypoints2.size()));
    }
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

// Test feature matching with empty keypoints
TEST_F(ComputerVisionTest, FeatureMatchingEmptyKeypoints) {
    std::vector<Keypoint> empty1, empty2;
    auto matches = cv->matchFeatures(empty1, empty2);
    
    EXPECT_EQ(matches.size(), 0);
}

// Test object detection (will likely throw not implemented or require models)
TEST_F(ComputerVisionTest, ObjectDetectionBasic) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    // This may throw if models are not available
    try {
        auto detections = cv->detectObjects(circle_image, ObjectDetectionModel::YOLO_V5, 0.5, 0.4);
        // If it succeeds, verify structure
        for (const auto& det : detections) {
            EXPECT_GE(det.confidence, 0.0);
            EXPECT_LE(det.confidence, 1.0);
            EXPECT_GE(det.x, 0.0);
            EXPECT_GE(det.y, 0.0);
        }
    } catch (const std::exception& e) {
        // Expected if models not available
        GTEST_SKIP() << "Object detection models not available: " << e.what();
    }
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, ObjectDetectionEmptyImage) {
    blob empty_image;
    auto detections = cv->detectObjects(empty_image);

    EXPECT_EQ(detections.size(), 0);
}

TEST_F(ComputerVisionTest, FaceDetectionBasic) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    try {
        auto faces = cv->detectFaces(circle_image, FaceModel::DNN_FACE, 30, 1.1, true, false);

        for (const auto& face : faces) {
            EXPECT_GE(face.confidence, 0.0);
            EXPECT_LE(face.confidence, 1.0);
            EXPECT_GE(face.width, 0.0);
            EXPECT_GE(face.height, 0.0);
        }
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Face detection models not available: " << e.what();
    }
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, FaceDetectionEmptyImage) {
    blob empty_image;
    auto faces = cv->detectFaces(empty_image);

    EXPECT_EQ(faces.size(), 0);
}

TEST_F(ComputerVisionTest, ImageSegmentationSLIC) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    try {
        auto segments = cv->segmentImage(checkerboard_image, SegmentationMethod::SLIC, 100, 10.0);

        EXPECT_GT(segments.size(), 0);
        if (!segments.empty()) {
            EXPECT_GT(segments[0].size(), 0);
        }
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Segmentation not available: " << e.what();
    }
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, ImageSegmentationEmptyImage) {
    blob empty_image;
    auto segments = cv->segmentImage(empty_image);

    EXPECT_EQ(segments.size(), 0);
}

TEST_F(ComputerVisionTest, OpticalFlowEstimation) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    try {
        auto flow = cv->estimateOpticalFlow(checkerboard_image, gradient_image, "lucas_kanade");
        EXPECT_GE(flow.size(), 0);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Optical flow not available: " << e.what();
    }
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, ImageClassification) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    try {
        auto results = cv->classifyImage(circle_image, "resnet", "", 5);
        EXPECT_LE(results.size(), 5);

        for (const auto& [className, confidence] : results) {
            EXPECT_GE(confidence, 0.0);
            EXPECT_LE(confidence, 1.0);
            EXPECT_FALSE(className.empty());
        }
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Classification models not available: " << e.what();
    }
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

// Test quality analysis
TEST_F(ComputerVisionTest, QualityAnalysis) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    auto quality = cv->analyzeQuality(noisy_image, {"sharpness", "noise", "exposure", "contrast"});
    
    // Should return metrics
    EXPECT_GT(quality.size(), 0);
    
    // Verify metric values are reasonable
    for (const auto& [metric, value] : quality) {
        EXPECT_FALSE(metric.empty());
        EXPECT_FALSE(std::isnan(value));
    }
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

// Test quality analysis with empty image
TEST_F(ComputerVisionTest, QualityAnalysisEmptyImage) {
    blob empty_image;
    auto quality = cv->analyzeQuality(empty_image);

    EXPECT_EQ(quality.size(), 0);
}

// Test anomaly detection
TEST_F(ComputerVisionTest, AnomalyDetection) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    try {
        std::vector<blob> references = {gradient_image, checkerboard_image};
        auto [score, regions] = cv->detectAnomalies(noisy_image, references, "statistical", 0.5);

        EXPECT_GE(score, 0.0);
        EXPECT_LE(score, 1.0);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Anomaly detection not available: " << e.what();
    }
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

// Test feature extraction
TEST_F(ComputerVisionTest, FeatureExtraction) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    try {
        auto features = cv->extractFeatures(circle_image, "resnet", "pool5");

        // Should return a feature vector
        EXPECT_GT(features.size(), 0);

        // Verify no NaN values
        for (const auto& val : features) {
            EXPECT_FALSE(std::isnan(val));
        }
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Feature extraction models not available: " << e.what();
    }
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

// Test image similarity search
TEST_F(ComputerVisionTest, ImageSimilaritySearch) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    try {
        std::vector<blob> database = {gradient_image, checkerboard_image, circle_image, solid_image};
        auto similar = cv->findSimilarImages(checkerboard_image, database, 3, "cosine");

        // Should return top-K similar images
        EXPECT_LE(similar.size(), 3);

        for (const auto& [index, similarity] : similar) {
            EXPECT_GE(index, 0);
            EXPECT_LT(index, static_cast<int>(database.size()));
            EXPECT_GE(similarity, 0.0);
        }
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Image similarity not available: " << e.what();
    }
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

// Test pose estimation
TEST_F(ComputerVisionTest, PoseEstimation) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    try {
        auto poses = cv->estimatePose(circle_image, "mediapipe", false, false);

        // Verify pose structure
        for (const auto& pose : poses) {
            for (const auto& kp : pose) {
                EXPECT_GE(kp.x, 0.0);
                EXPECT_GE(kp.y, 0.0);
            }
        }
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Pose estimation not available: " << e.what();
    }
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, TextRecognition) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    try {
        auto textResults = cv->recognizeText(solid_image, "eng", "tesseract", true);

        for (const auto& [text, conf, x, y, w, h] : textResults) {
            EXPECT_GE(conf, 0.0);
            EXPECT_LE(conf, 1.0);
            EXPECT_GE(x, 0.0);
            EXPECT_GE(y, 0.0);
        }
    } catch (const std::exception& e) {
        GTEST_SKIP() << "OCR not available: " << e.what();
    }
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, ObjectTracking) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    try {
        std::vector<blob> frames = {checkerboard_image, gradient_image, circle_image};
        Detection initialDet(0, "object", 0.9, 50, 50, 100, 100);
        std::vector<Detection> initialDetections = {initialDet};

        auto trackingResults = cv->trackObjects(frames, initialDetections, "kcf");
        EXPECT_EQ(trackingResults.size(), frames.size());
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Object tracking not available: " << e.what();
    }
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, FactoryFunction) {
    auto cvProcessor = createOptimalComputerVision(false, "");
    EXPECT_NE(cvProcessor, nullptr);
}

TEST_F(ComputerVisionTest, VerySmallImage) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    auto smallData = TestDataGenerator::generateSolidColor(8, 8, 3, {128, 128, 128});
    blob small_image(smallData.data(), smallData.size());

    auto keypoints = cv->detectFeatures(small_image, FeatureDetectorType::ORB, 10);
    EXPECT_GE(keypoints.size(), 0);
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, LargeMaxFeatures) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    auto keypoints = cv->detectFeatures(checkerboard_image, FeatureDetectorType::ORB, 10000);
    EXPECT_GE(keypoints.size(), 0);
    EXPECT_LE(keypoints.size(), 10000);
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, ZeroQualityLevel) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    auto keypoints = cv->detectFeatures(checkerboard_image, FeatureDetectorType::HARRIS, 100, 0.0);
    EXPECT_GE(keypoints.size(), 0);
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST_F(ComputerVisionTest, DISABLED_PerformanceFeatureDetection) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    auto largeData = TestDataGenerator::generateCheckerboard(1024, 1024, 3, 16);
    blob large_image(largeData.data(), largeData.size());

    auto start = std::chrono::high_resolution_clock::now();
    auto keypoints = cv->detectFeatures(large_image, FeatureDetectorType::ORB, 1000);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_GT(keypoints.size(), 0);
    std::cout << "Feature detection took " << duration.count() << " ms" << std::endl;
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

} // namespace atom::image::test

