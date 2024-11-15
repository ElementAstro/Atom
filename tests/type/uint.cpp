#ifndef ATOM_TYPE_TEST_UINT_HPP
#define ATOM_TYPE_TEST_UINT_HPP

#include <gtest/gtest.h>

#include "atom/type/uint.hpp"

TEST(UintTest, Literal_u8) {
    EXPECT_EQ(255_u8, static_cast<uint8_t>(255));
    EXPECT_THROW(256_u8, std::out_of_range);
}

TEST(UintTest, Literal_u16) {
    EXPECT_EQ(65535_u16, static_cast<uint16_t>(65535));
    EXPECT_THROW(65536_u16, std::out_of_range);
}

TEST(UintTest, Literal_u32) {
    EXPECT_EQ(4294967295_u32, static_cast<uint32_t>(4294967295));
    EXPECT_THROW(4294967296_u32, std::out_of_range);
}

TEST(UintTest, Literal_u64) {
    EXPECT_EQ(18446744073709551615_u64,
              static_cast<uint64_t>(18446744073709551615ULL));
    // No exception expected for uint64_t as it can hold very large values
}

#endif  // ATOM_TYPE_TEST_UINT_HPP