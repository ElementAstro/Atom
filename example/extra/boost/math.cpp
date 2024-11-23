#include "atom/extra/boost/math.hpp"

#include <cmath>
#include <iostream>
#include <vector>

using namespace atom::extra::boost;

int main() {
    // SpecialFunctions usage
    double alpha = 2.0, beta = 3.0;
    double value = 5.0;
    std::cout << "Beta function: "
              << SpecialFunctions<double>::beta(alpha, beta) << std::endl;
    std::cout << "Gamma function: " << SpecialFunctions<double>::gamma(value)
              << std::endl;
    std::cout << "Digamma function: "
              << SpecialFunctions<double>::digamma(value) << std::endl;
    std::cout << "Error function: " << SpecialFunctions<double>::erf(value)
              << std::endl;
    std::cout << "Bessel function J: "
              << SpecialFunctions<double>::besselJ(1, value) << std::endl;
    std::cout << "Legendre polynomial: "
              << SpecialFunctions<double>::legendreP(2, value) << std::endl;

    // Statistics usage
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::cout << "Mean: " << Statistics<double>::mean(data) << std::endl;
    std::cout << "Variance: " << Statistics<double>::variance(data)
              << std::endl;
    std::cout << "Skewness: " << Statistics<double>::skewness(data)
              << std::endl;
    std::cout << "Kurtosis: " << Statistics<double>::kurtosis(data)
              << std::endl;

    // Distributions usage
    Distributions<double>::NormalDistribution normalDist(0.0, 1.0);
    std::cout << "Normal PDF: " << normalDist.pdf(0.0) << std::endl;
    std::cout << "Normal CDF: " << normalDist.cdf(0.0) << std::endl;
    std::cout << "Normal Quantile: " << normalDist.quantile(0.5) << std::endl;

    Distributions<double>::StudentTDistribution studentTDist(10.0);
    std::cout << "Student's t PDF: " << studentTDist.pdf(0.0) << std::endl;
    std::cout << "Student's t CDF: " << studentTDist.cdf(0.0) << std::endl;
    std::cout << "Student's t Quantile: " << studentTDist.quantile(0.5)
              << std::endl;

    Distributions<double>::PoissonDistribution poissonDist(3.0);
    std::cout << "Poisson PDF: " << poissonDist.pdf(2.0) << std::endl;
    std::cout << "Poisson CDF: " << poissonDist.cdf(2.0) << std::endl;

    Distributions<double>::ExponentialDistribution expDist(1.0);
    std::cout << "Exponential PDF: " << expDist.pdf(1.0) << std::endl;
    std::cout << "Exponential CDF: " << expDist.cdf(1.0) << std::endl;

    // NumericalIntegration usage
    auto func = [](double x) { return std::sin(x); };
    std::cout << "Trapezoidal integration: "
              << NumericalIntegration<double>::trapezoidal(func, 0.0, M_PI)
              << std::endl;

    // Optimization usage
    auto quadratic = [](double x) { return (x - 2) * (x - 2); };
    std::cout << "Golden section search: "
              << Optimization<double>::goldenSectionSearch(quadratic, 0.0, 4.0,
                                                           1e-6)
              << std::endl;

    auto cubic = [](double x) { return x * x * x - 2 * x - 5; };
    auto cubicDerivative = [](double x) { return 3 * x * x - 2; };
    std::cout << "Newton-Raphson: "
              << Optimization<double>::newtonRaphson(cubic, cubicDerivative,
                                                     2.0, 1e-6, 100)
              << std::endl;

    // LinearAlgebra usage
    using Matrix = LinearAlgebra<double>::Matrix;
    using Vector = LinearAlgebra<double>::Vector;

    Matrix A(2, 2);
    A(0, 0) = 3;
    A(0, 1) = 2;
    A(1, 0) = 1;
    A(1, 1) = 2;

    Vector b(2);
    b(0) = 5;
    b(1) = 5;

    Vector x = LinearAlgebra<double>::solveLinearSystem(A, b);
    std::cout << "Solution to Ax = b: [" << x(0) << ", " << x(1) << "]"
              << std::endl;

    std::cout << "Determinant of A: " << LinearAlgebra<double>::determinant(A)
              << std::endl;

    Matrix B(2, 2);
    B(0, 0) = 1;
    B(0, 1) = 2;
    B(1, 0) = 3;
    B(1, 1) = 4;

    Matrix C = LinearAlgebra<double>::multiply(A, B);
    std::cout << "Product of A and B: [[" << C(0, 0) << ", " << C(0, 1)
              << "], [" << C(1, 0) << ", " << C(1, 1) << "]]" << std::endl;

    Matrix At = LinearAlgebra<double>::transpose(A);
    std::cout << "Transpose of A: [[" << At(0, 0) << ", " << At(0, 1) << "], ["
              << At(1, 0) << ", " << At(1, 1) << "]]" << std::endl;

    // ODESolver usage
    auto system = [](const std::vector<double>& y, std::vector<double>& dydt,
                     double t) {
        dydt[0] = y[1];
        dydt[1] = -y[0];
    };
    std::vector<double> initialState = {1.0, 0.0};
    std::vector<std::vector<double>> solution =
        ODESolver<double>::rungeKutta4(system, initialState, 0.0, 10.0, 0.1);
    std::cout << "ODE solution: ";
    for (const auto& state : solution) {
        std::cout << "[" << state[0] << ", " << state[1] << "] ";
    }
    std::cout << std::endl;

    // FinancialMath usage
    double stockPrice = 100.0;
    double strikePrice = 100.0;
    double riskFreeRate = 0.05;
    double volatility = 0.2;
    double timeToMaturity = 1.0;
    std::cout << "Black-Scholes call option price: "
              << FinancialMath<double>::blackScholesCall(
                     stockPrice, strikePrice, riskFreeRate, volatility,
                     timeToMaturity)
              << std::endl;

    double yield = 0.05;
    double couponRate = 0.06;
    double faceValue = 1000.0;
    int periods = 10;
    std::cout << "Modified duration: "
              << FinancialMath<double>::modifiedDuration(yield, couponRate,
                                                         faceValue, periods)
              << std::endl;

    std::cout << "Bond price: "
              << FinancialMath<double>::bondPrice(yield, couponRate, faceValue,
                                                  periods)
              << std::endl;

    double marketPrice = 10.0;
    std::cout << "Implied volatility: "
              << FinancialMath<double>::impliedVolatility(
                     marketPrice, stockPrice, strikePrice, riskFreeRate,
                     timeToMaturity)
              << std::endl;

    return 0;
}