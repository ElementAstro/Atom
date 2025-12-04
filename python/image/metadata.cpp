/**
 * @file metadata.cpp
 * @brief Python bindings for image metadata handling
 */

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/image/metadata/exif.hpp"

namespace py = pybind11;
using namespace atom::image;

void bind_metadata(py::module& m) {
    // EXIF exception
    py::register_exception<ExifException>(m, "ExifException",
                                          R"pbdoc(
        Exception class for EXIF parsing errors.

        This exception is raised when EXIF data parsing or manipulation fails.
        )pbdoc");

    // GpsCoordinate struct
    py::class_<GpsCoordinate>(m, "GpsCoordinate",
                              R"pbdoc(
        GPS coordinate representation.

        Stores GPS coordinates in degrees, minutes, seconds format with
        direction indicator (N/S for latitude, E/W for longitude).
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def(py::init<double, double, double, char>(), py::arg("degrees"),
             py::arg("minutes"), py::arg("seconds"), py::arg("direction"),
             "Construct with DMS values")
        .def_readwrite("degrees", &GpsCoordinate::degrees, "Degrees component")
        .def_readwrite("minutes", &GpsCoordinate::minutes, "Minutes component")
        .def_readwrite("seconds", &GpsCoordinate::seconds, "Seconds component")
        .def_readwrite("direction", &GpsCoordinate::direction,
                       "Direction (N/S/E/W)")
        .def("to_decimal_degrees", &GpsCoordinate::toDecimalDegrees,
             R"pbdoc(
            Convert to decimal degrees.

            Returns:
                Decimal degree representation (negative for S/W)
            )pbdoc")
        .def_static("from_decimal_degrees", &GpsCoordinate::fromDecimalDegrees,
                    py::arg("decimal"), py::arg("is_latitude"),
                    R"pbdoc(
            Create from decimal degrees.

            Args:
                decimal: Decimal degree value
                is_latitude: True for latitude, False for longitude

            Returns:
                GpsCoordinate instance
            )pbdoc")
        .def("to_string", &GpsCoordinate::toString,
             "Convert to string representation")
        .def("__str__", &GpsCoordinate::toString)
        .def("__repr__", [](const GpsCoordinate& self) {
            return "<GpsCoordinate " + self.toString() + ">";
        });

    // ExifData struct
    py::class_<ExifData>(m, "ExifData",
                         R"pbdoc(
        EXIF metadata container.

        Holds all EXIF (Exchangeable Image File Format) metadata extracted
        from an image file, including camera settings, GPS coordinates,
        timestamps, and more.

        All fields are optional and will be None if not present in the image.
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("camera_make", &ExifData::cameraMake,
                       "Camera manufacturer")
        .def_readwrite("camera_model", &ExifData::cameraModel, "Camera model")
        .def_readwrite("date_time", &ExifData::dateTime,
                       "Date and time of image capture")
        .def_readwrite("exposure_time", &ExifData::exposureTime,
                       "Exposure time in seconds")
        .def_readwrite("f_number", &ExifData::fNumber, "F-number (aperture)")
        .def_readwrite("iso_speed", &ExifData::isoSpeed, "ISO speed rating")
        .def_readwrite("focal_length", &ExifData::focalLength,
                       "Focal length in millimeters")
        .def_readwrite("gps_latitude", &ExifData::gpsLatitude,
                       "GPS latitude coordinate")
        .def_readwrite("gps_longitude", &ExifData::gpsLongitude,
                       "GPS longitude coordinate")
        .def_readwrite("orientation", &ExifData::orientation,
                       "Image orientation (1-8)")
        .def_readwrite("compression", &ExifData::compression,
                       "Compression method")
        .def_readwrite("image_width", &ExifData::imageWidth,
                       "Image width in pixels")
        .def_readwrite("image_height", &ExifData::imageHeight,
                       "Image height in pixels")
        .def_readwrite("color_space", &ExifData::colorSpace, "Color space")
        .def_readwrite("software", &ExifData::software,
                       "Software used to create image")
        .def("get_date_time_string", &ExifData::getDateTimeString,
             R"pbdoc(
            Get date/time as ISO 8601 string.

            Returns:
                ISO 8601 formatted string or None
            )pbdoc")
        .def("set_date_time_from_string", &ExifData::setDateTimeFromString,
             py::arg("date_time_str"),
             R"pbdoc(
            Set date/time from string.

            Args:
                date_time_str: Date/time string (EXIF format or ISO 8601)

            Returns:
                True if parsing succeeded
            )pbdoc")
        .def("__repr__", [](const ExifData& self) {
            std::string repr = "<ExifData";
            if (self.cameraMake)
                repr += " make=" + *self.cameraMake;
            if (self.cameraModel)
                repr += " model=" + *self.cameraModel;
            if (self.imageWidth && self.imageHeight) {
                repr += " size=" + std::to_string(*self.imageWidth) + "x" +
                        std::to_string(*self.imageHeight);
            }
            repr += ">";
            return repr;
        });

    // ExifParser class
    py::class_<ExifParser>(m, "ExifParser",
                           R"pbdoc(
        EXIF data parser.

        Parses EXIF metadata from image files. Supports common image formats
        including JPEG, TIFF, and others that embed EXIF data.

        Example:
            >>> parser = ExifParser("photo.jpg")
            >>> if parser.parse():
            >>>     exif = parser.get_exif_data()
            >>>     print(f"Camera: {exif.camera_make} {exif.camera_model}")
            >>>     print(f"ISO: {exif.iso_speed}")
        )pbdoc")
        .def(py::init<std::string_view>(), py::arg("filename"),
             "Construct parser for specified file")
        .def("parse", &ExifParser::parse,
             R"pbdoc(
            Parse EXIF data from file.

            Returns:
                True if parsing was successful
            )pbdoc")
        .def("get_exif_data", &ExifParser::getExifData,
             py::return_value_policy::reference_internal,
             R"pbdoc(
            Get parsed EXIF data.

            Returns:
                Reference to ExifData structure
            )pbdoc")
        .def("optimize", &ExifParser::optimize,
             "Optimize memory usage of EXIF data")
        .def("validate_data", &ExifParser::validateData,
             R"pbdoc(
            Validate data integrity.

            Returns:
                True if data is valid
            )pbdoc")
        .def("clone", &ExifParser::clone,
             R"pbdoc(
            Clone the parser instance.

            Returns:
                New ExifParser instance with same data
            )pbdoc")
        .def("serialize", &ExifParser::serialize,
             R"pbdoc(
            Serialize EXIF data to string.

            Returns:
                Serialized string representation
            )pbdoc")
        .def_static("deserialize", &ExifParser::deserialize, py::arg("data"),
                    R"pbdoc(
            Deserialize EXIF data from string.

            Args:
                data: Serialized data string

            Returns:
                New ExifParser instance
            )pbdoc")
        .def("__repr__", [](const ExifParser& self) { return "<ExifParser>"; });

    // Convenience functions
    m.def(
        "extract_exif",
        [](const std::string& filename) {
            ExifParser parser(filename);
            if (parser.parse()) {
                return py::cast(parser.getExifData());
            }
            return py::none();
        },
        py::arg("filename"),
        R"pbdoc(
        Extract EXIF data from image file.

        Convenience function that creates a parser, parses the file,
        and returns the EXIF data.

        Args:
            filename: Path to image file

        Returns:
            ExifData object or None if parsing failed

        Example:
            >>> exif = extract_exif("photo.jpg")
            >>> if exif:
            >>>     print(f"Taken with {exif.camera_make} {exif.camera_model}")
        )pbdoc");

    m.def(
        "has_exif",
        [](const std::string& filename) {
            ExifParser parser(filename);
            return parser.parse();
        },
        py::arg("filename"),
        R"pbdoc(
        Check if file contains EXIF data.

        Args:
            filename: Path to image file

        Returns:
            True if file contains valid EXIF data
        )pbdoc");

    m.def(
        "get_gps_coordinates",
        [](const std::string& filename) -> py::object {
            ExifParser parser(filename);
            if (parser.parse()) {
                const auto& exif = parser.getExifData();
                if (exif.gpsLatitude && exif.gpsLongitude) {
                    return py::make_tuple(
                        exif.gpsLatitude->toDecimalDegrees(),
                        exif.gpsLongitude->toDecimalDegrees());
                }
            }
            return py::none();
        },
        py::arg("filename"),
        R"pbdoc(
        Extract GPS coordinates from image.

        Args:
            filename: Path to image file

        Returns:
            Tuple of (latitude, longitude) in decimal degrees, or None

        Example:
            >>> coords = get_gps_coordinates("photo.jpg")
            >>> if coords:
            >>>     lat, lon = coords
            >>>     print(f"Location: {lat}, {lon}")
        )pbdoc");

    m.def(
        "get_camera_info",
        [](const std::string& filename) -> py::object {
            ExifParser parser(filename);
            if (parser.parse()) {
                const auto& exif = parser.getExifData();
                py::dict info;
                if (exif.cameraMake)
                    info["make"] = *exif.cameraMake;
                if (exif.cameraModel)
                    info["model"] = *exif.cameraModel;
                if (exif.lensModel)
                    info["lens"] = *exif.lensModel;
                if (exif.focalLength)
                    info["focal_length"] = *exif.focalLength;
                if (exif.fNumber)
                    info["aperture"] = *exif.fNumber;
                if (exif.isoSpeed)
                    info["iso"] = *exif.isoSpeed;
                if (exif.exposureTime)
                    info["exposure_time"] = *exif.exposureTime;
                return info;
            }
            return py::none();
        },
        py::arg("filename"),
        R"pbdoc(
        Extract camera information from image.

        Args:
            filename: Path to image file

        Returns:
            Dictionary with camera information, or None

        Example:
            >>> info = get_camera_info("photo.jpg")
            >>> if info:
            >>>     print(f"Camera: {info['make']} {info['model']}")
            >>>     print(f"Settings: ISO {info['iso']}, f/{info['aperture']}")
        )pbdoc");
}
