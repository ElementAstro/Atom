#ifndef ATOM_EXTRA_BOOST_MATH_HPP
#define ATOM_EXTRA_BOOST_MATH_HPP

#include <boost/math/distributions.hpp>
#include <boost/math/quadrature/trapezoidal.hpp>
#include <boost/math/special_functions.hpp>
#include <boost/math/statistics/univariate_statistics.hpp>

#include <boost/numeric/odeint.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/vector.hpp>

#include <cmath>
#include <functional>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace atom::extra::boost {

/**
 * @brief Concept to check if a type is numeric
 * @tparam T The type to check
 */
template <typename T>
concept Numeric = std::is_arithmetic_v<T>;

/**
 * @brief Wrapper class for special mathematical functions
 * @tparam T The numeric type
 */
template <Numeric T>
class SpecialFunctions {
public:
    /**
     * @brief Computes the beta function
     * @param alpha The alpha parameter
     * @param beta The beta parameter
     * @return The value of the beta function
     */
    [[nodiscard]] static constexpr T beta(T alpha, T beta) noexcept {
        return ::boost::math::beta(alpha, beta);
    }

    /**
     * @brief Computes the gamma function
     * @param value The input value
     * @return The value of the gamma function
     */
    [[nodiscard]] static constexpr T gamma(T value) noexcept {
        return ::boost::math::tgamma(value);
    }

    /**
     * @brief Computes the digamma function
     * @param value The input value
     * @return The value of the digamma function
     */
    [[nodiscard]] static constexpr T digamma(T value) noexcept {
        return ::boost::math::digamma(value);
    }

    /**
     * @brief Computes the error function
     * @param value The input value
     * @return The value of the error function
     */
    [[nodiscard]] static constexpr T erf(T value) noexcept {
        return ::boost::math::erf(value);
    }

    /**
     * @brief Computes the Bessel function of the first kind
     * @param order The order of the Bessel function
     * @param value The input value
     * @return The value of the Bessel function
     */
    [[nodiscard]] static constexpr T besselJ(int order, T value) noexcept {
        return ::boost::math::cyl_bessel_j(order, value);
    }

    /**
     * @brief Computes the Legendre polynomial
     * @param order The order of the polynomial
     * @param value The input value
     * @return The value of the Legendre polynomial
     */
    [[nodiscard]] static constexpr T legendreP(int order, T value) noexcept {
        return ::boost::math::legendre_p(order, value);
    }
};

/**
 * @brief Wrapper class for statistical functions
 * @tparam T The numeric type
 */
template <Numeric T>
class Statistics {
public:
    /**
     * @brief Computes the mean of a dataset
     * @param data The input dataset
     * @return The mean of the dataset
     */
    [[nodiscard]] static T mean(const std::vector<T>& data) {
        return ::boost::math::statistics::mean(data);
    }

    /**
     * @brief Computes the variance of a dataset
     * @param data The input dataset
     * @return The variance of the dataset
     */
    [[nodiscard]] static T variance(const std::vector<T>& data) {
        return ::boost::math::statistics::variance(data);
    }

    /**
     * @brief Computes the skewness of a dataset
     * @param data The input dataset
     * @return The skewness of the dataset
     */
    [[nodiscard]] static T skewness(const std::vector<T>& data) {
        return ::boost::math::statistics::skewness(data);
    }

    /**
     * @brief Computes the kurtosis of a dataset
     * @param data The input dataset
     * @return The kurtosis of the dataset
     */
    [[nodiscard]] static T kurtosis(const std::vector<T>& data) {
        return ::boost::math::statistics::kurtosis(data);
    }
};

/**
 * @brief Wrapper class for probability distributions
 * @tparam T The numeric type
 */
template <Numeric T>
class Distributions {
public:
    /**
     * @brief Wrapper class for normal distribution
     */
    class NormalDistribution {
    private:
        ::boost::math::normal_distribution<T> distribution_;

    public:
        /**
         * @brief Constructs a normal distribution with given mean and standard
         * deviation
         * @param mean The mean of the distribution
         * @param stddev The standard deviation of the distribution
         */
        constexpr NormalDistribution(T mean, T stddev) noexcept
            : distribution_(mean, stddev) {}

        /**
         * @brief Computes the probability density function (PDF) at a given
         * value
         * @param value The input value
         * @return The PDF value
         */
        [[nodiscard]] constexpr T pdf(T value) const noexcept {
            return ::boost::math::pdf(distribution_, value);
        }

        /**
         * @brief Computes the cumulative distribution function (CDF) at a given
         * value
         * @param value The input value
         * @return The CDF value
         */
        [[nodiscard]] constexpr T cdf(T value) const noexcept {
            return ::boost::math::cdf(distribution_, value);
        }

        /**
         * @brief Computes the quantile (inverse CDF) at a given probability
         * @param probability The input probability
         * @return The quantile value
         */
        [[nodiscard]] constexpr T quantile(T probability) const noexcept {
            return ::boost::math::quantile(distribution_, probability);
        }
    };

    /**
     * @brief Wrapper class for Student's t-distribution
     */
    class StudentTDistribution {
    private:
        ::boost::math::students_t_distribution<T> distribution_;

    public:
        /**
         * @brief Constructs a Student's t-distribution with given degrees of
         * freedom
         * @param degreesOfFreedom The degrees of freedom
         */
        explicit constexpr StudentTDistribution(T degreesOfFreedom) noexcept
            : distribution_(degreesOfFreedom) {}

        /**
         * @brief Computes the probability density function (PDF) at a given
         * value
         * @param value The input value
         * @return The PDF value
         */
        [[nodiscard]] constexpr T pdf(T value) const noexcept {
            return ::boost::math::pdf(distribution_, value);
        }

        /**
         * @brief Computes the cumulative distribution function (CDF) at a given
         * value
         * @param value The input value
         * @return The CDF value
         */
        [[nodiscard]] constexpr T cdf(T value) const noexcept {
            return ::boost::math::cdf(distribution_, value);
        }

        /**
         * @brief Computes the quantile (inverse CDF) at a given probability
         * @param probability The input probability
         * @return The quantile value
         */
        [[nodiscard]] constexpr T quantile(T probability) const noexcept {
            return ::boost::math::quantile(distribution_, probability);
        }
    };

    /**
     * @brief Wrapper class for Poisson distribution
     */
    class PoissonDistribution {
    private:
        ::boost::math::poisson_distribution<T> distribution_;

    public:
        /**
         * @brief Constructs a Poisson distribution with given mean
         * @param mean The mean of the distribution
         */
        explicit constexpr PoissonDistribution(T mean) noexcept
            : distribution_(mean) {}

        /**
         * @brief Computes the probability density function (PDF) at a given
         * value
         * @param value The input value
         * @return The PDF value
         */
        [[nodiscard]] constexpr T pdf(T value) const noexcept {
            return ::boost::math::pdf(distribution_, value);
        }

        /**
         * @brief Computes the cumulative distribution function (CDF) at a given
         * value
         * @param value The input value
         * @return The CDF value
         */
        [[nodiscard]] constexpr T cdf(T value) const noexcept {
            return ::boost::math::cdf(distribution_, value);
        }
    };

    /**
     * @brief Wrapper class for exponential distribution
     */
    class ExponentialDistribution {
    private:
        ::boost::math::exponential_distribution<T> distribution_;

    public:
        /**
         * @brief Constructs an exponential distribution with given rate
         * parameter
         * @param lambda The rate parameter
         */
        explicit constexpr ExponentialDistribution(T lambda) noexcept
            : distribution_(lambda) {}

        /**
         * @brief Computes the probability density function (PDF) at a given
         * value
         * @param value The input value
         * @return The PDF value
         */
        [[nodiscard]] constexpr T pdf(T value) const noexcept {
            return ::boost::math::pdf(distribution_, value);
        }

        /**
         * @brief Computes the cumulative distribution function (CDF) at a given
         * value
         * @param value The input value
         * @return The CDF value
         */
        [[nodiscard]] constexpr T cdf(T value) const noexcept {
            return ::boost::math::cdf(distribution_, value);
        }
    };
};

/**
 * @brief Wrapper class for numerical integration methods
 * @tparam T The numeric type
 */
template <Numeric T>
class NumericalIntegration {
public:
    /**
     * @brief Computes the integral of a function using the trapezoidal rule
     * @param func The function to integrate
     * @param start The start of the integration interval
     * @param end The end of the integration interval
     * @return The computed integral
     */
    [[nodiscard]] static T trapezoidal(const std::function<T(T)>& func, T start,
                                       T end) {
        return ::boost::math::quadrature::trapezoidal(func, start, end);
    }
};

/**
 * @brief Computes the factorial of a number using constexpr optimization
 * @tparam T The numeric type
 * @param number The input number
 * @return The factorial of the number
 */
template <Numeric T>
[[nodiscard]] constexpr T factorial(T number) noexcept {
    if constexpr (std::is_integral_v<T>) {
        if ((number <= 1)) [[likely]] {
            return 1;
        }
        return number * factorial(number - 1);
    } else {
        return std::tgamma(number + 1);
    }
}

/**
 * @brief Transforms a range of data using a given function
 * @tparam Range The type of the input range
 * @tparam Func The type of the transformation function
 * @param range The input range
 * @param func The transformation function
 * @return A transformed view of the input range
 */
template <std::ranges::input_range Range, typename Func>
[[nodiscard]] auto transformRange(Range&& range, Func func) {
    return std::ranges::transform_view(std::forward<Range>(range), func);
}

/**
 * @brief Wrapper class for optimization methods
 * @tparam T The numeric type
 */
template <Numeric T>
class Optimization {
public:
    /**
     * @brief Performs one-dimensional golden section search to find the minimum
     * of a function
     * @param func The function to minimize
     * @param start The start of the search interval
     * @param end The end of the search interval
     * @param tolerance The tolerance for convergence
     * @return The point where the function attains its minimum
     */
    [[nodiscard]] static T goldenSectionSearch(const std::function<T(T)>& func,
                                               T start, T end, T tolerance) {
        static constexpr T goldenRatio = static_cast<T>(0.618033988749895);
        T pointC = end - goldenRatio * (end - start);
        T pointD = start + goldenRatio * (end - start);

        while (std::abs(pointC - pointD) > tolerance) {
            if ((func(pointC) < func(pointD))) [[likely]] {
                end = pointD;
            } else {
                start = pointC;
            }
            pointC = end - goldenRatio * (end - start);
            pointD = start + goldenRatio * (end - start);
        }

        return (start + end) / 2;
    }

    /**
     * @brief Performs Newton-Raphson method to find the root of a function
     * @param func The function whose root is to be found
     * @param derivativeFunc The derivative of the function
     * @param initialGuess The initial guess for the root
     * @param tolerance The tolerance for convergence
     * @param maxIterations The maximum number of iterations
     * @return The root of the function
     * @throws std::runtime_error If the derivative is zero or maximum
     * iterations are reached
     */
    [[nodiscard]] static T newtonRaphson(
        const std::function<T(T)>& func,
        const std::function<T(T)>& derivativeFunc, T initialGuess, T tolerance,
        int maxIterations) {
        T currentGuess = initialGuess;
        for (int i = 0; i < maxIterations; ++i) {
            T funcValue = func(currentGuess);
            if ((std::abs(funcValue) < tolerance)) [[unlikely]] {
                return currentGuess;
            }
            T derivativeValue = derivativeFunc(currentGuess);
            if ((derivativeValue == 0)) [[unlikely]] {
                throw std::runtime_error(
                    "Derivative is zero. Cannot continue.");
            }
            currentGuess = currentGuess - funcValue / derivativeValue;
        }
        throw std::runtime_error("Max iterations reached without convergence.");
    }
};

/**
 * @brief Wrapper class for linear algebra operations
 * @tparam T The numeric type
 */
template <Numeric T>
class LinearAlgebra {
public:
    using Matrix = ::boost::numeric::ublas::matrix<T>;
    using Vector = ::boost::numeric::ublas::vector<T>;

    /**
     * @brief Solves a linear system of equations Ax = b
     * @param matrix The matrix A
     * @param vector The vector b
     * @return The solution vector x
     */
    [[nodiscard]] static Vector solveLinearSystem(const Matrix& matrix,
                                                  const Vector& vector) {
        ::boost::numeric::ublas::permutation_matrix<std::size_t>
            permutationMatrix(matrix.size1());
        Matrix matrixCopy = matrix;
        ::boost::numeric::ublas::lu_factorize(matrixCopy, permutationMatrix);
        Vector solution = vector;
        ::boost::numeric::ublas::lu_substitute(matrixCopy, permutationMatrix,
                                               solution);
        return solution;
    }

    /**
     * @brief Computes the determinant of a matrix
     * @param matrix The input matrix
     * @return The determinant of the matrix
     */
    [[nodiscard]] static T determinant(const Matrix& matrix) {
        Matrix matrixCopy = matrix;
        ::boost::numeric::ublas::permutation_matrix<std::size_t>
            permutationMatrix(matrix.size1());
        ::boost::numeric::ublas::lu_factorize(matrixCopy, permutationMatrix);
        T determinantValue = T{1};
        for (std::size_t i = 0; i < matrix.size1(); ++i) {
            determinantValue *= matrixCopy(i, i);
        }
        return determinantValue *
               (permutationMatrix.size() % 2 == 1 ? T{-1} : T{1});
    }

    /**
     * @brief Multiplies two matrices
     * @param matrix1 The first matrix
     * @param matrix2 The second matrix
     * @return The product of the two matrices
     */
    [[nodiscard]] static Matrix multiply(const Matrix& matrix1,
                                         const Matrix& matrix2) {
        return ::boost::numeric::ublas::prod(matrix1, matrix2);
    }

    /**
     * @brief Computes the transpose of a matrix
     * @param matrix The input matrix
     * @return The transpose of the matrix
     */
    [[nodiscard]] static Matrix transpose(const Matrix& matrix) {
        return ::boost::numeric::ublas::trans(matrix);
    }
};

/**
 * @brief Wrapper class for solving ordinary differential equations (ODEs)
 * @tparam T The numeric type
 */
template <Numeric T>
class ODESolver {
public:
    using State = std::vector<T>;
    using SystemFunction = std::function<void(const State&, State&, T)>;

    /**
     * @brief Solves an ODE using the 4th order Runge-Kutta method
     * @param system The system function defining the ODE
     * @param initialState The initial state of the system
     * @param startTime The start time
     * @param endTime The end time
     * @param stepSize The step size
     * @return A vector of states representing the solution
     */
    [[nodiscard]] static std::vector<State> rungeKutta4(
        const SystemFunction& system, State initialState, T startTime,
        T endTime, T stepSize) {
        std::vector<State> solution;
        solution.reserve(static_cast<size_t>((endTime - startTime) / stepSize) +
                         1);
        ::boost::numeric::odeint::runge_kutta4<State> stepper;
        ::boost::numeric::odeint::integrate_const(
            stepper, system, initialState, startTime, endTime, stepSize,
            [&solution](const State& state, T) { solution.push_back(state); });
        return solution;
    }
};

/**
 * @brief Wrapper class for financial mathematics functions
 * @tparam T The numeric type
 */
template <Numeric T>
class FinancialMath {
public:
    /**
     * @brief Computes the price of a European call option using the
     * Black-Scholes formula
     * @param stockPrice The current stock price
     * @param strikePrice The strike price of the option
     * @param riskFreeRate The risk-free interest rate
     * @param volatility The volatility of the stock
     * @param timeToMaturity The time to maturity of the option
     * @return The price of the European call option
     */
    [[nodiscard]] static T blackScholesCall(T stockPrice, T strikePrice,
                                            T riskFreeRate, T volatility,
                                            T timeToMaturity) noexcept {
        T sqrtTime = std::sqrt(timeToMaturity);
        T d1 = (std::log(stockPrice / strikePrice) +
                (riskFreeRate + T{0.5} * volatility * volatility) *
                    timeToMaturity) /
               (volatility * sqrtTime);
        T d2 = d1 - volatility * sqrtTime;

        static const ::boost::math::normal_distribution<T> normal;
        return stockPrice * ::boost::math::cdf(normal, d1) -
               strikePrice * std::exp(-riskFreeRate * timeToMaturity) *
                   ::boost::math::cdf(normal, d2);
    }

    /**
     * @brief Computes the modified duration of a bond
     * @param yield The yield to maturity
     * @param couponRate The coupon rate of the bond
     * @param faceValue The face value of the bond
     * @param periods The number of periods
     * @return The modified duration of the bond
     */
    [[nodiscard]] static T modifiedDuration(T yield, T couponRate, T faceValue,
                                            int periods) noexcept {
        T periodYield = yield / periods;
        T couponPayment = couponRate * faceValue / periods;
        T presentValue = T{0};
        T weightedPresentValue = T{0};

        for (int i = 1; i <= periods; ++i) {
            T discountFactor = std::pow(T{1} + periodYield, -i);
            presentValue += couponPayment * discountFactor;
            weightedPresentValue += i * couponPayment * discountFactor;
        }

        T finalDiscountFactor = std::pow(T{1} + periodYield, -periods);
        presentValue += faceValue * finalDiscountFactor;
        weightedPresentValue += periods * faceValue * finalDiscountFactor;

        return (weightedPresentValue / presentValue) / (T{1} + periodYield);
    }

    /**
     * @brief Computes the bond price
     * @param yield The yield to maturity
     * @param couponRate The coupon rate of the bond
     * @param faceValue The face value of the bond
     * @param periods The number of periods
     * @return The bond price
     */
    [[nodiscard]] static T bondPrice(T yield, T couponRate, T faceValue,
                                     int periods) noexcept {
        T periodYield = yield / periods;
        T couponPayment = couponRate * faceValue / periods;
        T presentValue = T{0};

        for (int i = 1; i <= periods; ++i) {
            presentValue += couponPayment * std::pow(T{1} + periodYield, -i);
        }
        presentValue += faceValue * std::pow(T{1} + periodYield, -periods);
        return presentValue;
    }

    /**
     * @brief Computes the implied volatility of an option
     * @param marketPrice The market price of the option
     * @param stockPrice The current stock price
     * @param strikePrice The strike price of the option
     * @param riskFreeRate The risk-free interest rate
     * @param timeToMaturity The time to maturity of the option
     * @return The implied volatility
     */
    [[nodiscard]] static T impliedVolatility(T marketPrice, T stockPrice,
                                             T strikePrice, T riskFreeRate,
                                             T timeToMaturity) {
        auto objectiveFunction = [=](T volatility) {
            return blackScholesCall(stockPrice, strikePrice, riskFreeRate,
                                    volatility, timeToMaturity) -
                   marketPrice;
        };
        auto derivative = [=](T volatility) {
            T sqrtTime = std::sqrt(timeToMaturity);
            T d1 = (std::log(stockPrice / strikePrice) +
                    (riskFreeRate + T{0.5} * volatility * volatility) *
                        timeToMaturity) /
                   (volatility * sqrtTime);
            static const ::boost::math::normal_distribution<T> normal;
            return stockPrice * ::boost::math::pdf(normal, d1) * sqrtTime;
        };
        return Optimization<T>::newtonRaphson(objectiveFunction, derivative,
                                              T{0.2}, T{1e-6}, 100);
    }
};

}  // namespace atom::extra::boost

#endif
