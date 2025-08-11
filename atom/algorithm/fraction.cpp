/*
 * fraction.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-28

Description: Implementation of Fraction class

**************************************************/

#include "fraction.hpp"

#include <cmath>
#include <sstream>

// Check if SSE4.1 or higher is supported
#if defined(__SSE4_1__) || defined(__AVX__) || defined(__AVX2__)
#include <immintrin.h>
#define ATOM_FRACTION_USE_SIMD
#endif

namespace atom::algorithm {
/* ------------------------ Arithmetic Operators ------------------------ */

auto Fraction::operator+=(const Fraction& other) -> Fraction& {
    try {
        if (other.numerator == 0)
            return *this;
        if (numerator == 0) {
            numerator = other.numerator;
            denominator = other.denominator;
            return *this;
        }

        long long commonDenominator =
            static_cast<long long>(denominator) * other.denominator;
        long long newNumerator =
            static_cast<long long>(numerator) * other.denominator +
            static_cast<long long>(other.numerator) * denominator;

        // Check for overflow
        if (newNumerator > std::numeric_limits<int>::max() ||
            newNumerator < std::numeric_limits<int>::min() ||
            commonDenominator > std::numeric_limits<int>::max() ||
            commonDenominator < std::numeric_limits<int>::min()) {
            throw FractionException("Integer overflow during addition.");
        }

        numerator = static_cast<int>(newNumerator);
        denominator = static_cast<int>(commonDenominator);
        reduce();
    } catch (const std::exception& e) {
        throw FractionException(std::string("Error in operator+=: ") +
                                e.what());
    }
    return *this;
}

auto Fraction::operator-=(const Fraction& other) -> Fraction& {
    try {
        // Fast path: if the subtrahend is 0, do nothing
        if (other.numerator == 0)
            return *this;

        // Use safe long long calculations to prevent overflow
        long long commonDenominator =
            static_cast<long long>(denominator) * other.denominator;
        long long newNumerator =
            static_cast<long long>(numerator) * other.denominator -
            static_cast<long long>(other.numerator) * denominator;

        // Check for overflow
        if (newNumerator > std::numeric_limits<int>::max() ||
            newNumerator < std::numeric_limits<int>::min() ||
            commonDenominator > std::numeric_limits<int>::max() ||
            commonDenominator < std::numeric_limits<int>::min()) {
            throw FractionException("Integer overflow during subtraction.");
        }

        numerator = static_cast<int>(newNumerator);
        denominator = static_cast<int>(commonDenominator);
        reduce();
    } catch (const std::exception& e) {
        throw FractionException(std::string("Error in operator-=: ") +
                                e.what());
    }
    return *this;
}

auto Fraction::operator*=(const Fraction& other) -> Fraction& {
    try {
        // Fast path: if the multiplier is 0, the result is 0
        if (other.numerator == 0 || numerator == 0) {
            numerator = 0;
            denominator = 1;
            return *this;
        }

        // Pre-calculate gcd to maximize reduction effect
        int gcd1 = gcd(numerator, other.denominator);
        int gcd2 = gcd(denominator, other.numerator);

        // Pre-reduction can reduce overflow risk
        long long n = (static_cast<long long>(numerator) / gcd1) *
                      (static_cast<long long>(other.numerator) / gcd2);
        long long d = (static_cast<long long>(denominator) / gcd2) *
                      (static_cast<long long>(other.denominator) / gcd1);

        // Check for overflow
        if (n > std::numeric_limits<int>::max() ||
            n < std::numeric_limits<int>::min() ||
            d > std::numeric_limits<int>::max() ||
            d < std::numeric_limits<int>::min()) {
            throw FractionException("Integer overflow during multiplication.");
        }

        numerator = static_cast<int>(n);
        denominator = static_cast<int>(d);
        // Reduce again to ensure simplest form
        reduce();
    } catch (const std::exception& e) {
        throw FractionException(std::string("Error in operator*=: ") +
                                e.what());
    }
    return *this;
}

auto Fraction::operator/=(const Fraction& other) -> Fraction& {
    try {
        if (other.numerator == 0) {
            throw FractionException("Division by zero.");
        }

        // Pre-calculate gcd to maximize reduction effect
        int gcd1 = gcd(numerator, other.numerator);
        int gcd2 = gcd(denominator, other.denominator);

        // Pre-reduction can reduce overflow risk
        long long n = (static_cast<long long>(numerator) / gcd1) *
                      (static_cast<long long>(other.denominator) / gcd2);
        long long d = (static_cast<long long>(denominator) / gcd2) *
                      (static_cast<long long>(other.numerator) / gcd1);

        // Ensure denominator is not zero
        if (d == 0) {
            throw FractionException(
                "Denominator cannot be zero after division.");
        }

        // Check for overflow
        if (n > std::numeric_limits<int>::max() ||
            n < std::numeric_limits<int>::min() ||
            d > std::numeric_limits<int>::max() ||
            d < std::numeric_limits<int>::min()) {
            throw FractionException("Integer overflow during division.");
        }

        numerator = static_cast<int>(n);
        denominator = static_cast<int>(d);
        // Ensure denominator is positive
        if (denominator < 0) {
            numerator = -numerator;
            denominator = -denominator;
        }
        // Reduce again to ensure simplest form
        reduce();
    } catch (const std::exception& e) {
        throw FractionException(std::string("Error in operator/=: ") +
                                e.what());
    }
    return *this;
}

/* ------------------------ Arithmetic Operators (Non-Member)
 * ------------------------ */

auto Fraction::operator+(const Fraction& other) const -> Fraction {
    Fraction result(*this);
    result += other;
    return result;
}

auto Fraction::operator-(const Fraction& other) const -> Fraction {
    Fraction result(*this);
    result -= other;
    return result;
}

auto Fraction::operator*(const Fraction& other) const -> Fraction {
    Fraction result(*this);
    result *= other;
    return result;
}

auto Fraction::operator/(const Fraction& other) const -> Fraction {
    Fraction result(*this);
    result /= other;
    return result;
}

/* ------------------------ Comparison Operators ------------------------ */

#if __cplusplus >= 202002L
auto Fraction::operator<=>(const Fraction& other) const
    -> std::strong_ordering {
    // Use cross-multiplication to compare fractions, avoiding overflow
    long long lhs = static_cast<long long>(numerator) * other.denominator;
    long long rhs = static_cast<long long>(other.numerator) * denominator;
    if (lhs < rhs) {
        return std::strong_ordering::less;
    }
    if (lhs > rhs) {
        return std::strong_ordering::greater;
    }
    return std::strong_ordering::equal;
}
#else
bool Fraction::operator<(const Fraction& other) const noexcept {
    // Use cross-multiplication for comparison, avoiding division
    return static_cast<long long>(numerator) * other.denominator <
           static_cast<long long>(other.numerator) * denominator;
}

bool Fraction::operator<=(const Fraction& other) const noexcept {
    return static_cast<long long>(numerator) * other.denominator <=
           static_cast<long long>(other.numerator) * denominator;
}

bool Fraction::operator>(const Fraction& other) const noexcept {
    return static_cast<long long>(numerator) * other.denominator >
           static_cast<long long>(other.numerator) * denominator;
}

bool Fraction::operator>=(const Fraction& other) const noexcept {
    return static_cast<long long>(numerator) * other.denominator >=
           static_cast<long long>(other.numerator) * denominator;
}
#endif

bool Fraction::operator==(const Fraction& other) const noexcept {
#if __cplusplus >= 202002L
    return (*this <=> other) == std::strong_ordering::equal;
#else
    // Since we always reduce fractions to their simplest form,
    // we can directly compare numerators and denominators.
    return (numerator == other.numerator) && (denominator == other.denominator);
#endif
}

/* ------------------------ Utility Methods ------------------------ */

auto Fraction::toString() const -> std::string {
    std::ostringstream oss;
    oss << numerator << '/' << denominator;
    return oss.str();
}

auto Fraction::invert() -> Fraction& {
    if (numerator == 0) {
        throw FractionException(
            "Cannot invert a fraction with numerator zero.");
    }
    std::swap(numerator, denominator);
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
    return *this;
}

std::optional<Fraction> Fraction::pow(int exponent) const noexcept {
    try {
        // Handle special cases
        if (exponent == 0) {
            // Any number to the power of 0 is 1
            return Fraction(1, 1);
        }

        if (exponent == 1) {
            // Power of 1 is itself
            return *this;
        }

        if (numerator == 0) {
            // 0 to any positive power is 0, negative power is invalid
            return exponent > 0 ? std::optional<Fraction>(Fraction(0, 1))
                                : std::nullopt;
        }

        // Handle negative exponent
        bool isNegativeExponent = exponent < 0;
        exponent = std::abs(exponent);

        // Calculate power
        long long resultNumerator = 1;
        long long resultDenominator = 1;

        long long n = numerator;
        long long d = denominator;

        // Use exponentiation by squaring (or simple iteration for now)
        for (int i = 0; i < exponent; i++) {
            resultNumerator *= n;
            resultDenominator *= d;

            // Check for overflow
            if (resultNumerator > std::numeric_limits<int>::max() ||
                resultNumerator < std::numeric_limits<int>::min() ||
                resultDenominator > std::numeric_limits<int>::max() ||
                resultDenominator < std::numeric_limits<int>::min()) {
                return std::nullopt;  // Overflow, return empty
            }
        }

        // If negative exponent, swap numerator and denominator
        if (isNegativeExponent) {
            if (resultNumerator == 0) {
                return std::nullopt;  // Cannot take negative power, denominator
                                      // would be 0
            }
            std::swap(resultNumerator, resultDenominator);
        }

        // If denominator is negative, adjust signs
        if (resultDenominator < 0) {
            resultNumerator = -resultNumerator;
            resultDenominator = -resultDenominator;
        }

        Fraction result(static_cast<int>(resultNumerator),
                        static_cast<int>(resultDenominator));
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<Fraction> Fraction::fromString(std::string_view str) noexcept {
    try {
        std::size_t pos = str.find('/');
        if (pos == std::string_view::npos) {
            // Try to parse the whole string as an integer
            int value = std::stoi(std::string(str));
            return Fraction(value, 1);
        } else {
            // Parse numerator and denominator
            std::string numeratorStr(str.substr(0, pos));
            std::string denominatorStr(str.substr(pos + 1));

            int n = std::stoi(numeratorStr);
            int d = std::stoi(denominatorStr);

            if (d == 0) {
                return std::nullopt;  // Denominator cannot be zero
            }

            return Fraction(n, d);
        }
    } catch (...) {
        return std::nullopt;  // Parsing failed or other exception
    }
}

/* ------------------------ Friend Functions ------------------------ */

auto operator<<(std::ostream& os, const Fraction& f) -> std::ostream& {
    os << f.toString();
    return os;
}

auto operator>>(std::istream& is, Fraction& f) -> std::istream& {
    int n = 0, d = 1;
    char sep = '\0';

    // First, try to read the numerator
    if (!(is >> n)) {
        is.setstate(std::ios::failbit);
        throw FractionException("Failed to read numerator.");
    }

    // Check if the next character is the separator '/'
    if (is.peek() == '/') {
        is.get(sep);  // Read the separator

        // Try to read the denominator
        if (!(is >> d)) {
            is.setstate(std::ios::failbit);
            throw FractionException("Failed to read denominator after '/'.");
        }

        if (d == 0) {
            is.setstate(std::ios::failbit);
            throw FractionException("Denominator cannot be zero.");
        }
    }

    // Set the fraction value and reduce
    f.numerator = n;
    f.denominator = d;
    f.reduce();

    return is;
}

/* ------------------------ Global Utility Functions ------------------------ */

auto makeFraction(double value, int max_denominator) -> Fraction {
    if (std::isnan(value) || std::isinf(value)) {
        throw FractionException("Cannot create Fraction from NaN or Infinity.");
    }

    // Handle zero
    if (value == 0.0) {
        return Fraction(0, 1);
    }

    // Handle sign
    int sign = (value < 0) ? -1 : 1;
    value = std::abs(value);

    // Use continued fraction algorithm for more accurate approximation
    double epsilon = 1.0 / max_denominator;
    int a = static_cast<int>(std::floor(value));
    double f_val = value - a;  // Renamed to avoid conflict with ostream f

    int h1 = 1, h2 = a;
    int k1 = 0, k2 = 1;

    while (f_val > epsilon && k2 < max_denominator) {
        double r = 1.0 / f_val;
        a = static_cast<int>(std::floor(r));
        f_val = r - a;

        int h = a * h2 + h1;
        int k = a * k2 + k1;

        if (k > max_denominator)
            break;

        h1 = h2;
        h2 = h;
        k1 = k2;
        k2 = k;
    }

    return Fraction(sign * h2, k2);
}

}  // namespace atom::algorithm
