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
                           "Supported image file formats for I/O operations")
        .value("UNKNOWN", ImageFormat::UNKNOWN, "Unknown or unsupported format")
        .value("JPEG", ImageFormat::JPEG, "JPEG format (lossy compression)")
        .value("PNG", ImageFormat::PNG,
               "PNG format (lossless compression, transparency support)")
        .value("BMP", ImageFormat::BMP, "BMP format (uncompressed bitmap)")
        .value("TIFF", ImageFormat::TIFF,
               "TIFF format (various compression options)")
        .value("GIF", ImageFormat::GIF, "GIF format (animation support)")
        .value("WEBP", ImageFormat::WEBP, "WebP format (modern web format)")
        .value("HDR", ImageFormat::HDR, "HDR format (high dynamic range)")
        .value("EXR", ImageFormat::EXR, "OpenEXR format (HDR for VFX)")
        .value("PSD", ImageFormat::PSD, "Photoshop format")
        .value("RAW", ImageFormat::RAW, "Camera RAW format")
        .value("FITS", ImageFormat::FITS, "FITS format (astronomy)")
        .value("SER", ImageFormat::SER, "SER format (astronomy video)")
        .value("ICO", ImageFormat::ICO, "ICO format (icons)")
        .value("TGA", ImageFormat::TGA, "TGA format (Targa)")
        .value("SVG", ImageFormat::SVG, "SVG format (vector graphics)")
        .value("HEIF", ImageFormat::HEIF, "HEIF/HEIC format")
        .value("AVIF", ImageFormat::AVIF, "AVIF format")
        .export_values();

    // LoadOptions struct
    py::class_<LoadOptions>(m, "LoadOptions",
                            R"pbdoc(
        Options for image loading operations.

        These options control how images are loaded from files or memory,
        including color conversion, scaling, and format-specific settings.
        )pbdoc")
        .def(py::init<>(), "Default constructor with sensible defaults")
        .def_readwrite("targetFormat", &LoadOptions::targetFormat,
                       "Target format hint for loading")
        .def_readwrite("targetWidth", &LoadOptions::targetWidth,
                       "Target width (0 = original)")
        .def_readwrite("targetHeight", &LoadOptions::targetHeight,
                       "Target height (0 = original)")
        .def_readwrite("preserveAspectRatio", &LoadOptions::preserveAspectRatio,
                       "Preserve aspect ratio when resizing")
        .def_readwrite("loadMetadata", &LoadOptions::loadMetadata,
                       "Load image metadata (EXIF, etc.)")
        .def_readwrite("convertToRGB", &LoadOptions::convertToRGB,
                       "Convert loaded image to RGB")
        .def_readwrite("convertToGrayscale", &LoadOptions::convertToGrayscale,
                       "Convert loaded image to grayscale")
        .def_readwrite("flipVertically", &LoadOptions::flipVertically,
                       "Flip image vertically on load")
        .def_readwrite("useCache", &LoadOptions::useCache,
                       "Use image cache if available")
        .def_readwrite("decodeRAW", &LoadOptions::decodeRAW,
                       "Decode RAW images")
        .def_readwrite("rawProcessing", &LoadOptions::rawProcessing,
                       "RAW processing options")
        .def("__repr__", [](const LoadOptions& self) {
            return "<LoadOptions grayscale=" +
                   std::string(self.convertToGrayscale ? "True" : "False") +
                   " metadata=" +
                   std::string(self.loadMetadata ? "True" : "False") + ">";
        });

    // LoadResult struct
    py::class_<LoadResult>(m, "LoadResult",
                           R"pbdoc(
        Result of an image loading operation.

        Contains the loaded image data, metadata, and status information.
        )pbdoc")
        .def(py::init<>())
        .def_readwrite("success", &LoadResult::success,
                       "Whether the load operation succeeded")
        .def_readwrite("image", &LoadResult::image, "Loaded image data")
        .def_readwrite("metadata", &LoadResult::metadata, "Image metadata")
        .def_readwrite("detectedFormat", &LoadResult::detectedFormat,
                       "Detected image format")
        .def_readwrite("errorMessage", &LoadResult::errorMessage,
                       "Error message if loading failed")
        .def_readwrite("errorCode", &LoadResult::errorCode, "Error code")
        .def_readwrite("loadTimeMs", &LoadResult::loadTimeMs,
                       "Time taken to load in milliseconds")
        .def_readwrite("bytesRead", &LoadResult::bytesRead,
                       "Number of bytes read")
        .def_readwrite("fromCache", &LoadResult::fromCache,
                       "Whether loaded from cache")
        .def("__bool__", [](const LoadResult& self) { return self.success; })
        .def("__repr__", [](const LoadResult& self) {
            return "<LoadResult success=" +
                   std::string(self.success ? "True" : "False") + ">";
        });

    // BatchLoadResult struct
    py::class_<BatchLoadResult>(m, "BatchLoadResult",
                                "Result of batch image loading operation")
        .def(py::init<>())
        .def_readwrite("results", &BatchLoadResult::results,
                       "Individual load results")
        .def_readwrite("successCount", &BatchLoadResult::successCount,
                       "Number of successful loads")
        .def_readwrite("failureCount", &BatchLoadResult::failureCount,
                       "Number of failed loads")
        .def_readwrite("totalTimeMs", &BatchLoadResult::totalTimeMs,
                       "Total time taken in milliseconds")
        .def_readwrite("totalBytesRead", &BatchLoadResult::totalBytesRead,
                       "Total bytes read")
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
        High-performance image loader with caching support.

        The ImageLoader class provides efficient image loading from files,
        memory buffers, and URLs with optional caching for repeated loads.

        Example:
            >>> loader = ImageLoader()
            >>> result = loader.load_from_file("image.jpg")
            >>> if result.success:
            >>>     process_image(result.image)
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def("loadFromFile", &ImageLoader::loadFromFile, py::arg("path"),
             py::arg("options") = LoadOptions{},
             R"pbdoc(
            Load image from file.

            Args:
                path: Path to the image file
                options: Load options

            Returns:
                LoadResult with image data and metadata
            )pbdoc")
        .def("loadFromMemory", &ImageLoader::loadFromMemory, py::arg("data"),
             py::arg("size"), py::arg("options") = LoadOptions{},
             R"pbdoc(
            Load image from memory buffer.

            Args:
                data: Image data pointer
                size: Data size in bytes
                options: Load options

            Returns:
                LoadResult with image data and metadata
            )pbdoc")
        .def("loadFromURL", &ImageLoader::loadFromURL, py::arg("url"),
             py::arg("options") = LoadOptions{},
             py::arg("timeout") = std::chrono::milliseconds{30000},
             R"pbdoc(
            Load image from URL.

            Args:
                url: Image URL
                options: Load options
                timeout: Request timeout

            Returns:
                LoadResult with image data and metadata
            )pbdoc")
        .def("loadBatch", &ImageLoader::loadBatch, py::arg("paths"),
             py::arg("options") = LoadOptions{},
             R"pbdoc(
            Load multiple images in batch.

            Args:
                paths: List of file paths
                options: Load options (applied to all)

            Returns:
                BatchLoadResult with all results
            )pbdoc")
        .def("loadAsync", &ImageLoader::loadAsync, py::arg("path"),
             py::arg("options") = LoadOptions{},
             "Start asynchronous image load, returns future")
        .def("canLoad", &ImageLoader::canLoad, py::arg("path"),
             "Check if file can be loaded")
        .def_static("getSupportedFormats", &ImageLoader::getSupportedFormats,
                    "Get list of supported image formats")
        .def("setCacheSize", &ImageLoader::setCacheSize, py::arg("maxEntries"),
             py::arg("maxMemoryMB"), "Set cache size limits")
        .def("clearCache", &ImageLoader::clearCache, "Clear the image cache")
        .def("getCacheStats", &ImageLoader::getCacheStats,
             "Get cache statistics as map")
        .def("registerCustomLoader", &ImageLoader::registerCustomLoader,
             py::arg("format"), py::arg("loader"),
             "Register a custom image loader for a format");

    // CompressionType enum
    py::enum_<CompressionType>(m, "CompressionType",
                               "Compression types for image saving")
        .value("NONE", CompressionType::NONE, "No compression")
        .value("LZW", CompressionType::LZW, "LZW compression")
        .value("DEFLATE", CompressionType::DEFLATE, "Deflate compression")
        .value("PACKBITS", CompressionType::PACKBITS, "PackBits compression")
        .value("JPEG", CompressionType::JPEG, "JPEG compression")
        .value("JPEG2000", CompressionType::JPEG2000, "JPEG2000 compression")
        .value("ZSTD", CompressionType::ZSTD, "Zstandard compression")
        .value("WEBP_LOSSY", CompressionType::WEBP_LOSSY,
               "WebP lossy compression")
        .value("WEBP_LOSSLESS", CompressionType::WEBP_LOSSLESS,
               "WebP lossless compression")
        .value("RLE", CompressionType::RLE, "Run-length encoding")
        .export_values();

    // SaveOptions struct
    py::class_<SaveOptions>(m, "SaveOptions",
                            R"pbdoc(
        Options for image saving operations.

        These options control how images are saved to files or memory,
        including compression settings and format-specific options.
        )pbdoc")
        .def(py::init<>(), "Default constructor with sensible defaults")
        .def_readwrite("targetFormat", &SaveOptions::targetFormat,
                       "Target image format")
        .def_readwrite("quality", &SaveOptions::quality,
                       "Quality for lossy compression (0-100)")
        .def_readwrite("compression", &SaveOptions::compression,
                       "Compression type")
        .def_readwrite("compressionLevel", &SaveOptions::compressionLevel,
                       "Compression level (0-9)")
        .def_readwrite("preserveMetadata", &SaveOptions::preserveMetadata,
                       "Preserve image metadata")
        .def_readwrite("metadata", &SaveOptions::metadata,
                       "Metadata to embed in saved image")
        .def_readwrite("optimize", &SaveOptions::optimize,
                       "Optimize output (may be slower)")
        .def_readwrite("progressive", &SaveOptions::progressive,
                       "Create progressive JPEG/PNG")
        .def_readwrite("interlaced", &SaveOptions::interlaced,
                       "Create interlaced image")
        .def_readwrite("bitDepth", &SaveOptions::bitDepth,
                       "Output bit depth (8, 16, 32)")
        .def_readwrite("colorSpace", &SaveOptions::colorSpace,
                       "Target color space")
        .def_readwrite("embedColorProfile", &SaveOptions::embedColorProfile,
                       "Embed color profile")
        .def_readwrite("stripMetadata", &SaveOptions::stripMetadata,
                       "Strip all metadata")
        .def("__repr__", [](const SaveOptions& self) {
            return "<SaveOptions quality=" + std::to_string(self.quality) +
                   " optimize=" +
                   std::string(self.optimize ? "True" : "False") + ">";
        });

    // SaveResult struct
    py::class_<SaveResult>(m, "SaveResult",
                           R"pbdoc(
        Result of an image saving operation.

        Contains status information about the save operation.
        )pbdoc")
        .def(py::init<>())
        .def_readwrite("success", &SaveResult::success,
                       "Whether the save operation succeeded")
        .def_readwrite("bytesWritten", &SaveResult::bytesWritten,
                       "Number of bytes written")
        .def_readwrite("errorMessage", &SaveResult::errorMessage,
                       "Error message if saving failed")
        .def_readwrite("errorCode", &SaveResult::errorCode, "Error code")
        .def_readwrite("saveTimeMs", &SaveResult::saveTimeMs,
                       "Time taken to save in milliseconds")
        .def_readwrite("outputPath", &SaveResult::outputPath,
                       "Output file path")
        .def_readwrite("outputFormat", &SaveResult::outputFormat,
                       "Actual output format used")
        .def("__bool__", [](const SaveResult& self) { return self.success; })
        .def("__repr__", [](const SaveResult& self) {
            return "<SaveResult success=" +
                   std::string(self.success ? "True" : "False") +
                   " bytes=" + std::to_string(self.bytesWritten) + ">";
        });

    // BatchSaveResult struct
    py::class_<BatchSaveResult>(m, "BatchSaveResult",
                                "Result of batch image saving operation")
        .def(py::init<>())
        .def_readwrite("results", &BatchSaveResult::results,
                       "Individual save results")
        .def_readwrite("successCount", &BatchSaveResult::successCount,
                       "Number of successful saves")
        .def_readwrite("failureCount", &BatchSaveResult::failureCount,
                       "Number of failed saves")
        .def_readwrite("totalTimeMs", &BatchSaveResult::totalTimeMs,
                       "Total time taken in milliseconds")
        .def_readwrite("totalBytesWritten", &BatchSaveResult::totalBytesWritten,
                       "Total bytes written")
        .def("__len__",
             [](const BatchSaveResult& self) { return self.results.size(); })
        .def("__repr__", [](const BatchSaveResult& self) {
            return "<BatchSaveResult success=" +
                   std::to_string(self.successCount) +
                   " failed=" + std::to_string(self.failureCount) + ">";
        });

    // ImageSaver class
    py::class_<ImageSaver>(m, "ImageSaver",
                           R"pbdoc(
        High-performance image saver with format conversion.

        The ImageSaver class provides efficient image saving to files
        and memory buffers with various format and compression options.

        Example:
            >>> saver = ImageSaver()
            >>> options = SaveOptions()
            >>> options.quality = 90
            >>> result = saver.saveToFile("output.jpg", image, options)
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def("saveToFile", &ImageSaver::saveToFile, py::arg("path"),
             py::arg("image"), py::arg("options") = SaveOptions{},
             R"pbdoc(
            Save image to file.

            Args:
                path: Output file path
                image: Image data to save
                options: Save options

            Returns:
                SaveResult with status information
            )pbdoc")
        .def("saveToMemory", &ImageSaver::saveToMemory, py::arg("image"),
             py::arg("options") = SaveOptions{},
             R"pbdoc(
            Save image to memory buffer.

            Args:
                image: Image data to save
                options: Save options

            Returns:
                Pair of (data vector, SaveResult)
            )pbdoc")
        .def("saveBatch", &ImageSaver::saveBatch, py::arg("paths"),
             py::arg("images"), py::arg("options") = SaveOptions{},
             R"pbdoc(
            Save multiple images in batch.

            Args:
                paths: List of output file paths
                images: List of images to save
                options: Save options (applied to all)

            Returns:
                BatchSaveResult with all results
            )pbdoc")
        .def("saveAsync", &ImageSaver::saveAsync, py::arg("path"),
             py::arg("image"), py::arg("options") = SaveOptions{},
             "Start asynchronous image save, returns future")
        .def("canSave", &ImageSaver::canSave, py::arg("format"),
             "Check if format is supported for saving")
        .def_static("getSupportedFormats", &ImageSaver::getSupportedFormats,
                    "Get list of supported output formats")
        .def("getOptimalFormat", &ImageSaver::getOptimalFormat,
             py::arg("image"), py::arg("useCase") = "general",
             "Get optimal format for image and use case")
        .def("estimateOutputSize", &ImageSaver::estimateOutputSize,
             py::arg("image"), py::arg("options"),
             "Estimate output file size in bytes")
        .def("registerCustomSaver", &ImageSaver::registerCustomSaver,
             py::arg("format"), py::arg("saver"),
             "Register a custom image saver for a format");

    // DetectionConfidence enum
    py::enum_<DetectionConfidence>(m, "DetectionConfidence",
                                   "Confidence level for format detection")
        .value("NONE", DetectionConfidence::NONE, "No detection")
        .value("LOW", DetectionConfidence::LOW, "Low confidence")
        .value("MEDIUM", DetectionConfidence::MEDIUM, "Medium confidence")
        .value("HIGH", DetectionConfidence::HIGH, "High confidence")
        .value("CERTAIN", DetectionConfidence::CERTAIN, "Certain detection")
        .export_values();

    // FormatDetectionResult
    py::class_<FormatDetectionResult>(m, "FormatDetectionResult",
                                      "Result of format detection operation")
        .def(py::init<>())
        .def_readwrite("format", &FormatDetectionResult::format,
                       "Detected format")
        .def_readwrite("confidence", &FormatDetectionResult::confidence,
                       "Detection confidence level")
        .def_readwrite("confidenceScore",
                       &FormatDetectionResult::confidenceScore,
                       "Detection confidence score (0-1)")
        .def_readwrite("mimeType", &FormatDetectionResult::mimeType,
                       "MIME type string")
        .def_readwrite("extension", &FormatDetectionResult::extension,
                       "Recommended file extension")
        .def_readwrite("description", &FormatDetectionResult::description,
                       "Format description")
        .def_readwrite("canRead", &FormatDetectionResult::canRead,
                       "Can be read by this library")
        .def_readwrite("canWrite", &FormatDetectionResult::canWrite,
                       "Can be written by this library")
        .def("__repr__", [](const FormatDetectionResult& self) {
            return "<FormatDetectionResult format=" +
                   std::to_string(static_cast<int>(self.format)) +
                   " confidence=" +
                   std::to_string(static_cast<int>(self.confidence)) + ">";
        });

    // MagicSignature struct
    py::class_<MagicSignature>(m, "MagicSignature",
                               "Magic bytes signature for format detection")
        .def(py::init<>())
        .def_readwrite("bytes", &MagicSignature::bytes, "Magic byte sequence")
        .def_readwrite("offset", &MagicSignature::offset,
                       "Offset from file start")
        .def_readwrite("mask", &MagicSignature::mask,
                       "Mask for partial matching");

    // FormatDetector class
    py::class_<FormatDetector>(m, "FormatDetector",
                               R"pbdoc(
        Automatic image format detection.

        Detects image format from file headers, file extensions, and content
        analysis. Supports both common formats and specialized formats.

        Example:
            >>> detector = FormatDetector()
            >>> result = detector.detectFromFile("image.unknown")
            >>> print(f"Format: {result.format}, Confidence: {result.confidence}")
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def("detectFromFile", &FormatDetector::detectFromFile, py::arg("path"),
             R"pbdoc(
            Detect format from file.

            Args:
                path: Path to the file

            Returns:
                FormatDetectionResult with detected format
            )pbdoc")
        .def("detectFromMemory", &FormatDetector::detectFromMemory,
             py::arg("data"), py::arg("size"),
             R"pbdoc(
            Detect format from memory buffer.

            Args:
                data: File content pointer
                size: Data size

            Returns:
                FormatDetectionResult with detected format
            )pbdoc")
        .def("detectFromBlob", &FormatDetector::detectFromBlob, py::arg("blob"),
             "Detect format from Blob object")
        .def("detectFromExtension", &FormatDetector::detectFromExtension,
             py::arg("extension"), "Get format from file extension")
        .def("getMimeType", &FormatDetector::getMimeType, py::arg("format"),
             "Get MIME type for format")
        .def("getExtensions", &FormatDetector::getExtensions, py::arg("format"),
             "Get possible file extensions for format")
        .def("isReadSupported", &FormatDetector::isReadSupported,
             py::arg("format"), "Check if format is supported for reading")
        .def("isWriteSupported", &FormatDetector::isWriteSupported,
             py::arg("format"), "Check if format is supported for writing")
        .def("registerCustomFormat", &FormatDetector::registerCustomFormat,
             py::arg("format"), py::arg("signatures"), py::arg("extensions"),
             py::arg("mimeType"), "Register custom format signature")
        .def_static("getFormatName", &FormatDetector::getFormatName,
                    py::arg("format"), "Get human-readable format name")
        .def_static("getSupportedFormats", &FormatDetector::getSupportedFormats,
                    "Get list of all supported formats");
}
