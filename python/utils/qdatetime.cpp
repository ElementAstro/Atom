#include "atom/utils/qdatetime.hpp"
#include "atom/utils/qtimezone.hpp"

#include <pybind11/chrono.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(datetime, m) {
    m.doc() = "DateTime utilities module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::utils::QDateTime::ParseError& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::logic_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // 定义函数指针类型，解决模板函数绑定问题
    using FromStringFunc1 =
        atom::utils::QDateTime (*)(const std::string&, const std::string&);
    using FromStringFunc2 = atom::utils::QDateTime (*)(
        const std::string&, const std::string&, const atom::utils::QTimeZone&);
    using ToStringFunc1 =
        std::string (atom::utils::QDateTime::*)(const std::string&) const;
    using ToStringFunc2 = std::string (atom::utils::QDateTime::*)(
        const std::string&, const atom::utils::QTimeZone&) const;

    // QDateTime class binding
    py::class_<atom::utils::QDateTime>(
        m, "DateTime",
        R"(Class representing a point in time with timezone support.

This class provides functionality to work with dates and times, including creation from strings,
arithmetic operations, and timezone conversions.

Examples:
    >>> from atom.utils import DateTime, TimeZone
    >>> # Current date and time
    >>> dt = DateTime.current_date_time()
    >>> # Parse from a string
    >>> dt = DateTime.from_string("2023-01-15 14:30:45", "%Y-%m-%d %H:%M:%S")
    >>> # Add time intervals
    >>> tomorrow = dt.add_days(1)
    >>> # Format to string
    >>> dt_str = dt.to_string("%Y-%m-%d")
)")
        .def(py::init<>(), "Initializes an invalid DateTime instance.")
        .def(py::init<const std::string&, const std::string&>(),
             py::arg("date_time_string"), py::arg("format"),
             "Constructs a DateTime object from a date-time string and format.")
        .def(py::init<const std::string&, const std::string&,
                      const atom::utils::QTimeZone&>(),
             py::arg("date_time_string"), py::arg("format"),
             py::arg("time_zone"),
             "Constructs a DateTime object from a date-time string, format, "
             "and time zone.")
        .def_static("current_date_time",
                    static_cast<atom::utils::QDateTime (*)()>(
                        &atom::utils::QDateTime::currentDateTime),
                    "Returns the current date and time.")
        .def_static(
            "current_date_time",
            static_cast<atom::utils::QDateTime (*)(
                const atom::utils::QTimeZone&)>(
                &atom::utils::QDateTime::currentDateTime),
            py::arg("time_zone"),
            "Returns the current date and time in the specified time zone.")
        .def_static(
            "from_string",
            static_cast<FromStringFunc1>(
                &atom::utils::QDateTime::fromString<const std::string&,
                                                    const std::string&>),
            py::arg("date_time_string"), py::arg("format"),
            "Constructs a DateTime object from a date-time string and format.")
        .def_static(
            "from_string",
            static_cast<FromStringFunc2>(
                &atom::utils::QDateTime::fromString<const std::string&,
                                                    const std::string&>),
            py::arg("date_time_string"), py::arg("format"),
            py::arg("time_zone"),
            "Constructs a DateTime object from a date-time string, format, and "
            "time zone.")
        .def(
            "to_string",
            static_cast<ToStringFunc1>(
                &atom::utils::QDateTime::toString<const std::string&>),
            py::arg("format"),
            "Converts the DateTime object to a string in the specified format.")
        .def("to_string",
             static_cast<ToStringFunc2>(
                 &atom::utils::QDateTime::toString<const std::string&>),
             py::arg("format"), py::arg("time_zone"),
             "Converts the DateTime object to a string in the specified format "
             "and time zone.")
        .def("to_time_t", &atom::utils::QDateTime::toTimeT,
             "Converts the DateTime object to a time_t value.")
        .def("is_valid", &atom::utils::QDateTime::isValid,
             "Checks if the DateTime object is valid.")
        .def("add_days", &atom::utils::QDateTime::addDays, py::arg("days"),
             "Adds a number of days to the DateTime object.")
        .def("add_secs", &atom::utils::QDateTime::addSecs, py::arg("seconds"),
             "Adds a number of seconds to the DateTime object.")
        .def("days_to", &atom::utils::QDateTime::daysTo, py::arg("other"),
             "Computes the number of days between this DateTime object and "
             "another.")
        .def("secs_to", &atom::utils::QDateTime::secsTo, py::arg("other"),
             "Computes the number of seconds between this DateTime object and "
             "another.")
        // New methods will be added here when implemented in the C++ class
        .def("add_msecs", &atom::utils::QDateTime::addMSecs, py::arg("msecs"),
             "Adds a number of milliseconds to the DateTime object.")
        .def("add_months", &atom::utils::QDateTime::addMonths,
             py::arg("months"),
             "Adds a number of months to the DateTime object.")
        .def("add_years", &atom::utils::QDateTime::addYears, py::arg("years"),
             "Adds a number of years to the DateTime object.")
        .def("get_date", &atom::utils::QDateTime::getDate,
             "Returns the date part of the DateTime object.")
        .def("get_time", &atom::utils::QDateTime::getTime,
             "Returns the time part of the DateTime object.")
        .def("set_date", &atom::utils::QDateTime::setDate, py::arg("year"),
             py::arg("month"), py::arg("day"),
             "Sets the date part of the DateTime object.")
        .def("set_time", &atom::utils::QDateTime::setTime, py::arg("hour"),
             py::arg("minute"), py::arg("second"), py::arg("ms") = 0,
             "Sets the time part of the DateTime object.")
        .def("set_time_zone", &atom::utils::QDateTime::setTimeZone,
             py::arg("time_zone"), "Sets the time zone of the DateTime object.")
        .def("time_zone", &atom::utils::QDateTime::timeZone,
             "Returns the time zone of the DateTime object.")
        .def("is_dst", &atom::utils::QDateTime::isDST,
             "Returns whether the DateTime is in Daylight Saving Time.")
        .def("to_utc", &atom::utils::QDateTime::toUTC,
             "Returns this DateTime converted to UTC.")
        .def("to_local_time", &atom::utils::QDateTime::toLocalTime,
             "Returns this DateTime converted to local time.")
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def(py::self < py::self)
        .def(py::self <= py::self)
        .def(py::self > py::self)
        .def(py::self >= py::self)
        // Python's dir() friendliness
        .def("__dir__", [](const atom::utils::QDateTime&) {
            return std::vector<std::string>{
                "add_days",  "add_msecs",     "add_months", "add_secs",
                "add_years", "days_to",       "get_date",   "get_time",
                "is_dst",    "is_valid",      "secs_to",    "set_date",
                "set_time",  "set_time_zone", "time_zone",  "to_local_time",
                "to_string", "to_time_t",     "to_utc"};
        });
}