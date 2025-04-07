#include "atom/algorithm/md5.hpp"

#include <pybind11/buffer_info.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(md5, m) {
    m.doc() = R"pbdoc(
        MD5 Hashing Algorithm
        ---------------------

        This module provides a modern, optimized implementation of the MD5 hashing algorithm
        with additional utility functions and binary data support.
        
        Example:
            >>> from atom.algorithm import md5
            >>> 
            >>> # Compute MD5 hash of a string
            >>> hash_value = md5.encrypt("Hello, world!")
            >>> print(hash_value)
            '6cd3556deb0da54bca060b4c39479839'
            
            >>> # Verify a hash
            >>> md5.verify("Hello, world!", hash_value)
            True
            
            >>> # Compute hash of binary data
            >>> import os
            >>> binary_data = os.urandom(1024)
            >>> binary_hash = md5.encrypt_binary(binary_data)
    )pbdoc";

    // Register MD5Exception
    py::register_exception<atom::algorithm::MD5Exception>(m, "MD5Exception",
                                                          PyExc_RuntimeError);

    // Main static functions
    m.def("encrypt", &atom::algorithm::MD5::encrypt<std::string>,
          py::arg("input"),
          R"pbdoc(
        Encrypts a string using the MD5 algorithm.
        
        Args:
            input: The input string to hash
            
        Returns:
            The MD5 hash as a lowercase hex string
            
        Raises:
            MD5Exception: If encryption fails
    )pbdoc");

    m.def("verify", &atom::algorithm::MD5::verify<std::string>,
          py::arg("input"), py::arg("hash"),
          R"pbdoc(
        Verifies if a string matches a given MD5 hash.
        
        Args:
            input: The input string to check
            hash: The expected MD5 hash
            
        Returns:
            True if the hash of input matches the expected hash, False otherwise
    )pbdoc");

    // Binary data support
    m.def(
        "encrypt_binary",
        [](py::buffer data) {
            py::buffer_info info = data.request();
            auto* data_ptr = reinterpret_cast<const std::byte*>(info.ptr);
            std::span<const std::byte> data_span(data_ptr, info.size);

            try {
                return atom::algorithm::MD5::encryptBinary(data_span);
            } catch (const atom::algorithm::MD5Exception& e) {
                throw py::value_error(e.what());
            }
        },
        py::arg("data"),
        R"pbdoc(
        Computes MD5 hash for binary data.
        
        Args:
            data: Binary data (bytes, bytearray, or any buffer-like object)
            
        Returns:
            The MD5 hash as a lowercase hex string
            
        Raises:
            ValueError: If encryption fails
    )pbdoc");

    // Add version information
    m.attr("__version__") = "1.0.0";
}