#include "atom/search/ttl.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(ttl, m) {
    m.doc() = "Time-to-Live (TTL) cache module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::search::TTLCacheException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // String TTLCache binding
    py::class_<atom::search::TTLCache<std::string, std::string>>(
        m, "StringCache",
        R"(A Time-to-Live (TTL) Cache with string keys and string values.

This class implements a TTL cache with an LRU eviction policy. Items in the cache 
expire after a specified duration and are evicted when the cache exceeds its maximum capacity.

Args:
    ttl: Duration in milliseconds after which items expire
    max_capacity: Maximum number of items the cache can hold
    cleanup_interval: Optional interval between cleanup operations in milliseconds

Examples:
    >>> from atom.search.ttl import StringCache
    >>> # Create a cache with 5-second TTL and capacity of 100
    >>> cache = StringCache(5000, 100)
    >>> cache.put("key1", "value1")
    >>> cache.get("key1")
    'value1'
)")
        .def(py::init<std::chrono::milliseconds, size_t,
                      std::optional<std::chrono::milliseconds>>(),
             py::arg("ttl"), py::arg("max_capacity"),
             py::arg("cleanup_interval") =
                 std::optional<std::chrono::milliseconds>(),
             "Constructs a TTLCache with the specified TTL and maximum "
             "capacity.")
        .def(
            "put",
            py::overload_cast<const std::string&, const std::string&>(
                &atom::search::TTLCache<std::string, std::string>::put),
            py::arg("key"), py::arg("value"),
            R"(Inserts a new key-value pair into the cache or updates an existing key.

Args:
    key: The key to insert or update
    value: The value associated with the key

Raises:
    RuntimeError: If there's an error inserting the item
)")
        .def("batch_put",
             &atom::search::TTLCache<std::string, std::string>::batch_put,
             py::arg("items"),
             R"(Batch insertion of multiple key-value pairs.

Args:
    items: List of key-value pairs to insert

Raises:
    RuntimeError: If there's an error inserting the items
)")
        .def(
            "get", &atom::search::TTLCache<std::string, std::string>::get,
            py::arg("key"),
            R"(Retrieves the value associated with the given key from the cache.

Args:
    key: The key whose associated value is to be retrieved

Returns:
    The value if found and not expired; otherwise, None
)")
        .def(
            "get_shared",
            &atom::search::TTLCache<std::string, std::string>::get_shared,
            py::arg("key"),
            R"(Retrieves the value as a shared pointer, avoiding copies for large objects.

Args:
    key: The key whose associated value is to be retrieved

Returns:
    Shared pointer to the value if found and not expired; otherwise, None
)")
        .def("batch_get",
             &atom::search::TTLCache<std::string, std::string>::batch_get,
             py::arg("keys"),
             R"(Batch retrieval of multiple values by keys.

Args:
    keys: List of keys to retrieve

Returns:
    List of values corresponding to the keys (None for missing or expired items)
)")
        .def("remove",
             &atom::search::TTLCache<std::string, std::string>::remove,
             py::arg("key"),
             R"(Removes an item from the cache.

Args:
    key: The key to remove

Returns:
    True if the item was found and removed, False otherwise
)")
        .def("contains",
             &atom::search::TTLCache<std::string, std::string>::contains,
             py::arg("key"),
             R"(Checks if a key exists in the cache and has not expired.

Args:
    key: The key to check

Returns:
    True if the key exists and has not expired, False otherwise
)")
        .def("cleanup",
             &atom::search::TTLCache<std::string, std::string>::cleanup,
             "Performs cache cleanup by removing expired items.")
        .def("force_cleanup",
             &atom::search::TTLCache<std::string, std::string>::force_cleanup,
             "Manually trigger a cleanup operation.")
        .def("hit_rate",
             &atom::search::TTLCache<std::string, std::string>::hitRate,
             R"(Gets the cache hit rate.

Returns:
    The ratio of cache hits to total accesses (between 0.0 and 1.0)
)")
        .def("size", &atom::search::TTLCache<std::string, std::string>::size,
             R"(Gets the current number of items in the cache.

Returns:
    The number of items in the cache
)")
        .def("capacity",
             &atom::search::TTLCache<std::string, std::string>::capacity,
             R"(Gets the maximum capacity of the cache.

Returns:
    The maximum capacity of the cache
)")
        .def("ttl", &atom::search::TTLCache<std::string, std::string>::ttl,
             R"(Gets the TTL duration of the cache.

Returns:
    The TTL duration in milliseconds
)")
        .def("clear", &atom::search::TTLCache<std::string, std::string>::clear,
             "Clears all items from the cache and resets hit/miss counts.")
        .def("resize",
             &atom::search::TTLCache<std::string, std::string>::resize,
             py::arg("new_capacity"),
             R"(Resizes the cache to a new maximum capacity.

If the new capacity is smaller than the current size,
the least recently used items will be evicted.

Args:
    new_capacity: The new maximum capacity

Raises:
    RuntimeError: If new_capacity is zero
)")
        .def("__contains__",
             &atom::search::TTLCache<std::string, std::string>::contains,
             "Support for 'in' operator to check if key exists.")
        .def("__len__", &atom::search::TTLCache<std::string, std::string>::size,
             "Support for len() function to get cache size.")
        .def(
            "__bool__",
            [](const atom::search::TTLCache<std::string, std::string>& cache) {
                return cache.size() > 0;
            },
            "Support for boolean evaluation.");

    // Integer TTLCache binding
    py::class_<atom::search::TTLCache<std::string, int>>(
        m, "IntCache",
        R"(A Time-to-Live (TTL) Cache with string keys and integer values.

This cache implements an LRU eviction policy with automatic expiration of items.

Examples:
    >>> from atom.search.ttl import IntCache
    >>> cache = IntCache(10000, 50)  # 10-second TTL, 50 items max
    >>> cache.put("user_id", 12345)
    >>> cache.get("user_id")
    12345
)")
        .def(py::init<std::chrono::milliseconds, size_t,
                      std::optional<std::chrono::milliseconds>>(),
             py::arg("ttl"), py::arg("max_capacity"),
             py::arg("cleanup_interval") =
                 std::optional<std::chrono::milliseconds>())
        .def("put",
             py::overload_cast<const std::string&, const int&>(
                 &atom::search::TTLCache<std::string, int>::put),
             py::arg("key"), py::arg("value"))
        .def("batch_put", &atom::search::TTLCache<std::string, int>::batch_put,
             py::arg("items"))
        .def("get", &atom::search::TTLCache<std::string, int>::get,
             py::arg("key"))
        .def("get_shared",
             &atom::search::TTLCache<std::string, int>::get_shared,
             py::arg("key"))
        .def("batch_get", &atom::search::TTLCache<std::string, int>::batch_get,
             py::arg("keys"))
        .def("remove", &atom::search::TTLCache<std::string, int>::remove,
             py::arg("key"))
        .def("contains", &atom::search::TTLCache<std::string, int>::contains,
             py::arg("key"))
        .def("cleanup", &atom::search::TTLCache<std::string, int>::cleanup)
        .def("force_cleanup",
             &atom::search::TTLCache<std::string, int>::force_cleanup)
        .def("hit_rate", &atom::search::TTLCache<std::string, int>::hitRate)
        .def("size", &atom::search::TTLCache<std::string, int>::size)
        .def("capacity", &atom::search::TTLCache<std::string, int>::capacity)
        .def("ttl", &atom::search::TTLCache<std::string, int>::ttl)
        .def("clear", &atom::search::TTLCache<std::string, int>::clear)
        .def("resize", &atom::search::TTLCache<std::string, int>::resize,
             py::arg("new_capacity"))
        .def("__contains__",
             &atom::search::TTLCache<std::string, int>::contains)
        .def("__len__", &atom::search::TTLCache<std::string, int>::size)
        .def("__bool__",
             [](const atom::search::TTLCache<std::string, int>& cache) {
                 return cache.size() > 0;
             });

    // Float TTLCache binding
    py::class_<atom::search::TTLCache<std::string, double>>(
        m, "FloatCache",
        R"(A Time-to-Live (TTL) Cache with string keys and floating-point values.

This cache implements an LRU eviction policy with automatic expiration of items.

Examples:
    >>> from atom.search.ttl import FloatCache
    >>> cache = FloatCache(30000, 100)  # 30-second TTL, 100 items max
    >>> cache.put("pi", 3.14159)
    >>> cache.get("pi")
    3.14159
)")
        .def(py::init<std::chrono::milliseconds, size_t,
                      std::optional<std::chrono::milliseconds>>(),
             py::arg("ttl"), py::arg("max_capacity"),
             py::arg("cleanup_interval") =
                 std::optional<std::chrono::milliseconds>())
        .def("put",
             py::overload_cast<const std::string&, const double&>(
                 &atom::search::TTLCache<std::string, double>::put),
             py::arg("key"), py::arg("value"))
        .def("batch_put",
             &atom::search::TTLCache<std::string, double>::batch_put,
             py::arg("items"))
        .def("get", &atom::search::TTLCache<std::string, double>::get,
             py::arg("key"))
        .def("get_shared",
             &atom::search::TTLCache<std::string, double>::get_shared,
             py::arg("key"))
        .def("batch_get",
             &atom::search::TTLCache<std::string, double>::batch_get,
             py::arg("keys"))
        .def("remove", &atom::search::TTLCache<std::string, double>::remove,
             py::arg("key"))
        .def("contains", &atom::search::TTLCache<std::string, double>::contains,
             py::arg("key"))
        .def("cleanup", &atom::search::TTLCache<std::string, double>::cleanup)
        .def("force_cleanup",
             &atom::search::TTLCache<std::string, double>::force_cleanup)
        .def("hit_rate", &atom::search::TTLCache<std::string, double>::hitRate)
        .def("size", &atom::search::TTLCache<std::string, double>::size)
        .def("capacity", &atom::search::TTLCache<std::string, double>::capacity)
        .def("ttl", &atom::search::TTLCache<std::string, double>::ttl)
        .def("clear", &atom::search::TTLCache<std::string, double>::clear)
        .def("resize", &atom::search::TTLCache<std::string, double>::resize,
             py::arg("new_capacity"))
        .def("__contains__",
             &atom::search::TTLCache<std::string, double>::contains)
        .def("__len__", &atom::search::TTLCache<std::string, double>::size)
        .def("__bool__",
             [](const atom::search::TTLCache<std::string, double>& cache) {
                 return cache.size() > 0;
             });

    // Factory functions to create caches with optimal parameters
    m.def(
        "create_string_cache",
        [](double ttl_seconds, size_t max_capacity) {
            auto ttl_ms =
                std::chrono::milliseconds(static_cast<int>(ttl_seconds * 1000));
            return std::make_unique<
                atom::search::TTLCache<std::string, std::string>>(ttl_ms,
                                                                  max_capacity);
        },
        py::arg("ttl_seconds"), py::arg("max_capacity"),
        R"(Create a TTL cache for string values with the specified parameters.

Args:
    ttl_seconds: TTL in seconds for cache items
    max_capacity: Maximum number of items the cache can hold

Returns:
    A new StringCache instance

Examples:
    >>> from atom.search.ttl import create_string_cache
    >>> cache = create_string_cache(10.5, 100)  # 10.5 seconds TTL, 100 items
)");

    m.def(
        "create_int_cache",
        [](double ttl_seconds, size_t max_capacity) {
            auto ttl_ms =
                std::chrono::milliseconds(static_cast<int>(ttl_seconds * 1000));
            return std::make_unique<atom::search::TTLCache<std::string, int>>(
                ttl_ms, max_capacity);
        },
        py::arg("ttl_seconds"), py::arg("max_capacity"),
        R"(Create a TTL cache for integer values with the specified parameters.

Args:
    ttl_seconds: TTL in seconds for cache items
    max_capacity: Maximum number of items the cache can hold

Returns:
    A new IntCache instance
)");

    m.def(
        "create_float_cache",
        [](double ttl_seconds, size_t max_capacity) {
            auto ttl_ms =
                std::chrono::milliseconds(static_cast<int>(ttl_seconds * 1000));
            return std::make_unique<
                atom::search::TTLCache<std::string, double>>(ttl_ms,
                                                             max_capacity);
        },
        py::arg("ttl_seconds"), py::arg("max_capacity"),
        R"(Create a TTL cache for floating-point values with the specified parameters.

Args:
    ttl_seconds: TTL in seconds for cache items
    max_capacity: Maximum number of items the cache can hold

Returns:
    A new FloatCache instance
)");
}