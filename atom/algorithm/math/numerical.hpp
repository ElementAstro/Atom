#ifndef ATOM_ALGORITHM_MATH_NUMERICAL_HPP
#define ATOM_ALGORITHM_MATH_NUMERICAL_HPP

#include <cmath>
#include <concepts>
#include <functional>
#include <limits>
#include <optional>
#include <vector>

#include "../core/rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief Numerical methods for solving equations and optimization
 *
 * This class provides common numerical algorithms including:
 * - Root finding (Newton-Raphson, bisection, secant method)
 * - Numerical integration (trapezoidal, Simpson's rule)
 * - Numerical differentiation
 * - Linear equation solving
 */
template <std::floating_point T>
class NumericalMethods {
public:
    using Function = std::function<T(T)>;
    using Function2D = std::function<T(T, T)>;

    /**
     * @brief Find root using Newton-Raphson method
     * @param f Function to find root of
     * @param df Derivative of the function
     * @param initial_guess Initial guess for the root
     * @param tolerance Convergence tolerance
     * @param max_iterations Maximum number of iterations
     * @return Root if found, nullopt otherwise
     */
    [[nodiscard]] static auto newtonRaphson(
        const Function& f, const Function& df, T initial_guess,
        T tolerance = T{1e-10},
        usize max_iterations = 100) -> std::optional<T> {
        T x = initial_guess;

        for (usize i = 0; i < max_iterations; ++i) {
            T fx = f(x);
            T dfx = df(x);

            if (std::abs(dfx) < std::numeric_limits<T>::epsilon()) {
                return std::nullopt;  // Derivative too small
            }

            T x_new = x - fx / dfx;

            if (std::abs(x_new - x) < tolerance) {
                return x_new;
            }

            x = x_new;
        }

        return std::nullopt;  // Did not converge
    }

    /**
     * @brief Find root using bisection method
     * @param f Function to find root of
     * @param a Left boundary (f(a) and f(b) must have opposite signs)
     * @param b Right boundary
     * @param tolerance Convergence tolerance
     * @param max_iterations Maximum number of iterations
     * @return Root if found, nullopt otherwise
     */
    [[nodiscard]] static auto bisection(
        const Function& f, T a, T b, T tolerance = T{1e-10},
        usize max_iterations = 100) -> std::optional<T> {
        T fa = f(a);
        T fb = f(b);

        // Check if root exists in interval
        if (fa * fb > T{0}) {
            return std::nullopt;
        }

        for (usize i = 0; i < max_iterations; ++i) {
            T c = (a + b) / T{2};
            T fc = f(c);

            if (std::abs(fc) < tolerance || (b - a) / T{2} < tolerance) {
                return c;
            }

            if (fa * fc < T{0}) {
                b = c;
                fb = fc;
            } else {
                a = c;
                fa = fc;
            }
        }

        return (a + b) / T{2};  // Return midpoint if max iterations reached
    }

    /**
     * @brief Find root using secant method
     * @param f Function to find root of
     * @param x0 First initial guess
     * @param x1 Second initial guess
     * @param tolerance Convergence tolerance
     * @param max_iterations Maximum number of iterations
     * @return Root if found, nullopt otherwise
     */
    [[nodiscard]] static auto secant(
        const Function& f, T x0, T x1, T tolerance = T{1e-10},
        usize max_iterations = 100) -> std::optional<T> {
        T f0 = f(x0);
        T f1 = f(x1);

        for (usize i = 0; i < max_iterations; ++i) {
            if (std::abs(f1 - f0) < std::numeric_limits<T>::epsilon()) {
                return std::nullopt;  // Division by zero
            }

            T x2 = x1 - f1 * (x1 - x0) / (f1 - f0);

            if (std::abs(x2 - x1) < tolerance) {
                return x2;
            }

            x0 = x1;
            f0 = f1;
            x1 = x2;
            f1 = f(x2);
        }

        return std::nullopt;  // Did not converge
    }

    /**
     * @brief Numerical integration using trapezoidal rule
     * @param f Function to integrate
     * @param a Lower bound
     * @param b Upper bound
     * @param n Number of intervals
     * @return Approximate integral value
     */
    [[nodiscard]] static auto trapezoidalRule(const Function& f, T a, T b,
                                              usize n) -> T {
        if (n == 0) {
            return T{0};
        }

        T h = (b - a) / static_cast<T>(n);
        T sum = (f(a) + f(b)) / T{2};

        for (usize i = 1; i < n; ++i) {
            T x = a + static_cast<T>(i) * h;
            sum += f(x);
        }

        return sum * h;
    }

    /**
     * @brief Numerical integration using Simpson's rule
     * @param f Function to integrate
     * @param a Lower bound
     * @param b Upper bound
     * @param n Number of intervals (must be even)
     * @return Approximate integral value
     */
    [[nodiscard]] static auto simpsonsRule(const Function& f, T a, T b,
                                           usize n) -> T {
        if (n == 0 || n % 2 != 0) {
            return T{0};  // n must be even
        }

        T h = (b - a) / static_cast<T>(n);
        T sum = f(a) + f(b);

        // Add odd-indexed terms (coefficient 4)
        for (usize i = 1; i < n; i += 2) {
            T x = a + static_cast<T>(i) * h;
            sum += T{4} * f(x);
        }

        // Add even-indexed terms (coefficient 2)
        for (usize i = 2; i < n; i += 2) {
            T x = a + static_cast<T>(i) * h;
            sum += T{2} * f(x);
        }

        return sum * h / T{3};
    }

    /**
     * @brief Numerical differentiation using central difference
     * @param f Function to differentiate
     * @param x Point at which to compute derivative
     * @param h Step size
     * @return Approximate derivative value
     */
    [[nodiscard]] static auto centralDifference(const Function& f, T x,
                                                T h = T{1e-8}) -> T {
        return (f(x + h) - f(x - h)) / (T{2} * h);
    }

    /**
     * @brief Numerical differentiation using forward difference
     * @param f Function to differentiate
     * @param x Point at which to compute derivative
     * @param h Step size
     * @return Approximate derivative value
     */
    [[nodiscard]] static auto forwardDifference(const Function& f, T x,
                                                T h = T{1e-8}) -> T {
        return (f(x + h) - f(x)) / h;
    }

    /**
     * @brief Numerical differentiation using backward difference
     * @param f Function to differentiate
     * @param x Point at which to compute derivative
     * @param h Step size
     * @return Approximate derivative value
     */
    [[nodiscard]] static auto backwardDifference(const Function& f, T x,
                                                 T h = T{1e-8}) -> T {
        return (f(x) - f(x - h)) / h;
    }

    /**
     * @brief Solve linear system Ax = b using Gaussian elimination
     * @param A Coefficient matrix (will be modified)
     * @param b Right-hand side vector (will be modified)
     * @return Solution vector if system is solvable, nullopt otherwise
     */
    [[nodiscard]] static auto gaussianElimination(
        std::vector<std::vector<T>>& A,
        std::vector<T>& b) -> std::optional<std::vector<T>> {
        usize n = A.size();
        if (n == 0 || A[0].size() != n || b.size() != n) {
            return std::nullopt;
        }

        // Forward elimination
        for (usize i = 0; i < n; ++i) {
            // Find pivot
            usize max_row = i;
            for (usize k = i + 1; k < n; ++k) {
                if (std::abs(A[k][i]) > std::abs(A[max_row][i])) {
                    max_row = k;
                }
            }

            // Swap rows
            if (max_row != i) {
                std::swap(A[i], A[max_row]);
                std::swap(b[i], b[max_row]);
            }

            // Check for singular matrix
            if (std::abs(A[i][i]) < std::numeric_limits<T>::epsilon()) {
                return std::nullopt;
            }

            // Eliminate column
            for (usize k = i + 1; k < n; ++k) {
                T factor = A[k][i] / A[i][i];
                for (usize j = i; j < n; ++j) {
                    A[k][j] -= factor * A[i][j];
                }
                b[k] -= factor * b[i];
            }
        }

        // Back substitution
        std::vector<T> x(n);
        for (i64 ii = static_cast<i64>(n) - 1; ii >= 0; --ii) {
            usize i = static_cast<usize>(ii);
            x[i] = b[i];
            for (usize j = i + 1; j < n; ++j) {
                x[i] -= A[i][j] * x[j];
            }
            x[i] /= A[i][i];
        }

        return x;
    }

    /**
     * @brief Find minimum using golden section search
     * @param f Function to minimize
     * @param a Left boundary
     * @param b Right boundary
     * @param tolerance Convergence tolerance
     * @return Minimum point if found
     */
    [[nodiscard]] static auto goldenSectionSearch(const Function& f, T a, T b,
                                                  T tolerance = T{1e-10}) -> T {
        constexpr T phi = T{1.618033988749895};  // Golden ratio
        constexpr T resphi = T{2} - phi;

        T x1 = a + resphi * (b - a);
        T x2 = b - resphi * (b - a);
        T f1 = f(x1);
        T f2 = f(x2);

        while (std::abs(b - a) > tolerance) {
            if (f1 < f2) {
                b = x2;
                x2 = x1;
                f2 = f1;
                x1 = a + resphi * (b - a);
                f1 = f(x1);
            } else {
                a = x1;
                x1 = x2;
                f1 = f2;
                x2 = b - resphi * (b - a);
                f2 = f(x2);
            }
        }

        return (a + b) / T{2};
    }
};

// Type aliases for common use cases
using NumericalMethodsF = NumericalMethods<f32>;
using NumericalMethodsD = NumericalMethods<f64>;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_MATH_NUMERICAL_HPP
