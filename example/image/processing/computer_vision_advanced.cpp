/**
 * @file computer_vision_advanced.cpp
 * @brief Advanced computer vision operations demonstration
 *
 * This example demonstrates:
 * - Object detection and recognition
 * - Feature matching and tracking
 * - Face detection and recognition
 * - Template matching and pattern recognition
 * - Optical flow and motion tracking
 * - Advanced image analysis techniques
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <iomanip>
#include <algorithm>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/processing/image_processor.hpp"

// Computer vision features require OpenCV
#ifdef ATOM_IMAGE_HAS_OPENCV
#include "atom/image/processing/computer_vision.hpp"
#include "atom/image/processing/feature_detection.hpp"
#include "atom/image/processing/object_detection.hpp"
#include "atom/image/processing/face_detection.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/features2d.hpp>
#endif

using namespace atom::image;
using namespace std::chrono;

#ifdef ATOM_IMAGE_HAS_OPENCV

/**
 * @brief Create test images for computer vision demonstrations
 */
std::vector<cv::Mat> createTestImages() {
    std::vector<cv::Mat> images;
    
    // Image 1: Geometric shapes for object detection
    cv::Mat shapes(400, 600, CV_8UC3, cv::Scalar(50, 50, 50));
    
    // Add rectangles
    cv::rectangle(shapes, cv::Point(50, 50), cv::Point(150, 150), cv::Scalar(0, 255, 0), -1);
    cv::rectangle(shapes, cv::Point(200, 100), cv::Point(300, 200), cv::Scalar(255, 0, 0), -1);
    
    // Add circles
    cv::circle(shapes, cv::Point(450, 100), 50, cv::Scalar(0, 0, 255), -1);
    cv::circle(shapes, cv::Point(100, 300), 40, cv::Scalar(255, 255, 0), -1);
    
    // Add triangles (using polylines)
    std::vector<cv::Point> triangle1 = {cv::Point(350, 250), cv::Point(400, 350), cv::Point(300, 350)};
    cv::fillPoly(shapes, std::vector<std::vector<cv::Point>>{triangle1}, cv::Scalar(255, 0, 255));
    
    images.push_back(shapes);
    
    // Image 2: Textured pattern for feature detection
    cv::Mat texture(300, 400, CV_8UC3);
    cv::randu(texture, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255));
    
    // Add some structured features
    for (int i = 0; i < 10; ++i) {
        cv::Point center(50 + i * 30, 50 + i * 20);
        cv::circle(texture, center, 10, cv::Scalar(255, 255, 255), 2);
        cv::line(texture, cv::Point(center.x - 15, center.y), cv::Point(center.x + 15, center.y), cv::Scalar(0, 0, 0), 2);
        cv::line(texture, cv::Point(center.x, center.y - 15), cv::Point(center.x, center.y + 15), cv::Scalar(0, 0, 0), 2);
    }
    
    images.push_back(texture);
    
    // Image 3: Simple face-like pattern
    cv::Mat face(200, 200, CV_8UC3, cv::Scalar(220, 180, 150)); // Skin color
    
    // Face outline
    cv::ellipse(face, cv::Point(100, 100), cv::Size(80, 100), 0, 0, 360, cv::Scalar(200, 160, 130), -1);
    
    // Eyes
    cv::circle(face, cv::Point(75, 80), 8, cv::Scalar(0, 0, 0), -1);
    cv::circle(face, cv::Point(125, 80), 8, cv::Scalar(0, 0, 0), -1);
    
    // Nose
    cv::line(face, cv::Point(100, 90), cv::Point(100, 110), cv::Scalar(180, 140, 110), 2);
    
    // Mouth
    cv::ellipse(face, cv::Point(100, 130), cv::Size(20, 10), 0, 0, 180, cv::Scalar(150, 50, 50), 2);
    
    images.push_back(face);
    
    return images;
}

/**
 * @brief Demonstrate feature detection and matching
 */
void demonstrateFeatureDetection() {
    std::cout << "\n=== Feature Detection and Matching ===\n";
    
    try {
        auto testImages = createTestImages();
        cv::Mat image1 = testImages[1]; // Textured image
        
        // Create a slightly modified version for matching
        cv::Mat image2;
        cv::Mat transform = cv::getRotationMatrix2D(cv::Point2f(200, 150), 15, 0.9);
        cv::warpAffine(image1, image2, transform, image1.size());
        
        // Add some noise
        cv::Mat noise(image2.size(), image2.type());
        cv::randu(noise, cv::Scalar(-20, -20, -20), cv::Scalar(20, 20, 20));
        image2 += noise;
        
        std::cout << "Testing feature detection algorithms:\n";
        
        // Test different feature detectors
        std::vector<std::pair<std::string, cv::Ptr<cv::Feature2D>>> detectors = {
            {"ORB", cv::ORB::create(500)},
            {"SIFT", cv::SIFT::create()},
            {"FAST", cv::FastFeatureDetector::create()}
        };

        // Add SURF if available (requires opencv_contrib)
        try {
            auto surf = cv::xfeatures2d::SURF::create(400);
            if (surf) {
                detectors.push_back({"SURF", surf});
            }
        } catch (...) {
            // SURF not available, skip
        }
        
        for (const auto& [name, detector] : detectors) {
            if (!detector) {
                std::cout << "  " << name << ": Not available\n";
                continue;
            }
            
            std::cout << "  Testing " << name << " detector:\n";
            
            try {
                auto start = steady_clock::now();
                
                // Detect keypoints
                std::vector<cv::KeyPoint> keypoints1, keypoints2;
                detector->detect(image1, keypoints1);
                detector->detect(image2, keypoints2);
                
                auto detectionTime = duration_cast<microseconds>(steady_clock::now() - start);
                
                std::cout << "    Image 1 keypoints: " << keypoints1.size() << "\n";
                std::cout << "    Image 2 keypoints: " << keypoints2.size() << "\n";
                std::cout << "    Detection time: " << detectionTime.count() << " μs\n";
                
                // Compute descriptors
                start = steady_clock::now();
                
                cv::Mat descriptors1, descriptors2;
                detector->compute(image1, keypoints1, descriptors1);
                detector->compute(image2, keypoints2, descriptors2);
                
                auto descriptorTime = duration_cast<microseconds>(steady_clock::now() - start);
                
                std::cout << "    Descriptor computation time: " << descriptorTime.count() << " μs\n";
                
                if (!descriptors1.empty() && !descriptors2.empty()) {
                    // Match features
                    start = steady_clock::now();
                    
                    cv::BFMatcher matcher;
                    std::vector<cv::DMatch> matches;
                    matcher.match(descriptors1, descriptors2, matches);
                    
                    auto matchingTime = duration_cast<microseconds>(steady_clock::now() - start);
                    
                    // Filter good matches
                    std::sort(matches.begin(), matches.end());
                    const int numGoodMatches = std::min(50, static_cast<int>(matches.size() * 0.3));
                    std::vector<cv::DMatch> goodMatches(matches.begin(), matches.begin() + numGoodMatches);
                    
                    std::cout << "    Total matches: " << matches.size() << "\n";
                    std::cout << "    Good matches: " << goodMatches.size() << "\n";
                    std::cout << "    Matching time: " << matchingTime.count() << " μs\n";
                    
                    if (!goodMatches.empty()) {
                        // Calculate match quality
                        double avgDistance = 0.0;
                        for (const auto& match : goodMatches) {
                            avgDistance += match.distance;
                        }
                        avgDistance /= goodMatches.size();
                        
                        std::cout << "    Average match distance: " << std::fixed << std::setprecision(2) << avgDistance << "\n";
                    }
                }
                
            } catch (const std::exception& e) {
                std::cout << "    Error: " << e.what() << "\n";
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in feature detection: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate object detection
 */
void demonstrateObjectDetection() {
    std::cout << "\n=== Object Detection ===\n";
    
    try {
        auto testImages = createTestImages();
        cv::Mat image = testImages[0]; // Shapes image
        
        std::cout << "Testing shape detection algorithms:\n";
        
        // Convert to grayscale for processing
        cv::Mat gray;
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        
        // Test 1: Circle detection using HoughCircles
        std::cout << "  Circle Detection (Hough Transform):\n";
        
        auto start = steady_clock::now();
        
        std::vector<cv::Vec3f> circles;
        cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 1, 50, 100, 30, 10, 100);
        
        auto circleTime = duration_cast<microseconds>(steady_clock::now() - start);
        
        std::cout << "    Detected circles: " << circles.size() << "\n";
        std::cout << "    Detection time: " << circleTime.count() << " μs\n";
        
        for (size_t i = 0; i < circles.size(); ++i) {
            cv::Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
            int radius = cvRound(circles[i][2]);
            std::cout << "    Circle " << i << ": center(" << center.x << "," << center.y << "), radius=" << radius << "\n";
        }
        
        // Test 2: Rectangle detection using contours
        std::cout << "  Rectangle Detection (Contours):\n";
        
        start = steady_clock::now();
        
        cv::Mat edges;
        cv::Canny(gray, edges, 50, 150);
        
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        int rectangleCount = 0;
        for (const auto& contour : contours) {
            std::vector<cv::Point> approx;
            cv::approxPolyDP(contour, approx, 0.02 * cv::arcLength(contour, true), true);
            
            if (approx.size() == 4 && cv::contourArea(contour) > 1000) {
                rectangleCount++;
                cv::Rect boundingRect = cv::boundingRect(approx);
                std::cout << "    Rectangle " << rectangleCount << ": " << boundingRect << "\n";
            }
        }
        
        auto rectTime = duration_cast<microseconds>(steady_clock::now() - start);
        
        std::cout << "    Detected rectangles: " << rectangleCount << "\n";
        std::cout << "    Detection time: " << rectTime.count() << " μs\n";
        
        // Test 3: Template matching
        std::cout << "  Template Matching:\n";
        
        // Create a small template from part of the image
        cv::Rect templateRect(50, 50, 50, 50); // Part of the green rectangle
        cv::Mat templateImg = image(templateRect);
        
        start = steady_clock::now();
        
        cv::Mat result;
        cv::matchTemplate(image, templateImg, result, cv::TM_CCOEFF_NORMED);
        
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
        
        auto templateTime = duration_cast<microseconds>(steady_clock::now() - start);
        
        std::cout << "    Best match location: (" << maxLoc.x << "," << maxLoc.y << ")\n";
        std::cout << "    Match confidence: " << std::fixed << std::setprecision(3) << maxVal << "\n";
        std::cout << "    Template matching time: " << templateTime.count() << " μs\n";
        
        // Test 4: Blob detection
        std::cout << "  Blob Detection:\n";
        
        start = steady_clock::now();
        
        cv::SimpleBlobDetector::Params params;
        params.filterByArea = true;
        params.minArea = 100;
        params.maxArea = 10000;
        params.filterByCircularity = true;
        params.minCircularity = 0.1;
        
        cv::Ptr<cv::SimpleBlobDetector> detector = cv::SimpleBlobDetector::create(params);
        std::vector<cv::KeyPoint> keypoints;
        detector->detect(gray, keypoints);
        
        auto blobTime = duration_cast<microseconds>(steady_clock::now() - start);
        
        std::cout << "    Detected blobs: " << keypoints.size() << "\n";
        std::cout << "    Blob detection time: " << blobTime.count() << " μs\n";
        
        for (size_t i = 0; i < keypoints.size(); ++i) {
            const auto& kp = keypoints[i];
            std::cout << "    Blob " << i << ": center(" << std::fixed << std::setprecision(1) 
                     << kp.pt.x << "," << kp.pt.y << "), size=" << kp.size << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in object detection: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate face detection
 */
void demonstrateFaceDetection() {
    std::cout << "\n=== Face Detection ===\n";
    
    try {
        auto testImages = createTestImages();
        cv::Mat image = testImages[2]; // Face-like image
        
        std::cout << "Testing face detection algorithms:\n";
        
        // Test 1: Haar Cascade face detection
        std::cout << "  Haar Cascade Face Detection:\n";
        
        try {
            cv::CascadeClassifier faceCascade;
            
            // Try to load face cascade (common paths)
            std::vector<std::string> cascadePaths = {
                "haarcascade_frontalface_alt.xml",
                "/usr/share/opencv4/haarcascades/haarcascade_frontalface_alt.xml",
                "/usr/local/share/opencv4/haarcascades/haarcascade_frontalface_alt.xml"
            };
            
            bool cascadeLoaded = false;
            for (const auto& path : cascadePaths) {
                if (faceCascade.load(path)) {
                    cascadeLoaded = true;
                    std::cout << "    Loaded cascade from: " << path << "\n";
                    break;
                }
            }
            
            if (cascadeLoaded) {
                cv::Mat gray;
                cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
                
                auto start = steady_clock::now();
                
                std::vector<cv::Rect> faces;
                faceCascade.detectMultiScale(gray, faces, 1.1, 3, 0, cv::Size(30, 30));
                
                auto detectionTime = duration_cast<microseconds>(steady_clock::now() - start);
                
                std::cout << "    Detected faces: " << faces.size() << "\n";
                std::cout << "    Detection time: " << detectionTime.count() << " μs\n";
                
                for (size_t i = 0; i < faces.size(); ++i) {
                    const auto& face = faces[i];
                    std::cout << "    Face " << i << ": " << face << "\n";
                }
            } else {
                std::cout << "    Haar cascade file not found (this is expected in test environment)\n";
            }
            
        } catch (const std::exception& e) {
            std::cout << "    Haar cascade error: " << e.what() << "\n";
        }
        
        // Test 2: Simple face-like pattern detection using template matching
        std::cout << "  Simple Pattern-Based Face Detection:\n";
        
        // Create a simple face template
        cv::Mat faceTemplate(60, 60, CV_8UC1, cv::Scalar(200));
        
        // Add eye patterns
        cv::circle(faceTemplate, cv::Point(18, 20), 3, cv::Scalar(0), -1);
        cv::circle(faceTemplate, cv::Point(42, 20), 3, cv::Scalar(0), -1);
        
        // Add mouth pattern
        cv::ellipse(faceTemplate, cv::Point(30, 45), cv::Size(8, 4), 0, 0, 180, cv::Scalar(100), 1);
        
        cv::Mat grayImage;
        cv::cvtColor(image, grayImage, cv::COLOR_BGR2GRAY);
        
        auto start = steady_clock::now();
        
        cv::Mat result;
        cv::matchTemplate(grayImage, faceTemplate, result, cv::TM_CCOEFF_NORMED);
        
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
        
        auto templateTime = duration_cast<microseconds>(steady_clock::now() - start);
        
        std::cout << "    Best match location: (" << maxLoc.x << "," << maxLoc.y << ")\n";
        std::cout << "    Match confidence: " << std::fixed << std::setprecision(3) << maxVal << "\n";
        std::cout << "    Template matching time: " << templateTime.count() << " μs\n";
        
        // Test 3: Eye detection using Haar cascades
        std::cout << "  Eye Detection:\n";
        
        try {
            cv::CascadeClassifier eyeCascade;
            
            std::vector<std::string> eyeCascadePaths = {
                "haarcascade_eye.xml",
                "/usr/share/opencv4/haarcascades/haarcascade_eye.xml",
                "/usr/local/share/opencv4/haarcascades/haarcascade_eye.xml"
            };
            
            bool eyeCascadeLoaded = false;
            for (const auto& path : eyeCascadePaths) {
                if (eyeCascade.load(path)) {
                    eyeCascadeLoaded = true;
                    break;
                }
            }
            
            if (eyeCascadeLoaded) {
                start = steady_clock::now();
                
                std::vector<cv::Rect> eyes;
                eyeCascade.detectMultiScale(grayImage, eyes, 1.1, 3, 0, cv::Size(10, 10));
                
                auto eyeTime = duration_cast<microseconds>(steady_clock::now() - start);
                
                std::cout << "    Detected eyes: " << eyes.size() << "\n";
                std::cout << "    Detection time: " << eyeTime.count() << " μs\n";
                
                for (size_t i = 0; i < eyes.size(); ++i) {
                    const auto& eye = eyes[i];
                    std::cout << "    Eye " << i << ": " << eye << "\n";
                }
            } else {
                std::cout << "    Eye cascade file not found (this is expected in test environment)\n";
            }
            
        } catch (const std::exception& e) {
            std::cout << "    Eye detection error: " << e.what() << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in face detection: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate optical flow and motion tracking
 */
void demonstrateOpticalFlow() {
    std::cout << "\n=== Optical Flow and Motion Tracking ===\n";
    
    try {
        // Create two frames with simulated motion
        cv::Mat frame1(300, 400, CV_8UC3, cv::Scalar(50, 50, 50));
        cv::Mat frame2(300, 400, CV_8UC3, cv::Scalar(50, 50, 50));
        
        // Add moving objects
        cv::circle(frame1, cv::Point(100, 100), 20, cv::Scalar(255, 255, 255), -1);
        cv::circle(frame2, cv::Point(120, 110), 20, cv::Scalar(255, 255, 255), -1); // Moved
        
        cv::rectangle(frame1, cv::Point(200, 150), cv::Point(250, 200), cv::Scalar(0, 255, 0), -1);
        cv::rectangle(frame2, cv::Point(210, 140), cv::Point(260, 190), cv::Scalar(0, 255, 0), -1); // Moved
        
        std::cout << "Testing optical flow algorithms:\n";
        
        // Convert to grayscale
        cv::Mat gray1, gray2;
        cv::cvtColor(frame1, gray1, cv::COLOR_BGR2GRAY);
        cv::cvtColor(frame2, gray2, cv::COLOR_BGR2GRAY);
        
        // Test 1: Lucas-Kanade optical flow
        std::cout << "  Lucas-Kanade Optical Flow:\n";
        
        // Detect features to track
        std::vector<cv::Point2f> points1;
        cv::goodFeaturesToTrack(gray1, points1, 100, 0.01, 10);
        
        std::cout << "    Features to track: " << points1.size() << "\n";
        
        if (!points1.empty()) {
            auto start = steady_clock::now();
            
            std::vector<cv::Point2f> points2;
            std::vector<uchar> status;
            std::vector<float> errors;
            
            cv::calcOpticalFlowPyrLK(gray1, gray2, points1, points2, status, errors);
            
            auto flowTime = duration_cast<microseconds>(steady_clock::now() - start);
            
            // Count successful tracks
            int trackedPoints = 0;
            double totalMotion = 0.0;
            
            for (size_t i = 0; i < status.size(); ++i) {
                if (status[i]) {
                    trackedPoints++;
                    double dx = points2[i].x - points1[i].x;
                    double dy = points2[i].y - points1[i].y;
                    totalMotion += sqrt(dx*dx + dy*dy);
                }
            }
            
            std::cout << "    Successfully tracked: " << trackedPoints << " points\n";
            std::cout << "    Average motion: " << std::fixed << std::setprecision(2) 
                     << (trackedPoints > 0 ? totalMotion / trackedPoints : 0.0) << " pixels\n";
            std::cout << "    Tracking time: " << flowTime.count() << " μs\n";
        }
        
        // Test 2: Dense optical flow (Farneback)
        std::cout << "  Dense Optical Flow (Farneback):\n";
        
        auto start = steady_clock::now();
        
        cv::Mat flow;
        cv::calcOpticalFlowFarneback(gray1, gray2, flow, 0.5, 3, 15, 3, 5, 1.2, 0);
        
        auto denseFlowTime = duration_cast<milliseconds>(steady_clock::now() - start);
        
        // Calculate flow statistics
        std::vector<cv::Mat> flowComponents;
        cv::split(flow, flowComponents);
        
        cv::Scalar meanFlow = cv::mean(flow);
        double avgMagnitude = sqrt(meanFlow[0]*meanFlow[0] + meanFlow[1]*meanFlow[1]);
        
        // Find maximum flow
        cv::Mat magnitude, angle;
        cv::cartToPolar(flowComponents[0], flowComponents[1], magnitude, angle, true);
        
        double minMag, maxMag;
        cv::minMaxLoc(magnitude, &minMag, &maxMag);
        
        std::cout << "    Flow field size: " << flow.size() << "\n";
        std::cout << "    Average flow magnitude: " << std::fixed << std::setprecision(2) << avgMagnitude << " pixels\n";
        std::cout << "    Maximum flow magnitude: " << maxMag << " pixels\n";
        std::cout << "    Dense flow time: " << denseFlowTime.count() << " ms\n";
        
        // Test 3: Background subtraction for motion detection
        std::cout << "  Background Subtraction:\n";
        
        start = steady_clock::now();
        
        cv::Ptr<cv::BackgroundSubtractor> backSub = cv::createBackgroundSubtractorMOG2();
        
        cv::Mat fgMask1, fgMask2;
        backSub->apply(frame1, fgMask1);
        backSub->apply(frame2, fgMask2);
        
        auto bgSubTime = duration_cast<microseconds>(steady_clock::now() - start);
        
        // Count foreground pixels
        int fgPixels1 = cv::countNonZero(fgMask1);
        int fgPixels2 = cv::countNonZero(fgMask2);
        
        std::cout << "    Foreground pixels frame 1: " << fgPixels1 << "\n";
        std::cout << "    Foreground pixels frame 2: " << fgPixels2 << "\n";
        std::cout << "    Background subtraction time: " << bgSubTime.count() << " μs\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error in optical flow: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate advanced image analysis
 */
void demonstrateAdvancedAnalysis() {
    std::cout << "\n=== Advanced Image Analysis ===\n";
    
    try {
        auto testImages = createTestImages();
        cv::Mat image = testImages[0]; // Shapes image
        
        std::cout << "Testing advanced analysis techniques:\n";
        
        // Test 1: Image moments and shape analysis
        std::cout << "  Image Moments and Shape Analysis:\n";
        
        cv::Mat gray;
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        
        auto start = steady_clock::now();
        
        // Find contours
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(gray, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        std::cout << "    Found contours: " << contours.size() << "\n";
        
        for (size_t i = 0; i < std::min(size_t(5), contours.size()); ++i) {
            if (cv::contourArea(contours[i]) > 100) {
                cv::Moments moments = cv::moments(contours[i]);
                
                // Calculate centroid
                double cx = moments.m10 / moments.m00;
                double cy = moments.m01 / moments.m00;
                
                // Calculate shape properties
                double area = cv::contourArea(contours[i]);
                double perimeter = cv::arcLength(contours[i], true);
                double circularity = 4 * CV_PI * area / (perimeter * perimeter);
                
                std::cout << "    Contour " << i << ":\n";
                std::cout << "      Centroid: (" << std::fixed << std::setprecision(1) << cx << "," << cy << ")\n";
                std::cout << "      Area: " << area << "\n";
                std::cout << "      Perimeter: " << perimeter << "\n";
                std::cout << "      Circularity: " << std::setprecision(3) << circularity << "\n";
            }
        }
        
        auto momentsTime = duration_cast<microseconds>(steady_clock::now() - start);
        std::cout << "    Analysis time: " << momentsTime.count() << " μs\n";
        
        // Test 2: Texture analysis using Local Binary Patterns
        std::cout << "  Texture Analysis (LBP simulation):\n";
        
        start = steady_clock::now();
        
        cv::Mat lbp(gray.size(), CV_8UC1, cv::Scalar(0));
        
        // Simple LBP implementation
        for (int y = 1; y < gray.rows - 1; ++y) {
            for (int x = 1; x < gray.cols - 1; ++x) {
                uint8_t center = gray.at<uint8_t>(y, x);
                uint8_t code = 0;
                
                // Check 8 neighbors
                if (gray.at<uint8_t>(y-1, x-1) >= center) code |= 1;
                if (gray.at<uint8_t>(y-1, x) >= center) code |= 2;
                if (gray.at<uint8_t>(y-1, x+1) >= center) code |= 4;
                if (gray.at<uint8_t>(y, x+1) >= center) code |= 8;
                if (gray.at<uint8_t>(y+1, x+1) >= center) code |= 16;
                if (gray.at<uint8_t>(y+1, x) >= center) code |= 32;
                if (gray.at<uint8_t>(y+1, x-1) >= center) code |= 64;
                if (gray.at<uint8_t>(y, x-1) >= center) code |= 128;
                
                lbp.at<uint8_t>(y, x) = code;
            }
        }
        
        auto lbpTime = duration_cast<milliseconds>(steady_clock::now() - start);
        
        // Calculate LBP histogram
        cv::Mat hist;
        int histSize = 256;
        float range[] = {0, 256};
        const float* histRange = {range};
        cv::calcHist(&lbp, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);
        
        // Find dominant patterns
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(hist, &minVal, &maxVal, &minLoc, &maxLoc);
        
        std::cout << "    LBP computation time: " << lbpTime.count() << " ms\n";
        std::cout << "    Most frequent pattern: " << maxLoc.y << " (count: " << maxVal << ")\n";
        std::cout << "    Texture uniformity: " << std::setprecision(3) << (maxVal / cv::sum(hist)[0]) << "\n";
        
        // Test 3: Color analysis
        std::cout << "  Color Analysis:\n";
        
        start = steady_clock::now();
        
        // Convert to different color spaces
        cv::Mat hsv, lab;
        cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
        cv::cvtColor(image, lab, cv::COLOR_BGR2Lab);
        
        // Calculate color histograms
        std::vector<cv::Mat> bgr_planes, hsv_planes;
        cv::split(image, bgr_planes);
        cv::split(hsv, hsv_planes);
        
        cv::Mat hist_b, hist_g, hist_r, hist_h;
        cv::calcHist(&bgr_planes[0], 1, 0, cv::Mat(), hist_b, 1, &histSize, &histRange);
        cv::calcHist(&bgr_planes[1], 1, 0, cv::Mat(), hist_g, 1, &histSize, &histRange);
        cv::calcHist(&bgr_planes[2], 1, 0, cv::Mat(), hist_r, 1, &histSize, &histRange);
        cv::calcHist(&hsv_planes[0], 1, 0, cv::Mat(), hist_h, 1, &histSize, &histRange);
        
        auto colorTime = duration_cast<microseconds>(steady_clock::now() - start);
        
        // Calculate color statistics
        cv::Scalar meanBGR = cv::mean(image);
        cv::Scalar meanHSV = cv::mean(hsv);
        
        std::cout << "    Color analysis time: " << colorTime.count() << " μs\n";
        std::cout << "    Mean BGR: (" << std::fixed << std::setprecision(1) 
                 << meanBGR[0] << "," << meanBGR[1] << "," << meanBGR[2] << ")\n";
        std::cout << "    Mean HSV: (" << meanHSV[0] << "," << meanHSV[1] << "," << meanHSV[2] << ")\n";
        
        // Dominant colors
        cv::minMaxLoc(hist_b, &minVal, &maxVal, &minLoc, &maxLoc);
        std::cout << "    Dominant blue value: " << maxLoc.y << "\n";
        cv::minMaxLoc(hist_g, &minVal, &maxVal, &minLoc, &maxLoc);
        std::cout << "    Dominant green value: " << maxLoc.y << "\n";
        cv::minMaxLoc(hist_r, &minVal, &maxVal, &minLoc, &maxLoc);
        std::cout << "    Dominant red value: " << maxLoc.y << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error in advanced analysis: " << e.what() << "\n";
    }
}

#endif // ATOM_IMAGE_HAS_OPENCV

int main() {
    std::cout << "=== Atom Image Advanced Computer Vision Demo ===\n";
    std::cout << "This example demonstrates advanced computer vision operations\n";

#ifdef ATOM_IMAGE_HAS_OPENCV
    // Run all demonstrations
    demonstrateFeatureDetection();
    demonstrateObjectDetection();
    demonstrateFaceDetection();
    demonstrateOpticalFlow();
    demonstrateAdvancedAnalysis();

    std::cout << "\n=== Advanced computer vision demo completed ===\n";
    std::cout << "\nKey capabilities demonstrated:\n";
    std::cout << "- Feature detection and matching (ORB, SIFT, SURF, FAST)\n";
    std::cout << "- Object detection (shapes, templates, blobs)\n";
    std::cout << "- Face detection (Haar cascades, pattern matching)\n";
    std::cout << "- Optical flow and motion tracking\n";
    std::cout << "- Advanced image analysis (moments, texture, color)\n";
    std::cout << "- Performance benchmarking for all algorithms\n";
#else
    std::cout << "\nNote: This example requires OpenCV support.\n";
    std::cout << "Please build with: cmake -DATOM_IMAGE_HAS_OPENCV=ON\n";
#endif

    return 0;
}
