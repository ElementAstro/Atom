// rust_int_methods.hpp
#pragma once

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>

#include "rust_error.hpp"
#include "rust_option.hpp"
#include "rust_result.hpp"
#include "rust_types.hpp"

#include "atom/error/exception.hpp"

namespace atom::algorithm {

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
            THROW_RUNTIME_ERROR("Division by zero");
        }
        if (a == MIN && b == -1) {
            return MIN;
        }
        return a / b;
    }

    static Int wrapping_rem(Int a, Int b) {
        if (b == 0) {
            THROW_RUNTIME_ERROR("Division by zero");
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
        if (value <= 1)
            return 1;

        // For value > 1, find smallest power of 2 >= value
        --value;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        if constexpr (sizeof(Int) >= 2)
            value |= value >> 8;
        if constexpr (sizeof(Int) >= 4)
            value |= value >> 16;
        if constexpr (sizeof(Int) >= 8)
            value |= value >> 32;
        ++value;

        return value;
    }

    static std::string to_string(Int value, int base = 10) {
        if (base < 2 || base > 36) {
            THROW_INVALID_ARGUMENT("Base must be between 2 and 36");
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
            THROW_RUNTIME_ERROR("Division by zero");
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
                THROW_RUNTIME_ERROR("Absolute value of MIN overflows");
            }
            return -a;
        }
        return a;
    }

    static Int bitwise_and(Int a, Int b) { return a & b; }

    static Option<Int> checked_bitand(Int a, Int b) {
        return Option<Int>::some(a & b);
    }

    static Int wrapping_bitand(Int a, Int b) { return a & b; }

    static Int saturating_bitand(Int a, Int b) { return a & b; }
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

}  // namespace atom::algorithm
