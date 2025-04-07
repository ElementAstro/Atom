#include "atom/algorithm/fraction.hpp"

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(fraction, m) {
    m.doc() = R"pbdoc(
        Fraction implementation module
        -----------------------------

        This module provides a robust fraction class for exact rational arithmetic.
        
        The Fraction class represents rational numbers as a numerator and denominator,
        always keeping the fraction in reduced form. It supports all standard arithmetic
        operations, comparison, conversion to various types, and additional utilities.

        Example:
            >>> from atom.algorithm import fraction
            >>> 
            >>> # Create fractions
            >>> a = fraction.Fraction(1, 2)    # 1/2
            >>> b = fraction.Fraction(3, 4)    # 3/4
            >>> 
            >>> # Arithmetic operations
            >>> c = a + b                      # 5/4
            >>> print(c)                       # "5/4"
            >>> 
            >>> # Converting from floats
            >>> d = fraction.make_fraction(0.333333)  # Approximate as a fraction
            >>> print(d)                       # "1/3" or a close approximation
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::algorithm::FractionException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Define the Fraction class
    py::class_<atom::algorithm::Fraction>(
        m, "Fraction",
        R"(Represents a fraction as a numerator and denominator.

This class provides exact representation of rational numbers and implements
various arithmetic operations, comparisons, and conversions.

Args:
    numerator: The numerator of the fraction (default: 0)
    denominator: The denominator of the fraction (default: 1)

Raises:
    ValueError: If denominator is zero

Examples:
    >>> from atom.algorithm.fraction import Fraction
    >>> 
    >>> # Create a fraction
    >>> f1 = Fraction(1, 2)    # 1/2
    >>> f2 = Fraction(3, 4)    # 3/4
    >>> 
    >>> # Basic arithmetic
    >>> f3 = f1 + f2           # 5/4
    >>> f4 = f1 * f2           # 3/8
    >>> 
    >>> # Comparisons
    >>> f1 < f2                # True
    >>> 
    >>> # Conversion
    >>> float(f1)              # 0.5
)")
        // Constructors
        .def(py::init<>(),
             "Default constructor. Creates a fraction with value 0/1.")
        .def(py::init<int>(), py::arg("numerator"),
             "Constructs a fraction from an integer.")
        .def(py::init<int, int>(), py::arg("numerator"), py::arg("denominator"),
             "Constructs a fraction with the given numerator and denominator.")

        // Arithmetic operators
        .def(py::self += py::self, "Add a fraction to this fraction.")
        .def(py::self -= py::self, "Subtract a fraction from this fraction.")
        .def(py::self *= py::self,
             "Multiply this fraction by another fraction.")
        .def(py::self /= py::self, "Divide this fraction by another fraction.")

        .def(py::self + py::self, "Add two fractions.")
        .def(py::self - py::self, "Subtract two fractions.")
        .def(py::self * py::self, "Multiply two fractions.")
        .def(py::self / py::self, "Divide two fractions.")

        // Comparison operators
        .def(py::self == py::self, "Check if two fractions are equal.")
        .def(py::self != py::self, "Check if two fractions are not equal.")
        .def(py::self < py::self,
             "Check if this fraction is less than another.")
        .def(py::self <= py::self,
             "Check if this fraction is less than or equal to another.")
        .def(py::self > py::self,
             "Check if this fraction is greater than another.")
        .def(py::self >= py::self,
             "Check if this fraction is greater than or equal to another.")

        // Type conversions
        .def(
            "__float__",
            [](const atom::algorithm::Fraction& f) {
                return static_cast<double>(f);
            },
            "Convert to float.")
        .def(
            "__int__",
            [](const atom::algorithm::Fraction& f) {
                return static_cast<int>(f);
            },
            "Convert to int (truncates towards zero).")
        .def("__str__", &atom::algorithm::Fraction::toString,
             "Return string representation of the fraction.")
        .def(
            "__repr__",
            [](const atom::algorithm::Fraction& f) {
                return "Fraction(" + f.toString() + ")";
            },
            "Return string representation for repr().")

        // Methods
        .def("to_string", &atom::algorithm::Fraction::toString,
             R"(Converts the fraction to a string representation.

Returns:
    String in the format "numerator/denominator" or just "numerator" when denominator is 1.
)")
        .def("to_double", &atom::algorithm::Fraction::toDouble,
             R"(Converts the fraction to a floating-point value.

Returns:
    The fraction as a double precision floating-point value.
)")
        .def("invert", &atom::algorithm::Fraction::invert,
             R"(Inverts the fraction (reciprocal).

Returns:
    Reference to the modified fraction.

Raises:
    ValueError: If the numerator is zero.
)")
        .def("abs", &atom::algorithm::Fraction::abs,
             R"(Returns the absolute value of the fraction.

Returns:
    A new Fraction representing the absolute value.
)")
        .def("is_zero", &atom::algorithm::Fraction::isZero,
             R"(Checks if the fraction is zero.

Returns:
    True if the fraction is zero, False otherwise.
)")
        .def("is_positive", &atom::algorithm::Fraction::isPositive,
             R"(Checks if the fraction is positive.

Returns:
    True if the fraction is positive, False otherwise.
)")
        .def("is_negative", &atom::algorithm::Fraction::isNegative,
             R"(Checks if the fraction is negative.

Returns:
    True if the fraction is negative, False otherwise.
)");

    // Factory functions
    m.def("make_fraction",
          py::overload_cast<int>(&atom::algorithm::makeFraction),
          py::arg("value"),
          R"(Creates a Fraction from an integer.

Args:
    value: The integer value.

Returns:
    A Fraction representing the integer.

Examples:
    >>> from atom.algorithm.fraction import make_fraction
    >>> f = make_fraction(5)  # Creates Fraction(5, 1)
)");

    m.def("make_fraction",
          py::overload_cast<double, int>(&atom::algorithm::makeFraction),
          py::arg("value"), py::arg("max_denominator") = 1000000,
          R"(Creates a Fraction from a double by approximating it.

Args:
    value: The double value to approximate.
    max_denominator: The maximum allowed denominator to limit the approximation (default: 1000000).

Returns:
    A Fraction approximating the double value.

Examples:
    >>> from atom.algorithm.fraction import make_fraction
    >>> f = make_fraction(0.333333)  # Approximates to something close to 1/3
    >>> f = make_fraction(0.5)       # Creates Fraction(1, 2)
    >>> f = make_fraction(3.14159, 100)  # Approximates π with max denominator 100
)");

    // Additional convenience functions for Python
    m.def(
        "gcd",
        [](int a, int b) {
            a = std::abs(a);
            b = std::abs(b);
            while (b != 0) {
                int temp = b;
                b = a % b;
                a = temp;
            }
            return a;
        },
        py::arg("a"), py::arg("b"),
        R"(Calculates the greatest common divisor (GCD) of two integers.

Args:
    a: First integer
    b: Second integer

Returns:
    The greatest common divisor of a and b

Examples:
    >>> from atom.algorithm.fraction import gcd
    >>> gcd(12, 8)  # Returns 4
    >>> gcd(17, 5)  # Returns 1
)");

    m.def(
        "lcm",
        [](int a, int b) {
            a = std::abs(a);
            b = std::abs(b);
            if (a == 0 || b == 0)
                return 0;

            // Calculate GCD first
            int g = a;
            int temp = b;
            while (temp != 0) {
                int t = temp;
                temp = g % temp;
                g = t;
            }

            // LCM = (a * b) / gcd(a, b)
            return (a / g) * b;  // Divide first to prevent overflow
        },
        py::arg("a"), py::arg("b"),
        R"(Calculates the least common multiple (LCM) of two integers.

Args:
    a: First integer
    b: Second integer

Returns:
    The least common multiple of a and b

Examples:
    >>> from atom.algorithm.fraction import lcm
    >>> lcm(4, 6)   # Returns 12
    >>> lcm(15, 25) # Returns 75
)");
}