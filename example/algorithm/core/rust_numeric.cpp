#include "atom/algorithm/rust_numeric.hpp"

#include <iostream>
#include <string>

using namespace atom::algorithm;

// Helper function to separate and label each example
void printHeader(const std::string& header) {
    std::cout << "\n";
    std::cout << "=================================================\n";
    std::cout << header << "\n";
    std::cout << "=================================================\n";
}

int main() {
    // ========================= Basic Type Examples ===========================
    printHeader("Basic Type Examples");

    i8 int8_value = 100;
    i16 int16_value = 1000;
    i32 int32_value = 100000;
    i64 int64_value = 10000000000;

    u8 uint8_value = 200;
    u16 uint16_value = 60000;
    u32 uint32_value = 4000000000;
    u64 uint64_value = 18000000000000000000ULL;

    f32 float32_value = 3.14159f;
    f64 float64_value = 2.718281828459045;

    std::cout << "Integer types: i8=" << static_cast<int>(int8_value)
              << ", i16=" << int16_value << ", i32=" << int32_value
              << ", i64=" << int64_value << "\n";

    std::cout << "Unsigned types: u8=" << static_cast<unsigned>(uint8_value)
              << ", u16=" << uint16_value << ", u32=" << uint32_value
              << ", u64=" << uint64_value << "\n";

    std::cout << "Floating-point types: f32=" << float32_value
              << ", f64=" << float64_value << "\n";

    // ========================= Result and Option Examples
    // =========================
    printHeader("Result and Option Examples");

    // Result examples
    auto int_result = I32::from_str("123");
    if (int_result.is_ok()) {
        std::cout << "Successfully parsed integer: " << int_result.unwrap()
                  << "\n";
    }

    auto error_result = I32::from_str("not_a_number");
    if (error_result.is_err()) {
        std::cout << "Parsing failed: " << error_result.unwrap_err().to_string()
                  << "\n";
    }

    // Safe unwrapping
    int safe_value = error_result.unwrap_or(42);
    std::cout << "Using default value on failure: " << safe_value << "\n";

    // Option examples
    Option<i32> some_value = Option<i32>::some(123);
    Option<i32> no_value = Option<i32>::none();

    std::cout << "Option::some contains value: " << some_value.unwrap() << "\n";
    std::cout << "Option::none with default value: " << no_value.unwrap_or(456)
              << "\n";

    // Chaining operations
    auto mapped = some_value.map([](i32 value) { return value * 2; });
    std::cout << "Mapping result: " << mapped.unwrap() << "\n";

    // ========================= Integer Operations Examples
    // =========================
    printHeader("Integer Operations Examples");

    // Basic arithmetic checked operations
    std::cout << "Checked arithmetic operations:\n";

    auto add_result = I8::checked_add(100, 27);
    if (add_result.is_none()) {
        std::cout << "100 + 27 causes i8 overflow\n";
    }

    auto sub_result = I8::checked_sub(-100, 29);
    if (sub_result.is_none()) {
        std::cout << "-100 - 29 causes i8 overflow\n";
    }

    // Saturating operations
    std::cout << "Saturating operations:\n";
    i8 sat_add = I8::saturating_add(120, 120);  // Should saturate to 127
    i8 sat_sub = I8::saturating_sub(-120, 20);  // Should saturate to -128
    std::cout << "saturating_add(120, 120) = " << static_cast<int>(sat_add)
              << "\n";
    std::cout << "saturating_sub(-120, 20) = " << static_cast<int>(sat_sub)
              << "\n";

    // Wrapping operations
    std::cout << "Wrapping operations:\n";
    i8 wrap_add = I8::wrapping_add(120, 120);  // Should wrap around to negative
    std::cout << "wrapping_add(120, 120) = " << static_cast<int>(wrap_add)
              << "\n";

    // Bit operations
    std::cout << "Bit operations:\n";
    u8 value = 0b10110110;
    std::cout << "count_ones(0b10110110) = " << U8::count_ones(value) << "\n";
    std::cout << "leading_zeros(0b10110110) = " << U8::leading_zeros(value)
              << "\n";
    std::cout << "trailing_zeros(0b10110110) = " << U8::trailing_zeros(value)
              << "\n";
    std::cout << "rotate_left(0b10110110, 2) = "
              << U8::to_bin_string(U8::rotate_left(value, 2)) << "\n";

    // Mathematical methods
    std::cout << "Mathematical methods:\n";
    std::cout << "gcd(48, 18) = " << I32::gcd(48, 18) << "\n";
    std::cout << "lcm(12, 18) = " << I32::lcm(12, 18) << "\n";

    // String conversions
    std::cout << "String conversions:\n";
    std::cout << "to_string(123, 2) = " << I32::to_string(123, 2) << "\n";
    std::cout << "to_string(123, 16) = " << I32::to_string(123, 16) << "\n";
    std::cout << "to_hex_string(123) = " << I32::to_hex_string(123) << "\n";
    std::cout << "to_bin_string(123) = " << I32::to_bin_string(123) << "\n";

    // ========================= Floating Point Operations Examples
    // =========================
    printHeader("Floating Point Operations Examples");

    // Constant values
    std::cout << "Floating point constants:\n";
    std::cout << "F32::PI = " << F32::PI << "\n";
    std::cout << "F32::E = " << F32::E << "\n";
    std::cout << "F32::INFINITY_VAL = " << F32::INFINITY_VAL << "\n";
    std::cout << "F32::NAN = " << F32::NAN << "\n";

    // Floating point operations and checks
    std::cout << "Floating point check operations:\n";
    f32 nan_val = F32::NAN;
    f32 inf_val = F32::INFINITY_VAL;
    f32 normal_val = 3.14159f;

    std::cout << "is_nan(NAN) = " << F32::is_nan(nan_val) << "\n";
    std::cout << "is_infinite(INFINITY) = " << F32::is_infinite(inf_val)
              << "\n";
    std::cout << "is_normal(3.14159) = " << F32::is_normal(normal_val) << "\n";

    // Math functions
    std::cout << "Floating point math functions:\n";
    std::cout << "sqrt(25.0) = " << F64::sqrt(25.0) << "\n";
    std::cout << "exp(1.0) = " << F64::exp(1.0) << "\n";
    std::cout << "ln(10.0) = " << F64::ln(10.0) << "\n";
    std::cout << "sin(F64::PI/2) = " << F64::sin(F64::PI / 2) << "\n";
    std::cout << "to_radians(180.0) = " << F64::to_radians(180.0) << "\n";
    std::cout << "to_degrees(F64::PI) = " << F64::to_degrees(F64::PI) << "\n";

    // Comparison and approximation
    std::cout << "Floating point comparison:\n";
    bool approx_equal = F64::approx_eq(0.1 + 0.2, 0.3, 1e-10);
    std::cout << "approx_eq(0.1 + 0.2, 0.3) = " << approx_equal << "\n";

    // String conversions
    std::cout << "Floating point string conversions:\n";
    auto float_result = F64::from_str("3.14159");
    if (float_result.is_ok()) {
        std::cout << "Parsed float: " << float_result.unwrap() << "\n";
    }

    std::cout << "to_string(F64::PI) = " << F64::to_string(F64::PI) << "\n";
    std::cout << "to_exp_string(0.000001) = " << F64::to_exp_string(0.000001)
              << "\n";

    // ========================= Range Examples =========================
    printHeader("Range Examples");

    // Basic ranges
    std::cout << "Basic ranges:\n";
    std::cout << "range(1, 5): ";
    for (int i : range(1, 5)) {
        std::cout << i << " ";
    }
    std::cout << "\n";

    std::cout << "range_inclusive(1, 5): ";
    for (int i : range_inclusive(1, 5)) {
        std::cout << i << " ";
    }
    std::cout << "\n";

    // Range properties
    auto r1 = range(1, 5);
    auto r2 = range_inclusive(10, 15);

    std::cout << "range(1, 5).len() = " << r1.len() << "\n";
    std::cout << "range(1, 5).contains(3) = " << r1.contains(3) << "\n";
    std::cout << "range(1, 5).contains(5) = " << r1.contains(5) << "\n";
    std::cout << "range_inclusive(10, 15).len() = " << r2.len() << "\n";

    // Empty range
    auto empty_range = range(10, 5);  // Empty range, start > end
    std::cout << "range(10, 5).is_empty() = " << empty_range.is_empty() << "\n";
    std::cout << "range(10, 5).len() = " << empty_range.len() << "\n";

    // ========================= Error Handling Examples
    // =========================
    printHeader("Error Handling Examples");

    // Integer parsing errors
    std::cout << "Integer parsing errors:\n";
    auto int_err1 = I32::from_str("abc");
    auto int_err2 = I32::from_str("2147483648");  // Out of range
    auto int_err3 = I32::from_str("");            // Empty string

    if (int_err1.is_err()) {
        std::cout << "Invalid character: " << int_err1.unwrap_err().to_string()
                  << "\n";
    }
    if (int_err2.is_err()) {
        std::cout << "Out of range: " << int_err2.unwrap_err().to_string()
                  << "\n";
    }
    if (int_err3.is_err()) {
        std::cout << "Empty string: " << int_err3.unwrap_err().to_string()
                  << "\n";
    }

    // Float parsing errors
    std::cout << "Float parsing errors:\n";
    auto float_err = F64::from_str("invalid");
    if (float_err.is_err()) {
        std::cout << "Invalid float: " << float_err.unwrap_err().to_string()
                  << "\n";
    }

    // Overflow errors
    std::cout << "Overflow errors:\n";
    try {
        auto overflow = Option<i8>::some(127).unwrap() + 1;
        std::cout << "Unchecked addition might cause overflow: "
                  << static_cast<int>(overflow) << "\n";
    } catch (const std::exception& e) {
        std::cout << "Caught exception: " << e.what() << "\n";
    }

    // Using Result and Option for safe operations
    auto safe_add = I8::checked_add(120, 10);
    if (safe_add.is_none()) {
        std::cout << "Overflow detected: 120 + 10 is too large for i8 type\n";
    } else {
        std::cout << "Safe addition successful: "
                  << static_cast<int>(safe_add.unwrap()) << "\n";
    }

    // Division by zero check
    std::cout << "Division by zero check:\n";
    auto div_result = I32::checked_div(10, 0);
    if (div_result.is_none()) {
        std::cout << "Division by zero handled safely\n";
    }

    try {
        auto div = I32::wrapping_div(10, 0);  // This will throw an exception
        std::cout << "Division result: " << div << "\n";
    } catch (const std::exception& e) {
        std::cout << "Division by zero exception: " << e.what() << "\n";
    }

    // ========================= Edge Cases Examples =========================
    printHeader("Edge Cases Examples");

    // Integer boundaries
    std::cout << "Integer boundaries:\n";
    std::cout << "I8::MIN = " << static_cast<int>(I8::MIN) << "\n";
    std::cout << "I8::MAX = " << static_cast<int>(I8::MAX) << "\n";
    std::cout << "I32::MIN = " << I32::MIN << "\n";
    std::cout << "I32::MAX = " << I32::MAX << "\n";

    // Boundary arithmetic
    std::cout << "Boundary arithmetic:\n";
    auto min_plus_1 = I8::checked_add(I8::MIN, 1);
    auto max_plus_1 = I8::checked_add(I8::MAX, 1);

    std::cout << "I8::MIN + 1 = " << static_cast<int>(min_plus_1.unwrap())
              << "\n";
    if (max_plus_1.is_none()) {
        std::cout << "I8::MAX + 1 causes overflow\n";
    }

    // Special cases with MIN value
    std::cout << "Special cases with MIN value:\n";
    auto abs_min = I8::checked_abs(I8::MIN);
    if (abs_min.is_none()) {
        std::cout << "abs(I8::MIN) causes overflow\n";
    }

    auto min_div_neg1 = I8::checked_div(I8::MIN, -1);
    if (min_div_neg1.is_none()) {
        std::cout << "I8::MIN / -1 causes overflow\n";
    }

    // Float boundaries
    std::cout << "Float boundaries:\n";
    std::cout << "F32::MIN approximately = " << F32::MIN << "\n";
    std::cout << "F32::MAX approximately = " << F32::MAX << "\n";
    std::cout << "F32::EPSILON = " << F32::EPSILON << "\n";

    // ====================== Helper Functions and Utilities
    // ======================
    printHeader("Helper Functions and Utilities");

    // Random numbers
    std::cout << "Random numbers:\n";
    std::cout << "I32::random(1, 100) = " << I32::random(1, 100) << "\n";
    std::cout << "F64::random() = " << F64::random() << "\n";

    // Numeric conversions
    std::cout << "Numeric conversions:\n";
    auto i32_to_i8 = IntMethods<i32>::try_into<i8>(100);
    if (i32_to_i8.is_some()) {
        std::cout << "i32(100) -> i8 successful: "
                  << static_cast<int>(i32_to_i8.unwrap()) << "\n";
    }

    auto large_to_i8 = IntMethods<i32>::try_into<i8>(1000);
    if (large_to_i8.is_none()) {
        std::cout << "i32(1000) -> i8 failed: value too large\n";
    }

    auto float_to_int = F64::try_into<i32>(123.45);
    if (float_to_int.is_some()) {
        std::cout << "f64(123.45) -> i32 successful: " << float_to_int.unwrap()
                  << "\n";
    }

    return 0;
}
