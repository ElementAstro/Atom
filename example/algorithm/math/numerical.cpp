/*
 * numerical.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * Example demonstrating numerical methods from
 * atom/algorithm/math/numerical.hpp
 */

#include "atom/algorithm/math/numerical.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>

// Define M_PI and M_E if not available (Windows compatibility)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

using namespace atom::algorithm;

// Demonstrate Newton-Raphson root finding
void demonstrateNewtonRaphson() {
    std::cout << "\n=== Newton-Raphson Root Finding ===\n";

    using NM = NumericalMethods<f64>;

    // Find root of f(x) = x^2 - 2 (should be sqrt(2))
    auto f1 = [](f64 x) { return x * x - 2.0; };
    auto df1 = [](f64 x) { return 2.0 * x; };

    auto root1 = NM::newtonRaphson(f1, df1, 1.0);
    if (root1) {
        std::cout << "Root of x^2 - 2 = 0: " << std::fixed
                  << std::setprecision(10) << *root1 << "\n";
        std::cout << "Expected (sqrt(2)): " << std::sqrt(2.0) << "\n";
        std::cout << "Error: " << std::abs(*root1 - std::sqrt(2.0)) << "\n";
    }

    // Find root of f(x) = cos(x) - x (should be ~0.739085)
    auto f2 = [](f64 x) { return std::cos(x) - x; };
    auto df2 = [](f64 x) { return -std::sin(x) - 1.0; };

    auto root2 = NM::newtonRaphson(f2, df2, 0.5);
    if (root2) {
        std::cout << "\nRoot of cos(x) - x = 0: " << *root2 << "\n";
        std::cout << "Verification: cos(" << *root2 << ") - " << *root2 << " = "
                  << f2(*root2) << "\n";
    }

    // Find root of f(x) = e^x - 3x (multiple roots)
    auto f3 = [](f64 x) { return std::exp(x) - 3.0 * x; };
    auto df3 = [](f64 x) { return std::exp(x) - 3.0; };

    auto root3a = NM::newtonRaphson(f3, df3, 0.5);
    auto root3b = NM::newtonRaphson(f3, df3, 2.0);

    std::cout << "\nRoots of e^x - 3x = 0:\n";
    if (root3a) {
        std::cout << "  Root 1: " << *root3a << " (f(x) = " << f3(*root3a)
                  << ")\n";
    }
    if (root3b) {
        std::cout << "  Root 2: " << *root3b << " (f(x) = " << f3(*root3b)
                  << ")\n";
    }
}

// Demonstrate bisection method
void demonstrateBisection() {
    std::cout << "\n=== Bisection Method ===\n";

    using NM = NumericalMethods<f64>;

    // Find root of f(x) = x^3 - x - 2 in [1, 2]
    auto f = [](f64 x) { return x * x * x - x - 2.0; };

    auto root = NM::bisection(f, 1.0, 2.0);
    if (root) {
        std::cout << "Root of x^3 - x - 2 = 0 in [1, 2]: " << std::fixed
                  << std::setprecision(10) << *root << "\n";
        std::cout << "Verification: f(" << *root << ") = " << f(*root) << "\n";
    }

    // Find root of sin(x) in [3, 4] (should be pi)
    auto sin_f = [](f64 x) { return std::sin(x); };

    auto pi_root = NM::bisection(sin_f, 3.0, 4.0);
    if (pi_root) {
        std::cout << "\nRoot of sin(x) = 0 in [3, 4]: " << *pi_root << "\n";
        std::cout << "Expected (pi): " << M_PI << "\n";
        std::cout << "Error: " << std::abs(*pi_root - M_PI) << "\n";
    }
}

// Demonstrate secant method
void demonstrateSecantMethod() {
    std::cout << "\n=== Secant Method ===\n";

    using NM = NumericalMethods<f64>;

    // Find root of f(x) = x^2 - 3
    auto f = [](f64 x) { return x * x - 3.0; };

    auto root = NM::secant(f, 1.0, 2.0);
    if (root) {
        std::cout << "Root of x^2 - 3 = 0: " << std::fixed
                  << std::setprecision(10) << *root << "\n";
        std::cout << "Expected (sqrt(3)): " << std::sqrt(3.0) << "\n";
        std::cout << "Error: " << std::abs(*root - std::sqrt(3.0)) << "\n";
    }

    // Find root of ln(x) - 1 (should be e)
    auto log_f = [](f64 x) { return std::log(x) - 1.0; };

    auto e_root = NM::secant(log_f, 2.0, 3.0);
    if (e_root) {
        std::cout << "\nRoot of ln(x) - 1 = 0: " << *e_root << "\n";
        std::cout << "Expected (e): " << M_E << "\n";
        std::cout << "Error: " << std::abs(*e_root - M_E) << "\n";
    }
}

// Demonstrate trapezoidal rule integration
void demonstrateTrapezoidalRule() {
    std::cout << "\n=== Trapezoidal Rule Integration ===\n";

    using NM = NumericalMethods<f64>;

    // Integrate x^2 from 0 to 1 (exact: 1/3)
    auto f1 = [](f64 x) { return x * x; };

    std::cout << "Integral of x^2 from 0 to 1:\n";
    std::cout << "  Exact: " << 1.0 / 3.0 << "\n";

    for (usize n : {10, 100, 1000, 10000}) {
        f64 result = NM::trapezoidalRule(f1, 0.0, 1.0, n);
        f64 error = std::abs(result - 1.0 / 3.0);
        std::cout << "  n=" << std::setw(5) << n << ": " << std::fixed
                  << std::setprecision(10) << result << " (error: " << error
                  << ")\n";
    }

    // Integrate sin(x) from 0 to pi (exact: 2)
    auto f2 = [](f64 x) { return std::sin(x); };

    std::cout << "\nIntegral of sin(x) from 0 to pi:\n";
    std::cout << "  Exact: 2.0\n";

    for (usize n : {10, 100, 1000}) {
        f64 result = NM::trapezoidalRule(f2, 0.0, M_PI, n);
        f64 error = std::abs(result - 2.0);
        std::cout << "  n=" << std::setw(5) << n << ": " << std::fixed
                  << std::setprecision(10) << result << " (error: " << error
                  << ")\n";
    }
}

// Demonstrate Simpson's rule integration
void demonstrateSimpsonRule() {
    std::cout << "\n=== Simpson's Rule Integration ===\n";

    using NM = NumericalMethods<f64>;

    // Integrate e^x from 0 to 1 (exact: e - 1)
    auto f = [](f64 x) { return std::exp(x); };
    f64 exact = M_E - 1.0;

    std::cout << "Integral of e^x from 0 to 1:\n";
    std::cout << "  Exact: " << exact << "\n";

    for (usize n : {10, 100, 1000}) {
        f64 result = NM::simpsonsRule(f, 0.0, 1.0, n);
        f64 error = std::abs(result - exact);
        std::cout << "  n=" << std::setw(5) << n << ": " << std::fixed
                  << std::setprecision(10) << result << " (error: " << error
                  << ")\n";
    }

    // Integrate 1/(1+x^2) from 0 to 1 (exact: pi/4)
    auto f2 = [](f64 x) { return 1.0 / (1.0 + x * x); };
    f64 exact2 = M_PI / 4.0;

    std::cout << "\nIntegral of 1/(1+x^2) from 0 to 1:\n";
    std::cout << "  Exact (pi/4): " << exact2 << "\n";

    for (usize n : {10, 100, 1000}) {
        f64 result = NM::simpsonsRule(f2, 0.0, 1.0, n);
        f64 error = std::abs(result - exact2);
        std::cout << "  n=" << std::setw(5) << n << ": " << std::fixed
                  << std::setprecision(10) << result << " (error: " << error
                  << ")\n";
    }
}

// Demonstrate numerical differentiation
void demonstrateNumericalDifferentiation() {
    std::cout << "\n=== Numerical Differentiation ===\n";

    using NM = NumericalMethods<f64>;

    // Derivative of sin(x) at x = pi/4 (exact: cos(pi/4) = sqrt(2)/2)
    auto f = [](f64 x) { return std::sin(x); };
    f64 x = M_PI / 4.0;
    f64 exact = std::cos(x);

    std::cout << "Derivative of sin(x) at x = pi/4:\n";
    std::cout << "  Exact (cos(pi/4)): " << exact << "\n";

    for (f64 h : {0.1, 0.01, 0.001, 0.0001}) {
        f64 result = NM::centralDifference(f, x, h);
        f64 error = std::abs(result - exact);
        std::cout << "  h=" << std::setw(6) << h << ": " << std::fixed
                  << std::setprecision(10) << result << " (error: " << error
                  << ")\n";
    }

    // Derivative of e^x at x = 1 (exact: e)
    auto f2 = [](f64 x) { return std::exp(x); };

    std::cout << "\nDerivative of e^x at x = 1:\n";
    std::cout << "  Exact (e): " << M_E << "\n";

    for (f64 h : {0.1, 0.01, 0.001, 0.0001}) {
        f64 result = NM::centralDifference(f2, 1.0, h);
        f64 error = std::abs(result - M_E);
        std::cout << "  h=" << std::setw(6) << h << ": " << std::fixed
                  << std::setprecision(10) << result << " (error: " << error
                  << ")\n";
    }
}

// Compare root-finding methods
void compareRootFindingMethods() {
    std::cout << "\n=== Root-Finding Methods Comparison ===\n";

    using NM = NumericalMethods<f64>;

    // Find root of x^3 - 2x - 5 = 0 (one real root ~2.0946)
    auto f = [](f64 x) { return x * x * x - 2.0 * x - 5.0; };
    auto df = [](f64 x) { return 3.0 * x * x - 2.0; };

    std::cout << "Finding root of x^3 - 2x - 5 = 0:\n\n";

    auto nr_root = NM::newtonRaphson(f, df, 2.0);
    if (nr_root) {
        std::cout << "Newton-Raphson: " << std::fixed << std::setprecision(10)
                  << *nr_root << " (f(x) = " << f(*nr_root) << ")\n";
    }

    auto bisect_root = NM::bisection(f, 2.0, 3.0);
    if (bisect_root) {
        std::cout << "Bisection:      " << std::fixed << std::setprecision(10)
                  << *bisect_root << " (f(x) = " << f(*bisect_root) << ")\n";
    }

    auto secant_root = NM::secant(f, 2.0, 3.0);
    if (secant_root) {
        std::cout << "Secant:         " << std::fixed << std::setprecision(10)
                  << *secant_root << " (f(x) = " << f(*secant_root) << ")\n";
    }
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   Numerical Methods Example\n";
    std::cout << "========================================\n";

    try {
        demonstrateNewtonRaphson();
        demonstrateBisection();
        demonstrateSecantMethod();
        demonstrateTrapezoidalRule();
        demonstrateSimpsonRule();
        demonstrateNumericalDifferentiation();
        compareRootFindingMethods();

        std::cout << "\n========================================\n";
        std::cout << "   All examples completed successfully!\n";
        std::cout << "========================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
