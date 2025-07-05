// filepath: atom/extra/boost/test_charconv.hpp
#ifndef ATOM_EXTRA_BOOST_TEST_CHARCONV_HPP
#define ATOM_EXTRA_BOOST_TEST_CHARCONV_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>


#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include "atom/extra/boost/charconv.hpp"

namespace atom::extra::boost::test {

using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::StartsWith;
using ::testing::ThrowsMessage;
using ::testing::TypedEq;

class BoostCharConvTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup default format options for testing
        defaultOptions = FormatOptions{};

        // Setup custom format options for testing
        customOptions.format = NumberFormat::SCIENTIFIC;
        customOptions.precision = 5;
        customOptions.uppercase = true;
        customOptions.thousandsSeparator = ',';
    }

    // Default format options
    FormatOptions defaultOptions;

    // Custom format options
    FormatOptions customOptions;
};

// Test integer to string conversion
TEST_F(BoostCharConvTest, IntToString) {
    // Test with default options and base 10
    EXPECT_EQ(BoostCharConv::intToString(42), "42");
    EXPECT_EQ(BoostCharConv::intToString(-123), "-123");
    EXPECT_EQ(BoostCharConv::intToString(0), "0");

    // Test with different bases
    EXPECT_EQ(BoostCharConv::intToString(15, 16), "f");
    EXPECT_EQ(BoostCharConv::intToString(10, 2), "1010");
    EXPECT_EQ(BoostCharConv::intToString(9, 8), "11");

    // Test with uppercase option
    FormatOptions uppercaseOptions;
    uppercaseOptions.uppercase = true;
    EXPECT_EQ(BoostCharConv::intToString(255, 16, uppercaseOptions), "FF");

    // Test with thousands separator
    FormatOptions separatorOptions;
    separatorOptions.thousandsSeparator = ',';
    EXPECT_EQ(BoostCharConv::intToString(1234567, 10, separatorOptions),
              "1,234,567");

    // Test with custom options
    EXPECT_EQ(BoostCharConv::intToString(1234567, 10, customOptions),
              "1,234,567");

    // Test with different integer types
    EXPECT_EQ(BoostCharConv::intToString(static_cast<int8_t>(-128)), "-128");
    EXPECT_EQ(BoostCharConv::intToString(static_cast<uint16_t>(65535)),
              "65535");
    EXPECT_EQ(BoostCharConv::intToString(static_cast<int32_t>(-2147483648)),
              "-2147483648");
    EXPECT_EQ(BoostCharConv::intToString(
                  static_cast<uint64_t>(18446744073709551615ULL), 16),
              "ffffffffffffffff");
}

// Test floating-point to string conversion
TEST_F(BoostCharConvTest, FloatToString) {
    // Test with default options (general format)
    EXPECT_EQ(BoostCharConv::floatToString(3.14159), "3.14159");
    EXPECT_EQ(BoostCharConv::floatToString(-0.0001), "-0.0001");

    // Test with scientific format
    FormatOptions scientificOptions;
    scientificOptions.format = NumberFormat::SCIENTIFIC;
    std::string result =
        BoostCharConv::floatToString(3.14159, scientificOptions);
    EXPECT_TRUE(result.find("e") != std::string::npos);

    // Test with fixed format
    FormatOptions fixedOptions;
    fixedOptions.format = NumberFormat::FIXED;
    fixedOptions.precision = 2;
    EXPECT_EQ(BoostCharConv::floatToString(3.14159, fixedOptions), "3.14");

    // Test with hex format
    FormatOptions hexOptions;
    hexOptions.format = NumberFormat::HEX;
    EXPECT_TRUE(BoostCharConv::floatToString(42.5, hexOptions).find("0x") !=
                std::string::npos);

    // Test with precision
    FormatOptions precisionOptions;
    precisionOptions.precision = 3;
    EXPECT_EQ(BoostCharConv::floatToString(3.14159, precisionOptions), "3.14");

    // Test with thousands separator
    FormatOptions separatorOptions;
    separatorOptions.thousandsSeparator = ',';
    EXPECT_EQ(BoostCharConv::floatToString(1234.567, separatorOptions),
              "1,234.567");

    // Test with custom options
    std::string customResult =
        BoostCharConv::floatToString(1234.567, customOptions);
    EXPECT_TRUE(customResult.find("E") != std::string::npos);
    EXPECT_TRUE(customResult.find("1,234") != std::string::npos);

    // Test with different float types
    EXPECT_NO_THROW(BoostCharConv::floatToString(static_cast<float>(3.14f)));
    EXPECT_NO_THROW(
        BoostCharConv::floatToString(static_cast<long double>(3.14L)));
}

// Test string to integer conversion
TEST_F(BoostCharConvTest, StringToInt) {
    // Test with base 10
    EXPECT_EQ(BoostCharConv::stringToInt<int>("42"), 42);
    EXPECT_EQ(BoostCharConv::stringToInt<int>("-123"), -123);
    EXPECT_EQ(BoostCharConv::stringToInt<int>("0"), 0);

    // Test with different bases
    EXPECT_EQ(BoostCharConv::stringToInt<int>("f", 16), 15);
    EXPECT_EQ(BoostCharConv::stringToInt<int>("1010", 2), 10);
    EXPECT_EQ(BoostCharConv::stringToInt<int>("11", 8), 9);

    // Test with different integer types
    EXPECT_EQ(BoostCharConv::stringToInt<int8_t>("-128"), -128);
    EXPECT_EQ(BoostCharConv::stringToInt<uint16_t>("65535"), 65535);
    EXPECT_EQ(BoostCharConv::stringToInt<int32_t>("-2147483648"), -2147483648);
    EXPECT_EQ(BoostCharConv::stringToInt<uint64_t>("18446744073709551615"),
              18446744073709551615ULL);

    // Test error cases
    EXPECT_THROW(BoostCharConv::stringToInt<int>("not a number"),
                 std::runtime_error);
    EXPECT_THROW(BoostCharConv::stringToInt<int>("42.5"), std::runtime_error);
    EXPECT_THROW(BoostCharConv::stringToInt<int>(""), std::runtime_error);
}

// Test string to floating-point conversion
TEST_F(BoostCharConvTest, StringToFloat) {
    // Test basic conversions
    EXPECT_FLOAT_EQ(BoostCharConv::stringToFloat<float>("3.14159"), 3.14159f);
    EXPECT_DOUBLE_EQ(BoostCharConv::stringToFloat<double>("-0.0001"), -0.0001);

    // Test scientific notation
    EXPECT_DOUBLE_EQ(BoostCharConv::stringToFloat<double>("1.23e+5"), 123000.0);
    EXPECT_DOUBLE_EQ(BoostCharConv::stringToFloat<double>("1.23E-5"),
                     0.0000123);

    // Test different float types
    EXPECT_FLOAT_EQ(BoostCharConv::stringToFloat<float>("3.14"), 3.14f);
    EXPECT_DOUBLE_EQ(BoostCharConv::stringToFloat<double>("3.14159265359"),
                     3.14159265359);

    // Test error cases
    EXPECT_THROW(BoostCharConv::stringToFloat<double>("not a number"),
                 std::runtime_error);
    EXPECT_THROW(BoostCharConv::stringToFloat<double>(""), std::runtime_error);
}

// Test special floating-point values
TEST_F(BoostCharConvTest, SpecialFloatingPointValues) {
    // Test NaN
    double nan_value = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(BoostCharConv::specialValueToString(nan_value), "NaN");

    // Test positive infinity
    double pos_inf = std::numeric_limits<double>::infinity();
    EXPECT_EQ(BoostCharConv::specialValueToString(pos_inf), "Inf");

    // Test negative infinity
    double neg_inf = -std::numeric_limits<double>::infinity();
    EXPECT_EQ(BoostCharConv::specialValueToString(neg_inf), "-Inf");

    // Test normal value (should use toString internally)
    EXPECT_EQ(BoostCharConv::specialValueToString(3.14159),
              BoostCharConv::toString(3.14159));
}

// Test generic toString function
TEST_F(BoostCharConvTest, ToString) {
    // Test with integral types
    EXPECT_EQ(BoostCharConv::toString(42), "42");
    EXPECT_EQ(BoostCharConv::toString(255, FormatOptions{NumberFormat::HEX}),
              "ff");

    // Test with floating-point types
    EXPECT_EQ(BoostCharConv::toString(3.14159), "3.14159");

    // Test with custom options
    std::string result = BoostCharConv::toString(1234.567, customOptions);
    EXPECT_TRUE(result.find("E") != std::string::npos);
    EXPECT_TRUE(result.find("1,234") != std::string::npos);
}

// Test generic fromString function
TEST_F(BoostCharConvTest, FromString) {
    // Test with integral types
    EXPECT_EQ(BoostCharConv::fromString<int>("42"), 42);
    EXPECT_EQ(BoostCharConv::fromString<int>("ff", 16), 255);

    // Test with floating-point types
    EXPECT_FLOAT_EQ(BoostCharConv::fromString<float>("3.14159"), 3.14159f);
    EXPECT_DOUBLE_EQ(BoostCharConv::fromString<double>("1.23e-5"), 0.0000123);

    // Test error cases
    EXPECT_THROW(BoostCharConv::fromString<int>("not a number"),
                 std::runtime_error);
    EXPECT_THROW(BoostCharConv::fromString<double>(""), std::runtime_error);
}

// Test error cases
TEST_F(BoostCharConvTest, ErrorCases) {
    // Test integer conversion errors
    EXPECT_THROW(
        {
            try {
                BoostCharConv::stringToInt<int>("not a number");
            } catch (const std::runtime_error& e) {
                EXPECT_THAT(e.what(),
                            HasSubstr("String to int conversion failed"));
                throw;
            }
        },
        std::runtime_error);

    // Test floating-point conversion errors
    EXPECT_THROW(
        {
            try {
                BoostCharConv::stringToFloat<double>("not a number");
            } catch (const std::runtime_error& e) {
                EXPECT_THAT(e.what(),
                            HasSubstr("String to float conversion failed"));
                throw;
            }
        },
        std::runtime_error);

    // Test boundary conditions
    std::string veryLargeNumber(1000, '9');  // A string with 1000 '9's
    EXPECT_THROW(BoostCharConv::stringToInt<int>(veryLargeNumber),
                 std::runtime_error);
}

// Test private utility methods through public methods
TEST_F(BoostCharConvTest, PrivateUtilityMethods) {
    // Test thousands separator
    FormatOptions separatorOptions;
    separatorOptions.thousandsSeparator = ',';

    EXPECT_EQ(BoostCharConv::intToString(1234567, 10, separatorOptions),
              "1,234,567");
    EXPECT_EQ(BoostCharConv::floatToString(1234567.89, separatorOptions),
              "1,234,567.89");

    // Test uppercase conversion
    FormatOptions uppercaseOptions;
    uppercaseOptions.uppercase = true;

    EXPECT_EQ(BoostCharConv::intToString(255, 16, uppercaseOptions), "FF");
    std::string floatResult = BoostCharConv::floatToString(
        1.23e-5, FormatOptions{NumberFormat::SCIENTIFIC, 2, true});
    EXPECT_TRUE(floatResult.find("E-") != std::string::npos);
}

// Test with extreme values
TEST_F(BoostCharConvTest, ExtremeValues) {
    // Test with minimum and maximum integer values
    EXPECT_NO_THROW(
        BoostCharConv::intToString(std::numeric_limits<int>::min()));
    EXPECT_NO_THROW(
        BoostCharConv::intToString(std::numeric_limits<int>::max()));
    EXPECT_NO_THROW(
        BoostCharConv::intToString(std::numeric_limits<int64_t>::min()));
    EXPECT_NO_THROW(
        BoostCharConv::intToString(std::numeric_limits<int64_t>::max()));

    // Test with minimum and maximum floating-point values
    EXPECT_NO_THROW(
        BoostCharConv::floatToString(std::numeric_limits<double>::min()));
    EXPECT_NO_THROW(
        BoostCharConv::floatToString(std::numeric_limits<double>::max()));
    EXPECT_NO_THROW(
        BoostCharConv::floatToString(std::numeric_limits<double>::lowest()));
    EXPECT_NO_THROW(
        BoostCharConv::floatToString(std::numeric_limits<double>::epsilon()));

    // Test with denormalized values
    EXPECT_NO_THROW(BoostCharConv::floatToString(
        std::numeric_limits<double>::denorm_min()));
}

}  // namespace atom::extra::boost::test

#endif  // ATOM_EXTRA_BOOST_TEST_CHARCONV_HPP
