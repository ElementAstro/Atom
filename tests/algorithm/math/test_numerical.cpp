#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <cmath>
#include <functional>
#include <vector>
#include "atom/algorithm/math/numerical.hpp"

using namespace atom::algorithm;

// Define PI constant if M_PI is not available
#ifndef M_PI
constexpr double M_PI = 3.14159265358979323846;
#endif

class NumericalMethodsTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }

    // Test functions
    auto linearFunction(double x) -> double { return 2.0 * x - 3.0; }
    auto linearDerivative(double) -> double { return 2.0; }

    auto quadraticFunction(double x) -> double { return x * x - 4.0; }
    auto quadraticDerivative(double x) -> double { return 2.0 * x; }

    auto cubicFunction(double x) -> double { return x * x * x - x - 2.0; }
    auto cubicDerivative(double x) -> double { return 3.0 * x * x - 1.0; }

    auto sineFunction(double x) -> double { return std::sin(x); }
    auto cosineFunction(double x) -> double { return std::cos(x); }

    auto exponentialFunction(double x) -> double { return std::exp(x); }
    auto exponentialDerivative(double x) -> double { return std::exp(x); }

    auto logFunction(double x) -> double { return std::log(x); }
    auto logDerivative(double x) -> double { return 1.0 / x; }

    auto sqrtFunction(double x) -> double { return std::sqrt(x); }
    auto sqrtDerivative(double x) -> double {
        return 1.0 / (2.0 * std::sqrt(x));
    }
};

TEST_F(NumericalMethodsTest, NewtonRaphson) {
    // Test with linear function (should converge in one iteration)
    auto result = NumericalMethodsD::newtonRaphson(
        [this](double x) { return linearFunction(x); },
        [this](double x) { return linearDerivative(x); }, 0.0);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 1.5, 1e-10);  // 2x - 3 = 0 => x = 1.5

    // Test with quadratic function (x^2 - 4 = 0 => x = ±2)
    result = NumericalMethodsD::newtonRaphson(
        [this](double x) { return quadraticFunction(x); },
        [this](double x) { return quadraticDerivative(x); },
        3.0  // Initial guess near +2
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 2.0, 1e-10);

    result = NumericalMethodsD::newtonRaphson(
        [this](double x) { return quadraticFunction(x); },
        [this](double x) { return quadraticDerivative(x); },
        -3.0  // Initial guess near -2
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), -2.0, 1e-10);

    // Test with cubic function
    result = NumericalMethodsD::newtonRaphson(
        [this](double x) { return cubicFunction(x); },
        [this](double x) { return cubicDerivative(x); }, 2.0);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 1.521, 1e-3);  // Approximate root

    // Test with exponential function
    result = NumericalMethodsD::newtonRaphson(
        [this](double x) { return exponentialFunction(x) - 1.0; },
        [this](double x) { return exponentialDerivative(x); }, 1.0);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 0.0, 1e-10);  // exp(x) - 1 = 0 => x = 0

    // Test with bad initial guess (should not converge)
    result = NumericalMethodsD::newtonRaphson(
        [this](double x) { return quadraticFunction(x); },
        [this](double x) { return quadraticDerivative(x); },
        0.0  // Bad guess for quadratic, derivative is zero at root
    );
    EXPECT_FALSE(result.has_value());
}

TEST_F(NumericalMethodsTest, Bisection) {
    // Test with linear function
    auto result = NumericalMethodsD::bisection(
        [this](double x) { return linearFunction(x); },
        0.0,  // f(0) = -3
        3.0   // f(3) = 3
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 1.5, 1e-10);

    // Test with quadratic function
    result = NumericalMethodsD::bisection(
        [this](double x) { return quadraticFunction(x); },
        1.0,  // f(1) = -3
        3.0   // f(3) = 5
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 2.0, 1e-10);

    // Test with sine function
    result = NumericalMethodsD::bisection(
        [this](double x) { return sineFunction(x); },
        3.0,  // sin(3) ≈ 0.141
        4.0   // sin(4) ≈ -0.757
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), M_PI, 1e-3);

    // Test with invalid interval (same sign at boundaries)
    result = NumericalMethodsD::bisection(
        [this](double x) { return quadraticFunction(x); },
        3.0,  // f(3) = 5
        5.0   // f(5) = 21
    );
    EXPECT_FALSE(result.has_value());
}

TEST_F(NumericalMethodsTest, Secant) {
    // Test with linear function
    auto result = NumericalMethodsD::secant(
        [this](double x) { return linearFunction(x); }, 0.0, 3.0);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 1.5, 1e-10);

    // Test with quadratic function
    result = NumericalMethodsD::secant(
        [this](double x) { return quadraticFunction(x); }, 1.0, 3.0);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 2.0, 1e-10);

    // Test with cubic function
    result = NumericalMethodsD::secant(
        [this](double x) { return cubicFunction(x); }, 1.0, 2.0);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 1.521, 1e-3);

    // Test with bad initial guesses (same values)
    result = NumericalMethodsD::secant(
        [this](double x) { return quadraticFunction(x); }, 1.0, 1.0);
    EXPECT_FALSE(result.has_value());
}

TEST_F(NumericalMethodsTest, TrapezoidalRule) {
    // Test with linear function (should be exact)
    double result = NumericalMethodsD::trapezoidalRule(
        [this](double x) { return linearFunction(x); }, 0.0, 2.0, 100);
    // Integral of 2x - 3 from 0 to 2 = [x^2 - 3x]_0^2 = 4 - 6 = -2
    EXPECT_NEAR(result, -2.0, 1e-10);

    // Test with quadratic function
    result = NumericalMethodsD::trapezoidalRule(
        [this](double x) { return quadraticFunction(x); }, 0.0, 2.0, 1000);
    // Integral of x^2 - 4 from 0 to 2 = [x^3/3 - 4x]_0^2 = 8/3 - 8 = -16/3
    double expected = -16.0 / 3.0;
    EXPECT_NEAR(result, expected, 1e-3);

    // Test with sine function
    result = NumericalMethodsD::trapezoidalRule(
        [this](double x) { return sineFunction(x); }, 0.0, M_PI, 1000);
    // Integral of sin(x) from 0 to π = 2
    EXPECT_NEAR(result, 2.0, 1e-3);

    // Test with zero intervals
    result = NumericalMethodsD::trapezoidalRule(
        [this](double x) { return linearFunction(x); }, 0.0, 2.0, 0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST_F(NumericalMethodsTest, SimpsonsRule) {
    // Test with linear function (should be exact)
    double result = NumericalMethodsD::simpsonsRule(
        [this](double x) { return linearFunction(x); }, 0.0, 2.0,
        100  // Must be even
    );
    EXPECT_NEAR(result, -2.0, 1e-10);

    // Test with quadratic function (should be exact for polynomials up to
    // degree 3)
    result = NumericalMethodsD::simpsonsRule(
        [this](double x) { return quadraticFunction(x); }, 0.0, 2.0, 100);
    double expected = -16.0 / 3.0;
    EXPECT_NEAR(result, expected, 1e-12);

    // Test with cubic function (should be exact for polynomials up to degree 3)
    result = NumericalMethodsD::simpsonsRule(
        [this](double x) { return cubicFunction(x); }, 0.0, 1.0, 100);
    // Integral of x^3 - x - 2 from 0 to 1 = [x^4/4 - x^2/2 - 2x]_0^1 = 1/4 -
    // 1/2 - 2 = -9/4
    expected = -9.0 / 4.0;
    EXPECT_NEAR(result, expected, 1e-12);

    // Test with sine function
    result = NumericalMethodsD::simpsonsRule(
        [this](double x) { return sineFunction(x); }, 0.0, M_PI, 100);
    EXPECT_NEAR(result, 2.0, 1e-6);

    // Test with odd number of intervals (should return 0)
    result = NumericalMethodsD::simpsonsRule(
        [this](double x) { return linearFunction(x); }, 0.0, 2.0, 101  // Odd
    );
    EXPECT_DOUBLE_EQ(result, 0.0);

    // Test with zero intervals
    result = NumericalMethodsD::simpsonsRule(
        [this](double x) { return linearFunction(x); }, 0.0, 2.0, 0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST_F(NumericalMethodsTest, CentralDifference) {
    // Test with linear function (should be exact)
    double result = NumericalMethodsD::centralDifference(
        [this](double x) { return linearFunction(x); }, 1.0, 1e-6);
    EXPECT_NEAR(result, 2.0, 1e-6);

    // Test with quadratic function
    result = NumericalMethodsD::centralDifference(
        [this](double x) { return quadraticFunction(x); }, 1.0, 1e-6);
    EXPECT_NEAR(result, 2.0, 1e-3);  // derivative of x^2 - 4 is 2x, at x=1 is 2

    // Test with sine function
    result = NumericalMethodsD::centralDifference(
        [this](double x) { return sineFunction(x); }, 0.0, 1e-6);
    EXPECT_NEAR(result, 1.0, 1e-6);  // cos(0) = 1

    // Test with exponential function
    result = NumericalMethodsD::centralDifference(
        [this](double x) { return exponentialFunction(x); }, 0.0, 1e-6);
    EXPECT_NEAR(result, 1.0, 1e-6);  // exp(0) = 1

    // Test with different step sizes
    double result_small = NumericalMethodsD::centralDifference(
        [this](double x) { return sineFunction(x); }, 1.0, 1e-8);
    double result_large = NumericalMethodsD::centralDifference(
        [this](double x) { return sineFunction(x); }, 1.0, 1e-4);
    // Both should be close to cos(1) ≈ 0.5403
    EXPECT_NEAR(result_small, std::cos(1.0), 1e-7);
    EXPECT_NEAR(result_large, std::cos(1.0), 1e-3);
}

TEST_F(NumericalMethodsTest, ForwardDifference) {
    // Test with linear function (should be exact)
    double result = NumericalMethodsD::forwardDifference(
        [this](double x) { return linearFunction(x); }, 1.0, 1e-6);
    EXPECT_NEAR(result, 2.0, 1e-6);

    // Test with quadratic function
    result = NumericalMethodsD::forwardDifference(
        [this](double x) { return quadraticFunction(x); }, 1.0, 1e-6);
    EXPECT_NEAR(result, 2.0, 1e-3);

    // Test with sine function
    result = NumericalMethodsD::forwardDifference(
        [this](double x) { return sineFunction(x); }, 0.0, 1e-6);
    EXPECT_NEAR(result, 1.0, 1e-6);
}

TEST_F(NumericalMethodsTest, BackwardDifference) {
    // Test with linear function (should be exact)
    double result = NumericalMethodsD::backwardDifference(
        [this](double x) { return linearFunction(x); }, 1.0, 1e-6);
    EXPECT_NEAR(result, 2.0, 1e-6);

    // Test with quadratic function
    result = NumericalMethodsD::backwardDifference(
        [this](double x) { return quadraticFunction(x); }, 1.0, 1e-6);
    EXPECT_NEAR(result, 2.0, 1e-3);

    // Test with sine function
    result = NumericalMethodsD::backwardDifference(
        [this](double x) { return sineFunction(x); }, 1.0, 1e-6);
    EXPECT_NEAR(result, std::cos(1.0), 1e-6);
}

TEST_F(NumericalMethodsTest, GaussianElimination) {
    // Test simple 2x2 system: 2x + y = 5, x + 3y = 7
    // Solution: x = 8/5 = 1.6, y = 9/5 = 1.8
    std::vector<std::vector<double>> A2 = {{2.0, 1.0}, {1.0, 3.0}};
    std::vector<double> b2 = {5.0, 7.0};

    auto result = NumericalMethodsD::gaussianElimination(A2, b2);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result.value()[0], 1.6, 1e-10);
    EXPECT_NEAR(result.value()[1], 1.8, 1e-10);

    // Test 3x3 system
    std::vector<std::vector<double>> A3 = {
        {2.0, 1.0, -1.0}, {-3.0, -1.0, 2.0}, {-2.0, 1.0, 2.0}};
    std::vector<double> b3 = {8.0, -11.0, -3.0};

    result = NumericalMethodsD::gaussianElimination(A3, b3);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result.value()[0], 2.0, 1e-10);
    EXPECT_NEAR(result.value()[1], 3.0, 1e-10);
    EXPECT_NEAR(result.value()[2], -1.0, 1e-10);

    // Test with singular matrix
    std::vector<std::vector<double>> singular = {{1.0, 2.0}, {2.0, 4.0}};
    std::vector<double> b_singular = {3.0, 6.0};

    result = NumericalMethodsD::gaussianElimination(singular, b_singular);
    EXPECT_FALSE(result.has_value());

    // Test with non-square matrix
    std::vector<std::vector<double>> non_square = {{1.0, 2.0, 3.0}};
    std::vector<double> b_non_square = {1.0};

    result = NumericalMethodsD::gaussianElimination(non_square, b_non_square);
    EXPECT_FALSE(result.has_value());

    // Test with mismatched dimensions
    std::vector<std::vector<double>> A_mismatch = {{1.0, 2.0}, {3.0, 4.0}};
    std::vector<double> b_mismatch = {1.0};  // Wrong size

    result = NumericalMethodsD::gaussianElimination(A_mismatch, b_mismatch);
    EXPECT_FALSE(result.has_value());
}

TEST_F(NumericalMethodsTest, GoldenSectionSearch) {
    // Test with quadratic function (parabola opening upward)
    // Golden section search converges to within tolerance, use 1e-7 for test
    auto result = NumericalMethodsD::goldenSectionSearch(
        [this](double x) { return quadraticFunction(x); }, -2.0, 2.0);
    // Minimum of x^2 - 4 is at x = 0
    EXPECT_NEAR(result, 0.0, 1e-7);

    // Test with shifted quadratic
    result = NumericalMethodsD::goldenSectionSearch(
        [this](double x) { return (x - 1.0) * (x - 1.0) + 2.0; }, -5.0, 5.0);
    // Minimum of (x-1)^2 + 2 is at x = 1
    EXPECT_NEAR(result, 1.0, 1e-7);

    // Test with cubic function (has local minimum)
    result = NumericalMethodsD::goldenSectionSearch(
        [this](double x) { return x * x * x - 3.0 * x + 1.0; }, -2.0, 2.0);
    // Local minimum should be around x = 1
    EXPECT_NEAR(result, 1.0, 1e-3);

    // Test with exponential function (monotonically increasing)
    result = NumericalMethodsD::goldenSectionSearch(
        [this](double x) { return exponentialFunction(x); }, -1.0, 1.0);
    // Minimum at left boundary
    EXPECT_NEAR(result, -1.0, 1e-7);

    // Test with narrow interval
    result = NumericalMethodsD::goldenSectionSearch(
        [this](double x) { return quadraticFunction(x); }, -0.1, 0.1);
    EXPECT_NEAR(result, 0.0, 1e-7);
}

TEST_F(NumericalMethodsTest, FloatType) {
    // Test with float type
    auto result = NumericalMethodsF::newtonRaphson(
        [](float x) { return 2.0f * x - 3.0f; }, [](float x) { return 2.0f; },
        0.0f);
    ASSERT_TRUE(result.has_value());
    EXPECT_FLOAT_EQ(result.value(), 1.5f);

    // Test integration with float
    float integral = NumericalMethodsF::trapezoidalRule(
        [](float x) { return 2.0f * x - 3.0f; }, 0.0f, 2.0f, 100);
    EXPECT_NEAR(integral, -2.0f, 1e-6f);
}

TEST_F(NumericalMethodsTest, EdgeCases) {
    // Test root finding with functions that have no roots
    auto result = NumericalMethodsD::newtonRaphson(
        [](double x) { return x * x + 1.0; },  // Always positive
        [](double x) { return 2.0 * x; }, 1.0);
    EXPECT_FALSE(result.has_value());

    // Test with very small tolerance (not zero, as zero tolerance may never
    // converge)
    result = NumericalMethodsD::newtonRaphson(
        [this](double x) { return linearFunction(x); },
        [this](double x) { return linearDerivative(x); }, 0.0, 1e-15);
    EXPECT_TRUE(result.has_value());  // Linear should converge quickly

    // Test integration with same limits
    double integral = NumericalMethodsD::trapezoidalRule(
        [this](double x) { return linearFunction(x); }, 1.0, 1.0, 100);
    EXPECT_DOUBLE_EQ(integral, 0.0);

    // Test differentiation at a point where function is well-defined
    // sqrt(x) derivative at x=1 is 1/(2*sqrt(1)) = 0.5
    double derivative = NumericalMethodsD::centralDifference(
        [this](double x) { return sqrtFunction(x); }, 1.0, 1e-8);
    EXPECT_NEAR(derivative, 0.5, 1e-6);
}

TEST_F(NumericalMethodsTest, Performance) {
    const size_t num_evaluations = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    // Test many root-finding operations
    // sin(0) - 0.5 = -0.5, sin(π/2) - 0.5 = 0.5, so root is in [0, π/2]
    for (size_t i = 0; i < num_evaluations; ++i) {
        auto result = NumericalMethodsD::bisection(
            [this](double x) { return sineFunction(x) - 0.5; }, 0.0,
            M_PI / 2.0);
        EXPECT_TRUE(result.has_value());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    spdlog::info("Completed {} bisection root-finding operations in {} ms",
                 num_evaluations, duration.count());

    // Test many integration operations
    start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_evaluations; ++i) {
        double integral = NumericalMethodsD::simpsonsRule(
            [this](double x) { return sineFunction(x); }, 0.0, M_PI, 100);
        EXPECT_NEAR(integral, 2.0, 1e-6);
    }

    end = std::chrono::high_resolution_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    spdlog::info("Completed {} Simpson's rule integrations in {} ms",
                 num_evaluations, duration.count());
}

TEST_F(NumericalMethodsTest, Comparison) {
    // Compare accuracy of different root-finding methods
    double tolerance = 1e-12;

    auto newton_result = NumericalMethodsD::newtonRaphson(
        [this](double x) { return quadraticFunction(x); },
        [this](double x) { return quadraticDerivative(x); }, 3.0, tolerance);

    auto bisection_result = NumericalMethodsD::bisection(
        [this](double x) { return quadraticFunction(x); }, 1.0, 3.0, tolerance);

    auto secant_result = NumericalMethodsD::secant(
        [this](double x) { return quadraticFunction(x); }, 1.0, 3.0, tolerance);

    ASSERT_TRUE(newton_result.has_value());
    ASSERT_TRUE(bisection_result.has_value());
    ASSERT_TRUE(secant_result.has_value());

    // All should give similar results
    EXPECT_NEAR(newton_result.value(), 2.0, tolerance);
    EXPECT_NEAR(bisection_result.value(), 2.0, tolerance);
    EXPECT_NEAR(secant_result.value(), 2.0, tolerance);

    // Compare integration methods
    double expected_integral = 2.0;  // ∫sin(x) from 0 to π
    usize n = 1000;

    double trapezoidal = NumericalMethodsD::trapezoidalRule(
        [this](double x) { return sineFunction(x); }, 0.0, M_PI, n);

    double simpsons = NumericalMethodsD::simpsonsRule(
        [this](double x) { return sineFunction(x); }, 0.0, M_PI, n);

    // Simpson's rule should be more accurate than trapezoidal rule
    double trapezoidal_error = std::abs(trapezoidal - expected_integral);
    double simpsons_error = std::abs(simpsons - expected_integral);

    EXPECT_LT(simpsons_error, trapezoidal_error);
    EXPECT_LT(simpsons_error, 1e-6);
}
