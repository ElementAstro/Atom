// rust_numeric.h
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>

#ifdef _WIN32
#undef NAN
#endif

namespace atom::algorithm {
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using isize = std::ptrdiff_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = std::size_t;

using f32 = float;
using f64 = double;

enum class ErrorKind {
    ParseIntError,
    ParseFloatError,
    DivideByZero,
    NumericOverflow,
    NumericUnderflow,
    InvalidOperation,
};

class Error {
private:
    ErrorKind m_kind;
    std::string m_message;

public:
    Error(ErrorKind kind, const std::string& message)
        : m_kind(kind), m_message(message) {}

    ErrorKind kind() const { return m_kind; }
    const std::string& message() const { return m_message; }

    std::string to_string() const {
        std::string kind_str;
        switch (m_kind) {
            case ErrorKind::ParseIntError:
                kind_str = "ParseIntError";
                break;
            case ErrorKind::ParseFloatError:
                kind_str = "ParseFloatError";
                break;
            case ErrorKind::DivideByZero:
                kind_str = "DivideByZero";
                break;
            case ErrorKind::NumericOverflow:
                kind_str = "NumericOverflow";
                break;
            case ErrorKind::NumericUnderflow:
                kind_str = "NumericUnderflow";
                break;
            case ErrorKind::InvalidOperation:
                kind_str = "InvalidOperation";
                break;
        }
        return kind_str + ": " + m_message;
    }
};

template <typename T>
class Result {
private:
    std::variant<T, Error> m_value;

public:
    Result(const T& value) : m_value(value) {}
    Result(const Error& error) : m_value(error) {}

    bool is_ok() const { return m_value.index() == 0; }
    bool is_err() const { return m_value.index() == 1; }

    const T& unwrap() const {
        if (is_ok()) {
            return std::get<0>(m_value);
        }
        throw std::runtime_error("Called unwrap() on an Err value: " +
                                 std::get<1>(m_value).to_string());
    }

    T unwrap_or(const T& default_value) const {
        if (is_ok()) {
            return std::get<0>(m_value);
        }
        return default_value;
    }

    const Error& unwrap_err() const {
        if (is_err()) {
            return std::get<1>(m_value);
        }
        throw std::runtime_error("Called unwrap_err() on an Ok value");
    }

    template <typename F>
    auto map(F&& f) const -> Result<decltype(f(std::declval<T>()))> {
        using U = decltype(f(std::declval<T>()));

        if (is_ok()) {
            return Result<U>(f(std::get<0>(m_value)));
        }
        return Result<U>(std::get<1>(m_value));
    }

    template <typename E>
    T unwrap_or_else(E&& e) const {
        if (is_ok()) {
            return std::get<0>(m_value);
        }
        return e(std::get<1>(m_value));
    }

    static Result<T> ok(const T& value) { return Result<T>(value); }

    static Result<T> err(ErrorKind kind, const std::string& message) {
        return Result<T>(Error(kind, message));
    }
};

template <typename T>
class Option {
private:
    bool m_has_value;
    T m_value;

public:
    Option() : m_has_value(false), m_value() {}
    explicit Option(T value) : m_has_value(true), m_value(value) {}

    bool has_value() const { return m_has_value; }
    bool is_some() const { return m_has_value; }
    bool is_none() const { return !m_has_value; }

    T value() const {
        if (!m_has_value) {
            throw std::runtime_error("Called value() on a None option");
        }
        return m_value;
    }

    T unwrap() const {
        if (!m_has_value) {
            throw std::runtime_error("Called unwrap() on a None option");
        }
        return m_value;
    }

    T unwrap_or(T default_value) const {
        return m_has_value ? m_value : default_value;
    }

    template <typename F>
    T unwrap_or_else(F&& f) const {
        return m_has_value ? m_value : f();
    }

    template <typename F>
    auto map(F&& f) const -> Option<decltype(f(std::declval<T>()))> {
        using U = decltype(f(std::declval<T>()));

        if (m_has_value) {
            return Option<U>(f(m_value));
        }
        return Option<U>();
    }

    template <typename F>
    auto and_then(F&& f) const -> decltype(f(std::declval<T>())) {
        using ReturnType = decltype(f(std::declval<T>()));

        if (m_has_value) {
            return f(m_value);
        }
        return ReturnType();
    }

    static Option<T> some(T value) { return Option<T>(value); }

    static Option<T> none() { return Option<T>(); }
};

template <typename T>
class Range {
private:
    T m_start;
    T m_end;
    bool m_inclusive;

public:
    class Iterator {
    private:
        T m_current;
        T m_end;
        bool m_inclusive;
        bool m_done;

    public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        using iterator_category = std::input_iterator_tag;

        Iterator(T start, T end, bool inclusive)
            : m_current(start),
              m_end(end),
              m_inclusive(inclusive),
              m_done(start > end || (start == end && !inclusive)) {}

        T operator*() const { return m_current; }

        Iterator& operator++() {
            if (m_current == m_end) {
                if (m_inclusive) {
                    m_done = true;
                    m_inclusive = false;
                }
            } else {
                ++m_current;
                m_done =
                    (m_current > m_end) || (m_current == m_end && !m_inclusive);
            }
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const {
            if (m_done && other.m_done)
                return true;
            if (m_done || other.m_done)
                return false;
            return m_current == other.m_current && m_end == other.m_end &&
                   m_inclusive == other.m_inclusive;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }
    };

    Range(T start, T end, bool inclusive = false)
        : m_start(start), m_end(end), m_inclusive(inclusive) {}

    Iterator begin() const { return Iterator(m_start, m_end, m_inclusive); }
    Iterator end() const { return Iterator(m_end, m_end, false); }

    bool contains(const T& value) const {
        if (m_inclusive) {
            return value >= m_start && value <= m_end;
        } else {
            return value >= m_start && value < m_end;
        }
    }

    usize len() const {
        if (m_start > m_end)
            return 0;
        usize length = static_cast<usize>(m_end - m_start);
        if (m_inclusive)
            length += 1;
        return length;
    }

    bool is_empty() const {
        return m_start >= m_end && !(m_inclusive && m_start == m_end);
    }
};

template <typename T>
Range<T> range(T start, T end) {
    return Range<T>(start, end, false);
}

template <typename T>
Range<T> range_inclusive(T start, T end) {
    return Range<T>(start, end, true);
}

template <typename Int, typename = std::enable_if_t<std::is_integral_v<Int>>>
class IntMethods {
public:
    static constexpr Int MIN = std::numeric_limits<Int>::min();
    static constexpr Int MAX = std::numeric_limits<Int>::max();

    template <typename ToInt>
    static Option<ToInt> try_into(Int value) {
        if (value < std::numeric_limits<ToInt>::min() ||
            value > std::numeric_limits<ToInt>::max()) {
            return Option<ToInt>::none();
        }
        return Option<ToInt>::some(static_cast<ToInt>(value));
    }

    static Option<Int> checked_add(Int a, Int b) {
        if ((b > 0 && a > MAX - b) || (b < 0 && a < MIN - b)) {
            return Option<Int>::none();
        }
        return Option<Int>::some(a + b);
    }

    static Option<Int> checked_sub(Int a, Int b) {
        if ((b > 0 && a < MIN + b) || (b < 0 && a > MAX + b)) {
            return Option<Int>::none();
        }
        return Option<Int>::some(a - b);
    }

    static Option<Int> checked_mul(Int a, Int b) {
        if (a == 0 || b == 0) {
            return Option<Int>::some(0);
        }
        if ((a > 0 && b > 0 && a > MAX / b) ||
            (a > 0 && b < 0 && b < MIN / a) ||
            (a < 0 && b > 0 && a < MIN / b) ||
            (a < 0 && b < 0 && a < MAX / b)) {
            return Option<Int>::none();
        }
        return Option<Int>::some(a * b);
    }

    static Option<Int> checked_div(Int a, Int b) {
        if (b == 0) {
            return Option<Int>::none();
        }
        if (a == MIN && b == -1) {
            return Option<Int>::none();
        }
        return Option<Int>::some(a / b);
    }

    static Option<Int> checked_rem(Int a, Int b) {
        if (b == 0) {
            return Option<Int>::none();
        }
        if (a == MIN && b == -1) {
            return Option<Int>::some(0);
        }
        return Option<Int>::some(a % b);
    }

    static Option<Int> checked_neg(Int a) {
        if (a == MIN) {
            return Option<Int>::none();
        }
        return Option<Int>::some(-a);
    }

    static Option<Int> checked_abs(Int a) {
        if (a == MIN) {
            return Option<Int>::none();
        }
        return Option<Int>::some(a < 0 ? -a : a);
    }

    static Option<Int> checked_pow(Int base, u32 exp) {
        if (exp == 0)
            return Option<Int>::some(1);
        if (base == 0)
            return Option<Int>::some(0);
        if (base == 1)
            return Option<Int>::some(1);
        if (base == -1)
            return Option<Int>::some(exp % 2 == 0 ? 1 : -1);

        Int result = 1;
        for (u32 i = 0; i < exp; ++i) {
            auto next = checked_mul(result, base);
            if (next.is_none())
                return Option<Int>::none();
            result = next.unwrap();
        }
        return Option<Int>::some(result);
    }

    static Option<Int> checked_shl(Int a, u32 shift) {
        const unsigned int bits = sizeof(Int) * 8;
        if (shift >= bits) {
            return Option<Int>::none();
        }

        if (a != 0 && shift > 0) {
            Int mask = MAX << (bits - shift);
            if ((a & mask) != 0 && (a & mask) != mask) {
                return Option<Int>::none();
            }
        }

        return Option<Int>::some(a << shift);
    }

    static Option<Int> checked_shr(Int a, u32 shift) {
        if (shift >= sizeof(Int) * 8) {
            return Option<Int>::none();
        }
        return Option<Int>::some(a >> shift);
    }

    static Int saturating_add(Int a, Int b) {
        auto result = checked_add(a, b);
        if (result.is_none()) {
            return b > 0 ? MAX : MIN;
        }
        return result.unwrap();
    }

    static Int saturating_sub(Int a, Int b) {
        auto result = checked_sub(a, b);
        if (result.is_none()) {
            return b > 0 ? MIN : MAX;
        }
        return result.unwrap();
    }

    static Int saturating_mul(Int a, Int b) {
        auto result = checked_mul(a, b);
        if (result.is_none()) {
            if ((a > 0 && b > 0) || (a < 0 && b < 0)) {
                return MAX;
            } else {
                return MIN;
            }
        }
        return result.unwrap();
    }

    static Int saturating_pow(Int base, u32 exp) {
        auto result = checked_pow(base, exp);
        if (result.is_none()) {
            if (base > 0) {
                return MAX;
            } else if (exp % 2 == 0) {
                return MAX;
            } else {
                return MIN;
            }
        }
        return result.unwrap();
    }

    static Int saturating_abs(Int a) {
        auto result = checked_abs(a);
        if (result.is_none()) {
            return MAX;
        }
        return result.unwrap();
    }

    static Int wrapping_add(Int a, Int b) {
        return static_cast<Int>(
            static_cast<typename std::make_unsigned<Int>::type>(a) +
            static_cast<typename std::make_unsigned<Int>::type>(b));
    }

    static Int wrapping_sub(Int a, Int b) {
        return static_cast<Int>(
            static_cast<typename std::make_unsigned<Int>::type>(a) -
            static_cast<typename std::make_unsigned<Int>::type>(b));
    }

    static Int wrapping_mul(Int a, Int b) {
        return static_cast<Int>(
            static_cast<typename std::make_unsigned<Int>::type>(a) *
            static_cast<typename std::make_unsigned<Int>::type>(b));
    }

    static Int wrapping_div(Int a, Int b) {
        if (b == 0) {
            throw std::runtime_error("Division by zero");
        }
        if (a == MIN && b == -1) {
            return MIN;
        }
        return a / b;
    }

    static Int wrapping_rem(Int a, Int b) {
        if (b == 0) {
            throw std::runtime_error("Division by zero");
        }
        if (a == MIN && b == -1) {
            return 0;
        }
        return a % b;
    }

    static Int wrapping_neg(Int a) {
        return static_cast<Int>(
            -static_cast<typename std::make_unsigned<Int>::type>(a));
    }

    static Int wrapping_abs(Int a) {
        if (a == MIN) {
            return MIN;
        }
        return a < 0 ? -a : a;
    }

    static Int wrapping_pow(Int base, u32 exp) {
        Int result = 1;
        for (u32 i = 0; i < exp; ++i) {
            result = wrapping_mul(result, base);
        }
        return result;
    }

    static Int wrapping_shl(Int a, u32 shift) {
        const unsigned int bits = sizeof(Int) * 8;
        if (shift >= bits) {
            shift %= bits;
        }
        return a << shift;
    }

    static Int wrapping_shr(Int a, u32 shift) {
        const unsigned int bits = sizeof(Int) * 8;
        if (shift >= bits) {
            shift %= bits;
        }
        return a >> shift;
    }

    static constexpr Int rotate_left(Int value, unsigned int shift) {
        constexpr unsigned int bits = sizeof(Int) * 8;
        shift %= bits;
        if (shift == 0)
            return value;
        return static_cast<Int>((value << shift) | (value >> (bits - shift)));
    }

    static constexpr Int rotate_right(Int value, unsigned int shift) {
        constexpr unsigned int bits = sizeof(Int) * 8;
        shift %= bits;
        if (shift == 0)
            return value;
        return static_cast<Int>((value >> shift) | (value << (bits - shift)));
    }

    static constexpr int count_ones(Int value) {
        typename std::make_unsigned<Int>::type uval = value;
        int count = 0;
        while (uval) {
            count += uval & 1;
            uval >>= 1;
        }
        return count;
    }

    static constexpr int count_zeros(Int value) {
        return sizeof(Int) * 8 - count_ones(value);
    }

    static constexpr int leading_zeros(Int value) {
        if (value == 0)
            return sizeof(Int) * 8;

        typename std::make_unsigned<Int>::type uval = value;
        int zeros = 0;
        const int total_bits = sizeof(Int) * 8;

        for (int i = total_bits - 1; i >= 0; --i) {
            if ((uval & (static_cast<typename std::make_unsigned<Int>::type>(1)
                         << i)) == 0) {
                zeros++;
            } else {
                break;
            }
        }

        return zeros;
    }

    static constexpr int trailing_zeros(Int value) {
        if (value == 0)
            return sizeof(Int) * 8;

        typename std::make_unsigned<Int>::type uval = value;
        int zeros = 0;

        while ((uval & 1) == 0) {
            zeros++;
            uval >>= 1;
        }

        return zeros;
    }

    static constexpr int leading_ones(Int value) {
        typename std::make_unsigned<Int>::type uval = value;
        int ones = 0;
        const int total_bits = sizeof(Int) * 8;

        for (int i = total_bits - 1; i >= 0; --i) {
            if ((uval & (static_cast<typename std::make_unsigned<Int>::type>(1)
                         << i)) != 0) {
                ones++;
            } else {
                break;
            }
        }

        return ones;
    }

    static constexpr int trailing_ones(Int value) {
        typename std::make_unsigned<Int>::type uval = value;
        int ones = 0;

        while ((uval & 1) != 0) {
            ones++;
            uval >>= 1;
        }

        return ones;
    }

    static constexpr Int reverse_bits(Int value) {
        typename std::make_unsigned<Int>::type uval = value;
        typename std::make_unsigned<Int>::type result = 0;
        const int total_bits = sizeof(Int) * 8;

        for (int i = 0; i < total_bits; ++i) {
            result = (result << 1) | (uval & 1);
            uval >>= 1;
        }

        return static_cast<Int>(result);
    }

    static constexpr Int swap_bytes(Int value) {
        typename std::make_unsigned<Int>::type uval = value;
        typename std::make_unsigned<Int>::type result = 0;
        const int byte_count = sizeof(Int);

        for (int i = 0; i < byte_count; ++i) {
            result |= ((uval >> (i * 8)) & 0xFF) << ((byte_count - 1 - i) * 8);
        }

        return static_cast<Int>(result);
    }

    static Int min(Int a, Int b) { return a < b ? a : b; }

    static Int max(Int a, Int b) { return a > b ? a : b; }

    static Int clamp(Int value, Int min, Int max) {
        if (value < min)
            return min;
        if (value > max)
            return max;
        return value;
    }

    static Int abs_diff(Int a, Int b) {
        if (a >= b)
            return a - b;
        return b - a;
    }

    static bool is_power_of_two(Int value) {
        return value > 0 && (value & (value - 1)) == 0;
    }

    static Int next_power_of_two(Int value) {
        if (value <= 0)
            return 1;

        const int bit_shift = sizeof(Int) * 8 - 1 - leading_zeros(value - 1);

        if (bit_shift >= sizeof(Int) * 8 - 1)
            return 0;

        return 1 << (bit_shift + 1);
    }

    static std::string to_string(Int value, int base = 10) {
        if (base < 2 || base > 36) {
            throw std::invalid_argument("Base must be between 2 and 36");
        }

        if (value == 0)
            return "0";

        bool negative = value < 0;
        typename std::make_unsigned<Int>::type abs_value =
            negative
                ? -static_cast<typename std::make_unsigned<Int>::type>(value)
                : value;

        std::string result;
        while (abs_value > 0) {
            int digit = abs_value % base;
            char digit_char;
            if (digit < 10) {
                digit_char = '0' + digit;
            } else {
                digit_char = 'a' + (digit - 10);
            }
            result = digit_char + result;
            abs_value /= base;
        }

        if (negative) {
            result = "-" + result;
        }

        return result;
    }

    static std::string to_hex_string(Int value, bool with_prefix = true) {
        std::ostringstream oss;
        if (with_prefix)
            oss << "0x";
        oss << std::hex
            << static_cast<typename std::conditional<
                   sizeof(Int) <= sizeof(int),
                   typename std::conditional<std::is_signed<Int>::value, int,
                                             unsigned int>::type,
                   typename std::conditional<
                       std::is_signed<Int>::value, Int,
                       typename std::make_unsigned<Int>::type>::type>::type>(
                   value);
        return oss.str();
    }

    static std::string to_bin_string(Int value, bool with_prefix = true) {
        if (value == 0)
            return with_prefix ? "0b0" : "0";

        std::string result;
        typename std::make_unsigned<Int>::type uval = value;

        while (uval > 0) {
            result = (uval & 1 ? '1' : '0') + result;
            uval >>= 1;
        }

        if (with_prefix) {
            result = "0b" + result;
        }

        return result;
    }

    static Result<Int> from_str_radix(const std::string& s, int radix) {
        try {
            if (radix < 2 || radix > 36) {
                return Result<Int>::err(ErrorKind::ParseIntError,
                                        "Radix must be between 2 and 36");
            }

            if (s.empty()) {
                return Result<Int>::err(ErrorKind::ParseIntError,
                                        "Cannot parse empty string");
            }

            size_t start_idx = 0;
            bool negative = false;

            if (s[0] == '+') {
                start_idx = 1;
            } else if (s[0] == '-') {
                negative = true;
                start_idx = 1;
            }

            if (start_idx >= s.length()) {
                return Result<Int>::err(
                    ErrorKind::ParseIntError,
                    "String contains only a sign with no digits");
            }

            if (s.length() > start_idx + 2 && s[start_idx] == '0') {
                char prefix = std::tolower(s[start_idx + 1]);
                if ((prefix == 'x' && radix == 16) ||
                    (prefix == 'b' && radix == 2) ||
                    (prefix == 'o' && radix == 8)) {
                    start_idx += 2;
                }
            }

            if (start_idx >= s.length()) {
                return Result<Int>::err(ErrorKind::ParseIntError,
                                        "String contains prefix but no digits");
            }

            typename std::make_unsigned<Int>::type result = 0;
            for (size_t i = start_idx; i < s.length(); ++i) {
                char c = s[i];
                int digit;

                if (c >= '0' && c <= '9') {
                    digit = c - '0';
                } else if (c >= 'a' && c <= 'z') {
                    digit = c - 'a' + 10;
                } else if (c >= 'A' && c <= 'Z') {
                    digit = c - 'A' + 10;
                } else if (c == '_' && i > start_idx && i < s.length() - 1) {
                    continue;
                } else {
                    return Result<Int>::err(ErrorKind::ParseIntError,
                                            "Invalid character in string");
                }

                if (digit >= radix) {
                    return Result<Int>::err(
                        ErrorKind::ParseIntError,
                        "Digit out of range for given radix");
                }

                // 检查溢出
                if (result >
                    (static_cast<typename std::make_unsigned<Int>::type>(MAX) -
                     digit) /
                        radix) {
                    return Result<Int>::err(ErrorKind::ParseIntError,
                                            "Overflow occurred during parsing");
                }

                result = result * radix + digit;
            }

            if (negative) {
                if (result >
                    static_cast<typename std::make_unsigned<Int>::type>(MAX) +
                        1) {
                    return Result<Int>::err(
                        ErrorKind::ParseIntError,
                        "Overflow occurred when negating value");
                }

                return Result<Int>::ok(static_cast<Int>(
                    -static_cast<typename std::make_unsigned<Int>::type>(
                        result)));
            } else {
                if (result >
                    static_cast<typename std::make_unsigned<Int>::type>(MAX)) {
                    return Result<Int>::err(
                        ErrorKind::ParseIntError,
                        "Value too large for the integer type");
                }

                return Result<Int>::ok(static_cast<Int>(result));
            }
        } catch (const std::exception& e) {
            return Result<Int>::err(ErrorKind::ParseIntError, e.what());
        }
    }

    static Int random(Int min = MIN, Int max = MAX) {
        static std::random_device rd;
        static std::mt19937 gen(rd());

        if (min > max) {
            std::swap(min, max);
        }

        using DistType = std::conditional_t<std::is_signed_v<Int>,
                                            std::uniform_int_distribution<Int>,
                                            std::uniform_int_distribution<Int>>;

        DistType dist(min, max);
        return dist(gen);
    }

    static std::tuple<Int, Int> div_rem(Int a, Int b) {
        if (b == 0) {
            throw std::runtime_error("Division by zero");
        }

        Int q = a / b;
        Int r = a % b;
        return {q, r};
    }

    static Int gcd(Int a, Int b) {
        a = abs(a);
        b = abs(b);

        while (b != 0) {
            Int t = b;
            b = a % b;
            a = t;
        }

        return a;
    }

    static Int lcm(Int a, Int b) {
        if (a == 0 || b == 0)
            return 0;

        a = abs(a);
        b = abs(b);

        Int g = gcd(a, b);
        return a / g * b;
    }

    static Int abs(Int a) {
        if (a < 0) {
            if (a == MIN) {
                throw std::runtime_error("Absolute value of MIN overflows");
            }
            return -a;
        }
        return a;
    }
};

template <typename Float,
          typename = std::enable_if_t<std::is_floating_point_v<Float>>>
class FloatMethods {
public:
    static constexpr Float INFINITY_VAL =
        std::numeric_limits<Float>::infinity();
    static constexpr Float NEG_INFINITY =
        -std::numeric_limits<Float>::infinity();
    static constexpr Float NAN = std::numeric_limits<Float>::quiet_NaN();
    static constexpr Float MIN = std::numeric_limits<Float>::lowest();
    static constexpr Float MAX = std::numeric_limits<Float>::max();
    static constexpr Float EPSILON = std::numeric_limits<Float>::epsilon();
    static constexpr Float PI = static_cast<Float>(3.14159265358979323846);
    static constexpr Float TAU = PI * 2;
    static constexpr Float E = static_cast<Float>(2.71828182845904523536);
    static constexpr Float SQRT_2 = static_cast<Float>(1.41421356237309504880);
    static constexpr Float LN_2 = static_cast<Float>(0.69314718055994530942);
    static constexpr Float LN_10 = static_cast<Float>(2.30258509299404568402);

    template <typename ToType>
    static Option<ToType> try_into(Float value) {
        if (std::is_integral_v<ToType>) {
            if (value <
                    static_cast<Float>(std::numeric_limits<ToType>::min()) ||
                value >
                    static_cast<Float>(std::numeric_limits<ToType>::max()) ||
                std::isnan(value)) {
                return Option<ToType>::none();
            }
            return Option<ToType>::some(static_cast<ToType>(value));
        } else if (std::is_floating_point_v<ToType>) {
            if (value < std::numeric_limits<ToType>::lowest() ||
                value > std::numeric_limits<ToType>::max()) {
                return Option<ToType>::none();
            }
            return Option<ToType>::some(static_cast<ToType>(value));
        }
        return Option<ToType>::none();
    }

    static bool is_nan(Float x) { return std::isnan(x); }

    static bool is_infinite(Float x) { return std::isinf(x); }

    static bool is_finite(Float x) { return std::isfinite(x); }

    static bool is_normal(Float x) { return std::isnormal(x); }

    static bool is_subnormal(Float x) {
        return std::fpclassify(x) == FP_SUBNORMAL;
    }

    static bool is_sign_positive(Float x) { return std::signbit(x) == 0; }

    static bool is_sign_negative(Float x) { return std::signbit(x) != 0; }

    static Float abs(Float x) { return std::abs(x); }

    static Float floor(Float x) { return std::floor(x); }

    static Float ceil(Float x) { return std::ceil(x); }

    static Float round(Float x) { return std::round(x); }

    static Float trunc(Float x) { return std::trunc(x); }

    static Float fract(Float x) { return x - std::floor(x); }

    static Float sqrt(Float x) { return std::sqrt(x); }

    static Float cbrt(Float x) { return std::cbrt(x); }

    static Float exp(Float x) { return std::exp(x); }

    static Float exp2(Float x) { return std::exp2(x); }

    static Float ln(Float x) { return std::log(x); }

    static Float log2(Float x) { return std::log2(x); }

    static Float log10(Float x) { return std::log10(x); }

    static Float log(Float x, Float base) {
        return std::log(x) / std::log(base);
    }

    static Float pow(Float x, Float y) { return std::pow(x, y); }

    static Float sin(Float x) { return std::sin(x); }

    static Float cos(Float x) { return std::cos(x); }

    static Float tan(Float x) { return std::tan(x); }

    static Float asin(Float x) { return std::asin(x); }

    static Float acos(Float x) { return std::acos(x); }

    static Float atan(Float x) { return std::atan(x); }

    static Float atan2(Float y, Float x) { return std::atan2(y, x); }

    static Float sinh(Float x) { return std::sinh(x); }

    static Float cosh(Float x) { return std::cosh(x); }

    static Float tanh(Float x) { return std::tanh(x); }

    static Float asinh(Float x) { return std::asinh(x); }

    static Float acosh(Float x) { return std::acosh(x); }

    static Float atanh(Float x) { return std::atanh(x); }

    static bool approx_eq(Float a, Float b, Float epsilon = EPSILON) {
        if (a == b)
            return true;

        Float diff = abs(a - b);
        if (a == 0 || b == 0 || diff < std::numeric_limits<Float>::min()) {
            return diff < epsilon;
        }

        return diff / (abs(a) + abs(b)) < epsilon;
    }

    static int total_cmp(Float a, Float b) {
        if (is_nan(a) && is_nan(b))
            return 0;
        if (is_nan(a))
            return 1;
        if (is_nan(b))
            return -1;

        if (a < b)
            return -1;
        if (a > b)
            return 1;
        return 0;
    }

    static Float min(Float a, Float b) {
        if (is_nan(a))
            return b;
        if (is_nan(b))
            return a;
        return a < b ? a : b;
    }

    static Float max(Float a, Float b) {
        if (is_nan(a))
            return b;
        if (is_nan(b))
            return a;
        return a > b ? a : b;
    }

    static Float clamp(Float value, Float min, Float max) {
        if (is_nan(value))
            return min;
        if (value < min)
            return min;
        if (value > max)
            return max;
        return value;
    }

    static std::string to_string(Float value, int precision = 6) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << value;
        return oss.str();
    }

    static std::string to_exp_string(Float value, int precision = 6) {
        std::ostringstream oss;
        oss << std::scientific << std::setprecision(precision) << value;
        return oss.str();
    }

    static Result<Float> from_str(const std::string& s) {
        try {
            size_t pos;
            if constexpr (std::is_same_v<Float, float>) {
                float val = std::stof(s, &pos);
                if (pos != s.length()) {
                    return Result<Float>::err(ErrorKind::ParseFloatError,
                                              "Failed to parse entire string");
                }
                return Result<Float>::ok(val);
            } else if constexpr (std::is_same_v<Float, double>) {
                double val = std::stod(s, &pos);
                if (pos != s.length()) {
                    return Result<Float>::err(ErrorKind::ParseFloatError,
                                              "Failed to parse entire string");
                }
                return Result<Float>::ok(val);
            } else {
                long double val = std::stold(s, &pos);
                if (pos != s.length()) {
                    return Result<Float>::err(ErrorKind::ParseFloatError,
                                              "Failed to parse entire string");
                }
                return Result<Float>::ok(static_cast<Float>(val));
            }
        } catch (const std::exception& e) {
            return Result<Float>::err(ErrorKind::ParseFloatError, e.what());
        }
    }

    static Float random(Float min = 0.0, Float max = 1.0) {
        static std::random_device rd;
        static std::mt19937 gen(rd());

        if (min > max) {
            std::swap(min, max);
        }

        std::uniform_real_distribution<Float> dist(min, max);
        return dist(gen);
    }

    static std::tuple<Float, Float> modf(Float x) {
        Float int_part;
        Float frac_part = std::modf(x, &int_part);
        return {int_part, frac_part};
    }

    static Float copysign(Float x, Float y) { return std::copysign(x, y); }

    static Float next_up(Float x) { return std::nextafter(x, INFINITY_VAL); }

    static Float next_down(Float x) { return std::nextafter(x, NEG_INFINITY); }

    static Float ulp(Float x) { return next_up(x) - x; }

    static Float to_radians(Float degrees) { return degrees * PI / 180.0f; }

    static Float to_degrees(Float radians) { return radians * 180.0f / PI; }

    static Float hypot(Float x, Float y) { return std::hypot(x, y); }

    static Float hypot(Float x, Float y, Float z) {
        return std::sqrt(x * x + y * y + z * z);
    }

    static Float lerp(Float a, Float b, Float t) { return a + t * (b - a); }

    static Float sign(Float x) {
        if (x > 0)
            return 1.0;
        if (x < 0)
            return -1.0;
        return 0.0;
    }
};

class I8 : public IntMethods<i8> {
public:
    static Result<i8> from_str(const std::string& s, int base = 10) {
        return from_str_radix(s, base);
    }
};

class I16 : public IntMethods<i16> {
public:
    static Result<i16> from_str(const std::string& s, int base = 10) {
        return from_str_radix(s, base);
    }
};

class I32 : public IntMethods<i32> {
public:
    static Result<i32> from_str(const std::string& s, int base = 10) {
        return from_str_radix(s, base);
    }
};

class I64 : public IntMethods<i64> {
public:
    static Result<i64> from_str(const std::string& s, int base = 10) {
        return from_str_radix(s, base);
    }
};

class U8 : public IntMethods<u8> {
public:
    static Result<u8> from_str(const std::string& s, int base = 10) {
        return from_str_radix(s, base);
    }
};

class U16 : public IntMethods<u16> {
public:
    static Result<u16> from_str(const std::string& s, int base = 10) {
        return from_str_radix(s, base);
    }
};

class U32 : public IntMethods<u32> {
public:
    static Result<u32> from_str(const std::string& s, int base = 10) {
        return from_str_radix(s, base);
    }
};

class U64 : public IntMethods<u64> {
public:
    static Result<u64> from_str(const std::string& s, int base = 10) {
        return from_str_radix(s, base);
    }
};

class Isize : public IntMethods<isize> {
public:
    static Result<isize> from_str(const std::string& s, int base = 10) {
        return from_str_radix(s, base);
    }
};

class Usize : public IntMethods<usize> {
public:
    static Result<usize> from_str(const std::string& s, int base = 10) {
        return from_str_radix(s, base);
    }
};

class F32 : public FloatMethods<f32> {
public:
    static Result<f32> from_str(const std::string& s) {
        return FloatMethods<f32>::from_str(s);
    }
};

class F64 : public FloatMethods<f64> {
public:
    static Result<f64> from_str(const std::string& s) {
        return FloatMethods<f64>::from_str(s);
    }
};

enum class Ordering { Less, Equal, Greater };

template <typename T>
class Ord {
public:
    static Ordering compare(const T& a, const T& b) {
        if (a < b)
            return Ordering::Less;
        if (a > b)
            return Ordering::Greater;
        return Ordering::Equal;
    }

    class Comparator {
    public:
        bool operator()(const T& a, const T& b) const {
            return compare(a, b) == Ordering::Less;
        }
    };

    template <typename F>
    static auto by_key(F&& key_fn) {
        class ByKey {
        private:
            F m_key_fn;

        public:
            ByKey(F key_fn) : m_key_fn(std::move(key_fn)) {}

            bool operator()(const T& a, const T& b) const {
                auto a_key = m_key_fn(a);
                auto b_key = m_key_fn(b);
                return a_key < b_key;
            }
        };

        return ByKey(std::forward<F>(key_fn));
    }
};

template <typename Iter, typename Func>
class MapIterator {
private:
    Iter m_iter;
    Func m_func;

public:
    using iterator_category =
        typename std::iterator_traits<Iter>::iterator_category;
    using difference_type =
        typename std::iterator_traits<Iter>::difference_type;
    using value_type = decltype(std::declval<Func>()(*std::declval<Iter>()));
    using pointer = value_type*;
    using reference = value_type&;

    MapIterator(Iter iter, Func func) : m_iter(iter), m_func(func) {}

    value_type operator*() const { return m_func(*m_iter); }

    MapIterator& operator++() {
        ++m_iter;
        return *this;
    }

    MapIterator operator++(int) {
        MapIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    bool operator==(const MapIterator& other) const {
        return m_iter == other.m_iter;
    }

    bool operator!=(const MapIterator& other) const {
        return !(*this == other);
    }
};

template <typename Container, typename Func>
class Map {
private:
    Container& m_container;
    Func m_func;

public:
    Map(Container& container, Func func)
        : m_container(container), m_func(func) {}

    auto begin() { return MapIterator(m_container.begin(), m_func); }

    auto end() { return MapIterator(m_container.end(), m_func); }
};

template <typename Container, typename Func>
Map<Container, Func> map(Container& container, Func func) {
    return Map<Container, Func>(container, func);
}

template <typename Iter, typename Pred>
class FilterIterator {
private:
    Iter m_iter;
    Iter m_end;
    Pred m_pred;

    void find_next_valid() {
        while (m_iter != m_end && !m_pred(*m_iter)) {
            ++m_iter;
        }
    }

public:
    using iterator_category = std::input_iterator_tag;
    using value_type = typename std::iterator_traits<Iter>::value_type;
    using difference_type =
        typename std::iterator_traits<Iter>::difference_type;
    using pointer = typename std::iterator_traits<Iter>::pointer;
    using reference = typename std::iterator_traits<Iter>::reference;

    FilterIterator(Iter begin, Iter end, Pred pred)
        : m_iter(begin), m_end(end), m_pred(pred) {
        find_next_valid();
    }

    reference operator*() const { return *m_iter; }

    pointer operator->() const { return &(*m_iter); }

    FilterIterator& operator++() {
        if (m_iter != m_end) {
            ++m_iter;
            find_next_valid();
        }
        return *this;
    }

    FilterIterator operator++(int) {
        FilterIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    bool operator==(const FilterIterator& other) const {
        return m_iter == other.m_iter;
    }

    bool operator!=(const FilterIterator& other) const {
        return !(*this == other);
    }
};

template <typename Container, typename Pred>
class Filter {
private:
    Container& m_container;
    Pred m_pred;

public:
    Filter(Container& container, Pred pred)
        : m_container(container), m_pred(pred) {}

    auto begin() {
        return FilterIterator(m_container.begin(), m_container.end(), m_pred);
    }

    auto end() {
        return FilterIterator(m_container.end(), m_container.end(), m_pred);
    }
};

template <typename Container, typename Pred>
Filter<Container, Pred> filter(Container& container, Pred pred) {
    return Filter<Container, Pred>(container, pred);
}

template <typename Iter>
class EnumerateIterator {
private:
    Iter m_iter;
    size_t m_index;

public:
    using iterator_category =
        typename std::iterator_traits<Iter>::iterator_category;
    using difference_type =
        typename std::iterator_traits<Iter>::difference_type;
    using value_type =
        std::pair<size_t, typename std::iterator_traits<Iter>::reference>;
    using pointer = value_type*;
    using reference = value_type;

    EnumerateIterator(Iter iter, size_t index = 0)
        : m_iter(iter), m_index(index) {}

    reference operator*() const { return {m_index, *m_iter}; }

    EnumerateIterator& operator++() {
        ++m_iter;
        ++m_index;
        return *this;
    }

    EnumerateIterator operator++(int) {
        EnumerateIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    bool operator==(const EnumerateIterator& other) const {
        return m_iter == other.m_iter;
    }

    bool operator!=(const EnumerateIterator& other) const {
        return !(*this == other);
    }
};

template <typename Container>
class Enumerate {
private:
    Container& m_container;

public:
    explicit Enumerate(Container& container) : m_container(container) {}

    auto begin() { return EnumerateIterator(m_container.begin()); }

    auto end() { return EnumerateIterator(m_container.end()); }
};

template <typename Container>
Enumerate<Container> enumerate(Container& container) {
    return Enumerate<Container>(container);
}
}  // namespace atom::algorithm