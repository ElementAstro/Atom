#include "atom/extra/inicpp/inicpp.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(inicpp, m) {
    m.doc() = R"(INI configuration file parsing module for the atom package.

This module provides a high-performance, type-safe INI configuration file parser
with support for nested sections, event listeners, path queries, and format conversion.

Features:
- Type-safe field value retrieval through templates
- Thread-safe concurrent read/write operations using shared locks
- High performance with parallel processing, memory pools, and Boost containers
- Extensible with custom separators, escape characters, and comment prefixes
- Rich functionality including nested sections, event listeners, path queries, and format conversion

Examples:
    >>> from atom.extra.inicpp import inicpp
    >>>
    >>> # Load INI file
    >>> ini_file = inicpp.IniFile()
    >>> ini_file.load("config.ini")
    >>>
    >>> # Access sections and fields
    >>> section = ini_file["database"]
    >>> host = section["host"].as_string()
    >>> port = section["port"].as_int()
    >>>
    >>> # Create new section and fields
    >>> new_section = ini_file["new_section"]
    >>> new_section["key"] = "value"
    >>> new_section["number"] = 42
    >>>
    >>> # Save changes
    >>> ini_file.save("config.ini")
)";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::logic_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // IniField class binding
    py::class_<inicpp::IniField>(m, "IniField",
                                 R"(Represents a field in an INI file section.

This class stores a key-value pair and provides type-safe methods
for retrieving values in different formats.

Examples:
    >>> field = section["key"]
    >>> string_value = field.as_string()
    >>> int_value = field.as_int()
    >>> bool_value = field.as_bool()
    >>> float_value = field.as_float()
)")
        .def(py::init<>(), "Create an empty field")
        .def(py::init<const std::string&>(), py::arg("value"),
             "Create a field with a string value")
        .def("as_string", &inicpp::IniField::asString,
             R"(Get the field value as a string.

Returns:
    The field value as a string.
)")
        .def("as_int", &inicpp::IniField::asInt,
             R"(Get the field value as an integer.

Returns:
    The field value as an integer.

Raises:
    ValueError: If the value cannot be converted to an integer.
)")
        .def("as_float", &inicpp::IniField::asFloat,
             R"(Get the field value as a float.

Returns:
    The field value as a float.

Raises:
    ValueError: If the value cannot be converted to a float.
)")
        .def("as_bool", &inicpp::IniField::asBool,
             R"(Get the field value as a boolean.

Returns:
    The field value as a boolean.

Raises:
    ValueError: If the value cannot be converted to a boolean.
)")
        .def("set",
             py::overload_cast<const std::string&>(&inicpp::IniField::set),
             py::arg("value"),
             R"(Set the field value from a string.

Args:
    value: The string value to set.
)")
        .def("set", py::overload_cast<int>(&inicpp::IniField::set),
             py::arg("value"),
             R"(Set the field value from an integer.

Args:
    value: The integer value to set.
)")
        .def("set", py::overload_cast<double>(&inicpp::IniField::set),
             py::arg("value"),
             R"(Set the field value from a float.

Args:
    value: The float value to set.
)")
        .def("set", py::overload_cast<bool>(&inicpp::IniField::set),
             py::arg("value"),
             R"(Set the field value from a boolean.

Args:
    value: The boolean value to set.
)")
        .def("is_empty", &inicpp::IniField::isEmpty,
             R"(Check if the field is empty.

Returns:
    True if the field has no value.
)")
        .def("clear", &inicpp::IniField::clear, R"(Clear the field value.)");

    // IniSection class binding
    py::class_<inicpp::IniSection>(m, "IniSection",
                                   R"(Represents a section in an INI file.

This class contains a collection of key-value pairs (fields) and provides
methods for accessing and modifying them.

Examples:
    >>> section = ini_file["database"]
    >>> section["host"] = "localhost"
    >>> section["port"] = 5432
    >>> host = section["host"].as_string()
    >>> port = section["port"].as_int()
)")
        .def(py::init<>(), "Create an empty section")
        .def(
            "__getitem__",
            [](inicpp::IniSection& self, const std::string& key)
                -> inicpp::IniField& { return self[key]; },
            py::return_value_policy::reference_internal,
            R"(Get a field by key.

Args:
    key: The field key.

Returns:
    Reference to the field.
)")
        .def(
            "__setitem__",
            [](inicpp::IniSection& self, const std::string& key,
               const std::string& value) { self[key] = value; },
            R"(Set a field value.

Args:
    key: The field key.
    value: The field value.
)")
        .def(
            "__contains__",
            [](const inicpp::IniSection& self, const std::string& key) {
                return self.count(key) > 0;
            },
            R"(Check if a field exists.

Args:
    key: The field key.

Returns:
    True if the field exists.
)")
        .def("size", &inicpp::IniSection::size,
             R"(Get the number of fields in the section.

Returns:
    The number of fields.
)")
        .def("empty", &inicpp::IniSection::empty,
             R"(Check if the section is empty.

Returns:
    True if the section has no fields.
)")
        .def("clear", &inicpp::IniSection::clear,
             R"(Remove all fields from the section.)")
        .def(
            "erase",
            [](inicpp::IniSection& self, const std::string& key) {
                return self.erase(key);
            },
            py::arg("key"),
            R"(Remove a field from the section.

Args:
    key: The field key to remove.

Returns:
    Number of fields removed (0 or 1).
)")
        .def(
            "keys",
            [](const inicpp::IniSection& self) {
                std::vector<std::string> keys;
                for (const auto& pair : self) {
                    keys.push_back(pair.first);
                }
                return keys;
            },
            R"(Get all field keys in the section.

Returns:
    List of field keys.
)");

    // IniFile class binding
    py::class_<inicpp::IniFile>(
        m, "IniFile",
        R"(Main class for loading, parsing, and saving INI files.

This class provides a high-level interface for working with INI configuration files,
supporting various features like nested sections, comments, and type-safe value access.

Examples:
    >>> # Load existing file
    >>> ini = inicpp.IniFile()
    >>> ini.load("config.ini")
    >>>
    >>> # Access data
    >>> db_section = ini["database"]
    >>> host = db_section["host"].as_string()
    >>>
    >>> # Modify data
    >>> ini["new_section"]["key"] = "value"
    >>>
    >>> # Save changes
    >>> ini.save("config.ini")
)")
        .def(py::init<>(), "Create an empty INI file")
        .def("load",
             py::overload_cast<const std::string&>(&inicpp::IniFile::load),
             py::arg("filename"),
             R"(Load INI data from a file.

Args:
    filename: Path to the INI file to load.

Raises:
    RuntimeError: If the file cannot be loaded or parsed.
)")
        .def("save",
             py::overload_cast<const std::string&>(&inicpp::IniFile::save),
             py::arg("filename"),
             R"(Save INI data to a file.

Args:
    filename: Path to the file to save.

Raises:
    RuntimeError: If the file cannot be saved.
)")
        .def("parse", &inicpp::IniFile::parse, py::arg("content"),
             R"(Parse INI data from a string.

Args:
    content: The INI content as a string.

Raises:
    RuntimeError: If the content cannot be parsed.
)")
        .def(
            "__getitem__",
            [](inicpp::IniFile& self, const std::string& section_name)
                -> inicpp::IniSection& { return self[section_name]; },
            py::return_value_policy::reference_internal,
            R"(Get a section by name.

Args:
    section_name: The section name.

Returns:
    Reference to the section.
)")
        .def(
            "__contains__",
            [](const inicpp::IniFile& self, const std::string& section_name) {
                return self.count(section_name) > 0;
            },
            R"(Check if a section exists.

Args:
    section_name: The section name.

Returns:
    True if the section exists.
)")
        .def("size", &inicpp::IniFile::size,
             R"(Get the number of sections in the file.

Returns:
    The number of sections.
)")
        .def("empty", &inicpp::IniFile::empty,
             R"(Check if the file is empty.

Returns:
    True if the file has no sections.
)")
        .def("clear", &inicpp::IniFile::clear,
             R"(Remove all sections from the file.)")
        .def(
            "erase",
            [](inicpp::IniFile& self, const std::string& section_name) {
                return self.erase(section_name);
            },
            py::arg("section_name"),
            R"(Remove a section from the file.

Args:
    section_name: The section name to remove.

Returns:
    Number of sections removed (0 or 1).
)")
        .def(
            "sections",
            [](const inicpp::IniFile& self) {
                std::vector<std::string> sections;
                for (const auto& pair : self) {
                    sections.push_back(pair.first);
                }
                return sections;
            },
            R"(Get all section names in the file.

Returns:
    List of section names.
)");
}
