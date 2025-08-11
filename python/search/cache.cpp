#include "atom/search/cache.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(cache, m) {
    m.doc() = "Resource cache module for the atom package";

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

    // String resource cache binding
    py::class_<atom::search::ResourceCache<std::string>>(
        m, "StringCache",
        R"(A thread-safe cache for storing and managing string resources with expiration times.

This class provides methods to insert, retrieve, and manage cached string resources.

Args:
    max_size: The maximum number of items the cache can hold.

Examples:
    >>> from atom.search.cache import StringCache
    >>> cache = StringCache(100)
    >>> cache.insert("key1", "value1", 60)  # Cache for 60 seconds
    >>> cache.contains("key1")
    True
    >>> value = cache.get("key1")
    >>> print(value)
    value1
)")
        .def(py::init<int>(), py::arg("max_size"),
             "Constructs a StringCache with the specified maximum size.")
        .def("insert", &atom::search::ResourceCache<std::string>::insert,
             py::arg("key"), py::arg("value"), py::arg("expiration_time"),
             R"(Inserts a resource into the cache with an expiration time.

Args:
    key: The key associated with the resource.
    value: The resource to be cached.
    expiration_time: The time in seconds after which the resource expires.
)")
        .def("contains", &atom::search::ResourceCache<std::string>::contains,
             py::arg("key"),
             R"(Checks if the cache contains a resource with the specified key.

Args:
    key: The key to check.

Returns:
    True if the cache contains the resource, false otherwise.
)")
        .def("get", &atom::search::ResourceCache<std::string>::get,
             py::arg("key"),
             R"(Retrieves a resource from the cache.

Args:
    key: The key associated with the resource.

Returns:
    The resource if found, None otherwise.
)")
        .def("remove", &atom::search::ResourceCache<std::string>::remove,
             py::arg("key"),
             R"(Removes a resource from the cache.

Args:
    key: The key associated with the resource to be removed.
)")
        .def("clear", &atom::search::ResourceCache<std::string>::clear,
             "Clears all resources from the cache.")
        .def("size", &atom::search::ResourceCache<std::string>::size,
             "Gets the number of resources in the cache.")
        .def("empty", &atom::search::ResourceCache<std::string>::empty,
             "Checks if the cache is empty.")
        .def("evict_oldest",
             &atom::search::ResourceCache<std::string>::evictOldest,
             "Evicts the oldest resource from the cache.")
        .def("is_expired", &atom::search::ResourceCache<std::string>::isExpired,
             py::arg("key"),
             R"(Checks if a resource with the specified key is expired.

Args:
    key: The key associated with the resource.

Returns:
    True if the resource is expired, false otherwise.
)")
        .def("set_max_size",
             &atom::search::ResourceCache<std::string>::setMaxSize,
             py::arg("max_size"),
             R"(Sets the maximum size of the cache.

Args:
    max_size: The new maximum size of the cache.
)")
        .def("set_expiration_time",
             &atom::search::ResourceCache<std::string>::setExpirationTime,
             py::arg("key"), py::arg("expiration_time"),
             R"(Sets the expiration time for a resource in the cache.

Args:
    key: The key associated with the resource.
    expiration_time: The new expiration time in seconds for the resource.
)")
        .def("remove_expired",
             &atom::search::ResourceCache<std::string>::removeExpired,
             "Removes expired resources from the cache.")
        .def(
            "insert_batch",
            &atom::search::ResourceCache<std::string>::insertBatch,
            py::arg("items"), py::arg("expiration_time"),
            R"(Inserts multiple resources into the cache with an expiration time.

Args:
    items: A list of key-value pairs to insert.
    expiration_time: The time in seconds after which the resources expire.

Examples:
    >>> cache.insert_batch([("key1", "value1"), ("key2", "value2")], 60)
)")
        .def("remove_batch",
             &atom::search::ResourceCache<std::string>::removeBatch,
             py::arg("keys"),
             R"(Removes multiple resources from the cache.

Args:
    keys: A list of keys associated with the resources to remove.
)")
        .def("on_insert", &atom::search::ResourceCache<std::string>::onInsert,
             py::arg("callback"),
             R"(Registers a callback to be called on insertion.

Args:
    callback: The callback function that accepts a key parameter.

Examples:
    >>> def on_insert_callback(key):
    ...     print(f"Inserted: {key}")
    ...
    >>> cache.on_insert(on_insert_callback)
)")
        .def("on_remove", &atom::search::ResourceCache<std::string>::onRemove,
             py::arg("callback"),
             R"(Registers a callback to be called on removal.

Args:
    callback: The callback function that accepts a key parameter.

Examples:
    >>> def on_remove_callback(key):
    ...     print(f"Removed: {key}")
    ...
    >>> cache.on_remove(on_remove_callback)
)")
        .def("get_statistics",
             &atom::search::ResourceCache<std::string>::getStatistics,
             R"(Retrieves cache statistics.

Returns:
    A tuple containing (hit_count, miss_count).

Examples:
    >>> hits, misses = cache.get_statistics()
    >>> hit_rate = hits / (hits + misses) if hits + misses > 0 else 0
    >>> print(f"Hit rate: {hit_rate:.2%}")
)");

    // Integer resource cache binding
    py::class_<atom::search::ResourceCache<int>>(
        m, "IntCache",
        R"(A thread-safe cache for storing and managing integer resources with expiration times.

Args:
    max_size: The maximum number of items the cache can hold.

Examples:
    >>> from atom.search.cache import IntCache
    >>> cache = IntCache(100)
    >>> cache.insert("user_id", 12345, 300)  # Cache for 300 seconds
    >>> cache.get("user_id")
    12345
)")
        .def(py::init<int>(), py::arg("max_size"),
             "Constructs an IntCache with the specified maximum size.")
        .def("insert", &atom::search::ResourceCache<int>::insert,
             py::arg("key"), py::arg("value"), py::arg("expiration_time"),
             "Inserts an integer resource into the cache with an expiration "
             "time.")
        .def("contains", &atom::search::ResourceCache<int>::contains,
             py::arg("key"),
             "Checks if the cache contains a resource with the specified key.")
        .def("get", &atom::search::ResourceCache<int>::get, py::arg("key"),
             "Retrieves an integer resource from the cache.")
        .def("remove", &atom::search::ResourceCache<int>::remove,
             py::arg("key"), "Removes a resource from the cache.")
        .def("clear", &atom::search::ResourceCache<int>::clear,
             "Clears all resources from the cache.")
        .def("size", &atom::search::ResourceCache<int>::size,
             "Gets the number of resources in the cache.")
        .def("empty", &atom::search::ResourceCache<int>::empty,
             "Checks if the cache is empty.")
        .def("evict_oldest", &atom::search::ResourceCache<int>::evictOldest,
             "Evicts the oldest resource from the cache.")
        .def("is_expired", &atom::search::ResourceCache<int>::isExpired,
             py::arg("key"),
             "Checks if a resource with the specified key is expired.")
        .def("set_max_size", &atom::search::ResourceCache<int>::setMaxSize,
             py::arg("max_size"), "Sets the maximum size of the cache.")
        .def("set_expiration_time",
             &atom::search::ResourceCache<int>::setExpirationTime,
             py::arg("key"), py::arg("expiration_time"),
             "Sets the expiration time for a resource in the cache.")
        .def("remove_expired", &atom::search::ResourceCache<int>::removeExpired,
             "Removes expired resources from the cache.")
        .def("insert_batch", &atom::search::ResourceCache<int>::insertBatch,
             py::arg("items"), py::arg("expiration_time"),
             "Inserts multiple integer resources into the cache with an "
             "expiration time.")
        .def("remove_batch", &atom::search::ResourceCache<int>::removeBatch,
             py::arg("keys"), "Removes multiple resources from the cache.")
        .def("on_insert", &atom::search::ResourceCache<int>::onInsert,
             py::arg("callback"),
             "Registers a callback to be called on insertion.")
        .def("on_remove", &atom::search::ResourceCache<int>::onRemove,
             py::arg("callback"),
             "Registers a callback to be called on removal.")
        .def("get_statistics", &atom::search::ResourceCache<int>::getStatistics,
             "Retrieves cache statistics (hits, misses).");

    // Double resource cache binding
    py::class_<atom::search::ResourceCache<double>>(
        m, "FloatCache",
        R"(A thread-safe cache for storing and managing floating-point resources with expiration times.

Args:
    max_size: The maximum number of items the cache can hold.

Examples:
    >>> from atom.search.cache import FloatCache
    >>> cache = FloatCache(100)
    >>> cache.insert("pi", 3.14159, 600)  # Cache for 600 seconds
    >>> cache.get("pi")
    3.14159
)")
        .def(py::init<int>(), py::arg("max_size"),
             "Constructs a FloatCache with the specified maximum size.")
        .def("insert", &atom::search::ResourceCache<double>::insert,
             py::arg("key"), py::arg("value"), py::arg("expiration_time"),
             "Inserts a floating-point resource into the cache with an "
             "expiration time.")
        .def("contains", &atom::search::ResourceCache<double>::contains,
             py::arg("key"),
             "Checks if the cache contains a resource with the specified key.")
        .def("get", &atom::search::ResourceCache<double>::get, py::arg("key"),
             "Retrieves a floating-point resource from the cache.")
        .def("remove", &atom::search::ResourceCache<double>::remove,
             py::arg("key"), "Removes a resource from the cache.")
        .def("clear", &atom::search::ResourceCache<double>::clear,
             "Clears all resources from the cache.")
        .def("size", &atom::search::ResourceCache<double>::size,
             "Gets the number of resources in the cache.")
        .def("empty", &atom::search::ResourceCache<double>::empty,
             "Checks if the cache is empty.")
        .def("evict_oldest", &atom::search::ResourceCache<double>::evictOldest,
             "Evicts the oldest resource from the cache.")
        .def("is_expired", &atom::search::ResourceCache<double>::isExpired,
             py::arg("key"),
             "Checks if a resource with the specified key is expired.")
        .def("set_max_size", &atom::search::ResourceCache<double>::setMaxSize,
             py::arg("max_size"), "Sets the maximum size of the cache.")
        .def("set_expiration_time",
             &atom::search::ResourceCache<double>::setExpirationTime,
             py::arg("key"), py::arg("expiration_time"),
             "Sets the expiration time for a resource in the cache.")
        .def("remove_expired",
             &atom::search::ResourceCache<double>::removeExpired,
             "Removes expired resources from the cache.")
        .def("insert_batch", &atom::search::ResourceCache<double>::insertBatch,
             py::arg("items"), py::arg("expiration_time"),
             "Inserts multiple floating-point resources into the cache with an "
             "expiration time.")
        .def("remove_batch", &atom::search::ResourceCache<double>::removeBatch,
             py::arg("keys"), "Removes multiple resources from the cache.")
        .def("on_insert", &atom::search::ResourceCache<double>::onInsert,
             py::arg("callback"),
             "Registers a callback to be called on insertion.")
        .def("on_remove", &atom::search::ResourceCache<double>::onRemove,
             py::arg("callback"),
             "Registers a callback to be called on removal.")
        .def("get_statistics",
             &atom::search::ResourceCache<double>::getStatistics,
             "Retrieves cache statistics (hits, misses).");

    // Factory function to create caches of different types
    m.def(
        "create_string_cache",
        [](int max_size) {
            return std::make_unique<atom::search::ResourceCache<std::string>>(
                max_size);
        },
        py::arg("max_size"),
        R"(Create a cache for string resources.

Args:
    max_size: The maximum number of items the cache can hold.

Returns:
    A StringCache object.
)");

    m.def(
        "create_int_cache",
        [](int max_size) {
            return std::make_unique<atom::search::ResourceCache<int>>(max_size);
        },
        py::arg("max_size"),
        R"(Create a cache for integer resources.

Args:
    max_size: The maximum number of items the cache can hold.

Returns:
    An IntCache object.
)");

    m.def(
        "create_float_cache",
        [](int max_size) {
            return std::make_unique<atom::search::ResourceCache<double>>(
                max_size);
        },
        py::arg("max_size"),
        R"(Create a cache for floating-point resources.

Args:
    max_size: The maximum number of items the cache can hold.

Returns:
    A FloatCache object.
)");
}
