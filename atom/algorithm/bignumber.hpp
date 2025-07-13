#ifndef ATOM_ALGORITHM_BIGNUMBER_HPP
#define ATOM_ALGORITHM_BIGNUMBER_HPP

#include <cctype>
#include <concepts>
#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace atom::algorithm {

/**
 * @class BigNumber
 * @brief A class to represent and manipulate large numbers with C++20 features.
 */
class BigNumber {
public:
    constexpr BigNumber() noexcept : isNegative_(false), digits_{0} {}

    /**
     * @brief Constructs a BigNumber from a string_view.
     * @param number The string representation of the number.
     * @throws std::invalid_argument If the string is not a valid number.
     */
    explicit BigNumber(std::string_view number);

    /**
     * @brief Constructs a BigNumber from an integer.
     * @tparam T Integer type that satisfies std::integral concept
     */
    template <std::integral T>
    constexpr explicit BigNumber(T number) noexcept;

    BigNumber(BigNumber&& other) noexcept = default;
    BigNumber& operator=(BigNumber&& other) noexcept = default;
    BigNumber(const BigNumber&) = default;
    BigNumber& operator=(const BigNumber&) = default;
    ~BigNumber() = default;

    /**
     * @brief Adds two BigNumber objects.
     * @param other The other BigNumber to add.
     * @return The result of the addition.
     */
    [[nodiscard]] auto add(const BigNumber& other) const -> BigNumber;

    /**
     * @brief Subtracts another BigNumber from this one.
     * @param other The BigNumber to subtract.
     * @return The result of the subtraction.
     */
    [[nodiscard]] auto subtract(const BigNumber& other) const -> BigNumber;

    /**
     * @brief Multiplies by another BigNumber.
     * @param other The BigNumber to multiply by.
     * @return The result of the multiplication.
     */
    [[nodiscard]] auto multiply(const BigNumber& other) const -> BigNumber;

    /**
     * @brief Divides by another BigNumber.
     * @param other The BigNumber to use as the divisor.
     * @return The result of the division.
     * @throws std::invalid_argument If the divisor is zero.
     */
    [[nodiscard]] auto divide(const BigNumber& other) const -> BigNumber;

    /**
     * @brief Calculates the power.
     * @param exponent The exponent value.
     * @return The result of the BigNumber raised to the exponent.
     * @throws std::invalid_argument If the exponent is negative.
     */
    [[nodiscard]] auto pow(int exponent) const -> BigNumber;

    /**
     * @brief Gets the string representation.
     * @return The string representation of the BigNumber.
     */
    [[nodiscard]] auto toString() const -> std::string;

    /**
     * @brief Sets the value from a string.
     * @param newStr The new string representation.
     * @return A reference to the updated BigNumber.
     * @throws std::invalid_argument If the string is not a valid number.
     */
    auto setString(std::string_view newStr) -> BigNumber&;

    /**
     * @brief Returns the negation of this number.
     * @return The negated BigNumber.
     */
    [[nodiscard]] auto negate() const -> BigNumber;

    /**
     * @brief Removes leading zeros.
     * @return The BigNumber with leading zeros removed.
     */
    [[nodiscard]] auto trimLeadingZeros() const noexcept -> BigNumber;

    /**
     * @brief Checks if two BigNumbers are equal.
     * @param other The BigNumber to compare.
     * @return True if they are equal.
     */
    [[nodiscard]] constexpr auto equals(const BigNumber& other) const noexcept
        -> bool;

    /**
     * @brief Checks if equal to an integer.
     * @tparam T The integer type.
     * @param other The integer to compare.
     * @return True if they are equal.
     */
    template <std::integral T>
    [[nodiscard]] constexpr auto equals(T other) const noexcept -> bool {
        return equals(BigNumber(other));
    }

    /**
     * @brief Checks if equal to a number represented as a string.
     * @param other The number string.
     * @return True if they are equal.
     */
    [[nodiscard]] auto equals(std::string_view other) const -> bool {
        return equals(BigNumber(other));
    }

    /**
     * @brief Gets the number of digits.
     * @return The number of digits.
     */
    [[nodiscard]] constexpr auto digits() const noexcept -> size_t {
        return digits_.size();
    }

    /**
     * @brief Checks if the number is negative.
     * @return True if the number is negative.
     */
    [[nodiscard]] constexpr auto isNegative() const noexcept -> bool {
        return isNegative_;
    }

    /**
     * @brief Checks if the number is positive or zero.
     * @return True if the number is positive or zero.
     */
    [[nodiscard]] constexpr auto isPositive() const noexcept -> bool {
        return !isNegative();
    }

    /**
     * @brief Checks if the number is even.
     * @return True if the number is even.
     */
    [[nodiscard]] constexpr auto isEven() const noexcept -> bool {
        return digits_.empty() ? true : (digits_[0] % 2 == 0);
    }

    /**
     * @brief Checks if the number is odd.
     * @return True if the number is odd.
     */
    [[nodiscard]] constexpr auto isOdd() const noexcept -> bool {
        return !isEven();
    }

    /**
     * @brief Gets the absolute value.
     * @return The absolute value.
     */
    [[nodiscard]] auto abs() const -> BigNumber;

    friend auto operator<<(std::ostream& os, const BigNumber& num)
        -> std::ostream&;
    friend auto operator+(const BigNumber& b1, const BigNumber& b2)
        -> BigNumber {
        return b1.add(b2);
    }
    friend auto operator-(const BigNumber& b1, const BigNumber& b2)
        -> BigNumber {
        return b1.subtract(b2);
    }
    friend auto operator*(const BigNumber& b1, const BigNumber& b2)
        -> BigNumber {
        return b1.multiply(b2);
    }
    friend auto operator/(const BigNumber& b1, const BigNumber& b2)
        -> BigNumber {
        return b1.divide(b2);
    }
    friend auto operator^(const BigNumber& b1, int b2) -> BigNumber {
        return b1.pow(b2);
    }
    friend auto operator==(const BigNumber& b1, const BigNumber& b2) noexcept
        -> bool {
        return b1.equals(b2);
    }
    friend auto operator>(const BigNumber& b1, const BigNumber& b2) -> bool;
    friend auto operator<(const BigNumber& b1, const BigNumber& b2) -> bool {
        return !(b1 == b2) && !(b1 > b2);
    }
    friend auto operator>=(const BigNumber& b1, const BigNumber& b2) -> bool {
        return b1 > b2 || b1 == b2;
    }
    friend auto operator<=(const BigNumber& b1, const BigNumber& b2) -> bool {
        return b1 < b2 || b1 == b2;
    }

    auto operator+=(const BigNumber& other) -> BigNumber&;
    auto operator-=(const BigNumber& other) -> BigNumber&;
    auto operator*=(const BigNumber& other) -> BigNumber&;
    auto operator/=(const BigNumber& other) -> BigNumber&;

    auto operator++() -> BigNumber&;
    auto operator--() -> BigNumber&;
    auto operator++(int) -> BigNumber;
    auto operator--(int) -> BigNumber;

    /**
     * @brief Accesses a digit at a specific position.
     * @param index The index to access.
     * @return The digit at that position.
     * @throws std::out_of_range If the index is out of range.
     */
    [[nodiscard]] constexpr auto at(size_t index) const -> uint8_t;

    /**
     * @brief Subscript operator.
     * @param index The index to access.
     * @return The digit at that position.
     * @throws std::out_of_range If the index is out of range.
     */
    auto operator[](size_t index) const -> uint8_t { return at(index); }

private:
    bool isNegative_;
    std::vector<uint8_t> digits_;

    static void validateString(std::string_view str);
    void validate() const;
    void initFromString(std::string_view str);

    [[nodiscard]] auto multiplyKaratsuba(const BigNumber& other) const
        -> BigNumber;
    static std::vector<uint8_t> karatsubaMultiply(std::span<const uint8_t> a,
                                                  std::span<const uint8_t> b);
};

template <std::integral T>
constexpr BigNumber::BigNumber(T number) noexcept : isNegative_(number < 0) {
    if (number == 0) {
        digits_.push_back(0);
        return;
    }

    auto absNumber =
        static_cast<std::make_unsigned_t<T>>(number < 0 ? -number : number);
    digits_.reserve(20);

    while (absNumber > 0) {
        digits_.push_back(static_cast<uint8_t>(absNumber % 10));
        absNumber /= 10;
    }
}

constexpr auto BigNumber::equals(const BigNumber& other) const noexcept
    -> bool {
    return isNegative_ == other.isNegative_ && digits_ == other.digits_;
}

constexpr auto BigNumber::at(size_t index) const -> uint8_t {
    if (index >= digits_.size()) {
        throw std::out_of_range("Index out of range in BigNumber::at");
    }
    return digits_[index];
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_BIGNUMBER_HPP
