// snowflake_bindings.cpp
#include "atom/algorithm/snowflake.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Constant epoch for Snowflake IDs (e.g., Jan 1, 2020)
constexpr uint64_t DEFAULT_TWEPOCH = 1577836800000;

// Helper function to wrap Statistics structure
py::dict convert_statistics_to_dict(
    const atom::algorithm::Snowflake<DEFAULT_TWEPOCH>::Statistics& stats) {
    py::dict result;
    result["total_ids_generated"] = stats.total_ids_generated;
    result["sequence_rollovers"] = stats.sequence_rollovers;
    result["timestamp_wait_count"] = stats.timestamp_wait_count;
    return result;
}

PYBIND11_MODULE(snowflake, m) {
    m.doc() = R"pbdoc(
        Snowflake ID Generator
        -----------------------

        This module provides a distributed ID generator based on Twitter's Snowflake algorithm.
        
        The Snowflake algorithm generates 64-bit unique IDs that are:
          - Time-based (roughly sortable by generation time)
          - Distributed (different workers/datacenter IDs produce different ranges)
          - High-performance (can generate thousands of IDs per second per node)
          
        The generated IDs are composed of:
          - Timestamp (milliseconds since a custom epoch)
          - Datacenter ID (5 bits)
          - Worker ID (5 bits)
          - Sequence number (12 bits, for multiple IDs in the same millisecond)
          
        Example:
            >>> from atom.algorithm import snowflake
            >>> 
            >>> # Create a generator with worker_id=1, datacenter_id=2
            >>> generator = snowflake.SnowflakeGenerator(1, 2)
            >>> 
            >>> # Generate a single ID
            >>> id = generator.next_id()
            >>> print(id)
            
            >>> # Generate multiple IDs at once
            >>> ids = generator.next_ids(5)  # Generate 5 IDs
            >>> print(ids)
            
            >>> # Extract timestamp from an ID
            >>> timestamp = generator.extract_timestamp(id)
            >>> print(timestamp)
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::algorithm::InvalidWorkerIdException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::algorithm::InvalidDatacenterIdException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::algorithm::InvalidTimestampException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const atom::algorithm::SnowflakeException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Define the Python class
    py::class_<atom::algorithm::Snowflake<DEFAULT_TWEPOCH, std::mutex>>(
        m, "SnowflakeGenerator",
        R"(Distributed unique ID generator based on Twitter's Snowflake algorithm.

The Snowflake algorithm generates 64-bit IDs composed of:
  - 41 bits for time in milliseconds (gives ~69 years of IDs)
  - 5 bits for a datacenter ID
  - 5 bits for a worker ID
  - 12 bits for a sequence number (for IDs generated in the same millisecond)

Args:
    worker_id: ID of the worker generating the IDs (0-31)
    datacenter_id: ID of the datacenter (0-31)

Examples:
    >>> generator = SnowflakeGenerator(1, 2)
    >>> id = generator.next_id()
)")
        .def(py::init<uint64_t, uint64_t>(), py::arg("worker_id") = 0,
             py::arg("datacenter_id") = 0,
             "Constructs a SnowflakeGenerator with the specified worker and "
             "datacenter IDs.")
        .def("init",
             &atom::algorithm::Snowflake<DEFAULT_TWEPOCH, std::mutex>::init,
             py::arg("worker_id"), py::arg("datacenter_id"),
             "Reinitializes the generator with new worker and datacenter IDs.")
        .def(
            "next_id",
            [](atom::algorithm::Snowflake<DEFAULT_TWEPOCH, std::mutex>& self) {
                return self.template nextid<1>()[0];
            },
            "Generates a single unique ID.")
        .def(
            "next_ids",
            [](atom::algorithm::Snowflake<DEFAULT_TWEPOCH, std::mutex>& self,
               size_t count) {
                if (count <= 0) {
                    throw py::value_error("Count must be greater than zero");
                }

                if (count == 1) {
                    auto result = self.template nextid<1>();
                    return std::vector<uint64_t>{result[0]};
                } else if (count <= 10) {
                    std::vector<uint64_t> result;
                    result.reserve(count);

                    if (count == 2) {
                        auto arr = self.template nextid<2>();
                        result.assign(arr.begin(), arr.end());
                    } else if (count == 3) {
                        auto arr = self.template nextid<3>();
                        result.assign(arr.begin(), arr.end());
                    } else if (count == 4) {
                        auto arr = self.template nextid<4>();
                        result.assign(arr.begin(), arr.end());
                    } else if (count == 5) {
                        auto arr = self.template nextid<5>();
                        result.assign(arr.begin(), arr.end());
                    } else if (count == 6) {
                        auto arr = self.template nextid<6>();
                        result.assign(arr.begin(), arr.end());
                    } else if (count == 7) {
                        auto arr = self.template nextid<7>();
                        result.assign(arr.begin(), arr.end());
                    } else if (count == 8) {
                        auto arr = self.template nextid<8>();
                        result.assign(arr.begin(), arr.end());
                    } else if (count == 9) {
                        auto arr = self.template nextid<9>();
                        result.assign(arr.begin(), arr.end());
                    } else {
                        auto arr = self.template nextid<10>();
                        result.assign(arr.begin(), arr.end());
                    }

                    return result;
                } else {
                    // For larger counts, generate IDs in batches
                    std::vector<uint64_t> result;
                    result.reserve(count);

                    const size_t BATCH_SIZE = 10;
                    size_t remaining = count;

                    while (remaining > 0) {
                        size_t batch = std::min(remaining, BATCH_SIZE);
                        std::vector<uint64_t> batch_ids;

                        if (batch == 10) {
                            auto arr = self.template nextid<10>();
                            batch_ids.assign(arr.begin(), arr.end());
                        } else if (batch == 9) {
                            auto arr = self.template nextid<9>();
                            batch_ids.assign(arr.begin(), arr.end());
                        } else if (batch == 8) {
                            auto arr = self.template nextid<8>();
                            batch_ids.assign(arr.begin(), arr.end());
                        } else if (batch == 7) {
                            auto arr = self.template nextid<7>();
                            batch_ids.assign(arr.begin(), arr.end());
                        } else if (batch == 6) {
                            auto arr = self.template nextid<6>();
                            batch_ids.assign(arr.begin(), arr.end());
                        } else if (batch == 5) {
                            auto arr = self.template nextid<5>();
                            batch_ids.assign(arr.begin(), arr.end());
                        } else if (batch == 4) {
                            auto arr = self.template nextid<4>();
                            batch_ids.assign(arr.begin(), arr.end());
                        } else if (batch == 3) {
                            auto arr = self.template nextid<3>();
                            batch_ids.assign(arr.begin(), arr.end());
                        } else if (batch == 2) {
                            auto arr = self.template nextid<2>();
                            batch_ids.assign(arr.begin(), arr.end());
                        } else {
                            auto arr = self.template nextid<1>();
                            batch_ids.assign(arr.begin(), arr.end());
                        }

                        result.insert(result.end(), batch_ids.begin(),
                                      batch_ids.end());
                        remaining -= batch;
                    }

                    return result;
                }
            },
            py::arg("count") = 1,
            R"(Generates multiple unique IDs at once.

Args:
    count: Number of IDs to generate (default is 1)

Returns:
    List of unique IDs
)")
        .def("validate_id",
             &atom::algorithm::Snowflake<DEFAULT_TWEPOCH,
                                         std::mutex>::validateId,
             py::arg("id"),
             R"(Validates if an ID was generated by this generator instance.

Args:
    id: The ID to validate

Returns:
    True if the ID was generated by this instance, False otherwise
)")
        .def("extract_timestamp",
             &atom::algorithm::Snowflake<DEFAULT_TWEPOCH,
                                         std::mutex>::extractTimestamp,
             py::arg("id"),
             R"(Extracts the timestamp from a Snowflake ID.

Args:
    id: The Snowflake ID

Returns:
    Timestamp in milliseconds since the epoch
)")
        .def(
            "parse_id",
            [](const atom::algorithm::Snowflake<DEFAULT_TWEPOCH, std::mutex>&
                   self,
               uint64_t id) {
                uint64_t timestamp, datacenter_id, worker_id, sequence;
                self.parseId(id, timestamp, datacenter_id, worker_id, sequence);

                py::dict result;
                result["timestamp"] = timestamp;
                result["datacenter_id"] = datacenter_id;
                result["worker_id"] = worker_id;
                result["sequence"] = sequence;
                return result;
            },
            py::arg("id"),
            R"(Parses a Snowflake ID into its constituent parts.

Args:
    id: The Snowflake ID to parse

Returns:
    Dictionary with 'timestamp', 'datacenter_id', 'worker_id', and 'sequence' components
)")
        .def("reset",
             &atom::algorithm::Snowflake<DEFAULT_TWEPOCH, std::mutex>::reset,
             "Resets the generator to its initial state.")
        .def("get_worker_id",
             &atom::algorithm::Snowflake<DEFAULT_TWEPOCH,
                                         std::mutex>::getWorkerId,
             "Returns the current worker ID.")
        .def("get_datacenter_id",
             &atom::algorithm::Snowflake<DEFAULT_TWEPOCH,
                                         std::mutex>::getDatacenterId,
             "Returns the current datacenter ID.")
        .def(
            "get_statistics",
            [](const atom::algorithm::Snowflake<DEFAULT_TWEPOCH, std::mutex>&
                   self) {
                return convert_statistics_to_dict(self.getStatistics());
            },
            "Returns statistics about ID generation.")
        .def(
            "serialize",
            &atom::algorithm::Snowflake<DEFAULT_TWEPOCH, std::mutex>::serialize,
            "Serializes the current state of the generator to a string.")
        .def("deserialize",
             &atom::algorithm::Snowflake<DEFAULT_TWEPOCH,
                                         std::mutex>::deserialize,
             py::arg("state"),
             "Deserializes the state of the generator from a string.");

    // Add constants
    m.attr("WORKER_ID_BITS") =
        atom::algorithm::Snowflake<DEFAULT_TWEPOCH, std::mutex>::WORKER_ID_BITS;
    m.attr("DATACENTER_ID_BITS") =
        atom::algorithm::Snowflake<DEFAULT_TWEPOCH,
                                   std::mutex>::DATACENTER_ID_BITS;
    m.attr("MAX_WORKER_ID") =
        atom::algorithm::Snowflake<DEFAULT_TWEPOCH, std::mutex>::MAX_WORKER_ID;
    m.attr("MAX_DATACENTER_ID") =
        atom::algorithm::Snowflake<DEFAULT_TWEPOCH,
                                   std::mutex>::MAX_DATACENTER_ID;
    m.attr("SEQUENCE_BITS") =
        atom::algorithm::Snowflake<DEFAULT_TWEPOCH, std::mutex>::SEQUENCE_BITS;
    m.attr("TWEPOCH") =
        atom::algorithm::Snowflake<DEFAULT_TWEPOCH, std::mutex>::TWEPOCH;
}