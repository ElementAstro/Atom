#include "atom/search/cache.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

/**
 * @brief Registers exception translations for the cache module.
 *
 * This function sets up proper exception handling to translate C++ exceptions
 * to appropriate Python exceptions for better error reporting.
 *
 * @param m The pybind11 module to register exceptions for
 */
void registerExceptionTranslations(py::module_& m) {
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
}

/**
 * @brief Template function to bind ResourceCache types to Python.
 *
 * This template function creates Python bindings for ResourceCache<T> types,
 * reducing code duplication across different cache types.
 *
 * @tparam T The type of resource stored in the cache
 * @param m The pybind11 module to bind to
 * @param class_name The Python class name for this cache type
 * @param doc_string The documentation string for this cache type
 * @param type_name Human-readable name for the type (e.g., "string", "integer")
 */
template<typename T>
void bindResourceCache(py::module_& m, const char* class_name,
                       const char* doc_string, const char* type_name) {
    py::class_<atom::search::ResourceCache<T>>(m, class_name, doc_string)
        .def(py::init<int>(), py::arg("max_size"),
             ("Constructs a " + std::string(class_name) + " with the specified maximum size.").c_str())

        // Core cache operations
        .def("insert", &atom::search::ResourceCache<T>::insert,
             py::arg("key"), py::arg("value"), py::arg("expiration_time"),
             ("Inserts a " + std::string(type_name) + " resource into the cache with an expiration time.").c_str())
        .def("contains", &atom::search::ResourceCache<T>::contains,
             py::arg("key"),
             "Checks if the cache contains a resource with the specified key.")
        .def("get", &atom::search::ResourceCache<T>::get, py::arg("key"),
             ("Retrieves a " + std::string(type_name) + " resource from the cache.").c_str())
        .def("remove", &atom::search::ResourceCache<T>::remove,
             py::arg("key"), "Removes a resource from the cache.")
        .def("clear", &atom::search::ResourceCache<T>::clear,
             "Clears all resources from the cache.")

        // Cache status and management
        .def("size", &atom::search::ResourceCache<T>::size,
             "Gets the number of resources in the cache.")
        .def("empty", &atom::search::ResourceCache<T>::empty,
             "Checks if the cache is empty.")
        .def("evict_oldest", &atom::search::ResourceCache<T>::evictOldest,
             "Evicts the oldest resource from the cache.")
        .def("is_expired", &atom::search::ResourceCache<T>::isExpired,
             py::arg("key"),
             "Checks if a resource with the specified key is expired.")
        .def("set_max_size", &atom::search::ResourceCache<T>::setMaxSize,
             py::arg("max_size"), "Sets the maximum size of the cache.")
        .def("set_expiration_time", &atom::search::ResourceCache<T>::setExpirationTime,
             py::arg("key"), py::arg("expiration_time"),
             "Sets the expiration time for a resource in the cache.")
        .def("remove_expired", &atom::search::ResourceCache<T>::removeExpired,
             "Removes expired resources from the cache.")

        // Batch operations
        .def("insert_batch", &atom::search::ResourceCache<T>::insertBatch,
             py::arg("items"), py::arg("expiration_time"),
             ("Inserts multiple " + std::string(type_name) + " resources into the cache with an expiration time.").c_str())
        .def("remove_batch", &atom::search::ResourceCache<T>::removeBatch,
             py::arg("keys"), "Removes multiple resources from the cache.")

        // Callback and statistics
        .def("on_insert", &atom::search::ResourceCache<T>::onInsert,
             py::arg("callback"),
             "Registers a callback to be called on insertion.")
        .def("on_remove", &atom::search::ResourceCache<T>::onRemove,
             py::arg("callback"),
             "Registers a callback to be called on removal.")
        .def("get_statistics", &atom::search::ResourceCache<T>::getStatistics,
             "Retrieves cache statistics (hits, misses).");
}

/**
 * @brief Binds the StringCache class to Python.
 *
 * This function creates Python bindings for the ResourceCache<std::string> class
 * with comprehensive documentation and examples.
 *
 * @param m The pybind11 module to bind to
 */
void bindStringCache(py::module_& m) {
    bindResourceCache<std::string>(
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

    >>> # Batch operations
    >>> cache.insert_batch([("key2", "value2"), ("key3", "value3")], 120)
    >>> cache.remove_batch(["key2", "key3"])

    >>> # Statistics and callbacks
    >>> def on_insert_callback(key):
    ...     print(f"Inserted: {key}")
    >>> cache.on_insert(on_insert_callback)
    >>> hits, misses = cache.get_statistics()
    >>> hit_rate = hits / (hits + misses) if hits + misses > 0 else 0
    >>> print(f"Hit rate: {hit_rate:.2%}")
)",
        "string");
}

/**
 * @brief Binds the IntCache class to Python.
 *
 * This function creates Python bindings for the ResourceCache<int> class
 * with comprehensive documentation and examples.
 *
 * @param m The pybind11 module to bind to
 */
void bindIntCache(py::module_& m) {
    bindResourceCache<int>(
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

    >>> # Check expiration and manage cache
    >>> cache.is_expired("user_id")
    False
    >>> cache.set_expiration_time("user_id", 600)  # Extend to 600 seconds
    >>> cache.remove_expired()  # Clean up expired entries
)",
        "integer");
}

/**
 * @brief Binds the FloatCache class to Python.
 *
 * This function creates Python bindings for the ResourceCache<double> class
 * with comprehensive documentation and examples.
 *
 * @param m The pybind11 module to bind to
 */
void bindFloatCache(py::module_& m) {
    bindResourceCache<double>(
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

    >>> # Cache management
    >>> cache.set_max_size(200)  # Increase cache size
    >>> cache.evict_oldest()     # Remove oldest entry
    >>> print(f"Cache size: {cache.size()}")
)",
        "floating-point");
}

/**
 * @brief Binds factory functions for creating cache instances.
 *
 * This function creates Python bindings for factory functions that provide
 * alternative ways to create cache instances.
 *
 * @param m The pybind11 module to bind to
 */
void bindFactoryFunctions(py::module_& m) {
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

Examples:
    >>> from atom.search.cache import create_string_cache
    >>> cache = create_string_cache(50)
    >>> cache.insert("key", "value", 60)
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

Examples:
    >>> from atom.search.cache import create_int_cache
    >>> cache = create_int_cache(50)
    >>> cache.insert("count", 42, 60)
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

Examples:
    >>> from atom.search.cache import create_float_cache
    >>> cache = create_float_cache(50)
    >>> cache.insert("temperature", 23.5, 60)
)");
}

/**
 * @brief Adds comprehensive module documentation and usage examples.
 *
 * This function sets the module's __doc__ attribute with detailed documentation
 * including usage examples for cache operations.
 *
 * @param m The pybind11 module to add documentation to
 */
void addModuleDocumentation(py::module_& m) {
    m.attr("__doc__") = R"(Resource cache module for the atom package.

This module provides thread-safe caching capabilities for different data types with expiration support.

Key Features:
- Thread-safe operations for concurrent access
- Automatic expiration of cached items
- Batch operations for improved performance
- Cache statistics and monitoring
- Callback support for cache events
- Configurable cache size limits
- LRU (Least Recently Used) eviction policy

Supported Cache Types:
- StringCache: For caching string resources
- IntCache: For caching integer resources
- FloatCache: For caching floating-point resources

Examples:
    >>> from atom.search.cache import StringCache, IntCache, FloatCache
    >>>
    >>> # Create different types of caches
    >>> string_cache = StringCache(100)
    >>> int_cache = IntCache(50)
    >>> float_cache = FloatCache(75)
    >>>
    >>> # Basic cache operations
    >>> string_cache.insert("user:123", "John Doe", 300)  # Cache for 5 minutes
    >>> int_cache.insert("score", 95, 600)                # Cache for 10 minutes
    >>> float_cache.insert("temperature", 23.5, 120)      # Cache for 2 minutes
    >>>
    >>> # Retrieve cached values
    >>> user_name = string_cache.get("user:123")
    >>> score = int_cache.get("score")
    >>> temp = float_cache.get("temperature")
    >>>
    >>> # Check cache status
    >>> if string_cache.contains("user:123"):
    >>>     print("User data is cached")
    >>>
    >>> # Batch operations
    >>> users = [("user:124", "Jane Smith"), ("user:125", "Bob Wilson")]
    >>> string_cache.insert_batch(users, 300)
    >>>
    >>> # Cache management
    >>> string_cache.remove_expired()  # Clean up expired entries
    >>> print(f"Cache size: {string_cache.size()}")
    >>>
    >>> # Statistics and monitoring
    >>> hits, misses = string_cache.get_statistics()
    >>> hit_rate = hits / (hits + misses) if hits + misses > 0 else 0
    >>> print(f"Cache hit rate: {hit_rate:.2%}")
    >>>
    >>> # Event callbacks
    >>> def on_cache_insert(key):
    >>>     print(f"Item cached: {key}")
    >>>
    >>> string_cache.on_insert(on_cache_insert)
    >>>
    >>> # Factory functions (alternative creation method)
    >>> from atom.search.cache import create_string_cache
    >>> cache = create_string_cache(100)
)";
}

PYBIND11_MODULE(cache, m) {
    m.doc() = "Resource cache module for the atom package";

    // Register exception translations
    registerExceptionTranslations(m);

    // Bind different cache types using the template function
    bindStringCache(m);
    bindIntCache(m);
    bindFloatCache(m);

    // Bind factory functions
    bindFactoryFunctions(m);

    // Add module documentation
    addModuleDocumentation(m);
}