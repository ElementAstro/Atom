/*
 * numerical.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * Python bindings for numerical methods from atom/algorithm/math/numerical.hpp
 */

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/algorithm/math/numerical.hpp"

namespace py = pybind11;
using namespace atom::algorithm;

PYBIND11_MODULE(numerical, m) {
    m.doc() = R"pbdoc(
        Numerical Methods Module
        ------------------------

        This module provides numerical algorithms for solving equations and optimization:
        - Root finding (Newton-Raphson, bisection, secant method)
        - Numerical integration (trapezoidal, Simpson's rule)
        - Numerical differentiation

        Example:
            >>> from atom.algorithm.math import numerical
            >>> # Find root of x^2 - 2 using Newton-Raphson
            >>> f = lambda x: x**2 - 2
            >>> df = lambda x: 2*x
            >>> root = numerical.newton_raphson(f, df, 1.0)
            >>> print(f"sqrt(2) ≈ {root}")
    )pbdoc";

    // NumericalMethods<double> bindings
    using NM = NumericalMethods<double>;

    // Newton-Raphson method
    m.def(
        "newton_raphson",
        [](std::function<double(double)> f, std::function<double(double)> df,
           double initial_guess, double tolerance, size_t max_iterations) {
            return NM::newtonRaphson(f, df, initial_guess, tolerance,
                                     max_iterations);
        },
        py::arg("f"), py::arg("df"), py::arg("initial_guess"),
        py::arg("tolerance") = 1e-10, py::arg("max_iterations") = 100,
        R"pbdoc(
            Find root using Newton-Raphson method.

            Args:
                f: Function to find root of
                df: Derivative of the function
                initial_guess: Initial guess for the root
                tolerance: Convergence tolerance (default: 1e-10)
                max_iterations: Maximum number of iterations (default: 100)

            Returns:
                Root if found, None otherwise

            Example:
                >>> f = lambda x: x**2 - 2
                >>> df = lambda x: 2*x
                >>> root = newton_raphson(f, df, 1.0)
        )pbdoc");

    // Bisection method
    m.def(
        "bisection",
        [](std::function<double(double)> f, double a, double b,
           double tolerance, size_t max_iterations) {
            return NM::bisection(f, a, b, tolerance, max_iterations);
        },
        py::arg("f"), py::arg("a"), py::arg("b"), py::arg("tolerance") = 1e-10,
        py::arg("max_iterations") = 100,
        R"pbdoc(
            Find root using bisection method.

            Args:
                f: Function to find root of
                a: Left boundary (f(a) and f(b) must have opposite signs)
                b: Right boundary
                tolerance: Convergence tolerance (default: 1e-10)
                max_iterations: Maximum number of iterations (default: 100)

            Returns:
                Root if found, None otherwise

            Example:
                >>> f = lambda x: x**3 - x - 2
                >>> root = bisection(f, 1.0, 2.0)
        )pbdoc");

    // Secant method
    m.def(
        "secant",
        [](std::function<double(double)> f, double x0, double x1,
           double tolerance, size_t max_iterations) {
            return NM::secant(f, x0, x1, tolerance, max_iterations);
        },
        py::arg("f"), py::arg("x0"), py::arg("x1"),
        py::arg("tolerance") = 1e-10, py::arg("max_iterations") = 100,
        R"pbdoc(
            Find root using secant method.

            Args:
                f: Function to find root of
                x0: First initial guess
                x1: Second initial guess
                tolerance: Convergence tolerance (default: 1e-10)
                max_iterations: Maximum number of iterations (default: 100)

            Returns:
                Root if found, None otherwise

            Example:
                >>> f = lambda x: x**2 - 3
                >>> root = secant(f, 1.0, 2.0)
        )pbdoc");

    // Trapezoidal rule
    m.def(
        "trapezoidal_rule",
        [](std::function<double(double)> f, double a, double b, size_t n) {
            return NM::trapezoidalRule(f, a, b, n);
        },
        py::arg("f"), py::arg("a"), py::arg("b"), py::arg("n"),
        R"pbdoc(
            Numerical integration using trapezoidal rule.

            Args:
                f: Function to integrate
                a: Lower bound
                b: Upper bound
                n: Number of intervals

            Returns:
                Approximate integral value

            Example:
                >>> import math
                >>> f = lambda x: x**2
                >>> integral = trapezoidal_rule(f, 0.0, 1.0, 1000)
                >>> print(f"∫x² dx from 0 to 1 ≈ {integral}")  # Should be ~0.333
        )pbdoc");

    // Simpson's rule
    m.def(
        "simpsons_rule",
        [](std::function<double(double)> f, double a, double b, size_t n) {
            return NM::simpsonsRule(f, a, b, n);
        },
        py::arg("f"), py::arg("a"), py::arg("b"), py::arg("n"),
        R"pbdoc(
            Numerical integration using Simpson's rule.

            Args:
                f: Function to integrate
                a: Lower bound
                b: Upper bound
                n: Number of intervals (must be even)

            Returns:
                Approximate integral value

            Example:
                >>> import math
                >>> f = lambda x: math.exp(x)
                >>> integral = simpsons_rule(f, 0.0, 1.0, 100)
                >>> print(f"∫e^x dx from 0 to 1 ≈ {integral}")  # Should be ~1.718
        )pbdoc");

    // Central difference (numerical derivative)
    m.def(
        "central_difference",
        [](std::function<double(double)> f, double x, double h) {
            return NM::centralDifference(f, x, h);
        },
        py::arg("f"), py::arg("x"), py::arg("h") = 1e-5,
        R"pbdoc(
            Numerical differentiation using central difference.

            Args:
                f: Function to differentiate
                x: Point at which to evaluate derivative
                h: Step size (default: 1e-5)

            Returns:
                Approximate derivative value

            Example:
                >>> import math
                >>> f = lambda x: math.sin(x)
                >>> derivative = central_difference(f, math.pi/4)
                >>> print(f"d/dx sin(x) at π/4 ≈ {derivative}")  # Should be ~0.707
        )pbdoc");

    // Forward difference
    m.def(
        "forward_difference",
        [](std::function<double(double)> f, double x, double h) {
            return NM::forwardDifference(f, x, h);
        },
        py::arg("f"), py::arg("x"), py::arg("h") = 1e-5,
        R"pbdoc(
            Numerical differentiation using forward difference.

            Args:
                f: Function to differentiate
                x: Point at which to evaluate derivative
                h: Step size (default: 1e-5)

            Returns:
                Approximate derivative value
        )pbdoc");

    // Backward difference
    m.def(
        "backward_difference",
        [](std::function<double(double)> f, double x, double h) {
            return NM::backwardDifference(f, x, h);
        },
        py::arg("f"), py::arg("x"), py::arg("h") = 1e-5,
        R"pbdoc(
            Numerical differentiation using backward difference.

            Args:
                f: Function to differentiate
                x: Point at which to evaluate derivative
                h: Step size (default: 1e-5)

            Returns:
                Approximate derivative value
        )pbdoc");

    // Gaussian quadrature
    m.def(
        "gaussian_quadrature",
        [](std::function<double(double)> f, double a, double b, size_t n) {
            return NM::gaussianQuadrature(f, a, b, n);
        },
        py::arg("f"), py::arg("a"), py::arg("b"), py::arg("n") = 5,
        R"pbdoc(
            Numerical integration using Gaussian quadrature.

            Args:
                f: Function to integrate
                a: Lower bound
                b: Upper bound
                n: Number of points (default: 5)

            Returns:
                Approximate integral value

            Note:
                Gaussian quadrature is more accurate than trapezoidal or
                Simpson's rule for smooth functions with fewer evaluations.
        )pbdoc");

    // Romberg integration
    m.def(
        "romberg",
        [](std::function<double(double)> f, double a, double b,
           size_t max_steps, double tolerance) {
            return NM::romberg(f, a, b, max_steps, tolerance);
        },
        py::arg("f"), py::arg("a"), py::arg("b"), py::arg("max_steps") = 10,
        py::arg("tolerance") = 1e-10,
        R"pbdoc(
            Numerical integration using Romberg's method.

            Args:
                f: Function to integrate
                a: Lower bound
                b: Upper bound
                max_steps: Maximum number of refinement steps (default: 10)
                tolerance: Convergence tolerance (default: 1e-10)

            Returns:
                Approximate integral value

            Note:
                Romberg integration uses Richardson extrapolation to improve
                the accuracy of the trapezoidal rule.
        )pbdoc");
}
