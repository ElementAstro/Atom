#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

#include "atom/type/uint.hpp"

namespace {

// Test fixture for uint literals
class UintLiteralsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test valid conversions for uint8_t
TEST_F(UintLiteralsTest, Uint8Valid) {
    auto value1 = 42_u8;
    static_assert(std::is_same_v<decltype(value1), uint8_t>,
                  "Type should be uint8_t");
    EXPECT_EQ(value1, 42);

    auto value2 = 0_u8;
    EXPECT_EQ(value2, 0);

    auto value3 = 255_u8;  // Maximum value
    EXPECT_EQ(value3, MAX_UINT8);
    EXPECT_EQ(value3, std::numeric_limits<uint8_t>::max());

    // Different bases
    auto hex_value = 0xFF_u8;
    EXPECT_EQ(hex_value, 255);

    auto oct_value = 0177_u8;
    EXPECT_EQ(oct_value, 127);

    auto bin_value = 0b11111111_u8;
    EXPECT_EQ(bin_value, 255);
}

// Test range errors for uint8_t
TEST_F(UintLiteralsTest, Uint8RangeError) {
    // Values exceeding uint8_t range should throw
    EXPECT_THROW(256_u8, std::out_of_range);
    EXPECT_THROW(1000_u8, std::out_of_range);
    EXPECT_THROW(0xFFF_u8, std::out_of_range);
}

// Test valid conversions for uint16_t
TEST_F(UintLiteralsTest, Uint16Valid) {
    auto value1 = 42_u16;
    static_assert(std::is_same_v<decltype(value1), uint16_t>,
                  "Type should be uint16_t");
    EXPECT_EQ(value1, 42);

    auto value2 = 0_u16;
    EXPECT_EQ(value2, 0);

    auto value3 = 65535_u16;  // Maximum value
    EXPECT_EQ(value3, MAX_UINT16);
    EXPECT_EQ(value3, std::numeric_limits<uint16_t>::max());

    // Different bases
    auto hex_value = 0xFFFF_u16;
    EXPECT_EQ(hex_value, 65535);

    auto oct_value = 0177777_u16;
    EXPECT_EQ(oct_value, 65535);

    auto bin_value = 0b1111111111111111_u16;
    EXPECT_EQ(bin_value, 65535);
}

// Test range errors for uint16_t
TEST_F(UintLiteralsTest, Uint16RangeError) {
    // Values exceeding uint16_t range should throw
    EXPECT_THROW(65536_u16, std::out_of_range);
    EXPECT_THROW(100000_u16, std::out_of_range);
    EXPECT_THROW(0x10000_u16, std::out_of_range);
}

// Test valid conversions for uint32_t
TEST_F(UintLiteralsTest, Uint32Valid) {
    auto value1 = 42_u32;
    static_assert(std::is_same_v<decltype(value1), uint32_t>,
                  "Type should be uint32_t");
    EXPECT_EQ(value1, 42);

    auto value2 = 0_u32;
    EXPECT_EQ(value2, 0);

    auto value3 = 4294967295_u32;  // Maximum value
    EXPECT_EQ(value3, MAX_UINT32);
    EXPECT_EQ(value3, std::numeric_limits<uint32_t>::max());

    // Different bases
    auto hex_value = 0xFFFFFFFF_u32;
    EXPECT_EQ(hex_value, 4294967295U);

    auto oct_value = 037777777777_u32;
    EXPECT_EQ(oct_value, 4294967295U);
}

// Test range errors for uint32_t
TEST_F(UintLiteralsTest, Uint32RangeError) {
    // Values exceeding uint32_t range should throw
    EXPECT_THROW(4294967296_u32, std::out_of_range);
    EXPECT_THROW(0x100000000_u32, std::out_of_range);
}

// Test valid conversions for uint64_t
TEST_F(UintLiteralsTest, Uint64Valid) {
    auto value1 = 42_u64;
    static_assert(std::is_same_v<decltype(value1), uint64_t>,
                  "Type should be uint64_t");
    EXPECT_EQ(value1, 42);

    auto value2 = 0_u64;
    EXPECT_EQ(value2, 0);

    auto value3 = 18446744073709551615_u64;  // Maximum value
    EXPECT_EQ(value3, std::numeric_limits<uint64_t>::max());

    // Different bases
    auto hex_value = 0xFFFFFFFFFFFFFFFF_u64;
    EXPECT_EQ(hex_value, std::numeric_limits<uint64_t>::max());

    // Larger values than unsigned long long will be handled by the compiler
    // and truncated to uint64_t max if necessary
}

// Test constexpr usage
TEST_F(UintLiteralsTest, ConstexprUsage) {
    // Literals should be usable in constexpr contexts
    constexpr auto constexpr_u8 = 123_u8;
    static_assert(constexpr_u8 == 123, "Constexpr uint8_t value mismatch");

    constexpr auto constexpr_u16 = 12345_u16;
    static_assert(constexpr_u16 == 12345, "Constexpr uint16_t value mismatch");

    constexpr auto constexpr_u32 = 123456789_u32;
    static_assert(constexpr_u32 == 123456789,
                  "Constexpr uint32_t value mismatch");

    constexpr auto constexpr_u64 = 1234567890123456789_u64;
    static_assert(constexpr_u64 == 1234567890123456789ULL,
                  "Constexpr uint64_t value mismatch");
}

// Test literal usage in expressions
TEST_F(UintLiteralsTest, LiteralInExpressions) {
    // Literals should work in expressions
    auto sum_u8 = 100_u8 + 50_u8;
    EXPECT_EQ(sum_u8, 150);
    static_assert(std::is_same_v<decltype(sum_u8), int>,
                  "Expression should yield int due to integer promotion");

    auto sum_u16 = 1000_u16 + 2000_u16;
    EXPECT_EQ(sum_u16, 3000);
    static_assert(std::is_same_v<decltype(sum_u16), int>,
                  "Expression should yield int due to integer promotion");

    auto sum_u32 = 1000000_u32 + 2000000_u32;
    EXPECT_EQ(sum_u32, 3000000);
    static_assert(std::is_same_v<decltype(sum_u32), uint32_t>,
                  "Expression should yield uint32_t");

    auto sum_u64 = 1000000000000_u64 + 2000000000000_u64;
    EXPECT_EQ(sum_u64, 3000000000000ULL);
    static_assert(std::is_same_v<decltype(sum_u64), uint64_t>,
                  "Expression should yield uint64_t");
}

// Test comparison between literals and standard types
TEST_F(UintLiteralsTest, LiteralComparisons) {
    // Literals should compare correctly with standard types
    EXPECT_TRUE(42_u8 == static_cast<uint8_t>(42));
    EXPECT_TRUE(1000_u16 == static_cast<uint16_t>(1000));
    EXPECT_TRUE(100000_u32 == static_cast<uint32_t>(100000));
    EXPECT_TRUE(10000000000_u64 == static_cast<uint64_t>(10000000000ULL));

    // Test type promotion in mixed expressions
    auto mixed1 = 10_u8 + 5;  // Promotes to int
    static_assert(std::is_same_v<decltype(mixed1), int>,
                  "Should promote to int");
    EXPECT_EQ(mixed1, 15);

    auto mixed2 = 1000_u16 * 2;  // Promotes to int
    static_assert(std::is_same_v<decltype(mixed2), int>,
                  "Should promote to int");
    EXPECT_EQ(mixed2, 2000);

    auto mixed3 = 100000_u32 + 1;  // Promotes to uint32_t (int is too small)
    static_assert(std::is_same_v<decltype(mixed3), uint32_t>,
                  "Should be uint32_t");
    EXPECT_EQ(mixed3, 100001U);

    auto mixed4 = 10000000000_u64 + 1;  // Promotes to uint64_t
    static_assert(std::is_same_v<decltype(mixed4), uint64_t>,
                  "Should be uint64_t");
    EXPECT_EQ(mixed4, 10000000001ULL);
}

// Test edge cases around maximum values
TEST_F(UintLiteralsTest, MaximumValueEdgeCases) {
    // Test values at and near the maximum
    auto max_u8 = 255_u8;
    EXPECT_EQ(max_u8, 255);
    EXPECT_THROW(256_u8, std::out_of_range);
    EXPECT_NO_THROW(254_u8);

    auto max_u16 = 65535_u16;
    EXPECT_EQ(max_u16, 65535);
    EXPECT_THROW(65536_u16, std::out_of_range);
    EXPECT_NO_THROW(65534_u16);

    auto max_u32 = 4294967295_u32;
    EXPECT_EQ(max_u32, 4294967295U);
    EXPECT_THROW(4294967296_u32, std::out_of_range);
    EXPECT_NO_THROW(4294967294_u32);

    auto max_u64 = 18446744073709551615_u64;
    EXPECT_EQ(max_u64, std::numeric_limits<uint64_t>::max());
}

// Test zero value for all types
TEST_F(UintLiteralsTest, ZeroValue) {
    auto zero_u8 = 0_u8;
    EXPECT_EQ(zero_u8, 0);
    static_assert(std::is_same_v<decltype(zero_u8), uint8_t>,
                  "Zero should be uint8_t");

    auto zero_u16 = 0_u16;
    EXPECT_EQ(zero_u16, 0);
    static_assert(std::is_same_v<decltype(zero_u16), uint16_t>,
                  "Zero should be uint16_t");

    auto zero_u32 = 0_u32;
    EXPECT_EQ(zero_u32, 0);
    static_assert(std::is_same_v<decltype(zero_u32), uint32_t>,
                  "Zero should be uint32_t");

    auto zero_u64 = 0_u64;
    EXPECT_EQ(zero_u64, 0);
    static_assert(std::is_same_v<decltype(zero_u64), uint64_t>,
                  "Zero should be uint64_t");
}

// Test the MAX_* constants
TEST_F(UintLiteralsTest, MaxConstants) {
    EXPECT_EQ(MAX_UINT8, 0xFF);
    EXPECT_EQ(MAX_UINT8, std::numeric_limits<uint8_t>::max());

    EXPECT_EQ(MAX_UINT16, 0xFFFF);
    EXPECT_EQ(MAX_UINT16, std::numeric_limits<uint16_t>::max());

    EXPECT_EQ(MAX_UINT32, 0xFFFFFFFF);
    EXPECT_EQ(MAX_UINT32, std::numeric_limits<uint32_t>::max());

    // Verify the constants are of the right types
    static_assert(std::is_same_v<decltype(MAX_UINT8), const uint8_t>,
                  "MAX_UINT8 should be uint8_t");
    static_assert(std::is_same_v<decltype(MAX_UINT16), const uint16_t>,
                  "MAX_UINT16 should be uint16_t");
    static_assert(std::is_same_v<decltype(MAX_UINT32), const uint32_t>,
                  "MAX_UINT32 should be uint32_t");
}

}  // namespace