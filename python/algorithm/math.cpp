#include "atom/algorithm/math.hpp"
#include "atom/error/exception.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(math, m) {
    m.doc() = R"pbdoc(
        Extra Math Library
        -----------------

        This module provides additional mathematical functions and utilities
        that complement Python's standard math module. It includes:

        - Safe integer operations with overflow/underflow detection
        - Bit manipulation functions
        - Fast arithmetic operations
        - Parallel vector operations
        - Number theory functions (GCD, LCM, primality tests)
        - Modular arithmetic

        Examples:
            >>> from atom.algorithm.math import gcd64, lcm64, is_prime
            >>> gcd64(24, 36)
            12
            >>> lcm64(4, 6)
            12
            >>> is_prime(17)
            True
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::error::InvalidArgument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::error::OverflowException& e) {
            PyErr_SetString(PyExc_OverflowError, e.what());
        } catch (const atom::error::UnderflowException& e) {
            PyErr_SetString(PyExc_ArithmeticError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        }
    });

    // Basic arithmetic operations
    m.def("mul_div_64", &atom::algorithm::mulDiv64, py::arg("operant"),
          py::arg("multiplier"), py::arg("divider"),
          R"pbdoc(
        Performs a 64-bit multiplication followed by division.

        This function calculates the result of (operant * multiplier) / divider.

        Args:
            operant: The first operand for multiplication
            multiplier: The second operand for multiplication
            divider: The divisor for the division operation

        Returns:
            The result of (operant * multiplier) / divider

        Raises:
            ValueError: If divider is zero

        Example:
            >>> mul_div_64(10, 20, 5)
            40
        )pbdoc");

    m.def("safe_add", &atom::algorithm::safeAdd, py::arg("a"), py::arg("b"),
          R"pbdoc(
        Performs a safe addition operation.

        This function adds two unsigned 64-bit integers, handling potential overflow.

        Args:
            a: The first operand for addition
            b: The second operand for addition

        Returns:
            The result of a + b

        Raises:
            OverflowError: If the operation would overflow

        Example:
            >>> safe_add(42, 13)
            55
        )pbdoc");

    m.def("safe_mul", &atom::algorithm::safeMul, py::arg("a"), py::arg("b"),
          R"pbdoc(
        Performs a safe multiplication operation.

        This function multiplies two unsigned 64-bit integers, handling potential overflow.

        Args:
            a: The first operand for multiplication
            b: The second operand for multiplication

        Returns:
            The result of a * b

        Raises:
            OverflowError: If the operation would overflow

        Example:
            >>> safe_mul(6, 7)
            42
        )pbdoc");

    m.def("safe_sub", &atom::algorithm::safeSub, py::arg("a"), py::arg("b"),
          R"pbdoc(
        Performs a safe subtraction operation.

        This function subtracts two unsigned 64-bit integers, handling potential underflow.

        Args:
            a: The first operand for subtraction
            b: The second operand for subtraction

        Returns:
            The result of a - b

        Raises:
            ArithmeticError: If the operation would underflow

        Example:
            >>> safe_sub(42, 12)
            30
        )pbdoc");

    m.def("safe_div", &atom::algorithm::safeDiv, py::arg("a"), py::arg("b"),
          R"pbdoc(
        Performs a safe division operation.

        This function divides two unsigned 64-bit integers, handling potential division by zero.

        Args:
            a: The numerator for division
            b: The denominator for division

        Returns:
            The result of a / b

        Raises:
            ValueError: If there is a division by zero

        Example:
            >>> safe_div(42, 7)
            6
        )pbdoc");

    // Bit manipulation functions
    m.def("rotl64", &atom::algorithm::rotl64, py::arg("n"), py::arg("c"),
          R"pbdoc(
        Rotates a 64-bit integer to the left.

        This function rotates a 64-bit integer to the left by a specified number of bits.

        Args:
            n: The 64-bit integer to rotate
            c: The number of bits to rotate

        Returns:
            The rotated 64-bit integer

        Example:
            >>> hex(rotl64(0x1234567812345678, 4))
            '0x2345678123456781'
        )pbdoc");

    m.def("rotr64", &atom::algorithm::rotr64, py::arg("n"), py::arg("c"),
          R"pbdoc(
        Rotates a 64-bit integer to the right.

        This function rotates a 64-bit integer to the right by a specified number of bits.

        Args:
            n: The 64-bit integer to rotate
            c: The number of bits to rotate

        Returns:
            The rotated 64-bit integer

        Example:
            >>> hex(rotr64(0x1234567812345678, 4))
            '0x8123456781234567'
        )pbdoc");

    m.def("clz64", &atom::algorithm::clz64, py::arg("x"),
          R"pbdoc(
        Counts the leading zeros in a 64-bit integer.

        This function counts the number of leading zeros in a 64-bit integer.

        Args:
            x: The 64-bit integer to count leading zeros in

        Returns:
            The number of leading zeros in the 64-bit integer

        Example:
            >>> clz64(0x0000000000000001)
            63
            >>> clz64(0x8000000000000000)
            0
        )pbdoc");

    m.def("normalize", &atom::algorithm::normalize, py::arg("x"),
          R"pbdoc(
        Normalizes a 64-bit integer.

        This function normalizes a 64-bit integer by shifting it to the left until
        the most significant bit is set.

        Args:
            x: The 64-bit integer to normalize

        Returns:
            The normalized 64-bit integer

        Example:
            >>> hex(normalize(0x0000000000000123))
            '0x8000000000000000'
        )pbdoc");

    m.def("bit_reverse64", &atom::algorithm::bitReverse64, py::arg("n"),
          R"pbdoc(
        Calculates the bitwise reverse of a 64-bit integer.

        This function calculates the bitwise reverse of a 64-bit integer.

        Args:
            n: The 64-bit integer to reverse

        Returns:
            The bitwise reverse of the 64-bit integer

        Example:
            >>> hex(bit_reverse64(0x0000000000000001))
            '0x8000000000000000'
            >>> hex(bit_reverse64(0x8000000000000000))
            '0x1'
        )pbdoc");

    // Arithmetic functions
    m.def("approximate_sqrt", &atom::algorithm::approximateSqrt, py::arg("n"),
          R"pbdoc(
        Approximates the square root of a 64-bit integer.

        This function approximates the square root of a 64-bit integer using a fast algorithm.

        Args:
            n: The 64-bit integer for which to approximate the square root

        Returns:
            The approximate square root of the 64-bit integer

        Example:
            >>> approximate_sqrt(25)
            5
            >>> approximate_sqrt(100)
            10
        )pbdoc");

    // Number theory functions
    m.def("gcd64", &atom::algorithm::gcd64, py::arg("a"), py::arg("b"),
          R"pbdoc(
        Calculates the greatest common divisor (GCD) of two 64-bit integers.

        This function calculates the greatest common divisor (GCD) of two 64-bit integers.

        Args:
            a: The first 64-bit integer
            b: The second 64-bit integer

        Returns:
            The greatest common divisor of the two 64-bit integers

        Example:
            >>> gcd64(48, 18)
            6
            >>> gcd64(35, 49)
            7
        )pbdoc");

    m.def("lcm64", &atom::algorithm::lcm64, py::arg("a"), py::arg("b"),
          R"pbdoc(
        Calculates the least common multiple (LCM) of two 64-bit integers.

        This function calculates the least common multiple (LCM) of two 64-bit integers.

        Args:
            a: The first 64-bit integer
            b: The second 64-bit integer

        Returns:
            The least common multiple of the two 64-bit integers

        Raises:
            OverflowError: If the operation would overflow

        Example:
            >>> lcm64(4, 6)
            12
            >>> lcm64(8, 12)
            24
        )pbdoc");

    m.def("is_power_of_two", &atom::algorithm::isPowerOfTwo, py::arg("n"),
          R"pbdoc(
        Checks if a 64-bit integer is a power of two.

        This function checks if a 64-bit integer is a power of two.

        Args:
            n: The 64-bit integer to check

        Returns:
            True if the 64-bit integer is a power of two, False otherwise

        Example:
            >>> is_power_of_two(16)
            True
            >>> is_power_of_two(18)
            False
        )pbdoc");

    m.def("next_power_of_two", &atom::algorithm::nextPowerOfTwo, py::arg("n"),
          R"pbdoc(
        Calculates the next power of two for a 64-bit integer.

        This function calculates the next power of two for a 64-bit integer.

        Args:
            n: The 64-bit integer for which to calculate the next power of two

        Returns:
            The next power of two for the 64-bit integer

        Example:
            >>> next_power_of_two(13)
            16
            >>> next_power_of_two(32)
            32
        )pbdoc");

    m.def("is_prime", &atom::algorithm::isPrime, py::arg("n"),
          R"pbdoc(
        Prime number checker using optimized trial division.

        Args:
            n: Number to check

        Returns:
            True if n is prime, False if n is not prime

        Example:
            >>> is_prime(17)
            True
            >>> is_prime(24)
            False
        )pbdoc");

    m.def("generate_primes", &atom::algorithm::generatePrimes, py::arg("limit"),
          R"pbdoc(
        Generates prime numbers up to a limit using the Sieve of Eratosthenes.

        Args:
            limit: Upper limit for prime generation

        Returns:
            List of primes up to limit

        Example:
            >>> generate_primes(20)
            [2, 3, 5, 7, 11, 13, 17, 19]
        )pbdoc");

    m.def("montgomery_multiply", &atom::algorithm::montgomeryMultiply,
          py::arg("a"), py::arg("b"), py::arg("n"),
          R"pbdoc(
        Montgomery modular multiplication.

        Args:
            a: First operand
            b: Second operand
            n: Modulus

        Returns:
            (a * b) mod n

        Example:
            >>> montgomery_multiply(7, 8, 13)
            4
        )pbdoc");

    m.def("mod_pow", &atom::algorithm::modPow, py::arg("base"),
          py::arg("exponent"), py::arg("modulus"),
          R"pbdoc(
        Modular exponentiation using Montgomery reduction.

        Args:
            base: Base value
            exponent: Exponent value
            modulus: Modulus

        Returns:
            (base^exponent) mod modulus

        Example:
            >>> mod_pow(2, 10, 1000)
            24
        )pbdoc");

    // Template specializations for vector operations
    // For int32
    m.def(
        "parallel_vector_add",
        [](py::array_t<int32_t> a, py::array_t<int32_t> b) {
            auto a_info = a.request();
            auto b_info = b.request();

            if (a_info.ndim != 1 || b_info.ndim != 1)
                throw std::runtime_error("Input arrays must be 1-dimensional");

            if (a_info.size != b_info.size)
                throw std::runtime_error(
                    "Input arrays must have the same size");

            std::span<const int32_t> a_span(
                static_cast<const int32_t*>(a_info.ptr), a_info.size);
            std::span<const int32_t> b_span(
                static_cast<const int32_t*>(b_info.ptr), b_info.size);

            auto result = atom::algorithm::parallelVectorAdd(a_span, b_span);

            // Create output array
            auto result_array = py::array_t<int32_t>(a_info.size);
            auto result_buffer = result_array.request();
            std::memcpy(result_buffer.ptr, result.data(),
                        result.size() * sizeof(int32_t));

            return result_array;
        },
        py::arg("a"), py::arg("b"),
        R"pbdoc(
    Parallel addition of two vectors using SIMD (int32).

    Args:
        a: First vector
        b: Second vector

    Returns:
        Result of element-wise addition

    Example:
        >>> import numpy as np
        >>> a = np.array([1, 2, 3, 4], dtype=np.int32)
        >>> b = np.array([5, 6, 7, 8], dtype=np.int32)
        >>> parallel_vector_add(a, b)
        array([6, 8, 10, 12], dtype=int32)
    )pbdoc");

    // For int64
    m.def(
        "parallel_vector_add",
        [](py::array_t<int64_t> a, py::array_t<int64_t> b) {
            auto a_info = a.request();
            auto b_info = b.request();

            if (a_info.ndim != 1 || b_info.ndim != 1)
                throw std::runtime_error("Input arrays must be 1-dimensional");

            if (a_info.size != b_info.size)
                throw std::runtime_error(
                    "Input arrays must have the same size");

            std::span<const int64_t> a_span(
                static_cast<const int64_t*>(a_info.ptr), a_info.size);
            std::span<const int64_t> b_span(
                static_cast<const int64_t*>(b_info.ptr), b_info.size);

            auto result = atom::algorithm::parallelVectorAdd(a_span, b_span);

            // Create output array
            auto result_array = py::array_t<int64_t>(a_info.size);
            auto result_buffer = result_array.request();
            std::memcpy(result_buffer.ptr, result.data(),
                        result.size() * sizeof(int64_t));

            return result_array;
        },
        py::arg("a"), py::arg("b"));

    // For float
    m.def(
        "parallel_vector_add",
        [](py::array_t<float> a, py::array_t<float> b) {
            auto a_info = a.request();
            auto b_info = b.request();

            if (a_info.ndim != 1 || b_info.ndim != 1)
                throw std::runtime_error("Input arrays must be 1-dimensional");

            if (a_info.size != b_info.size)
                throw std::runtime_error(
                    "Input arrays must have the same size");

            std::span<const float> a_span(static_cast<const float*>(a_info.ptr),
                                          a_info.size);
            std::span<const float> b_span(static_cast<const float*>(b_info.ptr),
                                          b_info.size);

            auto result = atom::algorithm::parallelVectorAdd(a_span, b_span);

            // Create output array
            auto result_array = py::array_t<float>(a_info.size);
            auto result_buffer = result_array.request();
            std::memcpy(result_buffer.ptr, result.data(),
                        result.size() * sizeof(float));

            return result_array;
        },
        py::arg("a"), py::arg("b"));

    // For double
    m.def(
        "parallel_vector_add",
        [](py::array_t<double> a, py::array_t<double> b) {
            auto a_info = a.request();
            auto b_info = b.request();

            if (a_info.ndim != 1 || b_info.ndim != 1)
                throw std::runtime_error("Input arrays must be 1-dimensional");

            if (a_info.size != b_info.size)
                throw std::runtime_error(
                    "Input arrays must have the same size");

            std::span<const double> a_span(
                static_cast<const double*>(a_info.ptr), a_info.size);
            std::span<const double> b_span(
                static_cast<const double*>(b_info.ptr), b_info.size);

            auto result = atom::algorithm::parallelVectorAdd(a_span, b_span);

            // Create output array
            auto result_array = py::array_t<double>(a_info.size);
            auto result_buffer = result_array.request();
            std::memcpy(result_buffer.ptr, result.data(),
                        result.size() * sizeof(double));

            return result_array;
        },
        py::arg("a"), py::arg("b"));

    // Parallel vector multiplication - int32
    m.def(
        "parallel_vector_mul",
        [](py::array_t<int32_t> a, py::array_t<int32_t> b) {
            auto a_info = a.request();
            auto b_info = b.request();

            if (a_info.ndim != 1 || b_info.ndim != 1)
                throw std::runtime_error("Input arrays must be 1-dimensional");

            if (a_info.size != b_info.size)
                throw std::runtime_error(
                    "Input arrays must have the same size");

            std::span<const int32_t> a_span(
                static_cast<const int32_t*>(a_info.ptr), a_info.size);
            std::span<const int32_t> b_span(
                static_cast<const int32_t*>(b_info.ptr), b_info.size);

            auto result = atom::algorithm::parallelVectorMul(a_span, b_span);

            // Create output array
            auto result_array = py::array_t<int32_t>(a_info.size);
            auto result_buffer = result_array.request();
            std::memcpy(result_buffer.ptr, result.data(),
                        result.size() * sizeof(int32_t));

            return result_array;
        },
        py::arg("a"), py::arg("b"),
        R"pbdoc(
    Parallel multiplication of two vectors using SIMD (int32).

    Args:
        a: First vector
        b: Second vector

    Returns:
        Result of element-wise multiplication

    Example:
        >>> import numpy as np
        >>> a = np.array([1, 2, 3, 4], dtype=np.int32)
        >>> b = np.array([5, 6, 7, 8], dtype=np.int32)
        >>> parallel_vector_mul(a, b)
        array([5, 12, 21, 32], dtype=int32)
    )pbdoc");

    // For int64
    m.def(
        "parallel_vector_mul",
        [](py::array_t<int64_t> a, py::array_t<int64_t> b) {
            auto a_info = a.request();
            auto b_info = b.request();

            if (a_info.ndim != 1 || b_info.ndim != 1)
                throw std::runtime_error("Input arrays must be 1-dimensional");

            if (a_info.size != b_info.size)
                throw std::runtime_error(
                    "Input arrays must have the same size");

            std::span<const int64_t> a_span(
                static_cast<const int64_t*>(a_info.ptr), a_info.size);
            std::span<const int64_t> b_span(
                static_cast<const int64_t*>(b_info.ptr), b_info.size);

            auto result = atom::algorithm::parallelVectorMul(a_span, b_span);

            // Create output array
            auto result_array = py::array_t<int64_t>(a_info.size);
            auto result_buffer = result_array.request();
            std::memcpy(result_buffer.ptr, result.data(),
                        result.size() * sizeof(int64_t));

            return result_array;
        },
        py::arg("a"), py::arg("b"));

    // For float
    m.def(
        "parallel_vector_mul",
        [](py::array_t<float> a, py::array_t<float> b) {
            auto a_info = a.request();
            auto b_info = b.request();

            if (a_info.ndim != 1 || b_info.ndim != 1)
                throw std::runtime_error("Input arrays must be 1-dimensional");

            if (a_info.size != b_info.size)
                throw std::runtime_error(
                    "Input arrays must have the same size");

            std::span<const float> a_span(static_cast<const float*>(a_info.ptr),
                                          a_info.size);
            std::span<const float> b_span(static_cast<const float*>(b_info.ptr),
                                          b_info.size);

            auto result = atom::algorithm::parallelVectorMul(a_span, b_span);

            // Create output array
            auto result_array = py::array_t<float>(a_info.size);
            auto result_buffer = result_array.request();
            std::memcpy(result_buffer.ptr, result.data(),
                        result.size() * sizeof(float));

            return result_array;
        },
        py::arg("a"), py::arg("b"));

    // For double
    m.def(
        "parallel_vector_mul",
        [](py::array_t<double> a, py::array_t<double> b) {
            auto a_info = a.request();
            auto b_info = b.request();

            if (a_info.ndim != 1 || b_info.ndim != 1)
                throw std::runtime_error("Input arrays must be 1-dimensional");

            if (a_info.size != b_info.size)
                throw std::runtime_error(
                    "Input arrays must have the same size");

            std::span<const double> a_span(
                static_cast<const double*>(a_info.ptr), a_info.size);
            std::span<const double> b_span(
                static_cast<const double*>(b_info.ptr), b_info.size);

            auto result = atom::algorithm::parallelVectorMul(a_span, b_span);

            // Create output array
            auto result_array = py::array_t<double>(a_info.size);
            auto result_buffer = result_array.request();
            std::memcpy(result_buffer.ptr, result.data(),
                        result.size() * sizeof(double));

            return result_array;
        },
        py::arg("a"), py::arg("b"));

    // Bind the fast_pow template for different integer types
    m.def(
        "fast_pow",
        [](int32_t base, int32_t exponent) {
            return atom::algorithm::fastPow(base, exponent);
        },
        py::arg("base"), py::arg("exponent"),
        R"pbdoc(
    Fast exponentiation for 32-bit integers.

    Args:
        base: The base value
        exponent: The exponent value

    Returns:
        The result of base^exponent

    Example:
        >>> fast_pow(2, 10)
        1024
        >>> fast_pow(3, 4)
        81
    )pbdoc");

    m.def(
        "fast_pow",
        [](int64_t base, int64_t exponent) {
            return atom::algorithm::fastPow(base, exponent);
        },
        py::arg("base"), py::arg("exponent"));

    // Add additional utility functions that would be useful in Python

    // Function to compute binomial coefficient efficiently
    m.def(
        "binomial_coefficient",
        [](uint64_t n, uint64_t k) {
            if (k > n)
                return 0ULL;
            if (k == 0 || k == n)
                return 1ULL;

            // Optimize by using symmetry
            if (k > n - k)
                k = n - k;

            uint64_t result = 1;
            for (uint64_t i = 0; i < k; ++i) {
                result = atom::algorithm::safeMul(result, n - i);
                result = atom::algorithm::safeDiv(result, i + 1);
            }

            return result;
        },
        py::arg("n"), py::arg("k"),
        R"pbdoc(
    Computes the binomial coefficient (n choose k).

    Args:
        n: The number of elements
        k: The number of elements to choose

    Returns:
        The binomial coefficient value

    Raises:
        OverflowError: If the computation would overflow

    Example:
        >>> binomial_coefficient(5, 2)
        10
        >>> binomial_coefficient(10, 3)
        120
    )pbdoc");

    // Function to compute factorial efficiently
    m.def(
        "factorial",
        [](uint64_t n) {
            if (n <= 1)
                return 1ULL;

            uint64_t result = 1;
            for (uint64_t i = 2; i <= n; ++i) {
                result = atom::algorithm::safeMul(result, i);
            }

            return result;
        },
        py::arg("n"),
        R"pbdoc(
    Computes the factorial of a number.

    Args:
        n: The input number

    Returns:
        The factorial of the number

    Raises:
        OverflowError: If the computation would overflow

    Example:
        >>> factorial(5)
        120
        >>> factorial(10)
        3628800
    )pbdoc");

    // Function to compute extended GCD
    m.def(
        "extended_gcd",
        [](int64_t a, int64_t b) {
            int64_t s = 0, old_s = 1;
            int64_t t = 1, old_t = 0;
            int64_t r = b, old_r = a;

            while (r != 0) {
                int64_t quotient = old_r / r;

                std::tie(old_r, r) = std::make_tuple(r, old_r - quotient * r);
                std::tie(old_s, s) = std::make_tuple(s, old_s - quotient * s);
                std::tie(old_t, t) = std::make_tuple(t, old_t - quotient * t);
            }

            // Return gcd, x, y such that ax + by = gcd
            return py::make_tuple(old_r, old_s, old_t);
        },
        py::arg("a"), py::arg("b"),
        R"pbdoc(
    Computes the extended GCD of two numbers.

    This function finds the greatest common divisor (gcd) of a and b,
    and coefficients x and y such that ax + by = gcd.

    Args:
        a: First number
        b: Second number

    Returns:
        A tuple (gcd, x, y) such that ax + by = gcd

    Example:
        >>> extended_gcd(35, 15)
        (5, 1, -2)  # 35*1 + 15*(-2) = 5
    )pbdoc");

    // Function to compute the modular multiplicative inverse
    m.def(
        "mod_inverse",
        [&m](int64_t a, int64_t modulus) {
            auto result = m.attr("extended_gcd")(a, modulus).cast<py::tuple>();
            int64_t gcd = result[0].cast<int64_t>();
            int64_t x = result[1].cast<int64_t>();

            if (gcd != 1) {
                throw py::value_error("Modular inverse does not exist");
            }

            int64_t inverse = x;
            return (inverse % modulus + modulus) %
                   modulus;  // Ensure result is positive
        },
        py::arg("a"), py::arg("m"),
        R"pbdoc(
    Computes the modular multiplicative inverse.

    This function finds the modular multiplicative inverse of a with respect to m,
    which is a number x such that ax ≡ 1 (mod m).

    Args:
        a: The number to find the inverse for
        m: The modulus

    Returns:
        The modular multiplicative inverse

    Raises:
        ValueError: If the inverse does not exist

    Example:
        >>> mod_inverse(3, 11)
        4  # Because (3 * 4) % 11 = 1
    )pbdoc");

    // Function to check if a number is a perfect square
    m.def(
        "is_perfect_square",
        [](uint64_t n) {
            uint64_t sqrt_n = atom::algorithm::approximateSqrt(n);
            return sqrt_n * sqrt_n == n;
        },
        py::arg("n"),
        R"pbdoc(
    Checks if a number is a perfect square.

    Args:
        n: The number to check

    Returns:
        True if n is a perfect square, False otherwise

    Example:
        >>> is_perfect_square(16)
        True
        >>> is_perfect_square(17)
        False
    )pbdoc");

    // Function to compute all divisors of a number
    m.def(
        "get_divisors",
        [](uint64_t n) {
            std::vector<uint64_t> divisors;

            for (uint64_t i = 1; i * i <= n; ++i) {
                if (n % i == 0) {
                    divisors.push_back(i);
                    if (i != n / i) {
                        divisors.push_back(n / i);
                    }
                }
            }

            std::sort(divisors.begin(), divisors.end());
            return divisors;
        },
        py::arg("n"),
        R"pbdoc(
    Finds all divisors of a number.

    Args:
        n: The number to find divisors for

    Returns:
        A sorted list of all divisors

    Example:
        >>> get_divisors(12)
        [1, 2, 3, 4, 6, 12]
    )pbdoc");

    // Function to check if a number is a perfect number
    m.def(
        "is_perfect_number",
        [](uint64_t n) {
            if (n <= 1)
                return false;

            uint64_t sum = 1;  // Start with 1 as it's always a divisor

            for (uint64_t i = 2; i * i <= n; ++i) {
                if (n % i == 0) {
                    sum += i;
                    if (i != n / i) {
                        sum += n / i;
                    }
                }
            }

            return sum == n;
        },
        py::arg("n"),
        R"pbdoc(
    Checks if a number is a perfect number.

    A perfect number is a positive integer that is equal to the sum of its proper divisors.

    Args:
        n: The number to check

    Returns:
        True if n is a perfect number, False otherwise

    Example:
        >>> is_perfect_number(6)  # 1 + 2 + 3 = 6
        True
        >>> is_perfect_number(28)  # 1 + 2 + 4 + 7 + 14 = 28
        True
        >>> is_perfect_number(12)
        False
    )pbdoc");

    // Function to compute Euler's totient function (phi)
    m.def(
        "euler_totient",
        [](uint64_t n) {
            if (n == 0)
                return 0ULL;

            uint64_t result = n;  // Initialize result as n

            // Consider all prime factors and subtract their multiples
            for (uint64_t p = 2; p * p <= n; ++p) {
                if (n % p == 0) {
                    // p is a prime factor
                    result -= result / p;

                    // Remove all occurrences of p
                    while (n % p == 0) {
                        n /= p;
                    }
                }
            }

            // If n is a prime greater than the square root
            if (n > 1) {
                result -= result / n;
            }

            return result;
        },
        py::arg("n"),
        R"pbdoc(
    Computes Euler's totient function (phi).

    Euler's totient function counts the positive integers up to n that are relatively prime to n.

    Args:
        n: The input number

    Returns:
        The value of Euler's totient function for n

    Example:
        >>> euler_totient(10)
        4  # 1, 3, 7, 9 are relatively prime to 10
        >>> euler_totient(7)
        6  # 1, 2, 3, 4, 5, 6 are relatively prime to 7 (which is prime)
    )pbdoc");
}
