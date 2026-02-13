/**
 * @file core.cpp
 * @brief Python bindings for core image data structures
 */

#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/image/core/exceptions.hpp"
#include "atom/image/core/image_blob.hpp"
#include "atom/image/core/image_metadata.hpp"

#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/core.hpp>
#endif

namespace py = pybind11;
using namespace atom::image;

/**
 * @brief Convert Blob to NumPy array
 */
template <typename T, BlobMode Mode>
py::array blob_to_numpy(const Blob<T, Mode>& blob) {
    auto rows = blob.getRows();
    auto cols = blob.getCols();
    auto channels = blob.getChannels();

    std::vector<ssize_t> shape;
    if (channels == 1) {
        shape = {rows, cols};
    } else {
        shape = {rows, cols, channels};
    }

    std::vector<ssize_t> strides;
    if (channels == 1) {
        strides = {static_cast<ssize_t>(cols), 1};
    } else {
        strides = {static_cast<ssize_t>(cols * channels),
                   static_cast<ssize_t>(channels), 1};
    }

    auto arr = py::array_t<uint8_t>(shape, strides);
    auto buf = arr.request();
    std::memcpy(buf.ptr, blob.data(), blob.size());

    return arr;
}

/**
 * @brief Create Blob from NumPy array
 */
template <typename T, BlobMode Mode>
Blob<T, Mode> numpy_to_blob(py::array_t<uint8_t> arr) {
    auto buf = arr.request();

    int rows = 1, cols = 1, channels = 1;

    if (buf.ndim == 2) {
        rows = static_cast<int>(buf.shape[0]);
        cols = static_cast<int>(buf.shape[1]);
        channels = 1;
    } else if (buf.ndim == 3) {
        rows = static_cast<int>(buf.shape[0]);
        cols = static_cast<int>(buf.shape[1]);
        channels = static_cast<int>(buf.shape[2]);
    } else {
        throw std::runtime_error("Array must be 2D or 3D");
    }

    return Blob<T, Mode>(buf.ptr, buf.size, rows, cols, channels);
}

/**
 * @brief Bind Blob class template for normal mode
 */
void bind_blob_normal(py::module& m) {
    using BlobType = blob;

    py::class_<BlobType>(m, "Blob", py::buffer_protocol(),
                         R"pbdoc(
        Image data container with flexible memory management.

        The Blob class provides a unified interface for image data storage
        with support for multiple backends (OpenCV, CImg, stb_image) and
        memory management modes (normal/owned vs fast/view-only).

        Attributes:
            rows: Number of rows (height) in the image
            cols: Number of columns (width) in the image
            channels: Number of color channels
            depth: Bit depth of the image
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def(py::init<void*, size_t, int, int, int, int>(), py::arg("ptr"),
             py::arg("size"), py::arg("rows"), py::arg("cols"),
             py::arg("channels") = 1, py::arg("depth") = DEFAULT_DEPTH,
             "Construct from raw data with dimensions")
#if __has_include(<opencv2/core.hpp>)
        .def_static(
            "load",
            [](const std::string& filename) {
                return BlobType::load(filename);
            },
            py::arg("filename"), "Load image from file")
        .def("save", &BlobType::save, py::arg("filename"), "Save image to file")
        .def("resize", &BlobType::resize, py::arg("new_rows"),
             py::arg("new_cols"), "Resize image to new dimensions")
        .def("rotate", &BlobType::rotate, py::arg("angle"),
             "Rotate image by angle in degrees")
        .def("flip", &BlobType::flip, py::arg("flip_code"),
             "Flip image (0=vertical, 1=horizontal, -1=both)")
        .def("convert_color", &BlobType::convert_color, py::arg("code"),
             "Convert color space")
        .def("apply_filter", &BlobType::apply_filter, py::arg("kernel"),
             "Apply convolution filter")
        .def(
            "to_mat", [](const BlobType& self) { return self.to_mat(); },
            "Convert to OpenCV Mat")
        .def("split_channels", &BlobType::split_channels,
             "Split image into separate channel blobs")
        .def_static("merge_channels", &BlobType::merge_channels,
                    py::arg("channel_blobs"), "Merge channel blobs into one")
#endif
        .def("getRows", &BlobType::getRows, "Get number of rows")
        .def("getCols", &BlobType::getCols, "Get number of columns")
        .def("getChannels", &BlobType::getChannels, "Get number of channels")
        .def("getDepth", &BlobType::getDepth, "Get bit depth")
        .def("getWidth", &BlobType::getWidth, "Get image width")
        .def("getHeight", &BlobType::getHeight, "Get image height")
        .def("size", &BlobType::size, "Get total size in bytes")
        .def("isEmpty", &BlobType::isEmpty, "Check if blob is empty")
        .def("getPixelSize", &BlobType::getPixelSize, "Get size of one pixel")
        .def("getImageSize", &BlobType::getImageSize, "Get total image size")
        .def(
            "data",
            [](const BlobType& self) {
                return py::memoryview::from_memory(
                    const_cast<std::byte*>(self.data()), self.size());
            },
            "Get raw data as memoryview")
        .def("clone", &BlobType::clone, "Create a deep copy")
        .def("deallocate", &BlobType::deallocate, "Deallocate memory")
        .def("allocate", &BlobType::allocate, py::arg("size"),
             "Allocate memory")
        .def("fill", &BlobType::fill, py::arg("value"), "Fill with value")
        .def(
            "slice",
            [](const BlobType& self, size_t offset, size_t length) {
                return self.slice(offset, length);
            },
            py::arg("offset"), py::arg("length"), "Get a slice of the blob")
        .def(
            "crop",
            [](const BlobType& self, int x, int y, int width, int height) {
                return self.crop(x, y, width, height);
            },
            py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"),
            "Crop image to rectangle")
        .def(
            "append",
            static_cast<void (BlobType::*)(const BlobType&)>(&BlobType::append),
            py::arg("other"), "Append another blob")
        .def("xorWith", &BlobType::xorWith, py::arg("other"),
             "XOR with another blob")
        .def("compress", &BlobType::compress, "Compress blob data")
        .def("decompress", &BlobType::decompress, "Decompress blob data")
        .def("serialize", &BlobType::serialize, "Serialize to bytes")
        .def_static("deserialize", &BlobType::deserialize, py::arg("data"),
                    "Deserialize from bytes")
        .def("alignMemory", &BlobType::alignMemory, py::arg("alignment") = 64,
             "Align memory for performance")
        .def(
            "at",
            [](const BlobType& self, int y, int x, int channel) {
                return self.at(y, x, channel);
            },
            py::arg("y"), py::arg("x"), py::arg("channel") = 0,
            "Get pixel value at position")
        .def(
            "to_numpy",
            [](const BlobType& self) {
                return blob_to_numpy<std::byte, BlobMode::NORMAL>(self);
            },
            "Convert to NumPy array")
        .def_static(
            "from_numpy",
            [](py::array_t<uint8_t> arr) {
                return numpy_to_blob<std::byte, BlobMode::NORMAL>(arr);
            },
            py::arg("array"), "Create from NumPy array")
        .def(py::self == py::self, "Equality comparison")
        .def("__len__", &BlobType::size, "Get size in bytes")
        .def(
            "__bool__", [](const BlobType& self) { return !self.isEmpty(); },
            "Check if not empty")
        .def(
            "__getitem__",
            [](const BlobType& self, size_t idx) { return self[idx]; },
            py::arg("idx"), "Get byte at index")
        .def("__repr__", [](const BlobType& self) {
            return "<Blob rows=" + std::to_string(self.getRows()) +
                   " cols=" + std::to_string(self.getCols()) +
                   " channels=" + std::to_string(self.getChannels()) +
                   " size=" + std::to_string(self.size()) + ">";
        });
}

void bind_core(py::module& m) {
    // BlobMode enum
    py::enum_<BlobMode>(m, "BlobMode",
                        R"pbdoc(
        Memory management mode for Blob objects.

        NORMAL: Owns the data (copies on construction)
        FAST: View-only mode (no ownership, references external data)
        )pbdoc")
        .value("NORMAL", BlobMode::NORMAL, "Normal mode with data ownership")
        .value("FAST", BlobMode::FAST, "Fast mode without data ownership")
        .export_values();

    // Bind Blob class
    bind_blob_normal(m);

    // Type aliases for convenience
    m.attr("blob") = m.attr("Blob");

    // Register exceptions
    py::register_exception<core::ImageException>(m, "ImageException");
    py::register_exception<core::FormatException>(m, "FormatException");
    py::register_exception<core::LoadException>(m, "LoadException");
    py::register_exception<core::SaveException>(m, "SaveException");
    py::register_exception<core::ProcessingException>(m, "ProcessingException");
    py::register_exception<core::MemoryException>(m, "MemoryException");
    py::register_exception<core::ValidationException>(m, "ValidationException");
    py::register_exception<core::IOException>(m, "IOException");
    py::register_exception<core::CodecException>(m, "CodecException");
    py::register_exception<core::OutOfBoundsException>(m,
                                                       "OutOfBoundsException");
    py::register_exception<core::UnsupportedOperationException>(
        m, "UnsupportedOperationException");
    py::register_exception<core::TimeoutException>(m, "TimeoutException");
    py::register_exception<core::ConfigurationException>(
        m, "ConfigurationException");

    // ImageMetadata class from core namespace
    py::class_<core::ImageMetadata>(m, "ImageMetadata",
                                    R"pbdoc(
        Container for image metadata.

        Stores key-value pairs of metadata associated with an image,
        supporting various value types including strings, integers,
        doubles, booleans, and time points.
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def(
            "set_string",
            [](core::ImageMetadata& self, const std::string& key,
               const std::string& value) { self.set(key, value); },
            py::arg("key"), py::arg("value"), "Set a string metadata value")
        .def(
            "set_int",
            [](core::ImageMetadata& self, const std::string& key, int value) {
                self.set(key, value);
            },
            py::arg("key"), py::arg("value"), "Set an integer metadata value")
        .def(
            "set_double",
            [](core::ImageMetadata& self, const std::string& key,
               double value) { self.set(key, value); },
            py::arg("key"), py::arg("value"), "Set a double metadata value")
        .def(
            "set_bool",
            [](core::ImageMetadata& self, const std::string& key, bool value) {
                self.set(key, value);
            },
            py::arg("key"), py::arg("value"), "Set a boolean metadata value")
        .def("getString", &core::ImageMetadata::getString, py::arg("key"),
             py::arg("default_value") = "", "Get string value")
        .def("getInt", &core::ImageMetadata::getInt, py::arg("key"),
             py::arg("default_value") = 0, "Get integer value")
        .def("getDouble", &core::ImageMetadata::getDouble, py::arg("key"),
             py::arg("default_value") = 0.0, "Get double value")
        .def("getBool", &core::ImageMetadata::getBool, py::arg("key"),
             py::arg("default_value") = false, "Get boolean value")
        .def("has", &core::ImageMetadata::has, py::arg("key"),
             "Check if key exists")
        .def("remove", &core::ImageMetadata::remove, py::arg("key"),
             "Remove a key")
        .def("clear", &core::ImageMetadata::clear, "Clear all metadata")
        .def("getKeys", &core::ImageMetadata::getKeys, "Get all keys")
        .def("size", &core::ImageMetadata::size, "Get number of entries")
        .def("empty", &core::ImageMetadata::empty, "Check if empty")
        .def("merge", &core::ImageMetadata::merge, py::arg("other"),
             py::arg("overwrite") = false, "Merge with another metadata object")
        .def("toString", &core::ImageMetadata::toString,
             "Convert to string representation")
        .def("fromString", &core::ImageMetadata::fromString, py::arg("str"),
             "Parse from string representation")
        .def("__len__", &core::ImageMetadata::size, "Get number of entries")
        .def("__contains__", &core::ImageMetadata::has, "Check if key exists")
        .def("__repr__", [](const core::ImageMetadata& self) {
            return "<ImageMetadata entries=" + std::to_string(self.size()) +
                   ">";
        });

    // Standard metadata keys namespace
    auto metadata_keys =
        m.def_submodule("MetadataKeys", "Standard metadata key constants");
    metadata_keys.attr("WIDTH") = core::MetadataKeys::WIDTH;
    metadata_keys.attr("HEIGHT") = core::MetadataKeys::HEIGHT;
    metadata_keys.attr("CHANNELS") = core::MetadataKeys::CHANNELS;
    metadata_keys.attr("BIT_DEPTH") = core::MetadataKeys::BIT_DEPTH;
    metadata_keys.attr("COLOR_SPACE") = core::MetadataKeys::COLOR_SPACE;
    metadata_keys.attr("FORMAT") = core::MetadataKeys::FORMAT;
    metadata_keys.attr("CAMERA_MAKE") = core::MetadataKeys::CAMERA_MAKE;
    metadata_keys.attr("CAMERA_MODEL") = core::MetadataKeys::CAMERA_MODEL;
    metadata_keys.attr("FOCAL_LENGTH") = core::MetadataKeys::FOCAL_LENGTH;
    metadata_keys.attr("APERTURE") = core::MetadataKeys::APERTURE;
    metadata_keys.attr("ISO") = core::MetadataKeys::ISO;
    metadata_keys.attr("SHUTTER_SPEED") = core::MetadataKeys::SHUTTER_SPEED;
    metadata_keys.attr("COPYRIGHT") = core::MetadataKeys::COPYRIGHT;
    metadata_keys.attr("ARTIST") = core::MetadataKeys::ARTIST;
    metadata_keys.attr("DATE_TIME") = core::MetadataKeys::DATE_TIME;
    metadata_keys.attr("DATE_TIME_ORIGINAL") =
        core::MetadataKeys::DATE_TIME_ORIGINAL;
    metadata_keys.attr("DATE_TIME_DIGITIZED") =
        core::MetadataKeys::DATE_TIME_DIGITIZED;
    metadata_keys.attr("GPS_LATITUDE") = core::MetadataKeys::GPS_LATITUDE;
    metadata_keys.attr("GPS_LONGITUDE") = core::MetadataKeys::GPS_LONGITUDE;
    metadata_keys.attr("GPS_ALTITUDE") = core::MetadataKeys::GPS_ALTITUDE;
    metadata_keys.attr("FILE_NAME") = core::MetadataKeys::FILE_NAME;
    metadata_keys.attr("FILE_SIZE") = core::MetadataKeys::FILE_SIZE;
    metadata_keys.attr("FILE_MODIFIED") = core::MetadataKeys::FILE_MODIFIED;
    metadata_keys.attr("SOFTWARE") = core::MetadataKeys::SOFTWARE;
    metadata_keys.attr("PROCESSING") = core::MetadataKeys::PROCESSING;
    metadata_keys.attr("COMPRESSION") = core::MetadataKeys::COMPRESSION;
    metadata_keys.attr("QUALITY") = core::MetadataKeys::QUALITY;
    metadata_keys.attr("CREATOR") = core::MetadataKeys::CREATOR;
}
