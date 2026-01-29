#include "atom/extra/pugixml/modern_xml.hpp"

#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(pugixml, m) {
    m.doc() =
        R"(Modern XML parsing and manipulation module for the atom package.

This module provides a modern C++ interface for XML parsing, manipulation,
and querying using the pugixml library. It offers type-safe operations,
RAII resource management, and a fluent API for XML document construction.

Features:
- Modern C++ interface with RAII and move semantics
- Type-safe XML parsing and manipulation
- XPath query support with result iteration
- Fluent API for document construction
- Comprehensive error handling with custom exceptions
- Support for various XML formats and encodings

Examples:
    >>> from atom.extra.pugixml import pugixml
    >>>
    >>> # Parse XML from string
    >>> doc = pugixml.Document.from_string('<root><item>value</item></root>')
    >>> root = doc.root()
    >>> print(root.name())
    >>>
    >>> # Create new document
    >>> doc = pugixml.Document.create_empty()
    >>> root = doc.create_root("config")
    >>> item = root.append_child("setting")
    >>> item.set_text("value")
    >>>
    >>> # Save to file
    >>> doc.save_to_file("config.xml")
)";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::extra::pugixml::ParseException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::extra::pugixml::XmlException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Custom exceptions
    py::register_exception<atom::extra::pugixml::ParseException>(
        m, "ParseException", PyExc_ValueError);
    py::register_exception<atom::extra::pugixml::XmlException>(
        m, "XmlException", PyExc_RuntimeError);

    // LoadOptions struct
    py::class_<atom::extra::pugixml::LoadOptions>(
        m, "LoadOptions",
        R"(Options for loading XML documents.

This struct contains various options that control how XML documents are loaded,
including parsing flags, encoding settings, and validation options.

Examples:
    >>> options = pugixml.LoadOptions()
    >>> options.preserve_whitespace = True
    >>> options.validate_structure = False
    >>> doc = pugixml.Document.from_file("data.xml", options)
)")
        .def(py::init<>(), "Create default load options")
        .def_readwrite("preserve_whitespace",
                       &atom::extra::pugixml::LoadOptions::preserve_whitespace,
                       "Whether to preserve whitespace in text nodes")
        .def_readwrite("validate_structure",
                       &atom::extra::pugixml::LoadOptions::validate_structure,
                       "Whether to validate XML structure during parsing")
        .def_readwrite("encoding", &atom::extra::pugixml::LoadOptions::encoding,
                       "Character encoding for the XML document");

    // SaveOptions struct
    py::class_<atom::extra::pugixml::SaveOptions>(
        m, "SaveOptions",
        R"(Options for saving XML documents.

This struct contains various options that control how XML documents are saved,
including formatting, encoding, and indentation settings.

Examples:
    >>> options = pugixml.SaveOptions()
    >>> options.indent = "  "  # Use 2 spaces for indentation
    >>> options.encoding = "UTF-8"
    >>> doc.save_to_file("output.xml", options)
)")
        .def(py::init<>(), "Create default save options")
        .def_readwrite("indent", &atom::extra::pugixml::SaveOptions::indent,
                       "Indentation string for formatting")
        .def_readwrite("encoding", &atom::extra::pugixml::SaveOptions::encoding,
                       "Character encoding for output");

    // Attribute class
    py::class_<atom::extra::pugixml::Attribute>(m, "Attribute",
                                                R"(Represents an XML attribute.

This class provides methods for accessing and modifying XML attribute
values with type-safe conversions.

Examples:
    >>> attr = node.attribute("id")
    >>> print(attr.value())
    >>> attr.set_value("new_value")
    >>> print(attr.as_int())
)")
        .def("name", &atom::extra::pugixml::Attribute::name,
             R"(Get the attribute name.

Returns:
    The attribute name as a string.
)")
        .def("value", &atom::extra::pugixml::Attribute::value,
             R"(Get the attribute value as a string.

Returns:
    The attribute value as a string.
)")
        .def("set_value", &atom::extra::pugixml::Attribute::set_value,
             py::arg("value"),
             R"(Set the attribute value.

Args:
    value: The new attribute value.
)")
        .def("as_string", &atom::extra::pugixml::Attribute::as_string,
             py::arg("default") = "",
             R"(Get the attribute value as a string.

Args:
    default: Default value if attribute is empty.

Returns:
    The attribute value as a string.
)")
        .def("as_int", &atom::extra::pugixml::Attribute::as_int,
             py::arg("default") = 0,
             R"(Get the attribute value as an integer.

Args:
    default: Default value if conversion fails.

Returns:
    The attribute value as an integer.
)")
        .def("as_uint", &atom::extra::pugixml::Attribute::as_uint,
             py::arg("default") = 0,
             R"(Get the attribute value as an unsigned integer.

Args:
    default: Default value if conversion fails.

Returns:
    The attribute value as an unsigned integer.
)")
        .def("as_double", &atom::extra::pugixml::Attribute::as_double,
             py::arg("default") = 0.0,
             R"(Get the attribute value as a double.

Args:
    default: Default value if conversion fails.

Returns:
    The attribute value as a double.
)")
        .def("as_float", &atom::extra::pugixml::Attribute::as_float,
             py::arg("default") = 0.0f,
             R"(Get the attribute value as a float.

Args:
    default: Default value if conversion fails.

Returns:
    The attribute value as a float.
)")
        .def("as_bool", &atom::extra::pugixml::Attribute::as_bool,
             py::arg("default") = false,
             R"(Get the attribute value as a boolean.

Args:
    default: Default value if conversion fails.

Returns:
    The attribute value as a boolean.
)")
        .def("empty", &atom::extra::pugixml::Attribute::empty,
             R"(Check if the attribute is empty.

Returns:
    True if the attribute is empty.
)")
        .def(
            "__bool__",
            [](const atom::extra::pugixml::Attribute& self) {
                return !self.empty();
            },
            R"(Check if the attribute exists and is not empty.)");

    // Node class
    py::class_<atom::extra::pugixml::Node>(m, "Node",
                                           R"(Represents an XML node.

This class provides methods for navigating, querying, and modifying
XML nodes and their children.

Examples:
    >>> node = doc.root()
    >>> print(node.name())
    >>> child = node.child("item")
    >>> print(child.text())
    >>>
    >>> # Add new child
    >>> new_child = node.append_child("new_item")
    >>> new_child.set_text("content")
)")
        .def("name", &atom::extra::pugixml::Node::name,
             R"(Get the node name.

Returns:
    The node name as a string.
)")
        .def("value", &atom::extra::pugixml::Node::value,
             R"(Get the node value.

Returns:
    The node value as a string.
)")
        .def("text", &atom::extra::pugixml::Node::text,
             R"(Get the text content of the node.

Returns:
    The text content as a string.
)")
        .def("set_name", &atom::extra::pugixml::Node::set_name, py::arg("name"),
             R"(Set the node name.

Args:
    name: The new node name.
)")
        .def("set_value", &atom::extra::pugixml::Node::set_value,
             py::arg("value"),
             R"(Set the node value.

Args:
    value: The new node value.
)")
        .def("set_text", &atom::extra::pugixml::Node::set_text, py::arg("text"),
             R"(Set the text content of the node.

Args:
    text: The new text content.
)")
        .def("append_child", &atom::extra::pugixml::Node::append_child,
             py::arg("name"),
             R"(Append a new child node.

Args:
    name: The name of the new child node.

Returns:
    The newly created child node.
)")
        .def("prepend_child", &atom::extra::pugixml::Node::prepend_child,
             py::arg("name"),
             R"(Prepend a new child node.

Args:
    name: The name of the new child node.

Returns:
    The newly created child node.
)")
        .def("insert_child_after",
             &atom::extra::pugixml::Node::insert_child_after, py::arg("name"),
             py::arg("node"),
             R"(Insert a new child node after the specified node.

Args:
    name: The name of the new child node.
    node: The reference node.

Returns:
    The newly created child node.
)")
        .def("insert_child_before",
             &atom::extra::pugixml::Node::insert_child_before, py::arg("name"),
             py::arg("node"),
             R"(Insert a new child node before the specified node.

Args:
    name: The name of the new child node.
    node: The reference node.

Returns:
    The newly created child node.
)")
        .def("remove_child",
             py::overload_cast<const atom::extra::pugixml::Node&>(
                 &atom::extra::pugixml::Node::remove_child),
             py::arg("node"),
             R"(Remove a child node.

Args:
    node: The child node to remove.

Returns:
    True if the node was removed successfully.
)")
        .def("remove_child",
             py::overload_cast<std::string_view>(
                 &atom::extra::pugixml::Node::remove_child),
             py::arg("name"),
             R"(Remove a child node by name.

Args:
    name: The name of the child node to remove.

Returns:
    True if the node was removed successfully.
)")
        .def("child", &atom::extra::pugixml::Node::child, py::arg("name"),
             R"(Get a child node by name.

Args:
    name: The name of the child node.

Returns:
    The child node, or an empty node if not found.
)")
        .def("attribute", &atom::extra::pugixml::Node::attribute,
             py::arg("name"),
             R"(Get an attribute by name.

Args:
    name: The name of the attribute.

Returns:
    The attribute, or an empty attribute if not found.
)")
        .def("append_attribute", &atom::extra::pugixml::Node::append_attribute,
             py::arg("name"),
             R"(Append a new attribute.

Args:
    name: The name of the new attribute.

Returns:
    The newly created attribute.
)")
        .def("prepend_attribute",
             &atom::extra::pugixml::Node::prepend_attribute, py::arg("name"),
             R"(Prepend a new attribute.

Args:
    name: The name of the new attribute.

Returns:
    The newly created attribute.
)")
        .def("remove_attribute",
             py::overload_cast<const atom::extra::pugixml::Attribute&>(
                 &atom::extra::pugixml::Node::remove_attribute),
             py::arg("attr"),
             R"(Remove an attribute.

Args:
    attr: The attribute to remove.

Returns:
    True if the attribute was removed successfully.
)")
        .def("remove_attribute",
             py::overload_cast<std::string_view>(
                 &atom::extra::pugixml::Node::remove_attribute),
             py::arg("name"),
             R"(Remove an attribute by name.

Args:
    name: The name of the attribute to remove.

Returns:
    True if the attribute was removed successfully.
)")
        .def("first_child", &atom::extra::pugixml::Node::first_child,
             R"(Get the first child node.

Returns:
    The first child node, or an empty node if no children.
)")
        .def("last_child", &atom::extra::pugixml::Node::last_child,
             R"(Get the last child node.

Returns:
    The last child node, or an empty node if no children.
)")
        .def("next_sibling", &atom::extra::pugixml::Node::next_sibling,
             R"(Get the next sibling node.

Returns:
    The next sibling node, or an empty node if no next sibling.
)")
        .def("previous_sibling", &atom::extra::pugixml::Node::previous_sibling,
             R"(Get the previous sibling node.

Returns:
    The previous sibling node, or an empty node if no previous sibling.
)")
        .def("parent", &atom::extra::pugixml::Node::parent,
             R"(Get the parent node.

Returns:
    The parent node, or an empty node if this is the root.
)")
        .def("empty", &atom::extra::pugixml::Node::empty,
             R"(Check if the node is empty.

Returns:
    True if the node is empty.
)")
        .def(
            "__bool__",
            [](const atom::extra::pugixml::Node& self) {
                return !self.empty();
            },
            R"(Check if the node exists and is not empty.)");

    // Document class
    py::class_<atom::extra::pugixml::Document>(m, "Document",
                                               R"(Represents an XML document.

This class provides methods for loading, parsing, creating, and saving
XML documents with modern C++ features and comprehensive error handling.

Examples:
    >>> # Load from file
    >>> doc = pugixml.Document.from_file("data.xml")
    >>>
    >>> # Parse from string
    >>> doc = pugixml.Document.from_string('<root><item>value</item></root>')
    >>>
    >>> # Create empty document
    >>> doc = pugixml.Document.create_empty()
    >>> root = doc.create_root("config")
)")
        .def_static("from_string", &atom::extra::pugixml::Document::from_string,
                    py::arg("xml"),
                    py::arg("options") = atom::extra::pugixml::LoadOptions{},
                    R"(Parse XML document from a string.

Args:
    xml: The XML content as a string.
    options: Load options for parsing.

Returns:
    A new Document instance.

Raises:
    ParseException: If the XML cannot be parsed.
)")
        .def_static("from_file", &atom::extra::pugixml::Document::from_file,
                    py::arg("path"),
                    py::arg("options") = atom::extra::pugixml::LoadOptions{},
                    R"(Load XML document from a file.

Args:
    path: Path to the XML file.
    options: Load options for parsing.

Returns:
    A new Document instance.

Raises:
    ParseException: If the file cannot be loaded or parsed.
)")
        .def_static("create_empty",
                    &atom::extra::pugixml::Document::create_empty,
                    py::arg("version") = "1.0", py::arg("encoding") = "UTF-8",
                    py::arg("standalone") = "",
                    R"(Create an empty XML document with declaration.

Args:
    version: XML version (default: "1.0").
    encoding: Character encoding (default: "UTF-8").
    standalone: Standalone declaration (default: empty).

Returns:
    A new empty Document instance.
)")
        .def("root", &atom::extra::pugixml::Document::root,
             R"(Get the root element of the document.

Returns:
    The root element node.
)")
        .def("document_element",
             &atom::extra::pugixml::Document::document_element,
             R"(Get the document element (same as root).

Returns:
    The document element node.
)")
        .def("create_root", &atom::extra::pugixml::Document::create_root,
             py::arg("name"),
             R"(Create a root element for the document.

Args:
    name: The name of the root element.

Returns:
    The newly created root element node.

Raises:
    XmlException: If the root element cannot be created.
)")
        .def("document", &atom::extra::pugixml::Document::document,
             R"(Get the full document node for advanced operations.

Returns:
    The document node.
)")
        .def("save_to_file", &atom::extra::pugixml::Document::save_to_file,
             py::arg("path"),
             py::arg("options") = atom::extra::pugixml::SaveOptions{},
             R"(Save the document to a file.

Args:
    path: Path to the output file.
    options: Save options for formatting.

Raises:
    XmlException: If the file cannot be saved.
)")
        .def("save_to_stream", &atom::extra::pugixml::Document::save_to_stream,
             py::arg("stream"),
             py::arg("options") = atom::extra::pugixml::SaveOptions{},
             R"(Save the document to a stream.

Args:
    stream: Output stream.
    options: Save options for formatting.
)")
        .def("to_string", &atom::extra::pugixml::Document::to_string,
             py::arg("options") = atom::extra::pugixml::SaveOptions{},
             R"(Convert the document to a string.

Args:
    options: Save options for formatting.

Returns:
    The document as an XML string.
)")
        .def("clone", &atom::extra::pugixml::Document::clone,
             R"(Create a deep copy of the document.

Returns:
    A new Document instance that is a copy of this document.
)");

    // Version information
    m.attr("__version__") = atom::extra::pugixml::version::string;

    py::module_ version_module =
        m.def_submodule("version", "Version information");
    version_module.attr("major") = atom::extra::pugixml::version::major;
    version_module.attr("minor") = atom::extra::pugixml::version::minor;
    version_module.attr("patch") = atom::extra::pugixml::version::patch;
    version_module.attr("string") = atom::extra::pugixml::version::string;
}
