#include "atom/type/trackable.hpp"

#include <pybind11/functional.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

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

    return py::class_<TrackableType>(m, class_name.c_str(), class_doc.c_str())
        .def(py::init<T>(), py::arg("initial_value"),
             "Constructs a Trackable object with the specified initial value.")
        .def(
            "subscribe", &TrackableType::subscribe, py::arg("on_change"),
            R"(Subscribe a callback function to be called when the value changes.

Args:
    on_change: A function taking two arguments (old_value, new_value) to be called on value changes.
)")
        .def("set_on_change_callback", &TrackableType::setOnChangeCallback,
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
            "The current value of the trackable object.")
        // Operator overloads
        .def(py::self += T())
        .def(py::self -= T())
        .def(py::self *= T())
        .def(py::self /= T());
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
}