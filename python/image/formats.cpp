/**
 * @file formats.cpp
 * @brief Python bindings for specialized image formats (FITS, SER)
 */

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/image/formats/advanced_formats.hpp"
#include "atom/image/formats/fits_data.hpp"
#include "atom/image/formats/fits_file.hpp"
#include "atom/image/formats/fits_header.hpp"
#include "atom/image/formats/hdu.hpp"

#ifdef ATOM_IMAGE_HAS_OPENCV
#include "atom/image/formats/ser/frame_processor.h"
#include "atom/image/formats/ser/quality.h"
#include "atom/image/formats/ser/ser.hpp"
#include "atom/image/formats/ser/ser_reader.h"
#include "atom/image/formats/ser/ser_writer.h"
#endif

namespace py = pybind11;

void bind_formats(py::module& m) {
    // FITS error codes
    py::enum_<FITSErrorCode>(m, "FITSErrorCode",
                             "Error codes for FITS file operations")
        .value("SUCCESS", FITSErrorCode::Success, "Operation successful")
        .value("FILE_NOT_EXIST", FITSErrorCode::FileNotExist,
               "File does not exist")
        .value("FILE_NOT_ACCESSIBLE", FITSErrorCode::FileNotAccessible,
               "File not accessible")
        .value("INVALID_FORMAT", FITSErrorCode::InvalidFormat,
               "Invalid FITS format")
        .value("READ_ERROR", FITSErrorCode::ReadError, "Read error")
        .value("WRITE_ERROR", FITSErrorCode::WriteError, "Write error")
        .value("MEMORY_ERROR", FITSErrorCode::MemoryError,
               "Memory allocation error")
        .value("COMPRESSION_ERROR", FITSErrorCode::CompressionError,
               "Compression error")
        .value("CORRUPTED_DATA", FITSErrorCode::CorruptedData, "Corrupted data")
        .value("UNSUPPORTED_FEATURE", FITSErrorCode::UnsupportedFeature,
               "Unsupported feature")
        .value("INTERNAL_ERROR", FITSErrorCode::InternalError, "Internal error")
        .export_values();

    // FITS exception
    py::register_exception<FITSFileException>(m, "FITSFileException",
                                              R"pbdoc(
        Exception class for FITS file operations.

        This exception is raised when FITS file operations fail.
        )pbdoc");

    // HDU (Header Data Unit) class
    py::class_<HDU>(m, "HDU",
                    R"pbdoc(
        FITS Header Data Unit.

        An HDU contains both header information and data. FITS files consist
        of one or more HDUs.
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def("get_header", &HDU::getHeader, "Get HDU header")
        .def("set_header", &HDU::setHeader, py::arg("header"), "Set HDU header")
        .def("get_data", &HDU::getData, "Get HDU data")
        .def("set_data", &HDU::setData, py::arg("data"), "Set HDU data")
        .def("get_type", &HDU::getType, "Get HDU type")
        .def("is_primary", &HDU::isPrimary, "Check if this is the primary HDU")
        .def("get_dimensions", &HDU::getDimensions, "Get data dimensions")
        .def("get_bitpix", &HDU::getBitpix, "Get BITPIX value")
        .def("__repr__", [](const HDU& self) {
            return "<HDU type=" +
                   std::to_string(static_cast<int>(self.getType())) + ">";
        });

    // FITSHeader class
    py::class_<FITSHeader>(m, "FITSHeader",
                           "FITS header with keyword-value pairs")
        .def(py::init<>(), "Default constructor")
        .def("set_keyword", &FITSHeader::setKeyword, py::arg("keyword"),
             py::arg("value"), py::arg("comment") = "", "Set a header keyword")
        .def("get_keyword", &FITSHeader::getKeyword, py::arg("keyword"),
             "Get a header keyword value")
        .def("has_keyword", &FITSHeader::hasKeyword, py::arg("keyword"),
             "Check if keyword exists")
        .def("remove_keyword", &FITSHeader::removeKeyword, py::arg("keyword"),
             "Remove a keyword")
        .def("get_all_keywords", &FITSHeader::getAllKeywords,
             "Get all keywords as dictionary")
        .def("get_comment", &FITSHeader::getComment, py::arg("keyword"),
             "Get comment for a keyword")
        .def("clear", &FITSHeader::clear, "Clear all keywords")
        .def("to_string", &FITSHeader::toString,
             "Convert header to string representation")
        .def("__len__", &FITSHeader::size, "Get number of keywords")
        .def("__contains__", &FITSHeader::hasKeyword, "Check if keyword exists")
        .def("__getitem__", &FITSHeader::getKeyword, "Get keyword value")
        .def("__setitem__",
             [](FITSHeader& self, const std::string& key,
                const std::string& value) { self.setKeyword(key, value); });

    // FITSFile class
    py::class_<FITSFile>(m, "FITSFile",
                         R"pbdoc(
        FITS file handler.

        The FITSFile class provides comprehensive support for reading and writing
        FITS (Flexible Image Transport System) files, commonly used in astronomy.

        Example:
            >>> fits = FITSFile("image.fits")
            >>> hdu_count = fits.get_hdu_count()
            >>> primary_hdu = fits.get_hdu(0)
            >>> data = primary_hdu.get_data()
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def(py::init<const std::string&>(), py::arg("filename"),
             "Construct and read FITS file")
        .def("read_fits", &FITSFile::readFITS, py::arg("filename"),
             "Read FITS file from disk")
        .def("read_fits_async", &FITSFile::readFITSAsync, py::arg("filename"),
             "Read FITS file asynchronously")
        .def("write_fits", &FITSFile::writeFITS, py::arg("filename"),
             "Write FITS file to disk")
        .def("write_fits_async", &FITSFile::writeFITSAsync, py::arg("filename"),
             "Write FITS file asynchronously")
        .def("get_hdu_count", &FITSFile::getHDUCount, "Get number of HDUs")
        .def("is_empty", &FITSFile::isEmpty, "Check if file is empty")
        .def("get_hdu", &FITSFile::getHDU, py::arg("index"), "Get HDU by index")
        .def("add_hdu", &FITSFile::addHDU, py::arg("hdu"), "Add HDU to file")
        .def("remove_hdu", &FITSFile::removeHDU, py::arg("index"),
             "Remove HDU by index")
        .def("get_primary_hdu", &FITSFile::getPrimaryHDU, "Get primary HDU")
        .def("set_primary_hdu", &FITSFile::setPrimaryHDU, py::arg("hdu"),
             "Set primary HDU")
        .def("validate", &FITSFile::validate, "Validate FITS file structure")
        .def("get_info", &FITSFile::getInfo, "Get file information")
        .def("clear", &FITSFile::clear, "Clear all HDUs")
        .def("__len__", &FITSFile::getHDUCount, "Get number of HDUs")
        .def("__getitem__", &FITSFile::getHDU, "Get HDU by index")
        .def("__repr__", [](const FITSFile& self) {
            return "<FITSFile HDUs=" + std::to_string(self.getHDUCount()) + ">";
        });

#ifdef ATOM_IMAGE_HAS_OPENCV
    // SER format bindings
    auto ser_module = m.def_submodule("ser", "SER video format support");

    // SER color ID enum
    py::enum_<serastro::SERColorID>(ser_module, "ColorID",
                                    "Color format identifiers for SER files")
        .value("MONO", serastro::SERColorID::MONO, "Monochrome")
        .value("BAYER_RGGB", serastro::SERColorID::BAYER_RGGB,
               "Bayer RGGB pattern")
        .value("BAYER_GRBG", serastro::SERColorID::BAYER_GRBG,
               "Bayer GRBG pattern")
        .value("BAYER_GBRG", serastro::SERColorID::BAYER_GBRG,
               "Bayer GBRG pattern")
        .value("BAYER_BGGR", serastro::SERColorID::BAYER_BGGR,
               "Bayer BGGR pattern")
        .value("RGB", serastro::SERColorID::RGB, "RGB color")
        .value("BGR", serastro::SERColorID::BGR, "BGR color")
        .export_values();

    // SER header
    py::class_<serastro::SERHeader>(ser_module, "Header",
                                    "SER file header information")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("file_id", &serastro::SERHeader::fileID,
                       "File identifier")
        .def_readwrite("lu_id", &serastro::SERHeader::luID,
                       "Little-endian unique ID")
        .def_readwrite("color_id", &serastro::SERHeader::colorID,
                       "Color format ID")
        .def_readwrite("little_endian", &serastro::SERHeader::littleEndian,
                       "Byte order flag")
        .def_readwrite("image_width", &serastro::SERHeader::imageWidth,
                       "Image width in pixels")
        .def_readwrite("image_height", &serastro::SERHeader::imageHeight,
                       "Image height in pixels")
        .def_readwrite("pixel_depth_per_plane",
                       &serastro::SERHeader::pixelDepthPerPlane,
                       "Bit depth per color plane")
        .def_readwrite("frame_count", &serastro::SERHeader::frameCount,
                       "Total number of frames")
        .def_readwrite("observer", &serastro::SERHeader::observer,
                       "Observer name")
        .def_readwrite("instrument", &serastro::SERHeader::instrument,
                       "Instrument name")
        .def_readwrite("telescope", &serastro::SERHeader::telescope,
                       "Telescope name")
        .def_readwrite("date_time", &serastro::SERHeader::dateTime,
                       "Date and time (UTC)")
        .def_readwrite("date_time_utc", &serastro::SERHeader::dateTimeUTC,
                       "UTC timestamp");

    // ReadOptions
    py::class_<serastro::ReadOptions>(ser_module, "ReadOptions",
                                      "Options for reading SER frames")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("convert_to_float",
                       &serastro::ReadOptions::convertToFloat,
                       "Convert to floating point")
        .def_readwrite("normalize_frame",
                       &serastro::ReadOptions::normalizeFrame,
                       "Normalize values to [0,1]")
        .def_readwrite("apply_bayer_decode",
                       &serastro::ReadOptions::applyBayerDecode,
                       "Apply Bayer pattern decoding")
        .def_readwrite("bayer_method", &serastro::ReadOptions::bayerMethod,
                       "Bayer decoding method")
        .def_readwrite("read_as_grayscale",
                       &serastro::ReadOptions::readAsGrayscale,
                       "Force grayscale reading")
        .def_readwrite("enable_cache", &serastro::ReadOptions::enableCache,
                       "Enable frame caching")
        .def_readwrite("max_cache_size", &serastro::ReadOptions::maxCacheSize,
                       "Maximum cache size in MB")
        .def_readwrite("flip_channels", &serastro::ReadOptions::flipChannels,
                       "Flip BGR to RGB");

    // SERReader
    py::class_<serastro::SERReader>(ser_module, "Reader",
                                    R"pbdoc(
        SER file reader.

        Read SER (Simple Event Recording) video files commonly used in
        astronomical imaging.

        Example:
            >>> reader = ser.Reader("video.ser")
            >>> frame_count = reader.get_frame_count()
            >>> frame = reader.read_frame(0)
        )pbdoc")
        .def(py::init<const std::filesystem::path&>(), py::arg("file_path"),
             "Construct reader for SER file")
        .def("get_header", &serastro::SERReader::getHeader,
             "Get SER file header")
        .def("get_file_path", &serastro::SERReader::getFilePath,
             "Get file path")
        .def("get_frame_count", &serastro::SERReader::getFrameCount,
             "Get total frame count")
        .def("get_width", &serastro::SERReader::getWidth, "Get frame width")
        .def("get_height", &serastro::SERReader::getHeight, "Get frame height")
        .def("get_bit_depth", &serastro::SERReader::getBitDepth,
             "Get bit depth")
        .def("get_color_id", &serastro::SERReader::getColorID,
             "Get color format ID")
        .def("is_color", &serastro::SERReader::isColor,
             "Check if frames are color")
        .def("read_frame", &serastro::SERReader::readFrame,
             py::arg("frame_index"),
             py::arg("options") = serastro::ReadOptions{},
             "Read a single frame")
        .def("read_frames", &serastro::SERReader::readFrames,
             py::arg("frame_indices"),
             py::arg("options") = serastro::ReadOptions{},
             "Read multiple frames")
        .def("read_frame_range", &serastro::SERReader::readFrameRange,
             py::arg("start_frame"), py::arg("end_frame"),
             py::arg("options") = serastro::ReadOptions{},
             "Read a range of frames")
        .def("has_timestamps", &serastro::SERReader::hasTimestamps,
             "Check if file has timestamps")
        .def("get_timestamp", &serastro::SERReader::getTimestamp,
             py::arg("frame_index"), "Get timestamp for a frame")
        .def("get_all_timestamps", &serastro::SERReader::getAllTimestamps,
             "Get all timestamps")
        .def("clear_cache", &serastro::SERReader::clearCache,
             "Clear frame cache");
#endif
}
