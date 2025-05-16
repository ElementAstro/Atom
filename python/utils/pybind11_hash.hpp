#pragma once

#include <functional>
#include <pybind11/pybind11.h>

// 为 pybind11::object 特化 std::hash
namespace std {
template <>
struct hash<pybind11::object> {
    size_t operator()(const pybind11::object& obj) const {
        // 使用 Python 的内置 hash 函数
        if (obj.is_none()) {
            // None 对象的特殊处理
            return 0;
        }
        if (!PyObject_HasAttrString(obj.ptr(), "__hash__")) {
            throw std::runtime_error("Python object is not hashable");
        }
        pybind11::gil_scoped_acquire gil;
        pybind11::function hash_fn = pybind11::module::import("builtins").attr("hash");
        try {
            return static_cast<size_t>(hash_fn(obj).cast<ssize_t>());
        } catch (const pybind11::error_already_set&) {
            throw std::runtime_error("Python object is not hashable");
        }
    }
};
}  // namespace std
