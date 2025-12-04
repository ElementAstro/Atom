/**
 * @file io.cpp
 * @brief Python bindings for image I/O operations
 */

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/image/io/format_detector.hpp"
#include "atom/image/io/image_loader.hpp"
#include "atom/image/io/image_saver.hpp"

namespace py = pybind11;
using namespace atom::image;

void bind_io(py::module& m) {
    // ImageFormat enum
    py::enum_<ImageFormat>(m, "ImageFormat",
                           R"pbdoc(
        Supported image file formats.

        This enum represents all image formats that can be detected and processed
        by the image I/O system.
        )pbdoc")
        .value("UNKNOWN", ImageFormat::UNKNOWN, "Unknown or unsupported format")
        .value("JPEG", ImageFormat::JPEG, "JPEG format")
        .value("PNG", ImageFormat::PNG, "PNG format")
        .value("BMP", ImageFormat::BMP, "BMP format")
        .value("TIFF", ImageFormat::TIFF, "TIFF format")
        .value("WEBP", ImageFormat::WEBP, "WebP format")
        .value("GIF", ImageFormat::GIF, "GIF format")
        .value("FITS", ImageFormat::FITS, "FITS astronomical format")
        .value("SER", ImageFormat::SER, "SER video format")
        .value("RAW", ImageFormat::RAW, "Camera RAW formats")
        .value("HEIF", ImageFormat::HEIF, "HEIF/HEIC format")
        .value("AVIF", ImageFormat::AVIF, "AVIF format")
        .value("PPM", ImageFormat::PPM, "PPM format")
        .value("PGM", ImageFormat::PGM, "PGM format")
        .value("PBM", ImageFormat::PBM, "PBM format")
        .export_values();

    // LoadOptions struct
    py::class_<LoadOptions>(m, "LoadOptions",
                            "Options for image loading operations")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("preferred_format", &LoadOptions::preferredFormat,
                       "Preferred image format (auto-detect if UNKNOWN)")
        .def_readwrite("convert_to_rgb", &LoadOptions::convertToRGB,
                       "Convert to RGB color space")
        .def_readwrite("normalize_pixels", &LoadOptions::normalizePixels,
                       "Normalize pixel values to [0,1]")
        .def_readwrite("target_width", &LoadOptions::targetWidth,
                       "Target width for resizing (-1 = no resize)")
        .def_readwrite("target_height", &LoadOptions::targetHeight,
                       "Target height for resizing (-1 = no resize)")
        .def_readwrite("preserve_aspect_ratio",
                       &LoadOptions::preserveAspectRatio,
                       "Preserve aspect ratio during resize")
        .def_readwrite("use_memory_mapping", &LoadOptions::useMemoryMapping,
                       "Use memory mapping for large files")
        .def_readwrite("max_memory_usage", &LoadOptions::maxMemoryUsage,
                       "Maximum memory usage in bytes")
        .def_readwrite("enable_caching", &LoadOptions::enableCaching,
                       "Enable result caching")
        .def_readwrite("cache_key", &LoadOptions::cacheKey, "Custom cache key")
        .def_readwrite("custom_options", &LoadOptions::customOptions,
                       "Format-specific custom options");

    // LoadResult struct
    py::class_<LoadResult>(m, "LoadResult",
                           "Result of an image loading operation")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("image_data", &LoadResult::imageData,
                       "Loaded image data")
        .def_readwrite("detected_format", &LoadResult::detectedFormat,
                       "Detected image format")
        .def_readwrite("metadata", &LoadResult::metadata, "Image metadata")
        .def_readwrite("error_message", &LoadResult::errorMessage,
                       "Error message if loading failed")
        .def_readwrite("success", &LoadResult::success,
                       "Whether loading was successful")
        .def_readwrite("load_time", &LoadResult::loadTime,
                       "Time taken to load the image")
        .def(
            "__bool__", [](const LoadResult& self) { return self.success; },
            "Check if loading was successful")
        .def("__repr__", [](const LoadResult& self) {
            return "<LoadResult success=" +
                   std::string(self.success ? "True" : "False") + " format=" +
                   std::to_string(static_cast<int>(self.detectedFormat)) + ">";
        });

    // BatchLoadResult struct
    py::class_<BatchLoadResult>(m, "BatchLoadResult",
                                "Result of a batch image loading operation")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("results", &BatchLoadResult::results,
                       "Individual load results")
        .def_readwrite("success_count", &BatchLoadResult::successCount,
                       "Number of successful loads")
        .def_readwrite("failure_count", &BatchLoadResult::failureCount,
                       "Number of failed loads")
        .def_readwrite("total_time", &BatchLoadResult::totalTime,
                       "Total time for batch operation")
        .def("__len__",
             [](const BatchLoadResult& self) { return self.results.size(); })
        .def("__repr__", [](const BatchLoadResult& self) {
            return "<BatchLoadResult success=" +
                   std::to_string(self.successCount) +
                   " failed=" + std::to_string(self.failureCount) + ">";
        });

    // ImageLoader class
    py::class_<ImageLoader>(m, "ImageLoader",
                            R"pbdoc(
        Advanced image loader with support for multiple formats.

        The ImageLoader class provides comprehensive image loading capabilities
        including format detection, streaming I/O, memory mapping, and batch
        operations.

        Example:
            >>> loader = ImageLoader()
            >>> result = loader.load_from_file("image.jpg")
            >>> if result.success:
            >>>     print(f"Loaded {result.image_data.cols()}x{result.image_data.rows()} image")
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def("load_from_file", &ImageLoader::loadFromFile, py::arg("file_path"),
             py::arg("options") = LoadOptions{},
             py::arg("progress_callback") = nullptr,
             R"pbdoc(
            Load image from file.

            Args:
                file_path: Path to the image file
                options: Loading options
                progress_callback: Optional callback for progress updates

            Returns:
                LoadResult with image data and metadata
            )pbdoc")
        .def("load_from_memory", &ImageLoader::loadFromMemory, py::arg("data"),
             py::arg("size"), py::arg("options") = LoadOptions{},
             py::arg("progress_callback") = nullptr,
             "Load image from memory buffer")
        .def("load_batch", &ImageLoader::loadBatch, py::arg("file_paths"),
             py::arg("options") = LoadOptions{}, py::arg("max_parallel") = 4,
             py::arg("progress_callback") = nullptr,
             R"pbdoc(
            Load multiple images in batch.

            Args:
                file_paths: List of file paths to load
                options: Loading options applied to all images
                max_parallel: Maximum number of parallel loading operations
                progress_callback: Optional callback for progress updates

            Returns:
                BatchLoadResult with all loading results
            )pbdoc")
        .def("load_async", &ImageLoader::loadAsync, py::arg("file_path"),
             py::arg("options") = LoadOptions{},
             "Load image asynchronously, returns a future")
        .def("supports_format", &ImageLoader::supportsFormat, py::arg("format"),
             "Check if a specific format is supported")
        .def("get_supported_formats", &ImageLoader::getSupportedFormats,
             "Get list of all supported formats")
        .def("clear_cache", &ImageLoader::clearCache, "Clear the image cache")
        .def("get_cache_size", &ImageLoader::getCacheSize,
             "Get current cache size in bytes")
        .def("set_max_cache_size", &ImageLoader::setMaxCacheSize,
             py::arg("size"), "Set maximum cache size in bytes");

    // SaveOptions struct
    py::class_<SaveOptions>(m, "SaveOptions",
                            "Options for image saving operations")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("format", &SaveOptions::format,
                       "Output format (auto-detect from extension if UNKNOWN)")
        .def_readwrite("quality", &SaveOptions::quality,
                       "Quality for lossy formats (0-100)")
        .def_readwrite("compression_level", &SaveOptions::compressionLevel,
                       "Compression level for lossless formats")
        .def_readwrite("preserve_metadata", &SaveOptions::preserveMetadata,
                       "Preserve existing metadata")
        .def_readwrite("overwrite", &SaveOptions::overwrite,
                       "Overwrite existing files")
        .def_readwrite("create_directories", &SaveOptions::createDirectories,
                       "Create parent directories if needed")
        .def_readwrite("custom_options", &SaveOptions::customOptions,
                       "Format-specific custom options");

    // SaveResult struct
    py::class_<SaveResult>(m, "SaveResult",
                           "Result of an image saving operation")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("success", &SaveResult::success,
                       "Whether saving was successful")
        .def_readwrite("bytes_written", &SaveResult::bytesWritten,
                       "Number of bytes written")
        .def_readwrite("error_message", &SaveResult::errorMessage,
                       "Error message if saving failed")
        .def_readwrite("save_time", &SaveResult::saveTime,
                       "Time taken to save the image")
        .def("__bool__", [](const SaveResult& self) { return self.success; });

    // ImageSaver class
    py::class_<ImageSaver>(
        m, "ImageSaver",
        "Advanced image saver with support for multiple formats")
        .def(py::init<>(), "Default constructor")
        .def("save_to_file", &ImageSaver::saveToFile, py::arg("image"),
             py::arg("file_path"), py::arg("options") = SaveOptions{},
             py::arg("progress_callback") = nullptr, "Save image to file")
        .def("save_to_memory", &ImageSaver::saveToMemory, py::arg("image"),
             py::arg("format") = ImageFormat::PNG,
             py::arg("options") = SaveOptions{}, "Save image to memory buffer")
        .def("save_batch", &ImageSaver::saveBatch, py::arg("images"),
             py::arg("file_paths"), py::arg("options") = SaveOptions{},
             py::arg("max_parallel") = 4, "Save multiple images in batch")
        .def("save_async", &ImageSaver::saveAsync, py::arg("image"),
             py::arg("file_path"), py::arg("options") = SaveOptions{},
             "Save image asynchronously")
        .def("supports_format", &ImageSaver::supportsFormat, py::arg("format"),
             "Check if a specific format is supported for saving");

    // FormatDetector class
    py::class_<FormatDetector>(m, "FormatDetector",
                               "Automatic image format detection")
        .def(py::init<>(), "Default constructor")
        .def("detect_from_file", &FormatDetector::detectFromFile,
             py::arg("file_path"), "Detect format from file")
        .def("detect_from_memory", &FormatDetector::detectFromMemory,
             py::arg("data"), py::arg("size"),
             "Detect format from memory buffer")
        .def("detect_from_extension", &FormatDetector::detectFromExtension,
             py::arg("filename"), "Detect format from file extension")
        .def("get_format_name", &FormatDetector::getFormatName,
             py::arg("format"), "Get human-readable format name")
        .def("get_format_extensions", &FormatDetector::getFormatExtensions,
             py::arg("format"), "Get list of file extensions for a format");
}
