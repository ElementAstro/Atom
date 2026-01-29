/*
 * web.cpp
 *
 * Main Python binding module for atom::web
 * This file aggregates all web-related submodules
 */

#include <pybind11/pybind11.h>

namespace py = pybind11;

// Forward declarations for submodule initialization functions
void init_address(py::module_& parent);
void init_curl(py::module_& parent);
void init_downloader(py::module_& parent);
void init_httpparser(py::module_& parent);
void init_mimetype(py::module_& parent);
void init_time(py::module_& parent);
void init_utils(py::module_& parent);

PYBIND11_MODULE(atom_web, m) {
    m.doc() = R"pbdoc(
        Atom Web Module
        ===============

        Comprehensive web utilities for the Atom framework, including:

        - **address**: Network address handling (IPv4, IPv6, Unix Domain)
        - **http**: HTTP client functionality (curl, downloader, parser)
        - **mime**: MIME type detection and management
        - **time**: Time synchronization and management (NTP, RTC, timezone)
        - **utils**: Network utilities (DNS, port scanning, connectivity)

        Examples
        --------
        >>> from atom import web
        >>> # Parse an IPv4 address
        >>> addr = web.IPv4("192.168.1.1/24")
        >>> print(addr.get_network_address())

        >>> # Make an HTTP request
        >>> curl = web.CurlWrapper()
        >>> curl.set_url("https://api.example.com/data")
        >>> response = curl.perform()

        >>> # Guess MIME type
        >>> mime_db = web.MimeTypes()
        >>> mime_type = mime_db.guess_type("document.pdf")
        >>> print(mime_type)  # 'application/pdf'
    )pbdoc";

    // Initialize all submodules
    init_address(m);
    init_curl(m);
    init_downloader(m);
    init_httpparser(m);
    init_mimetype(m);
    init_time(m);
    init_utils(m);
}
