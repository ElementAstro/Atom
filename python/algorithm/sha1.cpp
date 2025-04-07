#include "atom/algorithm/sha1.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

// Helper function to convert byte array to Python bytes
py::bytes array_to_bytes(
    const std::array<uint8_t, atom::algorithm::SHA1::DIGEST_SIZE>& digest) {
    return py::bytes(reinterpret_cast<const char*>(digest.data()),
                     digest.size());
}

PYBIND11_MODULE(sha1, m) {
    m.doc() = R"pbdoc(
        SHA-1 Cryptographic Hash Implementation
        --------------------------------------

        This module provides a SHA-1 hash implementation conforming to FIPS PUB 180-4.
        
        The SHA1 class allows incremental updates to compute the hash of large data,
        and supports both raw byte arrays and higher-level containers as input.
        
        Note: While SHA-1 is no longer considered secure for cryptographic purposes,
        it remains useful for non-security applications like data integrity checks.

        Example:
            >>> from atom.algorithm import sha1
            >>> 
            >>> # Create a new SHA1 hash object
            >>> hasher = sha1.SHA1()
            >>> 
            >>> # Update with data
            >>> hasher.update(b"Hello")
            >>> hasher.update(b", World!")
            >>> 
            >>> # Get digest as bytes
            >>> digest_bytes = hasher.digest_bytes()
            >>> print(digest_bytes.hex())
            >>> 
            >>> # Or as a hex string
            >>> digest_str = hasher.digest_string()
            >>> print(digest_str)
            >>> 
            >>> # One-step hashing convenience function
            >>> hash_value = sha1.compute_hash("Hello, World!")
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

    // Define the SHA1 class
    py::class_<atom::algorithm::SHA1>(
        m, "SHA1",
        R"(SHA-1 hash implementation conforming to FIPS PUB 180-4.

This class computes the SHA-1 hash of a sequence of bytes and produces a 20-byte digest.
It supports incremental updates, allowing the hash of large data to be computed in chunks.

Examples:
    >>> from atom.algorithm.sha1 import SHA1
    >>> 
    >>> # Create a new hash object
    >>> hasher = SHA1()
    >>> 
    >>> # Update with data incrementally
    >>> hasher.update(b"Hello")
    >>> hasher.update(b", World!")
    >>> 
    >>> # Get the digest as a hexadecimal string
    >>> digest = hasher.digest_string()
    >>> print(digest)
    >>> 
    >>> # Reset and start a new hash
    >>> hasher.reset()
    >>> hasher.update(b"New data")
    >>> digest2 = hasher.digest_string()
)")
        .def(py::init<>(),
             "Constructs a new SHA1 object with the initial hash values.")

        // Update methods
        .def(
            "update",
            [](atom::algorithm::SHA1& self, py::bytes data) {
                std::string str = data;
                self.update(reinterpret_cast<const uint8_t*>(str.data()),
                            str.size());
            },
            py::arg("data"),
            R"(Updates the hash with a sequence of bytes.

Args:
    data: Bytes data to hash.

Examples:
    >>> hasher = SHA1()
    >>> hasher.update(b"First chunk of data")
    >>> hasher.update(b"Second chunk of data")
)")

        .def(
            "update",
            [](atom::algorithm::SHA1& self, const std::string& data) {
                self.update(reinterpret_cast<const uint8_t*>(data.data()),
                            data.size());
            },
            py::arg("data"),
            R"(Updates the hash with a string.

Args:
    data: String data to hash (will be encoded as UTF-8).

Examples:
    >>> hasher = SHA1()
    >>> hasher.update("Text data")
)")

        // Digest methods
        .def(
            "digest", [](atom::algorithm::SHA1& self) { return self.digest(); },
            "Returns the hash digest as a 20-byte array.")

        .def(
            "digest_bytes",
            [](atom::algorithm::SHA1& self) {
                return array_to_bytes(self.digest());
            },
            R"(Finalizes the hash computation and returns the digest as bytes.

Returns:
    A 20-byte Python bytes object containing the SHA-1 digest.

Examples:
    >>> hasher = SHA1()
    >>> hasher.update(b"Hello, World!")
    >>> digest = hasher.digest_bytes()
    >>> print(digest.hex())
)")

        .def(
            "digest_string", &atom::algorithm::SHA1::digestAsString,
            R"(Finalizes the hash computation and returns the digest as a hexadecimal string.

Returns:
    A string containing the hexadecimal representation of the SHA-1 digest.

Examples:
    >>> hasher = SHA1()
    >>> hasher.update(b"Hello, World!")
    >>> digest = hasher.digest_string()
    >>> print(digest)  # "2ef7bde608ce5404e97d5f042f95f89f1c232871"
)")

        .def("reset", &atom::algorithm::SHA1::reset,
             R"(Resets the SHA1 object to its initial state.

This method clears the internal buffer and resets the hash state to allow
for hashing new data.

Examples:
    >>> hasher = SHA1()
    >>> hasher.update(b"Some data")
    >>> # Decide to start over
    >>> hasher.reset()
    >>> hasher.update(b"New data")
)")

        // Constants
        .def_readonly_static("DIGEST_SIZE", &atom::algorithm::SHA1::DIGEST_SIZE,
                             "The size of the SHA-1 digest in bytes (20).");

    // Utility functions
    m.def("bytes_to_hex",
          &atom::algorithm::bytesToHex<atom::algorithm::SHA1::DIGEST_SIZE>,
          py::arg("bytes"),
          R"(Converts a 20-byte array to a hexadecimal string.

Args:
    bytes: A 20-byte array to convert.

Returns:
    A string containing the hexadecimal representation of the byte array.

Examples:
    >>> hasher = SHA1()
    >>> hasher.update(b"Hello, World!")
    >>> digest = hasher.digest()
    >>> hex_str = bytes_to_hex(digest)
    >>> print(hex_str)
)");

    // Convenience functions
    m.def(
        "compute_hash",
        [](py::bytes data) {
            atom::algorithm::SHA1 hasher;
            std::string str = data;
            hasher.update(reinterpret_cast<const uint8_t*>(str.data()),
                          str.size());
            return hasher.digest();
        },
        py::arg("data"),
        R"(Computes the SHA-1 hash of the input bytes in a single operation.

Args:
    data: Bytes data to hash.

Returns:
    A 20-byte array containing the SHA-1 digest.

Examples:
    >>> from atom.algorithm.sha1 import compute_hash
    >>> digest = compute_hash(b"Hello, World!")
    >>> print(bytes_to_hex(digest))
)");

    m.def(
        "compute_hash_string",
        [](py::bytes data) {
            atom::algorithm::SHA1 hasher;
            std::string str = data;
            hasher.update(reinterpret_cast<const uint8_t*>(str.data()),
                          str.size());
            return hasher.digestAsString();
        },
        py::arg("data"),
        R"(Computes the SHA-1 hash of the input bytes and returns it as a hexadecimal string.

Args:
    data: Bytes data to hash.

Returns:
    A string containing the hexadecimal representation of the SHA-1 digest.

Examples:
    >>> from atom.algorithm.sha1 import compute_hash_string
    >>> hash_str = compute_hash_string(b"Hello, World!")
    >>> print(hash_str)  # "2ef7bde608ce5404e97d5f042f95f89f1c232871"
)");

    m.def(
        "compute_hash_string",
        [](const std::string& data) {
            atom::algorithm::SHA1 hasher;
            hasher.update(reinterpret_cast<const uint8_t*>(data.data()),
                          data.size());
            return hasher.digestAsString();
        },
        py::arg("data"),
        R"(Computes the SHA-1 hash of the input string and returns it as a hexadecimal string.

Args:
    data: String data to hash (will be encoded as UTF-8).

Returns:
    A string containing the hexadecimal representation of the SHA-1 digest.

Examples:
    >>> from atom.algorithm.sha1 import compute_hash_string
    >>> hash_str = compute_hash_string("Hello, World!")
    >>> print(hash_str)  # "2ef7bde608ce5404e97d5f042f95f89f1c232871"
)");

    m.def(
        "compute_hash_bytes",
        [](py::bytes data) {
            atom::algorithm::SHA1 hasher;
            std::string str = data;
            hasher.update(reinterpret_cast<const uint8_t*>(str.data()),
                          str.size());
            return array_to_bytes(hasher.digest());
        },
        py::arg("data"),
        R"(Computes the SHA-1 hash of the input bytes and returns it as bytes.

Args:
    data: Bytes data to hash.

Returns:
    A 20-byte Python bytes object containing the SHA-1 digest.

Examples:
    >>> from atom.algorithm.sha1 import compute_hash_bytes
    >>> hash_bytes = compute_hash_bytes(b"Hello, World!")
    >>> print(hash_bytes.hex())
)");

    m.def(
        "compute_hash_bytes",
        [](const std::string& data) {
            atom::algorithm::SHA1 hasher;
            hasher.update(reinterpret_cast<const uint8_t*>(data.data()),
                          data.size());
            return array_to_bytes(hasher.digest());
        },
        py::arg("data"),
        R"(Computes the SHA-1 hash of the input string and returns it as bytes.

Args:
    data: String data to hash (will be encoded as UTF-8).

Returns:
    A 20-byte Python bytes object containing the SHA-1 digest.

Examples:
    >>> from atom.algorithm.sha1 import compute_hash_bytes
    >>> hash_bytes = compute_hash_bytes("Hello, World!")
    >>> print(hash_bytes.hex())
)");

    // Parallel hash computation function (with simplified interface for Python)
    m.def(
        "compute_hashes_parallel",
        [](const std::vector<py::bytes>& data_list) {
            // Convert py::bytes to ByteContainers
            std::vector<std::string> string_data;
            string_data.reserve(data_list.size());

            for (const auto& data : data_list) {
                string_data.push_back(static_cast<std::string>(data));
            }

            // Variadic template expansion is not directly applicable here, so
            // we'll use a custom approach
            std::vector<std::array<uint8_t, atom::algorithm::SHA1::DIGEST_SIZE>>
                results;
            results.reserve(string_data.size());

#pragma omp parallel for
            for (size_t i = 0; i < string_data.size(); ++i) {
                atom::algorithm::SHA1 hasher;
                hasher.update(
                    reinterpret_cast<const uint8_t*>(string_data[i].data()),
                    string_data[i].size());

#pragma omp critical
                results.push_back(hasher.digest());
            }

            return results;
        },
        py::arg("data_list"),
        R"(Computes SHA-1 hashes of multiple data items in parallel.

Args:
    data_list: A list of bytes objects to hash.

Returns:
    A list of 20-byte arrays, each containing the SHA-1 digest of the corresponding input.

Examples:
    >>> from atom.algorithm.sha1 import compute_hashes_parallel
    >>> data = [b"First data", b"Second data", b"Third data"]
    >>> hashes = compute_hashes_parallel(data)
    >>> # Print the hash of the first item
    >>> print(bytes_to_hex(hashes[0]))
)");
}