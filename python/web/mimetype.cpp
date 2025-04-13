#include "atom/web/minetype.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(mimetype, m) {
    m.doc() = "MIME type handling module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const MimeTypeException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // MimeTypeConfig struct binding
    py::class_<MimeTypeConfig>(
        m, "MimeTypeConfig",
        R"(Configuration options for the MimeTypes class.

This class defines various settings that control the behavior of the MimeTypes class,
such as caching, leniency in MIME type detection, and deep scanning.

Args:
    lenient: Whether to be lenient in MIME type detection. Default is False.
    use_cache: Whether to use caching for frequent lookups. Default is True.
    cache_size: Maximum number of entries in the cache. Default is 1000.
    enable_deep_scanning: Whether to enable deep content scanning. Default is False.
    default_type: Default MIME type when unknown. Default is "application/octet-stream".

Examples:
    >>> from atom.web.mimetype import MimeTypeConfig
    >>> config = MimeTypeConfig(lenient=True, cache_size=2000)
    >>> config.lenient
    True
    >>> config.cache_size
    2000
)")
        .def(py::init<>(), "Constructs a default MimeTypeConfig object.")
        .def_readwrite("lenient", &MimeTypeConfig::lenient,
                       "Whether to be lenient in MIME type detection.")
        .def_readwrite("use_cache", &MimeTypeConfig::useCache,
                       "Whether to use caching for frequent lookups.")
        .def_readwrite("cache_size", &MimeTypeConfig::cacheSize,
                       "Maximum number of entries in the cache.")
        .def_readwrite("enable_deep_scanning",
                       &MimeTypeConfig::enableDeepScanning,
                       "Whether to enable deep content scanning.")
        .def_readwrite("default_type", &MimeTypeConfig::defaultType,
                       "Default MIME type when unknown.");

    // MimeTypes class binding
    py::class_<MimeTypes>(
        m, "MimeTypes",
        R"(A class for handling MIME types and file extensions.

This class provides methods to detect MIME types from file extensions,
guess file extensions from MIME types, and manage the MIME type database.

Args:
    known_files: A list of known file paths to initialize the database.
    lenient: Optional flag indicating whether to be lenient in MIME type detection.
    config: Optional configuration object with detailed settings.

Examples:
    >>> from atom.web.mimetype import MimeTypes, MimeTypeConfig
    >>> # Simple initialization
    >>> mime = MimeTypes(["/path/to/mime.types"])
    >>> # With custom configuration
    >>> config = MimeTypeConfig(lenient=True, cache_size=2000)
    >>> mime = MimeTypes(["/path/to/mime.types"], config)
    >>> # Guess MIME type and charset
    >>> mime_type, charset = mime.guess_type("example.txt")
    >>> print(mime_type)
    text/plain
)")
        .def(py::init<std::span<const std::string>, bool>(),
             py::arg("known_files"), py::arg("lenient") = false,
             "Constructs a MimeTypes object with known files and optional "
             "leniency setting.")
        .def(py::init<std::span<const std::string>, const MimeTypeConfig&>(),
             py::arg("known_files"), py::arg("config"),
             "Constructs a MimeTypes object with known files and custom "
             "configuration.")
        .def("read_json", &MimeTypes::readJson, py::arg("json_file"),
             R"(Reads MIME types from a JSON file.

Args:
    json_file: The path to the JSON file.

Raises:
    RuntimeError: If reading the JSON file fails.
)")
        .def("read_xml", &MimeTypes::readXml, py::arg("xml_file"),
             R"(Reads MIME types from an XML file.

Args:
    xml_file: The path to the XML file.

Raises:
    RuntimeError: If reading the XML file fails.
)")
        .def("guess_type", &MimeTypes::guessType, py::arg("url"),
             R"(Guesses the MIME type and charset of a URL.

Args:
    url: The URL to guess the MIME type for.

Returns:
    A tuple containing the guessed MIME type and charset, if available.
    Each element can be None if not determined.
)")
        .def("guess_all_extensions", &MimeTypes::guessAllExtensions,
             py::arg("mime_type"),
             R"(Guesses all possible file extensions for a given MIME type.

Args:
    mime_type: The MIME type to guess extensions for.

Returns:
    A list of possible file extensions.
)")
        .def("guess_extension", &MimeTypes::guessExtension,
             py::arg("mime_type"),
             R"(Guesses the file extension for a given MIME type.

Args:
    mime_type: The MIME type to guess the extension for.

Returns:
    The guessed file extension, if available, or None.
)")
        .def("add_type", &MimeTypes::addType, py::arg("mime_type"),
             py::arg("extension"),
             R"(Adds a new MIME type and file extension pair.

Args:
    mime_type: The MIME type to add.
    extension: The file extension to associate with the MIME type.

Raises:
    ValueError: If the input is invalid.
)")
        .def("add_types_batch", &MimeTypes::addTypesBatch, py::arg("types"),
             R"(Adds multiple MIME type and file extension pairs in batch.

Args:
    types: List of tuples containing MIME type and extension pairs.
)")
        .def("list_all_types", &MimeTypes::listAllTypes,
             "Lists all known MIME types and their associated file extensions.")
        .def(
            "guess_type_by_content",
            [](const MimeTypes& self, const std::string& filePath) {
                return self.guessTypeByContent(filePath);
            },
            py::arg("file_path"),
            R"(Guesses the MIME type of a file based on its content.

Args:
    file_path: The path to the file.

Returns:
    The guessed MIME type, if available, or None.

Raises:
    RuntimeError: If the file cannot be accessed.
)")
        .def("export_to_json", &MimeTypes::exportToJson, py::arg("json_file"),
             R"(Exports all MIME types to a JSON file.

Args:
    json_file: The path to the output JSON file.

Raises:
    RuntimeError: If exporting fails.
)")
        .def("export_to_xml", &MimeTypes::exportToXml, py::arg("xml_file"),
             R"(Exports all MIME types to an XML file.

Args:
    xml_file: The path to the output XML file.

Raises:
    RuntimeError: If exporting fails.
)")
        .def("clear_cache", &MimeTypes::clearCache,
             "Clears the internal cache to free memory.")
        .def("update_config", &MimeTypes::updateConfig, py::arg("config"),
             R"(Updates the configuration settings.

Args:
    config: New configuration options.
)")
        .def("get_config", &MimeTypes::getConfig,
             "Gets the current configuration.")
        .def("has_mime_type", &MimeTypes::hasMimeType, py::arg("mime_type"),
             R"(Checks if a MIME type is registered.

Args:
    mime_type: The MIME type to check.

Returns:
    True if the MIME type is registered, false otherwise.
)")
        .def("has_extension", &MimeTypes::hasExtension, py::arg("extension"),
             R"(Checks if a file extension is registered.

Args:
    extension: The file extension to check.

Returns:
    True if the extension is registered, false otherwise.
)");

    // Convenience functions
    m.def(
        "guess_type",
        [](const std::string& url, const std::vector<std::string>& db_files,
           bool lenient) {
            MimeTypes mime_types(db_files, lenient);
            return mime_types.guessType(url);
        },
        py::arg("url"), py::arg("db_files"), py::arg("lenient") = false,
        R"(Convenience function to guess MIME type without creating a MimeTypes instance.

Args:
    url: The URL to guess the MIME type for.
    db_files: List of database files to use.
    lenient: Whether to be lenient in MIME type detection. Default is False.

Returns:
    A tuple containing the guessed MIME type and charset, if available.
    Each element can be None if not determined.

Examples:
    >>> from atom.web.mimetype import guess_type
    >>> mime_type, charset = guess_type("example.txt", ["/path/to/mime.types"])
    >>> print(mime_type)
    text/plain
)");

    m.def(
        "guess_extension",
        [](const std::string& mime_type,
           const std::vector<std::string>& db_files, bool lenient) {
            MimeTypes mime_types(db_files, lenient);
            return mime_types.guessExtension(mime_type);
        },
        py::arg("mime_type"), py::arg("db_files"), py::arg("lenient") = false,
        R"(Convenience function to guess file extension without creating a MimeTypes instance.

Args:
    mime_type: The MIME type to guess the extension for.
    db_files: List of database files to use.
    lenient: Whether to be lenient in extension detection. Default is False.

Returns:
    The guessed file extension, if available, or None.

Examples:
    >>> from atom.web.mimetype import guess_extension
    >>> ext = guess_extension("text/plain", ["/path/to/mime.types"])
    >>> print(ext)
    .txt
)");

    // Create empty default database for simple usage
    m.def(
        "create_default_database",
        []() { return MimeTypes(std::vector<std::string>{}); },
        R"(Creates a MimeTypes instance with default settings and an empty database.

This function is useful when you want to build a MIME type database from scratch
or when you plan to load data later.

Returns:
    A MimeTypes instance with default settings.

Examples:
    >>> from atom.web.mimetype import create_default_database
    >>> mime = create_default_database()
    >>> mime.add_type("text/plain", ".txt")
    >>> mime_type, _ = mime.guess_type("example.txt")
    >>> print(mime_type)
    text/plain
)");
}