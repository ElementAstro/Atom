#include "atom/search/lru.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(lru, m) {
    m.doc() = "Thread-safe LRU cache module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::search::LRUCacheLockException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const atom::search::LRUCacheIOException& e) {
            PyErr_SetString(PyExc_IOError, e.what());
        } catch (const atom::search::LRUCacheException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Define CacheStatistics structure
    py::class_<atom::search::ThreadSafeLRUCache<std::string,
                                                std::string>::CacheStatistics>(
        m, "CacheStatistics",
        R"(Statistics about the LRU cache performance and state.

Contains metrics about the cache's usage and performance.

Attributes:
    hit_count: Number of cache hits
    miss_count: Number of cache misses
    hit_rate: Ratio of hits to total accesses (between 0.0 and 1.0)
    size: Current number of items in the cache
    max_size: Maximum capacity of the cache
    load_factor: Ratio of current size to maximum capacity
)")
        .def_readonly("hit_count",
                      &atom::search::ThreadSafeLRUCache<
                          std::string, std::string>::CacheStatistics::hitCount)
        .def_readonly("miss_count",
                      &atom::search::ThreadSafeLRUCache<
                          std::string, std::string>::CacheStatistics::missCount)
        .def_readonly("hit_rate",
                      &atom::search::ThreadSafeLRUCache<
                          std::string, std::string>::CacheStatistics::hitRate)
        .def_readonly("size",
                      &atom::search::ThreadSafeLRUCache<
                          std::string, std::string>::CacheStatistics::size)
        .def_readonly("max_size",
                      &atom::search::ThreadSafeLRUCache<
                          std::string, std::string>::CacheStatistics::maxSize)
        .def_readonly(
            "load_factor",
            &atom::search::ThreadSafeLRUCache<
                std::string, std::string>::CacheStatistics::loadFactor);

    // String-String Cache
    py::class_<atom::search::ThreadSafeLRUCache<std::string, std::string>>(
        m, "StringCache",
        R"(A thread-safe LRU (Least Recently Used) cache with string keys and values.

This class implements a thread-safe LRU cache with features like TTL, statistics tracking,
and persistence. It efficiently manages memory by evicting least recently used items when
capacity is reached.

Args:
    max_size: The maximum number of items the cache can hold

Examples:
    >>> from atom.search.lru import StringCache
    >>> cache = StringCache(100)  # Create cache with max 100 items
    >>> cache.put("key1", "value1")
    >>> cache.contains("key1")
    True
    >>> value = cache.get("key1")
    >>> print(value)
    value1
)")
        .def(py::init<size_t>(), py::arg("max_size"),
             "Constructs a ThreadSafeLRUCache with the specified maximum size.")
        .def("get",
             &atom::search::ThreadSafeLRUCache<std::string, std::string>::get,
             py::arg("key"),
             R"(Retrieves a value from the cache.

Args:
    key: The key of the item to retrieve

Returns:
    The value if found and not expired, None otherwise

Raises:
    RuntimeError: If a deadlock is detected
)")
        .def(
            "get_shared",
            [](atom::search::ThreadSafeLRUCache<std::string, std::string>& self,
               const std::string& key) {
                auto ptr = self.getShared(key);
                if (ptr) {
                    return *ptr;
                }
                return std::string();
            },
            py::arg("key"),
            R"(Retrieves a value from the cache as a copy.

Args:
    key: The key of the item to retrieve

Returns:
    The value if found and not expired, empty string otherwise
)")
        .def(
            "get_batch",
            [](atom::search::ThreadSafeLRUCache<std::string, std::string>& self,
               const std::vector<std::string>& keys) {
                auto result = self.getBatch(keys);
                std::vector<std::optional<std::string>> pythonResult;
                pythonResult.reserve(result.size());
                for (const auto& ptr : result) {
                    if (ptr) {
                        pythonResult.push_back(*ptr);
                    } else {
                        pythonResult.push_back(std::nullopt);
                    }
                }
                return pythonResult;
            },
            py::arg("keys"),
            R"(Batch retrieval of multiple values from the cache.

Args:
    keys: List of keys to retrieve

Returns:
    List of values corresponding to the keys (None for missing or expired items)
)")
        .def("contains",
             &atom::search::ThreadSafeLRUCache<std::string,
                                               std::string>::contains,
             py::arg("key"),
             R"(Checks if a key exists in the cache.

Args:
    key: The key to check

Returns:
    True if the key exists and is not expired, False otherwise
)")
        .def("put",
             &atom::search::ThreadSafeLRUCache<std::string, std::string>::put,
             py::arg("key"), py::arg("value"),
             py::arg("ttl") = std::optional<std::chrono::seconds>(),
             R"(Inserts or updates a value in the cache.

Args:
    key: The key of the item to insert or update
    value: The value to associate with the key
    ttl: Optional time-to-live duration in seconds for the cache item

Raises:
    ValueError: If memory allocation fails
    RuntimeError: For other internal errors
)")
        .def("put_batch",
             &atom::search::ThreadSafeLRUCache<std::string,
                                               std::string>::putBatch,
             py::arg("items"),
             py::arg("ttl") = std::optional<std::chrono::seconds>(),
             R"(Inserts or updates a batch of values in the cache.

Args:
    items: List of key-value pairs to insert
    ttl: Optional time-to-live duration in seconds for all cache items

Raises:
    RuntimeError: If an error occurs during batch insertion
)")
        .def("erase",
             &atom::search::ThreadSafeLRUCache<std::string, std::string>::erase,
             py::arg("key"),
             R"(Erases an item from the cache.

Args:
    key: The key of the item to remove

Returns:
    True if the item was found and removed, False otherwise
)")
        .def("clear",
             &atom::search::ThreadSafeLRUCache<std::string, std::string>::clear,
             "Clears all items from the cache.")
        .def("keys",
             &atom::search::ThreadSafeLRUCache<std::string, std::string>::keys,
             R"(Retrieves all keys in the cache.

Returns:
    A list containing all keys currently in the cache

Raises:
    RuntimeError: If an error occurs while retrieving keys
)")
        .def(
            "pop_lru",
            &atom::search::ThreadSafeLRUCache<std::string, std::string>::popLru,
            R"(Removes and returns the least recently used item.

Returns:
    A key-value pair if the cache is not empty, None otherwise
)")
        .def(
            "resize",
            &atom::search::ThreadSafeLRUCache<std::string, std::string>::resize,
            py::arg("new_max_size"),
            R"(Resizes the cache to a new maximum size.

If the new size is smaller, the least recently used items are removed until the cache size fits.

Args:
    new_max_size: The new maximum size of the cache

Raises:
    ValueError: If new_max_size is zero
    RuntimeError: If an error occurs during resizing
)")
        .def("size",
             &atom::search::ThreadSafeLRUCache<std::string, std::string>::size,
             R"(Gets the current size of the cache.

Returns:
    The number of items currently in the cache
)")
        .def("max_size",
             &atom::search::ThreadSafeLRUCache<std::string,
                                               std::string>::maxSize,
             R"(Gets the maximum size of the cache.

Returns:
    The maximum number of items the cache can hold
)")
        .def("load_factor",
             &atom::search::ThreadSafeLRUCache<std::string,
                                               std::string>::loadFactor,
             R"(Gets the current load factor of the cache.

The load factor is the ratio of the current size to the maximum size.

Returns:
    The load factor of the cache (between 0.0 and 1.0)
)")
        .def(
            "set_insert_callback",
            &atom::search::ThreadSafeLRUCache<std::string,
                                              std::string>::setInsertCallback,
            py::arg("callback"),
            R"(Sets the callback function to be called when a new item is inserted.

Args:
    callback: The callback function that takes a key and value

Examples:
    >>> def on_insert(key, value):
    ...     print(f"Inserted {key}: {value}")
    ...
    >>> cache.set_insert_callback(on_insert)
)")
        .def("set_erase_callback",
             &atom::search::ThreadSafeLRUCache<std::string,
                                               std::string>::setEraseCallback,
             py::arg("callback"),
             R"(Sets the callback function to be called when an item is erased.

Args:
    callback: The callback function that takes a key

Examples:
    >>> def on_erase(key):
    ...     print(f"Erased {key}")
    ...
    >>> cache.set_erase_callback(on_erase)
)")
        .def(
            "set_clear_callback",
            &atom::search::ThreadSafeLRUCache<std::string,
                                              std::string>::setClearCallback,
            py::arg("callback"),
            R"(Sets the callback function to be called when the cache is cleared.

Args:
    callback: The callback function

Examples:
    >>> def on_clear():
    ...     print("Cache cleared")
    ...
    >>> cache.set_clear_callback(on_clear)
)")
        .def("hit_rate",
             &atom::search::ThreadSafeLRUCache<std::string,
                                               std::string>::hitRate,
             R"(Gets the hit rate of the cache.

The hit rate is the ratio of cache hits to the total number of cache accesses.

Returns:
    The hit rate of the cache (between 0.0 and 1.0)
)")
        .def("get_statistics",
             &atom::search::ThreadSafeLRUCache<std::string,
                                               std::string>::getStatistics,
             R"(Gets comprehensive statistics about the cache.

Returns:
    A CacheStatistics object containing various metrics
)")
        .def("save_to_file",
             &atom::search::ThreadSafeLRUCache<std::string,
                                               std::string>::saveToFile,
             py::arg("filename"),
             R"(Saves the cache contents to a file.

Args:
    filename: The name of the file to save to

Raises:
    RuntimeError: If a deadlock is avoided while locking
    IOError: If file operations fail
)")
        .def("load_from_file",
             &atom::search::ThreadSafeLRUCache<std::string,
                                               std::string>::loadFromFile,
             py::arg("filename"),
             R"(Loads cache contents from a file.

Args:
    filename: The name of the file to load from

Raises:
    RuntimeError: If a deadlock is avoided while locking
    IOError: If file operations fail
)")
        .def("prune_expired",
             &atom::search::ThreadSafeLRUCache<std::string,
                                               std::string>::pruneExpired,
             R"(Prune expired items from the cache.

Returns:
    Number of items pruned
)")
        .def("prefetch",
             &atom::search::ThreadSafeLRUCache<std::string,
                                               std::string>::prefetch,
             py::arg("keys"), py::arg("loader"),
             py::arg("ttl") = std::optional<std::chrono::seconds>(),
             R"(Prefetch keys into the cache to improve hit rate.

Args:
    keys: List of keys to prefetch
    loader: Function to load values for keys not in cache
    ttl: Optional time-to-live for prefetched items

Returns:
    Number of items successfully prefetched

Examples:
    >>> def load_value(key):
    ...     # Get value from database or other source
    ...     return f"Value for {key}"
    ...
    >>> cache.prefetch(["key1", "key2", "key3"], load_value, 600)  # 10 minute TTL
)")
        .def("__contains__",
             &atom::search::ThreadSafeLRUCache<std::string,
                                               std::string>::contains,
             py::arg("key"), "Support for the 'in' operator.")
        .def("__len__",
             &atom::search::ThreadSafeLRUCache<std::string, std::string>::size,
             "Support for len() function.")
        .def(
            "__bool__",
            [](const atom::search::ThreadSafeLRUCache<std::string, std::string>&
                   cache) { return cache.size() > 0; },
            "Support for boolean evaluation.");

    // Integer Cache
    py::class_<atom::search::ThreadSafeLRUCache<std::string, int>>(
        m, "IntCache",
        R"(A thread-safe LRU (Least Recently Used) cache with string keys and integer values.

Thread-safe LRU cache implementation optimized for integer values.

Examples:
    >>> from atom.search.lru import IntCache
    >>> cache = IntCache(100)
    >>> cache.put("user_id", 12345)
    >>> cache.get("user_id")
    12345
)")
        .def(py::init<size_t>(), py::arg("max_size"))
        .def("get", &atom::search::ThreadSafeLRUCache<std::string, int>::get,
             py::arg("key"))
        .def(
            "get_shared",
            [](atom::search::ThreadSafeLRUCache<std::string, int>& self,
               const std::string& key) -> std::optional<int> {
                auto ptr = self.getShared(key);
                if (ptr) {
                    return *ptr;
                }
                return std::nullopt;
            },
            py::arg("key"))
        .def(
            "get_batch",
            [](atom::search::ThreadSafeLRUCache<std::string, int>& self,
               const std::vector<std::string>& keys) {
                auto result = self.getBatch(keys);
                std::vector<std::optional<int>> pythonResult;
                pythonResult.reserve(result.size());
                for (const auto& ptr : result) {
                    if (ptr) {
                        pythonResult.push_back(*ptr);
                    } else {
                        pythonResult.push_back(std::nullopt);
                    }
                }
                return pythonResult;
            },
            py::arg("keys"))
        .def("contains",
             &atom::search::ThreadSafeLRUCache<std::string, int>::contains,
             py::arg("key"))
        .def("put", &atom::search::ThreadSafeLRUCache<std::string, int>::put,
             py::arg("key"), py::arg("value"),
             py::arg("ttl") = std::optional<std::chrono::seconds>())
        .def("put_batch",
             &atom::search::ThreadSafeLRUCache<std::string, int>::putBatch,
             py::arg("items"),
             py::arg("ttl") = std::optional<std::chrono::seconds>())
        .def("erase",
             &atom::search::ThreadSafeLRUCache<std::string, int>::erase,
             py::arg("key"))
        .def("clear",
             &atom::search::ThreadSafeLRUCache<std::string, int>::clear)
        .def("keys", &atom::search::ThreadSafeLRUCache<std::string, int>::keys)
        .def("pop_lru",
             &atom::search::ThreadSafeLRUCache<std::string, int>::popLru)
        .def("resize",
             &atom::search::ThreadSafeLRUCache<std::string, int>::resize,
             py::arg("new_max_size"))
        .def("size", &atom::search::ThreadSafeLRUCache<std::string, int>::size)
        .def("max_size",
             &atom::search::ThreadSafeLRUCache<std::string, int>::maxSize)
        .def("load_factor",
             &atom::search::ThreadSafeLRUCache<std::string, int>::loadFactor)
        .def("hit_rate",
             &atom::search::ThreadSafeLRUCache<std::string, int>::hitRate)
        .def("prune_expired",
             &atom::search::ThreadSafeLRUCache<std::string, int>::pruneExpired)
        .def("save_to_file",
             &atom::search::ThreadSafeLRUCache<std::string, int>::saveToFile,
             py::arg("filename"))
        .def("load_from_file",
             &atom::search::ThreadSafeLRUCache<std::string, int>::loadFromFile,
             py::arg("filename"))
        .def("__contains__",
             &atom::search::ThreadSafeLRUCache<std::string, int>::contains,
             py::arg("key"))
        .def("__len__",
             &atom::search::ThreadSafeLRUCache<std::string, int>::size)
        .def("__bool__",
             [](const atom::search::ThreadSafeLRUCache<std::string, int>&
                    cache) { return cache.size() > 0; });

    // Float Cache
    py::class_<atom::search::ThreadSafeLRUCache<std::string, double>>(
        m, "FloatCache",
        R"(A thread-safe LRU (Least Recently Used) cache with string keys and float values.

Thread-safe LRU cache implementation optimized for floating-point values.

Examples:
    >>> from atom.search.lru import FloatCache
    >>> cache = FloatCache(100)
    >>> cache.put("pi", 3.14159)
    >>> cache.get("pi")
    3.14159
)")
        .def(py::init<size_t>(), py::arg("max_size"))
        .def("get", &atom::search::ThreadSafeLRUCache<std::string, double>::get,
             py::arg("key"))
        .def(
            "get_shared",
            [](atom::search::ThreadSafeLRUCache<std::string, double>& self,
               const std::string& key) -> std::optional<double> {
                auto ptr = self.getShared(key);
                if (ptr) {
                    return *ptr;
                }
                return std::nullopt;
            },
            py::arg("key"))
        .def(
            "get_batch",
            [](atom::search::ThreadSafeLRUCache<std::string, double>& self,
               const std::vector<std::string>& keys) {
                auto result = self.getBatch(keys);
                std::vector<std::optional<double>> pythonResult;
                pythonResult.reserve(result.size());
                for (const auto& ptr : result) {
                    if (ptr) {
                        pythonResult.push_back(*ptr);
                    } else {
                        pythonResult.push_back(std::nullopt);
                    }
                }
                return pythonResult;
            },
            py::arg("keys"))
        .def("contains",
             &atom::search::ThreadSafeLRUCache<std::string, double>::contains,
             py::arg("key"))
        .def("put", &atom::search::ThreadSafeLRUCache<std::string, double>::put,
             py::arg("key"), py::arg("value"),
             py::arg("ttl") = std::optional<std::chrono::seconds>())
        .def("put_batch",
             &atom::search::ThreadSafeLRUCache<std::string, double>::putBatch,
             py::arg("items"),
             py::arg("ttl") = std::optional<std::chrono::seconds>())
        .def("erase",
             &atom::search::ThreadSafeLRUCache<std::string, double>::erase,
             py::arg("key"))
        .def("clear",
             &atom::search::ThreadSafeLRUCache<std::string, double>::clear)
        .def("keys",
             &atom::search::ThreadSafeLRUCache<std::string, double>::keys)
        .def("pop_lru",
             &atom::search::ThreadSafeLRUCache<std::string, double>::popLru)
        .def("resize",
             &atom::search::ThreadSafeLRUCache<std::string, double>::resize,
             py::arg("new_max_size"))
        .def("size",
             &atom::search::ThreadSafeLRUCache<std::string, double>::size)
        .def("max_size",
             &atom::search::ThreadSafeLRUCache<std::string, double>::maxSize)
        .def("load_factor",
             &atom::search::ThreadSafeLRUCache<std::string, double>::loadFactor)
        .def("hit_rate",
             &atom::search::ThreadSafeLRUCache<std::string, double>::hitRate)
        .def("prune_expired",
             &atom::search::ThreadSafeLRUCache<std::string,
                                               double>::pruneExpired)
        .def("save_to_file",
             &atom::search::ThreadSafeLRUCache<std::string, double>::saveToFile,
             py::arg("filename"))
        .def("load_from_file",
             &atom::search::ThreadSafeLRUCache<std::string,
                                               double>::loadFromFile,
             py::arg("filename"))
        .def("__contains__",
             &atom::search::ThreadSafeLRUCache<std::string, double>::contains,
             py::arg("key"))
        .def("__len__",
             &atom::search::ThreadSafeLRUCache<std::string, double>::size)
        .def("__bool__",
             [](const atom::search::ThreadSafeLRUCache<std::string, double>&
                    cache) { return cache.size() > 0; });

    // Factory functions
    m.def(
        "create_string_cache",
        [](size_t max_size) {
            return std::make_unique<
                atom::search::ThreadSafeLRUCache<std::string, std::string>>(
                max_size);
        },
        py::arg("max_size"),
        R"(Create a string LRU cache with the specified capacity.

Args:
    max_size: Maximum number of items the cache can hold

Returns:
    A new StringCache instance

Examples:
    >>> from atom.search.lru import create_string_cache
    >>> cache = create_string_cache(1000)
)");

    m.def(
        "create_int_cache",
        [](size_t max_size) {
            return std::make_unique<
                atom::search::ThreadSafeLRUCache<std::string, int>>(max_size);
        },
        py::arg("max_size"),
        R"(Create an integer LRU cache with the specified capacity.

Args:
    max_size: Maximum number of items the cache can hold

Returns:
    A new IntCache instance
)");

    m.def(
        "create_float_cache",
        [](size_t max_size) {
            return std::make_unique<
                atom::search::ThreadSafeLRUCache<std::string, double>>(
                max_size);
        },
        py::arg("max_size"),
        R"(Create a float LRU cache with the specified capacity.

Args:
    max_size: Maximum number of items the cache can hold

Returns:
    A new FloatCache instance
)");
}