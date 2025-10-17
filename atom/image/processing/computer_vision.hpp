#ifndef ATOM_IMAGE_COMPUTER_VISION_HPP
#define ATOM_IMAGE_COMPUTER_VISION_HPP

/**
 * @file computer_vision.hpp
 * @brief Advanced computer vision operations
 *
 * This module provides comprehensive computer vision capabilities including
 * feature detection, object recognition, image analysis, and machine learning
 * based image processing operations.
 *
 * @author Atom Framework Team
 * @date 2025
 * @version 1.0.0
 */

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "../core/image_blob.hpp"

namespace atom::image {

/**
 * @brief Feature detector types
 */
enum class FeatureDetectorType {
    SIFT,    // Scale-Invariant Feature Transform
    SURF,    // Speeded-Up Robust Features
    ORB,     // Oriented FAST and Rotated BRIEF
    AKAZE,   // Accelerated-KAZE
    BRISK,   // Binary Robust Invariant Scalable Keypoints
    FAST,    // Features from Accelerated Segment Test
    HARRIS,  // Harris corner detector
    GFTT,    // Good Features to Track
    MSER,    // Maximally Stable Extremal Regions
    BLOB     // Blob detector
};

/**
 * @brief Object detection models
 */
enum class ObjectDetectionModel {
    YOLO_V5,       // YOLOv5 object detection
    YOLO_V8,       // YOLOv8 object detection
    SSD,           // Single Shot MultiBox Detector
    FASTER_RCNN,   // Faster R-CNN
    MOBILENET,     // MobileNet-SSD
    EFFICIENTDET,  // EfficientDet
    DETECTRON2,    // Detectron2 models
    CUSTOM         // Custom trained model
};

/**
 * @brief Face detection/recognition models
 */
enum class FaceModel {
    HAAR_CASCADE,  // Haar cascade classifier
    DNN_FACE,      // DNN-based face detection
    MTCNN,         // Multi-task CNN
    RETINAFACE,    // RetinaFace
    FACENET,       // FaceNet for recognition
    ARCFACE,       // ArcFace for recognition
    DLIB_68,       // Dlib 68-point landmark detector
    MEDIAPIPE      // MediaPipe face detection
};

/**
 * @brief Image segmentation methods
 */
enum class SegmentationMethod {
    WATERSHED,     // Watershed segmentation
    GRABCUT,       // GrabCut algorithm
    MEAN_SHIFT,    // Mean shift segmentation
    FELZENSZWALB,  // Felzenszwalb's algorithm
    SLIC,          // Simple Linear Iterative Clustering
    QUICKSHIFT,    // Quick shift segmentation
    SEMANTIC_SEG,  // Semantic segmentation (DNN)
    INSTANCE_SEG,  // Instance segmentation
    PANOPTIC_SEG   // Panoptic segmentation
};

/**
 * @brief Keypoint structure
 */
struct Keypoint {
    double x, y;                    // Coordinates
    double size;                    // Feature size
    double angle;                   // Orientation
    double response;                // Detector response
    int octave;                     // Octave level
    int classId;                    // Class ID
    std::vector<float> descriptor;  // Feature descriptor

    Keypoint(double x = 0, double y = 0, double size = 1, double angle = -1,
             double response = 0, int octave = 0, int classId = -1)
        : x(x),
          y(y),
          size(size),
          angle(angle),
          response(response),
          octave(octave),
          classId(classId) {}
};

/**
 * @brief Detection result structure
 */
struct Detection {
    int classId;                      // Object class ID
    std::string className;            // Object class name
    double confidence;                // Detection confidence
    double x, y, width, height;       // Bounding box
    std::vector<double> mask;         // Segmentation mask (if available)
    std::vector<Keypoint> keypoints;  // Object keypoints (if available)

    Detection(int id = -1, const std::string& name = "", double conf = 0.0,
              double x = 0, double y = 0, double w = 0, double h = 0)
        : classId(id),
          className(name),
          confidence(conf),
          x(x),
          y(y),
          width(w),
          height(h) {}
};

/**
 * @brief Face detection result
 */
struct FaceDetection {
    double x, y, width, height;       // Face bounding box
    double confidence;                // Detection confidence
    std::vector<Keypoint> landmarks;  // Facial landmarks
    std::vector<float> embedding;     // Face embedding for recognition
    double age;                       // Estimated age
    std::string gender;               // Estimated gender
    std::string emotion;              // Dominant emotion

    FaceDetection(double x = 0, double y = 0, double w = 0, double h = 0,
                  double conf = 0.0)
        : x(x), y(y), width(w), height(h), confidence(conf), age(-1) {}
};

/**
 * @brief Computer vision processor
 */
class ComputerVision {
public:
    ComputerVision() = default;
    virtual ~ComputerVision() = default;

    /**
     * @brief Detect and extract features from image
     * @param input Input image blob
     * @param detectorType Feature detector type
     * @param maxFeatures Maximum number of features to detect
     * @param qualityLevel Quality threshold for feature detection
     * @return Vector of detected keypoints
     */
    virtual std::vector<Keypoint> detectFeatures(
        const blob& input,
        FeatureDetectorType detectorType = FeatureDetectorType::ORB,
        int maxFeatures = 1000, double qualityLevel = 0.01) const;

    /**
     * @brief Match features between two images
     * @param keypoints1 Keypoints from first image
     * @param keypoints2 Keypoints from second image
     * @param matchingMethod Matching method ("brute_force", "flann",
     * "ratio_test")
     * @param distanceThreshold Distance threshold for matches
     * @return Vector of matched keypoint pairs (indices)
     */
    virtual std::vector<std::pair<int, int>> matchFeatures(
        const std::vector<Keypoint>& keypoints1,
        const std::vector<Keypoint>& keypoints2,
        const std::string& matchingMethod = "ratio_test",
        double distanceThreshold = 0.7) const;

    /**
     * @brief Detect objects in image
     * @param input Input image blob
     * @param model Object detection model
     * @param confidenceThreshold Minimum confidence threshold
     * @param nmsThreshold Non-maximum suppression threshold
     * @param modelPath Path to model file (if custom)
     * @return Vector of detected objects
     */
    virtual std::vector<Detection> detectObjects(
        const blob& input,
        ObjectDetectionModel model = ObjectDetectionModel::YOLO_V5,
        double confidenceThreshold = 0.5, double nmsThreshold = 0.4,
        const std::string& modelPath = "") const;

    /**
     * @brief Detect faces in image
     * @param input Input image blob
     * @param model Face detection model
     * @param minFaceSize Minimum face size
     * @param scaleFactor Scale factor for detection
     * @param detectLandmarks Whether to detect facial landmarks
     * @param recognizeFaces Whether to generate face embeddings
     * @return Vector of detected faces
     */
    virtual std::vector<FaceDetection> detectFaces(
        const blob& input, FaceModel model = FaceModel::DNN_FACE,
        int minFaceSize = 30, double scaleFactor = 1.1,
        bool detectLandmarks = true, bool recognizeFaces = false) const;

    /**
     * @brief Segment image into regions
     * @param input Input image blob
     * @param method Segmentation method
     * @param numSegments Target number of segments (for some methods)
     * @param compactness Compactness parameter (for SLIC)
     * @return Segmentation mask (label for each pixel)
     */
    virtual std::vector<std::vector<int>> segmentImage(
        const blob& input, SegmentationMethod method = SegmentationMethod::SLIC,
        int numSegments = 100, double compactness = 10.0) const;

    /**
     * @brief Track objects across video frames
     * @param frames Vector of video frames
     * @param initialDetections Initial object detections
     * @param tracker Tracking algorithm ("kcf", "csrt", "mosse", "mil")
     * @return Tracking results for each frame
     */
    virtual std::vector<std::vector<Detection>> trackObjects(
        const std::vector<blob>& frames,
        const std::vector<Detection>& initialDetections,
        const std::string& tracker = "kcf") const;

    /**
     * @brief Estimate optical flow between two frames
     * @param frame1 First frame
     * @param frame2 Second frame
     * @param method Flow estimation method ("lucas_kanade", "farneback",
     * "tvl1")
     * @param features Feature points to track (if empty, detect automatically)
     * @return Flow vectors for each point
     */
    virtual std::vector<std::pair<double, double>> estimateOpticalFlow(
        const blob& frame1, const blob& frame2,
        const std::string& method = "lucas_kanade",
        const std::vector<Keypoint>& features = {}) const;

    /**
     * @brief Perform image classification
     * @param input Input image blob
     * @param model Classification model ("resnet", "mobilenet", "efficientnet",
     * "custom")
     * @param modelPath Path to model file (if custom)
     * @param topK Number of top predictions to return
     * @return Classification results (class, confidence pairs)
     */
    virtual std::vector<std::pair<std::string, double>> classifyImage(
        const blob& input, const std::string& model = "resnet",
        const std::string& modelPath = "", int topK = 5) const;

    /**
     * @brief Detect and recognize text in image (OCR)
     * @param input Input image blob
     * @param language OCR language ("eng", "chi_sim", etc.)
     * @param ocrEngine OCR engine ("tesseract", "easyocr", "paddleocr")
     * @param preprocessImage Whether to preprocess image for better OCR
     * @return Detected text with bounding boxes and confidence
     */
    virtual std::vector<
        std::tuple<std::string, double, double, double, double, double>>
    recognizeText(const blob& input, const std::string& language = "eng",
                  const std::string& ocrEngine = "tesseract",
                  bool preprocessImage = true) const;

    /**
     * @brief Estimate pose from image
     * @param input Input image blob
     * @param model Pose estimation model ("openpose", "mediapipe", "alphapose")
     * @param detectHands Whether to detect hand poses
     * @param detectFace Whether to detect face pose
     * @return Detected poses with keypoints
     */
    virtual std::vector<std::vector<Keypoint>> estimatePose(
        const blob& input, const std::string& model = "mediapipe",
        bool detectHands = false, bool detectFace = false) const;

    /**
     * @brief Analyze image quality
     * @param input Input image blob
     * @param metrics Quality metrics to compute
     * @return Quality analysis results
     */
    virtual std::unordered_map<std::string, double> analyzeQuality(
        const blob& input,
        const std::vector<std::string>& metrics = {
            "sharpness", "noise", "exposure", "contrast"}) const;

    /**
     * @brief Detect image anomalies
     * @param input Input image blob
     * @param referenceImages Reference images for comparison
     * @param method Anomaly detection method ("statistical", "autoencoder",
     * "one_class_svm")
     * @param threshold Anomaly threshold
     * @return Anomaly score and detected regions
     */
    virtual std::pair<double, std::vector<std::vector<int>>> detectAnomalies(
        const blob& input, const std::vector<blob>& referenceImages,
        const std::string& method = "statistical",
        double threshold = 0.5) const;

    /**
     * @brief Generate image embeddings/features
     * @param input Input image blob
     * @param model Feature extraction model ("resnet", "vgg", "inception",
     * "clip")
     * @param layer Layer to extract features from
     * @return Feature vector
     */
    virtual std::vector<float> extractFeatures(
        const blob& input, const std::string& model = "resnet",
        const std::string& layer = "pool5") const;

    /**
     * @brief Find similar images using feature matching
     * @param query Query image
     * @param database Database of images to search
     * @param topK Number of similar images to return
     * @param metric Distance metric ("cosine", "euclidean", "manhattan")
     * @return Similar images with similarity scores
     */
    virtual std::vector<std::pair<int, double>> findSimilarImages(
        const blob& query, const std::vector<blob>& database, int topK = 10,
        const std::string& metric = "cosine") const;

protected:
    /**
     * @brief Initialize computer vision models
     * @param modelType Type of model to initialize
     * @param modelPath Path to model files
     * @return Success status
     */
    virtual bool initializeModel(const std::string& modelType,
                                 const std::string& modelPath = "") const;

    /**
     * @brief Preprocess image for computer vision operations
     * @param input Input image blob
     * @param targetSize Target image size
     * @param normalize Whether to normalize pixel values
     * @param meanSubtraction Mean values for subtraction
     * @return Preprocessed image data
     */
    virtual std::vector<float> preprocessImage(
        const blob& input, const std::pair<int, int>& targetSize = {224, 224},
        bool normalize = true,
        const std::vector<float>& meanSubtraction = {}) const;
};

/**
 * @brief Factory function to create optimal computer vision processor
 * @param useGPU Whether to use GPU acceleration if available
 * @param modelPath Path to model files directory
 * @return Unique pointer to computer vision processor
 */
std::unique_ptr<ComputerVision> createOptimalComputerVision(
    bool useGPU = false, const std::string& modelPath = "");

}  // namespace atom::image

#endif  // ATOM_IMAGE_COMPUTER_VISION_HPP
