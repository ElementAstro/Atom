/**
 * @file gpu_ml.cpp
 * @brief Python bindings for GPU acceleration and ML processing
 */

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/image/processing/gpu_acceleration.hpp"
#include "atom/image/processing/ml_processing.hpp"

namespace py = pybind11;
using namespace atom::image;

void bind_gpu_ml(py::module& m) {
    // GPU acceleration submodule
    auto gpu_module =
        m.def_submodule("gpu", "GPU-accelerated image processing operations");

    // GPU device info
    py::class_<GPUDeviceInfo>(gpu_module, "DeviceInfo",
                              "Information about GPU device")
        .def(py::init<>())
        .def_readwrite("name", &GPUDeviceInfo::name, "Device name")
        .def_readwrite("compute_capability", &GPUDeviceInfo::computeCapability,
                       "Compute capability")
        .def_readwrite("total_memory", &GPUDeviceInfo::totalMemory,
                       "Total memory in bytes")
        .def_readwrite("free_memory", &GPUDeviceInfo::freeMemory,
                       "Free memory in bytes")
        .def_readwrite("is_available", &GPUDeviceInfo::isAvailable,
                       "Whether device is available")
        .def("__repr__", [](const GPUDeviceInfo& self) {
            return "<GPUDeviceInfo name='" + self.name + "' memory=" +
                   std::to_string(self.totalMemory / (1024 * 1024)) + "MB>";
        });

    // GPU processor
    py::class_<GPUProcessor>(gpu_module, "Processor",
                             R"pbdoc(
        GPU-accelerated image processor.

        Provides GPU-accelerated versions of common image processing operations
        for improved performance on supported hardware.
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def_static("is_available", &GPUProcessor::isAvailable,
                    "Check if GPU acceleration is available")
        .def_static("get_device_count", &GPUProcessor::getDeviceCount,
                    "Get number of available GPU devices")
        .def_static("get_device_info", &GPUProcessor::getDeviceInfo,
                    py::arg("device_id") = 0,
                    "Get information about GPU device")
        .def("set_device", &GPUProcessor::setDevice, py::arg("device_id"),
             "Set active GPU device")
        .def("upload", &GPUProcessor::upload, py::arg("image"),
             "Upload image to GPU memory")
        .def("download", &GPUProcessor::download, py::arg("gpu_image"),
             "Download image from GPU memory")
        .def("resize_gpu", &GPUProcessor::resizeGPU, py::arg("input"),
             py::arg("width"), py::arg("height"), "Resize image on GPU")
        .def("filter_gpu", &GPUProcessor::filterGPU, py::arg("input"),
             py::arg("filter_type"), "Apply filter on GPU")
        .def("convert_color_gpu", &GPUProcessor::convertColorGPU,
             py::arg("input"), py::arg("target_space"),
             "Convert color space on GPU")
        .def("threshold_gpu", &GPUProcessor::thresholdGPU, py::arg("input"),
             py::arg("threshold"), "Apply thresholding on GPU")
        .def("blend_gpu", &GPUProcessor::blendGPU, py::arg("image1"),
             py::arg("image2"), py::arg("alpha"), "Blend images on GPU")
        .def("get_memory_usage", &GPUProcessor::getMemoryUsage,
             "Get current GPU memory usage")
        .def("clear_cache", &GPUProcessor::clearCache,
             "Clear GPU memory cache");

    // ML processing submodule
    auto ml_module =
        m.def_submodule("ml", "Machine learning-based image processing");

    // ML model types
    py::enum_<MLModelType>(ml_module, "ModelType",
                           "Types of ML models for image processing")
        .value("CLASSIFICATION", MLModelType::CLASSIFICATION,
               "Image classification")
        .value("DETECTION", MLModelType::DETECTION, "Object detection")
        .value("SEGMENTATION", MLModelType::SEGMENTATION,
               "Semantic segmentation")
        .value("SUPER_RESOLUTION", MLModelType::SUPER_RESOLUTION,
               "Super resolution")
        .value("DENOISING", MLModelType::DENOISING, "Image denoising")
        .value("STYLE_TRANSFER", MLModelType::STYLE_TRANSFER, "Style transfer")
        .value("ENHANCEMENT", MLModelType::ENHANCEMENT, "Image enhancement")
        .export_values();

    // ML inference options
    py::class_<MLInferenceOptions>(ml_module, "InferenceOptions",
                                   "Options for ML model inference")
        .def(py::init<>())
        .def_readwrite("use_gpu", &MLInferenceOptions::useGPU,
                       "Use GPU for inference")
        .def_readwrite("batch_size", &MLInferenceOptions::batchSize,
                       "Batch size for inference")
        .def_readwrite("num_threads", &MLInferenceOptions::numThreads,
                       "Number of CPU threads")
        .def_readwrite("confidence_threshold",
                       &MLInferenceOptions::confidenceThreshold,
                       "Confidence threshold for detections")
        .def_readwrite("nms_threshold", &MLInferenceOptions::nmsThreshold,
                       "NMS threshold for detections");

    // Detection result
    py::class_<DetectionResult>(ml_module, "DetectionResult",
                                "Object detection result")
        .def(py::init<>())
        .def_readwrite("class_id", &DetectionResult::classId,
                       "Detected class ID")
        .def_readwrite("class_name", &DetectionResult::className,
                       "Detected class name")
        .def_readwrite("confidence", &DetectionResult::confidence,
                       "Detection confidence")
        .def_readwrite("bbox", &DetectionResult::bbox,
                       "Bounding box (x, y, width, height)")
        .def("__repr__", [](const DetectionResult& self) {
            return "<DetectionResult class='" + self.className +
                   "' confidence=" + std::to_string(self.confidence) + ">";
        });

    // ML processor
    py::class_<MLProcessor>(ml_module, "Processor",
                            R"pbdoc(
        Machine learning-based image processor.

        Provides ML-powered image processing operations including classification,
        detection, segmentation, and enhancement.
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def("load_model", &MLProcessor::loadModel, py::arg("model_path"),
             py::arg("model_type"), py::arg("config_path") = "",
             "Load ML model from file")
        .def("classify", &MLProcessor::classify, py::arg("input"),
             py::arg("top_k") = 5, py::arg("options") = MLInferenceOptions{},
             R"pbdoc(
            Classify image.

            Args:
                input: Input image
                top_k: Number of top predictions to return
                options: Inference options

            Returns:
                List of (class_name, confidence) tuples
            )pbdoc")
        .def("detect_objects", &MLProcessor::detectObjects, py::arg("input"),
             py::arg("options") = MLInferenceOptions{},
             R"pbdoc(
            Detect objects in image.

            Args:
                input: Input image
                options: Inference options

            Returns:
                List of DetectionResult objects
            )pbdoc")
        .def("segment", &MLProcessor::segment, py::arg("input"),
             py::arg("options") = MLInferenceOptions{},
             R"pbdoc(
            Perform semantic segmentation.

            Args:
                input: Input image
                options: Inference options

            Returns:
                Segmentation mask
            )pbdoc")
        .def("super_resolve", &MLProcessor::superResolve, py::arg("input"),
             py::arg("scale_factor") = 2,
             py::arg("options") = MLInferenceOptions{},
             R"pbdoc(
            Perform super resolution.

            Args:
                input: Input image
                scale_factor: Upscaling factor
                options: Inference options

            Returns:
                Super-resolved image
            )pbdoc")
        .def("denoise_ml", &MLProcessor::denoiseML, py::arg("input"),
             py::arg("options") = MLInferenceOptions{},
             R"pbdoc(
            Denoise image using ML model.

            Args:
                input: Noisy input image
                options: Inference options

            Returns:
                Denoised image
            )pbdoc")
        .def("transfer_style", &MLProcessor::transferStyle, py::arg("content"),
             py::arg("style"), py::arg("options") = MLInferenceOptions{},
             R"pbdoc(
            Apply style transfer.

            Args:
                content: Content image
                style: Style image
                options: Inference options

            Returns:
                Stylized image
            )pbdoc")
        .def("enhance_ml", &MLProcessor::enhanceML, py::arg("input"),
             py::arg("options") = MLInferenceOptions{},
             R"pbdoc(
            Enhance image using ML model.

            Args:
                input: Input image
                options: Inference options

            Returns:
                Enhanced image
            )pbdoc")
        .def("get_model_info", &MLProcessor::getModelInfo,
             "Get information about loaded model")
        .def("is_model_loaded", &MLProcessor::isModelLoaded,
             "Check if a model is loaded")
        .def("unload_model", &MLProcessor::unloadModel, "Unload current model");

    // Convenience functions
    ml_module.def("classify_image", &classifyImage, py::arg("input"),
                  py::arg("model_path"), py::arg("top_k") = 5,
                  "Classify image with specified model");

    ml_module.def("detect_objects_in_image", &detectObjectsInImage,
                  py::arg("input"), py::arg("model_path"),
                  py::arg("confidence_threshold") = 0.5,
                  "Detect objects in image");

    ml_module.def("upscale_image", &upscaleImage, py::arg("input"),
                  py::arg("scale_factor") = 2, py::arg("model_path") = "",
                  "Upscale image using ML super resolution");
}
