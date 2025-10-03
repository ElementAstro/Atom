#include "atom/system/clipboard/clipboard.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(clipboard, m) {
    m.doc() = R"pbdoc(
        Clipboard Operations Module
        ---------------------------

        This module provides cross-platform clipboard operations with support for:
        - Text operations with Unicode support
        - Binary data operations with zero-copy support
        - Image operations (when OpenCV/CImg support is available)
        - Clipboard change monitoring with callbacks
        - Custom format registration

        Examples:
            >>> from atom.system import clipboard
            >>> # Get clipboard instance
            >>> clip = clipboard.Clipboard.instance()
            >>> 
            >>> # Text operations
            >>> clip.set_text("Hello, World!")
            >>> text = clip.get_text()
            >>> print(text)  # "Hello, World!"
            >>> 
            >>> # Check clipboard contents
            >>> if clip.has_text():
            ...     print("Clipboard contains text")
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const clip::ClipboardException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const clip::ClipboardFormatException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // ClipboardFormat class
    py::class_<clip::ClipboardFormat>(
        m, "ClipboardFormat",
        R"(Strong type for clipboard format identifiers.

This class represents a clipboard format identifier used to specify
the type of data being stored or retrieved from the clipboard.

Args:
    value: The format identifier value.

Examples:
    >>> from atom.system import clipboard
    >>> # Use predefined formats
    >>> text_format = clipboard.formats.TEXT
    >>> html_format = clipboard.formats.HTML
)")
        .def(py::init<unsigned int>(), py::arg("value"),
             "Constructs a ClipboardFormat with the specified value.")
        .def_property_readonly("value", [](const clip::ClipboardFormat& self) {
            return static_cast<unsigned int>(self);
        }, "Gets the format identifier value.")
        .def("__eq__", &clip::ClipboardFormat::operator==)
        .def("__int__", [](const clip::ClipboardFormat& self) {
            return static_cast<unsigned int>(self);
        });

    // Predefined formats namespace
    auto formats_module = m.def_submodule("formats", "Predefined clipboard formats");
    formats_module.attr("TEXT") = clip::formats::TEXT;
    formats_module.attr("HTML") = clip::formats::HTML;
    formats_module.attr("IMAGE_TIFF") = clip::formats::IMAGE_TIFF;
    formats_module.attr("IMAGE_PNG") = clip::formats::IMAGE_PNG;
    formats_module.attr("RTF") = clip::formats::RTF;

    // ClipboardResult template for void
    py::class_<clip::ClipboardResult<void>>(
        m, "ClipboardResultVoid",
        R"(Result type for clipboard operations that don't return a value.

This class represents the result of a clipboard operation that either
succeeds or fails with an error code.

Examples:
    >>> result = clip.set_text_safe("Hello")
    >>> if result:
        ...     print("Operation succeeded")
    >>> else:
        ...     print(f"Operation failed: {result.error()}")
)")
        .def("has_value", &clip::ClipboardResult<void>::has_value,
             "Returns True if the operation succeeded, False otherwise.")
        .def("__bool__", &clip::ClipboardResult<void>::operator bool,
             "Returns True if the operation succeeded, False otherwise.")
        .def("value", &clip::ClipboardResult<void>::value,
             "Checks that the operation succeeded (throws if it failed).")
        .def("error", &clip::ClipboardResult<void>::error,
             "Returns the error code if the operation failed.");

    // ClipboardResult template for string
    py::class_<clip::ClipboardResult<std::string>>(
        m, "ClipboardResultString",
        R"(Result type for clipboard operations that return a string.

Examples:
    >>> result = clip.get_text_safe()
    >>> if result:
        ...     print(f"Text: {result.value()}")
    >>> else:
        ...     print(f"Failed: {result.error()}")
)")
        .def("has_value", &clip::ClipboardResult<std::string>::has_value)
        .def("__bool__", &clip::ClipboardResult<std::string>::operator bool)
        .def("value", &clip::ClipboardResult<std::string>::value)
        .def("error", &clip::ClipboardResult<std::string>::error)
        .def("value_or", &clip::ClipboardResult<std::string>::value_or<std::string>,
             py::arg("default_value"),
             "Returns the value if available, otherwise returns the default value.");

    // Main Clipboard class
    py::class_<clip::Clipboard>(
        m, "Clipboard",
        R"(Cross-platform clipboard operations wrapper class.

This class provides a unified interface for clipboard operations across
different platforms with modern C++ features including exception-safe
RAII resource management and callback mechanisms for change monitoring.

The Clipboard class is a singleton - use Clipboard.instance() to get
the global clipboard instance.

Examples:
    >>> from atom.system import clipboard
    >>> clip = clipboard.Clipboard.instance()
    >>> 
    >>> # Text operations
    >>> clip.set_text("Hello, World!")
    >>> text = clip.get_text()
    >>> 
    >>> # Safe operations (no exceptions)
    >>> result = clip.set_text_safe("Safe text")
    >>> if result:
    ...     print("Text set successfully")
)")
        .def_static("instance", &clip::Clipboard::instance,
                   py::return_value_policy::reference,
                   R"(Get the singleton instance of the Clipboard.

Returns:
    Reference to the singleton Clipboard instance.

Examples:
    >>> clip = clipboard.Clipboard.instance()
)")

        // Core operations
        .def("open", &clip::Clipboard::open,
             R"(Open the clipboard for operations.

Raises:
    RuntimeError: If the clipboard cannot be opened.

Examples:
    >>> clip.open()
)")
        .def("close", &clip::Clipboard::close,
             R"(Close the clipboard.

Examples:
    >>> clip.close()
)")
        .def("clear", &clip::Clipboard::clear,
             R"(Clear the clipboard contents.

Raises:
    RuntimeError: If the clipboard cannot be cleared.

Examples:
    >>> clip.clear()
)")

        // Text operations
        .def("set_text", &clip::Clipboard::setText, py::arg("text"),
             R"(Set text to the clipboard.

Args:
    text: UTF-8 encoded text to place on the clipboard.

Raises:
    RuntimeError: If the operation fails.

Examples:
    >>> clip.set_text("Hello, World!")
)")
        .def("set_text_safe", &clip::Clipboard::setTextSafe, py::arg("text"),
             R"(Set text to the clipboard (non-throwing version).

Args:
    text: UTF-8 encoded text to place on the clipboard.

Returns:
    ClipboardResult indicating success or error.

Examples:
    >>> result = clip.set_text_safe("Hello")
    >>> if not result:
    ...     print(f"Failed: {result.error()}")
)")
        .def("get_text", &clip::Clipboard::getText,
             R"(Get text from the clipboard.

Returns:
    UTF-8 encoded clipboard text.

Raises:
    RuntimeError: If the operation fails or no text is available.

Examples:
    >>> text = clip.get_text()
    >>> print(text)
)")
        .def("get_text_safe", &clip::Clipboard::getTextSafe,
             R"(Get text from the clipboard (non-throwing version).

Returns:
    ClipboardResult containing the text if available, error otherwise.

Examples:
    >>> result = clip.get_text_safe()
    >>> if result:
    ...     print(f"Text: {result.value()}")
)")

        // Query operations
        .def("has_text", &clip::Clipboard::hasText,
             R"(Check if clipboard contains text data.

Returns:
    True if text is available, False otherwise.

Examples:
    >>> if clip.has_text():
    ...     text = clip.get_text()
)")
        .def("has_image", &clip::Clipboard::hasImage,
             R"(Check if clipboard contains image data.

Returns:
    True if image is available, False otherwise.

Examples:
    >>> if clip.has_image():
    ...     print("Clipboard contains an image")
)")
        .def("contains_format", &clip::Clipboard::containsFormat, py::arg("format"),
             R"(Check if clipboard contains data in a specific format.

Args:
    format: The clipboard format identifier to check.

Returns:
    True if format is available, False otherwise.

Examples:
    >>> if clip.contains_format(clipboard.formats.HTML):
    ...     print("Clipboard contains HTML")
)")
        .def("get_available_formats", &clip::Clipboard::getAvailableFormats,
             R"(Get list of available clipboard formats.

Returns:
    List of format identifiers currently available on the clipboard.

Examples:
    >>> formats = clip.get_available_formats()
    >>> for fmt in formats:
    ...     print(f"Available format: {fmt.value}")
)")

        // Change monitoring
        .def("register_change_callback", &clip::Clipboard::registerChangeCallback,
             py::arg("callback"),
             R"(Register a callback for clipboard change notifications.

Args:
    callback: Function to call when clipboard content changes.

Returns:
    Callback ID for unregistering, or 0 on failure.

Examples:
    >>> def on_change():
    ...     print("Clipboard changed!")
    >>> callback_id = clip.register_change_callback(on_change)
)")
        .def("unregister_change_callback", &clip::Clipboard::unregisterChangeCallback,
             py::arg("callback_id"),
             R"(Unregister a clipboard change callback.

Args:
    callback_id: The callback ID returned by register_change_callback.

Returns:
    True if callback was successfully unregistered, False otherwise.

Examples:
    >>> clip.unregister_change_callback(callback_id)
)")
        .def("has_changed", &clip::Clipboard::hasChanged,
             R"(Check if clipboard content has changed since last check.

Returns:
    True if content has changed, False otherwise.

Examples:
    >>> if clip.has_changed():
    ...     print("Clipboard content changed")
)")
        .def("mark_change_processed", &clip::Clipboard::markChangeProcessed,
             R"(Update internal change tracking state.

Call this after processing clipboard changes to reset the changed flag.

Examples:
    >>> clip.mark_change_processed()
)")

        // Static format registration
        .def_static("register_format", &clip::Clipboard::registerFormat,
                   py::arg("format_name"),
                   R"(Register a custom clipboard format.

Args:
    format_name: Name of the custom format.

Returns:
    Registered format identifier.

Raises:
    RuntimeError: On registration failure.

Examples:
    >>> custom_format = clipboard.Clipboard.register_format("MyCustomFormat")
)")
        .def_static("register_format_safe", &clip::Clipboard::registerFormatSafe,
                   py::arg("format_name"),
                   R"(Register a custom clipboard format (non-throwing version).

Args:
    format_name: Name of the custom format.

Returns:
    ClipboardResult containing the registered format identifier or error.

Examples:
    >>> result = clipboard.Clipboard.register_format_safe("MyFormat")
    >>> if result:
    ...     custom_format = result.value()
)")

        // Binary data operations
        .def("set_data", [](clip::Clipboard& self, clip::ClipboardFormat format, py::bytes data) {
            std::string str_data = data;
            std::span<const std::byte> byte_span(
                reinterpret_cast<const std::byte*>(str_data.data()),
                str_data.size()
            );
            self.setData(format, byte_span);
        }, py::arg("format"), py::arg("data"),
             R"(Set binary data to the clipboard in a specific format.

Args:
    format: The clipboard format identifier.
    data: Binary data to place on the clipboard.

Raises:
    RuntimeError: If the operation fails.

Examples:
    >>> data = b"Binary data content"
    >>> clip.set_data(clipboard.formats.RTF, data)
)")
        .def("set_data_safe", [](clip::Clipboard& self, clip::ClipboardFormat format, py::bytes data) {
            std::string str_data = data;
            std::span<const std::byte> byte_span(
                reinterpret_cast<const std::byte*>(str_data.data()),
                str_data.size()
            );
            return self.setDataSafe(format, byte_span);
        }, py::arg("format"), py::arg("data"),
             R"(Set binary data to the clipboard (non-throwing version).

Args:
    format: The clipboard format identifier.
    data: Binary data to place on the clipboard.

Returns:
    ClipboardResult indicating success or error.

Examples:
    >>> result = clip.set_data_safe(custom_format, b"data")
    >>> if not result:
    ...     print(f"Failed: {result.error()}")
)")
        .def("get_data", [](clip::Clipboard& self, clip::ClipboardFormat format) -> py::bytes {
            auto data = self.getData(format);
            return py::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        }, py::arg("format"),
             R"(Get binary data from the clipboard.

Args:
    format: The clipboard format identifier to retrieve.

Returns:
    Binary data if available.

Raises:
    RuntimeError: If the operation fails or format is not available.

Examples:
    >>> data = clip.get_data(clipboard.formats.RTF)
    >>> print(f"Retrieved {len(data)} bytes")
)")
        .def("get_data_safe", [](clip::Clipboard& self, clip::ClipboardFormat format) {
            auto result = self.getDataSafe(format);
            if (result) {
                auto& data = result.value();
                return py::make_tuple(true, py::bytes(reinterpret_cast<const char*>(data.data()), data.size()));
            } else {
                return py::make_tuple(false, py::bytes());
            }
        }, py::arg("format"),
             R"(Get binary data from the clipboard (non-throwing version).

Args:
    format: The clipboard format identifier to retrieve.

Returns:
    Tuple of (success: bool, data: bytes). If success is False, data will be empty.

Examples:
    >>> success, data = clip.get_data_safe(custom_format)
    >>> if success:
    ...     print(f"Retrieved {len(data)} bytes")
)")
        .def("get_format_name", &clip::Clipboard::getFormatName, py::arg("format"),
             R"(Get human-readable name for a clipboard format.

Args:
    format: The format identifier.

Returns:
    Format name if known.

Raises:
    RuntimeError: If format is unknown.

Examples:
    >>> name = clip.get_format_name(clipboard.formats.TEXT)
    >>> print(f"Format name: {name}")
)")
        .def("get_format_name_safe", &clip::Clipboard::getFormatNameSafe, py::arg("format"),
             R"(Get human-readable name for a clipboard format (non-throwing version).

Args:
    format: The format identifier.

Returns:
    ClipboardResult containing the format name if known, error otherwise.

Examples:
    >>> result = clip.get_format_name_safe(custom_format)
    >>> if result:
    ...     print(f"Format name: {result.value()}")
)");
}
