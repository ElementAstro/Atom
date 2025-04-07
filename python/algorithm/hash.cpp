// hash_bindings.cpp
#include "atom/algorithm/hash.hpp"
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(hash, m) {
    m.doc() = R"pbdoc(
        Optimized Hash Algorithms
        -----------------------

        This module provides a collection of optimized hash functions with thread-safe
        caching, parallel processing capability, and support for various data types.
        
        The module includes:
          - Standard hash functions optimized with SIMD instructions
          - Support for various hash algorithms (STD, FNV1A, etc.)
          - Utilities for combining and verifying hash values
          - Thread-safe hash caching
          - Hash computation for complex data structures
          
        Example:
            >>> from atom.algorithm import hash
            >>> 
            >>> # Compute hash of a string
            >>> h1 = hash.compute_hash("Hello, world!")
            >>> print(h1)
            
            >>> # Compute hash with a specific algorithm
            >>> h2 = hash.compute_hash("Hello, world!", hash.HashAlgorithm.FNV1A)
            >>> print(h2)
            
            >>> # Hash a list of values
            >>> h3 = hash.compute_hash([1, 2, 3, 4, 5])
            >>> print(h3)
            
            >>> # Verify if two hashes match
            >>> hash.verify_hash(h1, h2)  # False
            >>> hash.verify_hash(h1, h1)  # True
    )pbdoc";

    // Register HashAlgorithm enum
    py::enum_<atom::algorithm::HashAlgorithm>(m, "HashAlgorithm",
                                              "Available hash algorithms")
        .value("STD", atom::algorithm::HashAlgorithm::STD,
               "Standard library hash function")
        .value("FNV1A", atom::algorithm::HashAlgorithm::FNV1A,
               "FNV-1a hash algorithm")
        .value("XXHASH", atom::algorithm::HashAlgorithm::XXHASH,
               "xxHash algorithm")
        .value("CITYHASH", atom::algorithm::HashAlgorithm::CITYHASH,
               "CityHash algorithm")
        .value("MURMUR3", atom::algorithm::HashAlgorithm::MURMUR3,
               "MurmurHash3 algorithm")
        .export_values();

    // Function to compute hash of a string
    m.def(
        "compute_hash",
        [](const std::string& s, atom::algorithm::HashAlgorithm algo) {
            return atom::algorithm::computeHash(s, algo);
        },
        py::arg("value"),
        py::arg("algorithm") = atom::algorithm::HashAlgorithm::STD,
        R"pbdoc(
        Compute the hash value of a string.
        
        Args:
            value: The string value to hash
            algorithm: The hash algorithm to use (default: STD)
            
        Returns:
            The computed hash value
    )pbdoc");

    // Function to compute hash of an integer
    m.def(
        "compute_hash",
        [](int64_t value, atom::algorithm::HashAlgorithm algo) {
            return atom::algorithm::computeHash(value, algo);
        },
        py::arg("value"),
        py::arg("algorithm") = atom::algorithm::HashAlgorithm::STD,
        R"pbdoc(
        Compute the hash value of an integer.
        
        Args:
            value: The integer value to hash
            algorithm: The hash algorithm to use (default: STD)
            
        Returns:
            The computed hash value
    )pbdoc");

    // Function to compute hash of a float
    m.def(
        "compute_hash",
        [](double value, atom::algorithm::HashAlgorithm algo) {
            return atom::algorithm::computeHash(value, algo);
        },
        py::arg("value"),
        py::arg("algorithm") = atom::algorithm::HashAlgorithm::STD,
        R"pbdoc(
        Compute the hash value of a float.
        
        Args:
            value: The float value to hash
            algorithm: The hash algorithm to use (default: STD)
            
        Returns:
            The computed hash value
    )pbdoc");

    // Function to compute hash of a boolean
    m.def(
        "compute_hash",
        [](bool value, atom::algorithm::HashAlgorithm algo) {
            return atom::algorithm::computeHash(value, algo);
        },
        py::arg("value"),
        py::arg("algorithm") = atom::algorithm::HashAlgorithm::STD,
        R"pbdoc(
        Compute the hash value of a boolean.
        
        Args:
            value: The boolean value to hash
            algorithm: The hash algorithm to use (default: STD)
            
        Returns:
            The computed hash value
    )pbdoc");

    // Function to compute hash of bytes
    m.def(
        "compute_hash",
        [](py::bytes value, atom::algorithm::HashAlgorithm algo) {
            std::string str = static_cast<std::string>(value);
            return atom::algorithm::computeHash(str, algo);
        },
        py::arg("value"),
        py::arg("algorithm") = atom::algorithm::HashAlgorithm::STD,
        R"pbdoc(
        Compute the hash value of a bytes object.
        
        Args:
            value: The bytes object to hash
            algorithm: The hash algorithm to use (default: STD)
            
        Returns:
            The computed hash value
    )pbdoc");

    // Function to compute hash of a tuple
    m.def(
        "compute_hash",
        [](const py::tuple& tuple) {
            size_t result = 0;
            for (const auto& item : tuple) {
                py::object hash_obj =
                    py::module::import("builtins").attr("hash")(item);
                size_t item_hash = hash_obj.cast<size_t>();
                result = atom::algorithm::hashCombine(result, item_hash);
            }
            return result;
        },
        py::arg("value"),
        R"pbdoc(
        Compute the hash value of a tuple.
        
        Args:
            value: The tuple to hash
            
        Returns:
            The computed hash value
    )pbdoc");

    // Function to compute hash of a list
    m.def(
        "compute_hash",
        [](const py::list& list, bool parallel) {
            size_t result = 0;
            if (parallel && list.size() >= 1000) {
                // Parallel implementation for large lists
                py::gil_scoped_release release;
                std::vector<size_t> hashes(list.size());
                std::transform(
                    list.begin(), list.end(), hashes.begin(),
                    [](const py::handle& item) {
                        py::gil_scoped_acquire acquire;
                        py::object hash_obj =
                            py::module::import("builtins").attr("hash")(item);
                        return hash_obj.cast<size_t>();
                    });
                for (const auto& h : hashes) {
                    result = atom::algorithm::hashCombine(result, h);
                }
            } else {
                // Sequential implementation
                for (const auto& item : list) {
                    py::object hash_obj =
                        py::module::import("builtins").attr("hash")(item);
                    size_t item_hash = hash_obj.cast<size_t>();
                    result = atom::algorithm::hashCombine(result, item_hash);
                }
            }
            return result;
        },
        py::arg("value"), py::arg("parallel") = false,
        R"pbdoc(
        Compute the hash value of a list.
        
        Args:
            value: The list to hash
            parallel: Whether to use parallel processing for large lists (default: False)
            
        Returns:
            The computed hash value
    )pbdoc");

    // Function to compute hash of a dictionary
    m.def(
        "compute_hash",
        [](const py::dict& dict) {
            size_t result = 0;
            for (const auto& item : dict) {
                py::object key = py::reinterpret_borrow<py::object>(item.first);
                py::object value =
                    py::reinterpret_borrow<py::object>(item.second);

                py::object key_hash =
                    py::module::import("builtins").attr("hash")(key);
                py::object value_hash =
                    py::module::import("builtins").attr("hash")(value);

                size_t k_hash = key_hash.cast<size_t>();
                size_t v_hash = value_hash.cast<size_t>();

                result = atom::algorithm::hashCombine(
                    result, atom::algorithm::hashCombine(k_hash, v_hash));
            }
            return result;
        },
        py::arg("value"),
        R"pbdoc(
        Compute the hash value of a dictionary.
        
        Args:
            value: The dictionary to hash
            
        Returns:
            The computed hash value
    )pbdoc");

    // Function to compute hash of a set
    m.def(
        "compute_hash",
        [](const py::set& set) {
            size_t result = 0;
            // Sort the hash values for deterministic results
            std::vector<size_t> hashes;
            for (const auto& item : set) {
                py::object hash_obj =
                    py::module::import("builtins").attr("hash")(item);
                hashes.push_back(hash_obj.cast<size_t>());
            }
            std::sort(hashes.begin(), hashes.end());

            for (const auto& h : hashes) {
                result = atom::algorithm::hashCombine(result, h);
            }
            return result;
        },
        py::arg("value"),
        R"pbdoc(
        Compute the hash value of a set.
        
        Args:
            value: The set to hash
            
        Returns:
            The computed hash value
    )pbdoc");

    // Function to compute hash of None
    m.def(
        "compute_hash", [](const py::none&) { return static_cast<size_t>(0); },
        py::arg("value"),
        R"pbdoc(
        Compute the hash value of None.
        
        Args:
            value: None
            
        Returns:
            The hash value of None (0)
    )pbdoc");

    // Function to compute FNV-1a hash of a string
    m.def(
        "fnv1a_hash",
        [](const std::string& str, size_t basis) {
            return atom::algorithm::hash(str.c_str(), basis);
        },
        py::arg("value"), py::arg("basis") = 2166136261u,
        R"pbdoc(
        Compute the FNV-1a hash of a string.
        
        Args:
            value: The string to hash
            basis: The initial basis value (default: 2166136261)
            
        Returns:
            The computed FNV-1a hash value
    )pbdoc");

    // Function to hash combine
    m.def("hash_combine", &atom::algorithm::hashCombine, py::arg("seed"),
          py::arg("hash"),
          R"pbdoc(
        Combine two hash values into one.
        
        This function is useful for creating hash values for composite objects.
        
        Args:
            seed: The initial hash value
            hash: The hash value to combine with the seed
            
        Returns:
            The combined hash value
    )pbdoc");

    // Function to verify hash match
    m.def("verify_hash", &atom::algorithm::verifyHash, py::arg("hash1"),
          py::arg("hash2"), py::arg("tolerance") = 0,
          R"pbdoc(
        Verify if two hash values match.
        
        Args:
            hash1: The first hash value
            hash2: The second hash value
            tolerance: Allowed difference for fuzzy matching (default: 0)
            
        Returns:
            True if the hashes match within the tolerance, False otherwise
    )pbdoc");

    // String literal operator implementation (as a function)
    m.def(
        "string_hash",
        [](const std::string& str) {
            return atom::algorithm::hash(str.c_str());
        },
        py::arg("str"),
        R"pbdoc(
        Compute the hash value of a string using the FNV-1a algorithm.
        
        This is equivalent to the _hash string literal operator in C++.
        
        Args:
            str: The string to hash
            
        Returns:
            The computed hash value
    )pbdoc");

    // Cache utilities
    py::class_<atom::algorithm::HashCache<std::string>>(
        m, "StringHashCache", "Thread-safe hash cache for strings")
        .def(py::init<>())
        .def(
            "get",
            [](atom::algorithm::HashCache<std::string>& self,
               const std::string& key) -> py::object {
                auto result = self.get(key);
                if (result) {
                    return py::cast(*result);
                }
                return py::none();
            },
            py::arg("key"), "Get a cached hash value for a key if available")
        .def("set", &atom::algorithm::HashCache<std::string>::set,
             py::arg("key"), py::arg("hash"), "Set a hash value for a key")
        .def("clear", &atom::algorithm::HashCache<std::string>::clear,
             "Clear all cached values");

    // Add some useful utility functions

    // Function to generate a fast hash for a filename
    m.def(
        "filename_hash",
        [](const std::string& filename) {
            return atom::algorithm::hash(filename.c_str());
        },
        py::arg("filename"),
        R"pbdoc(
        Generate a fast hash for a filename.
        
        This is useful for creating unique identifiers for files.
        
        Args:
            filename: The filename to hash
            
        Returns:
            The computed hash value
    )pbdoc");

    // Function to benchmark different hash algorithms
    m.def(
        "benchmark_algorithms",
        [](const std::string& value, int iterations) {
            py::dict results;

            auto benchmark = [&](atom::algorithm::HashAlgorithm algo,
                                 const std::string& name) {
                py::object time = py::module::import("time");
                double start = time.attr("time")().cast<double>();

                size_t result = 0;
                for (int i = 0; i < iterations; ++i) {
                    result ^= atom::algorithm::computeHash(value, algo);
                }

                double end = time.attr("time")().cast<double>();
                double elapsed = end - start;

                results[name.c_str()] = py::make_tuple(elapsed, result);
            };

            benchmark(atom::algorithm::HashAlgorithm::STD, "STD");
            benchmark(atom::algorithm::HashAlgorithm::FNV1A, "FNV1A");

            // Other algorithms would be included here when implemented

            return results;
        },
        py::arg("value"), py::arg("iterations") = 100000,
        R"pbdoc(
        Benchmark different hash algorithms.
        
        Args:
            value: The string to hash
            iterations: Number of iterations to run (default: 100000)
            
        Returns:
            A dictionary with algorithm names as keys and tuples (time, hash_value) as values
    )pbdoc");

    // Function to compute hash distribution
    m.def(
        "analyze_hash_distribution",
        [](const py::list& values, atom::algorithm::HashAlgorithm algo) {
            std::vector<size_t> hashes;
            for (const auto& value : values) {
                if (py::isinstance<py::str>(value)) {
                    hashes.push_back(atom::algorithm::computeHash(
                        value.cast<std::string>(), algo));
                } else {
                    py::object hash_obj =
                        py::module::import("builtins").attr("hash")(value);
                    hashes.push_back(hash_obj.cast<size_t>());
                }
            }

            // Calculate distribution metrics
            double min_val = std::numeric_limits<double>::max();
            double max_val = std::numeric_limits<double>::min();
            std::unordered_map<size_t, int> collisions;

            for (const auto& h : hashes) {
                collisions[h]++;
                min_val = std::min(min_val, static_cast<double>(h));
                max_val = std::max(max_val, static_cast<double>(h));
            }

            // Count collision buckets
            int collision_count = 0;
            for (const auto& [_, count] : collisions) {
                if (count > 1) {
                    collision_count += (count - 1);
                }
            }

            py::dict results;
            results["count"] = hashes.size();
            results["min"] = min_val;
            results["max"] = max_val;
            results["range"] = max_val - min_val;
            results["collisions"] = collision_count;
            results["collision_rate"] =
                static_cast<double>(collision_count) / hashes.size();
            results["unique_hashes"] = collisions.size();

            return results;
        },
        py::arg("values"),
        py::arg("algorithm") = atom::algorithm::HashAlgorithm::STD,
        R"pbdoc(
        Analyze the distribution of hash values for a list of inputs.
        
        Args:
            values: The list of values to hash
            algorithm: The hash algorithm to use (default: STD)
            
        Returns:
            A dictionary with distribution metrics
    )pbdoc");

    // Add version information
    m.attr("__version__") = "1.0.0";
}