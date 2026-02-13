#ifndef ATOM_ALGORITHM_MATH_LINEAR_SOLVER_HPP
#define ATOM_ALGORITHM_MATH_LINEAR_SOLVER_HPP

#include <algorithm>
#include <cmath>
#include <concepts>
#include <vector>

#include "atom/algorithm/core/rust_numeric.hpp"
#include "atom/error/exception.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/numeric/ublas/io.hpp>
#include <boost/numeric/ublas/lu.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#endif

namespace atom::algorithm::detail {

/**
 * @brief Solve a system of linear equations using Gaussian elimination
 *        with partial pivoting.
 * @tparam T Floating-point type
 * @param A Coefficient matrix (n x n)
 * @param b Right-hand side vector (n)
 * @return Solution vector x such that A*x = b
 * @throws RuntimeError if the matrix is singular or nearly singular
 */
#ifdef ATOM_USE_BOOST
template <std::floating_point T>
auto solveLinearSystem(const std::vector<std::vector<T>>& A,
                       const std::vector<T>& b) -> std::vector<T> {
    i32 n = static_cast<i32>(A.size());
    boost::numeric::ublas::matrix<T> ublas_A(n, n);
    boost::numeric::ublas::vector<T> ublas_b(n);
    for (i32 i = 0; i < n; ++i) {
        for (i32 j = 0; j < n; ++j) {
            ublas_A(i, j) = A[i][j];
        }
        ublas_b(i) = b[i];
    }

    boost::numeric::ublas::permutation_matrix<usize> pm(ublas_A.size1());
    bool singular = boost::numeric::ublas::lu_factorize(ublas_A, pm);
    if (singular) {
        THROW_RUNTIME_ERROR("Matrix is singular.");
    }
    boost::numeric::ublas::lu_substitute(ublas_A, pm, ublas_b);

    std::vector<T> result(n);
    for (i32 i = 0; i < n; ++i) {
        result[i] = ublas_b(i);
    }
    return result;
}
#else
template <std::floating_point T>
auto solveLinearSystem(const std::vector<std::vector<T>>& A,
                       const std::vector<T>& b) -> std::vector<T> {
    i32 n = static_cast<i32>(A.size());
    std::vector<std::vector<T>> augmented(n, std::vector<T>(n + 1, 0.0));
    for (i32 i = 0; i < n; ++i) {
        for (i32 j = 0; j < n; ++j) {
            augmented[i][j] = A[i][j];
        }
        augmented[i][n] = b[i];
    }

    for (i32 i = 0; i < n; ++i) {
        // Partial pivoting
        i32 maxRow = i;
        for (i32 k = i + 1; k < n; ++k) {
            if (std::abs(augmented[k][i]) >
                std::abs(augmented[maxRow][i])) {
                maxRow = k;
            }
        }
        if (std::abs(augmented[maxRow][i]) < 1e-12) {
            THROW_RUNTIME_ERROR("Matrix is singular or nearly singular.");
        }
        std::swap(augmented[i], augmented[maxRow]);

        // Eliminate below
        for (i32 k = i + 1; k < n; ++k) {
            T factor = augmented[k][i] / augmented[i][i];
            for (i32 j = i; j <= n; ++j) {
                augmented[k][j] -= factor * augmented[i][j];
            }
        }
    }

    std::vector<T> x(n, 0.0);
    for (i32 i = n - 1; i >= 0; --i) {
        if (std::abs(augmented[i][i]) < 1e-12) {
            THROW_RUNTIME_ERROR(
                "Division by zero during back substitution.");
        }
        x[i] = augmented[i][n];
        for (i32 j = i + 1; j < n; ++j) {
            x[i] -= augmented[i][j] * x[j];
        }
        x[i] /= augmented[i][i];
    }

    return x;
}
#endif

}  // namespace atom::algorithm::detail

#endif  // ATOM_ALGORITHM_MATH_LINEAR_SOLVER_HPP
