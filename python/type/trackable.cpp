#include "atom/type/trackable.hpp"

#include <pybind11/functional.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <type_traits>

namespace py = pybind11;

// 运算符检测模板 - 用于检查类型是否支持特定运算符
template <typename T, typename = void>
struct has_plus_operator : std::false_type {};

template <typename T>
struct has_plus_operator<
    T, std::void_t<decltype(std::declval<T>() + std::declval<T>())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_minus_operator : std::false_type {};

template <typename T>
struct has_minus_operator<
    T, std::void_t<decltype(std::declval<T>() - std::declval<T>())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_multiply_operator : std::false_type {};

template <typename T>
struct has_multiply_operator<
    T, std::void_t<decltype(std::declval<T>() * std::declval<T>())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_divide_operator : std::false_type {};

template <typename T>
struct has_divide_operator<
    T, std::void_t<decltype(std::declval<T>() / std::declval<T>())>>
    : std::true_type {};

// 便捷变量模板
template <typename T>
inline constexpr bool has_plus_operator_v = has_plus_operator<T>::value;

template <typename T>
inline constexpr bool has_minus_operator_v = has_minus_operator<T>::value;

template <typename T>
inline constexpr bool has_multiply_operator_v = has_multiply_operator<T>::value;

template <typename T>
inline constexpr bool has_divide_operator_v = has_divide_operator<T>::value;

/**
 * @brief Declares a Trackable class for a specific type
 *
 * @tparam T The type of the tracked value
 * @param m The pybind11 module to add the class to
 * @param name The name of the class
 * @return A reference to the declared class
 */
template <typename T>
py::class_<Trackable<T>> declare_trackable(py::module& m,
                                           const std::string& name) {
    using TrackableType = Trackable<T>;

    std::string class_name = "Trackable" + name;
    std::string class_doc = "Trackable object for " + name +
                            " values.\n\n"
                            "A Trackable object allows observers to be "
                            "notified when its value changes.";

    // 基本类定义，不包含运算符
    auto cls =
        py::class_<TrackableType>(m, class_name.c_str(), class_doc.c_str())
            .def(py::init<T>(), py::arg("initial_value"),
                 "Constructs a Trackable object with the specified initial "
                 "value.")
            .def(
                "subscribe", &TrackableType::subscribe, py::arg("on_change"),
                R"(Subscribe a callback function to be called when the value changes.

Args:
    on_change: A function taking two arguments (old_value, new_value) to be called on value changes.
)")
            .def(
                "set_on_change_callback", &TrackableType::setOnChangeCallback,
                py::arg("on_change"),
                R"(Set a callback that will be triggered when the value changes.

Args:
    on_change: A function taking one argument (new_value) to be called on value changes.
)")
            .def("unsubscribe_all", &TrackableType::unsubscribeAll,
                 "Unsubscribe all observer functions.")
            .def("has_subscribers", &TrackableType::hasSubscribers,
                 "Checks if there are any subscribers.")
            .def("get", &TrackableType::get,
                 "Get the current value of the trackable object.")
            .def("get_type_name", &TrackableType::getTypeName,
                 "Get the demangled type name of the stored value.")
            .def("defer_notifications", &TrackableType::deferNotifications,
                 py::arg("defer"),
                 R"(Control whether notifications are deferred or not.

Args:
    defer: If True, notifications will be deferred until defer_notifications(False) is called.
)")
            .def("defer_scoped", &TrackableType::deferScoped,
                 R"(Creates a context manager for deferring notifications.

Returns:
    A context manager that will defer notifications while active and resume them when exited.

Examples:
    >>> with trackable.defer_scoped():
    ...     trackable.value = 1  # No notification yet
    ...     trackable.value = 2  # No notification yet
    ...     trackable.value = 3  # No notification yet
    ... # Notifications resume here with the last value change
)")
            // Python-specific methods
            .def("__str__",
                 [](const TrackableType& t) {
                     return py::str("Trackable<{}>({})")
                         .format(t.getTypeName(), t.get());
                 })
            .def("__repr__",
                 [](const TrackableType& t) {
                     return py::str("Trackable<{}>({})")
                         .format(t.getTypeName(), t.get());
                 })
            // Property access for the value
            .def_property(
                "value", &TrackableType::get,
                [](TrackableType& t, const T& val) { t = val; },
                "The current value of the trackable object.");

    // 根据类型是否支持运算符选择性地添加运算符绑定
    if constexpr (has_plus_operator_v<T>) {
        cls.def(py::self += T(), "Adds a value to this trackable object.");
    }

    if constexpr (has_minus_operator_v<T>) {
        cls.def(py::self -= T(),
                "Subtracts a value from this trackable object.");
    }

    if constexpr (has_multiply_operator_v<T>) {
        cls.def(py::self *= T(),
                "Multiplies this trackable object by a value.");
    }

    if constexpr (has_divide_operator_v<T>) {
        cls.def(py::self /= T(), "Divides this trackable object by a value.");
    }

    // 向 Python 暴露这个类型支持哪些运算符的信息
    cls.def_property_readonly_static(
        "supports_addition", [](py::object) { return has_plus_operator_v<T>; },
        "Whether this trackable type supports addition operations");

    cls.def_property_readonly_static(
        "supports_subtraction",
        [](py::object) { return has_minus_operator_v<T>; },
        "Whether this trackable type supports subtraction operations");

    cls.def_property_readonly_static(
        "supports_multiplication",
        [](py::object) { return has_multiply_operator_v<T>; },
        "Whether this trackable type supports multiplication operations");

    cls.def_property_readonly_static(
        "supports_division",
        [](py::object) { return has_divide_operator_v<T>; },
        "Whether this trackable type supports division operations");

    return cls;
}

PYBIND11_MODULE(trackable, m) {
    m.doc() = "Trackable type module for the atom package";

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

    // Register different trackable types
    declare_trackable<int>(m, "Int");
    declare_trackable<double>(m, "Float");
    declare_trackable<std::string>(m, "String");
    declare_trackable<bool>(m, "Bool");

    // Factory function to create trackable objects with type detection
    m.def(
        "create_trackable",
        [&m](py::object value) -> py::object {
            if (py::isinstance<py::int_>(value)) {
                return m.attr("TrackableInt")(py::cast<int>(value));
            } else if (py::isinstance<py::float_>(value)) {
                return m.attr("TrackableFloat")(py::cast<double>(value));
            } else if (py::isinstance<py::str>(value)) {
                return m.attr("TrackableString")(py::cast<std::string>(value));
            } else if (py::isinstance<py::bool_>(value)) {
                return m.attr("TrackableBool")(py::cast<bool>(value));
            } else {
                throw py::type_error("Unsupported value type for trackable");
            }
        },
        py::arg("value"),
        R"(Create a trackable object of the appropriate type based on the given value.

Args:
    value: The initial value for the trackable object (int, float, str, or bool)

Returns:
    A Trackable object containing the value

Examples:
    >>> from atom.trackable import create_trackable
    >>> t = create_trackable(42)
    >>> t.value += 10
    >>> print(t.value)
    52
    >>> 
    >>> # With change callback
    >>> def on_change(old, new):
    ...     print(f"Value changed from {old} to {new}")
    ... 
    >>> t.subscribe(on_change)
    >>> t.value = 100  # This will trigger the callback
)",
        py::return_value_policy::reference);

    // 提供一个辅助函数来查询类型是否支持特定运算符
    m.def(
        "supports_operation",
        [](py::object trackable_obj, const std::string& op) -> bool {
            if (op == "+" || op == "+=") {
                if (py::hasattr(trackable_obj, "supports_addition")) {
                    return trackable_obj.attr("supports_addition").cast<bool>();
                }
            } else if (op == "-" || op == "-=") {
                if (py::hasattr(trackable_obj, "supports_subtraction")) {
                    return trackable_obj.attr("supports_subtraction")
                        .cast<bool>();
                }
            } else if (op == "*" || op == "*=") {
                if (py::hasattr(trackable_obj, "supports_multiplication")) {
                    return trackable_obj.attr("supports_multiplication")
                        .cast<bool>();
                }
            } else if (op == "/" || op == "/=") {
                if (py::hasattr(trackable_obj, "supports_division")) {
                    return trackable_obj.attr("supports_division").cast<bool>();
                }
            }
            return false;
        },
        py::arg("trackable_obj"), py::arg("operation"),
        R"(Check if a trackable object supports a specific operation.

Args:
    trackable_obj: The trackable object to check
    operation: The operation to check for ("+", "+=", "-", "-=", "*", "*=", "/", "/=")

Returns:
    True if the operation is supported, False otherwise

Examples:
    >>> t_int = create_trackable(42)
    >>> t_str = create_trackable("hello")
    >>> supports_operation(t_int, "+")  # Returns True
    >>> supports_operation(t_str, "*")  # Returns False
)");
}