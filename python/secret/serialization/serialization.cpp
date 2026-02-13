/*
 * serialization.cpp
 *
 * Python bindings for serialization module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/secret/serialization/json.hpp"

namespace py = pybind11;
using namespace atom::secret;

void bind_serialization(py::module& m) {
    // JsonSerializer class
    py::class_<JsonSerializer>(m, "JsonSerializer",
                               R"pbdoc(
        JSON serialization utilities for password entries.
        )pbdoc")
        .def_static("serialize_entry", &JsonSerializer::serializeEntry,
                    py::arg("entry"), py::arg("pretty") = false,
                    "Serializes a password entry to JSON")
        .def_static("deserialize_entry", &JsonSerializer::deserializeEntry,
                    py::arg("json_str"),
                    "Deserializes a password entry from JSON")
        .def_static("serialize_entries", &JsonSerializer::serializeEntries,
                    py::arg("entries"), py::arg("pretty") = false,
                    "Serializes multiple password entries to JSON")
        .def_static("deserialize_entries", &JsonSerializer::deserializeEntries,
                    py::arg("json_str"),
                    "Deserializes multiple password entries from JSON")
        .def_static("serialize_entries_with_keys",
                    &JsonSerializer::serializeEntriesWithKeys,
                    py::arg("entries"), py::arg("pretty") = false,
                    "Serializes password entries with keys to JSON")
        .def_static("deserialize_entries_with_keys",
                    &JsonSerializer::deserializeEntriesWithKeys,
                    py::arg("json_str"),
                    "Deserializes password entries with keys from JSON");
}
