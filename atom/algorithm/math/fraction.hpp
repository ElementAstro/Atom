/*
 * fraction.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-28

Description: Implementation of Fraction class

**************************************************/

#ifndef ATOM_ALGORITHM_FRACTION_HPP
#define ATOM_ALGORITHM_FRACTION_HPP

#include <cmath>
#include <iostream>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

// 可选的Boost支持
#ifdef ATOM_USE_BOOST_RATIONAL
#include <boost/rational.hpp>
#endif

namespace atom::algorithm {

/**
 * @brief Exception class for Fraction errors.
 */
class FractionException : public std::runtime_error {
public:
    explicit FractionException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Represents a fraction with numerator and denominator.
 */
class Fraction {
private:
    int numerator;   /**< The numerator of the fraction. */
    int denominator; /**< The denominator of the fraction. */

    /**
     * @brief Computes the greatest common divisor (GCD) of two numbers.
     * @param a The first number.
     * @param b The second number.
     * @return The GCD of the two numbers.
     */
    static constexpr int gcd(int a, int b) noexcept {
        if (a == 0)
            return std::abs(b);
        if (b == 0)
            return std::abs(a);

        if (a == std::numeric_limits<int>::min()) {
            a = std::numeric_limits<int>::min() + 1;
        }
        if (b == std::numeric_limits<int>::min()) {
            b = std::numeric_limits<int>::min() + 1;
        }

        return std::abs(std::gcd(a, b));
    }

    constexpr void reduce() noexcept {
        if (denominator == 0) {
            return;
        }

        if (denominator < 0) {
            numerator = -numerator;
            denominator = -denominator;
        }

        int divisor = gcd(numerator, denominator);
        if (divisor > 1) {
            numerator /= divisor;
            denominator /= divisor;
        }
    }

public:
    /**
     * @brief Constructs a new Fraction object with the given numerator and
     * denominator.
     * @param n The numerator (default is 0).
     * @param d The denominator (default is 1).
     * @throws FractionException if the denominator is zero.
     */
    constexpr Fraction(int n, int d) : numerator(n), denominator(d) {
        if (denominator == 0) {
            throw FractionException("Denominator cannot be zero.");
        }
        reduce();
    }

    /**
     * @brief Constructs a new Fraction object with the given integer value.
     * @param value The integer value.
     */
    constexpr explicit Fraction(int value) noexcept
        : numerator(value), denominator(1) {}

    /**
     * @brief Default constructor. Initializes the fraction as 0/1.
     */
    constexpr Fraction() noexcept : Fraction(0, 1) {}

    /**
     * @brief Copy constructor
     * @param other The fraction to copy
     */
    constexpr Fraction(const Fraction&) noexcept = default;

    /**
     * @brief Move constructor
     * @param other The fraction to move from
     */
    constexpr Fraction(Fraction&&) noexcept = default;

    /**
     * @brief Copy assignment operator
     * @param other The fraction to copy
     * @return Reference to this fraction
     */
    constexpr Fraction& operator=(const Fraction&) noexcept = default;

    /**
     * @brief Move assignment operator
     * @param other The fraction to move from
     * @return Reference to this fraction
     */
    constexpr Fraction& operator=(Fraction&&) noexcept = default;

    /**
     * @brief Default destructor
     */
    ~Fraction() = default;

    /**
     * @brief Get the numerator of the fraction
     * @return The numerator
     */
    [[nodiscard]] constexpr int getNumerator() const noexcept {
        return numerator;
    }

    /**
     * @brief Get the denominator of the fraction
     * @return The denominator
     */
    [[nodiscard]] constexpr int getDenominator() const noexcept {
        return denominator;
    }

    /**
     * @brief Adds another fraction to this fraction.
     * @param other The fraction to add.
     * @return Reference to the modified fraction.
     * @throws FractionException on arithmetic overflow.
     */
    Fraction& operator+=(const Fraction& other);

    /**
     * @brief Subtracts another fraction from this fraction.
     * @param other The fraction to subtract.
     * @return Reference to the modified fraction.
     * @throws FractionException on arithmetic overflow.
     */
    Fraction& operator-=(const Fraction& other);

    /**
     * @brief Multiplies this fraction by another fraction.
     * @param other The fraction to multiply by.
     * @return Reference to the modified fraction.
     * @throws FractionException if multiplication leads to zero denominator.
     */
    Fraction& operator*=(const Fraction& other);

    /**
     * @brief Divides this fraction by another fraction.
     * @param other The fraction to divide by.
     * @return Reference to the modified fraction.
     * @throws FractionException if division by zero occurs.
     */
    Fraction& operator/=(const Fraction& other);

    /**
     * @brief Adds another fraction to this fraction.
     * @param other The fraction to add.
     * @return The result of addition.
     */
    [[nodiscard]] Fraction operator+(const Fraction& other) const;

    /**
     * @brief Subtracts another fraction from this fraction.
     * @param other The fraction to subtract.
     * @return The result of subtraction.
     */
    [[nodiscard]] Fraction operator-(const Fraction& other) const;

    /**
     * @brief Multiplies this fraction by another fraction.
     * @param other The fraction to multiply by.
     * @return The result of multiplication.
     */
    [[nodiscard]] Fraction operator*(const Fraction& other) const;

    /**
     * @brief Divides this fraction by another fraction.
     * @param other The fraction to divide by.
     * @return The result of division.
     */
    [[nodiscard]] Fraction operator/(const Fraction& other) const;

    /**
     * @brief Unary plus operator
     * @return Copy of this fraction
     */
    [[nodiscard]] constexpr Fraction operator+() const noexcept {
        return *this;
    }

    /**
     * @brief Unary minus operator
     * @return Negated copy of this fraction
     */
    [[nodiscard]] constexpr Fraction operator-() const noexcept {
        return Fraction(-numerator, denominator);
    }

#if __cplusplus >= 202002L
    /**
     * @brief Compares this fraction with another fraction.
     * @param other The fraction to compare with.
     * @return A std::strong_ordering indicating the comparison result.
     */
    [[nodiscard]] auto operator<=>(const Fraction& other) const
        -> std::strong_ordering;
#else
    /**
     * @brief Less than operator
     * @param other The fraction to compare with
     * @return True if this fraction is less than other
     */
    [[nodiscard]] bool operator<(const Fraction& other) const noexcept;

    /**
     * @brief Less than or equal operator
     * @param other The fraction to compare with
     * @return True if this fraction is less than or equal to other
     */
    [[nodiscard]] bool operator<=(const Fraction& other) const noexcept;

    /**
     * @brief Greater than operator
     * @param other The fraction to compare with
     * @return True if this fraction is greater than other
     */
    [[nodiscard]] bool operator>(const Fraction& other) const noexcept;

    /**
     * @brief Greater than or equal operator
     * @param other The fraction to compare with
     * @return True if this fraction is greater than or equal to other
     */
    [[nodiscard]] bool operator>=(const Fraction& other) const noexcept;
#endif

    /**
     * @brief Checks if this fraction is equal to another fraction.
     * @param other The fraction to compare with.
     * @return True if fractions are equal, false otherwise.
     */
    [[nodiscard]] bool operator==(const Fraction& other) const noexcept;

    /**
     * @brief Checks if this fraction is not equal to another fraction.
     * @param other The fraction to compare with.
     * @return True if fractions are not equal, false otherwise.
     */
    [[nodiscard]] bool operator!=(const Fraction& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Converts the fraction to a double value.
     * @return The fraction as a double.
     */
    [[nodiscard]] constexpr explicit operator double() const noexcept {
        return static_cast<double>(numerator) / denominator;
    }

    /**
     * @brief Converts the fraction to a float value.
     * @return The fraction as a float.
     */
    [[nodiscard]] constexpr explicit operator float() const noexcept {
        return static_cast<float>(numerator) / denominator;
    }

    /**
     * @brief Converts the fraction to an integer value.
     * @return The fraction as an integer (truncates towards zero).
     */
    [[nodiscard]] constexpr explicit operator int() const noexcept {
        return numerator / denominator;
    }

    /**
     * @brief Converts the fraction to a string representation.
     * @return The string representation of the fraction.
     */
    [[nodiscard]] std::string toString() const;

    /**
     * @brief Converts the fraction to a double value.
     * @return The fraction as a double.
     */
    [[nodiscard]] constexpr double toDouble() const noexcept {
        return static_cast<double>(*this);
    }

    /**
     * @brief Inverts the fraction (reciprocal).
     * @return Reference to the modified fraction.
     * @throws FractionException if numerator is zero.
     */
    Fraction& invert();

    /**
     * @brief Returns the absolute value of the fraction.
     * @return A new Fraction representing the absolute value.
     */
    [[nodiscard]] constexpr Fraction abs() const noexcept {
        return Fraction(numerator < 0 ? -numerator : numerator, denominator);
    }

    /**
     * @brief Checks if the fraction is zero.
     * @return True if the fraction is zero, false otherwise.
     */
    [[nodiscard]] constexpr bool isZero() const noexcept {
        return numerator == 0;
    }

    /**
     * @brief Checks if the fraction is positive.
     * @return True if the fraction is positive, false otherwise.
     */
    [[nodiscard]] constexpr bool isPositive() const noexcept {
        return numerator > 0;
    }

    /**
     * @brief Checks if the fraction is negative.
     * @return True if the fraction is negative, false otherwise.
     */
    [[nodiscard]] constexpr bool isNegative() const noexcept {
        return numerator < 0;
    }

    /**
     * @brief Safely computes the power of a fraction
     * @param exponent The exponent to raise the fraction to
     * @return The fraction raised to the given power, or std::nullopt if
     * operation cannot be performed
     */
    [[nodiscard]] std::optional<Fraction> pow(int exponent) const noexcept;

    /**
     * @brief Creates a fraction from a string representation (e.g., "3/4")
     * @param str The string to parse
     * @return The parsed fraction, or std::nullopt if parsing fails
     */
    [[nodiscard]] static std::optional<Fraction> fromString(
        std::string_view str) noexcept;

#ifdef ATOM_USE_BOOST_RATIONAL
    /**
     * @brief Converts to a boost::rational
     * @return Equivalent boost::rational<int>
     */
    [[nodiscard]] boost::rational<int> toBoostRational() const {
        return boost::rational<int>(numerator, denominator);
    }

    /**
     * @brief Constructs from a boost::rational
     * @param r The boost::rational to convert from
     */
    explicit Fraction(const boost::rational<int>& r)
        : numerator(r.numerator()), denominator(r.denominator()) {}
#endif

    /**
     * @brief Outputs the fraction to the output stream.
     * @param os The output stream.
     * @param f The fraction to output.
     * @return Reference to the output stream.
     */
    friend auto operator<<(std::ostream& os, const Fraction& f)
        -> std::ostream&;

    /**
     * @brief Inputs the fraction from the input stream.
     * @param is The input stream.
     * @param f The fraction to input.
     * @return Reference to the input stream.
     * @throws FractionException if the input format is invalid or denominator
     * is zero.
     */
    friend auto operator>>(std::istream& is, Fraction& f) -> std::istream&;
};

/**
 * @brief Creates a Fraction from an integer.
 * @param value The integer value.
 * @return A Fraction representing the integer.
 */
[[nodiscard]] inline constexpr Fraction makeFraction(int value) noexcept {
    return Fraction(value, 1);
}

/**
 * @brief Creates a Fraction from a double by approximating it.
 * @param value The double value.
 * @param max_denominator The maximum allowed denominator to limit the
 * approximation.
 * @return A Fraction approximating the double value.
 */
[[nodiscard]] Fraction makeFraction(double value,
                                    int max_denominator = 1000000);

/**
 * @brief User-defined literal for creating fractions (e.g., 3_fr)
 * @param value The integer value for the fraction
 * @return A Fraction representing the value
 */
[[nodiscard]] inline constexpr Fraction operator""_fr(
    unsigned long long value) noexcept {
    return Fraction(static_cast<int>(value), 1);
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_FRACTION_HPP