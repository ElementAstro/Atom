#include "atom/search/ttl.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

template <typename ValueType>
void define_cache_class(py::module& m, const char* class_name,
                        const char* doc_string) {
    using CacheType = atom::search::TTLCache<std::string, ValueType>;
    using EvictionCallback = typename CacheType::EvictionCallback;

    py::class_<CacheType>(m, class_name, doc_string)
        .def(py::init<std::chrono::milliseconds, size_t,
                      std::optional<std::chrono::milliseconds>>(),
             py::arg("ttl"), py::arg("max_capacity"),
             py::arg("cleanup_interval") =
                 std::optional<std::chrono::milliseconds>(),
             "Create TTL cache with basic parameters")
        .def(py::init<std::chrono::milliseconds, size_t,
                      std::optional<std::chrono::milliseconds>,
                      atom::search::CacheConfig>(),
             py::arg("ttl"), py::arg("max_capacity"),
             py::arg("cleanup_interval"),
             py::arg("config"),
             "Create TTL cache with configuration options")
        .def(py::init<std::chrono::milliseconds, size_t,
                      std::optional<std::chrono::milliseconds>,
                      atom::search::CacheConfig,
                      EvictionCallback>(),
             py::arg("ttl"), py::arg("max_capacity"),
             py::arg("cleanup_interval"),
             py::arg("config"),
             py::arg("eviction_callback"),
             "Create TTL cache with configuration and eviction callback")
        .def("put",
             py::overload_cast<const std::string&, const ValueType&>(
                 &CacheType::put),
             py::arg("key"), py::arg("value"),
             "Insert or update a key-value pair with default TTL")
        .def("put",
             py::overload_cast<const std::string&, const ValueType&,
                               std::optional<std::chrono::milliseconds>>(
                 &CacheType::put),
             py::arg("key"), py::arg("value"), py::arg("custom_ttl"),
             "Insert or update a key-value pair with custom TTL")
        .def("batch_put",
             py::overload_cast<const std::vector<std::pair<std::string, ValueType>>&>(
                 &CacheType::batch_put),
             py::arg("items"),
             "Batch insertion of multiple key-value pairs with default TTL")
        .def("batch_put",
             py::overload_cast<const std::vector<std::pair<std::string, ValueType>>&,
                               std::optional<std::chrono::milliseconds>>(
                 &CacheType::batch_put),
             py::arg("items"), py::arg("custom_ttl"),
             "Batch insertion of multiple key-value pairs with custom TTL")
        .def("get", &CacheType::get, py::arg("key"),
             "Retrieve a value by key")
        .def(
            "get_shared",
            [](atom::search::TTLCache<std::string, ValueType>& self,
               const std::string& key) -> py::object {
                auto value_ptr = self.get_shared(key);
                if (value_ptr) {
                    return py::cast(*value_ptr);
                }
                return py::none();
            },
            py::arg("key"))
        .def("batch_get", &CacheType::batch_get, py::arg("keys"),
             "Retrieve multiple values by their keys")
        .def("remove", &CacheType::remove, py::arg("key"),
             "Remove a key-value pair from the cache")
        .def("contains", &CacheType::contains, py::arg("key"),
             "Check if a key exists in the cache")
        .def("cleanup", &CacheType::cleanup,
             "Manually trigger cleanup of expired items")
        .def("force_cleanup", &CacheType::force_cleanup,
             "Force cleanup of all expired items immediately")
        .def("hit_rate", &CacheType::hitRate,
             "Get the cache hit rate as a percentage")
        .def("size", &CacheType::size,
             "Get the current number of items in the cache")
        .def("capacity", &CacheType::capacity,
             "Get the maximum capacity of the cache")
        .def("ttl", &CacheType::ttl,
             "Get the default TTL duration for cache items")
        .def("clear", &CacheType::clear,
             "Remove all items from the cache")
        .def("resize", &CacheType::resize, py::arg("new_capacity"),
             "Resize the cache to a new maximum capacity")
        .def("reserve", &CacheType::reserve, py::arg("size"),
             "Reserve space in the internal hash map for better performance")
        .def("set_eviction_callback", &CacheType::set_eviction_callback,
             py::arg("callback"),
             R"(Set or update the eviction callback function.

Args:
    callback: Function to call when items are evicted.
              Signature: callback(key: str, value: ValueType, expired: bool)

Examples:
    >>> def on_evict(key, value, expired):
    ...     print(f"Evicted {key}={value}, expired: {expired}")
    >>> cache.set_eviction_callback(on_evict)
)")
        .def("__contains__", &CacheType::contains)
        .def("__len__", &CacheType::size)
        .def("__bool__", [](const CacheType& cache) {
                 return cache.size() > 0;
             });
}

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

    // Bind CacheConfig structure
    py::class_<atom::search::CacheConfig>(
        m, "CacheConfig",
        R"(Configuration options for TTL Cache behavior.

This structure contains various settings that control how the TTL cache operates,
including cleanup behavior, statistics collection, and performance tuning.

Examples:
    >>> from atom.search.ttl import CacheConfig
    >>> config = CacheConfig()
    >>> config.enable_automatic_cleanup = True
    >>> config.enable_statistics = True
    >>> config.thread_safe = True
)")
        .def(py::init<>(), "Create a CacheConfig with default settings")
        .def_readwrite("enable_automatic_cleanup",
                       &atom::search::CacheConfig::enable_automatic_cleanup,
                       "Enable automatic cleanup of expired items (default: True)")
        .def_readwrite("enable_statistics",
                       &atom::search::CacheConfig::enable_statistics,
                       "Enable collection of cache statistics (default: True)")
        .def_readwrite("thread_safe",
                       &atom::search::CacheConfig::thread_safe,
                       "Enable thread-safe operations (default: True)")
        .def_readwrite("cleanup_batch_size",
                       &atom::search::CacheConfig::cleanup_batch_size,
                       "Number of items to process in each cleanup batch (default: 100)")
        .def_readwrite("load_factor",
                       &atom::search::CacheConfig::load_factor,
                       "Hash table load factor for performance tuning (default: 0.75)");

    // Define string cache
    define_cache_class<std::string>(
        m, "StringCache",
        R"(A Time-to-Live (TTL) Cache with string keys and string values.

This class implements a TTL cache with an LRU eviction policy. Items in the cache
expire after a specified duration and are evicted when the cache exceeds its maximum capacity.
Supports advanced features like custom TTL per item, eviction callbacks, and configurable behavior.

Args:
    ttl: Duration in milliseconds after which items expire
    max_capacity: Maximum number of items the cache can hold
    cleanup_interval: Optional interval between cleanup operations in milliseconds
    config: Optional CacheConfig for advanced behavior control
    eviction_callback: Optional callback function for eviction events

Examples:
    >>> from atom.search.ttl import StringCache, CacheConfig
    >>> # Create a basic cache with 5-second TTL and capacity of 100
    >>> cache = StringCache(5000, 100)
    >>> cache.put("key1", "value1")
    >>> cache.get("key1")
    'value1'
    >>>
    >>> # Create cache with custom configuration
    >>> config = CacheConfig()
    >>> config.enable_statistics = True
    >>> cache = StringCache(5000, 100, None, config)
    >>>
    >>> # Create cache with eviction callback
    >>> def on_evict(key, value, expired):
    ...     print(f"Evicted {key}={value}, expired: {expired}")
    >>> cache = StringCache(5000, 100, None, CacheConfig(), on_evict)
)");

    // Define integer cache
    define_cache_class<int>(
        m, "IntCache",
        R"(A Time-to-Live (TTL) Cache with string keys and integer values.

This cache implements an LRU eviction policy with automatic expiration of items.
Supports all advanced features including custom TTL, eviction callbacks, and configuration options.

Examples:
    >>> from atom.search.ttl import IntCache, CacheConfig
    >>> cache = IntCache(10000, 50)  # 10-second TTL, 50 items max
    >>> cache.put("user_id", 12345)
    >>> cache.get("user_id")
    12345
    >>>
    >>> # Use custom TTL for specific items
    >>> cache.put("temp_value", 999, 2000)  # 2-second TTL for this item
)");

    // Define floating-point cache
    define_cache_class<double>(
        m, "FloatCache",
        R"(A Time-to-Live (TTL) Cache with string keys and floating-point values.

This cache implements an LRU eviction policy with automatic expiration of items.
Supports all advanced features including custom TTL, eviction callbacks, and configuration options.

Examples:
    >>> from atom.search.ttl import FloatCache, CacheConfig
    >>> cache = FloatCache(30000, 100)  # 30-second TTL, 100 items max
    >>> cache.put("pi", 3.14159)
    >>> cache.get("pi")
    3.14159
    >>>
    >>> # Use custom TTL for specific items
    >>> cache.put("temp_pi", 3.14, 5000)  # 5-second TTL for this item
)");

    // Factory functions for creating caches with optimal parameters
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
