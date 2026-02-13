#include "atom/utils/random/random.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <random>

namespace py = pybind11;

PYBIND11_MODULE(random, m) {
    m.doc() = R"pbdoc(
        Random Number Generation Module
        --------------------------------

        This module provides random number generation utilities:
        - Random number generators with various distributions
        - Random string generation
        - Secure shuffling

        Examples:
            >>> from atom.utils import random
            >>> # Generate random integers
            >>> rng = random.RandomInt(1, 100)
            >>> value = rng.generate()
            >>> values = rng.vector(10)
            >>>
            >>> # Generate random string
            >>> s = random.generate_random_string(16)
    )pbdoc";

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

    // Random integer generator
    using RandomInt =
        atom::utils::Random<std::mt19937, std::uniform_int_distribution<int>>;
    py::class_<RandomInt>(m, "RandomInt",
                          "Random integer generator with uniform distribution")
        .def(py::init<int, int>(), py::arg("min"), py::arg("max"),
             "Create random integer generator with min and max values")
        .def("generate", &RandomInt::operator(),
             "Generate a single random integer")
        .def("vector", &RandomInt::vector, py::arg("count"),
             "Generate a vector of random integers")
        .def("seed", &RandomInt::seed,
             py::arg("value") = std::random_device{}(),
             "Re-seed the random number generator")
        .def_static("range", &RandomInt::range, py::arg("count"),
                    py::arg("min"), py::arg("max"),
                    "Generate a range of random integers");

    // Random double generator
    using RandomDouble =
        atom::utils::Random<std::mt19937,
                            std::uniform_real_distribution<double>>;
    py::class_<RandomDouble>(
        m, "RandomDouble", "Random double generator with uniform distribution")
        .def(py::init<double, double>(), py::arg("min"), py::arg("max"),
             "Create random double generator with min and max values")
        .def("generate", &RandomDouble::operator(),
             "Generate a single random double")
        .def("vector", &RandomDouble::vector, py::arg("count"),
             "Generate a vector of random doubles")
        .def("seed", &RandomDouble::seed,
             py::arg("value") = std::random_device{}(),
             "Re-seed the random number generator")
        .def_static("range", &RandomDouble::range, py::arg("count"),
                    py::arg("min"), py::arg("max"),
                    "Generate a range of random doubles");

    // Random float generator
    using RandomFloat =
        atom::utils::Random<std::mt19937,
                            std::uniform_real_distribution<float>>;
    py::class_<RandomFloat>(m, "RandomFloat",
                            "Random float generator with uniform distribution")
        .def(py::init<float, float>(), py::arg("min"), py::arg("max"),
             "Create random float generator with min and max values")
        .def("generate", &RandomFloat::operator(),
             "Generate a single random float")
        .def("vector", &RandomFloat::vector, py::arg("count"),
             "Generate a vector of random floats")
        .def("seed", &RandomFloat::seed,
             py::arg("value") = std::random_device{}(),
             "Re-seed the random number generator")
        .def_static("range", &RandomFloat::range, py::arg("count"),
                    py::arg("min"), py::arg("max"),
                    "Generate a range of random floats");

    // Random string generation
    m.def("generate_random_string", &atom::utils::generateRandomString,
          py::arg("length"), py::arg("charset") = "", py::arg("secure") = false,
          R"(Generate a random string of specified length.

Args:
    length: Length of the string to generate
    charset: Optional character set to use (defaults to alphanumeric)
    secure: Whether to use secure random generation

Returns:
    Random string

Examples:
    >>> s = generate_random_string(16)
    >>> s = generate_random_string(8, "0123456789")
    >>> s = generate_random_string(32, secure=True)
)");

    // Secure shuffle
    m.def(
        "secure_shuffle",
        [](std::vector<py::object>& container) {
            std::random_device rd;
            std::mt19937_64 g(rd());
            std::ranges::shuffle(container, g);
        },
        py::arg("container"),
        R"(Shuffle elements in a list using a secure random generator.

Args:
    container: List to shuffle (modified in-place)

Examples:
    >>> items = [1, 2, 3, 4, 5]
    >>> secure_shuffle(items)
)");
}
