#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/algorithm/base.hpp"

namespace py = pybind11;

PYBIND11_MODULE(base, m) {
    m.doc() = R"pbdoc(
        Base Encoding/Decoding Algorithms
        ---------------------------------

        This module provides functions for encoding and decoding data in various formats:
        - Base32 encoding and decoding
        - Base64 encoding and decoding
        - XOR encryption and decryption

        Examples:
            >>> import atom.algorithm.base as base
            >>> base.base64_encode("Hello, world!")
            'SGVsbG8sIHdvcmxkIQ=='
            >>> base.base64_decode('SGVsbG8sIHdvcmxkIQ==')
            'Hello, world!'
    )pbdoc";

    // Register exception translation
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

    // Base32 encoding/decoding
    m.def(
        "encode_base32",
        [](py::bytes data) {
            std::string_view view(PyBytes_AsString(data.ptr()),
                                  PyBytes_Size(data.ptr()));
            std::span<const uint8_t> span(
                reinterpret_cast<const uint8_t*>(view.data()), view.size());

            auto result = atom::algorithm::encodeBase32(span);
            if (result) {
                return result.value();
            } else {
                throw py::value_error(result.error().error());
            }
        },
        py::arg("data"), R"pbdoc(
        Encode binary data using Base32.

        Args:
            data (bytes): The binary data to encode.

        Returns:
            str: The Base32 encoded string.

        Raises:
            ValueError: If encoding fails.

        Example:
            >>> encode_base32(b'hello')
            'NBSWY3DP'
    )pbdoc");

    m.def(
        "decode_base32",
        [](const std::string& encoded) {
            auto result = atom::algorithm::decodeBase32(encoded);
            if (result) {
                return py::bytes(
                    reinterpret_cast<const char*>(result.value().data()),
                    result.value().size());
            } else {
                throw py::value_error(result.error().error());
            }
        },
        py::arg("encoded"), R"pbdoc(
        Decode a Base32 encoded string back to binary data.

        Args:
            encoded (str): The Base32 encoded string.

        Returns:
            bytes: The decoded binary data.

        Raises:
            ValueError: If decoding fails.

        Example:
            >>> decode_base32('NBSWY3DP')
            b'hello'
    )pbdoc");

    // Base64 encoding/decoding
    m.def(
        "base64_encode",
        [](const std::string& input, bool padding) {
            auto result = atom::algorithm::base64Encode(input, padding);
            if (result) {
                return result.value();
            } else {
                throw py::value_error(result.error().error());
            }
        },
        py::arg("input"), py::arg("padding") = true, R"pbdoc(
        Encode a string using Base64.

        Args:
            input (str): The string to encode.
            padding (bool, optional): Whether to add padding characters. Defaults to True.

        Returns:
            str: The Base64 encoded string.

        Raises:
            ValueError: If encoding fails.

        Example:
            >>> base64_encode("hello")
            'aGVsbG8='
            >>> base64_encode("hello", padding=False)
            'aGVsbG8'
    )pbdoc");

    m.def(
        "base64_decode",
        [](const std::string& input) {
            auto result = atom::algorithm::base64Decode(input);
            if (result) {
                return result.value();
            } else {
                throw py::value_error(result.error().error());
            }
        },
        py::arg("input"), R"pbdoc(
        Decode a Base64 encoded string.

        Args:
            input (str): The Base64 encoded string.

        Returns:
            str: The decoded string.

        Raises:
            ValueError: If decoding fails.

        Example:
            >>> base64_decode('aGVsbG8=')
            'hello'
    )pbdoc");

    // XOR encryption/decryption
    m.def("xor_encrypt", &atom::algorithm::xorEncrypt, py::arg("plaintext"),
          py::arg("key"),
          R"pbdoc(
        Encrypt a string using XOR algorithm.

        Args:
            plaintext (str): The string to encrypt.
            key (int): The encryption key (0-255).

        Returns:
            str: The encrypted string.

        Example:
            >>> encrypted = xor_encrypt("hello", 42)
            >>> # Result is binary data
    )pbdoc");

    m.def("xor_decrypt", &atom::algorithm::xorDecrypt, py::arg("ciphertext"),
          py::arg("key"),
          R"pbdoc(
        Decrypt a string using XOR algorithm.

        Args:
            ciphertext (str): The encrypted string.
            key (int): The decryption key (0-255).

        Returns:
            str: The decrypted string.

        Example:
            >>> encrypted = xor_encrypt("hello", 42)
            >>> xor_decrypt(encrypted, 42)
            'hello'
    )pbdoc");

    // Base64 validation
    m.def("is_base64", &atom::algorithm::isBase64, py::arg("str"),
          R"pbdoc(
        Check if a string is a valid Base64 encoded string.

        Args:
            str (str): The string to validate.

        Returns:
            bool: True if the string is valid Base64, False otherwise.

        Example:
            >>> is_base64('aGVsbG8=')
            True
            >>> is_base64('not base64')
            False
    )pbdoc");

    // Binary versions for working with bytes
    m.def(
        "base64_encode_binary",
        [](py::bytes input, bool padding) {
            std::string_view view(PyBytes_AsString(input.ptr()),
                                  PyBytes_Size(input.ptr()));
            auto result = atom::algorithm::base64Encode(view, padding);
            if (result) {
                return result.value();
            } else {
                throw py::value_error(result.error().error());
            }
        },
        py::arg("input"), py::arg("padding") = true, R"pbdoc(
        Encode binary data using Base64.

        Args:
            input (bytes): The binary data to encode.
            padding (bool, optional): Whether to add padding characters. Defaults to True.

        Returns:
            str: The Base64 encoded string.

        Example:
            >>> base64_encode_binary(b'\x00\x01\x02\x03')
            'AAECAw=='
    )pbdoc");

    m.def(
        "base64_decode_binary",
        [](const std::string& input) {
            auto result = atom::algorithm::base64Decode(input);
            if (result) {
                return py::bytes(result.value());
            } else {
                throw py::value_error(result.error().error());
            }
        },
        py::arg("input"), R"pbdoc(
        Decode a Base64 encoded string to binary data.

        Args:
            input (str): The Base64 encoded string.

        Returns:
            bytes: The decoded binary data.

        Example:
            >>> base64_decode_binary('AAECAw==')
            b'\x00\x01\x02\x03'
    )pbdoc");

    // Compatibility with Python's standard base64 module
    m.def(
        "b64encode",
        [](py::bytes input, bool padding) {
            std::string_view view(PyBytes_AsString(input.ptr()),
                                  PyBytes_Size(input.ptr()));
            auto result = atom::algorithm::base64Encode(view, padding);
            if (result) {
                return py::bytes(result.value());
            } else {
                throw py::value_error(result.error().error());
            }
        },
        py::arg("input"), py::arg("padding") = true, R"pbdoc(
        Encode binary data using Base64 (returns bytes).

        This function matches the API of Python's `base64.b64encode`.

        Args:
            input (bytes): The binary data to encode.
            padding (bool, optional): Whether to add padding characters. Defaults to True.

        Returns:
            bytes: The Base64 encoded data as bytes.
    )pbdoc");

    m.def(
        "b64decode",
        [](py::bytes input) {
            std::string_view view(PyBytes_AsString(input.ptr()),
                                  PyBytes_Size(input.ptr()));
            auto result = atom::algorithm::base64Decode(view);
            if (result) {
                return py::bytes(result.value());
            } else {
                throw py::value_error(result.error().error());
            }
        },
        py::arg("input"), R"pbdoc(
        Decode Base64 encoded data (accepts bytes).

        This function matches the API of Python's `base64.b64decode`.

        Args:
            input (bytes): The Base64 encoded data.

        Returns:
            bytes: The decoded binary data.
    )pbdoc");

    // Support for string inputs in b64decode
    m.def(
        "b64decode",
        [](const std::string& input) {
            auto result = atom::algorithm::base64Decode(input);
            if (result) {
                return py::bytes(result.value());
            } else {
                throw py::value_error(result.error().error());
            }
        },
        py::arg("input"), "Decode Base64 encoded string to bytes");

    // Binary versions of XOR functions
    m.def(
        "xor_encrypt_bytes",
        [](py::bytes plaintext, uint8_t key) {
            std::string_view view(PyBytes_AsString(plaintext.ptr()),
                                  PyBytes_Size(plaintext.ptr()));
            std::string result = atom::algorithm::xorEncrypt(view, key);
            return py::bytes(result);
        },
        py::arg("plaintext"), py::arg("key"), R"pbdoc(
        Encrypt binary data using XOR algorithm.

        Args:
            plaintext (bytes): The binary data to encrypt.
            key (int): The encryption key (0-255).

        Returns:
            bytes: The encrypted data.
    )pbdoc");

    m.def(
        "xor_decrypt_bytes",
        [](py::bytes ciphertext, uint8_t key) {
            std::string_view view(PyBytes_AsString(ciphertext.ptr()),
                                  PyBytes_Size(ciphertext.ptr()));
            std::string result = atom::algorithm::xorDecrypt(view, key);
            return py::bytes(result);
        },
        py::arg("ciphertext"), py::arg("key"), R"pbdoc(
        Decrypt binary data using XOR algorithm.

        Args:
            ciphertext (bytes): The encrypted data.
            key (int): The decryption key (0-255).

        Returns:
            bytes: The decrypted data.
    )pbdoc");

    // Parallel processing capability
    m.def(
        "parallel_process",
        [](py::bytes data, size_t thread_count, py::function func) {
            // Get data as bytes
            std::string_view view(PyBytes_AsString(data.ptr()),
                                  PyBytes_Size(data.ptr()));
            std::vector<uint8_t> input_data(view.begin(), view.end());

            // Create output buffer
            std::vector<uint8_t> result_data = input_data;

            // Process data in parallel
            std::span<uint8_t> span(result_data);

            atom::algorithm::parallelExecute(
                span, thread_count, [&func](std::span<uint8_t> chunk) {
                    // Convert chunk to Python bytes
                    py::gil_scoped_acquire acquire;
                    try {
                        py::bytes py_chunk(
                            reinterpret_cast<const char*>(chunk.data()),
                            chunk.size());
                        py::bytes result = func(py_chunk);

                        // Copy result back if size matches
                        if (static_cast<size_t>(PyBytes_Size(result.ptr())) ==
                            chunk.size()) {
                            std::string_view result_view(
                                PyBytes_AsString(result.ptr()),
                                PyBytes_Size(result.ptr()));
                            std::copy(result_view.begin(), result_view.end(),
                                      chunk.begin());
                        }
                    } catch (py::error_already_set& e) {
                        throw std::runtime_error(
                            std::string("Python callback error: ") + e.what());
                    }
                });

            return py::bytes(reinterpret_cast<const char*>(result_data.data()),
                             result_data.size());
        },
        py::arg("data"), py::arg("thread_count") = 0, py::arg("func"), R"pbdoc(
        Process binary data in parallel across multiple threads.

        Args:
            data (bytes): The binary data to process.
            thread_count (int, optional): Number of threads to use. Default is 0 (auto).
            func (callable): Function that processes a chunk of data.
                             Should accept and return bytes objects of the same size.

        Returns:
            bytes: The processed data.

        Example:
            >>> def process_chunk(chunk):
            ...     return bytes(b ^ 42 for b in chunk)
            >>> parallel_process(b'hello world', 2, process_chunk)
            b'*\x0f\x16\x16\x17K\x04\x17\x03\x16\x0e'
    )pbdoc");
}
