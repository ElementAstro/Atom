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

    // GPUBackend enum
    py::enum_<GPUBackend>(gpu_module, "Backend", "GPU computing backends")
        .value("CUDA", GPUBackend::CUDA, "NVIDIA CUDA")
        .value("OPENCL", GPUBackend::OPENCL, "OpenCL")
        .value("VULKAN", GPUBackend::VULKAN, "Vulkan compute")
        .value("METAL", GPUBackend::METAL, "Apple Metal")
        .value("DIRECTCOMPUTE", GPUBackend::DIRECTCOMPUTE, "DirectCompute")
        .value("HIP", GPUBackend::HIP, "AMD HIP")
        .value("SYCL", GPUBackend::SYCL, "SYCL/oneAPI")
        .value("AUTO", GPUBackend::AUTO, "Auto-select best backend")
        .export_values();

    // GPUMemoryType enum
    py::enum_<GPUMemoryType>(gpu_module, "MemoryType", "GPU memory types")
        .value("DEVICE", GPUMemoryType::DEVICE, "Device memory")
        .value("HOST", GPUMemoryType::HOST, "Host memory")
        .value("UNIFIED", GPUMemoryType::UNIFIED, "Unified memory")
        .value("PINNED", GPUMemoryType::PINNED, "Pinned host memory")
        .export_values();

    // GPUDeviceInfo struct
    py::class_<GPUDeviceInfo>(gpu_module, "DeviceInfo",
                              "Information about GPU device")
        .def(py::init<>())
        .def_readwrite("id", &GPUDeviceInfo::id, "Device ID")
        .def_readwrite("name", &GPUDeviceInfo::name, "Device name")
        .def_readwrite("vendor", &GPUDeviceInfo::vendor, "Device vendor")
        .def_readwrite("backend", &GPUDeviceInfo::backend, "GPU backend")
        .def_readwrite("computeUnits", &GPUDeviceInfo::computeUnits,
                       "Number of compute units")
        .def_readwrite("clockFrequency", &GPUDeviceInfo::clockFrequency,
                       "Clock frequency in MHz")
        .def_readwrite("totalMemory", &GPUDeviceInfo::totalMemory,
                       "Total memory in bytes")
        .def_readwrite("freeMemory", &GPUDeviceInfo::freeMemory,
                       "Free memory in bytes")
        .def_readwrite("maxWorkGroupSize", &GPUDeviceInfo::maxWorkGroupSize,
                       "Maximum work group size")
        .def_readwrite("supportsDouble", &GPUDeviceInfo::supportsDouble,
                       "Double precision support")
        .def_readwrite("supportsHalf", &GPUDeviceInfo::supportsHalf,
                       "Half precision support")
        .def_readwrite("driverVersion", &GPUDeviceInfo::driverVersion,
                       "Driver version")
        .def("__repr__", [](const GPUDeviceInfo& self) {
            return "<GPUDeviceInfo name='" + self.name + "' memory=" +
                   std::to_string(self.totalMemory / (1024 * 1024)) + "MB>";
        });

    // GPUBuffer class
    py::class_<GPUBuffer>(gpu_module, "Buffer", "GPU memory buffer")
        .def(py::init<>(), "Default constructor")
        .def("allocate", &GPUBuffer::allocate, py::arg("size"),
             py::arg("memType") = GPUMemoryType::DEVICE, "Allocate GPU memory")
        .def("deallocate", &GPUBuffer::deallocate, "Free GPU memory")
        .def("upload", &GPUBuffer::upload, py::arg("data"), py::arg("size"),
             "Upload data to GPU")
        .def("download", &GPUBuffer::download, py::arg("data"), py::arg("size"),
             "Download data from GPU")
        .def("copyTo", &GPUBuffer::copyTo, py::arg("dest"),
             "Copy to another buffer")
        .def("size", &GPUBuffer::size, "Get buffer size")
        .def("isAllocated", &GPUBuffer::isAllocated, "Check if allocated")
        .def("getMemoryType", &GPUBuffer::getMemoryType, "Get memory type");

    // GPUContext class
    py::class_<GPUContext>(gpu_module, "Context", "GPU execution context")
        .def(py::init<>(), "Default constructor")
        .def("initialize", &GPUContext::initialize,
             py::arg("backend") = GPUBackend::AUTO, py::arg("deviceId") = 0,
             "Initialize GPU context")
        .def("isInitialized", &GPUContext::isInitialized,
             "Check if context is initialized")
        .def("getBackend", &GPUContext::getBackend, "Get active backend")
        .def("getDeviceInfo", &GPUContext::getDeviceInfo,
             "Get device information")
        .def("synchronize", &GPUContext::synchronize,
             "Synchronize GPU operations")
        .def("getMemoryUsage", &GPUContext::getMemoryUsage,
             "Get current memory usage")
        .def("release", &GPUContext::release, "Release GPU resources");

    // GPUImageProcessor class
    py::class_<GPUImageProcessor>(gpu_module, "ImageProcessor",
                                  R"pbdoc(
        GPU-accelerated image processor.

        Provides GPU-accelerated versions of common image processing operations
        for improved performance on supported hardware.

        Example:
            >>> processor = gpu.ImageProcessor()
            >>> processor.initialize(gpu.Backend.CUDA)
            >>> result = processor.resize(image, 800, 600)
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def("initialize", &GPUImageProcessor::initialize,
             py::arg("backend") = GPUBackend::AUTO, py::arg("deviceId") = 0,
             "Initialize GPU processor")
        .def("uploadImage", &GPUImageProcessor::uploadImage, py::arg("image"),
             "Upload image to GPU memory")
        .def("downloadImage", &GPUImageProcessor::downloadImage,
             py::arg("gpuImage"), "Download image from GPU memory")
        .def("convolve", &GPUImageProcessor::convolve, py::arg("input"),
             py::arg("kernel"), "Apply convolution on GPU")
        .def("blur", &GPUImageProcessor::blur, py::arg("input"),
             py::arg("kernelSize") = 5, py::arg("sigma") = 1.0,
             "Apply Gaussian blur on GPU")
        .def("resize", &GPUImageProcessor::resize, py::arg("input"),
             py::arg("newWidth"), py::arg("newHeight"),
             py::arg("interpolation") = "linear", "Resize image on GPU")
        .def("convertColorSpace", &GPUImageProcessor::convertColorSpace,
             py::arg("input"), py::arg("fromSpace"), py::arg("toSpace"),
             "Convert color space on GPU")
        .def("equalizeHistogram", &GPUImageProcessor::equalizeHistogram,
             py::arg("input"), "Equalize histogram on GPU")
        .def("morphology", &GPUImageProcessor::morphology, py::arg("input"),
             py::arg("operation"), py::arg("kernelSize") = 3,
             "Apply morphological operation on GPU")
        .def("edgeDetect", &GPUImageProcessor::edgeDetect, py::arg("input"),
             py::arg("method") = "sobel", "Detect edges on GPU")
        .def("executeCustomKernel", &GPUImageProcessor::executeCustomKernel,
             py::arg("kernelSource"), py::arg("inputs"), py::arg("outputs"),
             py::arg("params"), "Execute custom GPU kernel")
        .def("processBatch", &GPUImageProcessor::processBatch,
             py::arg("images"), py::arg("operation"), py::arg("params"),
             "Process batch of images on GPU")
        .def("benchmark", &GPUImageProcessor::benchmark, py::arg("input"),
             py::arg("operation"), py::arg("iterations") = 100,
             "Benchmark GPU operation")
        .def("getPerformanceStats", &GPUImageProcessor::getPerformanceStats,
             "Get performance statistics")
        .def_static("isAvailable", &GPUImageProcessor::isAvailable,
                    py::arg("backend") = GPUBackend::AUTO,
                    "Check if GPU acceleration is available")
        .def_static("getAvailableBackends",
                    &GPUImageProcessor::getAvailableBackends,
                    "Get list of available GPU backends")
        .def_static("getDevices", &GPUImageProcessor::getDevices,
                    py::arg("backend") = GPUBackend::AUTO,
                    "Get list of available GPU devices");

    // Factory function
    gpu_module.def("createOptimalGPUProcessor", &createOptimalGPUProcessor,
                   "Create optimal GPU processor for current hardware");

    // ML processing submodule
    auto ml_module =
        m.def_submodule("ml", "Machine learning-based image processing");

    // MLModelType enum
    py::enum_<MLModelType>(ml_module, "ModelType",
                           "Types of ML models for image processing")
        .value("ESRGAN", MLModelType::ESRGAN, "Enhanced SRGAN")
        .value("REAL_ESRGAN", MLModelType::REAL_ESRGAN, "Real-ESRGAN")
        .value("SRCNN", MLModelType::SRCNN, "Super-Resolution CNN")
        .value("VDSR", MLModelType::VDSR, "Very Deep SR")
        .value("EDSR", MLModelType::EDSR, "Enhanced Deep SR")
        .value("WAIFU2X", MLModelType::WAIFU2X, "Waifu2x upscaler")
        .value("DNCNN", MLModelType::DNCNN, "DnCNN denoiser")
        .value("FFDNET", MLModelType::FFDNET, "FFDNet denoiser")
        .value("RIDNET", MLModelType::RIDNET, "RIDNet denoiser")
        .value("CBDNET", MLModelType::CBDNET, "CBDNet denoiser")
        .value("NEURAL_STYLE", MLModelType::NEURAL_STYLE,
               "Neural style transfer")
        .value("FAST_STYLE", MLModelType::FAST_STYLE, "Fast style transfer")
        .value("ADAIN", MLModelType::ADAIN, "AdaIN style transfer")
        .value("COLORIZATION", MLModelType::COLORIZATION, "Image colorization")
        .value("INPAINTING", MLModelType::INPAINTING, "Image inpainting")
        .value("BACKGROUND_REMOVAL", MLModelType::BACKGROUND_REMOVAL,
               "Background removal")
        .value("FACE_RESTORATION", MLModelType::FACE_RESTORATION,
               "Face restoration")
        .value("CUSTOM", MLModelType::CUSTOM, "Custom model")
        .export_values();

    // MLBackend enum
    py::enum_<MLBackend>(ml_module, "Backend", "ML inference backends")
        .value("ONNX", MLBackend::ONNX, "ONNX Runtime")
        .value("TENSORRT", MLBackend::TENSORRT, "NVIDIA TensorRT")
        .value("OPENVINO", MLBackend::OPENVINO, "Intel OpenVINO")
        .value("PYTORCH", MLBackend::PYTORCH, "PyTorch/LibTorch")
        .value("TENSORFLOW", MLBackend::TENSORFLOW, "TensorFlow")
        .value("NCNN", MLBackend::NCNN, "NCNN")
        .value("MNN", MLBackend::MNN, "MNN")
        .value("PADDLE", MLBackend::PADDLE, "PaddlePaddle")
        .value("AUTO", MLBackend::AUTO, "Auto-select backend")
        .export_values();

    // MLParams struct
    py::class_<MLParams>(ml_module, "Params", "ML processing parameters")
        .def(py::init<>())
        .def_readwrite("modelType", &MLParams::modelType, "Model type")
        .def_readwrite("backend", &MLParams::backend, "Inference backend")
        .def_readwrite("deviceId", &MLParams::deviceId, "Device ID")
        .def_readwrite("useGPU", &MLParams::useGPU, "Use GPU")
        .def_readwrite("batchSize", &MLParams::batchSize, "Batch size")
        .def_readwrite("numThreads", &MLParams::numThreads, "CPU threads")
        .def_readwrite("scaleFactor", &MLParams::scaleFactor, "Scale factor")
        .def_readwrite("tileSize", &MLParams::tileSize, "Tile size")
        .def_readwrite("tileOverlap", &MLParams::tileOverlap, "Tile overlap")
        .def_readwrite("denoiseStrength", &MLParams::denoiseStrength,
                       "Denoise strength");

    // MLResult struct
    py::class_<MLResult>(ml_module, "Result", "ML processing result")
        .def(py::init<>())
        .def_readwrite("success", &MLResult::success, "Operation succeeded")
        .def_readwrite("image", &MLResult::image, "Result image")
        .def_readwrite("processingTimeMs", &MLResult::processingTimeMs,
                       "Processing time in ms")
        .def_readwrite("errorMessage", &MLResult::errorMessage, "Error message")
        .def_readwrite("metadata", &MLResult::metadata, "Additional metadata")
        .def("__bool__", [](const MLResult& self) { return self.success; });

    // MLImageProcessor class
    py::class_<MLImageProcessor>(ml_module, "ImageProcessor",
                                 R"pbdoc(
        Machine learning-based image processor.

        Provides ML-powered image processing operations including super-resolution,
        denoising, style transfer, and enhancement.

        Example:
            >>> processor = ml.ImageProcessor()
            >>> processor.loadModel("esrgan.onnx", ml.ModelType.ESRGAN)
            >>> result = processor.superResolve(image, scale_factor=4)
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def("loadModel", &MLImageProcessor::loadModel, py::arg("modelPath"),
             py::arg("modelType") = MLModelType::CUSTOM,
             py::arg("backend") = MLBackend::AUTO, "Load ML model from file")
        .def("downloadModel", &MLImageProcessor::downloadModel,
             py::arg("modelType"), py::arg("targetPath") = "",
             "Download pre-trained model")
        .def("isModelAvailable", &MLImageProcessor::isModelAvailable,
             py::arg("modelType"), "Check if model is available")
        .def("superResolve", &MLImageProcessor::superResolve, py::arg("input"),
             py::arg("params") = MLParams{},
             R"pbdoc(
            Perform super resolution.

            Args:
                input: Input image
                params: Processing parameters

            Returns:
                MLResult with upscaled image
            )pbdoc")
        .def("denoise", &MLImageProcessor::denoise, py::arg("input"),
             py::arg("params") = MLParams{},
             R"pbdoc(
            Denoise image using ML model.

            Args:
                input: Noisy input image
                params: Processing parameters

            Returns:
                MLResult with denoised image
            )pbdoc")
        .def("transferStyle", &MLImageProcessor::transferStyle,
             py::arg("content"), py::arg("style"),
             py::arg("params") = MLParams{},
             R"pbdoc(
            Apply neural style transfer.

            Args:
                content: Content image
                style: Style image
                params: Processing parameters

            Returns:
                MLResult with stylized image
            )pbdoc")
        .def("enhance", &MLImageProcessor::enhance, py::arg("input"),
             py::arg("params") = MLParams{}, "Enhance image using ML model")
        .def("restore", &MLImageProcessor::restore, py::arg("input"),
             py::arg("params") = MLParams{}, "Restore degraded image")
        .def("generateFromText", &MLImageProcessor::generateFromText,
             py::arg("prompt"), py::arg("params") = MLParams{},
             "Generate image from text prompt")
        .def("inpaint", &MLImageProcessor::inpaint, py::arg("input"),
             py::arg("mask"), py::arg("params") = MLParams{},
             "Inpaint masked region")
        .def("colorize", &MLImageProcessor::colorize, py::arg("input"),
             py::arg("params") = MLParams{}, "Colorize grayscale image")
        .def("removeBackground", &MLImageProcessor::removeBackground,
             py::arg("input"), py::arg("params") = MLParams{},
             "Remove image background")
        .def("restoreFace", &MLImageProcessor::restoreFace, py::arg("input"),
             py::arg("params") = MLParams{}, "Restore/enhance faces")
        .def("getModelInfo", &MLImageProcessor::getModelInfo,
             py::arg("modelType"), "Get model information")
        .def("benchmarkModel", &MLImageProcessor::benchmarkModel,
             py::arg("modelType"), py::arg("inputSize"),
             py::arg("iterations") = 10, "Benchmark model performance")
        .def("unloadModel", &MLImageProcessor::unloadModel,
             "Unload current model")
        .def_static("getSupportedModels", &MLImageProcessor::getSupportedModels,
                    "Get list of supported model types")
        .def_static("getRecommendedBackend",
                    &MLImageProcessor::getRecommendedBackend,
                    "Get recommended inference backend");

    // Factory function
    ml_module.def("createOptimalMLProcessor", &createOptimalMLProcessor,
                  py::arg("useGPU") = true,
                  "Create optimal ML processor for current hardware");
}
