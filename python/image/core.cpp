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
template <BlobType T, BlobMode Mode>
py::array blob_to_numpy(const Blob<T, Mode>& blob) {
    auto rows = blob.rows();
    auto cols = blob.cols();
    auto channels = blob.channels();

    std::vector<ssize_t> shape;
    if (channels == 1) {
        shape = {rows, cols};
    } else {
        shape = {rows, cols, channels};
    }

    std::vector<ssize_t> strides;
    if (channels == 1) {
        strides = {cols, 1};
    } else {
        strides = {cols * channels, channels, 1};
    }

    // Create numpy array with copy of data
    auto arr = py::array_t<uint8_t>(shape, strides);
    auto buf = arr.request();
    std::memcpy(buf.ptr, blob.data(), blob.size());

    return arr;
}

/**
 * @brief Create Blob from NumPy array
 */
template <BlobType T, BlobMode Mode>
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
 * @brief Bind Blob class template
 */
template <BlobType T, BlobMode Mode>
void bind_blob_type(py::module& m, const std::string& name) {
    using BlobType = Blob<T, Mode>;

    py::class_<BlobType>(m, name.c_str(), py::buffer_protocol(),
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
        .def_static("load", &BlobType::load, py::arg("filename"),
                    "Load image from file")
        .def("save", &BlobType::save, py::arg("filename"), "Save image to file")
        .def("resize", &BlobType::resize, py::arg("new_width"),
             py::arg("new_height"), "Resize image to new dimensions")
        .def("rotate", &BlobType::rotate, py::arg("angle"),
             "Rotate image by angle in degrees")
        .def("flip", &BlobType::flip, py::arg("flip_code"),
             "Flip image (0=vertical, 1=horizontal, -1=both)")
        .def("rows", &BlobType::rows, "Get number of rows")
        .def("cols", &BlobType::cols, "Get number of columns")
        .def("channels", &BlobType::channels, "Get number of channels")
        .def("depth", &BlobType::depth, "Get bit depth")
        .def("size", &BlobType::size, "Get total size in bytes")
        .def("empty", &BlobType::empty, "Check if blob is empty")
        .def(
            "data",
            [](const BlobType& self) {
                return py::memoryview::from_memory(
                    const_cast<std::byte*>(self.data()), self.size());
            },
            "Get raw data as memoryview")
        .def("clone", &BlobType::clone, "Create a deep copy")
        .def("clear", &BlobType::clear, "Clear all data")
        .def("serialize", &BlobType::serialize, "Serialize to bytes")
        .def_static("deserialize", &BlobType::deserialize, py::arg("data"),
                    "Deserialize from bytes")
        .def("to_numpy", &blob_to_numpy<T, Mode>, "Convert to NumPy array")
        .def_static("from_numpy", &numpy_to_blob<T, Mode>, py::arg("array"),
                    "Create from NumPy array")
        .def(py::self == py::self, "Equality comparison")
        .def(py::self != py::self, "Inequality comparison")
        .def("__len__", &BlobType::size, "Get size in bytes")
        .def(
            "__bool__", [](const BlobType& self) { return !self.empty(); },
            "Check if not empty")
        .def("__repr__", [](const BlobType& self) {
            return "<Blob rows=" + std::to_string(self.rows()) +
                   " cols=" + std::to_string(self.cols()) +
                   " channels=" + std::to_string(self.channels()) + ">";
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

    // Bind common Blob instantiations
    bind_blob_type<std::byte, BlobMode::NORMAL>(m, "Blob");
    bind_blob_type<std::byte, BlobMode::FAST>(m, "FastBlob");
    bind_blob_type<const std::byte, BlobMode::NORMAL>(m, "ConstBlob");

    // Type aliases for convenience
    m.attr("blob") = m.attr("Blob");
    m.attr("fast_blob") = m.attr("FastBlob");
    m.attr("const_blob") = m.attr("ConstBlob");

    // ImageMetadata class
    py::class_<ImageMetadata>(m, "ImageMetadata",
                              "Container for image metadata including "
                              "dimensions, format, and custom tags")
        .def(py::init<>(), "Default constructor")
        .def(py::init<int, int, int>(), py::arg("width"), py::arg("height"),
             py::arg("channels"), "Construct with basic image properties")
        .def_readwrite("width", &ImageMetadata::width, "Image width in pixels")
        .def_readwrite("height", &ImageMetadata::height,
                       "Image height in pixels")
        .def_readwrite("channels", &ImageMetadata::channels,
                       "Number of color channels")
        .def_readwrite("bit_depth", &ImageMetadata::bitDepth,
                       "Bit depth per channel")
        .def_readwrite("color_space", &ImageMetadata::colorSpace,
                       "Color space name")
        .def_readwrite("format", &ImageMetadata::format, "Image format")
        .def("set_tag", &ImageMetadata::setTag, py::arg("key"),
             py::arg("value"), "Set a custom metadata tag")
        .def("get_tag", &ImageMetadata::getTag, py::arg("key"),
             "Get a custom metadata tag value")
        .def("has_tag", &ImageMetadata::hasTag, py::arg("key"),
             "Check if a tag exists")
        .def("remove_tag", &ImageMetadata::removeTag, py::arg("key"),
             "Remove a metadata tag")
        .def("get_all_tags", &ImageMetadata::getAllTags,
             "Get all custom tags as dictionary")
        .def("clear_tags", &ImageMetadata::clearTags, "Clear all custom tags")
        .def("merge", &ImageMetadata::merge, py::arg("other"),
             "Merge metadata from another ImageMetadata object")
        .def("to_json", &ImageMetadata::toJson,
             "Convert metadata to JSON string")
        .def_static("from_json", &ImageMetadata::fromJson, py::arg("json"),
                    "Create metadata from JSON string")
        .def("__repr__", [](const ImageMetadata& self) {
            return "<ImageMetadata " + std::to_string(self.width) + "x" +
                   std::to_string(self.height) +
                   " channels=" + std::to_string(self.channels) + ">";
        });

    // Standard metadata keys namespace
    auto metadata_keys =
        m.def_submodule("MetadataKeys", "Standard metadata key constants");
    metadata_keys.attr("WIDTH") = MetadataKeys::WIDTH;
    metadata_keys.attr("HEIGHT") = MetadataKeys::HEIGHT;
    metadata_keys.attr("CHANNELS") = MetadataKeys::CHANNELS;
    metadata_keys.attr("BIT_DEPTH") = MetadataKeys::BIT_DEPTH;
    metadata_keys.attr("COLOR_SPACE") = MetadataKeys::COLOR_SPACE;
    metadata_keys.attr("FORMAT") = MetadataKeys::FORMAT;
    metadata_keys.attr("CAMERA_MAKE") = MetadataKeys::CAMERA_MAKE;
    metadata_keys.attr("CAMERA_MODEL") = MetadataKeys::CAMERA_MODEL;
    metadata_keys.attr("LENS_MODEL") = MetadataKeys::LENS_MODEL;
    metadata_keys.attr("FOCAL_LENGTH") = MetadataKeys::FOCAL_LENGTH;
    metadata_keys.attr("APERTURE") = MetadataKeys::APERTURE;
    metadata_keys.attr("ISO") = MetadataKeys::ISO;
    metadata_keys.attr("SHUTTER_SPEED") = MetadataKeys::SHUTTER_SPEED;
    metadata_keys.attr("COPYRIGHT") = MetadataKeys::COPYRIGHT;
    metadata_keys.attr("ARTIST") = MetadataKeys::ARTIST;
}
