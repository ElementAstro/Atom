/*
 * computer_vision.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file computer_vision.cpp
 * @brief Computer vision algorithms example for Atom Image library
 *
 * This example demonstrates:
 * - Feature detection and matching
 * - Object detection
 * - Image segmentation
 * - Template matching
 * - Optical flow
 */

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>

#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>
#endif

using namespace std;

/**
 * @brief Simple point structure
 */
struct Point2D {
    float x, y;
    Point2D(float x = 0, float y = 0) : x(x), y(y) {}
};

/**
 * @brief Simple rectangle structure
 */
struct Rectangle {
    float x, y, width, height;
    Rectangle(float x = 0, float y = 0, float w = 0, float h = 0)
        : x(x), y(y), width(w), height(h) {}
};

/**
 * @brief Feature detector class
 */
class FeatureDetector {
public:
    /**
     * @brief Detect corners using Harris corner detection
     */
    static vector<Point2D> detectHarrisCorners(int width, int height) {
        cout << "Detecting Harris corners in " << width << "x" << height << " image..." << endl;

        vector<Point2D> corners;

#ifdef ATOM_IMAGE_HAS_OPENCV
        // Create a synthetic image for demonstration
        cv::Mat image = cv::Mat::zeros(height, width, CV_8UC1);

        // Add some synthetic features
        cv::rectangle(image, cv::Point(50, 50), cv::Point(150, 150), cv::Scalar(255), 2);
        cv::rectangle(image, cv::Point(200, 100), cv::Point(300, 200), cv::Scalar(255), 2);
        cv::circle(image, cv::Point(400, 150), 50, cv::Scalar(255), 2);

        // Detect Harris corners
        cv::Mat corners_mat;
        cv::cornerHarris(image, corners_mat, 2, 3, 0.04);

        // Find corner points
        cv::Mat corners_norm;
        cv::normalize(corners_mat, corners_norm, 0, 255, cv::NORM_MINMAX, CV_32FC1);

        for (int i = 0; i < corners_norm.rows; i++) {
            for (int j = 0; j < corners_norm.cols; j++) {
                if (corners_norm.at<float>(i, j) > 100) {
                    corners.emplace_back(j, i);
                }
            }
        }

        cout << "  Found " << corners.size() << " Harris corners" << endl;
#else
        // Simulate corner detection without OpenCV
        random_device rd;
        mt19937 gen(rd());
        uniform_real_distribution<float> x_dist(0, width);
        uniform_real_distribution<float> y_dist(0, height);

        int num_corners = 10 + gen() % 20;
        for (int i = 0; i < num_corners; ++i) {
            corners.emplace_back(x_dist(gen), y_dist(gen));
        }

        cout << "  Simulated " << corners.size() << " corner points" << endl;
#endif

        return corners;
    }

    /**
     * @brief Detect SIFT features
     */
    static vector<Point2D> detectSIFTFeatures(int width, int height) {
        cout << "Detecting SIFT features in " << width << "x" << height << " image..." << endl;

        vector<Point2D> features;

#ifdef ATOM_IMAGE_HAS_OPENCV
        // Create synthetic image
        cv::Mat image = cv::Mat::zeros(height, width, CV_8UC1);

        // Add synthetic patterns
        for (int i = 0; i < 5; ++i) {
            cv::Point center(100 + i * 80, 100 + i * 30);
            cv::circle(image, center, 20 + i * 5, cv::Scalar(255), -1);
        }

        // Detect SIFT features
        auto sift = cv::SIFT::create();
        vector<cv::KeyPoint> keypoints;
        cv::Mat descriptors;

        sift->detectAndCompute(image, cv::noArray(), keypoints, descriptors);

        for (const auto& kp : keypoints) {
            features.emplace_back(kp.pt.x, kp.pt.y);
        }

        cout << "  Found " << features.size() << " SIFT features" << endl;
#else
        // Simulate SIFT detection
        random_device rd;
        mt19937 gen(rd());
        uniform_real_distribution<float> x_dist(0, width);
        uniform_real_distribution<float> y_dist(0, height);

        int num_features = 20 + gen() % 30;
        for (int i = 0; i < num_features; ++i) {
            features.emplace_back(x_dist(gen), y_dist(gen));
        }

        cout << "  Simulated " << features.size() << " SIFT features" << endl;
#endif

        return features;
    }
};

/**
 * @brief Object detector class
 */
class ObjectDetector {
public:
    /**
     * @brief Detect objects using template matching
     */
    static vector<Rectangle> detectObjects(int width, int height, const string& object_type) {
        cout << "Detecting " << object_type << " objects in " << width << "x" << height << " image..." << endl;

        vector<Rectangle> detections;

#ifdef ATOM_IMAGE_HAS_OPENCV
        // Create synthetic scene
        cv::Mat scene = cv::Mat::zeros(height, width, CV_8UC3);

        // Add some objects to detect
        cv::rectangle(scene, cv::Point(50, 50), cv::Point(150, 150), cv::Scalar(0, 255, 0), -1);
        cv::rectangle(scene, cv::Point(200, 100), cv::Point(280, 180), cv::Scalar(0, 255, 0), -1);
        cv::circle(scene, cv::Point(400, 200), 40, cv::Scalar(0, 0, 255), -1);

        // Create template
        cv::Mat template_img = cv::Mat::zeros(60, 60, CV_8UC3);
        cv::rectangle(template_img, cv::Point(10, 10), cv::Point(50, 50), cv::Scalar(0, 255, 0), -1);

        // Template matching
        cv::Mat result;
        cv::matchTemplate(scene, template_img, result, cv::TM_CCOEFF_NORMED);

        // Find matches above threshold
        double threshold = 0.7;
        cv::Mat locations;
        cv::findNonZero(result > threshold, locations);

        for (int i = 0; i < locations.total(); ++i) {
            cv::Point match_loc = locations.at<cv::Point>(i);
            detections.emplace_back(match_loc.x, match_loc.y,
                                  template_img.cols, template_img.rows);
        }

        cout << "  Found " << detections.size() << " " << object_type << " objects" << endl;
#else
        // Simulate object detection
        random_device rd;
        mt19937 gen(rd());
        uniform_real_distribution<float> x_dist(0, width - 100);
        uniform_real_distribution<float> y_dist(0, height - 100);
        uniform_real_distribution<float> size_dist(50, 150);

        int num_objects = 2 + gen() % 5;
        for (int i = 0; i < num_objects; ++i) {
            float size = size_dist(gen);
            detections.emplace_back(x_dist(gen), y_dist(gen), size, size);
        }

        cout << "  Simulated " << detections.size() << " " << object_type << " objects" << endl;
#endif

        return detections;
    }

    /**
     * @brief Perform image segmentation
     */
    static void performSegmentation(int width, int height) {
        cout << "Performing image segmentation on " << width << "x" << height << " image..." << endl;

#ifdef ATOM_IMAGE_HAS_OPENCV
        // Create synthetic image
        cv::Mat image = cv::Mat::zeros(height, width, CV_8UC3);

        // Add regions with different colors
        cv::rectangle(image, cv::Point(0, 0), cv::Point(width/2, height/2), cv::Scalar(100, 100, 255), -1);
        cv::rectangle(image, cv::Point(width/2, 0), cv::Point(width, height/2), cv::Scalar(100, 255, 100), -1);
        cv::rectangle(image, cv::Point(0, height/2), cv::Point(width/2, height), cv::Scalar(255, 100, 100), -1);
        cv::rectangle(image, cv::Point(width/2, height/2), cv::Point(width, height), cv::Scalar(255, 255, 100), -1);

        // Add some noise
        cv::Mat noise;
        cv::randn(noise, cv::Scalar::all(0), cv::Scalar::all(20));
        image += noise;

        // Perform K-means clustering for segmentation
        cv::Mat data;
        image.convertTo(data, CV_32F);
        data = data.reshape(1, data.total());

        cv::Mat labels, centers;
        int k = 4;
        cv::kmeans(data, k, labels, cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 20, 1.0),
                   3, cv::KMEANS_PP_CENTERS, centers);

        cout << "  Segmented image into " << k << " regions using K-means clustering" << endl;

        // Count pixels in each segment
        vector<int> segment_counts(k, 0);
        for (int i = 0; i < labels.rows; ++i) {
            segment_counts[labels.at<int>(i)]++;
        }

        for (int i = 0; i < k; ++i) {
            cout << "    Segment " << i << ": " << segment_counts[i] << " pixels ("
                 << (100.0 * segment_counts[i] / labels.rows) << "%)" << endl;
        }
#else
        cout << "  Simulated segmentation into 4 regions" << endl;
        cout << "    Segment 0: 25% of pixels" << endl;
        cout << "    Segment 1: 25% of pixels" << endl;
        cout << "    Segment 2: 25% of pixels" << endl;
        cout << "    Segment 3: 25% of pixels" << endl;
#endif
    }
};

/**
 * @brief Optical flow calculator
 */
class OpticalFlowCalculator {
public:
    /**
     * @brief Calculate optical flow between two frames
     */
    static void calculateOpticalFlow(int width, int height) {
        cout << "Calculating optical flow for " << width << "x" << height << " frames..." << endl;

#ifdef ATOM_IMAGE_HAS_OPENCV
        // Create two synthetic frames
        cv::Mat frame1 = cv::Mat::zeros(height, width, CV_8UC1);
        cv::Mat frame2 = cv::Mat::zeros(height, width, CV_8UC1);

        // Add moving objects
        cv::circle(frame1, cv::Point(100, 100), 30, cv::Scalar(255), -1);
        cv::circle(frame2, cv::Point(120, 110), 30, cv::Scalar(255), -1);

        cv::rectangle(frame1, cv::Point(200, 150), cv::Point(250, 200), cv::Scalar(255), -1);
        cv::rectangle(frame2, cv::Point(210, 160), cv::Point(260, 210), cv::Scalar(255), -1);

        // Calculate optical flow using Lucas-Kanade method
        vector<cv::Point2f> corners1;
        cv::goodFeaturesToTrack(frame1, corners1, 100, 0.01, 10);

        vector<cv::Point2f> corners2;
        vector<uchar> status;
        vector<float> errors;

        cv::calcOpticalFlowPyrLK(frame1, frame2, corners1, corners2, status, errors);

        // Calculate flow statistics
        float total_flow = 0;
        int valid_flows = 0;

        for (size_t i = 0; i < corners1.size(); ++i) {
            if (status[i]) {
                float dx = corners2[i].x - corners1[i].x;
                float dy = corners2[i].y - corners1[i].y;
                float flow_magnitude = sqrt(dx*dx + dy*dy);
                total_flow += flow_magnitude;
                valid_flows++;
            }
        }

        if (valid_flows > 0) {
            cout << "  Tracked " << valid_flows << " feature points" << endl;
            cout << "  Average flow magnitude: " << (total_flow / valid_flows) << " pixels" << endl;
        }
#else
        cout << "  Simulated optical flow calculation" << endl;
        cout << "  Tracked 25 feature points" << endl;
        cout << "  Average flow magnitude: 15.3 pixels" << endl;
#endif
    }
};

/**
 * @brief Demonstrate computer vision algorithms
 */
void demonstrateComputerVision() {
    cout << "=== Computer Vision Algorithms Demo ===" << endl;

    int width = 640, height = 480;

    // 1. Feature detection
    cout << "\n1. Feature Detection:" << endl;
    auto harris_corners = FeatureDetector::detectHarrisCorners(width, height);
    auto sift_features = FeatureDetector::detectSIFTFeatures(width, height);

    // 2. Object detection
    cout << "\n2. Object Detection:" << endl;
    auto rectangles = ObjectDetector::detectObjects(width, height, "rectangular");

    // 3. Image segmentation
    cout << "\n3. Image Segmentation:" << endl;
    ObjectDetector::performSegmentation(width, height);

    // 4. Optical flow
    cout << "\n4. Optical Flow:" << endl;
    OpticalFlowCalculator::calculateOpticalFlow(width, height);

    // Summary
    cout << "\n=== Summary ===" << endl;
    cout << "Harris corners detected: " << harris_corners.size() << endl;
    cout << "SIFT features detected: " << sift_features.size() << endl;
    cout << "Objects detected: " << rectangles.size() << endl;
    cout << "Segmentation completed successfully" << endl;
    cout << "Optical flow calculated successfully" << endl;
}

/**
 * @brief Main function
 */
int main() {
    try {
        cout << "Computer Vision Algorithms Example" << endl;
        cout << "=================================" << endl;

        // Display available backends
        cout << "\nAvailable backends:" << endl;
#ifdef ATOM_IMAGE_HAS_OPENCV
        cout << "  - OpenCV: Available (full functionality)" << endl;
#else
        cout << "  - OpenCV: Not available (simulation mode)" << endl;
#endif

        demonstrateComputerVision();

        cout << "\nComputer vision demonstration completed!" << endl;
        return 0;

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}
