#include "atom/algorithm/bignumber.hpp"
#include "atom/error/exception.hpp"

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(bignumber, m) {
    m.doc() = R"pbdoc(
        BigNumber - Arbitrary Precision Integer Arithmetic
        ------------------------------------------------

        This module provides the BigNumber class for arbitrary precision integer arithmetic.
        It supports all standard arithmetic operations, comparisons, and utility functions
        for working with very large integers that exceed the range of built-in integer types.

        Features:
        - Arbitrary precision integer arithmetic
        - All standard arithmetic operations (+, -, *, /, ^)
        - Comprehensive comparison operations
        - String and integer construction
        - Utility methods for number properties
        - Efficient algorithms including Karatsuba multiplication

        Examples:
            >>> from atom.algorithm.bignumber import BigNumber
            >>>
            >>> # Create from string or integer
            >>> big1 = BigNumber("123456789012345678901234567890")
            >>> big2 = BigNumber(42)
            >>>
            >>> # Arithmetic operations
            >>> result = big1 + big2
            >>> print(result.to_string())
            >>>
            >>> # Power operations
            >>> power_result = big2 ** 10
            >>>
            >>> # Comparisons
            >>> if big1 > big2:
            >>>     print("big1 is larger")
            >>>
            >>> # Utility methods
            >>> print(f"Number of digits: {big1.digits()}")
            >>> print(f"Is even: {big1.is_even()}")
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::error::InvalidArgument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::error::RuntimeError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        }
    });

    // Register BigNumber class
    py::class_<atom::algorithm::BigNumber>(m, "BigNumber",
                                           R"pbdoc(
        A class for arbitrary precision integer arithmetic.

        The BigNumber class can represent and manipulate integers of any size,
        limited only by available memory. It provides all standard arithmetic
        operations and maintains exact precision for all calculations.
        )pbdoc")

        // Constructors
        .def(py::init<>(), "Constructs a BigNumber with value 0.")
        .def(py::init<std::string_view>(), py::arg("number"),
             R"pbdoc(
             Constructs a BigNumber from a string representation.

             Args:
                 number: String representation of the number (e.g., "123456789")

             Raises:
                 ValueError: If the string is not a valid number representation
             )pbdoc")
        .def(py::init<int>(), py::arg("number"),
             "Constructs a BigNumber from an integer.")
        .def(py::init<long>(), py::arg("number"),
             "Constructs a BigNumber from a long integer.")
        .def(py::init<long long>(), py::arg("number"),
             "Constructs a BigNumber from a long long integer.")

        // Arithmetic operations
        .def("add", &atom::algorithm::BigNumber::add, py::arg("other"),
             R"pbdoc(
             Adds another BigNumber to this one.

             Args:
                 other: The BigNumber to add

             Returns:
                 A new BigNumber containing the sum
             )pbdoc")
        .def("subtract", &atom::algorithm::BigNumber::subtract,
             py::arg("other"),
             R"pbdoc(
             Subtracts another BigNumber from this one.

             Args:
                 other: The BigNumber to subtract

             Returns:
                 A new BigNumber containing the difference
             )pbdoc")
        .def("multiply", &atom::algorithm::BigNumber::multiply,
             py::arg("other"),
             R"pbdoc(
             Multiplies this BigNumber by another.

             Args:
                 other: The BigNumber to multiply by

             Returns:
                 A new BigNumber containing the product
             )pbdoc")
        .def("divide", &atom::algorithm::BigNumber::divide, py::arg("other"),
             R"pbdoc(
             Divides this BigNumber by another.

             Args:
                 other: The BigNumber to divide by

             Returns:
                 A new BigNumber containing the quotient

             Raises:
                 ValueError: If the divisor is zero
             )pbdoc")
        .def("pow", &atom::algorithm::BigNumber::pow, py::arg("exponent"),
             R"pbdoc(
             Raises this BigNumber to the given power.

             Args:
                 exponent: The integer exponent

             Returns:
                 A new BigNumber containing the result

             Raises:
                 ValueError: If the exponent is negative
             )pbdoc")

        // String operations
        .def("to_string", &atom::algorithm::BigNumber::toString,
             R"pbdoc(
             Returns the string representation of this BigNumber.

             Returns:
                 String representation of the number
             )pbdoc")
        .def("set_string", &atom::algorithm::BigNumber::setString,
             py::arg("new_str"),
             R"pbdoc(
             Sets the value from a string representation.

             Args:
                 new_str: The new string representation

             Returns:
                 Reference to this BigNumber for chaining

             Raises:
                 ValueError: If the string is not a valid number
             )pbdoc",
             py::return_value_policy::reference_internal)

        // Utility operations
        .def("negate", &atom::algorithm::BigNumber::negate,
             R"pbdoc(
             Returns the negation of this number.

             Returns:
                 A new BigNumber with opposite sign
             )pbdoc")
        .def("abs", &atom::algorithm::BigNumber::abs,
             R"pbdoc(
             Returns the absolute value of this number.

             Returns:
                 A new BigNumber with the absolute value
             )pbdoc")
        .def("trim_leading_zeros",
             &atom::algorithm::BigNumber::trimLeadingZeros,
             R"pbdoc(
             Returns a copy with leading zeros removed.

             Returns:
                 A new BigNumber with leading zeros removed
             )pbdoc")

        // Comparison operations
        .def("equals",
             static_cast<bool (atom::algorithm::BigNumber::*)(
                 const atom::algorithm::BigNumber&) const>(
                 &atom::algorithm::BigNumber::equals),
             py::arg("other"),
             R"pbdoc(
             Checks if this BigNumber equals another BigNumber.

             Args:
                 other: The BigNumber to compare with

             Returns:
                 True if the numbers are equal
             )pbdoc")

        // Property methods
        .def("digits", &atom::algorithm::BigNumber::digits,
             R"pbdoc(
             Returns the number of digits in this BigNumber.

             Returns:
                 The count of digits
             )pbdoc")
        .def("is_negative", &atom::algorithm::BigNumber::isNegative,
             R"pbdoc(
             Checks if this BigNumber is negative.

             Returns:
                 True if the number is negative
             )pbdoc")
        .def("is_positive", &atom::algorithm::BigNumber::isPositive,
             R"pbdoc(
             Checks if this BigNumber is positive or zero.

             Returns:
                 True if the number is positive or zero
             )pbdoc")
        .def("is_even", &atom::algorithm::BigNumber::isEven,
             R"pbdoc(
             Checks if this BigNumber is even.

             Returns:
                 True if the number is even
             )pbdoc")
        .def("is_odd", &atom::algorithm::BigNumber::isOdd,
             R"pbdoc(
             Checks if this BigNumber is odd.

             Returns:
                 True if the number is odd
             )pbdoc")

        // Array-like access
        .def("at", &atom::algorithm::BigNumber::at, py::arg("index"),
             R"pbdoc(
             Accesses a digit at the specified position.

             Args:
                 index: The position to access (0 is least significant digit)

             Returns:
                 The digit at that position (0-9)

             Raises:
                 IndexError: If the index is out of range
             )pbdoc")
        .def("__getitem__", &atom::algorithm::BigNumber::at, py::arg("index"),
             "Support for indexing with [] operator.")

        // Python special methods
        .def("__str__", &atom::algorithm::BigNumber::toString,
             "String representation for str() function.")
        .def(
            "__repr__",
            [](const atom::algorithm::BigNumber& bn) {
                return "BigNumber('" + bn.toString() + "')";
            },
            "String representation for repr() function.")
        .def("__len__", &atom::algorithm::BigNumber::digits,
             "Support for len() function - returns number of digits.")
        .def(
            "__bool__",
            [](const atom::algorithm::BigNumber& bn) {
                return !bn.equals(atom::algorithm::BigNumber(0));
            },
            "Support for boolean evaluation - False if zero, True otherwise.")

        // Arithmetic operators
        .def(py::self + py::self)
        .def(py::self - py::self)
        .def(py::self * py::self)
        .def(py::self / py::self)
        .def("__pow__", &atom::algorithm::BigNumber::pow, py::arg("exponent"))

        // Comparison operators
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def(py::self < py::self)
        .def(py::self <= py::self)
        .def(py::self > py::self)
        .def(py::self >= py::self)

        // In-place arithmetic operators
        .def(py::self += py::self)
        .def(py::self -= py::self)
        .def(py::self *= py::self)
        .def(py::self /= py::self)

        // Increment/decrement operators
        .def(
            "__iadd__",
            [](atom::algorithm::BigNumber& bn,
               int) -> atom::algorithm::BigNumber& { return ++bn; },
            "Pre-increment operator (++bn)")
        .def(
            "__isub__",
            [](atom::algorithm::BigNumber& bn,
               int) -> atom::algorithm::BigNumber& { return --bn; },
            "Pre-decrement operator (--bn)");
}
