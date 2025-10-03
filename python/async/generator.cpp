#include "atom/async/generator.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Template for declaring Generator with different types
template <typename T>
void declare_generator(py::module& m, const std::string& type_name) {
    std::string class_name = "Generator" + type_name;

    py::class_<atom::async::Generator<T>>(
        m, class_name.c_str(),
        R"(A generator class using C++20 coroutines.

This generator provides a convenient way to create and use coroutines that
yield values, similar to Python generators.

Examples:
    >>> from atom.async import generator
    >>>
    >>> # Create a generator from a range
    >>> g = generator.range_int(0, 5)
    >>>
    >>> # Iterate through the generator
    >>> for value in g:
    >>>     print(value)
    >>>
    >>> # Create a generator from a list
    >>> items = [1, 2, 3, 4, 5]
    >>> g = generator.from_range_int(items)
    >>> for value in g:
    >>>     print(value)
)")
        .def("__iter__",
             [](atom::async::Generator<T>& self) { return std::ref(self); })
        .def("__next__", [](atom::async::Generator<T>& self) {
            auto it = self.begin();
            if (it == self.end()) {
                throw py::stop_iteration();
            }
            T value = *it;
            ++it;  // Advance the generator
            return value;
        });
}

// Template for declaring TwoWayGenerator with different types
template <typename Yield, typename Receive = void>
void declare_two_way_generator(py::module& m, const std::string& type_name) {
    std::string class_name = "TwoWayGenerator" + type_name;

    py::class_<atom::async::TwoWayGenerator<Yield, Receive>>(
        m, class_name.c_str(),
        R"(A generator that can also receive values from the caller.

This advanced generator allows bi-directional communication, where the generator
yields values and can also receive values from the caller.

Examples:
    >>> from atom.async import generator
    >>>
    >>> # Create a two-way generator
    >>> g = generator.create_echo_generator_int()
    >>>
    >>> # Send values and get responses
    >>> value = g.next(42)  # Send 42, get 42 back
    >>> print(value)
    >>>
    >>> # Check if generator is done
    >>> print(g.done())
)")
        .def("next", &atom::async::TwoWayGenerator<Yield, Receive>::next,
             py::arg("value") = Receive{},
             R"(Advances the generator and returns the next value.

Args:
    value: Value to send to the generator

Returns:
    The yielded value

Raises:
    RuntimeError: If the generator is done
)")
        .def("done", &atom::async::TwoWayGenerator<Yield, Receive>::done,
             "Checks if the generator is done");
}

// Specialization for TwoWayGenerator with void receive type
template <typename Yield>
void declare_two_way_generator_void_receive(py::module& m,
                                            const std::string& type_name) {
    std::string class_name = "TwoWayGenerator" + type_name;

    py::class_<atom::async::TwoWayGenerator<Yield, void>>(
        m, class_name.c_str(),
        R"(A generator that yields values but doesn't receive input.

This generator yields values to the caller but doesn't receive input.

Examples:
    >>> from atom.async import generator
    >>>
    >>> # Create a one-way generator
    >>> g = generator.create_counter_generator_int(10)
    >>>
    >>> # Get values
    >>> for i in range(5):
    >>>     value = g.next()
    >>>     print(value)
)")
        .def("next", &atom::async::TwoWayGenerator<Yield, void>::next,
             R"(Advances the generator and returns the next value.

Returns:
    The yielded value

Raises:
    RuntimeError: If the generator is done
)")
        .def("done", &atom::async::TwoWayGenerator<Yield, void>::done,
             "Checks if the generator is done");
}

// Helper functions to create generators

// Create a from_range generator
template <typename T>
atom::async::Generator<T> create_from_range(const std::vector<T>& range) {
    return atom::async::from_range(range);
}

// Create a range generator
template <typename T>
atom::async::Generator<T> create_range(T begin, T end, T step = T{1}) {
    return atom::async::range<T>(begin, end, step);
}

// Create an infinite range generator
template <typename T>
atom::async::Generator<T> create_infinite_range(T start = T{}, T step = T{1}) {
    return atom::async::infinite_range<T>(start, step);
}

// Example echo generator
atom::async::TwoWayGenerator<int, int> create_echo_generator_int() {
    int value;
    while (true) {
        value = co_yield value;
    }
}

// Example counter generator
atom::async::TwoWayGenerator<int, void> create_counter_generator_int(
    int limit) {
    for (int i = 0; i < limit; i++) {
        co_yield i;
    }
}

PYBIND11_MODULE(generator, m) {
    m.doc() = R"pbdoc(
        C++20 Coroutine-based Generator
        ------------------------------

        This module provides Python bindings for C++20 coroutine-based generators,
        allowing for efficient, lazy evaluation of sequences and bi-directional
        communication with coroutines.

        The module includes:
          - Standard generators that yield values in a sequence
          - Two-way generators that can both yield and receive values
          - Utility functions to create generators from ranges, sequences, etc.
          - Support for infinite generators with safe iteration

        Example:
            >>> from atom.async import generator
            >>>
            >>> # Create a range generator
            >>> g = generator.range_int(0, 5)
            >>>
            >>> # Iterate through all values
            >>> for i in g:
            >>>     print(i)  # Prints 0, 1, 2, 3, 4
            >>>
            >>> # Create an infinite generator (use with caution!)
            >>> inf_gen = generator.infinite_range_int(1, 2)
            >>>
            >>> # Take only the first few values from infinite generator
            >>> for i, value in enumerate(inf_gen):
            >>>     print(value)
            >>>     if i >= 4:
            >>>         break  # Stop after 5 values
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::logic_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Declare generators for various types
    declare_generator<int>(m, "Int");
    declare_generator<float>(m, "Float");
    declare_generator<double>(m, "Double");
    declare_generator<std::string>(m, "String");

    // Declare two-way generators
    declare_two_way_generator<int, int>(m, "IntInt");
    declare_two_way_generator<std::string, std::string>(m, "StringString");

    // Declare one-way generators
    declare_two_way_generator_void_receive<int>(m, "IntVoid");
    declare_two_way_generator_void_receive<float>(m, "FloatVoid");
    declare_two_way_generator_void_receive<std::string>(m, "StringVoid");

    // Functions to create generators from ranges
    m.def(
        "from_range_int", &create_from_range<int>, py::arg("range"),
        R"(Creates a generator that yields each element in a list of integers.

Args:
    range: The list of integers to yield elements from

Returns:
    A generator that yields elements from the list

Examples:
    >>> g = from_range_int([1, 2, 3, 4, 5])
    >>> for value in g:
    >>>     print(value)  # Prints 1, 2, 3, 4, 5
)");

    m.def("from_range_float", &create_from_range<float>, py::arg("range"),
          "Creates a generator that yields each element in a list of floats.");

    m.def("from_range_double", &create_from_range<double>, py::arg("range"),
          "Creates a generator that yields each element in a list of doubles.");

    m.def("from_range_string", &create_from_range<std::string>,
          py::arg("range"),
          "Creates a generator that yields each element in a list of strings.");

    // Functions to create range generators
    m.def("range_int", &create_range<int>, py::arg("begin"), py::arg("end"),
          py::arg("step") = 1,
          R"(Creates a generator that yields integers from begin to end.

Args:
    begin: The first element to yield
    end: One past the last element to yield
    step: The step between elements (default: 1)

Returns:
    A generator that yields integers from begin to end

Examples:
    >>> g = range_int(0, 5)
    >>> list(g)  # [0, 1, 2, 3, 4]

    >>> g = range_int(0, 10, 2)
    >>> list(g)  # [0, 2, 4, 6, 8]
)");

    m.def("range_float", &create_range<float>, py::arg("begin"), py::arg("end"),
          py::arg("step") = 1.0f,
          "Creates a generator that yields floats from begin to end.");

    m.def("range_double", &create_range<double>, py::arg("begin"),
          py::arg("end"), py::arg("step") = 1.0,
          "Creates a generator that yields doubles from begin to end.");

    // Functions to create infinite range generators
    m.def("infinite_range_int", &create_infinite_range<int>,
          py::arg("start") = 0, py::arg("step") = 1,
          R"(Creates a generator that yields integers infinitely.

Args:
    start: The starting element (default: 0)
    step: The step between elements (default: 1)

Returns:
    A generator that yields integers infinitely

Examples:
    >>> g = infinite_range_int()
    >>> # Be careful! This is an infinite generator
    >>> # You must break out of loops manually
    >>> for i, value in enumerate(g):
    >>>     print(value)
    >>>     if i >= 9:
    >>>         break  # Stop after 10 values
)");

    m.def("infinite_range_float", &create_infinite_range<float>,
          py::arg("start") = 0.0f, py::arg("step") = 1.0f,
          "Creates a generator that yields floats infinitely.");

    m.def("infinite_range_double", &create_infinite_range<double>,
          py::arg("start") = 0.0, py::arg("step") = 1.0,
          "Creates a generator that yields doubles infinitely.");

    // Example generators
    m.def(
        "create_echo_generator_int", &create_echo_generator_int,
        R"(Creates a two-way generator that echoes back whatever is sent to it.

Returns:
    A two-way generator that echoes values

Examples:
    >>> g = create_echo_generator_int()
    >>> g.next(42)  # Returns 42
    >>> g.next(100)  # Returns 100
)");

    m.def("create_counter_generator_int", &create_counter_generator_int,
          py::arg("limit"),
          R"(Creates a generator that counts from 0 to limit-1.

Args:
    limit: The upper bound (exclusive) for counting

Returns:
    A generator that yields sequential integers

Examples:
    >>> g = create_counter_generator_int(5)
    >>> g.next()  # Returns 0
    >>> g.next()  # Returns 1
    >>> list(g)  # Get remaining values: [2, 3, 4]
)");

    // Utility functions for generator management and testing
    m.def(
        "benchmark_generator_performance",
        [](size_t num_elements) -> py::dict {
            using namespace std::chrono;

            py::dict results;

            // Benchmark range generator
            auto start = high_resolution_clock::now();

            auto gen = create_range<int>(0, static_cast<int>(num_elements));
            size_t count = 0;
            for (auto value : gen) {
                count++;
                // Simulate some work
                volatile int dummy = value;
                (void)dummy;
            }

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            // Calculate statistics
            double total_time_us = duration.count();
            double elements_per_second = (num_elements * 1000000.0) / total_time_us;
            double avg_time_per_element = total_time_us / num_elements;

            results[py::str("num_elements")] = num_elements;
            results[py::str("total_time_us")] = total_time_us;
            results[py::str("elements_per_second")] = elements_per_second;
            results[py::str("avg_time_per_element_us")] = avg_time_per_element;
            results[py::str("elements_processed")] = count;

            return results;
        },
        py::arg("num_elements") = 100000,
        R"pbdoc(
        Benchmark generator performance with a range generator.

        Args:
            num_elements: Number of elements to generate (default: 100,000)

        Returns:
            dict: Benchmark results with timing and throughput metrics

        Examples:
            >>> results = benchmark_generator_performance(50000)
            >>> print(f"Elements per second: {results['elements_per_second']:.2f}")
            >>> print(f"Avg time per element: {results['avg_time_per_element_us']:.2f} μs")
        )pbdoc")

    .def(
        "create_fibonacci_generator",
        [](int limit) -> atom::async::TwoWayGenerator<int, void> {
            int a = 0, b = 1;
            for (int i = 0; i < limit; ++i) {
                if (i == 0) {
                    co_yield a;
                } else if (i == 1) {
                    co_yield b;
                } else {
                    int next = a + b;
                    a = b;
                    b = next;
                    co_yield next;
                }
            }
        },
        py::arg("limit"),
        R"pbdoc(
        Create a generator that yields Fibonacci numbers.

        Args:
            limit: Number of Fibonacci numbers to generate

        Returns:
            A generator that yields Fibonacci sequence

        Examples:
            >>> fib_gen = create_fibonacci_generator(10)
            >>> for i in range(10):
            ...     print(fib_gen.next())  # Prints first 10 Fibonacci numbers
        )pbdoc")

    .def(
        "create_prime_generator",
        [](int limit) -> atom::async::TwoWayGenerator<int, void> {
            auto is_prime = [](int n) {
                if (n < 2) return false;
                for (int i = 2; i * i <= n; ++i) {
                    if (n % i == 0) return false;
                }
                return true;
            };

            int count = 0;
            int num = 2;
            while (count < limit) {
                if (is_prime(num)) {
                    co_yield num;
                    count++;
                }
                num++;
            }
        },
        py::arg("limit"),
        R"pbdoc(
        Create a generator that yields prime numbers.

        Args:
            limit: Number of prime numbers to generate

        Returns:
            A generator that yields prime numbers

        Examples:
            >>> prime_gen = create_prime_generator(5)
            >>> for i in range(5):
            ...     print(prime_gen.next())  # Prints first 5 primes: 2, 3, 5, 7, 11
        )pbdoc")

    .def(
        "create_transform_generator",
        [](const std::vector<int>& data, py::function transform_func) -> atom::async::TwoWayGenerator<py::object, void> {
            for (const auto& item : data) {
                py::gil_scoped_acquire acquire;
                try {
                    auto result = transform_func(item);
                    co_yield result;
                } catch (const py::error_already_set& e) {
                    // Skip items that cause transformation errors
                    continue;
                }
            }
        },
        py::arg("data"), py::arg("transform_func"),
        R"pbdoc(
        Create a generator that applies a transformation function to each element.

        Args:
            data: List of integers to transform
            transform_func: Function to apply to each element

        Returns:
            A generator that yields transformed values

        Examples:
            >>> data = [1, 2, 3, 4, 5]
            >>> transform_gen = create_transform_generator(data, lambda x: x * x)
            >>> for i in range(len(data)):
            ...     print(transform_gen.next())  # Prints 1, 4, 9, 16, 25
        )pbdoc")

    .def(
        "create_filter_generator",
        [](const std::vector<int>& data, py::function filter_func) -> atom::async::TwoWayGenerator<int, void> {
            for (const auto& item : data) {
                py::gil_scoped_acquire acquire;
                try {
                    if (filter_func(item).cast<bool>()) {
                        co_yield item;
                    }
                } catch (const py::error_already_set& e) {
                    // Skip items that cause filter errors
                    continue;
                }
            }
        },
        py::arg("data"), py::arg("filter_func"),
        R"pbdoc(
        Create a generator that filters elements based on a predicate function.

        Args:
            data: List of integers to filter
            filter_func: Predicate function that returns True for items to keep

        Returns:
            A generator that yields filtered values

        Examples:
            >>> data = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
            >>> even_gen = create_filter_generator(data, lambda x: x % 2 == 0)
            >>> while not even_gen.done():
            ...     print(even_gen.next())  # Prints 2, 4, 6, 8, 10
        )pbdoc");

    // Add version and feature information
    m.attr("__version__") = "1.0.0";

    // Feature detection
    m.attr("HAS_COROUTINE_SUPPORT") = true;
    m.attr("HAS_TWO_WAY_GENERATORS") = true;
    m.attr("HAS_INFINITE_GENERATORS") = true;

#ifdef ATOM_USE_BOOST_LOCKFREE
    m.attr("HAS_BOOST_LOCKFREE") = true;
#else
    m.attr("HAS_BOOST_LOCKFREE") = false;
#endif

#ifdef ATOM_USE_ASIO
    m.attr("HAS_ASIO") = true;
#else
    m.attr("HAS_ASIO") = false;
#endif

    // Platform information
#ifdef ATOM_PLATFORM_WINDOWS
    m.attr("PLATFORM") = "Windows";
#elif defined(ATOM_PLATFORM_APPLE)
    m.attr("PLATFORM") = "macOS";
#elif defined(ATOM_PLATFORM_LINUX)
    m.attr("PLATFORM") = "Linux";
#else
    m.attr("PLATFORM") = "Unknown";
#endif
}
