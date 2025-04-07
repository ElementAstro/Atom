#include "atom/utils/time.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(time, m) {
    m.doc() = "Time utilities module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::utils::TimeConvertException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Register TimeConvertException
    py::register_exception<atom::utils::TimeConvertException>(
        m, "TimeConvertException");

    // Time utility functions
    m.def("validate_timestamp_format", &atom::utils::validateTimestampFormat,
          py::arg("timestamp_str"), py::arg("format") = "%Y-%m-%d %H:%M:%S",
          R"(Validates a timestamp string against a specified format.

Args:
    timestamp_str: The timestamp string to validate
    format: The expected format (default: "%Y-%m-%d %H:%M:%S")

Returns:
    bool: True if the timestamp matches the format, False otherwise

Examples:
    >>> from atom.utils.time import validate_timestamp_format
    >>> validate_timestamp_format("2023-10-27 12:34:56")
    True
    >>> validate_timestamp_format("2023/10/27", "%Y/%m/%d")
    True
)");

    m.def("get_timestamp_string", &atom::utils::getTimestampString,
          R"(Retrieves the current timestamp as a formatted string.

This function returns the current local time formatted as a string with the
pattern "%Y-%m-%d %H:%M:%S".

Returns:
    str: The current timestamp formatted as "%Y-%m-%d %H:%M:%S"
    
Raises:
    TimeConvertException: If time conversion fails

Examples:
    >>> from atom.utils.time import get_timestamp_string
    >>> get_timestamp_string()
    "2023-10-27 14:30:45"
)");

    m.def("convert_to_china_time", &atom::utils::convertToChinaTime,
          py::arg("utc_time_str"),
          R"(Converts a UTC time string to China Standard Time (CST, UTC+8).

This function takes a UTC time string formatted as "%Y-%m-%d %H:%M:%S" and
converts it to China Standard Time (CST), returning the time as a string with
the same format.

Args:
    utc_time_str: A string representing the UTC time in the format "%Y-%m-%d %H:%M:%S"

Returns:
    str: The corresponding time in China Standard Time, formatted as "%Y-%m-%d %H:%M:%S"
    
Raises:
    TimeConvertException: If the input format is invalid or conversion fails

Examples:
    >>> from atom.utils.time import convert_to_china_time
    >>> convert_to_china_time("2023-10-27 06:30:00")
    "2023-10-27 14:30:00"
)");

    m.def(
        "get_china_timestamp_string", &atom::utils::getChinaTimestampString,
        R"(Retrieves the current China Standard Time (CST) as a formatted timestamp string.

This function returns the current local time in China Standard Time (CST),
formatted as a string with the pattern "%Y-%m-%d %H:%M:%S".

Returns:
    str: The current China Standard Time formatted as "%Y-%m-%d %H:%M:%S"
    
Raises:
    TimeConvertException: If time conversion fails

Examples:
    >>> from atom.utils.time import get_china_timestamp_string
    >>> get_china_timestamp_string()
    "2023-10-27 14:30:45"
)");

    m.def("timestamp_to_string", &atom::utils::timeStampToString,
          py::arg("timestamp"), py::arg("format") = "%Y-%m-%d %H:%M:%S",
          R"(Converts a timestamp to a formatted string.

This function takes a timestamp (in seconds since the Unix epoch) and
converts it to a string representation.

Args:
    timestamp: The timestamp to be converted, in seconds since the Unix epoch
    format: The format string (default: "%Y-%m-%d %H:%M:%S")

Returns:
    str: The string representation of the timestamp
    
Raises:
    TimeConvertException: If the timestamp is invalid or conversion fails

Examples:
    >>> from atom.utils.time import timestamp_to_string
    >>> timestamp_to_string(1698424245)
    "2023-10-27 14:30:45"
    >>> timestamp_to_string(1698424245, "%Y/%m/%d")
    "2023/10/27"
)");

    m.def(
        "to_string",
        [](const std::tm& tm, std::string_view format) {
            return atom::utils::toString(tm, format);
        },
        py::arg("tm"), py::arg("format"),
        R"(Converts a tm structure to a formatted string.

This function takes a tm structure representing a date and time and
converts it to a formatted string according to the specified format.

Args:
    tm: The tm structure to be converted to a string
    format: The format string (e.g., "%Y-%m-%d %H:%M:%S")

Returns:
    str: The formatted time string based on the tm structure and format
    
Raises:
    TimeConvertException: If formatting fails

Examples:
    >>> import time
    >>> from atom.utils.time import to_string
    >>> tm = time.localtime()
    >>> to_string(tm, "%Y-%m-%d %H:%M:%S")
    "2023-10-27 14:30:45"
)");

    m.def("get_utc_time", &atom::utils::getUtcTime,
          R"(Retrieves the current UTC time as a formatted string.

This function returns the current UTC time formatted as a string with the
pattern "%Y-%m-%d %H:%M:%S".

Returns:
    str: The current UTC time formatted as "%Y-%m-%d %H:%M:%S"
    
Raises:
    TimeConvertException: If time conversion fails

Examples:
    >>> from atom.utils.time import get_utc_time
    >>> get_utc_time()
    "2023-10-27 06:30:45"
)");

    m.def("timestamp_to_time", &atom::utils::timestampToTime,
          py::arg("timestamp"),
          R"(Converts a timestamp to a tm structure.

This function takes a timestamp (in seconds since the Unix epoch) and
converts it to a tm structure, which represents a calendar date and time.

Args:
    timestamp: The timestamp to be converted, in seconds since the Unix epoch

Returns:
    Optional[tm]: The corresponding tm structure representing the timestamp, 
                  or None if conversion fails

Examples:
    >>> from atom.utils.time import timestamp_to_time
    >>> tm = timestamp_to_time(1698424245)
    >>> tm.tm_year + 1900  # tm_year is years since 1900
    2023
)");

    m.def(
        "get_elapsed_milliseconds",
        [](const py::object& time_point) {
            // Convert Python datetime to std::chrono::time_point
            try {
                auto tp =
                    time_point.cast<std::chrono::steady_clock::time_point>();
                return atom::utils::getElapsedMilliseconds(tp);
            } catch (const py::cast_error&) {
                try {
                    auto tp =
                        time_point
                            .cast<std::chrono::system_clock::time_point>();
                    return atom::utils::getElapsedMilliseconds<
                        std::chrono::system_clock>(tp);
                } catch (const py::cast_error&) {
                    throw py::type_error("Expected a valid time_point object");
                }
            }
        },
        py::arg("start_time"),
        R"(Get time elapsed since a specific time point in milliseconds.

Args:
    start_time: The starting time point

Returns:
    int: Elapsed time in milliseconds
    
Raises:
    TypeError: If the input is not a valid time point

Examples:
    >>> import time
    >>> from atom.utils.time import get_elapsed_milliseconds
    >>> from datetime import datetime
    >>> start = datetime.now()
    >>> time.sleep(1)  # Sleep for 1 second
    >>> elapsed = get_elapsed_milliseconds(start)
    >>> elapsed >= 1000  # At least 1000 ms elapsed
    True
)");

    // Additional Python-specific utility functions
    m.def(
        "now", []() { return std::chrono::system_clock::now(); },
        R"(Returns the current time as a time_point object.

Returns:
    time_point: The current time

Examples:
    >>> from atom.utils.time import now, get_elapsed_milliseconds
    >>> import time
    >>> start = now()
    >>> time.sleep(0.5)  # Sleep for 0.5 seconds
    >>> elapsed = get_elapsed_milliseconds(start)
    >>> 450 <= elapsed <= 550  # About 500 ms with some tolerance
    True
)");

    m.def(
        "format_time",
        [](double milliseconds) {
            int hours = static_cast<int>(milliseconds / (1000 * 60 * 60));
            milliseconds -= hours * (1000 * 60 * 60);

            int minutes = static_cast<int>(milliseconds / (1000 * 60));
            milliseconds -= minutes * (1000 * 60);

            int seconds = static_cast<int>(milliseconds / 1000);
            milliseconds -= seconds * 1000;

            char buffer[16];
            snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d", hours,
                     minutes, seconds, static_cast<int>(milliseconds));
            return std::string(buffer);
        },
        py::arg("milliseconds"),
        R"(Formats time in milliseconds to HH:MM:SS.mmm format.

Args:
    milliseconds: Time in milliseconds

Returns:
    str: Formatted time string

Examples:
    >>> from atom.utils.time import format_time
    >>> format_time(3661234)  # 1 hour, 1 minute, 1.234 seconds
    "01:01:01.234"
)");

    m.def(
        "parse_time_format",
        [](const std::string& time_str, const std::string& format) {
            std::tm tm = {};
            std::istringstream ss(time_str);
            ss >> std::get_time(&tm, format.c_str());

            if (ss.fail()) {
                throw py::value_error(
                    "Failed to parse time string with the given format");
            }

            return tm;
        },
        py::arg("time_str"), py::arg("format"),
        R"(Parse a time string according to a specified format.

Args:
    time_str: The time string to parse
    format: The format string (e.g., "%Y-%m-%d %H:%M:%S")

Returns:
    tm: The parsed time as a tm structure
    
Raises:
    ValueError: If parsing fails

Examples:
    >>> from atom.utils.time import parse_time_format
    >>> tm = parse_time_format("2023-10-27 14:30:45", "%Y-%m-%d %H:%M:%S")
    >>> tm.tm_year + 1900
    2023
)");

    m.def(
        "time_diff",
        [](const std::string& time1, const std::string& time2,
           const std::string& format = "%Y-%m-%d %H:%M:%S") {
            std::tm tm1 = {};
            std::tm tm2 = {};

            std::istringstream ss1(time1);
            ss1 >> std::get_time(&tm1, format.c_str());
            if (ss1.fail()) {
                throw py::value_error("Failed to parse first time string");
            }

            std::istringstream ss2(time2);
            ss2 >> std::get_time(&tm2, format.c_str());
            if (ss2.fail()) {
                throw py::value_error("Failed to parse second time string");
            }

            std::time_t t1 = std::mktime(&tm1);
            std::time_t t2 = std::mktime(&tm2);

            if (t1 == -1 || t2 == -1) {
                throw py::value_error("Invalid time conversion");
            }

            return std::difftime(t2, t1);  // t2 - t1 in seconds
        },
        py::arg("time1"), py::arg("time2"),
        py::arg("format") = "%Y-%m-%d %H:%M:%S",
        R"(Calculate the difference between two time strings in seconds.

Args:
    time1: The first time string
    time2: The second time string
    format: The format of the time strings (default: "%Y-%m-%d %H:%M:%S")

Returns:
    float: The difference in seconds (time2 - time1)
    
Raises:
    ValueError: If parsing or conversion fails

Examples:
    >>> from atom.utils.time import time_diff
    >>> diff = time_diff("2023-10-27 14:30:00", "2023-10-27 14:30:30")
    >>> diff
    30.0
)");
}