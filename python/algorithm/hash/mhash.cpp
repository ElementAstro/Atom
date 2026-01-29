#include "atom/algorithm/mhash.hpp"

#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Helper function to convert uint8_t array to Python bytes
py::bytes array_to_bytes(
    const std::array<uint8_t, atom::algorithm::K_HASH_SIZE>& hash) {
    return py::bytes(reinterpret_cast<const char*>(hash.data()), hash.size());
}

// Helper function to convert Keccak hash to hex string
std::string hash_to_hex(
    const std::array<uint8_t, atom::algorithm::K_HASH_SIZE>& hash) {
    return atom::algorithm::hexstringFromData(std::string_view(
        reinterpret_cast<const char*>(hash.data()), hash.size()));
}

PYBIND11_MODULE(mhash, m) {
    m.doc() = R"pbdoc(
        Optimized Hashing Algorithms
        ---------------------------

        This module provides implementation of MinHash for similarity estimation
        and Keccak-256 cryptographic hash functions.

        The module includes:
          - MinHash implementation for estimating Jaccard similarity between sets
          - Keccak-256 cryptographic hash function (compatible with Ethereum's keccak256)
          - Utility functions for hex string conversion

        Example:
            >>> from atom.algorithm import mhash
            >>>
            >>> # Computing Keccak-256 hash
            >>> h = mhash.keccak256("Hello, world!")
            >>> print(mhash.hash_to_hex(h))

            >>> # Using MinHash for similarity estimation
            >>> minhash = mhash.MinHash(100)  # 100 hash functions
            >>> sig1 = minhash.compute_signature(["a", "b", "c", "d"])
            >>> sig2 = minhash.compute_signature(["c", "d", "e", "f"])
            >>> similarity = mhash.MinHash.jaccard_index(sig1, sig2)
            >>> print(f"Estimated Jaccard similarity: {similarity}")
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

    // MinHash class binding
    py::class_<atom::algorithm::MinHash>(
        m, "MinHash",
        R"(Implementation of MinHash algorithm for estimating Jaccard similarity.

The MinHash algorithm generates hash signatures for sets and estimates the
Jaccard index (similarity) between sets based on these signatures.

Args:
    num_hashes: The number of hash functions to use for MinHash.

Examples:
    >>> from atom.algorithm.mhash import MinHash
    >>> # Create a MinHash with 100 hash functions
    >>> minhash = MinHash(100)
    >>> # Compute signatures for two sets
    >>> sig1 = minhash.compute_signature(["apple", "orange", "banana", "pear"])
    >>> sig2 = minhash.compute_signature(["apple", "orange", "grape", "lemon"])
    >>> # Estimate similarity
    >>> similarity = MinHash.jaccard_index(sig1, sig2)
    >>> print(f"Estimated similarity: {similarity}")
)")
        .def(py::init<size_t>(), py::arg("num_hashes"),
             "Constructs a MinHash object with a specified number of hash "
             "functions.")
        .def(
            "compute_signature",
            [](const atom::algorithm::MinHash& self,
               const std::vector<std::string>& set) {
                return self.computeSignature(set);
            },
            py::arg("set"),
            R"(Computes the MinHash signature (hash values) for a given set.

Args:
    set: The set of elements for which to compute the MinHash signature.

Returns:
    List of hash values representing the MinHash signature for the set.
)")
        .def(
            "compute_signature",
            [](const atom::algorithm::MinHash& self,
               const std::vector<int>& set) {
                return self.computeSignature(set);
            },
            py::arg("set"),
            R"(Computes the MinHash signature (hash values) for a given set of integers.

Args:
    set: The set of integers for which to compute the MinHash signature.

Returns:
    List of hash values representing the MinHash signature for the set.
)")
        .def(
            "compute_signature",
            [](const atom::algorithm::MinHash& self,
               const std::set<std::string>& set) {
                return self.computeSignature(set);
            },
            py::arg("set"),
            R"(Computes the MinHash signature (hash values) for a given set.

Args:
    set: The set of elements for which to compute the MinHash signature.

Returns:
    List of hash values representing the MinHash signature for the set.
)")
        .def_static(
            "jaccard_index", &atom::algorithm::MinHash::jaccardIndex,
            py::arg("sig1"), py::arg("sig2"),
            R"(Computes the Jaccard index between two sets based on their MinHash signatures.

Args:
    sig1: MinHash signature of the first set.
    sig2: MinHash signature of the second set.

Returns:
    Estimated Jaccard index (similarity) between the two sets, a value between 0.0 and 1.0.

Raises:
    ValueError: If the signatures have different lengths.
)");

    // Keccak-256 hash function binding
    m.def(
        "keccak256",
        [](const std::string& input) {
            return atom::algorithm::keccak256(input);
        },
        py::arg("input"),
        R"(Computes the Keccak-256 hash of the input string.

Args:
    input: Input string to hash.

Returns:
    The computed hash as a bytes object of 32 bytes.
)");

    m.def(
        "keccak256",
        [](py::bytes input) {
            std::string str = input;
            return atom::algorithm::keccak256(str);
        },
        py::arg("input"),
        R"(Computes the Keccak-256 hash of the input bytes.

Args:
    input: Input bytes to hash.

Returns:
    The computed hash as a bytes object of 32 bytes.
)");

    // Hex string conversion utilities
    m.def("hex_to_bytes", &atom::algorithm::dataFromHexstring,
          py::arg("hex_string"),
          R"(Converts a hexadecimal string to binary data.

Args:
    hex_string: Hexadecimal string to convert.

Returns:
    Binary data as bytes.

Raises:
    ValueError: If the input is not a valid hexadecimal string.
)");

    m.def("bytes_to_hex", &atom::algorithm::hexstringFromData, py::arg("data"),
          R"(Converts binary data to a hexadecimal string.

Args:
    data: Binary data to convert.

Returns:
    Hexadecimal string representation.
)");

    m.def("hash_to_hex", &hash_to_hex, py::arg("hash"),
          R"(Converts a Keccak hash to a hexadecimal string.

Args:
    hash: The Keccak hash to convert.

Returns:
    Hexadecimal string representation of the hash.
)");

    m.def("hash_to_bytes", &array_to_bytes, py::arg("hash"),
          R"(Converts a Keccak hash to Python bytes.

Args:
    hash: The Keccak hash to convert.

Returns:
    The hash as Python bytes.
)");

    m.def("supports_hex_string_conversion",
          &atom::algorithm::supportsHexStringConversion, py::arg("string"),
          R"(Checks if a string can be converted from/to hex.

Args:
    string: The string to check.

Returns:
    True if the string can be converted, False otherwise.
)");

    // Constants
    m.attr("HASH_SIZE") = atom::algorithm::K_HASH_SIZE;
}
