#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <numeric>

// Define error macros to avoid atom error system namespace pollution
#define THROW_RUNTIME_ERROR(msg) throw std::runtime_error(msg)
#define THROW_INVALID_ARGUMENT(msg) throw std::invalid_argument(msg)

#include "computer_vision.hpp"

#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/dnn.hpp>
#endif

namespace atom::image {

std::vector<Keypoint> ComputerVision::detectFeatures(const blob& input,
                                                     FeatureDetectorType detectorType,
                                                     int maxFeatures,
                                                     double qualityLevel) const {
    if (input.isEmpty()) {
        return {};
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat gray;
    
    // Convert to grayscale if needed
    if (src.channels() > 1) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = src;
    }
    
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    
    switch (detectorType) {
        case FeatureDetectorType::ORB: {
            auto detector = cv::ORB::create(maxFeatures);
            detector->detectAndCompute(gray, cv::noArray(), keypoints, descriptors);
            break;
        }
        case FeatureDetectorType::SIFT: {
            auto detector = cv::SIFT::create(maxFeatures);
            detector->detectAndCompute(gray, cv::noArray(), keypoints, descriptors);
            break;
        }
        case FeatureDetectorType::SURF: {
            // Note: SURF is in opencv_contrib
            THROW_RUNTIME_ERROR("SURF detector requires OpenCV contrib modules");
            break;
        }
        case FeatureDetectorType::AKAZE: {
            auto detector = cv::AKAZE::create();
            detector->detectAndCompute(gray, cv::noArray(), keypoints, descriptors);
            break;
        }
        case FeatureDetectorType::BRISK: {
            auto detector = cv::BRISK::create();
            detector->detectAndCompute(gray, cv::noArray(), keypoints, descriptors);
            break;
        }
        case FeatureDetectorType::FAST: {
            auto detector = cv::FastFeatureDetector::create();
            detector->detect(gray, keypoints);
            break;
        }
        case FeatureDetectorType::HARRIS: {
            // Harris corner detection
            cv::Mat corners;
            cv::cornerHarris(gray, corners, 2, 3, 0.04);
            
            // Find local maxima
            cv::Mat dilated;
            cv::dilate(corners, dilated, cv::Mat());
            
            for (int y = 0; y < corners.rows; ++y) {
                for (int x = 0; x < corners.cols; ++x) {
                    if (corners.at<float>(y, x) > qualityLevel * 255 && 
                        corners.at<float>(y, x) == dilated.at<float>(y, x)) {
                        keypoints.emplace_back(cv::Point2f(x, y), 1.0f);
                        if (keypoints.size() >= static_cast<size_t>(maxFeatures)) {
                            break;
                        }
                    }
                }
                if (keypoints.size() >= static_cast<size_t>(maxFeatures)) {
                    break;
                }
            }
            break;
        }
        case FeatureDetectorType::GFTT: {
            std::vector<cv::Point2f> corners;
            cv::goodFeaturesToTrack(gray, corners, maxFeatures, qualityLevel, 10.0);
            
            for (const auto& corner : corners) {
                keypoints.emplace_back(corner, 1.0f);
            }
            break;
        }
        default:
            THROW_RUNTIME_ERROR("Unsupported feature detector type");
    }
    
    // Convert OpenCV keypoints to our format
    std::vector<Keypoint> result;
    result.reserve(keypoints.size());
    
    for (size_t i = 0; i < keypoints.size(); ++i) {
        const auto& kp = keypoints[i];
        Keypoint point;
        point.x = kp.pt.x;
        point.y = kp.pt.y;
        point.size = kp.size;
        point.angle = kp.angle;
        point.response = kp.response;
        point.octave = kp.octave;
        
        // Copy descriptor if available
        if (!descriptors.empty() && i < static_cast<size_t>(descriptors.rows)) {
            cv::Mat desc = descriptors.row(static_cast<int>(i));
            point.descriptor.resize(desc.cols);
            std::memcpy(point.descriptor.data(), desc.data, desc.cols * sizeof(float));
        }
        
        result.push_back(point);
    }
    
    return result;
#else
    THROW_RUNTIME_ERROR("OpenCV required for feature detection");
#endif
}

std::vector<FeatureMatch> ComputerVision::matchFeatures(const std::vector<Keypoint>& features1,
                                                        const std::vector<Keypoint>& features2,
                                                        double maxDistance,
                                                        double ratioThreshold) const {
    std::vector<FeatureMatch> matches;
    
    if (features1.empty() || features2.empty()) {
        return matches;
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    // Convert keypoints and descriptors to OpenCV format
    std::vector<cv::KeyPoint> kp1, kp2;
    cv::Mat desc1, desc2;
    
    // Check if descriptors are available
    if (features1[0].descriptor.empty() || features2[0].descriptor.empty()) {
        THROW_RUNTIME_ERROR("Feature descriptors required for matching");
    }
    
    int descSize = static_cast<int>(features1[0].descriptor.size());
    desc1.create(static_cast<int>(features1.size()), descSize, CV_32F);
    desc2.create(static_cast<int>(features2.size()), descSize, CV_32F);
    
    for (size_t i = 0; i < features1.size(); ++i) {
        kp1.emplace_back(cv::Point2f(features1[i].x, features1[i].y), features1[i].size);
        std::memcpy(desc1.row(static_cast<int>(i)).data, features1[i].descriptor.data(), 
                   descSize * sizeof(float));
    }
    
    for (size_t i = 0; i < features2.size(); ++i) {
        kp2.emplace_back(cv::Point2f(features2[i].x, features2[i].y), features2[i].size);
        std::memcpy(desc2.row(static_cast<int>(i)).data, features2[i].descriptor.data(), 
                   descSize * sizeof(float));
    }
    
    // Perform matching
    cv::BFMatcher matcher;
    std::vector<std::vector<cv::DMatch>> knnMatches;
    matcher.knnMatch(desc1, desc2, knnMatches, 2);
    
    // Apply ratio test and distance threshold
    for (const auto& match : knnMatches) {
        if (match.size() >= 2) {
            if (match[0].distance < ratioThreshold * match[1].distance &&
                match[0].distance < maxDistance) {
                
                FeatureMatch featureMatch;
                featureMatch.queryIdx = match[0].queryIdx;
                featureMatch.trainIdx = match[0].trainIdx;
                featureMatch.distance = match[0].distance;
                featureMatch.point1 = {features1[match[0].queryIdx].x, features1[match[0].queryIdx].y};
                featureMatch.point2 = {features2[match[0].trainIdx].x, features2[match[0].trainIdx].y};
                
                matches.push_back(featureMatch);
            }
        }
    }
    
    return matches;
#else
    THROW_RUNTIME_ERROR("OpenCV required for feature matching");
#endif
}

std::vector<Detection> ComputerVision::detectObjects(const blob& input,
                                                    ObjectDetectionModel model,
                                                    double confidenceThreshold,
                                                    double nmsThreshold,
                                                    const std::string& modelPath) const {
    if (input.isEmpty()) {
        return {};
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    std::vector<Detection> detections;
    
    switch (model) {
        case ObjectDetectionModel::YOLO_V5:
        case ObjectDetectionModel::YOLO_V8: {
            if (modelPath.empty()) {
                THROW_RUNTIME_ERROR("Model path required for YOLO detection");
            }
            
            // Load YOLO model
            cv::dnn::Net net = cv::dnn::readNet(modelPath);
            
            // Prepare input blob
            cv::Mat blob;
            cv::dnn::blobFromImage(src, blob, 1.0/255.0, cv::Size(640, 640), cv::Scalar(0,0,0), true, false);
            net.setInput(blob);
            
            // Run inference
            std::vector<cv::Mat> outputs;
            net.forward(outputs, net.getUnconnectedOutLayersNames());
            
            // Process outputs (simplified)
            for (const auto& output : outputs) {
                for (int i = 0; i < output.rows; ++i) {
                    const float* data = output.ptr<float>(i);
                    float confidence = data[4];
                    
                    if (confidence > confidenceThreshold) {
                        Detection detection;
                        detection.bbox.x = static_cast<int>((data[0] - data[2]/2) * src.cols);
                        detection.bbox.y = static_cast<int>((data[1] - data[3]/2) * src.rows);
                        detection.bbox.width = static_cast<int>(data[2] * src.cols);
                        detection.bbox.height = static_cast<int>(data[3] * src.rows);
                        detection.confidence = confidence;
                        detection.classId = static_cast<int>(std::max_element(data + 5, data + output.cols) - (data + 5));
                        detection.className = "object_" + std::to_string(detection.classId);
                        
                        detections.push_back(detection);
                    }
                }
            }
            
            // Apply Non-Maximum Suppression
            std::vector<cv::Rect> boxes;
            std::vector<float> confidences;
            std::vector<int> classIds;
            
            for (const auto& det : detections) {
                boxes.emplace_back(det.bbox.x, det.bbox.y, det.bbox.width, det.bbox.height);
                confidences.push_back(det.confidence);
                classIds.push_back(det.classId);
            }
            
            std::vector<int> indices;
            cv::dnn::NMSBoxes(boxes, confidences, confidenceThreshold, nmsThreshold, indices);
            
            std::vector<Detection> finalDetections;
            for (int idx : indices) {
                finalDetections.push_back(detections[idx]);
            }
            
            return finalDetections;
        }
        default:
            THROW_RUNTIME_ERROR("Unsupported object detection model");
    }
#else
    THROW_RUNTIME_ERROR("OpenCV required for object detection");
#endif
}

std::vector<Face> ComputerVision::detectFaces(const blob& input,
                                              double scaleFactor,
                                              int minNeighbors,
                                              const std::string& cascadePath) const {
    if (input.isEmpty()) {
        return {};
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat gray;
    
    // Convert to grayscale
    if (src.channels() > 1) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = src;
    }
    
    // Load face cascade
    cv::CascadeClassifier faceCascade;
    std::string cascadeFile = cascadePath.empty() ? 
        cv::samples::findFile("haarcascades/haarcascade_frontalface_alt.xml") : cascadePath;
    
    if (!faceCascade.load(cascadeFile)) {
        THROW_RUNTIME_ERROR("Could not load face cascade classifier");
    }
    
    // Detect faces
    std::vector<cv::Rect> faceRects;
    faceCascade.detectMultiScale(gray, faceRects, scaleFactor, minNeighbors);
    
    // Convert to our format
    std::vector<Face> faces;
    faces.reserve(faceRects.size());
    
    for (const auto& rect : faceRects) {
        Face face;
        face.bbox.x = rect.x;
        face.bbox.y = rect.y;
        face.bbox.width = rect.width;
        face.bbox.height = rect.height;
        face.confidence = 1.0f; // Haar cascades don't provide confidence scores
        
        faces.push_back(face);
    }
    
    return faces;
#else
    THROW_RUNTIME_ERROR("OpenCV required for face detection");
#endif
}

blob ComputerVision::trackObject(const blob& input,
                                const BoundingBox& initialBox,
                                const std::string& trackerType) const {
    if (input.isEmpty()) {
        return blob{};
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    
    // Create tracker based on type
    cv::Ptr<cv::Tracker> tracker;
    
    if (trackerType == "KCF") {
        tracker = cv::TrackerKCF::create();
    } else if (trackerType == "CSRT") {
        tracker = cv::TrackerCSRT::create();
    } else if (trackerType == "MIL") {
        tracker = cv::TrackerMIL::create();
    } else {
        THROW_RUNTIME_ERROR("Unsupported tracker type: " + trackerType);
    }
    
    // Initialize tracker
    cv::Rect2d bbox(initialBox.x, initialBox.y, initialBox.width, initialBox.height);
    tracker->init(src, bbox);
    
    // For single frame tracking, just return the input
    // In a real application, this would be called on subsequent frames
    return input;
#else
    THROW_RUNTIME_ERROR("OpenCV required for object tracking");
#endif
}

std::vector<std::vector<Keypoint>> ComputerVision::estimatePose(
    const blob& input,
    const std::string& model,
    bool detectHands,
    bool detectFace) const {
    
    if (input.isEmpty()) {
        return {};
    }
    
    // Pose estimation would require specialized models like OpenPose, MediaPipe, etc.
    // This is a placeholder implementation
    THROW_RUNTIME_ERROR("Pose estimation not implemented - requires specialized models");
}

blob ComputerVision::segmentImage(const blob& input,
                                 const std::string& model,
                                 int numClasses) const {
    if (input.isEmpty()) {
        return blob{};
    }
    
    // Image segmentation would require specialized models
    // This is a placeholder implementation
    THROW_RUNTIME_ERROR("Image segmentation not implemented - requires specialized models");
}

std::vector<std::string> ComputerVision::classifyImage(const blob& input,
                                                      const std::string& model,
                                                      int topK) const {
    if (input.isEmpty()) {
        return {};
    }
    
    // Image classification would require specialized models
    // This is a placeholder implementation
    THROW_RUNTIME_ERROR("Image classification not implemented - requires specialized models");
}

bool ComputerVision::initializeModel(const std::string& modelType, const std::string& modelPath) const {
    // Model initialization logic would go here
    return !modelPath.empty();
}

std::vector<float> ComputerVision::preprocessImage(const blob& input,
                                                  const std::pair<int, int>& targetSize,
                                                  bool normalize,
                                                  const std::vector<float>& meanSubtraction) const {
    if (input.isEmpty()) {
        return {};
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat processed;
    
    // Resize to target size
    cv::resize(src, processed, cv::Size(targetSize.first, targetSize.second));
    
    // Convert to float
    processed.convertTo(processed, CV_32F);
    
    // Normalize if requested
    if (normalize) {
        processed /= 255.0;
    }
    
    // Apply mean subtraction if provided
    if (!meanSubtraction.empty() && meanSubtraction.size() == 3) {
        std::vector<cv::Mat> channels;
        cv::split(processed, channels);
        
        for (size_t i = 0; i < channels.size() && i < meanSubtraction.size(); ++i) {
            channels[i] -= meanSubtraction[i];
        }
        
        cv::merge(channels, processed);
    }
    
    // Convert to vector
    std::vector<float> result;
    result.assign(processed.ptr<float>(), processed.ptr<float>() + processed.total() * processed.channels());
    
    return result;
#else
    THROW_RUNTIME_ERROR("OpenCV required for image preprocessing");
#endif
}

}  // namespace atom::image
