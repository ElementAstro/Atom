#include "atom/utils/qtimezone.hpp"
#include "atom/utils/qdatetime.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(qtimezone, m) {
    m.doc() = "QTimeZone implementation module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::utils::GetTimeException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const atom::error::Exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // QTimeZone class binding
    py::class_<atom::utils::QTimeZone>(m, "QTimeZone",
                                       R"(A class representing a time zone.

The QTimeZone class provides functionality for managing and interacting with time zones.
It includes methods to obtain time zone identifiers, offsets from UTC, and information
about daylight saving time.

Args:
    time_zone_id (str, optional): The identifier of the time zone to use.

Examples:
    >>> from atom.utils import QTimeZone
    >>> tz = QTimeZone("America/New_York")
    >>> tz.identifier()
    'America/New_York'
)")
        .def(py::init<>(),
             "Initializes an invalid QTimeZone instance with no time zone "
             "identifier.")
        .def(py::init<std::string>(), py::arg("time_zone_id"),
             "Constructs a QTimeZone object from a time zone identifier.")
        .def_static("available_time_zone_ids",
                    &atom::utils::QTimeZone::availableTimeZoneIds,
                    R"(Returns a list of available time zone identifiers.

Returns:
    list of str: A list of valid time zone identifiers.
)")
        .def("identifier", &atom::utils::QTimeZone::identifier,
             R"(Gets the time zone identifier.

Returns:
    str: The time zone identifier.
)")
        .def("id", &atom::utils::QTimeZone::id,
             R"(Gets the time zone identifier (alias for identifier()).

Returns:
    str: The time zone identifier.
)")
        .def("display_name", &atom::utils::QTimeZone::displayName,
             R"(Gets the display name of the time zone.

Returns:
    str: The display name of the time zone.
)")
        .def("is_valid", &atom::utils::QTimeZone::isValid,
             R"(Checks if the QTimeZone object is valid.

Returns:
    bool: True if the QTimeZone object is valid, False otherwise.
)")
        .def("offset_from_utc", &atom::utils::QTimeZone::offsetFromUtc,
             py::arg("date_time"),
             R"(Gets the offset from UTC for a specific date and time.

Args:
    date_time: The QDateTime object for which to calculate the offset.

Returns:
    datetime.timedelta: The offset from UTC.

Raises:
    RuntimeError: If the time conversion fails.
)")
        .def("standard_time_offset",
             &atom::utils::QTimeZone::standardTimeOffset,
             R"(Gets the standard time offset from UTC.

Returns:
    datetime.timedelta: The standard time offset from UTC.
)")
        .def("daylight_time_offset",
             &atom::utils::QTimeZone::daylightTimeOffset,
             R"(Gets the daylight saving time offset from UTC.

Returns:
    datetime.timedelta: The daylight saving time offset from UTC.
)")
        .def("has_daylight_time", &atom::utils::QTimeZone::hasDaylightTime,
             R"(Checks if the time zone observes daylight saving time.

Returns:
    bool: True if the time zone observes daylight saving time, False otherwise.
)")
        .def(
            "is_daylight_time", &atom::utils::QTimeZone::isDaylightTime,
            py::arg("date_time"),
            R"(Checks if a specific date and time is within the daylight saving time period.

Args:
    date_time: The QDateTime object to check.

Returns:
    bool: True if the specified date and time falls within the daylight saving time period, False otherwise.

Raises:
    RuntimeError: If the time conversion fails.
)");
}