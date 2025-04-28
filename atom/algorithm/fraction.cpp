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
#include <numeric>
#include <sstream>

// 检查是否支持SSE4.1或更高版本
#if defined(__SSE4_1__) || defined(__AVX__) || defined(__AVX2__)
#include <immintrin.h>
#define ATOM_FRACTION_USE_SIMD
#endif

namespace atom::algorithm {

/* ------------------------ Private Methods ------------------------ */

constexpr int Fraction::gcd(int a, int b) noexcept {
    // 处理特殊情况
    if (a == 0)
        return std::abs(b);
    if (b == 0)
        return std::abs(a);

    // 防止最小整数的特殊情况
    if (a == std::numeric_limits<int>::min()) {
        a = std::numeric_limits<int>::min() + 1;
    }
    if (b == std::numeric_limits<int>::min()) {
        b = std::numeric_limits<int>::min() + 1;
    }

    return std::abs(std::gcd(a, b));
}

void Fraction::reduce() noexcept {
    if (denominator == 0) {
        // 分母为零的检查在构造函数和运算符中处理
        return;
    }

    // 确保分母为正
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }

    // 计算最大公约数并简化分数
    int divisor = gcd(numerator, denominator);
    if (divisor > 1) {
        numerator /= divisor;
        denominator /= divisor;
    }
}

/* ------------------------ Arithmetic Operators ------------------------ */

auto Fraction::operator+=(const Fraction& other) -> Fraction& {
    try {
        // 快速路径：如果加数为0，不执行任何操作
        if (other.numerator == 0)
            return *this;
        if (numerator == 0) {
            numerator = other.numerator;
            denominator = other.denominator;
            return *this;
        }

        // 使用安全的长整形计算来防止溢出
        long long commonDenominator =
            static_cast<long long>(denominator) * other.denominator;
        long long newNumerator =
            static_cast<long long>(numerator) * other.denominator +
            static_cast<long long>(other.numerator) * denominator;

        // 检查溢出情况
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
        // 快速路径：如果减数为0，不执行任何操作
        if (other.numerator == 0)
            return *this;

        // 使用安全的长整形计算来防止溢出
        long long commonDenominator =
            static_cast<long long>(denominator) * other.denominator;
        long long newNumerator =
            static_cast<long long>(numerator) * other.denominator -
            static_cast<long long>(other.numerator) * denominator;

        // 检查溢出情况
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
        // 快速路径：如果乘数为0，结果为0
        if (other.numerator == 0 || numerator == 0) {
            numerator = 0;
            denominator = 1;
            return *this;
        }

        // 为了最大化约分的效果，先预先计算gcd
        int gcd1 = gcd(numerator, other.denominator);
        int gcd2 = gcd(denominator, other.numerator);

        // 预先约分可以减少溢出风险
        long long n = (static_cast<long long>(numerator) / gcd1) *
                      (static_cast<long long>(other.numerator) / gcd2);
        long long d = (static_cast<long long>(denominator) / gcd2) *
                      (static_cast<long long>(other.denominator) / gcd1);

        // 检查溢出情况
        if (n > std::numeric_limits<int>::max() ||
            n < std::numeric_limits<int>::min() ||
            d > std::numeric_limits<int>::max() ||
            d < std::numeric_limits<int>::min()) {
            throw FractionException("Integer overflow during multiplication.");
        }

        numerator = static_cast<int>(n);
        denominator = static_cast<int>(d);
        // 再进行一次约分以确保最简形式
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

        // 为了最大化约分的效果，先预先计算gcd
        int gcd1 = gcd(numerator, other.numerator);
        int gcd2 = gcd(denominator, other.denominator);

        // 预先约分可以减少溢出风险
        long long n = (static_cast<long long>(numerator) / gcd1) *
                      (static_cast<long long>(other.denominator) / gcd2);
        long long d = (static_cast<long long>(denominator) / gcd2) *
                      (static_cast<long long>(other.numerator) / gcd1);

        // 确保分母不为零
        if (d == 0) {
            throw FractionException(
                "Denominator cannot be zero after division.");
        }

        // 检查溢出情况
        if (n > std::numeric_limits<int>::max() ||
            n < std::numeric_limits<int>::min() ||
            d > std::numeric_limits<int>::max() ||
            d < std::numeric_limits<int>::min()) {
            throw FractionException("Integer overflow during division.");
        }

        numerator = static_cast<int>(n);
        denominator = static_cast<int>(d);
        // 确保分母为正
        if (denominator < 0) {
            numerator = -numerator;
            denominator = -denominator;
        }
        // 再进行一次约分以确保最简形式
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
    // 使用跨乘法来比较分数，避免溢出
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
    // 使用跨乘法比较，避免除法
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
    // 由于我们总是将分数约分到最简形式，所以可以直接比较分子和分母
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
        // 处理特殊情况
        if (exponent == 0) {
            // 任何数的0次幂都是1
            return Fraction(1, 1);
        }

        if (exponent == 1) {
            // 1次幂就是它自己
            return *this;
        }

        if (numerator == 0) {
            // 0的任何正数次幂都是0，负数次幂无效
            return exponent > 0 ? std::optional<Fraction>(Fraction(0, 1))
                                : std::nullopt;
        }

        // 处理负指数
        bool isNegativeExponent = exponent < 0;
        exponent = std::abs(exponent);

        // 计算幂
        long long resultNumerator = 1;
        long long resultDenominator = 1;

        long long n = numerator;
        long long d = denominator;

        // 使用快速幂算法
        for (int i = 0; i < exponent; i++) {
            resultNumerator *= n;
            resultDenominator *= d;

            // 检查溢出
            if (resultNumerator > std::numeric_limits<int>::max() ||
                resultNumerator < std::numeric_limits<int>::min() ||
                resultDenominator > std::numeric_limits<int>::max() ||
                resultDenominator < std::numeric_limits<int>::min()) {
                return std::nullopt;  // 溢出，返回空值
            }
        }

        // 如果是负指数，交换分子和分母
        if (isNegativeExponent) {
            if (resultNumerator == 0) {
                return std::nullopt;  // 无法求负数次幂，分母会变为0
            }
            std::swap(resultNumerator, resultDenominator);
        }

        // 如果分母为负，调整符号
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
            // 尝试将整个字符串解析为整数
            int value = std::stoi(std::string(str));
            return Fraction(value, 1);
        } else {
            // 解析分子和分母
            std::string numeratorStr(str.substr(0, pos));
            std::string denominatorStr(str.substr(pos + 1));

            int n = std::stoi(numeratorStr);
            int d = std::stoi(denominatorStr);

            if (d == 0) {
                return std::nullopt;  // 分母不能为零
            }

            return Fraction(n, d);
        }
    } catch (...) {
        return std::nullopt;  // 解析失败或其他异常
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

    // 首先尝试读取分子
    if (!(is >> n)) {
        is.setstate(std::ios::failbit);
        throw FractionException("Failed to read numerator.");
    }

    // 检查下一个字符是否是分隔符'/'
    if (is.peek() == '/') {
        is.get(sep);  // 读取分隔符

        // 尝试读取分母
        if (!(is >> d)) {
            is.setstate(std::ios::failbit);
            throw FractionException("Failed to read denominator after '/'.");
        }

        if (d == 0) {
            is.setstate(std::ios::failbit);
            throw FractionException("Denominator cannot be zero.");
        }
    }

    // 设置分数值并约分
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

    // 处理零
    if (value == 0.0) {
        return Fraction(0, 1);
    }

    // 处理符号
    int sign = (value < 0) ? -1 : 1;
    value = std::abs(value);

    // 使用连分数算法实现更精确的近似
    double epsilon = 1.0 / max_denominator;
    int a = static_cast<int>(std::floor(value));
    double f = value - a;

    int h1 = 1, h2 = a;
    int k1 = 0, k2 = 1;

    while (f > epsilon && k2 < max_denominator) {
        double r = 1.0 / f;
        a = static_cast<int>(std::floor(r));
        f = r - a;

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