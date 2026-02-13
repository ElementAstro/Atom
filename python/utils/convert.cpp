#include "atom/utils/conversion/convert.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(convert, m) {
    m.doc() = R"pbdoc(
        Windows String Conversion Utilities Module
        -------------------------------------------

        This module provides Windows-specific string conversion utilities.
        These functions are only available on Windows platforms.

        Note: This module is Windows-only and will not be available on other platforms.

        Examples:
            >>> from atom.utils import convert
            >>> # Convert string to LPWSTR
            >>> lpwstr = convert.string_to_lpwstr("Hello")
    )pbdoc";

#ifdef _WIN32
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

    // String to LPWSTR conversions
    m.def(
        "char_to_lpwstr",
        [](const std::string& str) -> py::bytes {
            LPWSTR result = atom::utils::CharToLPWSTR(str);
            // Convert LPWSTR to bytes for Python
            int len = WideCharToMultiByte(CP_UTF8, 0, result, -1, nullptr, 0,
                                          nullptr, nullptr);
            std::string utf8_str(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, result, -1, &utf8_str[0], len,
                                nullptr, nullptr);
            return py::bytes(utf8_str);
        },
        py::arg("str"), "Convert string to LPWSTR (returned as bytes)");

    m.def(
        "wchar_array_to_string",
        [](py::bytes wchar_bytes) -> std::string {
            // This is a simplified version - actual implementation would need
            // proper WCHAR* handling
            std::string str = wchar_bytes;
            return atom::utils::WCharArrayToString(
                reinterpret_cast<const WCHAR*>(str.c_str()));
        },
        py::arg("wchar_array"), "Convert WCHAR array to string");

    m.def(
        "string_to_lpstr",
        [](const std::string& str) -> std::string {
            LPSTR result = atom::utils::StringToLPSTR(str);
            std::string ret(result);
            delete[] result;
            return ret;
        },
        py::arg("str"), "Convert string to LPSTR");

    m.def(
        "wstring_to_lpstr",
        [](const std::wstring& wstr) -> std::string {
            LPSTR result = atom::utils::WStringToLPSTR(wstr);
            std::string ret(result);
            delete[] result;
            return ret;
        },
        py::arg("wstr"), "Convert wide string to LPSTR");

    m.def(
        "string_to_lpwstr",
        [](const std::string& str) -> py::bytes {
            LPWSTR result = atom::utils::StringToLPWSTR(str);
            int len = WideCharToMultiByte(CP_UTF8, 0, result, -1, nullptr, 0,
                                          nullptr, nullptr);
            std::string utf8_str(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, result, -1, &utf8_str[0], len,
                                nullptr, nullptr);
            delete[] result;
            return py::bytes(utf8_str);
        },
        py::arg("str"), "Convert string to LPWSTR (returned as bytes)");

    m.def(
        "lpwstr_to_string",
        [](py::bytes lpwstr_bytes) -> std::string {
            // Simplified - actual implementation needs proper LPWSTR handling
            return std::string(lpwstr_bytes);
        },
        py::arg("lpwstr"), "Convert LPWSTR to string");

    m.def(
        "lpcwstr_to_string",
        [](py::bytes lpcwstr_bytes) -> std::string {
            // Simplified - actual implementation needs proper LPCWSTR handling
            return std::string(lpcwstr_bytes);
        },
        py::arg("lpcwstr"), "Convert LPCWSTR to string");

    m.def(
        "wstring_to_lpwstr",
        [](const std::wstring& wstr) -> py::bytes {
            LPWSTR result = atom::utils::WStringToLPWSTR(wstr);
            int len = WideCharToMultiByte(CP_UTF8, 0, result, -1, nullptr, 0,
                                          nullptr, nullptr);
            std::string utf8_str(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, result, -1, &utf8_str[0], len,
                                nullptr, nullptr);
            delete[] result;
            return py::bytes(utf8_str);
        },
        py::arg("wstr"), "Convert wide string to LPWSTR (returned as bytes)");

    m.def(
        "lpwstr_to_wstring",
        [](py::bytes lpwstr_bytes) -> std::wstring {
            // Simplified - actual implementation needs proper LPWSTR handling
            std::string str(lpwstr_bytes);
            return std::wstring(str.begin(), str.end());
        },
        py::arg("lpwstr"), "Convert LPWSTR to wide string");

    m.def(
        "lpcwstr_to_wstring",
        [](py::bytes lpcwstr_bytes) -> std::wstring {
            // Simplified - actual implementation needs proper LPCWSTR handling
            std::string str(lpcwstr_bytes);
            return std::wstring(str.begin(), str.end());
        },
        py::arg("lpcwstr"), "Convert LPCWSTR to wide string");

#else
    m.def("_not_available", []() {
        throw std::runtime_error(
            "Windows conversion utilities are only available on Windows");
    });
#endif
}
