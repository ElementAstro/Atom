#include "atom/utils/lcg.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(utils, m) {
    m.doc() = "Utility functions and classes for the atom package";

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

    // LCG class binding
    py::class_<atom::utils::LCG>(
        m, "LCG",
        R"(Linear Congruential Generator for pseudo-random number generation.

This class implements a Linear Congruential Generator (LCG) which is a type
of pseudo-random number generator. It provides various methods to generate
random numbers following different distributions.

Args:
    seed: The initial seed value. Defaults to the current time.

Examples:
    >>> from atom.utils import LCG
    >>> lcg = LCG(42)
    >>> lcg.next_int(1, 10)  # Random integer between 1 and 10
    >>> lcg.next_double()  # Random double between 0.0 and 1.0
)")
        .def(py::init<atom::utils::LCG::result_type>(),
             py::arg("seed") = static_cast<atom::utils::LCG::result_type>(
                 std::chrono::steady_clock::now().time_since_epoch().count()),
             "Constructs an LCG with an optional seed.")
        .def("next", &atom::utils::LCG::next,
             "Generates the next random number in the sequence.")
        .def("seed", &atom::utils::LCG::seed, py::arg("new_seed"),
             "Seeds the generator with a new seed value.")
        .def("save_state", &atom::utils::LCG::saveState, py::arg("filename"),
             R"(Saves the current state of the generator to a file.

Args:
    filename: The name of the file to save the state to.

Raises:
    RuntimeError: If the file cannot be opened.
)")
        .def("load_state", &atom::utils::LCG::loadState, py::arg("filename"),
             R"(Loads the state of the generator from a file.

Args:
    filename: The name of the file to load the state from.

Raises:
    RuntimeError: If the file cannot be opened or is corrupt.
)")
        .def("next_int", &atom::utils::LCG::nextInt, py::arg("min") = 0,
             py::arg("max") = std::numeric_limits<int>::max(),
             R"(Generates a random integer within a specified range.

Args:
    min: The minimum value (inclusive). Defaults to 0.
    max: The maximum value (inclusive). Defaults to the maximum value of int.

Returns:
    A random integer within the specified range.

Raises:
    ValueError: If min > max.
)")
        .def("next_double", &atom::utils::LCG::nextDouble, py::arg("min") = 0.0,
             py::arg("max") = 1.0,
             R"(Generates a random double within a specified range.

Args:
    min: The minimum value (inclusive). Defaults to 0.0.
    max: The maximum value (exclusive). Defaults to 1.0.

Returns:
    A random double within the specified range.

Raises:
    ValueError: If min >= max.
)")
        .def(
            "next_bernoulli", &atom::utils::LCG::nextBernoulli,
            py::arg("probability") = 0.5,
            R"(Generates a random boolean value based on a specified probability.

Args:
    probability: The probability of returning true. Defaults to 0.5.

Returns:
    A random boolean value.

Raises:
    ValueError: If probability is not in [0,1].
)")
        .def(
            "next_gaussian", &atom::utils::LCG::nextGaussian,
            py::arg("mean") = 0.0, py::arg("stddev") = 1.0,
            R"(Generates a random number following a Gaussian (normal) distribution.

Args:
    mean: The mean of the distribution. Defaults to 0.0.
    stddev: The standard deviation of the distribution. Defaults to 1.0.

Returns:
    A random number following a Gaussian distribution.

Raises:
    ValueError: If stddev <= 0.
)")
        .def("next_poisson", &atom::utils::LCG::nextPoisson,
             py::arg("lambda") = 1.0,
             R"(Generates a random number following a Poisson distribution.

Args:
    lambda: The rate parameter (lambda) of the distribution. Defaults to 1.0.

Returns:
    A random number following a Poisson distribution.

Raises:
    ValueError: If lambda <= 0.
)")
        .def(
            "next_exponential", &atom::utils::LCG::nextExponential,
            py::arg("lambda") = 1.0,
            R"(Generates a random number following an Exponential distribution.

Args:
    lambda: The rate parameter (lambda) of the distribution. Defaults to 1.0.

Returns:
    A random number following an Exponential distribution.

Raises:
    ValueError: If lambda <= 0.
)")
        .def("next_geometric", &atom::utils::LCG::nextGeometric,
             py::arg("probability") = 0.5,
             R"(Generates a random number following a Geometric distribution.

Args:
    probability: The probability of success in each trial. Defaults to 0.5.

Returns:
    A random number following a Geometric distribution.

Raises:
    ValueError: If probability not in (0,1).
)")
        .def("next_gamma", &atom::utils::LCG::nextGamma, py::arg("shape"),
             py::arg("scale") = 1.0,
             R"(Generates a random number following a Gamma distribution.

Args:
    shape: The shape parameter of the distribution.
    scale: The scale parameter of the distribution. Defaults to 1.0.

Returns:
    A random number following a Gamma distribution.

Raises:
    ValueError: If shape or scale <= 0.
)")
        .def("next_beta", &atom::utils::LCG::nextBeta, py::arg("alpha"),
             py::arg("beta"),
             R"(Generates a random number following a Beta distribution.

Args:
    alpha: The alpha parameter of the distribution.
    beta: The beta parameter of the distribution.

Returns:
    A random number following a Beta distribution.

Raises:
    ValueError: If alpha or beta <= 0.
)")
        .def("next_chi_squared", &atom::utils::LCG::nextChiSquared,
             py::arg("degrees_of_freedom"),
             R"(Generates a random number following a Chi-Squared distribution.

Args:
    degrees_of_freedom: The degrees of freedom of the distribution.

Returns:
    A random number following a Chi-Squared distribution.

Raises:
    ValueError: If degrees_of_freedom <= 0.
)")
        .def(
            "next_hypergeometric", &atom::utils::LCG::nextHypergeometric,
            py::arg("total"), py::arg("success"), py::arg("draws"),
            R"(Generates a random number following a Hypergeometric distribution.

Args:
    total: The total number of items.
    success: The number of successful items.
    draws: The number of draws.

Returns:
    A random number following a Hypergeometric distribution.

Raises:
    ValueError: If parameters are invalid.
)")
        .def("next_discrete",
             py::overload_cast<const std::vector<double>&>(
                 &atom::utils::LCG::nextDiscrete),
             py::arg("weights"),
             R"(Generates a random index based on a discrete distribution.

Args:
    weights: The weights of the discrete distribution.

Returns:
    A random index based on the weights.

Raises:
    ValueError: If weights is empty or contains negative values.
)")
        .def("next_multinomial",
             py::overload_cast<int, const std::vector<double>&>(
                 &atom::utils::LCG::nextMultinomial),
             py::arg("trials"), py::arg("probabilities"),
             R"(Generates a multinomial distribution.

Args:
    trials: The number of trials.
    probabilities: The probabilities of each outcome.

Returns:
    A vector of counts for each outcome.

Raises:
    ValueError: If probabilities is invalid.
)")
        .def(
            "shuffle",
            [](atom::utils::LCG& lcg, py::list list) {
                // Convert Python list to a vector
                std::vector<py::object> vec(list.size());
                for (size_t i = 0; i < list.size(); ++i) {
                    vec[i] = list[i];
                }

                // Shuffle the vector
                lcg.shuffle(vec);

                // Convert back to Python list
                py::list result;
                for (const auto& obj : vec) {
                    result.append(obj);
                }
                return result;
            },
            py::arg("data"),
            R"(Shuffles a list of data.

Args:
    data: The list of data to shuffle.

Returns:
    A new shuffled list.
)")
        .def(
            "sample",
            [](atom::utils::LCG& lcg, py::list data, int sample_size) {
                // Convert Python list to a vector
                std::vector<py::object> vec(data.size());
                for (size_t i = 0; i < data.size(); ++i) {
                    vec[i] = data[i];
                }

                // Sample from the vector
                if (sample_size > static_cast<int>(vec.size())) {
                    throw std::invalid_argument(
                        "Sample size cannot be greater than the size of the "
                        "input data");
                }

                // Use the sample implementation and convert back to Python list
                auto sampled = lcg.sample(vec, sample_size);
                py::list result;
                for (const auto& obj : sampled) {
                    result.append(obj);
                }
                return result;
            },
            py::arg("data"), py::arg("sample_size"),
            R"(Samples a subset of data from a list.

Args:
    data: The list of data to sample from.
    sample_size: The number of elements to sample.

Returns:
    A list containing the sampled elements.

Raises:
    ValueError: If sample_size > len(data).
)");

    // Add Python-specific aliases for better Pythonic API
    m.attr("LCG").attr("next_int") = m.attr("LCG").attr("next_int");
    m.attr("LCG").attr("next_float") = m.attr("LCG").attr("next_double");
    m.attr("LCG").attr("random") = m.attr("LCG").attr("next_double");
    m.attr("LCG").attr("random_int") = m.attr("LCG").attr("next_int");
    m.attr("LCG").attr("randint") = m.attr("LCG").attr("next_int");
    m.attr("LCG").attr("choice") = m.attr("LCG").attr("next_discrete");
}
