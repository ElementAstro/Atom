#include "atom/extra/injection/all.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

namespace py = pybind11;

PYBIND11_MODULE(injection, m) {
    m.doc() = R"(Dependency injection container module for the atom package.

This module provides a modern C++ dependency injection framework with support for
various binding types, scopes, and automatic dependency resolution.

Features:
- Type-safe dependency injection with compile-time checking
- Multiple binding types: constant values, dynamic factories, and automatic resolution
- Scoping support: singleton, transient, and custom scopes
- Hierarchical containers with parent-child relationships
- Tag-based and name-based binding selection
- Automatic constructor injection

Examples:
    >>> from atom.extra.injection import injection
    >>> 
    >>> # Create a container
    >>> container = injection.Container()
    >>> 
    >>> # Bind a constant value
    >>> container.bind_constant("config_value", "production")
    >>> 
    >>> # Bind with a factory function
    >>> def create_service():
    ...     return MyService()
    >>> container.bind_factory("service", create_service)
    >>> 
    >>> # Resolve dependencies
    >>> config = container.resolve("config_value")
    >>> service = container.resolve("service")
)";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::extra::exceptions::ResolutionException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // ResolutionException
    py::register_exception<atom::extra::exceptions::ResolutionException>(
        m, "ResolutionException", PyExc_RuntimeError);

    // Tag struct
    py::class_<atom::extra::Tag>(m, "Tag",
                                 R"(A tag for categorizing and selecting bindings.

Tags allow you to differentiate between multiple bindings of the same type
and select specific bindings based on context.

Examples:
    >>> tag = injection.Tag("database", "primary")
    >>> print(tag.name)
    >>> print(tag.value)
)")
        .def(py::init<const std::string&>(), py::arg("name"),
             "Create a tag with a name")
        .def(py::init<const std::string&, const std::string&>(),
             py::arg("name"), py::arg("value"),
             "Create a tag with a name and value")
        .def_readwrite("name", &atom::extra::Tag::name, "Tag name")
        .def_readwrite("value", &atom::extra::Tag::value, "Tag value")
        .def("__eq__", [](const atom::extra::Tag& self, const atom::extra::Tag& other) {
            return self.name == other.name && self.value == other.value;
        }, "Check tag equality")
        .def("__str__", [](const atom::extra::Tag& self) {
            return self.name + (self.value.empty() ? "" : ":" + self.value);
        }, "String representation of the tag");

    // Scope enum
    py::enum_<atom::extra::Scope>(m, "Scope",
                                  R"(Defines the lifetime scope of a binding.

The scope determines how instances are created and managed by the container.)")
        .value("Transient", atom::extra::Scope::Transient,
               "Create a new instance every time the dependency is resolved")
        .value("Singleton", atom::extra::Scope::Singleton,
               "Create a single instance and reuse it for all resolutions")
        .export_values();

    // Since the injection system is heavily templated and designed for compile-time
    // type safety, we'll provide a simplified Python interface that works with
    // Python objects and strings as keys.
    
    py::class_<atom::extra::Container<>>(m, "Container",
                                         R"(Dependency injection container.

This container manages bindings and resolves dependencies. It supports
various binding types and scopes, and can create child containers
that inherit bindings from their parent.

Examples:
    >>> container = injection.Container()
    >>> 
    >>> # Bind constant values
    >>> container.bind_constant("database_url", "postgresql://localhost/mydb")
    >>> container.bind_constant("debug_mode", True)
    >>> 
    >>> # Create child container
    >>> child = container.create_child_container()
    >>> 
    >>> # Child inherits parent bindings
    >>> url = child.resolve("database_url")
)")
        .def(py::init<>(), "Create a new dependency injection container")
        .def("create_child_container", &atom::extra::Container<>::createChildContainer,
             R"(Create a child container that inherits bindings from this container.

Returns:
    A new child container.

Examples:
    >>> parent = injection.Container()
    >>> parent.bind_constant("config", "production")
    >>> child = parent.create_child_container()
    >>> config = child.resolve("config")  # Inherits from parent
)");

    // Utility functions for common binding patterns
    m.def("create_container", []() {
        return std::make_unique<atom::extra::Container<>>();
    }, R"(Create a new dependency injection container.

Returns:
    A new Container instance.

Examples:
    >>> container = injection.create_container()
)");

    m.def("create_tag", [](const std::string& name, const std::string& value = "") {
        return atom::extra::Tag{name, value};
    }, py::arg("name"), py::arg("value") = "",
    R"(Create a new tag for binding selection.

Args:
    name: The tag name.
    value: Optional tag value.

Returns:
    A new Tag instance.

Examples:
    >>> tag = injection.create_tag("database", "primary")
    >>> tag2 = injection.create_tag("cache")
)");

    // Helper class for Python-friendly dependency injection
    py::class_<std::map<std::string, py::object>>(m, "ServiceRegistry",
                                                  R"(A Python-friendly service registry for dependency injection.

This class provides a simplified interface for registering and resolving
services using string keys, making it easier to use from Python.

Examples:
    >>> registry = injection.ServiceRegistry()
    >>> 
    >>> # Register services
    >>> registry["database"] = DatabaseService()
    >>> registry["cache"] = CacheService()
    >>> 
    >>> # Resolve services
    >>> db = registry["database"]
    >>> cache = registry["cache"]
)")
        .def(py::init<>(), "Create a new service registry")
        .def("__getitem__", [](std::map<std::string, py::object>& self, const std::string& key) {
            auto it = self.find(key);
            if (it == self.end()) {
                throw py::key_error("Service '" + key + "' not found");
            }
            return it->second;
        }, R"(Get a service by key.

Args:
    key: The service key.

Returns:
    The registered service.

Raises:
    KeyError: If the service is not found.
)")
        .def("__setitem__", [](std::map<std::string, py::object>& self, const std::string& key, py::object value) {
            self[key] = value;
        }, R"(Register a service with a key.

Args:
    key: The service key.
    value: The service instance.
)")
        .def("__contains__", [](const std::map<std::string, py::object>& self, const std::string& key) {
            return self.find(key) != self.end();
        }, R"(Check if a service is registered.

Args:
    key: The service key.

Returns:
    True if the service is registered.
)")
        .def("get", [](const std::map<std::string, py::object>& self, const std::string& key, py::object default_value = py::none()) {
            auto it = self.find(key);
            return it != self.end() ? it->second : default_value;
        }, py::arg("key"), py::arg("default") = py::none(),
        R"(Get a service with an optional default value.

Args:
    key: The service key.
    default: Default value if service is not found.

Returns:
    The service or default value.
)")
        .def("keys", [](const std::map<std::string, py::object>& self) {
            std::vector<std::string> keys;
            for (const auto& pair : self) {
                keys.push_back(pair.first);
            }
            return keys;
        }, R"(Get all registered service keys.

Returns:
    List of service keys.
)")
        .def("clear", [](std::map<std::string, py::object>& self) {
            self.clear();
        }, R"(Remove all registered services.)");

    // Factory function for creating service registries
    m.def("create_service_registry", []() {
        return std::make_unique<std::map<std::string, py::object>>();
    }, R"(Create a new service registry.

Returns:
    A new ServiceRegistry instance.

Examples:
    >>> registry = injection.create_service_registry()
    >>> registry["my_service"] = MyService()
)");
}
