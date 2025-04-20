#include "atom/type/robin_hood.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// 包装threading_policy枚举
enum class ThreadingPolicy {
    UNSAFE,       // No thread safety
    READER_LOCK,  // Reader-writer lock
    MUTEX         // Full mutex lock
};

// 模板类绑定，支持不同键类型的哈希映射
template <typename K, typename V>
py::class_<atom::utils::unordered_flat_map<K, V>> declare_robin_hood_map(
    py::module& m, const std::string& name) {
    using MapType = atom::utils::unordered_flat_map<K, V>;
    using ThreadingPolicyType = typename MapType::threading_policy;

    std::string class_name = "RobinHood" + name;
    std::string class_doc = "Robin Hood hash map with " + name +
                            " keys.\n\n"
                            "A fast unordered map implementation using Robin "
                            "Hood hashing with linear probing.";

    // 定义Python类
    return py::class_<MapType>(m, class_name.c_str(), class_doc.c_str())
        .def(py::init<>(), "Constructs an empty Robin Hood hash map.")
        .def(py::init<ThreadingPolicyType>(), py::arg("policy"),
             "Constructs an empty Robin Hood hash map with specified threading "
             "policy.")
        .def("empty", &MapType::empty,
             "Returns true if the container is empty.")
        .def("size", &MapType::size,
             "Returns the number of elements in the container.")
        .def("max_size", &MapType::max_size,
             "Returns the maximum number of elements the container can hold.")
        .def("clear", &MapType::clear,
             "Removes all elements from the container.")
        .def("at", (V & (MapType::*)(const K&)) & MapType::at, py::arg("key"),
             "Returns a reference to the mapped value of the element with key.")
        .def("bucket_count", &MapType::bucket_count,
             "Returns the number of buckets in the container.")
        .def("load_factor", &MapType::load_factor,
             "Returns the average number of elements per bucket.")
        .def("max_load_factor",
             (float (MapType::*)() const noexcept) & MapType::max_load_factor,
             "Returns the current maximum load factor.")
        .def("max_load_factor",
             (void (MapType::*)(float))&MapType::max_load_factor, py::arg("ml"),
             "Sets the maximum load factor of the container.")
        // Python-specific methods
        .def("__len__", &MapType::size, "Support for len() function.")
        .def(
            "__bool__", [](const MapType& map) { return !map.empty(); },
            "Support for boolean evaluation.")
        .def(
            "__contains__",
            [](const MapType& map, const K& key) {
                try {
                    map.at(key);
                    return true;
                } catch (const std::out_of_range&) {
                    return false;
                }
            },
            py::arg("key"), "Support for the 'in' operator.")
        .def(
            "__getitem__",
            [](MapType& map, const K& key) {
                try {
                    return map.at(key);
                } catch (const std::out_of_range& e) {
                    throw py::key_error(e.what());
                }
            },
            py::arg("key"), "Support for subscript operator [].")
        .def(
            "__setitem__",
            [](MapType& map, const K& key, const V& value) {
                map.insert(key, value);
            },
            py::arg("key"), py::arg("value"),
            "Support for subscript assignment.")
        .def(
            "get",
            [](const MapType& map, const K& key, const V& default_value) {
                try {
                    return map.at(key);
                } catch (const std::out_of_range&) {
                    return default_value;
                }
            },
            py::arg("key"), py::arg("default_value"),
            "Returns the value for key if key is in the dictionary, else "
            "default_value.");
}

PYBIND11_MODULE(robin_hood, m) {
    m.doc() = "Robin Hood hash map implementation for the atom package";

    // 注册异常转换器
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::out_of_range& e) {
            PyErr_SetString(PyExc_KeyError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // 注册线程策略枚举
    py::enum_<ThreadingPolicy>(m, "ThreadingPolicy",
                               "Threading safety policies")
        .value("UNSAFE", ThreadingPolicy::UNSAFE, "No thread safety")
        .value("READER_LOCK", ThreadingPolicy::READER_LOCK,
               "Reader-writer lock for concurrent reads")
        .value("MUTEX", ThreadingPolicy::MUTEX,
               "Full mutex lock for thread safety")
        .export_values();

    // 注册不同键类型的RobinHood映射
    declare_robin_hood_map<std::string, py::object>(m, "StrObj");
    declare_robin_hood_map<std::string, std::string>(m, "StrStr");
    declare_robin_hood_map<std::string, int>(m, "StrInt");
    declare_robin_hood_map<int, py::object>(m, "IntObj");
    declare_robin_hood_map<int, std::string>(m, "IntStr");
    declare_robin_hood_map<int, int>(m, "IntInt");

    // 创建哈希映射的工厂函数
    m.def(
        "create_str_map",
        []() {
            return atom::utils::unordered_flat_map<std::string, py::object>();
        },
        R"(
        Create a Robin Hood hash map with string keys and any values.

        Returns:
            A new RobinHoodStrObj instance.

        Examples:
            >>> from atom.type import robin_hood
            >>> map = robin_hood.create_str_map()
            >>> map["key"] = "value"
            >>> map["key"]
            'value'
    )");

    m.def(
        "create_int_map",
        []() { return atom::utils::unordered_flat_map<int, py::object>(); },
        R"(
        Create a Robin Hood hash map with integer keys and any values.

        Returns:
            A new RobinHoodIntObj instance.

        Examples:
            >>> from atom.type import robin_hood
            >>> map = robin_hood.create_int_map()
            >>> map[1] = "value"
            >>> map[1]
            'value'
    )");

    // 创建具有线程安全策略的哈希映射工厂函数
    m.def(
        "create_threadsafe_map",
        [](ThreadingPolicy policy) {
            using MapType =
                atom::utils::unordered_flat_map<std::string, py::object>;
            using ThreadingPolicyType = typename MapType::threading_policy;

            ThreadingPolicyType cpp_policy;
            switch (policy) {
                case ThreadingPolicy::UNSAFE:
                    cpp_policy = ThreadingPolicyType::unsafe;
                    break;
                case ThreadingPolicy::READER_LOCK:
                    cpp_policy = ThreadingPolicyType::reader_lock;
                    break;
                case ThreadingPolicy::MUTEX:
                    cpp_policy = ThreadingPolicyType::mutex;
                    break;
            }

            return MapType(cpp_policy);
        },
        py::arg("policy") = ThreadingPolicy::READER_LOCK, R"(
        Create a thread-safe Robin Hood hash map with string keys.

        Args:
            policy: Threading policy to use (default: READER_LOCK)

        Returns:
            A new thread-safe RobinHoodStrObj instance.

        Examples:
            >>> from atom.type import robin_hood
            >>> map = robin_hood.create_threadsafe_map(robin_hood.ThreadingPolicy.MUTEX)
            >>> map["key"] = "value"
            >>> map["key"]
            'value'
    )");
}