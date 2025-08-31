#include <gtest/gtest.h>

#include "atom/algorithm/rust_numeric.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace atom::algorithm;

// ====================== Type Aliases Tests ======================
TEST(RustNumericTypesTest, TypeAliases) {
    EXPECT_EQ(sizeof(i8), 1);
    EXPECT_EQ(sizeof(i16), 2);
    EXPECT_EQ(sizeof(i32), 4);
    EXPECT_EQ(sizeof(i64), 8);
    EXPECT_EQ(sizeof(isize), sizeof(std::ptrdiff_t));

    EXPECT_EQ(sizeof(u8), 1);
    EXPECT_EQ(sizeof(u16), 2);
    EXPECT_EQ(sizeof(u32), 4);
    EXPECT_EQ(sizeof(u64), 8);
    EXPECT_EQ(sizeof(usize), sizeof(std::size_t));

    EXPECT_EQ(sizeof(f32), 4);
    EXPECT_EQ(sizeof(f64), 8);
}

// ====================== Error Tests ======================
TEST(RustNumericErrorTest, ErrorKindToString) {
    Error parse_int_error(ErrorKind::ParseIntError, "Failed to parse integer");
    Error div_zero_error(ErrorKind::DivideByZero, "Division by zero");

    EXPECT_EQ(parse_int_error.to_string(),
              "ParseIntError: Failed to parse integer");
    EXPECT_EQ(div_zero_error.to_string(), "DivideByZero: Division by zero");
}

// ====================== Result Tests ======================
TEST(RustNumericResultTest, OkResult) {
    Result<int> ok_result = Result<int>::ok(42);

    EXPECT_TRUE(ok_result.is_ok());
    EXPECT_FALSE(ok_result.is_err());
    EXPECT_EQ(ok_result.unwrap(), 42);
    EXPECT_EQ(ok_result.unwrap_or(10), 42);
}

TEST(RustNumericResultTest, ErrResult) {
    Result<int> err_result =
        Result<int>::err(ErrorKind::ParseIntError, "Failed to parse int");

    EXPECT_FALSE(err_result.is_ok());
    EXPECT_TRUE(err_result.is_err());
    EXPECT_EQ(err_result.unwrap_or(10), 10);
    EXPECT_THROW(err_result.unwrap(), std::runtime_error);

    Error error = err_result.unwrap_err();
    EXPECT_EQ(error.kind(), ErrorKind::ParseIntError);
    EXPECT_EQ(error.message(), "Failed to parse int");
}

TEST(RustNumericResultTest, MapMethod) {
    Result<int> ok_result = Result<int>::ok(42);
    auto mapped_ok = ok_result.map([](int x) { return x * 2; });
    EXPECT_TRUE(mapped_ok.is_ok());
    EXPECT_EQ(mapped_ok.unwrap(), 84);

    Result<int> err_result =
        Result<int>::err(ErrorKind::ParseIntError, "Error");
    auto mapped_err = err_result.map([](int x) { return x * 2; });
    EXPECT_TRUE(mapped_err.is_err());
}

TEST(RustNumericResultTest, UnwrapOrElseMethod) {
    Result<int> ok_result = Result<int>::ok(42);
    int ok_value = ok_result.unwrap_or_else([](const Error&) { return 10; });
    EXPECT_EQ(ok_value, 42);

    Result<int> err_result =
        Result<int>::err(ErrorKind::ParseIntError, "Error");
    int err_value = err_result.unwrap_or_else([](const Error&) { return 10; });
    EXPECT_EQ(err_value, 10);
}

// ====================== Option Tests ======================
TEST(RustNumericOptionTest, SomeOption) {
    Option<int> some = Option<int>::some(42);

    EXPECT_TRUE(some.has_value());
    EXPECT_TRUE(some.is_some());
    EXPECT_FALSE(some.is_none());
    EXPECT_EQ(some.value(), 42);
    EXPECT_EQ(some.unwrap(), 42);
    EXPECT_EQ(some.unwrap_or(10), 42);
}

TEST(RustNumericOptionTest, NoneOption) {
    Option<int> none = Option<int>::none();

    EXPECT_FALSE(none.has_value());
    EXPECT_FALSE(none.is_some());
    EXPECT_TRUE(none.is_none());
    EXPECT_THROW(none.value(), std::runtime_error);
    EXPECT_THROW(none.unwrap(), std::runtime_error);
    EXPECT_EQ(none.unwrap_or(10), 10);
}

TEST(RustNumericOptionTest, MapMethod) {
    Option<int> some = Option<int>::some(42);
    auto mapped_some = some.map([](int x) { return x * 2; });
    EXPECT_TRUE(mapped_some.is_some());
    EXPECT_EQ(mapped_some.unwrap(), 84);

    Option<int> none = Option<int>::none();
    auto mapped_none = none.map([](int x) { return x * 2; });
    EXPECT_TRUE(mapped_none.is_none());
}

TEST(RustNumericOptionTest, UnwrapOrElseMethod) {
    Option<int> some = Option<int>::some(42);
    int some_value = some.unwrap_or_else([]() { return 10; });
    EXPECT_EQ(some_value, 42);

    Option<int> none = Option<int>::none();
    int none_value = none.unwrap_or_else([]() { return 10; });
    EXPECT_EQ(none_value, 10);
}

TEST(RustNumericOptionTest, AndThenMethod) {
    Option<int> some = Option<int>::some(42);
    auto result_some =
        some.and_then([](int x) { return Option<double>::some(x * 1.5); });
    EXPECT_TRUE(result_some.is_some());
    EXPECT_DOUBLE_EQ(result_some.unwrap(), 63.0);

    Option<int> none = Option<int>::none();
    auto result_none =
        none.and_then([](int x) { return Option<double>::some(x * 1.5); });
    EXPECT_TRUE(result_none.is_none());
}

// ====================== Range Tests ======================
TEST(RustNumericRangeTest, Iteration) {
    Range<int> r(1, 5);
    std::vector<int> values;

    for (int v : r) {
        values.push_back(v);
    }

    EXPECT_EQ(values.size(), 4);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
    EXPECT_EQ(values[3], 4);
}

TEST(RustNumericRangeTest, InclusiveIteration) {
    Range<int> r(1, 5, true);
    std::vector<int> values;

    for (int v : r) {
        values.push_back(v);
    }

    EXPECT_EQ(values.size(), 5);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
    EXPECT_EQ(values[3], 4);
    EXPECT_EQ(values[4], 5);
}

TEST(RustNumericRangeTest, EmptyRange) {
    Range<int> r(5, 1);
    std::vector<int> values;

    for (int v : r) {
        values.push_back(v);
    }

    EXPECT_TRUE(values.empty());
}

TEST(RustNumericRangeTest, ContainsMethod) {
    Range<int> r(1, 5);
    EXPECT_TRUE(r.contains(1));
    EXPECT_TRUE(r.contains(3));
    EXPECT_FALSE(r.contains(5));
    EXPECT_FALSE(r.contains(0));

    Range<int> inclusive_r(1, 5, true);
    EXPECT_TRUE(inclusive_r.contains(5));
}

TEST(RustNumericRangeTest, LenMethod) {
    Range<int> r(1, 5);
    EXPECT_EQ(r.len(), 4);

    Range<int> inclusive_r(1, 5, true);
    EXPECT_EQ(inclusive_r.len(), 5);

    Range<int> empty_r(5, 1);
    EXPECT_EQ(empty_r.len(), 0);
}

TEST(RustNumericRangeTest, IsEmptyMethod) {
    Range<int> r(1, 5);
    EXPECT_FALSE(r.is_empty());

    Range<int> empty_r(5, 1);
    EXPECT_TRUE(empty_r.is_empty());

    Range<int> same_r(5, 5);
    EXPECT_TRUE(same_r.is_empty());

    Range<int> inclusive_same_r(5, 5, true);
    EXPECT_FALSE(inclusive_same_r.is_empty());
}

TEST(RustNumericRangeTest, RangeFunction) {
    auto r = range(1, 5);
    std::vector<int> values;

    for (int v : r) {
        values.push_back(v);
    }

    EXPECT_EQ(values.size(), 4);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[3], 4);
}

TEST(RustNumericRangeTest, RangeInclusiveFunction) {
    auto r = range_inclusive(1, 5);
    std::vector<int> values;

    for (int v : r) {
        values.push_back(v);
    }

    EXPECT_EQ(values.size(), 5);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[4], 5);
}

// ====================== Integer Methods Tests ======================
TEST(RustNumericIntegerTest, MinMax) {
    EXPECT_EQ(I8::MIN, -128);
    EXPECT_EQ(I8::MAX, 127);
    EXPECT_EQ(U8::MIN, 0);
    EXPECT_EQ(U8::MAX, 255);
}

TEST(RustNumericIntegerTest, TryInto) {
    auto u8_from_i8 = I8::try_into<u8>(42);
    EXPECT_TRUE(u8_from_i8.is_some());
    EXPECT_EQ(u8_from_i8.unwrap(), 42);

    auto u8_from_negative_i8 = I8::try_into<u8>(-42);
    EXPECT_TRUE(u8_from_negative_i8.is_none());

    auto i8_from_large_u8 = U8::try_into<i8>(200);
    EXPECT_TRUE(i8_from_large_u8.is_none());
}

TEST(RustNumericIntegerTest, CheckedOperations) {
    // Checked Add
    auto add_ok = I8::checked_add(100, 20);
    EXPECT_TRUE(add_ok.is_some());
    EXPECT_EQ(add_ok.unwrap(), 120);

    auto add_overflow = I8::checked_add(100, 100);
    EXPECT_TRUE(add_overflow.is_none());

    // Checked Sub
    auto sub_ok = I8::checked_sub(100, 20);
    EXPECT_TRUE(sub_ok.is_some());
    EXPECT_EQ(sub_ok.unwrap(), 80);

    auto sub_underflow = I8::checked_sub(-100, 100);
    EXPECT_TRUE(sub_underflow.is_none());

    // Checked Mul
    auto mul_ok = I8::checked_mul(12, 10);
    EXPECT_TRUE(mul_ok.is_some());
    EXPECT_EQ(mul_ok.unwrap(), 120);

    auto mul_overflow = I8::checked_mul(100, 100);
    EXPECT_TRUE(mul_overflow.is_none());

    // Checked Div
    auto div_ok = I8::checked_div(120, 10);
    EXPECT_TRUE(div_ok.is_some());
    EXPECT_EQ(div_ok.unwrap(), 12);

    auto div_by_zero = I8::checked_div(120, 0);
    EXPECT_TRUE(div_by_zero.is_none());

    // Min int edge case
    auto div_min_by_neg1 = I8::checked_div(I8::MIN, -1);
    EXPECT_TRUE(div_min_by_neg1.is_none());

    // Checked Rem
    auto rem_ok = I8::checked_rem(125, 10);
    EXPECT_TRUE(rem_ok.is_some());
    EXPECT_EQ(rem_ok.unwrap(), 5);

    auto rem_by_zero = I8::checked_rem(125, 0);
    EXPECT_TRUE(rem_by_zero.is_none());

    // Checked Neg
    auto neg_ok = I8::checked_neg(100);
    EXPECT_TRUE(neg_ok.is_some());
    EXPECT_EQ(neg_ok.unwrap(), -100);

    auto neg_overflow = I8::checked_neg(I8::MIN);
    EXPECT_TRUE(neg_overflow.is_none());

    // Checked Abs
    auto abs_ok = I8::checked_abs(-100);
    EXPECT_TRUE(abs_ok.is_some());
    EXPECT_EQ(abs_ok.unwrap(), 100);

    auto abs_overflow = I8::checked_abs(I8::MIN);
    EXPECT_TRUE(abs_overflow.is_none());
}

TEST(RustNumericIntegerTest, CheckedBitOperations) {
    // Checked Shl
    auto shl_ok = I8::checked_shl(1, 3);
    EXPECT_TRUE(shl_ok.is_some());
    EXPECT_EQ(shl_ok.unwrap(), 8);

    auto shl_overflow = I8::checked_shl(1, 10);
    EXPECT_TRUE(shl_overflow.is_none());

    // Checked Shr
    auto shr_ok = I8::checked_shr(16, 2);
    EXPECT_TRUE(shr_ok.is_some());
    EXPECT_EQ(shr_ok.unwrap(), 4);

    auto shr_overflow = I8::checked_shr(16, 10);
    EXPECT_TRUE(shr_overflow.is_none());
}

TEST(RustNumericIntegerTest, SaturatingOperations) {
    // Saturating Add
    EXPECT_EQ(I8::saturating_add(100, 20), 120);
    EXPECT_EQ(I8::saturating_add(100, 100), I8::MAX);
    EXPECT_EQ(I8::saturating_add(-100, -100), I8::MIN);

    // Saturating Sub
    EXPECT_EQ(I8::saturating_sub(100, 20), 80);
    EXPECT_EQ(I8::saturating_sub(-100, 100), I8::MIN);
    EXPECT_EQ(I8::saturating_sub(100, -100), I8::MAX);

    // Saturating Mul
    EXPECT_EQ(I8::saturating_mul(12, 10), 120);
    EXPECT_EQ(I8::saturating_mul(100, 100), I8::MAX);
    EXPECT_EQ(I8::saturating_mul(-100, 100), I8::MIN);

    // Saturating Pow
    EXPECT_EQ(I8::saturating_pow(2, 3), 8);
    EXPECT_EQ(I8::saturating_pow(2, 10), I8::MAX);
}

TEST(RustNumericIntegerTest, WrappingOperations) {
    // Wrapping Add
    EXPECT_EQ(I8::wrapping_add(100, 20), 120);
    EXPECT_NE(I8::wrapping_add(100, 100), 200);  // Should wrap

    // Wrapping Sub
    EXPECT_EQ(I8::wrapping_sub(100, 20), 80);
    EXPECT_NE(I8::wrapping_sub(-100, 100), -200);  // Should wrap

    // Wrapping Mul
    EXPECT_EQ(I8::wrapping_mul(12, 10), 120);
    EXPECT_NE(I8::wrapping_mul(100, 100), 10000);  // Should wrap

    // Wrapping Div
    EXPECT_EQ(I8::wrapping_div(120, 10), 12);
    EXPECT_THROW(I8::wrapping_div(120, 0), std::runtime_error);
    EXPECT_EQ(I8::wrapping_div(I8::MIN, -1), I8::MIN);  // Special case

    // Wrapping Rem
    EXPECT_EQ(I8::wrapping_rem(125, 10), 5);
    EXPECT_THROW(I8::wrapping_rem(125, 0), std::runtime_error);

    // Wrapping Neg
    EXPECT_EQ(I8::wrapping_neg(100), -100);
    EXPECT_EQ(I8::wrapping_neg(I8::MIN), I8::MIN);  // Should wrap to itself

    // Wrapping Abs
    EXPECT_EQ(I8::wrapping_abs(-100), 100);
    EXPECT_EQ(I8::wrapping_abs(I8::MIN), I8::MIN);  // Should wrap to itself
}

TEST(RustNumericIntegerTest, BitManipulation) {
    // Rotate Left
    EXPECT_EQ(U8::rotate_left(0b00000001, 1), 0b00000010);
    EXPECT_EQ(U8::rotate_left(0b10000000, 1), 0b00000001);
    EXPECT_EQ(U8::rotate_left(0b10000001, 1), 0b00000011);

    // Rotate Right
    EXPECT_EQ(U8::rotate_right(0b00000010, 1), 0b00000001);
    EXPECT_EQ(U8::rotate_right(0b00000001, 1), 0b10000000);
    EXPECT_EQ(U8::rotate_right(0b11000000, 1), 0b01100000);

    // Bit Counting
    EXPECT_EQ(U8::count_ones(0b10101010), 4);
    EXPECT_EQ(U8::count_zeros(0b10101010), 4);
    EXPECT_EQ(U8::leading_zeros(0b00101010), 2);
    EXPECT_EQ(U8::trailing_zeros(0b10100000), 5);
    EXPECT_EQ(U8::leading_ones(0b11100000), 3);
    EXPECT_EQ(U8::trailing_ones(0b00000111), 3);

    // Bit Reversal
    EXPECT_EQ(U8::reverse_bits(0b10101010), 0b01010101);

    // Byte Swapping
    EXPECT_EQ(U16::swap_bytes(0x1234), 0x3412);
    EXPECT_EQ(U32::swap_bytes(0x12345678), 0x78563412);
}

TEST(RustNumericIntegerTest, UtilityFunctions) {
    // Min, Max, Clamp
    EXPECT_EQ(I32::min(10, 20), 10);
    EXPECT_EQ(I32::max(10, 20), 20);
    EXPECT_EQ(I32::clamp(15, 10, 20), 15);
    EXPECT_EQ(I32::clamp(5, 10, 20), 10);
    EXPECT_EQ(I32::clamp(25, 10, 20), 20);

    // abs_diff
    EXPECT_EQ(I32::abs_diff(10, 20), 10);
    EXPECT_EQ(I32::abs_diff(20, 10), 10);

    // Power of two checks
    EXPECT_TRUE(U32::is_power_of_two(1));
    EXPECT_TRUE(U32::is_power_of_two(2));
    EXPECT_TRUE(U32::is_power_of_two(4));
    EXPECT_TRUE(U32::is_power_of_two(8));
    EXPECT_FALSE(U32::is_power_of_two(0));
    EXPECT_FALSE(U32::is_power_of_two(3));
    EXPECT_FALSE(U32::is_power_of_two(6));

    // Next power of two
    EXPECT_EQ(U32::next_power_of_two(0), 1);
    EXPECT_EQ(U32::next_power_of_two(1), 1);
    EXPECT_EQ(U32::next_power_of_two(2), 2);
    EXPECT_EQ(U32::next_power_of_two(3), 4);
    EXPECT_EQ(U32::next_power_of_two(5), 8);
    EXPECT_EQ(U32::next_power_of_two(7), 8);
}

TEST(RustNumericIntegerTest, StringConversion) {
    // To String
    EXPECT_EQ(I32::to_string(42), "42");
    EXPECT_EQ(I32::to_string(-42), "-42");
    EXPECT_EQ(I32::to_string(42, 16), "2a");
    EXPECT_EQ(I32::to_string(42, 2), "101010");

    // To Hex/Bin String
    EXPECT_EQ(I32::to_hex_string(42), "0x2a");
    EXPECT_EQ(I32::to_hex_string(42, false), "2a");
    EXPECT_EQ(I32::to_bin_string(42), "0b101010");
    EXPECT_EQ(I32::to_bin_string(42, false), "101010");

    // From String
    auto from_dec = I32::from_str("42");
    EXPECT_TRUE(from_dec.is_ok());
    EXPECT_EQ(from_dec.unwrap(), 42);

    auto from_neg = I32::from_str("-42");
    EXPECT_TRUE(from_neg.is_ok());
    EXPECT_EQ(from_neg.unwrap(), -42);

    auto from_hex = I32::from_str_radix("2a", 16);
    EXPECT_TRUE(from_hex.is_ok());
    EXPECT_EQ(from_hex.unwrap(), 42);

    auto from_bin = I32::from_str_radix("101010", 2);
    EXPECT_TRUE(from_bin.is_ok());
    EXPECT_EQ(from_bin.unwrap(), 42);

    auto from_hex_prefix = I32::from_str_radix("0x2a", 16);
    EXPECT_TRUE(from_hex_prefix.is_ok());
    EXPECT_EQ(from_hex_prefix.unwrap(), 42);

    auto from_bin_prefix = I32::from_str_radix("0b101010", 2);
    EXPECT_TRUE(from_bin_prefix.is_ok());
    EXPECT_EQ(from_bin_prefix.unwrap(), 42);

    // Invalid strings
    auto invalid_radix = I32::from_str_radix("42", 37);
    EXPECT_TRUE(invalid_radix.is_err());

    auto empty_str = I32::from_str("");
    EXPECT_TRUE(empty_str.is_err());

    auto invalid_chars = I32::from_str("42x");
    EXPECT_TRUE(invalid_chars.is_err());

    auto only_sign = I32::from_str("+");
    EXPECT_TRUE(only_sign.is_err());
}

TEST(RustNumericIntegerTest, MathOperations) {
    // Random
    int random_value = I32::random(1, 100);
    EXPECT_GE(random_value, 1);
    EXPECT_LE(random_value, 100);

    // Division with Remainder
    auto [quotient, remainder] = I32::div_rem(10, 3);
    EXPECT_EQ(quotient, 3);
    EXPECT_EQ(remainder, 1);

    // GCD & LCM
    EXPECT_EQ(I32::gcd(12, 18), 6);
    EXPECT_EQ(I32::lcm(12, 18), 36);
    EXPECT_EQ(I32::gcd(-12, 18), 6);
    EXPECT_EQ(I32::lcm(-12, 18), 36);

    // Abs
    EXPECT_EQ(I32::abs(-42), 42);
    EXPECT_EQ(I32::abs(42), 42);
    EXPECT_THROW(I32::abs(std::numeric_limits<i32>::min()), std::runtime_error);
}

// ====================== Float Methods Tests ======================
TEST(RustNumericFloatTest, Constants) {
    EXPECT_TRUE(std::isinf(F32::INFINITY_VAL));
    EXPECT_TRUE(std::isinf(F32::NEG_INFINITY));
    EXPECT_TRUE(std::isnan(F32::NAN));
    EXPECT_FLOAT_EQ(F32::PI, 3.14159265358979323846f);
    EXPECT_FLOAT_EQ(F32::E, 2.71828182845904523536f);
}

TEST(RustNumericFloatTest, ConversionMethods) {
    // Try Into
    auto int_from_float = F32::try_into<int>(42.5f);
    EXPECT_TRUE(int_from_float.is_some());
    EXPECT_EQ(int_from_float.unwrap(), 42);

    auto int_from_large_float = F32::try_into<i8>(500.0f);
    EXPECT_TRUE(int_from_large_float.is_none());

    auto float_from_double = F64::try_into<float>(1e30);
    EXPECT_TRUE(float_from_double.is_none());

    // From String
    auto float_from_str = F32::from_str("42.5");
    EXPECT_TRUE(float_from_str.is_ok());
    EXPECT_FLOAT_EQ(float_from_str.unwrap(), 42.5f);

    auto invalid_float = F32::from_str("not a number");
    EXPECT_TRUE(invalid_float.is_err());

    auto incomplete_parse = F32::from_str("42.5xyz");
    EXPECT_TRUE(incomplete_parse.is_err());

    // To String
    EXPECT_EQ(F32::to_string(42.5f), "42.500000");
    EXPECT_EQ(F32::to_string(42.5f, 2), "42.50");

    std::string exp_str = F32::to_exp_string(42500.0f);
    EXPECT_TRUE(exp_str.find("e+04") != std::string::npos);
}

TEST(RustNumericFloatTest, ClassificationMethods) {
    EXPECT_TRUE(F32::is_nan(F32::NAN));
    EXPECT_FALSE(F32::is_nan(1.0f));

    EXPECT_TRUE(F32::is_infinite(F32::INFINITY_VAL));
    EXPECT_TRUE(F32::is_infinite(F32::NEG_INFINITY));
    EXPECT_FALSE(F32::is_infinite(1.0f));

    EXPECT_TRUE(F32::is_finite(1.0f));
    EXPECT_FALSE(F32::is_finite(F32::INFINITY_VAL));

    EXPECT_TRUE(F32::is_normal(1.0f));
    EXPECT_FALSE(F32::is_normal(0.0f));

    EXPECT_TRUE(F32::is_subnormal(std::numeric_limits<float>::denorm_min()));
    EXPECT_FALSE(F32::is_subnormal(1.0f));

    EXPECT_TRUE(F32::is_sign_positive(1.0f));
    EXPECT_FALSE(F32::is_sign_positive(-1.0f));

    EXPECT_TRUE(F32::is_sign_negative(-1.0f));
    EXPECT_FALSE(F32::is_sign_negative(1.0f));
}

TEST(RustNumericFloatTest, BasicMathOperations) {
    EXPECT_FLOAT_EQ(F32::abs(-42.5f), 42.5f);
    EXPECT_FLOAT_EQ(F32::floor(42.7f), 42.0f);
    EXPECT_FLOAT_EQ(F32::ceil(42.2f), 43.0f);
    EXPECT_FLOAT_EQ(F32::round(42.5f), 43.0f);
    EXPECT_FLOAT_EQ(F32::trunc(42.7f), 42.0f);
    EXPECT_FLOAT_EQ(F32::fract(42.7f), 0.7f);

    EXPECT_FLOAT_EQ(F32::sqrt(16.0f), 4.0f);
    EXPECT_FLOAT_EQ(F32::cbrt(8.0f), 2.0f);

    EXPECT_NEAR(F32::exp(1.0f), F32::E, 1e-6f);
    EXPECT_FLOAT_EQ(F32::exp2(3.0f), 8.0f);

    EXPECT_NEAR(F32::ln(F32::E), 1.0f, 1e-6f);
    EXPECT_FLOAT_EQ(F32::log2(8.0f), 3.0f);
    EXPECT_FLOAT_EQ(F32::log10(100.0f), 2.0f);
    EXPECT_FLOAT_EQ(F32::log(100.0f, 10.0f), 2.0f);

    EXPECT_FLOAT_EQ(F32::pow(2.0f, 3.0f), 8.0f);
}

TEST(RustNumericFloatTest, TrigonometricFunctions) {
    EXPECT_NEAR(F32::sin(F32::PI / 6), 0.5f, 1e-6f);
    EXPECT_NEAR(F32::cos(F32::PI / 3), 0.5f, 1e-6f);
    EXPECT_NEAR(F32::tan(F32::PI / 4), 1.0f, 1e-6f);

    EXPECT_NEAR(F32::asin(0.5f), F32::PI / 6, 1e-6f);
    EXPECT_NEAR(F32::acos(0.5f), F32::PI / 3, 1e-6f);
    EXPECT_NEAR(F32::atan(1.0f), F32::PI / 4, 1e-6f);
    EXPECT_NEAR(F32::atan2(1.0f, 1.0f), F32::PI / 4, 1e-6f);

    EXPECT_NEAR(F32::sinh(1.0f), (F32::exp(1.0f) - F32::exp(-1.0f)) / 2, 1e-6f);
    EXPECT_NEAR(F32::cosh(1.0f), (F32::exp(1.0f) + F32::exp(-1.0f)) / 2, 1e-6f);
    EXPECT_NEAR(F32::tanh(1.0f), F32::sinh(1.0f) / F32::cosh(1.0f), 1e-6f);

    EXPECT_NEAR(F32::asinh(1.0f), std::asinh(1.0f), 1e-6f);
    EXPECT_NEAR(F32::acosh(2.0f), std::acosh(2.0f), 1e-6f);
    EXPECT_NEAR(F32::atanh(0.5f), std::atanh(0.5f), 1e-6f);
}

TEST(RustNumericFloatTest, ComparisonFunctions) {
    EXPECT_TRUE(F32::approx_eq(1.0f, 1.0f + F32::EPSILON / 2));
    EXPECT_FALSE(F32::approx_eq(1.0f, 1.1f));

    EXPECT_EQ(F32::total_cmp(1.0f, 2.0f), -1);
    EXPECT_EQ(F32::total_cmp(2.0f, 1.0f), 1);
    EXPECT_EQ(F32::total_cmp(1.0f, 1.0f), 0);
    EXPECT_EQ(F32::total_cmp(F32::NAN, F32::NAN), 0);
    EXPECT_EQ(F32::total_cmp(1.0f, F32::NAN), -1);
    EXPECT_EQ(F32::total_cmp(F32::NAN, 1.0f), 1);

    EXPECT_FLOAT_EQ(F32::min(1.0f, 2.0f), 1.0f);
    EXPECT_FLOAT_EQ(F32::min(F32::NAN, 1.0f), 1.0f);

    EXPECT_FLOAT_EQ(F32::max(1.0f, 2.0f), 2.0f);
    EXPECT_FLOAT_EQ(F32::max(F32::NAN, 1.0f), 1.0f);

    EXPECT_FLOAT_EQ(F32::clamp(1.5f, 1.0f, 2.0f), 1.5f);
    EXPECT_FLOAT_EQ(F32::clamp(0.5f, 1.0f, 2.0f), 1.0f);
    EXPECT_FLOAT_EQ(F32::clamp(2.5f, 1.0f, 2.0f), 2.0f);
    EXPECT_FLOAT_EQ(F32::clamp(F32::NAN, 1.0f, 2.0f), 1.0f);
}

TEST(RustNumericFloatTest, UtilityFunctions) {
    // Random
    float random_value = F32::random(1.0f, 100.0f);
    EXPECT_GE(random_value, 1.0f);
    EXPECT_LE(random_value, 100.0f);

    // modf
    auto [int_part, frac_part] = F32::modf(42.75f);
    EXPECT_FLOAT_EQ(int_part, 42.0f);
    EXPECT_FLOAT_EQ(frac_part, 0.75f);

    // copysign
    EXPECT_FLOAT_EQ(F32::copysign(42.0f, -1.0f), -42.0f);
    EXPECT_FLOAT_EQ(F32::copysign(-42.0f, 1.0f), 42.0f);

    // next_up, next_down, ulp
    EXPECT_GT(F32::next_up(0.0f), 0.0f);
    EXPECT_LT(F32::next_down(0.0f), 0.0f);
    EXPECT_GT(F32::ulp(1.0f), 0.0f);

    // Angle conversion
    EXPECT_NEAR(F32::to_radians(180.0f), F32::PI, 1e-6f);
    EXPECT_NEAR(F32::to_degrees(F32::PI), 180.0f, 1e-6f);

    // Hypot
    EXPECT_FLOAT_EQ(F32::hypot(3.0f, 4.0f), 5.0f);
    EXPECT_FLOAT_EQ(F32::hypot(3.0f, 4.0f, 12.0f), 13.0f);

    // Lerp
    EXPECT_FLOAT_EQ(F32::lerp(0.0f, 10.0f, 0.5f), 5.0f);

    // Sign
    EXPECT_FLOAT_EQ(F32::sign(42.0f), 1.0f);
    EXPECT_FLOAT_EQ(F32::sign(-42.0f), -1.0f);
    EXPECT_FLOAT_EQ(F32::sign(0.0f), 0.0f);
}

// ====================== Ord Tests ======================
TEST(RustNumericOrdTest, CompareFunction) {
    EXPECT_EQ(Ord<int>::compare(1, 2), Ordering::Less);
    EXPECT_EQ(Ord<int>::compare(2, 1), Ordering::Greater);
    EXPECT_EQ(Ord<int>::compare(1, 1), Ordering::Equal);
}

TEST(RustNumericOrdTest, ComparatorClass) {
    Ord<int>::Comparator int_cmp;
    EXPECT_TRUE(int_cmp(1, 2));
    EXPECT_FALSE(int_cmp(2, 1));
    EXPECT_FALSE(int_cmp(1, 1));
}

TEST(RustNumericOrdTest, ByKeyFunction) {
    struct Person {
        std::string name;
        int age;
    };

    Person alice{"Alice", 30};
    Person bob{"Bob", 25};

    auto age_cmp = Ord<Person>::by_key([](const Person& p) { return p.age; });

    EXPECT_FALSE(age_cmp(alice, bob));  // 30 < 25 is false
    EXPECT_TRUE(age_cmp(bob, alice));   // 25 < 30 is true
}

// ====================== Functional Util Tests ======================
TEST(RustNumericFunctionalTest, Map) {
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    std::vector<int> squares;

    for (int sq : map(numbers, [](int x) { return x * x; })) {
        squares.push_back(sq);
    }

    ASSERT_EQ(squares.size(), 5);
    EXPECT_EQ(squares[0], 1);
    EXPECT_EQ(squares[1], 4);
    EXPECT_EQ(squares[2], 9);
    EXPECT_EQ(squares[3], 16);
    EXPECT_EQ(squares[4], 25);
}

TEST(RustNumericFunctionalTest, Filter) {
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> evens;

    for (int n : filter(numbers, [](int x) { return x % 2 == 0; })) {
        evens.push_back(n);
    }

    ASSERT_EQ(evens.size(), 5);
    EXPECT_EQ(evens[0], 2);
    EXPECT_EQ(evens[1], 4);
    EXPECT_EQ(evens[2], 6);
    EXPECT_EQ(evens[3], 8);
    EXPECT_EQ(evens[4], 10);
}

TEST(RustNumericFunctionalTest, Enumerate) {
    std::vector<std::string> words = {"apple", "banana", "cherry"};
    std::vector<std::pair<size_t, std::string>> indexed_words;

    for (auto [idx, word] : enumerate(words)) {
        indexed_words.emplace_back(idx, word);
    }

    ASSERT_EQ(indexed_words.size(), 3);
    EXPECT_EQ(indexed_words[0].first, 0);
    EXPECT_EQ(indexed_words[0].second, "apple");
    EXPECT_EQ(indexed_words[1].first, 1);
    EXPECT_EQ(indexed_words[1].second, "banana");
    EXPECT_EQ(indexed_words[2].first, 2);
    EXPECT_EQ(indexed_words[2].second, "cherry");
}
