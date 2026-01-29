/**
 * @file filters.cpp
 * @brief Python bindings for image processing and filter operations
 */

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/image/processing/filters.hpp"
#include "atom/image/processing/image_processor.hpp"

namespace py = pybind11;
using namespace atom::image;

void bind_filters(py::module& m) {
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
        .def_readwrite("interpolation", &ProcessingOptions::interpolation,
                       "Interpolation method for resizing")
        .def_readwrite("borderMode", &ProcessingOptions::borderMode,
                       "Border handling mode")
        .def_readwrite("numThreads", &ProcessingOptions::numThreads,
                       "Number of processing threads (0=auto)")
        .def_readwrite("useGPU", &ProcessingOptions::useGPU,
                       "Use GPU acceleration if available")
        .def_readwrite("preserveMetadata", &ProcessingOptions::preserveMetadata,
                       "Preserve image metadata during processing")
        .def_readwrite("quality", &ProcessingOptions::quality,
                       "Quality setting for lossy operations (0-100)")
        .def_readwrite("colorSpace", &ProcessingOptions::colorSpace,
                       "Working color space")
        .def_readwrite("gammaCorrect", &ProcessingOptions::gammaCorrect,
                       "Apply gamma correction")
        .def("__repr__", [](const ProcessingOptions& self) {
            return "<ProcessingOptions quality=" +
                   std::to_string(self.quality) +
                   " threads=" + std::to_string(self.numThreads) + ">";
        });

    // FilterParams struct
    py::class_<FilterParams>(m, "FilterParams",
                             "Parameters for filter operations")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("kernelSize", &FilterParams::kernelSize,
                       "Filter kernel size")
        .def_readwrite("sigma", &FilterParams::sigma,
                       "Sigma for Gaussian operations")
        .def_readwrite("sigma2", &FilterParams::sigma2,
                       "Second sigma parameter")
        .def_readwrite("threshold1", &FilterParams::threshold1,
                       "First threshold")
        .def_readwrite("threshold2", &FilterParams::threshold2,
                       "Second threshold")
        .def_readwrite("strength", &FilterParams::strength,
                       "Filter strength (0-1)")
        .def_readwrite("iterations", &FilterParams::iterations,
                       "Number of iterations")
        .def_readwrite("borderMode", &FilterParams::borderMode,
                       "Border handling mode")
        .def_readwrite("normalize", &FilterParams::normalize,
                       "Normalize output");

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
            >>> blurred = processor.applyFilter(resized, FilterType.GAUSSIAN_BLUR)
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def(py::init<const ProcessingOptions&>(), py::arg("options"),
             "Construct with processing options")
        .def("convertFormat", &ImageProcessor::convertFormat, py::arg("input"),
             py::arg("targetFormat"),
             R"pbdoc(
            Convert image to different format.

            Args:
                input: Input image
                targetFormat: Target format string

            Returns:
                Converted image
            )pbdoc")
        .def("resize", &ImageProcessor::resize, py::arg("input"),
             py::arg("newWidth"), py::arg("newHeight"),
             py::arg("interpolation") = "cubic",
             R"pbdoc(
            Resize image to new dimensions.

            Args:
                input: Input image
                newWidth: Target width
                newHeight: Target height
                interpolation: Interpolation method ("nearest", "linear", "cubic", "lanczos")

            Returns:
                Resized image
            )pbdoc")
        .def("rotate", &ImageProcessor::rotate, py::arg("input"),
             py::arg("angle"),
             py::arg("center") = std::pair<double, double>{-1, -1},
             py::arg("scale") = 1.0,
             R"pbdoc(
            Rotate image by angle in degrees.

            Args:
                input: Input image
                angle: Rotation angle in degrees
                center: Rotation center (default: image center)
                scale: Scale factor

            Returns:
                Rotated image
            )pbdoc")
        .def("crop", &ImageProcessor::crop, py::arg("input"), py::arg("x"),
             py::arg("y"), py::arg("width"), py::arg("height"),
             "Crop image to specified rectangle")
        .def("applyFilter", &ImageProcessor::applyFilter, py::arg("input"),
             py::arg("filterType"), py::arg("params") = FilterParams{},
             R"pbdoc(
            Apply filter to image.

            Args:
                input: Input image
                filterType: Type of filter to apply
                params: Filter parameters

            Returns:
                Filtered image
            )pbdoc")
        .def("applyCustomKernel", &ImageProcessor::applyCustomKernel,
             py::arg("input"), py::arg("kernel"),
             py::arg("anchor") = std::pair<int, int>{-1, -1},
             py::arg("delta") = 0.0, py::arg("normalize") = true,
             "Apply custom convolution kernel")
        .def("adjustBrightnessContrast",
             &ImageProcessor::adjustBrightnessContrast, py::arg("input"),
             py::arg("brightness"), py::arg("contrast"),
             "Adjust image brightness (-100 to 100) and contrast (0.0 to 3.0)")
        .def("adjustGamma", &ImageProcessor::adjustGamma, py::arg("input"),
             py::arg("gamma"), "Apply gamma correction (0.1 to 3.0)")
        .def("enhanceHistogram", &ImageProcessor::enhanceHistogram,
             py::arg("input"), py::arg("clipLimit") = 2.0,
             py::arg("tileSize") = 8,
             "Enhance image using CLAHE histogram equalization")
        .def("detectEdges", &ImageProcessor::detectEdges, py::arg("input"),
             py::arg("method") = "canny", py::arg("threshold1") = 50.0,
             py::arg("threshold2") = 150.0, "Detect edges in image")
        .def("denoise", &ImageProcessor::denoise, py::arg("input"),
             py::arg("strength") = 10.0, py::arg("method") = "bilateral",
             "Remove noise from image")
        .def("processBatch", &ImageProcessor::processBatch, py::arg("inputs"),
             py::arg("operation"), py::arg("params") = FilterParams{},
             R"pbdoc(
            Process multiple images in batch.

            Args:
                inputs: List of input images
                operation: Operation to apply
                params: Operation parameters

            Returns:
                List of processed images
            )pbdoc")
        .def("getStatistics", &ImageProcessor::getStatistics, py::arg("input"),
             R"pbdoc(
            Get image statistics.

            Args:
                input: Input image

            Returns:
                Dictionary with min, max, mean, stddev, histogram
            )pbdoc")
        .def("calculateQualityMetrics",
             &ImageProcessor::calculateQualityMetrics, py::arg("input"),
             py::arg("reference") = blob{},
             R"pbdoc(
            Calculate image quality metrics.

            Args:
                input: Input image
                reference: Reference image for comparison (optional)

            Returns:
                Dictionary with quality metrics (PSNR, SSIM, etc.)
            )pbdoc")
        .def("setOptions", &ImageProcessor::setOptions, py::arg("options"),
             "Set processing options")
        .def("getOptions", &ImageProcessor::getOptions,
             "Get current processing options");

    // ImageFilter class
    py::class_<ImageFilter>(m, "ImageFilter",
                            R"pbdoc(
        Image filter processor.

        Provides various image filtering operations including blur,
        sharpening, edge detection, noise reduction, and morphological
        operations.
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def("apply", &ImageFilter::apply, py::arg("input"),
             py::arg("filterType"), py::arg("params") = FilterParams{},
             "Apply filter to image")
        .def("applyMultiple", &ImageFilter::applyMultiple, py::arg("input"),
             py::arg("filterTypes"), py::arg("params") = FilterParams{},
             "Apply multiple filters in sequence")
        .def("applyWithMask", &ImageFilter::applyWithMask, py::arg("input"),
             py::arg("mask"), py::arg("filterType"),
             py::arg("params") = FilterParams{}, "Apply filter with mask")
        .def("createKernel", &ImageFilter::createKernel, py::arg("filterType"),
             py::arg("size"), py::arg("params") = FilterParams{},
             "Create filter kernel")
        .def("convolve", &ImageFilter::convolve, py::arg("input"),
             py::arg("kernel"), py::arg("normalize") = true,
             "Convolve image with kernel")
        .def("separableConvolve", &ImageFilter::separableConvolve,
             py::arg("input"), py::arg("kernelX"), py::arg("kernelY"),
             "Apply separable convolution")
        .def_static("getFilterInfo", &ImageFilter::getFilterInfo,
                    py::arg("filterType"), "Get information about filter type");

    // Factory function for optimal filter
    m.def("createOptimalFilter", &createOptimalFilter,
          py::arg("useGPU") = false,
          "Create optimal filter processor based on available hardware");
}
