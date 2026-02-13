#ifndef ATOM_ALGORITHM_MATH_LEVENBERG_MARQUARDT_HPP
#define ATOM_ALGORITHM_MATH_LEVENBERG_MARQUARDT_HPP

#include <cmath>
#include <concepts>
#include <functional>
#include <vector>

#include <spdlog/spdlog.h>
#include "atom/algorithm/core/rust_numeric.hpp"
#include "atom/algorithm/math/linear_solver.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/numeric/ublas/io.hpp>
#include <boost/numeric/ublas/lu.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#endif

namespace atom::algorithm::detail {

/**
 * @brief Nonlinear function type for curve fitting
 * @tparam T Floating-point type
 */
template <std::floating_point T>
using NonlinearFunction = std::function<T(T, const std::vector<T>&)>;

/**
 * @brief Solve a nonlinear least-squares problem using the
 *        Levenberg-Marquardt method.
 * @tparam T Floating-point type
 * @param x Vector of x values
 * @param y Vector of y values
 * @param func Nonlinear function to fit: f(x, params) -> y
 * @param initial_params Initial guess for the parameters
 * @param max_iterations Maximum number of iterations
 * @param lambda Regularization parameter (damping factor)
 * @param epsilon Convergence criterion
 * @return Vector of optimized parameters
 */
template <std::floating_point T>
auto levenbergMarquardt(const std::vector<T>& x, const std::vector<T>& y,
                        NonlinearFunction<T> func,
                        std::vector<T> initial_params,
                        i32 max_iterations = 100, T lambda = 0.01,
                        T epsilon = 1e-8) -> std::vector<T> {
    i32 n = static_cast<i32>(x.size());
    i32 m = static_cast<i32>(initial_params.size());
    std::vector<T> params = initial_params;
    std::vector<T> prevParams(m);
    std::vector<std::vector<T>> jacobian(n, std::vector<T>(m));

    for (i32 iteration = 0; iteration < max_iterations; ++iteration) {
        std::vector<T> residuals(n);
        for (i32 i = 0; i < n; ++i) {
            try {
                residuals[i] = y[i] - func(x[i], params);
            } catch (const std::exception& e) {
                spdlog::error("Exception in func: {}", e.what());
                throw;
            }
            for (i32 j = 0; j < m; ++j) {
                T h = std::max(T(1e-6), std::abs(params[j]) * T(1e-6));
                std::vector<T> paramsPlusH = params;
                paramsPlusH[j] += h;
                try {
                    jacobian[i][j] =
                        (func(x[i], paramsPlusH) - func(x[i], params)) / h;
                } catch (const std::exception& e) {
                    spdlog::error("Exception in jacobian computation: {}",
                                  e.what());
                    throw;
                }
            }
        }

        std::vector<std::vector<T>> JTJ(m, std::vector<T>(m, 0.0));
        std::vector<T> jTr(m, 0.0);
        for (i32 i = 0; i < m; ++i) {
            for (i32 j = 0; j < m; ++j) {
                for (i32 k = 0; k < n; ++k) {
                    JTJ[i][j] += jacobian[k][i] * jacobian[k][j];
                }
                if (i == j)
                    JTJ[i][j] += lambda;
            }
            for (i32 k = 0; k < n; ++k) {
                jTr[i] += jacobian[k][i] * residuals[k];
            }
        }

        std::vector<T> delta;
        try {
            delta = solveLinearSystem(JTJ, jTr);
        } catch (const std::exception& e) {
            spdlog::error("Exception in solving linear system: {}",
                          e.what());
            throw;
        }

        prevParams = params;
        for (i32 i = 0; i < m; ++i) {
            params[i] += delta[i];
        }

        T diff = 0;
        for (i32 i = 0; i < m; ++i) {
            diff += std::abs(params[i] - prevParams[i]);
        }
        if (diff < epsilon) {
            break;
        }
    }

    return params;
}

}  // namespace atom::algorithm::detail

#endif  // ATOM_ALGORITHM_MATH_LEVENBERG_MARQUARDT_HPP
