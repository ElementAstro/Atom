#ifndef ATOM_EXTRA_BOOST_TEST_MATH_HPP
#define ATOM_EXTRA_BOOST_TEST_MATH_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cmath>
#include <functional>
#include <stdexcept>
#include <vector>

#include "atom/extra/boost/math.hpp"

namespace atom::extra::boost::test {

using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::FloatNear;
using ::testing::Gt;
using ::testing::Lt;
using ::testing::ThrowsMessage;

// Test tolerance values
constexpr double DOUBLE_TOL = 1e-10;
constexpr double FLOAT_TOL = 1e-6;

class SpecialFunctionsTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(SpecialFunctionsTest, BetaFunction) {
    // Test beta function with known values
    EXPECT_NEAR(SpecialFunctions<double>::beta(1.0, 1.0), 1.0, DOUBLE_TOL);
    EXPECT_NEAR(SpecialFunctions<double>::beta(2.0, 3.0), 0.0833333333333333,
                DOUBLE_TOL);
    EXPECT_NEAR(SpecialFunctions<double>::beta(5.0, 2.0), 0.0333333333333333,
                DOUBLE_TOL);

    // Test with float
    EXPECT_NEAR(SpecialFunctions<float>::beta(1.0f, 1.0f), 1.0f, FLOAT_TOL);
}

TEST_F(SpecialFunctionsTest, GammaFunction) {
    // Test gamma function with known values
    EXPECT_NEAR(SpecialFunctions<double>::gamma(1.0), 1.0, DOUBLE_TOL);
    EXPECT_NEAR(SpecialFunctions<double>::gamma(2.0), 1.0, DOUBLE_TOL);
    EXPECT_NEAR(SpecialFunctions<double>::gamma(3.0), 2.0, DOUBLE_TOL);
    EXPECT_NEAR(SpecialFunctions<double>::gamma(4.0), 6.0, DOUBLE_TOL);
    EXPECT_NEAR(SpecialFunctions<double>::gamma(5.0), 24.0, DOUBLE_TOL);

    // Test with float
    EXPECT_NEAR(SpecialFunctions<float>::gamma(5.0f), 24.0f, FLOAT_TOL);
}

TEST_F(SpecialFunctionsTest, DigammaFunction) {
    // Test digamma function with known values
    EXPECT_NEAR(SpecialFunctions<double>::digamma(1.0), -0.57721566490153,
                DOUBLE_TOL);
    EXPECT_NEAR(SpecialFunctions<double>::digamma(2.0), 0.42278433509847,
                DOUBLE_TOL);

    // Test with float
    EXPECT_NEAR(SpecialFunctions<float>::digamma(2.0f), 0.42278433509847f,
                FLOAT_TOL);
}

TEST_F(SpecialFunctionsTest, ErfFunction) {
    // Test error function with known values
    EXPECT_NEAR(SpecialFunctions<double>::erf(0.0), 0.0, DOUBLE_TOL);
    EXPECT_NEAR(SpecialFunctions<double>::erf(1.0), 0.84270079294971,
                DOUBLE_TOL);
    EXPECT_NEAR(SpecialFunctions<double>::erf(-1.0), -0.84270079294971,
                DOUBLE_TOL);

    // Test with float
    EXPECT_NEAR(SpecialFunctions<float>::erf(1.0f), 0.84270079294971f,
                FLOAT_TOL);
}

TEST_F(SpecialFunctionsTest, BesselJFunction) {
    // Test Bessel J function with known values
    EXPECT_NEAR(SpecialFunctions<double>::besselJ(0, 0.0), 1.0, DOUBLE_TOL);
    EXPECT_NEAR(SpecialFunctions<double>::besselJ(0, 1.0), 0.7651976865579,
                DOUBLE_TOL);
    EXPECT_NEAR(SpecialFunctions<double>::besselJ(1, 1.0), 0.4400505857449,
                DOUBLE_TOL);

    // Test with float
    EXPECT_NEAR(SpecialFunctions<float>::besselJ(0, 1.0f), 0.7651976865579f,
                FLOAT_TOL);
}

TEST_F(SpecialFunctionsTest, LegendrePFunction) {
    // Test Legendre P function with known values
    EXPECT_NEAR(SpecialFunctions<double>::legendreP(0, 0.0), 1.0, DOUBLE_TOL);
    EXPECT_NEAR(SpecialFunctions<double>::legendreP(1, 0.5), 0.5, DOUBLE_TOL);
    EXPECT_NEAR(SpecialFunctions<double>::legendreP(2, 0.5), -0.125,
                DOUBLE_TOL);

    // Test with float
    EXPECT_NEAR(SpecialFunctions<float>::legendreP(1, 0.5f), 0.5f, FLOAT_TOL);
}

class StatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test data
        testData = {1.0, 2.0, 3.0, 4.0, 5.0};

        // Set up data with more variance
        dataWithVariance = {10.0, 20.0, 30.0, 40.0, 50.0};

        // Set up data with skewness
        dataWithSkew = {1.0, 2.0, 3.0, 4.0, 10.0};

        // Set up data with positive kurtosis (more peaked)
        dataWithKurtosis = {-5.0, -1.0, 0.0, 0.0, 0.0, 1.0, 5.0};
    }

    std::vector<double> testData;
    std::vector<double> dataWithVariance;
    std::vector<double> dataWithSkew;
    std::vector<double> dataWithKurtosis;
};

TEST_F(StatisticsTest, Mean) {
    // Test mean calculation
    EXPECT_NEAR(Statistics<double>::mean(testData), 3.0, DOUBLE_TOL);
    EXPECT_NEAR(Statistics<double>::mean(dataWithVariance), 30.0, DOUBLE_TOL);

    // Test with empty data
    EXPECT_THROW(Statistics<double>::mean({}), std::runtime_error);
}

TEST_F(StatisticsTest, Variance) {
    // Test variance calculation
    EXPECT_NEAR(Statistics<double>::variance(testData), 2.5, DOUBLE_TOL);
    EXPECT_NEAR(Statistics<double>::variance(dataWithVariance), 250.0,
                DOUBLE_TOL);

    // Test with empty data
    EXPECT_THROW(Statistics<double>::variance({}), std::runtime_error);
}

TEST_F(StatisticsTest, Skewness) {
    // Test skewness calculation
    EXPECT_NEAR(Statistics<double>::skewness(testData), 0.0, DOUBLE_TOL);
    EXPECT_GT(Statistics<double>::skewness(dataWithSkew),
              0.0);  // Positive skew

    // Test with empty data
    EXPECT_THROW(Statistics<double>::skewness({}), std::runtime_error);
}

TEST_F(StatisticsTest, Kurtosis) {
    // Test kurtosis calculation
    double testDataKurtosis = Statistics<double>::kurtosis(testData);
    double withKurtosisVal = Statistics<double>::kurtosis(dataWithKurtosis);

    // Test with empty data
    EXPECT_THROW(Statistics<double>::kurtosis({}), std::runtime_error);
}

class DistributionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up normal distribution
        normalDist =
            std::make_unique<Distributions<double>::NormalDistribution>(0.0,
                                                                        1.0);

        // Set up t-distribution
        tDist =
            std::make_unique<Distributions<double>::StudentTDistribution>(5.0);

        // Set up Poisson distribution
        poissonDist =
            std::make_unique<Distributions<double>::PoissonDistribution>(3.0);

        // Set up exponential distribution
        expDist =
            std::make_unique<Distributions<double>::ExponentialDistribution>(
                1.0);
    }

    std::unique_ptr<Distributions<double>::NormalDistribution> normalDist;
    std::unique_ptr<Distributions<double>::StudentTDistribution> tDist;
    std::unique_ptr<Distributions<double>::PoissonDistribution> poissonDist;
    std::unique_ptr<Distributions<double>::ExponentialDistribution> expDist;
};

TEST_F(DistributionsTest, NormalDistribution) {
    // Test PDF values
    EXPECT_NEAR(normalDist->pdf(0.0), 0.3989422804014327, DOUBLE_TOL);
    EXPECT_NEAR(normalDist->pdf(1.0), 0.24197072451914337, DOUBLE_TOL);

    // Test CDF values
    EXPECT_NEAR(normalDist->cdf(0.0), 0.5, DOUBLE_TOL);
    EXPECT_NEAR(normalDist->cdf(1.0), 0.8413447460685429, DOUBLE_TOL);

    // Test quantile values
    EXPECT_NEAR(normalDist->quantile(0.5), 0.0, DOUBLE_TOL);
    EXPECT_NEAR(normalDist->quantile(0.975), 1.96, 0.01);  // Approximate
}

TEST_F(DistributionsTest, StudentTDistribution) {
    // Test PDF values
    EXPECT_NEAR(tDist->pdf(0.0), 0.3796074730251464, DOUBLE_TOL);

    // Test CDF values
    EXPECT_NEAR(tDist->cdf(0.0), 0.5, DOUBLE_TOL);
    EXPECT_NEAR(tDist->cdf(1.0), 0.8186429488517661, DOUBLE_TOL);

    // Test quantile values
    EXPECT_NEAR(tDist->quantile(0.5), 0.0, DOUBLE_TOL);
    EXPECT_NEAR(tDist->quantile(0.975), 2.571, 0.01);  // Approximate
}

TEST_F(DistributionsTest, PoissonDistribution) {
    // Test PDF values
    EXPECT_NEAR(poissonDist->pdf(0), 0.04978706836786395, DOUBLE_TOL);
    EXPECT_NEAR(poissonDist->pdf(3), 0.2240418108349196, DOUBLE_TOL);

    // Test CDF values
    EXPECT_NEAR(poissonDist->cdf(0), 0.04978706836786395, DOUBLE_TOL);
    EXPECT_NEAR(poissonDist->cdf(3), 0.6472883255927178, DOUBLE_TOL);
}

TEST_F(DistributionsTest, ExponentialDistribution) {
    // Test PDF values
    EXPECT_NEAR(expDist->pdf(0.0), 1.0, DOUBLE_TOL);
    EXPECT_NEAR(expDist->pdf(1.0), 0.36787944117144233, DOUBLE_TOL);

    // Test CDF values
    EXPECT_NEAR(expDist->cdf(0.0), 0.0, DOUBLE_TOL);
    EXPECT_NEAR(expDist->cdf(1.0), 0.6321205588285577, DOUBLE_TOL);
}

class NumericalIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test functions
        linearFunc = [](double x) { return x; };
        squareFunc = [](double x) { return x * x; };
        sineFunc = [](double x) { return std::sin(x); };
    }

    std::function<double(double)> linearFunc;
    std::function<double(double)> squareFunc;
    std::function<double(double)> sineFunc;
};

TEST_F(NumericalIntegrationTest, TrapezoidalRule) {
    // Test with linear function: ∫x dx from 0 to 1 = 0.5
    EXPECT_NEAR(NumericalIntegration<double>::trapezoidal(linearFunc, 0.0, 1.0),
                0.5, DOUBLE_TOL);

    // Test with square function: ∫x² dx from 0 to 1 = 1/3
    EXPECT_NEAR(NumericalIntegration<double>::trapezoidal(squareFunc, 0.0, 1.0),
                1.0 / 3.0, DOUBLE_TOL);

    // Test with sine function: ∫sin(x) dx from 0 to π = 2
    EXPECT_NEAR(NumericalIntegration<double>::trapezoidal(sineFunc, 0.0, M_PI),
                2.0, DOUBLE_TOL);
}

class FactorialTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(FactorialTest, IntegerFactorial) {
    // Test factorials of integers
    EXPECT_EQ(factorial<int>(0), 1);
    EXPECT_EQ(factorial<int>(1), 1);
    EXPECT_EQ(factorial<int>(2), 2);
    EXPECT_EQ(factorial<int>(3), 6);
    EXPECT_EQ(factorial<int>(4), 24);
    EXPECT_EQ(factorial<int>(5), 120);
}

TEST_F(FactorialTest, FloatingPointFactorial) {
    // Test factorial with floating-point numbers
    EXPECT_NEAR(factorial<double>(0.0), 1.0, DOUBLE_TOL);
    EXPECT_NEAR(factorial<double>(1.0), 1.0, DOUBLE_TOL);
    EXPECT_NEAR(factorial<double>(4.0), 24.0, DOUBLE_TOL);

    // Test with non-integer values
    EXPECT_NEAR(factorial<double>(0.5), 0.886226925452758, DOUBLE_TOL);
    EXPECT_NEAR(factorial<double>(1.5), 1.3293403881791, 0.0001);
}

class TransformRangeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test data
        testData = {1, 2, 3, 4, 5};
    }

    std::vector<int> testData;
};

TEST_F(TransformRangeTest, BasicTransformation) {
    // Double each element
    auto doubledView = transformRange(testData, [](int x) { return x * 2; });
    std::vector<int> doubledVector(doubledView.begin(), doubledView.end());
    EXPECT_THAT(doubledVector, ElementsAre(2, 4, 6, 8, 10));

    // Square each element
    auto squaredView = transformRange(testData, [](int x) { return x * x; });
    std::vector<int> squaredVector(squaredView.begin(), squaredView.end());
    EXPECT_THAT(squaredVector, ElementsAre(1, 4, 9, 16, 25));
}

class OptimizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test functions
        // f(x) = x^2, min at x = 0
        parabola = [](double x) { return x * x; };

        // f(x) = (x-2)^2, min at x = 2
        shiftedParabola = [](double x) { return (x - 2) * (x - 2); };

        // f(x) = x^3 - 6x^2 + 9x + 1, has a root at x = 1
        cubicFunc = [](double x) { return x * x * x - 6 * x * x + 9 * x + 1; };

        // Derivative of cubicFunc: f'(x) = 3x^2 - 12x + 9
        cubicDerivative = [](double x) { return 3 * x * x - 12 * x + 9; };
    }

    std::function<double(double)> parabola;
    std::function<double(double)> shiftedParabola;
    std::function<double(double)> cubicFunc;
    std::function<double(double)> cubicDerivative;
};

TEST_F(OptimizationTest, GoldenSectionSearch) {
    // Test finding minimum of parabola
    double minPoint =
        Optimization<double>::goldenSectionSearch(parabola, -1.0, 1.0, 1e-6);
    EXPECT_NEAR(minPoint, 0.0, 0.001);

    // Test finding minimum of shifted parabola
    minPoint = Optimization<double>::goldenSectionSearch(shiftedParabola, 0.0,
                                                         4.0, 1e-6);
    EXPECT_NEAR(minPoint, 2.0, 0.001);
}

TEST_F(OptimizationTest, NewtonRaphson) {
    // Test finding root of cubic function
    double root = Optimization<double>::newtonRaphson(
        cubicFunc, cubicDerivative, 0.5, 1e-6, 100);
    EXPECT_NEAR(root, 1.0, 0.001);

    // Test with bad initial guess but still converges
    root = Optimization<double>::newtonRaphson(cubicFunc, cubicDerivative, 0.9,
                                               1e-6, 100);
    EXPECT_NEAR(root, 1.0, 0.001);

    // Test exception when derivative is zero
    auto constantDerivative = [](double) { return 0.0; };
    EXPECT_THROW(Optimization<double>::newtonRaphson(
                     cubicFunc, constantDerivative, 0.5, 1e-6, 100),
                 std::runtime_error);

    // Test exception when max iterations reached
    EXPECT_THROW(Optimization<double>::newtonRaphson(
                     [](double x) {
                         return std::sin(x);
                     },  // Function that needs many iterations
                     [](double x) { return std::cos(x); },
                     3.0,    // Bad initial guess
                     1e-10,  // Very tight tolerance
                     1       // Only one iteration allowed
                     ),
                 std::runtime_error);
}

class LinearAlgebraTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test matrices and vectors
        matrixA.resize(2, 2);
        matrixA(0, 0) = 2.0;
        matrixA(0, 1) = 1.0;
        matrixA(1, 0) = 1.0;
        matrixA(1, 1) = 3.0;

        matrixB.resize(2, 2);
        matrixB(0, 0) = 1.0;
        matrixB(0, 1) = 2.0;
        matrixB(1, 0) = 3.0;
        matrixB(1, 1) = 4.0;

        vectorB.resize(2);
        vectorB(0) = 5.0;
        vectorB(1) = 7.0;
    }

    LinearAlgebra<double>::Matrix matrixA;
    LinearAlgebra<double>::Matrix matrixB;
    LinearAlgebra<double>::Vector vectorB;
};

TEST_F(LinearAlgebraTest, SolveLinearSystem) {
    // Test solving Ax = b
    LinearAlgebra<double>::Vector solution =
        LinearAlgebra<double>::solveLinearSystem(matrixA, vectorB);

    // Expected solution
    EXPECT_NEAR(solution(0), 1.5, DOUBLE_TOL);
    EXPECT_NEAR(solution(1), 2.0, DOUBLE_TOL);

    // Verify solution: A*x should equal b
    double b0 = matrixA(0, 0) * solution(0) + matrixA(0, 1) * solution(1);
    double b1 = matrixA(1, 0) * solution(0) + matrixA(1, 1) * solution(1);
    EXPECT_NEAR(b0, vectorB(0), DOUBLE_TOL);
    EXPECT_NEAR(b1, vectorB(1), DOUBLE_TOL);
}

TEST_F(LinearAlgebraTest, Determinant) {
    // Test determinant calculation
    double detA = LinearAlgebra<double>::determinant(matrixA);
    EXPECT_NEAR(detA, 5.0,
                DOUBLE_TOL);  // det([[2, 1], [1, 3]]) = 2*3 - 1*1 = 5

    double detB = LinearAlgebra<double>::determinant(matrixB);
    EXPECT_NEAR(detB, -2.0,
                DOUBLE_TOL);  // det([[1, 2], [3, 4]]) = 1*4 - 2*3 = -2
}

TEST_F(LinearAlgebraTest, MatrixMultiplication) {
    // Test matrix multiplication
    LinearAlgebra<double>::Matrix product =
        LinearAlgebra<double>::multiply(matrixA, matrixB);

    // Expected result: C = A*B
    EXPECT_NEAR(product(0, 0), 5.0, DOUBLE_TOL);   // C(0,0) = 2*1 + 1*3 = 5
    EXPECT_NEAR(product(0, 1), 8.0, DOUBLE_TOL);   // C(0,1) = 2*2 + 1*4 = 8
    EXPECT_NEAR(product(1, 0), 10.0, DOUBLE_TOL);  // C(1,0) = 1*1 + 3*3 = 10
    EXPECT_NEAR(product(1, 1), 14.0, DOUBLE_TOL);  // C(1,1) = 1*2 + 3*4 = 14
}

TEST_F(LinearAlgebraTest, MatrixTranspose) {
    // Test matrix transpose
    LinearAlgebra<double>::Matrix transposeA =
        LinearAlgebra<double>::transpose(matrixA);

    // Expected result: A^T
    EXPECT_NEAR(transposeA(0, 0), 2.0, DOUBLE_TOL);
    EXPECT_NEAR(transposeA(0, 1), 1.0, DOUBLE_TOL);
    EXPECT_NEAR(transposeA(1, 0), 1.0, DOUBLE_TOL);
    EXPECT_NEAR(transposeA(1, 1), 3.0, DOUBLE_TOL);

    // Test non-square matrix
    LinearAlgebra<double>::Matrix matrixC(2, 3);
    matrixC(0, 0) = 1.0;
    matrixC(0, 1) = 2.0;
    matrixC(0, 2) = 3.0;
    matrixC(1, 0) = 4.0;
    matrixC(1, 1) = 5.0;
    matrixC(1, 2) = 6.0;

    LinearAlgebra<double>::Matrix transposeC =
        LinearAlgebra<double>::transpose(matrixC);

    // Check dimensions
    EXPECT_EQ(transposeC.size1(), 3);
    EXPECT_EQ(transposeC.size2(), 2);

    // Check values
    EXPECT_NEAR(transposeC(0, 0), 1.0, DOUBLE_TOL);
    EXPECT_NEAR(transposeC(0, 1), 4.0, DOUBLE_TOL);
    EXPECT_NEAR(transposeC(1, 0), 2.0, DOUBLE_TOL);
    EXPECT_NEAR(transposeC(1, 1), 5.0, DOUBLE_TOL);
    EXPECT_NEAR(transposeC(2, 0), 3.0, DOUBLE_TOL);
    EXPECT_NEAR(transposeC(2, 1), 6.0, DOUBLE_TOL);
}

class ODESolverTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Define test ODE systems

        // Simple exponential growth: dy/dt = y
        exponentialSystem = [](const ODESolver<double>::State& y,
                               ODESolver<double>::State& dydt,
                               double /*t*/) { dydt[0] = y[0]; };

        // Simple harmonic oscillator: d²x/dt² + x = 0
        // As a system: dx/dt = v, dv/dt = -x
        oscillatorSystem = [](const ODESolver<double>::State& y,
                              ODESolver<double>::State& dydt, double /*t*/) {
            dydt[0] = y[1];   // dx/dt = v
            dydt[1] = -y[0];  // dv/dt = -x
        };
    }

    ODESolver<double>::SystemFunction exponentialSystem;
    ODESolver<double>::SystemFunction oscillatorSystem;
};

TEST_F(ODESolverTest, RungeKutta4ExponentialGrowth) {
    // Test exponential growth: y' = y, y(0) = 1 => y(t) = e^t
    ODESolver<double>::State initialState = {1.0};
    auto solution = ODESolver<double>::rungeKutta4(exponentialSystem,
                                                   initialState, 0.0, 1.0, 0.1);

    // Check final value (should be close to e^1 = 2.71828...)
    EXPECT_NEAR(solution.back()[0], std::exp(1.0), 0.01);

    // Check that solution size is as expected (11 points including start and
    // end)
    EXPECT_EQ(solution.size(), 11);
}

TEST_F(ODESolverTest, RungeKutta4HarmonicOscillator) {
    // Test simple harmonic oscillator: x'' + x = 0
    // Initial conditions: x(0) = 0, v(0) = 1
    // Solution: x(t) = sin(t), v(t) = cos(t)
    ODESolver<double>::State initialState = {0.0, 1.0};
    auto solution = ODESolver<double>::rungeKutta4(
        oscillatorSystem, initialState, 0.0, M_PI, 0.1);

    // At t = π, x should be close to sin(π) = 0 and v should be close to cos(π)
    // = -1
    EXPECT_NEAR(solution.back()[0], 0.0, 0.01);
    EXPECT_NEAR(solution.back()[1], -1.0, 0.01);
}

class FinancialMathTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up parameters for financial calculations
        stockPrice = 100.0;
        strikePrice = 100.0;
        riskFreeRate = 0.05;   // 5%
        volatility = 0.2;      // 20%
        timeToMaturity = 1.0;  // 1 year

        yield = 0.05;        // 5%
        couponRate = 0.06;   // 6%
        faceValue = 1000.0;  // $1000
        periods = 2;         // 2 periods
    }

    double stockPrice;
    double strikePrice;
    double riskFreeRate;
    double volatility;
    double timeToMaturity;

    double yield;
    double couponRate;
    double faceValue;
    int periods;
};

TEST_F(FinancialMathTest, BlackScholesCall) {
    // Test Black-Scholes formula for at-the-money option
    double callPrice = FinancialMath<double>::blackScholesCall(
        stockPrice, strikePrice, riskFreeRate, volatility, timeToMaturity);

    // Expected price for given parameters (approximately 10.45)
    EXPECT_NEAR(callPrice, 10.45, 0.1);

    // Test with deep in-the-money option
    callPrice = FinancialMath<double>::blackScholesCall(
        120.0, strikePrice, riskFreeRate, volatility, timeToMaturity);
    EXPECT_GT(callPrice, 20.0);

    // Test with deep out-of-the-money option
    callPrice = FinancialMath<double>::blackScholesCall(
        80.0, strikePrice, riskFreeRate, volatility, timeToMaturity);
    EXPECT_LT(callPrice, 5.0);
}

TEST_F(FinancialMathTest, BondPrice) {
    // Test bond price calculation
    double price =
        FinancialMath<double>::bondPrice(yield, couponRate, faceValue, periods);

    // Expected price for given parameters
    // Coupon payment = 0.06 * 1000 / 2 = 30 per period
    // PV = 30/(1.025) + 30/(1.025)² + 1000/(1.025)² = 1019.51
    EXPECT_NEAR(price, 1019.51, 0.1);

    // Test with higher yield (price should be lower)
    double higherYieldPrice =
        FinancialMath<double>::bondPrice(0.07, couponRate, faceValue, periods);
    EXPECT_LT(higherYieldPrice, price);

    // Test with lower yield (price should be higher)
    double lowerYieldPrice =
        FinancialMath<double>::bondPrice(0.03, couponRate, faceValue, periods);
    EXPECT_GT(lowerYieldPrice, price);
}

TEST_F(FinancialMathTest, ModifiedDuration) {
    // Test modified duration calculation
    double duration = FinancialMath<double>::modifiedDuration(
        yield, couponRate, faceValue, periods);

    // Expected duration for given parameters
    EXPECT_GT(duration, 0.0);

    // Test relationship between duration and price changes
    double initialPrice =
        FinancialMath<double>::bondPrice(yield, couponRate, faceValue, periods);

    // Small change in yield
    double deltaYield = 0.0001;
    double newPrice = FinancialMath<double>::bondPrice(
        yield + deltaYield, couponRate, faceValue, periods);

    // Price change approximately equals -duration * price * change in yield
    double expectedChange = -duration * initialPrice * deltaYield;
    double actualChange = newPrice - initialPrice;

    EXPECT_NEAR(actualChange / expectedChange, 1.0, 0.1);
}

TEST_F(FinancialMathTest, ImpliedVolatility) {
    // Calculate option price with known volatility
    double knownPrice = FinancialMath<double>::blackScholesCall(
        stockPrice, strikePrice, riskFreeRate, volatility, timeToMaturity);

    // Now use this price to recover the implied volatility
    double impliedVol = FinancialMath<double>::impliedVolatility(
        knownPrice, stockPrice, strikePrice, riskFreeRate, timeToMaturity);

    // The recovered volatility should be close to the original
    EXPECT_NEAR(impliedVol, volatility, 0.01);
}

}  // namespace atom::extra::boost::test

#endif  // ATOM_EXTRA_BOOST_TEST_MATH_HPP
