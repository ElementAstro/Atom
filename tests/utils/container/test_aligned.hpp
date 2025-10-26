#ifndef ATOM_UTILS_TEST_ALIGNED_HPP
#define ATOM_UTILS_TEST_ALIGNED_HPP

#include <gtest/gtest.h>
#include <array>
#include <type_traits>
#include "atom/utils/aligned.hpp"

namespace atom::utils::test {

// Helper trait to check if parameters would make a valid aligned storage
template <std::size_t ImplSize, std::size_t ImplAlign, std::size_t StorageSize,
          std::size_t StorageAlign>
struct IsValidAlignedStorage {
    static constexpr bool value =
        (StorageSize >= ImplSize) && (StorageAlign % ImplAlign == 0);
};

class AlignedStorageTest : public ::testing::Test {};

// Helper structs for testing different alignments
struct alignas(1) Align1 {
    char c;
};
struct alignas(2) Align2 {
    short s;
};
struct alignas(4) Align4 {
    int i;
};
struct alignas(8) Align8 {
    double d;
};
struct alignas(16) Align16 {
    std::array<double, 2> a;
};
struct alignas(32) Align32 {
    std::array<double, 4> a;
};

// Test valid size and alignment combinations
TEST_F(AlignedStorageTest, ValidSizeAndAlignment) {
    // Standard case - storage larger than implementation
    EXPECT_TRUE((IsValidAlignedStorage<1, 1, 2, 2>::value));
    EXPECT_TRUE((IsValidAlignedStorage<2, 2, 4, 4>::value));
    EXPECT_TRUE((IsValidAlignedStorage<4, 4, 8, 8>::value));
    EXPECT_TRUE((IsValidAlignedStorage<8, 8, 16, 16>::value));

    // Equal size but valid alignment
    EXPECT_TRUE((IsValidAlignedStorage<8, 4, 8, 8>::value));

    // Larger alignment but same size
    EXPECT_TRUE((IsValidAlignedStorage<8, 4, 8, 16>::value));
}

// Test invalid size combinations
TEST_F(AlignedStorageTest, InvalidSize) {
    EXPECT_FALSE((IsValidAlignedStorage<2, 1, 1, 1>::value));
    EXPECT_FALSE((IsValidAlignedStorage<4, 1, 2, 2>::value));
    EXPECT_FALSE((IsValidAlignedStorage<8, 1, 4, 4>::value));
}

// Test invalid alignment combinations
TEST_F(AlignedStorageTest, InvalidAlignment) {
    EXPECT_FALSE((IsValidAlignedStorage<1, 2, 2, 1>::value));
    EXPECT_FALSE((IsValidAlignedStorage<1, 4, 4, 2>::value));
    EXPECT_FALSE((IsValidAlignedStorage<1, 8, 8, 4>::value));
}

// Test with different types and their alignments
TEST_F(AlignedStorageTest, TypeAlignments) {
    EXPECT_TRUE((IsValidAlignedStorage<sizeof(char), alignof(char), sizeof(int),
                                       alignof(int)>::value));

    EXPECT_TRUE(
        (IsValidAlignedStorage<sizeof(int), alignof(int), sizeof(double),
                               alignof(double)>::value));

    EXPECT_TRUE(
        (IsValidAlignedStorage<sizeof(double), alignof(double), sizeof(Align16),
                               alignof(Align16)>::value));
}

// Test power-of-2 alignments
TEST_F(AlignedStorageTest, PowerOf2Alignments) {
    EXPECT_TRUE((IsValidAlignedStorage<16, 16, 32, 32>::value));
    EXPECT_TRUE((IsValidAlignedStorage<32, 32, 64, 64>::value));

    EXPECT_TRUE(
        (IsValidAlignedStorage<sizeof(Align16), alignof(Align16),
                               sizeof(Align32), alignof(Align32)>::value));
}

// Test edge cases
TEST_F(AlignedStorageTest, EdgeCases) {
    EXPECT_TRUE((IsValidAlignedStorage<8, 8, 8, 8>::value));
    EXPECT_TRUE((IsValidAlignedStorage<1, 1, 1, 1>::value));

    EXPECT_TRUE((IsValidAlignedStorage<sizeof(Align32), alignof(Align32),
                                       sizeof(Align32) * 2,
                                       alignof(Align32) * 2>::value));
}

#ifdef ATOM_USE_BOOST
// Additional tests when Boost is enabled
TEST_F(AlignedStorageTest, BoostSpecificValidations) {
    using ValidBoost = ValidateAlignedStorage<8, 8, 16, 16>;
    static_cast<void>(sizeof(ValidBoost));
}
#endif

// Test with standard containers
TEST_F(AlignedStorageTest, ContainerAlignments) {
    // Test container alignment validations
    constexpr bool validVector = IsValidAlignedStorage<
        sizeof(std::vector<int>), alignof(std::vector<int>),
        sizeof(std::array<int, 8>), alignof(std::array<int, 8>)>::value;
    static_cast<void>(validVector);
}

// Test compilation failure cases
struct CompilationFailureTests {
    // These should fail to compile:
    // using Invalid1 = ValidateAlignedStorage<2, 1, 1, 1>; // Size too small
    // using Invalid2 = ValidateAlignedStorage<1, 2, 2, 1>; // Alignment invalid
    // using Invalid3 = ValidateAlignedStorage<8, 8, 4, 4>; // Storage too small
};

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_ALIGNED_HPP
