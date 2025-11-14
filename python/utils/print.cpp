#include "atom/utils/debug/print.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(print, m) {
    m.doc() = R"pbdoc(
        Print and Logging Utilities Module
        -----------------------------------

        This module provides comprehensive printing and logging utilities:
        - Formatted printing with println
        - Colored and styled console output
        - Progress bars
        - Performance timing
        - Table and chart printing
        - Statistical analysis

        Examples:
            >>> from atom.utils import print as prt
            >>> prt.println("Hello {}", "World")
            >>> prt.print_colored(prt.Color.RED, "Error message")
            >>> prt.print_progress_bar(0.5)
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

    // Enums
    py::enum_<atom::utils::LogLevel>(m, "LogLevel")
        .value("DEBUG", atom::utils::LogLevel::DEBUG_LEVEL)
        .value("INFO", atom::utils::LogLevel::INFO_LEVEL)
        .value("WARNING", atom::utils::LogLevel::WARNING_LEVEL)
        .value("ERROR", atom::utils::LogLevel::ERROR_LEVEL)
        .export_values();

    py::enum_<atom::utils::ProgressBarStyle>(m, "ProgressBarStyle")
        .value("BASIC", atom::utils::ProgressBarStyle::BASIC)
        .value("BLOCK", atom::utils::ProgressBarStyle::BLOCK)
        .value("ARROW", atom::utils::ProgressBarStyle::ARROW)
        .value("PERCENTAGE", atom::utils::ProgressBarStyle::PERCENTAGE)
        .export_values();

    py::enum_<atom::utils::TextStyle>(m, "TextStyle")
        .value("BOLD", atom::utils::TextStyle::BOLD)
        .value("UNDERLINE", atom::utils::TextStyle::UNDERLINE)
        .value("BLINK", atom::utils::TextStyle::BLINK)
        .value("REVERSE", atom::utils::TextStyle::REVERSE)
        .value("CONCEALED", atom::utils::TextStyle::CONCEALED)
        .export_values();

    py::enum_<atom::utils::Color>(m, "Color")
        .value("RED", atom::utils::Color::RED)
        .value("GREEN", atom::utils::Color::GREEN)
        .value("YELLOW", atom::utils::Color::YELLOW)
        .value("BLUE", atom::utils::Color::BLUE)
        .value("MAGENTA", atom::utils::Color::MAGENTA)
        .value("CYAN", atom::utils::Color::CYAN)
        .value("WHITE", atom::utils::Color::WHITE)
        .export_values();

    // Basic printing functions
    m.def(
        "println",
        [](const std::string& text) { atom::utils::println("{}", text); },
        py::arg("text"), "Print text with newline");

    m.def(
        "print_colored",
        [](atom::utils::Color color, const std::string& text) {
            atom::utils::printColored(color, "{}", text);
        },
        py::arg("color"), py::arg("text"), "Print colored text");

    m.def(
        "print_styled",
        [](atom::utils::TextStyle style, const std::string& text) {
            atom::utils::printStyled(style, "{}", text);
        },
        py::arg("style"), py::arg("text"), "Print styled text");

    // Progress bar
    m.def("print_progress_bar", &atom::utils::printProgressBar,
          py::arg("progress"), py::arg("bar_width") = 50,
          py::arg("style") = atom::utils::ProgressBarStyle::BASIC,
          "Display progress bar");

    // Table printing
    m.def("print_table", &atom::utils::printTable, py::arg("data"),
          "Print data in table format");

    // JSON printing
    m.def("print_json", &atom::utils::printJson, py::arg("json"),
          py::arg("indent") = 2, "Pretty-print JSON with indentation");

    // Bar chart
    m.def("print_bar_chart", &atom::utils::printBarChart, py::arg("data"),
          py::arg("max_width") = 50, "Print horizontal bar chart");

    // PerformanceTimer class
    py::class_<atom::utils::PerformanceTimer>(
        m, "PerformanceTimer", "High-precision performance timer")
        .def(py::init<>(), "Create a new performance timer")
        .def("reset", &atom::utils::PerformanceTimer::reset, "Reset timer")
        .def("elapsed", &atom::utils::PerformanceTimer::elapsed,
             "Get elapsed time in seconds")
        .def_static(
            "measure_void",
            [](const std::string& operation_name, py::function func) {
                atom::utils::PerformanceTimer::measureVoid(
                    operation_name, [&func]() { func(); });
            },
            py::arg("operation_name"), py::arg("func"),
            "Measure execution time of a function");

    // MathStats class
    py::class_<atom::utils::MathStats>(m, "MathStats",
                                       "Statistical analysis utilities")
        .def_static(
            "mean",
            [](const std::vector<double>& data) {
                return atom::utils::MathStats::mean(data);
            },
            py::arg("data"), "Calculate arithmetic mean")
        .def_static(
            "median",
            [](std::vector<double> data) {
                return atom::utils::MathStats::median(data);
            },
            py::arg("data"), "Calculate median")
        .def_static(
            "standard_deviation",
            [](const std::vector<double>& data) {
                return atom::utils::MathStats::standardDeviation(data);
            },
            py::arg("data"), "Calculate standard deviation");
}
