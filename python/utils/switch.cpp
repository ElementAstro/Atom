#include "atom/utils/switch.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(switch, m) {
    m.doc() = "String-based switch statement utilities module for the atom package";

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

    // Statistics snapshot structure
    py::class_<atom::utils::StringSwitch<>::Stats::Snapshot>(m, "StatsSnapshot",
        R"(Performance statistics snapshot for StringSwitch operations.
        
        This structure contains a thread-safe snapshot of performance metrics
        including call counts, cache performance, and response times.
        )")
        .def_readonly("total_calls", &atom::utils::StringSwitch<>::Stats::Snapshot::totalCalls,
                     "Total number of match calls made")
        .def_readonly("cache_hits", &atom::utils::StringSwitch<>::Stats::Snapshot::cacheHits,
                     "Number of cache hits")
        .def_readonly("cache_misses", &atom::utils::StringSwitch<>::Stats::Snapshot::cacheMisses,
                     "Number of cache misses")
        .def_readonly("hit_ratio", &atom::utils::StringSwitch<>::Stats::Snapshot::hitRatio,
                     "Cache hit ratio (0.0 to 1.0)")
        .def_readonly("avg_response_time", &atom::utils::StringSwitch<>::Stats::Snapshot::avgResponseTime,
                     "Average response time in seconds")
        .def_readonly("error_count", &atom::utils::StringSwitch<>::Stats::Snapshot::errorCount,
                     "Number of errors encountered")
        .def_readonly("total_cases", &atom::utils::StringSwitch<>::Stats::Snapshot::totalCases,
                     "Total number of registered cases");

    // StringSwitch class for Python functions (non-thread-safe version)
    py::class_<atom::utils::StringSwitch<false>>(m, "StringSwitch",
        R"(A string-based switch statement implementation for Python.
        
        This class allows you to register Python functions associated with string keys,
        similar to a switch statement. It supports caching and performance monitoring.
        
        Examples:
            >>> from atom.utils import switch
            >>> sw = switch.StringSwitch()
            >>> sw.register_case("hello", lambda: "Hello, World!")
            >>> sw.register_case("goodbye", lambda: "Goodbye!")
            >>> result = sw.match("hello")
            >>> print(result)  # "Hello, World!"
        )")
        .def(py::init<>(), "Create a new StringSwitch instance")
        .def("register_case", 
             [](atom::utils::StringSwitch<false>& self, const std::string& key, py::function func) {
                 self.registerCase(key, [func]() -> atom::utils::StringSwitch<false>::ReturnType {
                     try {
                         auto result = func();
                         if (py::isinstance<py::str>(result)) {
                             return atom::utils::String(result.cast<std::string>());
                         } else if (py::isinstance<py::int_>(result)) {
                             return result.cast<int>();
                         } else {
                             return atom::utils::String(py::str(result).cast<std::string>());
                         }
                     } catch (const std::exception& e) {
                         throw std::runtime_error(std::string("Function execution failed: ") + e.what());
                     }
                 });
             },
             py::arg("key"), py::arg("function"),
             R"(Register a case with the given string key and Python function.
             
             Args:
                 key: The string key for the case.
                 function: The Python function to associate with the key.
                 
             Raises:
                 ValueError: If the key is empty.
                 RuntimeError: If the case is already registered.
                 
             Examples:
                 >>> sw.register_case("test", lambda: "Test result")
             )")
        .def("unregister_case", &atom::utils::StringSwitch<false>::unregisterCase<const std::string&>,
             py::arg("key"),
             R"(Unregister a case with the given string key.
             
             Args:
                 key: The string key for the case to unregister.
                 
             Returns:
                 True if the case was found and unregistered, False otherwise.
                 
             Examples:
                 >>> success = sw.unregister_case("test")
             )")
        .def("clear_cases", &atom::utils::StringSwitch<false>::clearCases,
             R"(Clear all registered cases.
             
             Examples:
                 >>> sw.clear_cases()
             )")
        .def("match", 
             [](atom::utils::StringSwitch<false>& self, const std::string& key) -> py::object {
                 auto result = self.match(key);
                 if (result) {
                     return std::visit([](auto&& arg) -> py::object {
                         using T = std::decay_t<decltype(arg)>;
                         if constexpr (std::is_same_v<T, std::monostate>) {
                             return py::none();
                         } else if constexpr (std::is_same_v<T, int>) {
                             return py::cast(arg);
                         } else if constexpr (std::is_same_v<T, atom::utils::String>) {
                             return py::cast(std::string(arg));
                         } else {
                             return py::none();
                         }
                     }, *result);
                 }
                 return py::none();
             },
             py::arg("key"),
             R"(Match the given string against registered cases.
             
             Args:
                 key: The string key to match.
                 
             Returns:
                 The result of the matched function, or None if no match found.
                 
             Examples:
                 >>> result = sw.match("hello")
             )")
        .def("set_default", 
             [](atom::utils::StringSwitch<false>& self, py::function func) {
                 self.setDefault([func]() -> atom::utils::StringSwitch<false>::ReturnType {
                     try {
                         auto result = func();
                         if (py::isinstance<py::str>(result)) {
                             return atom::utils::String(result.cast<std::string>());
                         } else if (py::isinstance<py::int_>(result)) {
                             return result.cast<int>();
                         } else {
                             return atom::utils::String(py::str(result).cast<std::string>());
                         }
                     } catch (const std::exception& e) {
                         throw std::runtime_error(std::string("Default function execution failed: ") + e.what());
                     }
                 });
             },
             py::arg("function"),
             R"(Set the default function to be called if no match is found.
             
             Args:
                 function: The Python function to use as default.
                 
             Examples:
                 >>> sw.set_default(lambda: "Default response")
             )")
        .def("get_cases", &atom::utils::StringSwitch<false>::getCases,
             R"(Get a list of all registered case keys.
             
             Returns:
                 List of all registered string keys.
                 
             Examples:
                 >>> cases = sw.get_cases()
             )")
        .def("has_case", &atom::utils::StringSwitch<false>::hasCase<const std::string&>,
             py::arg("key"),
             R"(Check if a case exists for the given key.
             
             Args:
                 key: The string key to check.
                 
             Returns:
                 True if the case exists, False otherwise.
                 
             Examples:
                 >>> exists = sw.has_case("hello")
             )")
        .def("get_stats", 
             [](const atom::utils::StringSwitch<false>& self) {
                 return self.getStats().getSnapshot();
             },
             R"(Get performance statistics snapshot.
             
             Returns:
                 StatsSnapshot object with current performance metrics.
                 
             Examples:
                 >>> stats = sw.get_stats()
                 >>> print(f"Total calls: {stats.total_calls}")
             )")
        .def("reset_stats", &atom::utils::StringSwitch<false>::resetStats,
             R"(Reset performance statistics.
             
             Examples:
                 >>> sw.reset_stats()
             )")
        .def("size", &atom::utils::StringSwitch<false>::size,
             R"(Get the number of registered cases.
             
             Returns:
                 Number of registered cases.
                 
             Examples:
                 >>> count = sw.size()
             )")
        .def("empty", &atom::utils::StringSwitch<false>::empty,
             R"(Check if the switch has no registered cases.
             
             Returns:
                 True if no cases are registered, False otherwise.
                 
             Examples:
                 >>> is_empty = sw.empty()
             )")
        .def("__len__", &atom::utils::StringSwitch<false>::size,
             "Support for len() function")
        .def("__bool__", [](const atom::utils::StringSwitch<false>& self) {
                 return !self.empty();
             },
             "Support for bool() function");
}
