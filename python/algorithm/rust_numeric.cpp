#include "atom/algorithm/rust_numeric.hpp"

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace atom::algorithm;

PYBIND11_MODULE(rust_numeric, m) {
    m.doc() = "Rust-like numeric types and utilities for Python";

    // Bind ErrorKind enum
    py::enum_<ErrorKind>(m, "ErrorKind", "Error kinds for numeric operations")
        .value("ParseIntError", ErrorKind::ParseIntError,
               "Error parsing integer")
        .value("ParseFloatError", ErrorKind::ParseFloatError,
               "Error parsing float")
        .value("DivideByZero", ErrorKind::DivideByZero, "Division by zero")
        .value("NumericOverflow", ErrorKind::NumericOverflow,
               "Numeric overflow")
        .value("NumericUnderflow", ErrorKind::NumericUnderflow,
               "Numeric underflow")
        .value("InvalidOperation", ErrorKind::InvalidOperation,
               "Invalid operation");

    // Bind Error class
    py::class_<Error>(m, "Error", "Error type for numeric operations")
        .def(py::init<ErrorKind, const std::string &>(), py::arg("kind"),
             py::arg("message"),
             "Create a new Error with the given kind and message")
        .def("kind", &Error::kind, "Get the error kind")
        .def("message", &Error::message, "Get the error message")
        .def("to_string", &Error::to_string, "Get the error as a string")
        .def("__str__", &Error::to_string);

    // Bind Result template for different numeric types
    // I8 Result
    py::class_<Result<i8>>(m, "I8Result", "Result type for i8 operations")
        .def(py::init<i8>(), py::arg("value"),
             "Create a new Result with a value")
        .def(py::init<Error>(), py::arg("error"),
             "Create a new Result with an error")
        .def("is_ok", &Result<i8>::is_ok, "Check if the result is Ok")
        .def("is_err", &Result<i8>::is_err, "Check if the result is Error")
        .def("unwrap", &Result<i8>::unwrap, "Get the value, throws if Error")
        .def("unwrap_or", &Result<i8>::unwrap_or, py::arg("default_value"),
             "Get the value or a default value if Error")
        .def("unwrap_err", &Result<i8>::unwrap_err,
             "Get the error, throws if Ok");

    // I32 Result
    py::class_<Result<i32>>(m, "I32Result", "Result type for i32 operations")
        .def(py::init<i32>(), py::arg("value"),
             "Create a new Result with a value")
        .def(py::init<Error>(), py::arg("error"),
             "Create a new Result with an error")
        .def("is_ok", &Result<i32>::is_ok, "Check if the result is Ok")
        .def("is_err", &Result<i32>::is_err, "Check if the result is Error")
        .def("unwrap", &Result<i32>::unwrap, "Get the value, throws if Error")
        .def("unwrap_or", &Result<i32>::unwrap_or, py::arg("default_value"),
             "Get the value or a default value if Error")
        .def("unwrap_err", &Result<i32>::unwrap_err,
             "Get the error, throws if Ok");

    // I64 Result
    py::class_<Result<i64>>(m, "I64Result", "Result type for i64 operations")
        .def(py::init<i64>(), py::arg("value"),
             "Create a new Result with a value")
        .def(py::init<Error>(), py::arg("error"),
             "Create a new Result with an error")
        .def("is_ok", &Result<i64>::is_ok, "Check if the result is Ok")
        .def("is_err", &Result<i64>::is_err, "Check if the result is Error")
        .def("unwrap", &Result<i64>::unwrap, "Get the value, throws if Error")
        .def("unwrap_or", &Result<i64>::unwrap_or, py::arg("default_value"),
             "Get the value or a default value if Error")
        .def("unwrap_err", &Result<i64>::unwrap_err,
             "Get the error, throws if Ok");

    // F32 Result
    py::class_<Result<f32>>(m, "F32Result", "Result type for f32 operations")
        .def(py::init<f32>(), py::arg("value"),
             "Create a new Result with a value")
        .def(py::init<Error>(), py::arg("error"),
             "Create a new Result with an error")
        .def("is_ok", &Result<f32>::is_ok, "Check if the result is Ok")
        .def("is_err", &Result<f32>::is_err, "Check if the result is Error")
        .def("unwrap", &Result<f32>::unwrap, "Get the value, throws if Error")
        .def("unwrap_or", &Result<f32>::unwrap_or, py::arg("default_value"),
             "Get the value or a default value if Error")
        .def("unwrap_err", &Result<f32>::unwrap_err,
             "Get the error, throws if Ok");

    // F64 Result
    py::class_<Result<f64>>(m, "F64Result", "Result type for f64 operations")
        .def(py::init<f64>(), py::arg("value"),
             "Create a new Result with a value")
        .def(py::init<Error>(), py::arg("error"),
             "Create a new Result with an error")
        .def("is_ok", &Result<f64>::is_ok, "Check if the result is Ok")
        .def("is_err", &Result<f64>::is_err, "Check if the result is Error")
        .def("unwrap", &Result<f64>::unwrap, "Get the value, throws if Error")
        .def("unwrap_or", &Result<f64>::unwrap_or, py::arg("default_value"),
             "Get the value or a default value if Error")
        .def("unwrap_err", &Result<f64>::unwrap_err,
             "Get the error, throws if Ok");

    // Option template for different numeric types
    py::class_<Option<i32>>(m, "I32Option", "Option type for i32")
        .def(py::init<>(), "Create a None option")
        .def(py::init<i32>(), py::arg("value"),
             "Create a Some option with value")
        .def("is_some", &Option<i32>::is_some, "Check if option has a value")
        .def("is_none", &Option<i32>::is_none, "Check if option has no value")
        .def("unwrap", &Option<i32>::unwrap, "Get the value, throws if None")
        .def("unwrap_or", &Option<i32>::unwrap_or, py::arg("default"),
             "Get the value or a default if None")
        .def_static("some", &Option<i32>::some, py::arg("value"),
                    "Create a Some option with value")
        .def_static("none", &Option<i32>::none, "Create a None option");

    // Option template for different numeric types
    py::class_<Option<f64>>(m, "F64Option", "Option type for f64")
        .def(py::init<>(), "Create a None option")
        .def(py::init<f64>(), py::arg("value"),
             "Create a Some option with value")
        .def("is_some", &Option<f64>::is_some, "Check if option has a value")
        .def("is_none", &Option<f64>::is_none, "Check if option has no value")
        .def("unwrap", &Option<f64>::unwrap, "Get the value, throws if None")
        .def("unwrap_or", &Option<f64>::unwrap_or, py::arg("default"),
             "Get the value or a default if None")
        .def_static("some", &Option<f64>::some, py::arg("value"),
                    "Create a Some option with value")
        .def_static("none", &Option<f64>::none, "Create a None option");

    // Bind Range template
    py::class_<Range<int>>(m, "IntRange", "Range of integers")
        .def(py::init<int, int, bool>(), py::arg("start"), py::arg("end"),
             py::arg("inclusive") = false,
             "Create a range from start to end (exclusive or inclusive)")
        .def(
            "__iter__",
            [](Range<int> &r) { return py::make_iterator(r.begin(), r.end()); })
        .def("contains", &Range<int>::contains, py::arg("value"),
             "Check if value is in range")
        .def("__contains__", &Range<int>::contains, py::arg("value"),
             "Support for the 'in' operator")
        .def("len", &Range<int>::len, "Get length of range")
        .def("__len__", &Range<int>::len, "Support for len() function")
        .def("is_empty", &Range<int>::is_empty, "Check if range is empty");

    // Helper functions for ranges
    m.def("range", &range<int>, py::arg("start"), py::arg("end"),
          "Create a range from start to end (exclusive)");
    m.def("range_inclusive", &range_inclusive<int>, py::arg("start"),
          py::arg("end"), "Create a range from start to end (inclusive)");

    // Bind integer classes
    py::class_<I8>(m, "I8", "8-bit signed integer utilities")
        .def_readonly_static("MIN", &I8::MIN, "Minimum value")
        .def_readonly_static("MAX", &I8::MAX, "Maximum value")
        .def_static("from_str", &I8::from_str, py::arg("s"),
                    py::arg("base") = 10, "Parse string to i8")
        .def_static("checked_add", &I8::checked_add, py::arg("a"), py::arg("b"),
                    "Add with overflow checking")
        .def_static("checked_sub", &I8::checked_sub, py::arg("a"), py::arg("b"),
                    "Subtract with overflow checking")
        .def_static("checked_mul", &I8::checked_mul, py::arg("a"), py::arg("b"),
                    "Multiply with overflow checking")
        .def_static("checked_div", &I8::checked_div, py::arg("a"), py::arg("b"),
                    "Divide with overflow checking")
        .def_static("saturating_add", &I8::saturating_add, py::arg("a"),
                    py::arg("b"), "Add with saturation")
        .def_static("saturating_sub", &I8::saturating_sub, py::arg("a"),
                    py::arg("b"), "Subtract with saturation")
        .def_static("wrapping_add", &I8::wrapping_add, py::arg("a"),
                    py::arg("b"), "Add with wrapping")
        .def_static("wrapping_sub", &I8::wrapping_sub, py::arg("a"),
                    py::arg("b"), "Subtract with wrapping")
        .def_static("count_ones", &I8::count_ones, py::arg("value"),
                    "Count the number of ones in the binary representation")
        .def_static("count_zeros", &I8::count_zeros, py::arg("value"),
                    "Count the number of zeros in the binary representation")
        .def_static("to_string", &I8::to_string, py::arg("value"),
                    py::arg("base") = 10, "Convert to string with given base");

    // I32 class
    py::class_<I32>(m, "I32", "32-bit signed integer utilities")
        .def_readonly_static("MIN", &I32::MIN, "Minimum value")
        .def_readonly_static("MAX", &I32::MAX, "Maximum value")
        .def_static("from_str", &I32::from_str, py::arg("s"),
                    py::arg("base") = 10, "Parse string to i32")
        .def_static("checked_add", &I32::checked_add, py::arg("a"),
                    py::arg("b"), "Add with overflow checking")
        .def_static("checked_sub", &I32::checked_sub, py::arg("a"),
                    py::arg("b"), "Subtract with overflow checking")
        .def_static("checked_mul", &I32::checked_mul, py::arg("a"),
                    py::arg("b"), "Multiply with overflow checking")
        .def_static("checked_div", &I32::checked_div, py::arg("a"),
                    py::arg("b"), "Divide with overflow checking")
        .def_static("saturating_add", &I32::saturating_add, py::arg("a"),
                    py::arg("b"), "Add with saturation")
        .def_static("saturating_sub", &I32::saturating_sub, py::arg("a"),
                    py::arg("b"), "Subtract with saturation")
        .def_static("wrapping_add", &I32::wrapping_add, py::arg("a"),
                    py::arg("b"), "Add with wrapping")
        .def_static("wrapping_sub", &I32::wrapping_sub, py::arg("a"),
                    py::arg("b"), "Subtract with wrapping")
        .def_static("count_ones", &I32::count_ones, py::arg("value"),
                    "Count the number of ones in the binary representation")
        .def_static("count_zeros", &I32::count_zeros, py::arg("value"),
                    "Count the number of zeros in the binary representation")
        .def_static("to_string", &I32::to_string, py::arg("value"),
                    py::arg("base") = 10, "Convert to string with given base")
        .def_static("to_hex_string", &I32::to_hex_string, py::arg("value"),
                    py::arg("with_prefix") = true,
                    "Convert to hexadecimal string")
        .def_static("to_bin_string", &I32::to_bin_string, py::arg("value"),
                    py::arg("with_prefix") = true, "Convert to binary string")
        .def_static("random", &I32::random, py::arg("min") = I32::MIN,
                    py::arg("max") = I32::MAX,
                    "Generate a random i32 in the given range");

    // I64 class
    py::class_<I64>(m, "I64", "64-bit signed integer utilities")
        .def_readonly_static("MIN", &I64::MIN, "Minimum value")
        .def_readonly_static("MAX", &I64::MAX, "Maximum value")
        .def_static("from_str", &I64::from_str, py::arg("s"),
                    py::arg("base") = 10, "Parse string to i64")
        .def_static("checked_add", &I64::checked_add, py::arg("a"),
                    py::arg("b"), "Add with overflow checking")
        .def_static("checked_sub", &I64::checked_sub, py::arg("a"),
                    py::arg("b"), "Subtract with overflow checking")
        .def_static("checked_mul", &I64::checked_mul, py::arg("a"),
                    py::arg("b"), "Multiply with overflow checking")
        .def_static("checked_div", &I64::checked_div, py::arg("a"),
                    py::arg("b"), "Divide with overflow checking")
        .def_static("saturating_add", &I64::saturating_add, py::arg("a"),
                    py::arg("b"), "Add with saturation")
        .def_static("saturating_sub", &I64::saturating_sub, py::arg("a"),
                    py::arg("b"), "Subtract with saturation")
        .def_static("wrapping_add", &I64::wrapping_add, py::arg("a"),
                    py::arg("b"), "Add with wrapping")
        .def_static("wrapping_sub", &I64::wrapping_sub, py::arg("a"),
                    py::arg("b"), "Subtract with wrapping")
        .def_static("count_ones", &I64::count_ones, py::arg("value"),
                    "Count the number of ones in the binary representation")
        .def_static("count_zeros", &I64::count_zeros, py::arg("value"),
                    "Count the number of zeros in the binary representation")
        .def_static("to_string", &I64::to_string, py::arg("value"),
                    py::arg("base") = 10, "Convert to string with given base");

    // U32 class
    py::class_<U32>(m, "U32", "32-bit unsigned integer utilities")
        .def_readonly_static("MIN", &U32::MIN, "Minimum value")
        .def_readonly_static("MAX", &U32::MAX, "Maximum value")
        .def_static("from_str", &U32::from_str, py::arg("s"),
                    py::arg("base") = 10, "Parse string to u32")
        .def_static("checked_add", &U32::checked_add, py::arg("a"),
                    py::arg("b"), "Add with overflow checking")
        .def_static("checked_sub", &U32::checked_sub, py::arg("a"),
                    py::arg("b"), "Subtract with overflow checking")
        .def_static("checked_mul", &U32::checked_mul, py::arg("a"),
                    py::arg("b"), "Multiply with overflow checking")
        .def_static("checked_div", &U32::checked_div, py::arg("a"),
                    py::arg("b"), "Divide with overflow checking")
        .def_static("wrapping_add", &U32::wrapping_add, py::arg("a"),
                    py::arg("b"), "Add with wrapping")
        .def_static("wrapping_sub", &U32::wrapping_sub, py::arg("a"),
                    py::arg("b"), "Subtract with wrapping")
        .def_static("to_string", &U32::to_string, py::arg("value"),
                    py::arg("base") = 10, "Convert to string with given base");

    // U64 class
    py::class_<U64>(m, "U64", "64-bit unsigned integer utilities")
        .def_readonly_static("MIN", &U64::MIN, "Minimum value")
        .def_readonly_static("MAX", &U64::MAX, "Maximum value")
        .def_static("from_str", &U64::from_str, py::arg("s"),
                    py::arg("base") = 10, "Parse string to u64")
        .def_static("checked_add", &U64::checked_add, py::arg("a"),
                    py::arg("b"), "Add with overflow checking")
        .def_static("checked_sub", &U64::checked_sub, py::arg("a"),
                    py::arg("b"), "Subtract with overflow checking")
        .def_static("checked_mul", &U64::checked_mul, py::arg("a"),
                    py::arg("b"), "Multiply with overflow checking")
        .def_static("checked_div", &U64::checked_div, py::arg("a"),
                    py::arg("b"), "Divide with overflow checking")
        .def_static("wrapping_add", &U64::wrapping_add, py::arg("a"),
                    py::arg("b"), "Add with wrapping")
        .def_static("wrapping_sub", &U64::wrapping_sub, py::arg("a"),
                    py::arg("b"), "Subtract with wrapping")
        .def_static("to_string", &U64::to_string, py::arg("value"),
                    py::arg("base") = 10, "Convert to string with given base");

    // F32 class
    py::class_<F32>(m, "F32", "32-bit floating point utilities")
        .def_readonly_static("MIN", &F32::MIN, "Minimum finite value")
        .def_readonly_static("MAX", &F32::MAX, "Maximum finite value")
        .def_readonly_static("EPSILON", &F32::EPSILON, "Machine epsilon value")
        .def_readonly_static("INFINITY", &F32::INFINITY_VAL,
                             "Positive infinity")
        .def_readonly_static("NEG_INFINITY", &F32::NEG_INFINITY,
                             "Negative infinity")
        .def_readonly_static("NAN", &F32::NAN, "Not a Number")
        .def_readonly_static("PI", &F32::PI, "π value")
        .def_readonly_static("TAU", &F32::TAU, "τ value (2π)")
        .def_readonly_static("E", &F32::E, "e value")
        .def_static("from_str", &F32::from_str, py::arg("s"),
                    "Parse string to f32")
        .def_static("is_nan", &F32::is_nan, py::arg("x"),
                    "Check if value is NaN")
        .def_static("is_infinite", &F32::is_infinite, py::arg("x"),
                    "Check if value is infinite")
        .def_static("is_finite", &F32::is_finite, py::arg("x"),
                    "Check if value is finite")
        .def_static("abs", &F32::abs, py::arg("x"), "Absolute value")
        .def_static("sqrt", &F32::sqrt, py::arg("x"), "Square root")
        .def_static("sin", &F32::sin, py::arg("x"), "Sine function")
        .def_static("cos", &F32::cos, py::arg("x"), "Cosine function")
        .def_static("to_string", &F32::to_string, py::arg("value"),
                    py::arg("precision") = 6,
                    "Convert to string with given precision");

    // F64 class
    py::class_<F64>(m, "F64", "64-bit floating point utilities")
        .def_readonly_static("MIN", &F64::MIN, "Minimum finite value")
        .def_readonly_static("MAX", &F64::MAX, "Maximum finite value")
        .def_readonly_static("EPSILON", &F64::EPSILON, "Machine epsilon value")
        .def_readonly_static("INFINITY", &F64::INFINITY_VAL,
                             "Positive infinity")
        .def_readonly_static("NEG_INFINITY", &F64::NEG_INFINITY,
                             "Negative infinity")
        .def_readonly_static("NAN", &F64::NAN, "Not a Number")
        .def_readonly_static("PI", &F64::PI, "π value")
        .def_readonly_static("TAU", &F64::TAU, "τ value (2π)")
        .def_readonly_static("E", &F64::E, "e value")
        .def_static("from_str", &F64::from_str, py::arg("s"),
                    "Parse string to f64")
        .def_static("is_nan", &F64::is_nan, py::arg("x"),
                    "Check if value is NaN")
        .def_static("is_infinite", &F64::is_infinite, py::arg("x"),
                    "Check if value is infinite")
        .def_static("is_finite", &F64::is_finite, py::arg("x"),
                    "Check if value is finite")
        .def_static("abs", &F64::abs, py::arg("x"), "Absolute value")
        .def_static("sqrt", &F64::sqrt, py::arg("x"), "Square root")
        .def_static("sin", &F64::sin, py::arg("x"), "Sine function")
        .def_static("cos", &F64::cos, py::arg("x"), "Cosine function")
        .def_static("to_string", &F64::to_string, py::arg("value"),
                    py::arg("precision") = 6,
                    "Convert to string with given precision")
        .def_static("to_exp_string", &F64::to_exp_string, py::arg("value"),
                    py::arg("precision") = 6,
                    "Convert to exponential string with given precision");

    // Add usage examples
    m.attr("__doc__") = R"(
    Rust-like numeric types and utilities for Python

    This module provides Rust-inspired numeric types and operations with
    controlled overflow behavior, checked arithmetic, and error handling patterns.

    Examples:
        >>> from atom.algorithm.rust_numeric import I32, F64
        >>> # Parse string to integer with error handling
        >>> result = I32.from_str("123")
        >>> if result.is_ok():
        >>>     print(result.unwrap())
        >>> else:
        >>>     print(result.unwrap_err())
        123

        >>> # Check for overflow when adding
        >>> option = I32.checked_add(2147483647, 1)  # MAX_INT32 + 1
        >>> if option.is_some():
        >>>     print(option.unwrap())
        >>> else:
        >>>     print("Overflow occurred")
        Overflow occurred

        >>> # Saturating operations (clamping to min/max)
        >>> saturated = I32.saturating_add(2147483647, 1000)
        >>> print(saturated)
        2147483647

        >>> # Working with ranges
        >>> from atom.algorithm.rust_numeric import range, range_inclusive
        >>> r = range(1, 5)  # 1, 2, 3, 4
        >>> for i in r:
        >>>     print(i)
        1
        2
        3
        4
    )";
}
