#include "bignumber.hpp"

#include <algorithm>
#include <cassert>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/multiprecision/cpp_int.hpp>
#endif

namespace atom::algorithm {

BigNumber::BigNumber(std::string_view number) {
    try {
        validateString(number);
        initFromString(number);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in BigNumber constructor: {}", e.what());
        throw;
    }
}

void BigNumber::validateString(std::string_view str) {
    if (str.empty()) {
        THROW_INVALID_ARGUMENT("Empty string is not a valid number");
    }

    size_t start = 0;
    if (str[0] == '-') {
        if (str.size() == 1) {
            THROW_INVALID_ARGUMENT(
                "Invalid number format: just a negative sign");
        }
        start = 1;
    }

    if (!std::ranges::all_of(str.begin() + start, str.end(),
                             [](char c) { return std::isdigit(c) != 0; })) {
        THROW_INVALID_ARGUMENT("Invalid character in number string");
    }
}

void BigNumber::initFromString(std::string_view str) {
    isNegative_ = !str.empty() && str[0] == '-';
    size_t start = isNegative_ ? 1 : 0;

    size_t nonZeroPos = str.find_first_not_of('0', start);

    if (nonZeroPos == std::string_view::npos) {
        isNegative_ = false;
        digits_ = {0};
        return;
    }

    digits_.reserve(str.size() - nonZeroPos);
    for (auto it = str.rbegin(); it != str.rend() - nonZeroPos; ++it) {
        if (*it != '-') {
            digits_.push_back(static_cast<uint8_t>(*it - '0'));
        }
    }
}

auto BigNumber::toString() const -> std::string {
    if (digits_.empty() || (digits_.size() == 1 && digits_[0] == 0)) {
        return "0";
    }

    std::string result;
    result.reserve(digits_.size() + (isNegative_ ? 1 : 0));

    if (isNegative_) {
        result.push_back('-');
    }

    for (auto it = digits_.rbegin(); it != digits_.rend(); ++it) {
        result.push_back(static_cast<char>(*it + '0'));
    }

    return result;
}

auto BigNumber::setString(std::string_view newStr) -> BigNumber& {
    try {
        validateString(newStr);
        initFromString(newStr);
        return *this;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in setString: {}", e.what());
        throw;
    }
}

auto BigNumber::trimLeadingZeros() const noexcept -> BigNumber {
    if (digits_.empty() || (digits_.size() == 1 && digits_[0] == 0)) {
        return BigNumber();
    }

    auto lastNonZero = std::find_if(digits_.rbegin(), digits_.rend(),
                                    [](uint8_t digit) { return digit != 0; });

    if (lastNonZero == digits_.rend()) {
        return BigNumber();
    }

    BigNumber result;
    result.isNegative_ = isNegative_;
    result.digits_.assign(digits_.begin(), lastNonZero.base());
    return result;
}

auto BigNumber::add(const BigNumber& other) const -> BigNumber {
    try {
        LOG_F(INFO, "Adding {} and {}", toString(), other.toString());

#ifdef ATOM_USE_BOOST
        boost::multiprecision::cpp_int num1(toString());
        boost::multiprecision::cpp_int num2(other.toString());
        boost::multiprecision::cpp_int result = num1 + num2;
        return BigNumber(result.str());
#else
        if (isNegative_ != other.isNegative_) {
            if (isNegative_) {
                return other.subtract(abs());
            } else {
                return subtract(other.abs());
            }
        }

        BigNumber result;
        result.isNegative_ = isNegative_;

        const auto& a = digits_;
        const auto& b = other.digits_;

        result.digits_.reserve(std::max(a.size(), b.size()) + 1);

        uint8_t carry = 0;
        size_t i = 0;

        while (i < a.size() || i < b.size() || carry) {
            uint8_t sum = carry;
            if (i < a.size())
                sum += a[i];
            if (i < b.size())
                sum += b[i];

            carry = sum / 10;
            result.digits_.push_back(sum % 10);
            ++i;
        }

        LOG_F(INFO, "Result of addition: {}", result.toString());
        return result;
#endif
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in BigNumber::add: {}", e.what());
        throw;
    }
}

auto BigNumber::subtract(const BigNumber& other) const -> BigNumber {
    try {
        LOG_F(INFO, "Subtracting {} from {}", other.toString(), toString());

#ifdef ATOM_USE_BOOST
        boost::multiprecision::cpp_int num1(toString());
        boost::multiprecision::cpp_int num2(other.toString());
        boost::multiprecision::cpp_int result = num1 - num2;
        return BigNumber(result.str());
#else
        // 处理符号不同的情况
        if (isNegative_ != other.isNegative_) {
            if (isNegative_) {
                // (-a) - b = -(a + b)
                BigNumber result = abs().add(other);
                result.isNegative_ = true;
                return result;
            } else {
                // a - (-b) = a + b
                return add(other.abs());
            }
        }

        bool resultNegative;
        const BigNumber *larger, *smaller;

        if (abs().equals(other.abs())) {
            return BigNumber();
        } else if ((isNegative_ && *this > other) ||
                   (!isNegative_ && *this < other)) {
            larger = &other;
            smaller = this;
            resultNegative = !isNegative_;
        } else {
            larger = this;
            smaller = &other;
            resultNegative = isNegative_;
        }

        BigNumber result;
        result.isNegative_ = resultNegative;

        const auto& a = larger->digits_;
        const auto& b = smaller->digits_;

        result.digits_.reserve(a.size());

        int borrow = 0;

        for (size_t i = 0; i < a.size(); ++i) {
            int diff = a[i] - borrow;
            if (i < b.size())
                diff -= b[i];

            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }

            result.digits_.push_back(static_cast<uint8_t>(diff));
        }

        while (!result.digits_.empty() && result.digits_.back() == 0) {
            result.digits_.pop_back();
        }

        if (result.digits_.empty()) {
            result.digits_.push_back(0);
            result.isNegative_ = false;
        }

        LOG_F(INFO, "Result of subtraction: {}", result.toString());
        return result;
#endif
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in BigNumber::subtract: {}", e.what());
        throw;
    }
}

auto BigNumber::multiply(const BigNumber& other) const -> BigNumber {
    try {
        LOG_F(INFO, "Multiplying {} and {}", toString(), other.toString());

#ifdef ATOM_USE_BOOST
        boost::multiprecision::cpp_int num1(toString());
        boost::multiprecision::cpp_int num2(other.toString());
        boost::multiprecision::cpp_int result = num1 * num2;
        return BigNumber(result.str());
#else
        if ((digits_.size() == 1 && digits_[0] == 0) ||
            (other.digits_.size() == 1 && other.digits_[0] == 0)) {
            return BigNumber();
        }

        if (digits_.size() > 100 && other.digits_.size() > 100) {
            return multiplyKaratsuba(other);
        }

        // 结果的符号
        bool resultNegative = isNegative_ != other.isNegative_;

        std::vector<uint8_t> result(digits_.size() + other.digits_.size(), 0);

        for (size_t i = 0; i < digits_.size(); ++i) {
            uint8_t carry = 0;
            for (size_t j = 0; j < other.digits_.size() || carry; ++j) {
                uint16_t product =
                    result[i + j] +
                    digits_[i] *
                        (j < other.digits_.size() ? other.digits_[j] : 0) +
                    carry;

                result[i + j] = product % 10;
                carry = product / 10;
            }
        }

        while (!result.empty() && result.back() == 0) {
            result.pop_back();
        }

        BigNumber resultNum;
        resultNum.isNegative_ = resultNegative && !result.empty();
        resultNum.digits_ = std::move(result);

        if (resultNum.digits_.empty()) {
            resultNum.digits_.push_back(0);
        }

        LOG_F(INFO, "Result of multiplication: {}", resultNum.toString());
        return resultNum;
#endif
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in BigNumber::multiply: {}", e.what());
        throw;
    }
}

auto BigNumber::multiplyKaratsuba(const BigNumber& other) const -> BigNumber {
    try {
        LOG_F(INFO, "Using Karatsuba algorithm to multiply {} and {}",
              toString(), other.toString());

        bool resultNegative = isNegative_ != other.isNegative_;

        std::vector<uint8_t> result =
            karatsubaMultiply(std::span<const uint8_t>(digits_),
                              std::span<const uint8_t>(other.digits_));

        BigNumber resultNum;
        resultNum.isNegative_ = resultNegative && !result.empty();
        resultNum.digits_ = std::move(result);

        if (resultNum.digits_.empty()) {
            resultNum.digits_.push_back(0);
        }

        return resultNum;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in BigNumber::multiplyKaratsuba: {}", e.what());
        throw;
    }
}

std::vector<uint8_t> BigNumber::karatsubaMultiply(std::span<const uint8_t> a,
                                                  std::span<const uint8_t> b) {
    if (a.size() <= 32 || b.size() <= 32) {
        std::vector<uint8_t> result(a.size() + b.size(), 0);
        for (size_t i = 0; i < a.size(); ++i) {
            uint8_t carry = 0;
            for (size_t j = 0; j < b.size() || carry; ++j) {
                uint16_t product =
                    result[i + j] + a[i] * (j < b.size() ? b[j] : 0) + carry;

                result[i + j] = product % 10;
                carry = product / 10;
            }
        }

        while (!result.empty() && result.back() == 0) {
            result.pop_back();
        }

        return result;
    }

    if (a.size() < b.size()) {
        return karatsubaMultiply(b, a);
    }

    size_t m = a.size() / 2;

    std::span<const uint8_t> low1(a.data(), m);
    std::span<const uint8_t> high1(a.data() + m, a.size() - m);

    std::span<const uint8_t> low2;
    std::span<const uint8_t> high2;

    if (b.size() <= m) {
        low2 = b;
        high2 = std::span<const uint8_t>();
    } else {
        low2 = std::span<const uint8_t>(b.data(), m);
        high2 = std::span<const uint8_t>(b.data() + m, b.size() - m);
    }

    auto z0 = karatsubaMultiply(low1, low2);
    auto z1 = karatsubaMultiply(low1, high2);
    auto z2 = karatsubaMultiply(high1, low2);
    auto z3 = karatsubaMultiply(high1, high2);

    std::vector<uint8_t> result(a.size() + b.size(), 0);

    for (size_t i = 0; i < z0.size(); ++i) {
        result[i] += z0[i];
    }

    for (size_t i = 0; i < z1.size(); ++i) {
        result[i + m] += z1[i];
    }

    for (size_t i = 0; i < z2.size(); ++i) {
        result[i + m] += z2[i];
    }

    for (size_t i = 0; i < z3.size(); ++i) {
        result[i + 2 * m] += z3[i];
    }

    uint8_t carry = 0;
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] += carry;
        carry = result[i] / 10;
        result[i] %= 10;
    }

    // 移除尾随的零
    while (!result.empty() && result.back() == 0) {
        result.pop_back();
    }

    return result;
}

auto BigNumber::divide(const BigNumber& other) const -> BigNumber {
    try {
        LOG_F(INFO, "Dividing {} by {}", toString(), other.toString());

#ifdef ATOM_USE_BOOST
        boost::multiprecision::cpp_int num1(toString());
        boost::multiprecision::cpp_int num2(other.toString());
        if (num2 == 0) {
            LOG_F(ERROR, "Division by zero");
            THROW_INVALID_ARGUMENT("Division by zero");
        }
        boost::multiprecision::cpp_int result = num1 / num2;
        return BigNumber(result.str());
#else
        if (other.equals(BigNumber("0"))) {
            LOG_F(ERROR, "Division by zero");
            THROW_INVALID_ARGUMENT("Division by zero");
        }

        bool resultNegative = isNegative_ != other.isNegative_;
        BigNumber dividend = abs();
        BigNumber divisor = other.abs();
        BigNumber quotient("0");
        BigNumber current("0");

        for (char digit : dividend.toString()) {
            current = current.multiply(BigNumber("10"))
                          .add(BigNumber(std::string(1, digit)));
            int count = 0;
            while (current >= divisor) {
                current = current.subtract(divisor);
                ++count;
            }
            quotient = quotient.multiply(BigNumber("10"))
                           .add(BigNumber(std::to_string(count)));
        }

        quotient = quotient.trimLeadingZeros();
        if (resultNegative && !quotient.equals(BigNumber("0"))) {
            quotient = quotient.negate();
        }

        LOG_F(INFO, "Result of division: {}", quotient.toString());
        return quotient;
#endif
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in BigNumber::divide: {}", e.what());
        throw;
    }
}

auto BigNumber::pow(int exponent) const -> BigNumber {
    try {
        LOG_F(INFO, "Raising {} to the power of {}", toString(), exponent);

#ifdef ATOM_USE_BOOST
        boost::multiprecision::cpp_int base(toString());
        boost::multiprecision::cpp_int result =
            boost::multiprecision::pow(base, exponent);
        return BigNumber(result.str());
#else
        if (exponent < 0) {
            LOG_F(ERROR, "Negative exponents are not supported");
            THROW_INVALID_ARGUMENT("Negative exponents are not supported");
        }
        if (exponent == 0) {
            return BigNumber("1");
        }
        if (exponent == 1) {
            return *this;
        }
        BigNumber result("1");
        BigNumber base = *this;
        while (exponent != 0) {
            if (exponent & 1) {
                result = result.multiply(base);
            }
            exponent >>= 1;
            if (exponent != 0) {
                base = base.multiply(base);
            }
        }
        LOG_F(INFO, "Result of exponentiation: {}", result.toString());
        return result;
#endif
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in BigNumber::pow: {}", e.what());
        throw;
    }
}

auto operator>(const BigNumber& b1, const BigNumber& b2) -> bool {
    try {
        LOG_F(INFO, "Comparing if {} > {}", b1.toString(), b2.toString());

#ifdef ATOM_USE_BOOST
        boost::multiprecision::cpp_int num1(b1.toString());
        boost::multiprecision::cpp_int num2(b2.toString());
        return num1 > num2;
#else
        if (b1.isNegative_ || b2.isNegative_) {
            if (b1.isNegative_ && b2.isNegative_) {
                LOG_F(INFO, "Both numbers are negative. Flipping comparison.");
                return atom::algorithm::BigNumber(b2).abs() >
                       atom::algorithm::BigNumber(b1).abs();
            }
            return b1.isNegative_ < b2.isNegative_;
        }

        BigNumber b1Trimmed = b1.trimLeadingZeros();
        BigNumber b2Trimmed = b2.trimLeadingZeros();

        if (b1Trimmed.digits_.size() != b2Trimmed.digits_.size()) {
            return b1Trimmed.digits_.size() > b2Trimmed.digits_.size();
        }
        return b1Trimmed.digits_ > b2Trimmed.digits_;
#endif
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in operator>: {}", e.what());
        throw;
    }
}

void BigNumber::validate() const {
    if (digits_.empty()) {
        THROW_INVALID_ARGUMENT("Empty string is not a valid number");
    }
    size_t start = 0;
    if (isNegative_) {
        if (digits_.size() == 1) {
            THROW_INVALID_ARGUMENT("Invalid number format");
        }
        start = 1;
    }
    for (size_t i = start; i < digits_.size(); ++i) {
        if (std::isdigit(digits_[i]) == 0) {
            THROW_INVALID_ARGUMENT("Invalid character in number string");
        }
    }
}

}  // namespace atom::algorithm