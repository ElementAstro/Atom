#include "atom/extra/boost/math.hpp"

#include <pybind11/functional.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

template <typename T>
void declare_math_classes(py::module& m, const std::string& type_suffix) {
    // Special Functions class binding
    {
        using SFClass = atom::extra::boost::SpecialFunctions<T>;
        std::string class_name = "SpecialFunctions" + type_suffix;

        py::class_<SFClass>(
            m, class_name.c_str(),
            R"(Wrapper class for special mathematical functions.

This class provides various special mathematical functions like beta, gamma,
digamma, error function, Bessel functions, and Legendre polynomials.

Examples:
    >>> from atom.extra.boost import math
    >>> math.SpecialFunctions.beta(2.0, 3.0)
    0.08333333333333333
)")
            .def_static("beta", &SFClass::beta, py::arg("alpha"),
                        py::arg("beta"),
                        R"(Computes the beta function.

Args:
    alpha: The alpha parameter.
    beta: The beta parameter.

Returns:
    The value of the beta function.
)")
            .def_static("gamma", &SFClass::gamma, py::arg("value"),
                        R"(Computes the gamma function.

Args:
    value: The input value.

Returns:
    The value of the gamma function.
)")
            .def_static("digamma", &SFClass::digamma, py::arg("value"),
                        R"(Computes the digamma function.

Args:
    value: The input value.

Returns:
    The value of the digamma function.
)")
            .def_static("erf", &SFClass::erf, py::arg("value"),
                        R"(Computes the error function.

Args:
    value: The input value.

Returns:
    The value of the error function.
)")
            .def_static("bessel_j", &SFClass::besselJ, py::arg("order"),
                        py::arg("value"),
                        R"(Computes the Bessel function of the first kind.

Args:
    order: The order of the Bessel function.
    value: The input value.

Returns:
    The value of the Bessel function.
)")
            .def_static("legendre_p", &SFClass::legendreP, py::arg("order"),
                        py::arg("value"),
                        R"(Computes the Legendre polynomial.

Args:
    order: The order of the polynomial.
    value: The input value.

Returns:
    The value of the Legendre polynomial.
)");
    }

    // Statistics class binding
    {
        using StatsClass = atom::extra::boost::Statistics<T>;
        std::string class_name = "Statistics" + type_suffix;

        py::class_<StatsClass>(m, class_name.c_str(),
                               R"(Wrapper class for statistical functions.

This class provides statistical functions like mean, variance, skewness, and kurtosis.

Examples:
    >>> from atom.extra.boost import math
    >>> math.Statistics.mean([1.0, 2.0, 3.0, 4.0, 5.0])
    3.0
)")
            .def_static("mean", &StatsClass::mean, py::arg("data"),
                        R"(Computes the mean of a dataset.

Args:
    data: The input dataset as a list.

Returns:
    The mean of the dataset.
)")
            .def_static("variance", &StatsClass::variance, py::arg("data"),
                        R"(Computes the variance of a dataset.

Args:
    data: The input dataset as a list.

Returns:
    The variance of the dataset.
)")
            .def_static("skewness", &StatsClass::skewness, py::arg("data"),
                        R"(Computes the skewness of a dataset.

Args:
    data: The input dataset as a list.

Returns:
    The skewness of the dataset.
)")
            .def_static("kurtosis", &StatsClass::kurtosis, py::arg("data"),
                        R"(Computes the kurtosis of a dataset.

Args:
    data: The input dataset as a list.

Returns:
    The kurtosis of the dataset.
)");
    }

    // Distributions class bindings
    {
        using DistClass = atom::extra::boost::Distributions<T>;
        std::string class_name = "Distributions" + type_suffix;

        // Normal distribution
        std::string normal_name = "NormalDistribution" + type_suffix;
        py::class_<typename DistClass::NormalDistribution> normal_dist(
            m, normal_name.c_str(),
            R"(Normal distribution wrapper class.

This class provides methods to work with the normal (Gaussian) distribution.

Args:
    mean: The mean of the distribution.
    stddev: The standard deviation of the distribution.

Examples:
    >>> from atom.extra.boost import math
    >>> dist = math.NormalDistribution(0.0, 1.0)
    >>> dist.pdf(0.0)
    0.3989422804014327
)");

        normal_dist
            .def(py::init<T, T>(), py::arg("mean"), py::arg("stddev"),
                 "Constructs a normal distribution with given mean and "
                 "standard deviation.")
            .def(
                "pdf", &DistClass::NormalDistribution::pdf, py::arg("value"),
                R"(Computes the probability density function (PDF) at a given value.

Args:
    value: The input value.

Returns:
    The PDF value.
)")
            .def(
                "cdf", &DistClass::NormalDistribution::cdf, py::arg("value"),
                R"(Computes the cumulative distribution function (CDF) at a given value.

Args:
    value: The input value.

Returns:
    The CDF value.
)")
            .def("quantile", &DistClass::NormalDistribution::quantile,
                 py::arg("probability"),
                 R"(Computes the quantile (inverse CDF) at a given probability.

Args:
    probability: The input probability (0.0 to 1.0).

Returns:
    The quantile value.
)");

        // Student's t-distribution
        std::string t_dist_name = "StudentTDistribution" + type_suffix;
        py::class_<typename DistClass::StudentTDistribution> t_dist(
            m, t_dist_name.c_str(),
            R"(Student's t-distribution wrapper class.

This class provides methods to work with the Student's t-distribution.

Args:
    degrees_of_freedom: The degrees of freedom parameter.

Examples:
    >>> from atom.extra.boost import math
    >>> dist = math.StudentTDistribution(5.0)
    >>> dist.pdf(0.0)
    0.3796217051910755
)");

        t_dist
            .def(py::init<T>(), py::arg("degrees_of_freedom"),
                 "Constructs a Student's t-distribution with given degrees of "
                 "freedom.")
            .def(
                "pdf", &DistClass::StudentTDistribution::pdf, py::arg("value"),
                R"(Computes the probability density function (PDF) at a given value.

Args:
    value: The input value.

Returns:
    The PDF value.
)")
            .def(
                "cdf", &DistClass::StudentTDistribution::cdf, py::arg("value"),
                R"(Computes the cumulative distribution function (CDF) at a given value.

Args:
    value: The input value.

Returns:
    The CDF value.
)")
            .def("quantile", &DistClass::StudentTDistribution::quantile,
                 py::arg("probability"),
                 R"(Computes the quantile (inverse CDF) at a given probability.

Args:
    probability: The input probability (0.0 to 1.0).

Returns:
    The quantile value.
)");

        // Poisson distribution
        std::string poisson_name = "PoissonDistribution" + type_suffix;
        py::class_<typename DistClass::PoissonDistribution> poisson_dist(
            m, poisson_name.c_str(),
            R"(Poisson distribution wrapper class.

This class provides methods to work with the Poisson distribution.

Args:
    mean: The mean parameter of the distribution.

Examples:
    >>> from atom.extra.boost import math
    >>> dist = math.PoissonDistribution(5.0)
    >>> dist.pdf(5)
    0.17546736976785074
)");

        poisson_dist
            .def(py::init<T>(), py::arg("mean"),
                 "Constructs a Poisson distribution with given mean.")
            .def(
                "pdf", &DistClass::PoissonDistribution::pdf, py::arg("value"),
                R"(Computes the probability density function (PDF) at a given value.

Args:
    value: The input value.

Returns:
    The PDF value.
)")
            .def(
                "cdf", &DistClass::PoissonDistribution::cdf, py::arg("value"),
                R"(Computes the cumulative distribution function (CDF) at a given value.

Args:
    value: The input value.

Returns:
    The CDF value.
)");

        // Exponential distribution
        std::string exp_name = "ExponentialDistribution" + type_suffix;
        py::class_<typename DistClass::ExponentialDistribution> exp_dist(
            m, exp_name.c_str(),
            R"(Exponential distribution wrapper class.

This class provides methods to work with the exponential distribution.

Args:
    lambda: The rate parameter of the distribution.

Examples:
    >>> from atom.extra.boost import math
    >>> dist = math.ExponentialDistribution(1.0)
    >>> dist.pdf(1.0)
    0.36787944117144233
)");

        exp_dist
            .def(py::init<T>(), py::arg("lambda"),
                 "Constructs an exponential distribution with given rate "
                 "parameter.")
            .def(
                "pdf", &DistClass::ExponentialDistribution::pdf,
                py::arg("value"),
                R"(Computes the probability density function (PDF) at a given value.

Args:
    value: The input value.

Returns:
    The PDF value.
)")
            .def(
                "cdf", &DistClass::ExponentialDistribution::cdf,
                py::arg("value"),
                R"(Computes the cumulative distribution function (CDF) at a given value.

Args:
    value: The input value.

Returns:
    The CDF value.
)");
    }

    // Numerical Integration class binding
    {
        using IntClass = atom::extra::boost::NumericalIntegration<T>;
        std::string class_name = "NumericalIntegration" + type_suffix;

        py::class_<IntClass>(
            m, class_name.c_str(),
            R"(Wrapper class for numerical integration methods.

This class provides numerical integration functions like the trapezoidal rule.

Examples:
    >>> from atom.extra.boost import math
    >>> import math as pymath
    >>> math.NumericalIntegration.trapezoidal(lambda x: pymath.sin(x), 0.0, pymath.pi)
    2.0
)")
            .def_static(
                "trapezoidal", &IntClass::trapezoidal, py::arg("func"),
                py::arg("start"), py::arg("end"),
                R"(Computes the integral of a function using the trapezoidal rule.

Args:
    func: The function to integrate.
    start: The start of the integration interval.
    end: The end of the integration interval.

Returns:
    The computed integral.
)");
    }

    // Optimization class binding
    {
        using OptClass = atom::extra::boost::Optimization<T>;
        std::string class_name = "Optimization" + type_suffix;

        py::class_<OptClass>(m, class_name.c_str(),
                             R"(Wrapper class for optimization methods.

This class provides optimization methods like golden section search and Newton-Raphson.

Examples:
    >>> from atom.extra.boost import math
    >>> def f(x): return (x-2)**2
    >>> math.Optimization.golden_section_search(f, 0.0, 4.0, 1e-6)
    2.0000009536743164
)")
            .def_static(
                "golden_section_search", &OptClass::goldenSectionSearch,
                py::arg("func"), py::arg("start"), py::arg("end"),
                py::arg("tolerance"),
                R"(Performs one-dimensional golden section search to find the minimum of a function.

Args:
    func: The function to minimize.
    start: The start of the search interval.
    end: The end of the search interval.
    tolerance: The tolerance for convergence.

Returns:
    The point where the function attains its minimum.
)")
            .def_static(
                "newton_raphson", &OptClass::newtonRaphson, py::arg("func"),
                py::arg("derivative_func"), py::arg("initial_guess"),
                py::arg("tolerance"), py::arg("max_iterations"),
                R"(Performs Newton-Raphson method to find the root of a function.

Args:
    func: The function whose root is to be found.
    derivative_func: The derivative of the function.
    initial_guess: The initial guess for the root.
    tolerance: The tolerance for convergence.
    max_iterations: The maximum number of iterations.

Returns:
    The root of the function.

Raises:
    RuntimeError: If the derivative is zero or maximum iterations are reached without convergence.
)");
    }

    // Linear Algebra class binding
    {
        using LAClass = atom::extra::boost::LinearAlgebra<T>;
        using Matrix = typename LAClass::Matrix;
        using Vector = typename LAClass::Vector;

        std::string class_name = "LinearAlgebra" + type_suffix;

        // Register Matrix and Vector types
        std::string matrix_name = "Matrix" + type_suffix;
        py::class_<Matrix>(m, matrix_name.c_str(),
                           "Matrix class for linear algebra operations.")
            .def(py::init<>())
            .def(py::init<size_t, size_t>())
            .def("__getitem__",
                 [](const Matrix& m, const std::pair<size_t, size_t>& idx) {
                     if (idx.first >= m.size1() || idx.second >= m.size2())
                         throw py::index_error("Matrix index out of bounds");
                     return m(idx.first, idx.second);
                 })
            .def("__setitem__",
                 [](Matrix& m, const std::pair<size_t, size_t>& idx, T value) {
                     if (idx.first >= m.size1() || idx.second >= m.size2())
                         throw py::index_error("Matrix index out of bounds");
                     m(idx.first, idx.second) = value;
                 })
            .def("__str__", [](const Matrix& m) {
                std::ostringstream oss;
                oss << "Matrix(" << m.size1() << ", " << m.size2() << ")\n";
                for (size_t i = 0; i < m.size1(); ++i) {
                    oss << "[ ";
                    for (size_t j = 0; j < m.size2(); ++j) {
                        oss << m(i, j) << " ";
                    }
                    oss << "]\n";
                }
                return oss.str();
            });

        std::string vector_name = "Vector" + type_suffix;
        py::class_<Vector>(m, vector_name.c_str(),
                           "Vector class for linear algebra operations.")
            .def(py::init<>())
            .def(py::init<size_t>())
            .def("__getitem__",
                 [](const Vector& v, size_t idx) {
                     if (idx >= v.size())
                         throw py::index_error("Vector index out of bounds");
                     return v(idx);
                 })
            .def("__setitem__",
                 [](Vector& v, size_t idx, T value) {
                     if (idx >= v.size())
                         throw py::index_error("Vector index out of bounds");
                     v(idx) = value;
                 })
            .def("__len__", [](const Vector& v) { return v.size(); })
            .def("__str__", [](const Vector& v) {
                std::ostringstream oss;
                oss << "Vector(" << v.size() << ") [ ";
                for (size_t i = 0; i < v.size(); ++i) {
                    oss << v(i) << " ";
                }
                oss << "]";
                return oss.str();
            });

        py::class_<LAClass>(m, class_name.c_str(),
                            R"(Wrapper class for linear algebra operations.

This class provides linear algebra operations such as solving linear systems,
computing determinants, matrix multiplication, and transpose.

Examples:
    >>> from atom.extra.boost import math
    >>> A = math.Matrix(2, 2)
    >>> A[0, 0] = 1.0; A[0, 1] = 2.0; A[1, 0] = 3.0; A[1, 1] = 4.0
    >>> b = math.Vector(2)
    >>> b[0] = 5.0; b[1] = 6.0
    >>> x = math.LinearAlgebra.solve_linear_system(A, b)
    >>> print(x)
    Vector(2) [ -4.0 4.5 ]
)")
            .def_static("solve_linear_system", &LAClass::solveLinearSystem,
                        py::arg("matrix"), py::arg("vector"),
                        R"(Solves a linear system of equations Ax = b.

Args:
    matrix: The matrix A.
    vector: The vector b.

Returns:
    The solution vector x.
)")
            .def_static("determinant", &LAClass::determinant, py::arg("matrix"),
                        R"(Computes the determinant of a matrix.

Args:
    matrix: The input matrix.

Returns:
    The determinant of the matrix.
)")
            .def_static("multiply", &LAClass::multiply, py::arg("matrix1"),
                        py::arg("matrix2"),
                        R"(Multiplies two matrices.

Args:
    matrix1: The first matrix.
    matrix2: The second matrix.

Returns:
    The product of the two matrices.
)")
            .def_static("transpose", &LAClass::transpose, py::arg("matrix"),
                        R"(Computes the transpose of a matrix.

Args:
    matrix: The input matrix.

Returns:
    The transpose of the matrix.
)");
    }

    // ODE Solver class binding
    {
        using ODEClass = atom::extra::boost::ODESolver<T>;
        using State = typename ODEClass::State;
        using SystemFunction = typename ODEClass::SystemFunction;

        std::string class_name = "ODESolver" + type_suffix;

        py::class_<ODEClass>(
            m, class_name.c_str(),
            R"(Wrapper class for solving ordinary differential equations (ODEs).

This class provides methods for solving ODEs such as the 4th order Runge-Kutta method.

Examples:
    >>> from atom.extra.boost import math
    >>> def harmonic_oscillator(y, dydt, t):
    ...     dydt[0] = y[1]
    ...     dydt[1] = -y[0]
    >>> init_state = [1.0, 0.0]
    >>> solution = math.ODESolver.runge_kutta4(harmonic_oscillator, init_state, 0.0, 10.0, 0.01)
    >>> # solution contains the state at each time step
)")
            .def_static(
                "runge_kutta4", &ODEClass::rungeKutta4, py::arg("system"),
                py::arg("initial_state"), py::arg("start_time"),
                py::arg("end_time"), py::arg("step_size"),
                R"(Solves an ODE using the 4th order Runge-Kutta method.

Args:
    system: The system function defining the ODE.
    initial_state: The initial state of the system.
    start_time: The start time.
    end_time: The end time.
    step_size: The step size.

Returns:
    A list of states representing the solution at each time step.
)");
    }

    // Financial Math class binding
    {
        using FinClass = atom::extra::boost::FinancialMath<T>;
        std::string class_name = "FinancialMath" + type_suffix;

        py::class_<FinClass>(
            m, class_name.c_str(),
            R"(Wrapper class for financial mathematics functions.

This class provides financial math functions such as Black-Scholes option pricing,
bond pricing, and implied volatility calculation.

Examples:
    >>> from atom.extra.boost import math
    >>> # Calculate call option price
    >>> math.FinancialMath.black_scholes_call(100.0, 100.0, 0.05, 0.2, 1.0)
    10.450583572185805
)")
            .def_static(
                "black_scholes_call", &FinClass::blackScholesCall,
                py::arg("stock_price"), py::arg("strike_price"),
                py::arg("risk_free_rate"), py::arg("volatility"),
                py::arg("time_to_maturity"),
                R"(Computes the price of a European call option using the Black-Scholes formula.

Args:
    stock_price: The current stock price.
    strike_price: The strike price of the option.
    risk_free_rate: The risk-free interest rate.
    volatility: The volatility of the stock.
    time_to_maturity: The time to maturity of the option.

Returns:
    The price of the European call option.
)")
            .def_static("modified_duration", &FinClass::modifiedDuration,
                        py::arg("yield"), py::arg("coupon_rate"),
                        py::arg("face_value"), py::arg("periods"),
                        R"(Computes the modified duration of a bond.

Args:
    yield: The yield to maturity.
    coupon_rate: The coupon rate of the bond.
    face_value: The face value of the bond.
    periods: The number of periods.

Returns:
    The modified duration of the bond.
)")
            .def_static("bond_price", &FinClass::bondPrice, py::arg("yield"),
                        py::arg("coupon_rate"), py::arg("face_value"),
                        py::arg("periods"),
                        R"(Computes the price of a bond.

Args:
    yield: The yield to maturity.
    coupon_rate: The coupon rate of the bond.
    face_value: The face value of the bond.
    periods: The number of periods.

Returns:
    The price of the bond.
)")
            .def_static("implied_volatility", &FinClass::impliedVolatility,
                        py::arg("market_price"), py::arg("stock_price"),
                        py::arg("strike_price"), py::arg("risk_free_rate"),
                        py::arg("time_to_maturity"),
                        R"(Computes the implied volatility of an option.

Args:
    market_price: The market price of the option.
    stock_price: The current stock price.
    strike_price: The strike price of the option.
    risk_free_rate: The risk-free interest rate.
    time_to_maturity: The time to maturity of the option.

Returns:
    The implied volatility of the option.
)");
    }
}

// Define module-level functions
template <typename T>
void define_functions(py::module& m, const std::string& type_suffix) {
    // Factorial function
    m.def("factorial", &atom::extra::boost::factorial<T>, py::arg("number"),
          R"(Computes the factorial of a number.

Args:
    number: The input number.

Returns:
    The factorial of the number.

Examples:
    >>> from atom.extra.boost import math
    >>> math.factorial(5)
    120
)");

    // Define aliases for easier access to common instantiations
    if (type_suffix == "Float") {
        // Add types to module scope
        m.attr("SpecialFunctions") = m.attr("SpecialFunctionsFloat");
        m.attr("Statistics") = m.attr("StatisticsFloat");
        m.attr("NormalDistribution") = m.attr("NormalDistributionFloat");
        m.attr("StudentTDistribution") = m.attr("StudentTDistributionFloat");
        m.attr("PoissonDistribution") = m.attr("PoissonDistributionFloat");
        m.attr("ExponentialDistribution") =
            m.attr("ExponentialDistributionFloat");
        m.attr("NumericalIntegration") = m.attr("NumericalIntegrationFloat");
        m.attr("Optimization") = m.attr("OptimizationFloat");
        m.attr("LinearAlgebra") = m.attr("LinearAlgebraFloat");
        m.attr("Matrix") = m.attr("MatrixFloat");
        m.attr("Vector") = m.attr("VectorFloat");
        m.attr("ODESolver") = m.attr("ODESolverFloat");
        m.attr("FinancialMath") = m.attr("FinancialMathFloat");
    }
}

PYBIND11_MODULE(math, m) {
    m.doc() = "Boost Math wrapper module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Declare classes with different numeric types
    declare_math_classes<float>(m, "Float");
    declare_math_classes<double>(m, "Double");

    // Define module-level functions
    define_functions<double>(m, "Float");

    // Add version info
    m.attr("__version__") = "1.0.0";
}
