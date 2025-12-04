/**
 * @file processing.cpp
 * @brief Python bindings for image processing operations
 */

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/image/processing/computer_vision.hpp"
#include "atom/image/processing/enhancement.hpp"
#include "atom/image/processing/filters.hpp"
#include "atom/image/processing/image_processor.hpp"
#include "atom/image/processing/transforms.hpp"

namespace py = pybind11;
using namespace atom::image;

// Forward declarations for additional binding functions
void bind_transforms(py::module& m);
void bind_enhancement(py::module& m);
void bind_realtime(py::module& m);

void bind_processing(py::module& m) {
    // FilterType enum
    py::enum_<FilterType>(m, "FilterType",
                          "Types of image filters available for processing")
        // Basic filters
        .value("GAUSSIAN_BLUR", FilterType::GAUSSIAN_BLUR,
               "Gaussian blur filter")
        .value("BOX_BLUR", FilterType::BOX_BLUR, "Box blur filter")
        .value("MOTION_BLUR", FilterType::MOTION_BLUR, "Motion blur filter")
        .value("RADIAL_BLUR", FilterType::RADIAL_BLUR, "Radial blur filter")
        // Sharpening
        .value("SHARPEN", FilterType::SHARPEN, "Sharpen filter")
        .value("UNSHARP_MASK", FilterType::UNSHARP_MASK, "Unsharp mask filter")
        .value("HIGH_PASS", FilterType::HIGH_PASS, "High-pass filter")
        // Edge detection
        .value("SOBEL", FilterType::SOBEL, "Sobel edge detection")
        .value("PREWITT", FilterType::PREWITT, "Prewitt edge detection")
        .value("ROBERTS", FilterType::ROBERTS, "Roberts edge detection")
        .value("CANNY", FilterType::CANNY, "Canny edge detection")
        .value("LAPLACIAN", FilterType::LAPLACIAN, "Laplacian edge detection")
        // Noise reduction
        .value("MEDIAN", FilterType::MEDIAN, "Median filter")
        .value("BILATERAL", FilterType::BILATERAL, "Bilateral filter")
        .value("NON_LOCAL_MEANS", FilterType::NON_LOCAL_MEANS,
               "Non-local means filter")
        .value("WIENER", FilterType::WIENER, "Wiener filter")
        // Morphological
        .value("EROSION", FilterType::EROSION, "Erosion operation")
        .value("DILATION", FilterType::DILATION, "Dilation operation")
        .value("OPENING", FilterType::OPENING, "Opening operation")
        .value("CLOSING", FilterType::CLOSING, "Closing operation")
        .value("GRADIENT", FilterType::GRADIENT, "Morphological gradient")
        .value("TOP_HAT", FilterType::TOP_HAT, "Top-hat transform")
        .value("BLACK_HAT", FilterType::BLACK_HAT, "Black-hat transform")
        // Frequency domain
        .value("LOW_PASS", FilterType::LOW_PASS, "Low-pass filter")
        .value("HIGH_PASS_FREQ", FilterType::HIGH_PASS_FREQ,
               "High-pass frequency filter")
        .value("BAND_PASS", FilterType::BAND_PASS, "Band-pass filter")
        .value("BAND_STOP", FilterType::BAND_STOP, "Band-stop filter")
        .value("NOTCH", FilterType::NOTCH, "Notch filter")
        // Artistic
        .value("EMBOSS", FilterType::EMBOSS, "Emboss effect")
        .value("EDGE_ENHANCE", FilterType::EDGE_ENHANCE, "Edge enhancement")
        .value("FIND_EDGES", FilterType::FIND_EDGES, "Find edges")
        .value("SMOOTH", FilterType::SMOOTH, "Smooth filter")
        .value("SMOOTH_MORE", FilterType::SMOOTH_MORE, "Extra smooth filter")
        .value("CUSTOM_KERNEL", FilterType::CUSTOM_KERNEL,
               "Custom convolution kernel")
        .export_values();

    // StructuringElement enum
    py::enum_<StructuringElement>(
        m, "StructuringElement",
        "Shapes for morphological structuring elements")
        .value("RECTANGLE", StructuringElement::RECTANGLE,
               "Rectangular element")
        .value("ELLIPSE", StructuringElement::ELLIPSE, "Elliptical element")
        .value("CROSS", StructuringElement::CROSS, "Cross-shaped element")
        .value("DIAMOND", StructuringElement::DIAMOND, "Diamond-shaped element")
        .value("CUSTOM", StructuringElement::CUSTOM, "Custom element")
        .export_values();

    // ProcessingOptions struct
    py::class_<ProcessingOptions>(
        m, "ProcessingOptions",
        "Configuration options for image processing operations")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("preserve_aspect_ratio",
                       &ProcessingOptions::preserveAspectRatio,
                       "Preserve aspect ratio during transformations")
        .def_readwrite("use_multithreading",
                       &ProcessingOptions::useMultithreading,
                       "Enable multithreading for processing")
        .def_readwrite("quality", &ProcessingOptions::quality,
                       "Quality setting for lossy operations (0-100)")
        .def_readwrite("enable_simd", &ProcessingOptions::enableSIMD,
                       "Enable SIMD optimizations")
        .def_readwrite("max_memory_usage", &ProcessingOptions::maxMemoryUsage,
                       "Maximum memory usage in bytes");

    // ImageProcessor class
    py::class_<ImageProcessor>(m, "ImageProcessor",
                               R"pbdoc(
        High-performance image processing pipeline.

        The ImageProcessor class provides a comprehensive set of image processing
        operations including filtering, transformations, enhancement, and format
        conversion with automatic optimization and backend selection.

        Example:
            >>> processor = ImageProcessor()
            >>> resized = processor.resize(image, 800, 600)
            >>> blurred = processor.apply_filter(resized, FilterType.GAUSSIAN_BLUR)
        )pbdoc")
        .def(py::init<const ProcessingOptions&>(),
             py::arg("options") = ProcessingOptions{},
             "Construct with processing options")
        .def("convert_format", &ImageProcessor::convertFormat, py::arg("input"),
             py::arg("target_format"), "Convert image to different format")
        .def("resize", &ImageProcessor::resize, py::arg("input"),
             py::arg("new_width"), py::arg("new_height"),
             py::arg("algorithm") = "cubic",
             R"pbdoc(
            Resize image to new dimensions.

            Args:
                input: Input image
                new_width: Target width
                new_height: Target height
                algorithm: Resize algorithm ("nearest", "linear", "cubic", "lanczos")

            Returns:
                Resized image
            )pbdoc")
        .def("rotate", &ImageProcessor::rotate, py::arg("input"),
             py::arg("angle"), py::arg("expand_canvas") = true,
             "Rotate image by angle in degrees")
        .def("crop", &ImageProcessor::crop, py::arg("input"), py::arg("x"),
             py::arg("y"), py::arg("width"), py::arg("height"),
             "Crop image to specified rectangle")
        .def("apply_filter", &ImageProcessor::applyFilter, py::arg("input"),
             py::arg("filter_type"),
             py::arg("parameters") = std::unordered_map<std::string, double>{},
             R"pbdoc(
            Apply filter to image.

            Args:
                input: Input image
                filter_type: Type of filter to apply
                parameters: Filter-specific parameters

            Returns:
                Filtered image
            )pbdoc")
        .def("apply_custom_kernel", &ImageProcessor::applyCustomKernel,
             py::arg("input"), py::arg("kernel"), py::arg("kernel_size"),
             "Apply custom convolution kernel")
        .def("adjust_brightness_contrast",
             &ImageProcessor::adjustBrightnessContrast, py::arg("input"),
             py::arg("brightness"), py::arg("contrast"),
             "Adjust image brightness and contrast")
        .def("adjust_gamma", &ImageProcessor::adjustGamma, py::arg("input"),
             py::arg("gamma"), "Apply gamma correction")
        .def("enhance_histogram", &ImageProcessor::enhanceHistogram,
             py::arg("input"), py::arg("adaptive") = false,
             "Enhance image using histogram equalization")
        .def("denoise", &ImageProcessor::denoise, py::arg("input"),
             py::arg("strength") = 10.0, "Remove noise from image")
        .def("sharpen", &ImageProcessor::sharpen, py::arg("input"),
             py::arg("amount") = 1.0, "Sharpen image")
        .def("detect_edges", &ImageProcessor::detectEdges, py::arg("input"),
             py::arg("method") = "canny", "Detect edges in image")
        .def("convert_color_space", &ImageProcessor::convertColorSpace,
             py::arg("input"), py::arg("target_space"),
             "Convert image color space")
        .def("flip", &ImageProcessor::flip, py::arg("input"),
             py::arg("flip_code"),
             "Flip image (0=vertical, 1=horizontal, -1=both)")
        .def("transpose", &ImageProcessor::transpose, py::arg("input"),
             "Transpose image (swap rows and columns)")
        .def("apply_morphology", &ImageProcessor::applyMorphology,
             py::arg("input"), py::arg("operation"),
             py::arg("element") = StructuringElement::RECTANGLE,
             py::arg("size") = 3, "Apply morphological operation")
        .def("blend", &ImageProcessor::blend, py::arg("image1"),
             py::arg("image2"), py::arg("alpha"),
             "Blend two images with alpha blending")
        .def("create_thumbnail", &ImageProcessor::createThumbnail,
             py::arg("input"), py::arg("max_size"),
             "Create thumbnail with maximum dimension")
        .def("auto_enhance", &ImageProcessor::autoEnhance, py::arg("input"),
             "Automatically enhance image quality")
        .def("get_histogram", &ImageProcessor::getHistogram, py::arg("input"),
             py::arg("channel") = -1, "Get image histogram")
        .def("normalize", &ImageProcessor::normalize, py::arg("input"),
             "Normalize image pixel values")
        .def("threshold", &ImageProcessor::threshold, py::arg("input"),
             py::arg("threshold_value"), py::arg("max_value") = 255.0,
             py::arg("type") = "binary", "Apply thresholding to image")
        .def("adaptive_threshold", &ImageProcessor::adaptiveThreshold,
             py::arg("input"), py::arg("max_value") = 255.0,
             py::arg("method") = "gaussian", py::arg("block_size") = 11,
             py::arg("constant") = 2.0, "Apply adaptive thresholding");

    // Filter class
    py::class_<Filter>(m, "Filter", "Base class for image filters")
        .def("apply", &Filter::apply, py::arg("input"),
             py::arg("parameters") = FilterParamMap{}, "Apply filter to image")
        .def("get_name", &Filter::getName, "Get filter name")
        .def("get_description", &Filter::getDescription,
             "Get filter description")
        .def("get_parameter_info", &Filter::getParameterInfo,
             "Get information about filter parameters");

    // FilterFactory for creating filters
    m.def("create_filter", &createFilter, py::arg("type"),
          "Create a filter instance by type");

    // Transform operations
    m.def("flip_horizontal", &flipHorizontal, py::arg("input"),
          "Flip image horizontally");

    m.def("flip_vertical", &flipVertical, py::arg("input"),
          "Flip image vertically");

    m.def("rotate_90", &rotate90, py::arg("input"), py::arg("clockwise") = true,
          "Rotate image by 90 degrees");

    m.def("rotate_180", &rotate180, py::arg("input"),
          "Rotate image by 180 degrees");

    // Enhancement operations
    m.def("auto_contrast", &autoContrast, py::arg("input"),
          py::arg("clip_percent") = 1.0, "Automatically adjust image contrast");

    m.def("auto_white_balance", &autoWhiteBalance, py::arg("input"),
          "Automatically adjust white balance");

    m.def("remove_vignette", &removeVignette, py::arg("input"),
          "Remove vignetting effect");

    // Bind additional processing submodules
    bind_transforms(m);
    bind_enhancement(m);
    bind_realtime(m);
}
