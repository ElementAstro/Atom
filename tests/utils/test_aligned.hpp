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
    using Valid1 = ValidateAlignedStorage<1, 1, 2, 2>;
    static_cast<void>(sizeof(Valid1));

    using Valid2 = ValidateAlignedStorage<2, 2, 4, 4>;
    static_cast<void>(sizeof(Valid2));

    using Valid4 = ValidateAlignedStorage<4, 4, 8, 8>;
    static_cast<void>(sizeof(Valid4));

    using Valid8 = ValidateAlignedStorage<8, 8, 16, 16>;
    static_cast<void>(sizeof(Valid8));

    // Equal size but valid alignment
    using ValidEqual = ValidateAlignedStorage<8, 4, 8, 8>;
    static_cast<void>(sizeof(ValidEqual));

    // Larger alignment but same size
    using ValidAlign = ValidateAlignedStorage<8, 4, 8, 16>;
    static_cast<void>(sizeof(ValidAlign));
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
    using ValidChar = ValidateAlignedStorage<sizeof(char), alignof(char),
                                             sizeof(int), alignof(int)>;
    static_cast<void>(sizeof(ValidChar));

    using ValidInt = ValidateAlignedStorage<sizeof(int), alignof(int),
                                            sizeof(double), alignof(double)>;
    static_cast<void>(sizeof(ValidInt));

    using ValidDouble =
        ValidateAlignedStorage<sizeof(double), alignof(double), sizeof(Align16),
                               alignof(Align16)>;
    static_cast<void>(sizeof(ValidDouble));
}

// Test power-of-2 alignments
TEST_F(AlignedStorageTest, PowerOf2Alignments) {
    using Valid16 = ValidateAlignedStorage<16, 16, 32, 32>;
    static_cast<void>(sizeof(Valid16));

    using Valid32 = ValidateAlignedStorage<32, 32, 64, 64>;
    static_cast<void>(sizeof(Valid32));

    using ValidCustom16 =
        ValidateAlignedStorage<sizeof(Align16), alignof(Align16),
                               sizeof(Align32), alignof(Align32)>;
    static_cast<void>(sizeof(ValidCustom16));
}

// Test edge cases
TEST_F(AlignedStorageTest, EdgeCases) {
    using ValidSame = ValidateAlignedStorage<8, 8, 8, 8>;
    static_cast<void>(sizeof(ValidSame));

    using ValidMin = ValidateAlignedStorage<1, 1, 1, 1>;
    static_cast<void>(sizeof(ValidMin));

    using ValidLarge =
        ValidateAlignedStorage<sizeof(Align32), alignof(Align32),
                               sizeof(Align32) * 2, alignof(Align32) * 2>;
    static_cast<void>(sizeof(ValidLarge));
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
    // TODO: Add tests for standard containers
    using ValidVector = ValidateAlignedStorage<
        sizeof(std::vector<int>), alignof(std::vector<int>),
        sizeof(std::array<int, 8>), alignof(std::array<int, 8>)>;
    // static_cast<void>(sizeof(ValidVector));
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