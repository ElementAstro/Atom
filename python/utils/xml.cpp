#include "atom/utils/format/xml.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(xml, m) {
    m.doc() = R"pbdoc(
        XML Utilities Module
        --------------------

        This module provides XML reading and writing utilities:
        - XMLReader for parsing XML files
        - XMLWriter for creating XML files
        - Element and attribute access
        - Path-based queries

        Examples:
            >>> from atom.utils import xml
            >>> reader = xml.XMLReader("config.xml")
            >>> value = reader.get_element_text("setting")
            >>> children = reader.get_child_element_names("root")
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // XMLReader class
    py::class_<atom::utils::XMLReader>(m, "XMLReader",
                                       "XML file reader and parser")
        .def(py::init<std::string_view>(), py::arg("file_path"),
             R"(Create XMLReader for the specified file.

Args:
    file_path: Path to the XML file to read

Raises:
    RuntimeError: If the file cannot be loaded
)")
        .def(
            "get_child_element_names",
            [](const atom::utils::XMLReader& self,
               const std::string& parent_element_name) -> py::object {
                auto result = self.getChildElementNames(parent_element_name);
                if (std::holds_alternative<std::vector<std::string>>(result)) {
                    return py::cast(std::get<std::vector<std::string>>(result));
                } else {
                    throw std::runtime_error(std::get<std::string>(result));
                }
            },
            py::arg("parent_element_name"),
            "Get names of all child elements of the specified parent element")
        .def(
            "get_element_text",
            [](const atom::utils::XMLReader& self,
               const std::string& element_name) -> std::string {
                auto result = self.getElementText(element_name);
                if (std::holds_alternative<std::string>(result)) {
                    return std::get<std::string>(result);
                } else {
                    throw std::runtime_error(std::get<std::string>(result));
                }
            },
            py::arg("element_name"), "Get text value of the specified element")
        .def(
            "get_attribute_value",
            [](const atom::utils::XMLReader& self,
               const std::string& element_name,
               const std::string& attribute_name) -> std::string {
                auto result =
                    self.getAttributeValue(element_name, attribute_name);
                if (std::holds_alternative<std::string>(result)) {
                    return std::get<std::string>(result);
                } else {
                    throw std::runtime_error(std::get<std::string>(result));
                }
            },
            py::arg("element_name"), py::arg("attribute_name"),
            "Get value of the specified attribute")
        .def("get_root_element_names",
             &atom::utils::XMLReader::getRootElementNames,
             "Get names of all root elements in the XML file")
        .def("has_child_element", &atom::utils::XMLReader::hasChildElement,
             py::arg("parent_element_name"), py::arg("child_element_name"),
             "Check if parent element has a child element with the specified "
             "name")
        .def(
            "get_value_by_path",
            [](const atom::utils::XMLReader& self,
               const std::string& path) -> std::string {
                auto result = self.getValueByPath(path);
                if (std::holds_alternative<std::string>(result)) {
                    return std::get<std::string>(result);
                } else {
                    throw std::runtime_error(std::get<std::string>(result));
                }
            },
            py::arg("path"),
            R"(Get text value of element specified by path.

Args:
    path: Path to the element (e.g., "root/child/element")

Returns:
    Text value of the element

Raises:
    RuntimeError: If the path is invalid or element not found
)");

    // XMLWriter class
    py::class_<atom::utils::XMLWriter>(m, "XMLWriter",
                                       "XML file writer and generator")
        .def(py::init<std::string_view>(), py::arg("file_path"),
             R"(Create XMLWriter for the specified file.

Args:
    file_path: Path to the XML file to write

Raises:
    RuntimeError: If the file cannot be created
)")
        .def("add_element", &atom::utils::XMLWriter::addElement,
             py::arg("parent_element_name"), py::arg("element_name"),
             py::arg("element_text") = "",
             "Add a new element to the specified parent element")
        .def("add_attribute", &atom::utils::XMLWriter::addAttribute,
             py::arg("element_name"), py::arg("attribute_name"),
             py::arg("attribute_value"),
             "Add an attribute to the specified element")
        .def("save", &atom::utils::XMLWriter::save,
             "Save the XML document to file");
}
