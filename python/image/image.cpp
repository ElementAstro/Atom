/**
 * @file image.cpp
 * @brief Python bindings for the Atom Image Processing module
 *
 * This file provides the main module definition and imports all submodule
 * bindings.
 */

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/image/image.hpp"

namespace py = pybind11;

// Forward declarations for binding functions defined in submodule files
// Core module (core/)
void bind_core(py::module& m);

// I/O module (io/)
void bind_io(py::module& m);

// Formats module (formats/)
void bind_formats(py::module& m);

// Metadata module (metadata/)
void bind_metadata(py::module& m);

// Processing modules (processing/)
void bind_filters(py::module& m);
void bind_transforms(py::module& m);
void bind_enhancement(py::module& m);
void bind_computer_vision(py::module& m);
void bind_gpu_ml(py::module& m);
void bind_realtime(py::module& m);
void bind_ocr(py::module& m);

/**
 * @brief Main module definition for atom_image
 */
PYBIND11_MODULE(atom_image, m) {
    m.doc() = R"pbdoc(
        Atom Image Processing Module
        =============================

        Comprehensive image processing library for Python with support for:
        - Multiple image formats (JPEG, PNG, TIFF, FITS, SER, etc.)
        - Advanced image processing operations
        - Metadata extraction and manipulation
        - Computer vision algorithms
        - GPU acceleration (when available)
        - OCR capabilities (when available)

        The module is organized into several submodules:

        Core Module (atom_image.core):
            - Blob: Main image data container
            - BlobMode: Memory management modes
            - Image metadata structures

        I/O Module (atom_image.io):
            - ImageLoader: Load images from files or memory
            - ImageSaver: Save images to files
            - FormatDetector: Automatic format detection

        Processing Module (atom_image.processing):
            - ImageProcessor: Main processing pipeline
            - Filters: Various image filters
            - Transforms: Geometric transformations
            - Enhancement: Image enhancement operations
            - ComputerVision: CV algorithms

        Formats Module (atom_image.formats):
            - FITS file support
            - SER format support
            - Advanced format handlers

        Metadata Module (atom_image.metadata):
            - EXIF data extraction and manipulation
            - GPS coordinate handling

        Example Usage:
        --------------
        >>> import atom_image as ai
        >>> import numpy as np
        >>>
        >>> # Load an image
        >>> loader = ai.ImageLoader()
        >>> result = loader.load_from_file("image.jpg")
        >>>
        >>> # Process the image
        >>> processor = ai.ImageProcessor()
        >>> resized = processor.resize(result.image_data, 800, 600)
        >>>
        >>> # Apply filters
        >>> blurred = processor.apply_filter(resized, ai.FilterType.GAUSSIAN_BLUR)
        >>>
        >>> # Save result
        >>> saver = ai.ImageSaver()
        >>> saver.save_to_file(blurred, "output.jpg")
    )pbdoc";

    // Module version and feature information
    py::class_<atom::image::Version>(
        m, "Version", "Version information for the image processing module")
        .def_readonly_static("MAJOR", &atom::image::Version::MAJOR,
                             "Major version number")
        .def_readonly_static("MINOR", &atom::image::Version::MINOR,
                             "Minor version number")
        .def_readonly_static("PATCH", &atom::image::Version::PATCH,
                             "Patch version number")
        .def_readonly_static("STRING", &atom::image::Version::STRING,
                             "Full version string");

    py::class_<atom::image::Features>(
        m, "Features", "Feature availability flags for optional dependencies")
        .def_readonly_static("HAS_OPENCV", &atom::image::Features::HAS_OPENCV,
                             "Whether OpenCV support is available")
        .def_readonly_static("HAS_CFITSIO", &atom::image::Features::HAS_CFITSIO,
                             "Whether CFITSIO support is available")
        .def_readonly_static("HAS_OCR", &atom::image::Features::HAS_OCR,
                             "Whether OCR support is available");

    // Module-level functions
    m.def("get_version", &atom::image::getVersion,
          "Get module version information");

    m.def("get_features", &atom::image::getFeatures,
          "Get available features and capabilities");

    // Bind submodules - organized by category
    // Core functionality
    bind_core(m);

    // I/O operations
    bind_io(m);

    // Format support
    bind_formats(m);

    // Metadata handling
    bind_metadata(m);

    // Processing modules
    bind_filters(m);
    bind_transforms(m);
    bind_enhancement(m);
    bind_computer_vision(m);
    bind_gpu_ml(m);
    bind_realtime(m);
    bind_ocr(m);

    // Module attributes
    m.attr("__version__") = atom::image::Version::STRING;
    m.attr("__author__") = "Atom Framework Team";
}
