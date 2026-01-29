#include "atom/utils/debug/color_print.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(color_print, m) {
    m.doc() = R"pbdoc(
        Color Print Module
        ------------------

        This module provides utilities for printing colored text to the console
        using ANSI escape codes.

        Features:
        - Multiple color options (Black, Red, Green, Yellow, Blue, Magenta, Cyan, White)
        - Bright color variants
        - Text styles (Bold, Italic, Underline, etc.)
        - Formatted printing with std::format-style syntax

        Examples:
            >>> from atom.utils import color_print
            >>> # Print colored text
            >>> color_print.print_colored("Hello", color_print.ColorCode.Red)
            >>> # Print with style
            >>> color_print.print_colored("Bold text", color_print.ColorCode.Green, color_print.TextStyle.Bold)
            >>> # Print with newline
            >>> color_print.print_colored_line("Hello World", color_print.ColorCode.Blue)
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Expose ColorCode enum
    py::enum_<atom::utils::ColorCode>(
        m, "ColorCode",
        R"(ANSI color codes for terminal text coloring.

Available colors:
    - Black, Red, Green, Yellow, Blue, Magenta, Cyan, White
    - BrightBlack, BrightRed, BrightGreen, BrightYellow, BrightBlue, BrightMagenta, BrightCyan, BrightWhite
)")
        .value("Black", atom::utils::ColorCode::Black)
        .value("Red", atom::utils::ColorCode::Red)
        .value("Green", atom::utils::ColorCode::Green)
        .value("Yellow", atom::utils::ColorCode::Yellow)
        .value("Blue", atom::utils::ColorCode::Blue)
        .value("Magenta", atom::utils::ColorCode::Magenta)
        .value("Cyan", atom::utils::ColorCode::Cyan)
        .value("White", atom::utils::ColorCode::White)
        .value("BrightBlack", atom::utils::ColorCode::BrightBlack)
        .value("BrightRed", atom::utils::ColorCode::BrightRed)
        .value("BrightGreen", atom::utils::ColorCode::BrightGreen)
        .value("BrightYellow", atom::utils::ColorCode::BrightYellow)
        .value("BrightBlue", atom::utils::ColorCode::BrightBlue)
        .value("BrightMagenta", atom::utils::ColorCode::BrightMagenta)
        .value("BrightCyan", atom::utils::ColorCode::BrightCyan)
        .value("BrightWhite", atom::utils::ColorCode::BrightWhite)
        .export_values();

    // Expose TextStyle enum
    py::enum_<atom::utils::TextStyle>(
        m, "TextStyle",
        R"(ANSI text formatting attributes for terminal text styling.

Available styles:
    - Normal, Bold, Dim, Italic, Underline, Blinking, Reverse, Hidden, Strikethrough
)")
        .value("Normal", atom::utils::TextStyle::Normal)
        .value("Bold", atom::utils::TextStyle::Bold)
        .value("Dim", atom::utils::TextStyle::Dim)
        .value("Italic", atom::utils::TextStyle::Italic)
        .value("Underline", atom::utils::TextStyle::Underline)
        .value("Blinking", atom::utils::TextStyle::Blinking)
        .value("Reverse", atom::utils::TextStyle::Reverse)
        .value("Hidden", atom::utils::TextStyle::Hidden)
        .value("Strikethrough", atom::utils::TextStyle::Strikethrough)
        .export_values();

    // Expose ColorPrinter class
    py::class_<atom::utils::ColorPrinter>(
        m, "ColorPrinter",
        R"(Color printing utility class with modern formatting support.

This class provides static methods for colored text output in terminals.
)")
        .def_static("print_colored",
                    py::overload_cast<std::string_view, atom::utils::ColorCode,
                                      atom::utils::TextStyle>(
                        &atom::utils::ColorPrinter::printColored),
                    py::arg("text"), py::arg("color"),
                    py::arg("style") = atom::utils::TextStyle::Normal,
                    R"(Print text with specified color and style.

Args:
    text: The text to print
    color: The text color (ColorCode enum)
    style: The text style (TextStyle enum, default: Normal)

Examples:
    >>> ColorPrinter.print_colored("Hello", ColorCode.Red)
    >>> ColorPrinter.print_colored("Bold", ColorCode.Green, TextStyle.Bold)
)")
        .def_static(
            "print_colored_line", &atom::utils::ColorPrinter::printColoredLine,
            py::arg("text"), py::arg("color"),
            py::arg("style") = atom::utils::TextStyle::Normal,
            R"(Print text with specified color and style, followed by a newline.

Args:
    text: The text to print
    color: The text color (ColorCode enum)
    style: The text style (TextStyle enum, default: Normal)

Examples:
    >>> ColorPrinter.print_colored_line("Hello", ColorCode.Blue)
)");

    // Convenience functions
    m.def(
        "print_colored",
        [](const std::string& text, atom::utils::ColorCode color,
           atom::utils::TextStyle style) {
            atom::utils::ColorPrinter::printColored(text, color, style);
        },
        py::arg("text"), py::arg("color"),
        py::arg("style") = atom::utils::TextStyle::Normal,
        "Print colored text (convenience function).");

    m.def(
        "print_colored_line",
        [](const std::string& text, atom::utils::ColorCode color,
           atom::utils::TextStyle style) {
            atom::utils::ColorPrinter::printColoredLine(text, color, style);
        },
        py::arg("text"), py::arg("color"),
        py::arg("style") = atom::utils::TextStyle::Normal,
        "Print colored text with newline (convenience function).");
}
