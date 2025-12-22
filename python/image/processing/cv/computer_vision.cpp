/**
 * @file computer_vision.cpp
 * @brief Python bindings for computer vision operations
 */

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/image/processing/computer_vision.hpp"

namespace py = pybind11;
using namespace atom::image;

void bind_computer_vision(py::module& m) {
    auto cv_module =
        m.def_submodule("cv", "Computer vision algorithms and operations");

    // FeatureDetectorType enum
    py::enum_<FeatureDetectorType>(cv_module, "FeatureDetectorType",
                                   "Feature detection algorithms")
        .value("SIFT", FeatureDetectorType::SIFT,
               "Scale-Invariant Feature Transform")
        .value("SURF", FeatureDetectorType::SURF, "Speeded-Up Robust Features")
        .value("ORB", FeatureDetectorType::ORB,
               "Oriented FAST and Rotated BRIEF")
        .value("AKAZE", FeatureDetectorType::AKAZE, "Accelerated KAZE")
        .value("BRISK", FeatureDetectorType::BRISK,
               "Binary Robust Invariant Scalable Keypoints")
        .value("FAST", FeatureDetectorType::FAST,
               "Features from Accelerated Segment Test")
        .value("HARRIS", FeatureDetectorType::HARRIS, "Harris corner detector")
        .value("GFTT", FeatureDetectorType::GFTT, "Good Features to Track")
        .value("MSER", FeatureDetectorType::MSER,
               "Maximally Stable Extremal Regions")
        .value("BLOB", FeatureDetectorType::BLOB, "Simple blob detector")
        .export_values();

    // ObjectDetectionModel enum
    py::enum_<ObjectDetectionModel>(cv_module, "ObjectDetectionModel",
                                    "Object detection models")
        .value("YOLO_V5", ObjectDetectionModel::YOLO_V5, "YOLOv5")
        .value("YOLO_V8", ObjectDetectionModel::YOLO_V8, "YOLOv8")
        .value("SSD", ObjectDetectionModel::SSD, "Single Shot Detector")
        .value("FASTER_RCNN", ObjectDetectionModel::FASTER_RCNN, "Faster R-CNN")
        .value("MOBILENET", ObjectDetectionModel::MOBILENET, "MobileNet SSD")
        .value("EFFICIENTDET", ObjectDetectionModel::EFFICIENTDET,
               "EfficientDet")
        .value("DETECTRON2", ObjectDetectionModel::DETECTRON2, "Detectron2")
        .value("CUSTOM", ObjectDetectionModel::CUSTOM, "Custom model")
        .export_values();

    // FaceModel enum
    py::enum_<FaceModel>(cv_module, "FaceModel",
                         "Face detection/recognition models")
        .value("HAAR_CASCADE", FaceModel::HAAR_CASCADE,
               "Haar cascade classifier")
        .value("DNN_FACE", FaceModel::DNN_FACE, "DNN face detector")
        .value("MTCNN", FaceModel::MTCNN, "Multi-task CNN")
        .value("RETINAFACE", FaceModel::RETINAFACE, "RetinaFace")
        .value("FACENET", FaceModel::FACENET, "FaceNet")
        .value("ARCFACE", FaceModel::ARCFACE, "ArcFace")
        .value("DLIB_68", FaceModel::DLIB_68, "Dlib 68 landmarks")
        .value("MEDIAPIPE", FaceModel::MEDIAPIPE, "MediaPipe face mesh")
        .export_values();

    // SegmentationMethod enum
    py::enum_<SegmentationMethod>(cv_module, "SegmentationMethod",
                                  "Image segmentation methods")
        .value("WATERSHED", SegmentationMethod::WATERSHED,
               "Watershed algorithm")
        .value("GRABCUT", SegmentationMethod::GRABCUT, "GrabCut algorithm")
        .value("MEAN_SHIFT", SegmentationMethod::MEAN_SHIFT, "Mean shift")
        .value("FELZENSZWALB", SegmentationMethod::FELZENSZWALB,
               "Felzenszwalb method")
        .value("SLIC", SegmentationMethod::SLIC, "SLIC superpixels")
        .value("QUICKSHIFT", SegmentationMethod::QUICKSHIFT, "Quickshift")
        .value("SEMANTIC_SEG", SegmentationMethod::SEMANTIC_SEG,
               "Semantic segmentation")
        .value("INSTANCE_SEG", SegmentationMethod::INSTANCE_SEG,
               "Instance segmentation")
        .value("PANOPTIC_SEG", SegmentationMethod::PANOPTIC_SEG,
               "Panoptic segmentation")
        .export_values();

    // Keypoint struct
    py::class_<Keypoint>(cv_module, "Keypoint", "Detected feature keypoint")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("x", &Keypoint::x, "X coordinate")
        .def_readwrite("y", &Keypoint::y, "Y coordinate")
        .def_readwrite("size", &Keypoint::size, "Feature size")
        .def_readwrite("angle", &Keypoint::angle, "Feature orientation")
        .def_readwrite("response", &Keypoint::response, "Detector response")
        .def_readwrite("octave", &Keypoint::octave, "Pyramid octave")
        .def_readwrite("classId", &Keypoint::classId, "Object class ID")
        .def("__repr__", [](const Keypoint& self) {
            return "<Keypoint x=" + std::to_string(self.x) +
                   " y=" + std::to_string(self.y) + ">";
        });

    // Detection struct
    py::class_<Detection>(cv_module, "Detection", "Object detection result")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("x", &Detection::x, "Bounding box X")
        .def_readwrite("y", &Detection::y, "Bounding box Y")
        .def_readwrite("width", &Detection::width, "Bounding box width")
        .def_readwrite("height", &Detection::height, "Bounding box height")
        .def_readwrite("confidence", &Detection::confidence,
                       "Detection confidence")
        .def_readwrite("classId", &Detection::classId, "Object class ID")
        .def_readwrite("className", &Detection::className, "Object class name")
        .def("__repr__", [](const Detection& self) {
            return "<Detection class=" + self.className +
                   " conf=" + std::to_string(self.confidence) + ">";
        });

    // FaceDetection struct
    py::class_<FaceDetection>(cv_module, "FaceDetection",
                              "Face detection result")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("x", &FaceDetection::x, "Face bounding box X")
        .def_readwrite("y", &FaceDetection::y, "Face bounding box Y")
        .def_readwrite("width", &FaceDetection::width,
                       "Face bounding box width")
        .def_readwrite("height", &FaceDetection::height,
                       "Face bounding box height")
        .def_readwrite("confidence", &FaceDetection::confidence,
                       "Detection confidence")
        .def_readwrite("landmarks", &FaceDetection::landmarks,
                       "Facial landmarks")
        .def_readwrite("embedding", &FaceDetection::embedding,
                       "Face embedding vector")
        .def("__repr__", [](const FaceDetection& self) {
            return "<FaceDetection conf=" + std::to_string(self.confidence) +
                   ">";
        });

    // FeatureMatch struct
    py::class_<FeatureMatch>(cv_module, "FeatureMatch", "Feature match result")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("queryIdx", &FeatureMatch::queryIdx,
                       "Query keypoint index")
        .def_readwrite("trainIdx", &FeatureMatch::trainIdx,
                       "Train keypoint index")
        .def_readwrite("distance", &FeatureMatch::distance, "Match distance")
        .def("__repr__", [](const FeatureMatch& self) {
            return "<FeatureMatch q=" + std::to_string(self.queryIdx) +
                   " t=" + std::to_string(self.trainIdx) + ">";
        });

    // ComputerVision class
    py::class_<ComputerVision>(cv_module, "ComputerVision",
                               R"pbdoc(
        Comprehensive computer vision processor.

        Provides feature detection, object detection, face detection,
        segmentation, tracking, and other computer vision algorithms.

        Example:
            >>> cv = ComputerVision()
            >>> keypoints = cv.detectFeatures(image, FeatureDetectorType.ORB)
            >>> faces = cv.detectFaces(image, FaceModel.DNN_FACE)
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def("detectFeatures", &ComputerVision::detectFeatures,
             py::arg("input"),
             py::arg("detectorType") = FeatureDetectorType::ORB,
             py::arg("maxFeatures") = 500,
             R"pbdoc(
            Detect features in image.

            Args:
                input: Input image
                detectorType: Feature detector type
                maxFeatures: Maximum number of features

            Returns:
                Tuple of (keypoints, descriptors)
            )pbdoc")
        .def("matchFeatures", &ComputerVision::matchFeatures,
             py::arg("descriptors1"), py::arg("descriptors2"),
             py::arg("method") = "BF", py::arg("crossCheck") = true,
             R"pbdoc(
            Match feature descriptors.

            Args:
                descriptors1: Descriptors from first image
                descriptors2: Descriptors from second image
                method: Matching method (BF, FLANN)
                crossCheck: Use cross-check for better matches

            Returns:
                List of feature matches
            )pbdoc")
        .def("detectObjects", &ComputerVision::detectObjects, py::arg("input"),
             py::arg("model") = ObjectDetectionModel::YOLO_V5,
             py::arg("confidenceThreshold") = 0.5,
             py::arg("nmsThreshold") = 0.4,
             R"pbdoc(
            Detect objects in image.

            Args:
                input: Input image
                model: Object detection model
                confidenceThreshold: Minimum confidence
                nmsThreshold: Non-maximum suppression threshold

            Returns:
                List of detections
            )pbdoc")
        .def("detectFaces", &ComputerVision::detectFaces, py::arg("input"),
             py::arg("model") = FaceModel::DNN_FACE,
             py::arg("confidenceThreshold") = 0.5,
             py::arg("detectLandmarks") = true,
             R"pbdoc(
            Detect faces in image.

            Args:
                input: Input image
                model: Face detection model
                confidenceThreshold: Minimum confidence
                detectLandmarks: Also detect facial landmarks

            Returns:
                List of face detections
            )pbdoc")
        .def("segmentImage", &ComputerVision::segmentImage, py::arg("input"),
             py::arg("method") = SegmentationMethod::WATERSHED,
             py::arg("params") = std::map<std::string, double>{},
             R"pbdoc(
            Segment image.

            Args:
                input: Input image
                method: Segmentation method
                params: Method-specific parameters

            Returns:
                Segmentation mask
            )pbdoc")
        .def("trackObjects", &ComputerVision::trackObjects, py::arg("frame"),
             py::arg("detections"), py::arg("method") = "SORT",
             R"pbdoc(
            Track objects across frames.

            Args:
                frame: Current frame
                detections: Current detections
                method: Tracking method (SORT, DeepSORT, ByteTrack)

            Returns:
                List of tracked objects with IDs
            )pbdoc")
        .def("computeOpticalFlow", &ComputerVision::computeOpticalFlow,
             py::arg("prevFrame"), py::arg("nextFrame"),
             py::arg("method") = "farneback",
             R"pbdoc(
            Compute optical flow between frames.

            Args:
                prevFrame: Previous frame
                nextFrame: Next frame
                method: Flow method (farneback, lucas_kanade, rlof)

            Returns:
                Flow field
            )pbdoc")
        .def("classifyImage", &ComputerVision::classifyImage, py::arg("input"),
             py::arg("modelPath"), py::arg("topK") = 5,
             "Classify image using neural network")
        .def("performOCR", &ComputerVision::performOCR, py::arg("input"),
             py::arg("language") = "eng", "Perform OCR text recognition")
        .def("estimatePose", &ComputerVision::estimatePose, py::arg("input"),
             py::arg("model") = "mediapipe", "Estimate human pose in image")
        .def("analyzeQuality", &ComputerVision::analyzeQuality,
             py::arg("input"),
             R"pbdoc(
            Analyze image quality.

            Returns dictionary with:
            - blur_score: Blur/sharpness score
            - noise_score: Noise level
            - exposure_score: Exposure quality
            - contrast_score: Contrast level
            )pbdoc")
        .def("detectAnomalies", &ComputerVision::detectAnomalies,
             py::arg("input"), py::arg("referenceImages"),
             "Detect anomalies compared to reference images")
        .def("extractFeatureVector", &ComputerVision::extractFeatureVector,
             py::arg("input"), py::arg("model") = "resnet50",
             "Extract deep feature vector from image")
        .def("findSimilarImages", &ComputerVision::findSimilarImages,
             py::arg("query"), py::arg("database"), py::arg("topK") = 10,
             "Find similar images in database")
        .def("loadModel", &ComputerVision::loadModel, py::arg("modelPath"),
             py::arg("configPath") = "", py::arg("modelType") = "onnx",
             "Load custom model")
        .def("setModelPath", &ComputerVision::setModelPath, py::arg("basePath"),
             "Set base path for model files");

    // Factory function
    cv_module.def("createOptimalComputerVision", &createOptimalComputerVision,
                  py::arg("useGPU") = false,
                  "Create optimal CV processor for current hardware");
}
