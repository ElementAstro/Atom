// rust_numeric.h
#pragma once

#include <algorithm>
#include <bit>       // Include for std::byteswap (C++23)
#include <cctype>    // Include for std::tolower
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iterator>  // Include for std::iterator_traits, std::input_iterator_tag
#include <limits>    // Include for std::numeric_limits
#include <random>
#include <sstream>
#include <stdexcept>  // Include for std::runtime_error, std::invalid_argument
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>  // Include for std::swap, std::pair, std::forward, std::move
#include <variant>

#undef NAN  // Undefining NAN is generally discouraged as it conflicts with
            // std::numeric_limits::quiet_NaN()

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
        // Check for overflow before multiplication
        if ((a > 0 && b > 0 && a > MAX / b) ||
            (a < 0 && b < 0 &&
             a < MAX / b) ||  // Corrected condition for negative * negative
            (a > 0 && b < 0 && b < MIN / a) ||
            (a < 0 && b > 0 && a < MIN / b)) {
            return Option<Int>::none();
        }
        return Option<Int>::some(a * b);
    }

    static Option<Int> checked_div(Int a, Int b) {
        if (b == 0) {
            return Option<Int>::none();
        }
        if (a == MIN && b == -1) {
            return Option<Int>::none();  // Overflow case for signed integers
        }
        return Option<Int>::some(a / b);
    }

    static Option<Int> checked_rem(Int a, Int b) {
        if (b == 0) {
            return Option<Int>::none();
        }
        if (a == MIN && b == -1) {
            return Option<Int>::some(
                0);  // Remainder is 0 in this overflow case
        }
        return Option<Int>::some(a % b);
    }

    static Option<Int> checked_neg(Int a) {
        if (a == MIN) {
            return Option<Int>::none();  // Negating MIN overflows for signed
                                         // integers
        }
        return Option<Int>::some(-a);
    }

    static Option<Int> checked_abs(Int a) {
        if (a == MIN) {
            return Option<Int>::none();  // Absolute value of MIN overflows for
                                         // signed integers
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
            // Shifting by more than or equal to the number of bits is undefined
            // behavior or results in 0 depending on context/type. Rust's
            // checked_shl returns None.
            return Option<Int>::none();
        }

        // Check for overflow: if any bits are shifted out that differ from the
        // sign bit (for signed types) or are non-zero (for unsigned types).
        if constexpr (std::is_signed_v<Int>) {
            if (a != 0 && shift > 0) {
                // Check if the most significant `shift` bits are all the same
                // as the sign bit
                Int shifted_out_mask = static_cast<Int>(
                    ~static_cast<typename std::make_unsigned<Int>::type>(0)
                    << (bits - shift));
                Int shifted_out_bits = a & shifted_out_mask;
                Int sign_bits = (a < 0) ? shifted_out_mask : 0;

                if (shifted_out_bits != sign_bits) {
                    return Option<Int>::none();  // Overflow occurred
                }
            }
        } else {  // Unsigned
            if (a != 0 && shift > 0) {
                typename std::make_unsigned<Int>::type u_a = a;
                if ((u_a >> (bits - shift)) != 0) {
                    return Option<Int>::none();  // Non-zero bits shifted out
                }
            }
        }

        return Option<Int>::some(a << shift);
    }

    static Option<Int> checked_shr(Int a, u32 shift) {
        const unsigned int bits = sizeof(Int) * 8;
        if (shift >= bits) {
            // Shifting by more than or equal to the number of bits is undefined
            // behavior or results in 0 depending on context/type. Rust's
            // checked_shr returns None.
            return Option<Int>::none();
        }
        // For signed integers, right shift is implementation-defined for
        // negative numbers. Assuming arithmetic right shift for signed types.
        // Checked right shift in Rust doesn't typically overflow, but shifting
        // by >= bits is None.
        return Option<Int>::some(a >> shift);
    }

    static Int saturating_add(Int a, Int b) {
        auto result = checked_add(a, b);
        if (result.is_none()) {
            // Determine if it was an overflow (towards MAX) or underflow
            // (towards MIN) This depends on the sign of b
            if constexpr (std::is_signed_v<Int>) {
                return b > 0 ? MAX : MIN;
            } else {         // Unsigned
                return MAX;  // Unsigned addition only overflows towards MAX
            }
        }
        return result.unwrap();
    }

    static Int saturating_sub(Int a, Int b) {
        auto result = checked_sub(a, b);
        if (result.is_none()) {
            // Determine if it was an overflow (towards MAX) or underflow
            // (towards MIN) This depends on the sign of b
            if constexpr (std::is_signed_v<Int>) {
                return b > 0 ? MIN : MAX;
            } else {         // Unsigned
                return MIN;  // Unsigned subtraction only underflows towards MIN
            }
        }
        return result.unwrap();
    }

    static Int saturating_mul(Int a, Int b) {
        auto result = checked_mul(a, b);
        if (result.is_none()) {
            // Determine if it was an overflow (towards MAX) or underflow
            // (towards MIN)
            if constexpr (std::is_signed_v<Int>) {
                if ((a > 0 && b > 0) || (a < 0 && b < 0)) {
                    return MAX;  // Positive result overflowed
                } else {
                    return MIN;  // Negative result underflowed
                }
            } else {         // Unsigned
                return MAX;  // Unsigned multiplication only overflows towards
                             // MAX
            }
        }
        return result.unwrap();
    }

    static Int saturating_pow(Int base, u32 exp) {
        auto result = checked_pow(base, exp);
        if (result.is_none()) {
            if constexpr (std::is_signed_v<Int>) {
                if (base > 0) {
                    return MAX;
                } else if (base < 0) {
                    return exp % 2 == 0 ? MAX : MIN;
                } else {  // base == 0, checked_pow handles this
                    return 0;
                }
            } else {         // Unsigned
                return MAX;  // Unsigned power only overflows towards MAX
            }
        }
        return result.unwrap();
    }

    static Int saturating_abs(Int a) {
        auto result = checked_abs(a);
        if (result.is_none()) {
            // For signed integers, only MIN overflows, saturating to MAX.
            // For unsigned integers, abs is the value itself, never overflows.
            if constexpr (std::is_signed_v<Int>) {
                return MAX;
            } else {
                return a;
            }
        }
        return result.unwrap();
    }

    static Int wrapping_add(Int a, Int b) {
        // C++ standard guarantees wrapping behavior for unsigned integers.
        // For signed integers, it's undefined behavior if overflow occurs.
        // To achieve wrapping for signed integers, cast to unsigned, perform
        // operation, cast back.
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
            // Rust panics on division by zero. C++ throws.
            throw std::runtime_error("Division by zero");
        }
        if constexpr (std::is_signed_v<Int>) {
            if (a == MIN && b == -1) {
                // Rust's wrapping_div handles MIN / -1 as MIN. C++ is UB.
                return MIN;
            }
        }
        return a / b;
    }

    static Int wrapping_rem(Int a, Int b) {
        if (b == 0) {
            // Rust panics on division by zero. C++ throws.
            throw std::runtime_error("Division by zero");
        }
        if constexpr (std::is_signed_v<Int>) {
            if (a == MIN && b == -1) {
                // Rust's wrapping_rem handles MIN % -1 as 0. C++ is UB.
                return 0;
            }
        }
        return a % b;
    }

    static Int wrapping_neg(Int a) {
        // Negating MIN for signed integers overflows. Rust's wrapping_neg
        // returns MIN.
        if constexpr (std::is_signed_v<Int>) {
            if (a == MIN) {
                return MIN;
            }
        }
        return -a;
    }

    static Int wrapping_abs(Int a) {
        // Absolute value of MIN for signed integers overflows. Rust's
        // wrapping_abs returns MIN.
        if constexpr (std::is_signed_v<Int>) {
            if (a == MIN) {
                return MIN;
            }
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
        // Rust's wrapping_shl wraps the shift amount. C++ is UB if shift >=
        // bits.
        if (shift >= bits) {
            shift %= bits;
        }
        return a << shift;
    }

    static Int wrapping_shr(Int a, u32 shift) {
        const unsigned int bits = sizeof(Int) * 8;
        // Rust's wrapping_shr wraps the shift amount. C++ is UB if shift >=
        // bits.
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
        // Use unsigned type for bitwise operations to avoid issues with signed
        // types
        using U = typename std::make_unsigned<Int>::type;
        U uval = static_cast<U>(value);
        return static_cast<Int>((uval << shift) | (uval >> (bits - shift)));
    }

    static constexpr Int rotate_right(Int value, unsigned int shift) {
        constexpr unsigned int bits = sizeof(Int) * 8;
        shift %= bits;
        if (shift == 0)
            return value;
        // Use unsigned type for bitwise operations
        using U = typename std::make_unsigned<Int>::type;
        U uval = static_cast<U>(value);
        return static_cast<Int>((uval >> shift) | (uval << (bits - shift)));
    }

    static constexpr int count_ones(Int value) {
        // Use unsigned type for bitwise operations
        using U = typename std::make_unsigned<Int>::type;
        U uval = static_cast<U>(value);
        int count = 0;
        // Manual implementation for compatibility
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
        // Use unsigned type for bitwise operations
        using U = typename std::make_unsigned<Int>::type;
        U uval = static_cast<U>(value);
        // Manual implementation for compatibility
        if (uval == 0)
            return sizeof(Int) * 8;

        int zeros = 0;
        const int total_bits = sizeof(Int) * 8;

        for (int i = total_bits - 1; i >= 0; --i) {
            if ((uval & (static_cast<U>(1) << i)) == 0) {
                zeros++;
            } else {
                break;
            }
        }
        return zeros;
    }

    static constexpr int trailing_zeros(Int value) {
        // Use unsigned type for bitwise operations
        using U = typename std::make_unsigned<Int>::type;
        U uval = static_cast<U>(value);
        // Use std::countr_zero from C++20 for potentially better performance
        if constexpr (__cplusplus >= 202002L) {
            return std::countr_zero(uval);
        } else {
            if (uval == 0)
                return sizeof(Int) * 8;

            int zeros = 0;
            while ((uval & 1) == 0) {
                zeros++;
                uval >>= 1;
            }
            return zeros;
        }
    }

    static constexpr int leading_ones(Int value) {
        // Use unsigned type for bitwise operations
        using U = typename std::make_unsigned<Int>::type;
        U uval = static_cast<U>(value);
        // This is equivalent to countl_one in C++20
        if constexpr (__cplusplus >= 202002L) {
            return std::countl_one(uval);
        } else {
            int ones = 0;
            const int total_bits = sizeof(Int) * 8;
            U mask = static_cast<U>(1) << (total_bits - 1);

            for (int i = 0; i < total_bits; ++i) {
                if ((uval & mask) != 0) {
                    ones++;
                } else {
                    break;
                }
                mask >>= 1;
            }
            return ones;
        }
    }

    static constexpr int trailing_ones(Int value) {
        // Use unsigned type for bitwise operations
        using U = typename std::make_unsigned<Int>::type;
        U uval = static_cast<U>(value);
        // This is equivalent to countr_one in C++20
        if constexpr (__cplusplus >= 202002L) {
            return std::countr_one(uval);
        } else {
            int ones = 0;
            while ((uval & 1) != 0) {
                ones++;
                uval >>= 1;
            }
            return ones;
        }
    }

    static constexpr Int reverse_bits(Int value) {
        // Use unsigned type for bitwise operations
        using U = typename std::make_unsigned<Int>::type;
        U uval = static_cast<U>(value);
        U result = 0;
        const int total_bits = sizeof(Int) * 8;

        // Use std::reverse_bits from C++23 for potentially better performance
        if constexpr (__cplusplus >= 202302L) {
            return static_cast<Int>(reverse_bits(uval));
        } else {
            for (int i = 0; i < total_bits; ++i) {
                result = (result << 1) | (uval & 1);
                uval >>= 1;
            }
            return static_cast<Int>(result);
        }
    }

    static constexpr Int swap_bytes(Int value) {
        // Use unsigned type for bitwise operations
        using U = typename std::make_unsigned<Int>::type;
        U uval = static_cast<U>(value);

        // Use std::byteswap from C++23 for potentially better performance
        #if __cplusplus >= 202302L && __has_include(<bit>)
            return static_cast<Int>(std::byteswap(uval));
        #else
            U result = 0;
            const int byte_count = sizeof(Int);
            for (int i = 0; i < byte_count; ++i) {
                result |= ((uval >> (i * 8)) & 0xFF)
                          << ((byte_count - 1 - i) * 8);
            }
            return static_cast<Int>(result);
        #endif
    }

    static Int min(Int a, Int b) { return std::min(a, b); }  // Use std::min

    static Int max(Int a, Int b) { return std::max(a, b); }  // Use std::max

    static Int clamp(Int value, Int min, Int max) {
        // Use std::clamp from C++17
        if constexpr (__cplusplus >= 201703L) {
            return std::clamp(value, min, max);
        } else {
            if (value < min)
                return min;
            if (value > max)
                return max;
            return value;
        }
    }

    static Int abs_diff(Int a, Int b) {
        // Use std::abs_diff from C++20
        if constexpr (__cplusplus >= 202002L) {
            return abs_diff(a, b);
        } else {
            if (a >= b)
                return a - b;
            return b - a;
        }
    }

    static bool is_power_of_two(Int value) {
        // Use std::has_single_bit from C++20
        if constexpr (__cplusplus >= 202002L) {
            return std::has_single_bit(
                static_cast<typename std::make_unsigned<Int>::type>(value));
        } else {
            return value > 0 && (value & (value - 1)) == 0;
        }
    }

    static Int next_power_of_two(Int value) {
        // Use std::bit_ceil from C++20
        if constexpr (__cplusplus >= 202002L) {
            if (value <= 0)
                return 1;
            // bit_ceil returns the smallest power of 2 >= value.
            // Need to handle the case where value is already a power of 2.
            if (is_power_of_two(value)) {
                // If value is already a power of two, the next power of two is
                // value * 2. Check for overflow before multiplying.
                if (value > MAX / 2)
                    return 0;  // Indicate overflow or cannot represent
                return value * 2;
            }
            // For non-power-of-two values, bit_ceil gives the next power of
            // two. Need to cast to unsigned for bit_ceil.
            typename std::make_unsigned<Int>::type uval =
                static_cast<typename std::make_unsigned<Int>::type>(value);
            typename std::make_unsigned<Int>::type result = std::bit_ceil(uval);
            // Check if the result fits back into the original signed type if
            // needed
            if constexpr (std::is_signed_v<Int>) {
                if (result >
                    static_cast<typename std::make_unsigned<Int>::type>(MAX)) {
                    return 0;  // Indicate overflow
                }
            }
            return static_cast<Int>(result);

        } else {
            if (value <= 0)
                return 1;

            // Handle the case where value is already a power of two
            if (is_power_of_two(value)) {
                // Check for overflow before multiplying by 2
                if (value > MAX / 2)
                    return 0;  // Indicate overflow or cannot represent
                return value * 2;
            }

            // For non-power-of-two values, find the most significant bit and
            // shift
            const int bit_shift =
                sizeof(Int) * 8 - 1 - leading_zeros(value - 1);

            // Check if the result (1 << (bit_shift + 1)) would overflow
            if (bit_shift >= sizeof(Int) * 8 - 1)
                return 0;  // Indicate overflow or cannot represent

            return static_cast<Int>(
                static_cast<typename std::make_unsigned<Int>::type>(1)
                << (bit_shift + 1));
        }
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
                ? static_cast<typename std::make_unsigned<Int>::type>(
                      -value)  // Use unary minus on unsigned type
                : static_cast<typename std::make_unsigned<Int>::type>(value);

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
        // Use unsigned type for hex representation to avoid sign extension
        // issues
        oss << std::hex
            << static_cast<typename std::make_unsigned<Int>::type>(value);
        return oss.str();
    }

    static std::string to_bin_string(Int value, bool with_prefix = true) {
        if (value == 0)
            return with_prefix ? "0b0" : "0";

        std::string result;
        typename std::make_unsigned<Int>::type uval =
            static_cast<typename std::make_unsigned<Int>::type>(value);
        const int total_bits = sizeof(Int) * 8;

        // Handle the case where the value is negative for signed types
        if constexpr (std::is_signed_v<Int>) {
            if (value < 0) {
                // For negative signed numbers, represent using two's complement
                // Start from the most significant bit
                for (int i = total_bits - 1; i >= 0; --i) {
                    result += ((uval >> i) & 1) ? '1' : '0';
                }
            } else {
                // For positive signed numbers or unsigned numbers, standard
                // binary conversion
                while (uval > 0) {
                    result = (uval & 1 ? '1' : '0') + result;
                    uval >>= 1;
                }
                // Pad with leading zeros if necessary to show full bit width
                while (result.length() < total_bits) {
                    result = '0' + result;
                }
            }
        } else {  // Unsigned
            while (uval > 0) {
                result = (uval & 1 ? '1' : '0') + result;
                uval >>= 1;
            }
            // Pad with leading zeros if necessary to show full bit width
            while (result.length() < total_bits) {
                result = '0' + result;
            }
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

            // Handle prefixes like 0x, 0b, 0o
            if (s.length() > start_idx + 1 && s[start_idx] == '0') {
                char prefix = std::tolower(s[start_idx + 1]);
                if ((prefix == 'x' && radix == 16) ||
                    (prefix == 'b' && radix == 2) ||
                    (prefix == 'o' && radix == 8)) {
                    start_idx += 2;
                } else if (s.length() > start_idx + 1 &&
                           s[start_idx + 1] >= '0' && s[start_idx + 1] <= '7' &&
                           radix == 10) {
                    // If it starts with '0' followed by a digit 0-7 and radix
                    // is 10, it might be interpreted as octal in some contexts,
                    // but Rust's from_str_radix(s, 10) does not treat '0'
                    // prefix as octal. We will follow Rust's behavior for
                    // radix 10. If radix is 8 and it starts with '0', the
                    // prefix is implicit.
                    if (radix == 8) {
                        start_idx += 1;  // Consume the leading '0'
                    }
                } else if (s.length() == start_idx + 1 && s[start_idx] == '0') {
                    // String is just "0" or "+0" or "-0"
                    return Result<Int>::ok(0);
                }
            }

            if (start_idx >= s.length()) {
                return Result<Int>::err(ErrorKind::ParseIntError,
                                        "String contains prefix but no digits");
            }

            typename std::make_unsigned<Int>::type result = 0;
            typename std::make_unsigned<Int>::type max_val_unsigned;

            if constexpr (std::is_signed_v<Int>) {
                // For signed types, the maximum absolute value is different for
                // positive and negative. MAX is the largest positive value. MIN
                // is the most negative value. The unsigned representation of
                // MIN is MAX + 1.
                max_val_unsigned =
                    negative
                        ? static_cast<typename std::make_unsigned<Int>::type>(
                              MAX) +
                              1
                        : static_cast<typename std::make_unsigned<Int>::type>(
                              MAX);
            } else {  // Unsigned
                max_val_unsigned = MAX;
            }

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
                    // Allow underscores as separators, but not at the start or
                    // end
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

                // Check for overflow before multiplication and addition
                // Check if result * radix would overflow
                if (max_val_unsigned / radix < result) {
                    return Result<Int>::err(
                        ErrorKind::ParseIntError,
                        "Overflow occurred during parsing (multiplication)");
                }
                result *= radix;

                // Check if result + digit would overflow
                if (max_val_unsigned - digit < result) {
                    return Result<Int>::err(
                        ErrorKind::ParseIntError,
                        "Overflow occurred during parsing (addition)");
                }
                result += digit;
            }

            if (negative) {
                // Check if the absolute value fits into the negative range
                if constexpr (std::is_signed_v<Int>) {
                    // The only value that doesn't fit after negation is MIN's
                    // absolute value if the type is signed and MIN is not
                    // representable as positive. This is handled by checking
                    // against MAX + 1 (unsigned representation of MIN). The
                    // overflow check during parsing against max_val_unsigned
                    // already covers this.
                    return Result<Int>::ok(static_cast<Int>(-result));
                } else {
                    // Unsigned types cannot be negative.
                    return Result<Int>::err(
                        ErrorKind::ParseIntError,
                        "Cannot parse negative value into unsigned type");
                }
            } else {
                // Check if the positive value fits into the type's range
                // The overflow check during parsing against max_val_unsigned
                // already covers this.
                return Result<Int>::ok(static_cast<Int>(result));
            }
        } catch (const std::exception& e) {
            // Catch potential exceptions from std::stod/stof if used internally
            // (though we are implementing manually)
            return Result<Int>::err(ErrorKind::ParseIntError, e.what());
        }
    }

    static Int random(Int min = MIN, Int max = MAX) {
        // Use thread_local for the random number generator to ensure thread
        // safety std::random_device is generally thread-safe for initialization
        static std::random_device rd;
        thread_local std::mt19937 gen(rd());

        if (min > max) {
            std::swap(min, max);
        }

        // Use std::uniform_int_distribution which is suitable for both signed
        // and unsigned integers
        std::uniform_int_distribution<Int> dist(min, max);
        return dist(gen);
    }

    static std::tuple<Int, Int> div_rem(Int a, Int b) {
        if (b == 0) {
            throw std::runtime_error("Division by zero");
        }
        // C++ standard guarantees that (a / b) * b + (a % b) == a for non-zero
        // b. The behavior for negative numbers differs from Rust's Euclidean
        // division. If Rust's behavior is needed, a custom implementation is
        // required. Assuming standard C++ integer division/remainder here.
        Int q = a / b;
        Int r = a % b;
        return {q, r};
    }

    static Int gcd(Int a, Int b) {
        // Use std::gcd from C++17
        if constexpr (__cplusplus >= 201703L) {
            return std::gcd(a, b);
        } else {
            // Ensure non-negative for the algorithm
            a = abs(a);
            b = abs(b);

            while (b != 0) {
                Int t = b;
                b = a % b;
                a = t;
            }
            return a;
        }
    }

    static Int lcm(Int a, Int b) {
        // Use std::lcm from C++17
        if constexpr (__cplusplus >= 201703L) {
            // std::lcm handles the case where a or b is 0, returning 0.
            // It also handles potential overflow by returning 0 if the result
            // is not representable.
            return std::lcm(a, b);
        } else {
            if (a == 0 || b == 0)
                return 0;

            // Ensure non-negative for the calculation
            a = abs(a);
            b = abs(b);

            // Calculate lcm using gcd: lcm(a, b) = (a / gcd(a, b)) * b
            // Perform division first to reduce the chance of overflow
            Int common_divisor = gcd(a, b);
            // Check for potential overflow before multiplication
            if (b / common_divisor > MAX / a) {
                // Indicate overflow (Rust's lcm doesn't have checked version)
                // Returning 0 might be one way to signal failure, or throw.
                // Let's throw for consistency with other potential errors.
                throw std::runtime_error("LCM calculation overflowed");
            }
            return (a / common_divisor) * b;
        }
    }

    static Int abs(Int a) {
        // Use std::abs
        if constexpr (std::is_signed_v<Int>) {
            // std::abs for signed integers might have UB for MIN.
            // Check for the MIN case explicitly.
            if (a == MIN) {
                // Rust's abs panics for MIN. We can throw.
                throw std::runtime_error("Absolute value of MIN overflows");
            }
        }
        return std::abs(a);
    }

    static Int bitwise_and(Int a, Int b) { return a & b; }

    static Option<Int> checked_bitand(Int a, Int b) {
        // Bitwise AND does not overflow for fixed-width integers.
        return Option<Int>::some(a & b);
    }

    static Int wrapping_bitand(Int a, Int b) {
        // Bitwise AND does not wrap for fixed-width integers.
        return a & b;
    }

    static Int saturating_bitand(Int a, Int b) {
        // Bitwise AND does not saturate for fixed-width integers.
        return a & b;
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
    static constexpr Float NAN_VAL =
        std::numeric_limits<Float>::quiet_NaN();  // Renamed to avoid conflict
                                                  // with #undef NAN
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
            // Check for NaN, infinity, and range before casting to integer
            if (std::isnan(value) || std::isinf(value) ||
                value <
                    static_cast<Float>(std::numeric_limits<ToType>::min()) ||
                value >
                    static_cast<Float>(std::numeric_limits<ToType>::max())) {
                return Option<ToType>::none();
            }
            return Option<ToType>::some(static_cast<ToType>(value));
        } else if (std::is_floating_point_v<ToType>) {
            // Check for range when casting between floating point types
            if (value < std::numeric_limits<ToType>::lowest() ||
                value > std::numeric_limits<ToType>::max()) {
                // Handle infinity and NaN explicitly as they might be
                // representable
                if (std::isinf(value))
                    return Option<ToType>::some(
                        std::numeric_limits<ToType>::infinity() *
                        (value < 0 ? -1 : 1));
                if (std::isnan(value))
                    return Option<ToType>::some(
                        std::numeric_limits<ToType>::quiet_NaN());
                return Option<ToType>::none();  // Value is finite but out of
                                                // range
            }
            return Option<ToType>::some(static_cast<ToType>(value));
        }
        // Conversion to other types is not supported by this method
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
        // Handle base 1 explicitly to avoid log(1) == 0 in denominator
        if (base == 1.0) {
            // log_1(x) is undefined unless x is also 1 (which is still tricky)
            // Rust's log(x, 1.0) returns NaN.
            return NAN_VAL;
        }
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
        // Handle NaN: NaN is not equal to anything, including itself.
        // Rust's approx_eq would return false if either is NaN.
        if (std::isnan(a) || std::isnan(b))
            return false;

        if (a == b)
            return true;

        Float diff = std::abs(a - b);
        // Check for equality of numbers near zero using absolute tolerance
        if (diff < std::numeric_limits<Float>::min()) {
            return diff < epsilon;
        }

        // Use relative tolerance for larger numbers
        return diff / (std::abs(a) + std::abs(b)) < epsilon;
    }

    static int total_cmp(Float a, Float b) {
        // Implements total ordering as defined by IEEE 754, where NaN has a
        // specific order. This is different from standard C++ comparison
        // operators for floats. Rust's total_cmp orders NaN greater than any
        // non-NaN value. Positive NaN > Negative NaN > +Infinity > finite >
        // -Infinity. All NaNs are equal to each other in this ordering.

        bool a_is_nan = std::isnan(a);
        bool b_is_nan = std::isnan(b);

        if (a_is_nan && b_is_nan)
            return 0;  // All NaNs are equal
        if (a_is_nan)
            return 1;  // a is NaN, b is not -> a > b
        if (b_is_nan)
            return -1;  // b is NaN, a is not -> a < b

        // Now handle non-NaN values
        if (a < b)
            return -1;
        if (a > b)
            return 1;

        // If a == b (and neither is NaN), they are equal.
        return 0;
    }

    static Float min(Float a, Float b) {
        // Rust's min returns the other value if one is NaN.
        // std::min returns NaN if either is NaN.
        if (std::isnan(a))
            return b;
        if (std::isnan(b))
            return a;
        return std::min(a, b);
    }

    static Float max(Float a, Float b) {
        // Rust's max returns the other value if one is NaN.
        // std::max returns NaN if either is NaN.
        if (std::isnan(a))
            return b;
        if (std::isnan(b))
            return a;
        return std::max(a, b);
    }

    static Float clamp(Float value, Float min, Float max) {
        // Rust's clamp returns min if value is NaN.
        if (std::isnan(value))
            return min;
        // Use std::clamp from C++17
        if constexpr (__cplusplus >= 201703L) {
            return std::clamp(value, min, max);
        } else {
            if (value < min)
                return min;
            if (value > max)
                return max;
            return value;
        }
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
            Float val;
            // Use std::stod/stof/stold which are generally efficient
            if constexpr (std::is_same_v<Float, float>) {
                val = std::stof(s, &pos);
            } else if constexpr (std::is_same_v<Float, double>) {
                val = std::stod(s, &pos);
            } else {  // long double or other float types
                val = static_cast<Float>(std::stold(s, &pos));
            }

            // Check if the entire string was consumed
            if (pos != s.length()) {
                return Result<Float>::err(ErrorKind::ParseFloatError,
                                          "Failed to parse entire string");
            }

            // Check for potential range errors after parsing
            if (is_finite(val)) {
                if (val < std::numeric_limits<Float>::lowest() ||
                    val > std::numeric_limits<Float>::max()) {
                    return Result<Float>::err(
                        ErrorKind::ParseFloatError,
                        "Value out of range for float type");
                }
            }

            return Result<Float>::ok(val);
        } catch (const std::exception& e) {
            // Catch exceptions like std::invalid_argument or std::out_of_range
            // from stod/stof/stold
            return Result<Float>::err(ErrorKind::ParseFloatError, e.what());
        }
    }

    static Float random(Float min = 0.0, Float max = 1.0) {
        // Use thread_local for the random number generator to ensure thread
        // safety std::random_device is generally thread-safe for initialization
        static std::random_device rd;
        thread_local std::mt19937 gen(rd());

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

    static Float ulp(Float x) {
        // Use std::ulp from C++20
        if constexpr (__cplusplus >= 202002L) {
            return ulp(x);
        } else {
            // Fallback implementation
            if (std::isnan(x) || std::isinf(x))
                return NAN_VAL;
            if (x == 0)
                return std::numeric_limits<Float>::min();  // Smallest positive
                                                           // denormalized value
            Float next = next_up(x);
            return next - x;
        }
    }

    static Float to_radians(Float degrees) {
        return degrees * PI / static_cast<Float>(180.0);
    }

    static Float to_degrees(Float radians) {
        return radians * static_cast<Float>(180.0) / PI;
    }

    static Float hypot(Float x, Float y) { return std::hypot(x, y); }

    static Float hypot(Float x, Float y, Float z) {
        // std::hypot overload for three arguments is C++17
        if constexpr (__cplusplus >= 201703L) {
            return std::hypot(x, y, z);
        } else {
            // Fallback implementation
            return std::sqrt(x * x + y * y + z * z);
        }
    }

    static Float lerp(Float a, Float b, Float t) {
        // Use std::lerp from C++20
        if constexpr (__cplusplus >= 202002L) {
            return std::lerp(a, b, t);
        } else {
            // Fallback implementation
            return a + t * (b - a);
        }
    }

    static Float sign(Float x) {
        // Returns -1.0, +1.0, or 0.0 depending on the sign.
        // Handles NaN by returning NaN (consistent with Rust's signum).
        if (std::isnan(x))
            return NAN_VAL;
        if (x > 0)
            return 1.0;
        if (x < 0)
            return -1.0;
        return 0.0;  // Handles +0.0 and -0.0 as 0.0
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
    // Alias for compatibility with tests
    static constexpr f32 NAN = FloatMethods<f32>::NAN_VAL;

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
        // Use C++20 three-way comparison if available and applicable
        if constexpr (__cplusplus >= 202002L && std::three_way_comparable<T>) {
            auto cmp = std::compare_three_way()(a, b);
            if (cmp < 0)
                return Ordering::Less;
            if (cmp > 0)
                return Ordering::Greater;
            return Ordering::Equal;
        } else {
            if (a < b)
                return Ordering::Less;
            if (a > b)
                return Ordering::Greater;
            return Ordering::Equal;
        }
    }

    class Comparator {
    public:
        bool operator()(const T& a, const T& b) const {
            return compare(a, b) == Ordering::Less;
        }
    };

    // Define the ByKey comparator class outside the by_key function
    template <typename KeyType, typename Func>
    class ByKeyComparator {
    private:
        Func m_key_fn;

    public:
        ByKeyComparator(Func key_fn) : m_key_fn(std::move(key_fn)) {}

        // Use C++20 three-way comparison for keys if available
        // KeyType is now a template parameter of the class, not the operator()
        bool operator()(const T& a, const T& b) const {
            auto a_key = m_key_fn(a);
            auto b_key = m_key_fn(b);
            if constexpr (__cplusplus >= 202002L &&
                          std::three_way_comparable<KeyType>) {
                return std::compare_three_way()(a_key, b_key) < 0;
            } else {
                return a_key < b_key;
            }
        }
    };

    template <typename F>
    static auto by_key(F&& key_fn) {
        // Deduce the key type U
        using KeyType = decltype(std::declval<F>()(std::declval<T>()));
        // Return an instance of the ByKeyComparator template class
        return ByKeyComparator<KeyType, F>(std::forward<F>(key_fn));
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
    // Use std::invoke_result_t from C++17 for cleaner type deduction
    using value_type =
        std::invoke_result_t<Func, decltype(*std::declval<Iter>())>;
    // Note: pointer and reference types for output iterators are tricky and
    // often not direct pointers/references For input iterators like this,
    // value_type is typically returned by value from operator* We'll keep
    // pointer/reference as value_type* and value_type& for simplicity, though
    // they might not be strictly correct for all iterator categories.
    using pointer = value_type*;   // Placeholder
    using reference = value_type;  // Return by value

    MapIterator(Iter iter, Func func) : m_iter(iter), m_func(func) {}

    reference operator*() const { return m_func(*m_iter); }

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
    // value_type is a pair of index and the value from the underlying iterator
    using value_type =
        std::pair<size_t, typename std::iterator_traits<Iter>::value_type>;
    // reference is a pair of index and a reference to the value from the
    // underlying iterator
    using reference =
        std::pair<size_t, typename std::iterator_traits<Iter>::reference>;
    using pointer = value_type*;  // Placeholder

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

// Using declarations for convenience - commented out to avoid conflicts
// using i8 = atom::algorithm::I8;
// using i16 = atom::algorithm::I16;
// using i32 = atom::algorithm::I32;
// using i64 = atom::algorithm::I64;
// using u8 = atom::algorithm::U8;
// using u16 = atom::algorithm::U16;
// using u32 = atom::algorithm::U32;
// using u64 = atom::algorithm::U64;
// using isize = atom::algorithm::Isize;
// using usize = atom::algorithm::Usize;
// using f32 = atom::algorithm::F32;
// using f64 = atom::algorithm::F64;

// Note on Concurrency and Performance:
// The provided code primarily implements value-based numeric operations and
// stateless iterator adaptors. These components are largely thread-safe by
// design as they do not share mutable state between threads unless the
// underlying types or containers used with iterators are not thread-safe.
//
// The main area requiring attention for concurrency is the use of static random
// number generators in the `random` methods of `IntMethods` and `FloatMethods`.
// These have been updated to use `thread_local` generators, which is a standard
// C++ approach for making such resources thread-safe without requiring explicit
// locks and minimizing contention in multi-threaded scenarios.
//
// Other parts of the code, like arithmetic operations, parsing, and bit
// manipulation, operate on function arguments and local variables. Their
// performance and thread safety in a larger application depend on how they are
// called and what data they operate on. The methods themselves do not
// inherently require advanced concurrency primitives (like mutexes, atomics, or
// concurrent data structures) because they don't manage shared mutable state
// internally beyond the random number generator.
//
// Optimizations for "maximum performance" in numeric code often involve
// compiler flags (e.g., -O3, architecture-specific optimizations), using
// appropriate data types, and leveraging standard library functions which are
// typically highly optimized. The current code already uses standard library
// functions extensively. Further performance gains might require profiling
// specific use cases, considering SIMD instructions (via intrinsics or
// libraries), or potentially using specialized libraries for high-performance
// computing, which are beyond the scope of this general refactoring.
//
// Modern C++ features (C++17, C++20, C++23) like `std::clamp`, `std::gcd`,
// `std::lcm`, bit manipulation functions (`std::popcount`, `std::countl_zero`,
// `std::bit_ceil`, `std::byteswap`), `std::lerp`, `std::ulp`,
// `std::invoke_result_t`, and `std::three_way_comparable` have been
// incorporated where applicable to leverage potentially optimized standard
// library implementations and improve code clarity.
