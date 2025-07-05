#include "atom/search/ttl.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

template <typename ValueType>
void define_cache_class(py::module& m, const char* class_name,
                        const char* doc_string) {
    py::class_<atom::search::TTLCache<std::string, ValueType>>(m, class_name,
                                                               doc_string)
        .def(py::init<std::chrono::milliseconds, size_t,
                      std::optional<std::chrono::milliseconds>>(),
             py::arg("ttl"), py::arg("max_capacity"),
             py::arg("cleanup_interval") =
                 std::optional<std::chrono::milliseconds>())
        .def("put",
             py::overload_cast<const std::string&, const ValueType&>(
                 &atom::search::TTLCache<std::string, ValueType>::put),
             py::arg("key"), py::arg("value"))
        .def("batch_put",
             &atom::search::TTLCache<std::string, ValueType>::batch_put,
             py::arg("items"))
        .def("get", &atom::search::TTLCache<std::string, ValueType>::get,
             py::arg("key"))
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
        .def("batch_get",
             &atom::search::TTLCache<std::string, ValueType>::batch_get,
             py::arg("keys"))
        .def("remove", &atom::search::TTLCache<std::string, ValueType>::remove,
             py::arg("key"))
        .def("contains",
             &atom::search::TTLCache<std::string, ValueType>::contains,
             py::arg("key"))
        .def("cleanup",
             &atom::search::TTLCache<std::string, ValueType>::cleanup)
        .def("force_cleanup",
             &atom::search::TTLCache<std::string, ValueType>::force_cleanup)
        .def("hit_rate",
             &atom::search::TTLCache<std::string, ValueType>::hitRate)
        .def("size", &atom::search::TTLCache<std::string, ValueType>::size)
        .def("capacity",
             &atom::search::TTLCache<std::string, ValueType>::capacity)
        .def("ttl", &atom::search::TTLCache<std::string, ValueType>::ttl)
        .def("clear", &atom::search::TTLCache<std::string, ValueType>::clear)
        .def("resize", &atom::search::TTLCache<std::string, ValueType>::resize,
             py::arg("new_capacity"))
        .def("__contains__",
             &atom::search::TTLCache<std::string, ValueType>::contains)
        .def("__len__", &atom::search::TTLCache<std::string, ValueType>::size)
        .def("__bool__",
             [](const atom::search::TTLCache<std::string, ValueType>& cache) {
                 return cache.size() > 0;
             });
}

PYBIND11_MODULE(ttl, m) {
    m.doc() = "Time-to-Live (TTL) cache module for the atom package";

    // 注册异常转换
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

    // 定义字符串缓存
    define_cache_class<std::string>(
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
)");

    // 定义整数缓存
    define_cache_class<int>(
        m, "IntCache",
        R"(A Time-to-Live (TTL) Cache with string keys and integer values.

This cache implements an LRU eviction policy with automatic expiration of items.

Examples:
    >>> from atom.search.ttl import IntCache
    >>> cache = IntCache(10000, 50)  # 10-second TTL, 50 items max
    >>> cache.put("user_id", 12345)
    >>> cache.get("user_id")
    12345
)");

    // 定义浮点数缓存
    define_cache_class<double>(
        m, "FloatCache",
        R"(A Time-to-Live (TTL) Cache with string keys and floating-point values.

This cache implements an LRU eviction policy with automatic expiration of items.

Examples:
    >>> from atom.search.ttl import FloatCache
    >>> cache = FloatCache(30000, 100)  # 30-second TTL, 100 items max
    >>> cache.put("pi", 3.14159)
    >>> cache.get("pi")
    3.14159
)");

    // 工厂函数，创建缓存并使用最优参数
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
