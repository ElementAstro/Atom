/*
 * test_aligned.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-1

Description: Tests for aligned storage validation utilities

**************************************************/

#ifndef ATOM_UTILS_TEST_ALIGNED_HPP
#define ATOM_UTILS_TEST_ALIGNED_HPP

#include <gtest/gtest.h>
#include <cstddef>
#include <type_traits>
#include "atom/utils/memory/aligned.hpp"

namespace atom::utils::test {

class AlignedStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup if needed
    }
    
    void TearDown() override {
        // Test cleanup if needed
    }
};

// Test basic aligned storage validation - valid cases
TEST_F(AlignedStorageTest, ValidAlignedStorage) {
    // Test case where storage is exactly the right size and alignment
    EXPECT_NO_THROW(
        ValidateAlignedStorage<8, 4, 8, 4> validator1;
        (void)validator1; // Suppress unused variable warning
    );
    
    // Test case where storage is larger than implementation
    EXPECT_NO_THROW(
        ValidateAlignedStorage<8, 4, 16, 4> validator2;
        (void)validator2;
    );

    // Test case where storage alignment is multiple of implementation alignment
    EXPECT_NO_THROW(
        ValidateAlignedStorage<8, 4, 8, 8> validator3;
        (void)validator3;
    );

    // Test case with larger alignment multiple
    EXPECT_NO_THROW(
        ValidateAlignedStorage<8, 2, 8, 8> validator4;
        (void)validator4;
    );
}

// Test aligned storage validation with common data types
TEST_F(AlignedStorageTest, CommonDataTypes) {
    // Test with int (typically 4 bytes, 4-byte aligned)
    EXPECT_NO_THROW(
        ValidateAlignedStorage<sizeof(int), alignof(int), sizeof(int), alignof(int)> intValidator;
        (void)intValidator;
    );

    // Test with double (typically 8 bytes, 8-byte aligned)
    EXPECT_NO_THROW(
        ValidateAlignedStorage<sizeof(double), alignof(double), sizeof(double), alignof(double)> doubleValidator;
        (void)doubleValidator;
    );

    // Test with char (1 byte, 1-byte aligned)
    EXPECT_NO_THROW(
        ValidateAlignedStorage<sizeof(char), alignof(char), sizeof(char), alignof(char)> charValidator;
        (void)charValidator;
    );

    // Test with pointer (typically 8 bytes on 64-bit, pointer-aligned)
    EXPECT_NO_THROW(
        ValidateAlignedStorage<sizeof(void*), alignof(void*), sizeof(void*), alignof(void*)> ptrValidator;
        (void)ptrValidator;
    );
}

// Test aligned storage validation with oversized storage
TEST_F(AlignedStorageTest, OversizedStorage) {
    // Storage much larger than implementation
    EXPECT_NO_THROW(
        ValidateAlignedStorage<4, 4, 64, 4> oversizedValidator1;
        (void)oversizedValidator1;
    );

    // Storage with much larger alignment
    EXPECT_NO_THROW(
        ValidateAlignedStorage<8, 4, 8, 32> oversizedValidator2;
        (void)oversizedValidator2;
    );

    // Both size and alignment oversized
    EXPECT_NO_THROW(
        ValidateAlignedStorage<8, 4, 64, 32> oversizedValidator3;
        (void)oversizedValidator3;
    );
}

// Test edge cases with minimum values
TEST_F(AlignedStorageTest, MinimumValues) {
    // Minimum possible sizes and alignments
    EXPECT_NO_THROW(
        ValidateAlignedStorage<1, 1, 1, 1> minValidator;
        (void)minValidator;
    );

    // Implementation size 1, storage larger
    EXPECT_NO_THROW(
        ValidateAlignedStorage<1, 1, 8, 1> minValidator2;
        (void)minValidator2;
    );

    // Implementation alignment 1, storage alignment larger
    EXPECT_NO_THROW(
        ValidateAlignedStorage<8, 1, 8, 8> minValidator3;
        (void)minValidator3;
    );
}

// Test with powers of 2 alignments (common in practice)
TEST_F(AlignedStorageTest, PowerOfTwoAlignments) {
    // Test various power-of-2 alignments
    EXPECT_NO_THROW([](){
        ValidateAlignedStorage<16, 1, 16, 1> align1;
        ValidateAlignedStorage<16, 2, 16, 2> align2;
        ValidateAlignedStorage<16, 4, 16, 4> align4;
        ValidateAlignedStorage<16, 8, 16, 8> align8;
        ValidateAlignedStorage<16, 16, 16, 16> align16;
        (void)align1; (void)align2; (void)align4; (void)align8; (void)align16;
    }());

    // Test alignment multiples
    EXPECT_NO_THROW([](){
        ValidateAlignedStorage<16, 2, 16, 4> alignMultiple1;  // 4 is multiple of 2
        ValidateAlignedStorage<16, 4, 16, 8> alignMultiple2;  // 8 is multiple of 4
        ValidateAlignedStorage<16, 8, 16, 16> alignMultiple3; // 16 is multiple of 8
        (void)alignMultiple1; (void)alignMultiple2; (void)alignMultiple3;
    }());
}

// Test template instantiation with different parameter combinations
TEST_F(AlignedStorageTest, TemplateInstantiation) {
    // Test that the template can be instantiated with various combinations
    using Validator1 = ValidateAlignedStorage<8, 4, 8, 4>;
    using Validator2 = ValidateAlignedStorage<16, 8, 32, 16>;
    using Validator3 = ValidateAlignedStorage<1, 1, 1024, 64>;
    
    // These should compile without issues
    static_assert(std::is_class_v<Validator1>);
    static_assert(std::is_class_v<Validator2>);
    static_assert(std::is_class_v<Validator3>);
    
    // Test instantiation
    EXPECT_NO_THROW([](){
        Validator1 v1;
        Validator2 v2;
        Validator3 v3;
        (void)v1; (void)v2; (void)v3;
    }());
}

// Test with realistic scenarios
TEST_F(AlignedStorageTest, RealisticScenarios) {
    // Scenario: Storing a struct in aligned storage
    struct TestStruct {
        int a;
        double b;
        char c;
    };
    
    EXPECT_NO_THROW(
        ValidateAlignedStorage<sizeof(TestStruct), alignof(TestStruct),
                             sizeof(TestStruct), alignof(TestStruct)> structValidator;
        (void)structValidator;
    );

    // Scenario: Over-aligned storage for SIMD operations
    EXPECT_NO_THROW(
        ValidateAlignedStorage<16, 4, 16, 16> simdValidator; // 16-byte aligned for SSE
        (void)simdValidator;
    );

    // Scenario: Cache-line aligned storage
    EXPECT_NO_THROW(
        ValidateAlignedStorage<32, 8, 64, 64> cacheLineValidator; // 64-byte cache line
        (void)cacheLineValidator;
    );
}

#ifdef ATOM_USE_BOOST
// Test Boost-specific functionality if available
TEST_F(AlignedStorageTest, BoostSpecificFeatures) {
    // Test that Boost static assertions work
    EXPECT_NO_THROW(
        ValidateAlignedStorage<8, 4, 8, 4> boostValidator;
        (void)boostValidator;
    );
    
    // The Boost version should provide additional type trait validations
    // These are compile-time checks, so if this compiles, the test passes
    SUCCEED() << "Boost-specific aligned storage validation compiled successfully";
}
#endif

// Test compile-time nature of validation
TEST_F(AlignedStorageTest, CompileTimeValidation) {
    // These validations should happen at compile time
    // If the code compiles, the validation passed
    
    constexpr bool test1 = std::is_class_v<ValidateAlignedStorage<8, 4, 8, 4>>;
    constexpr bool test2 = std::is_class_v<ValidateAlignedStorage<16, 8, 32, 16>>;
    constexpr bool test3 = std::is_class_v<ValidateAlignedStorage<1, 1, 1024, 1>>;
    
    EXPECT_TRUE(test1);
    EXPECT_TRUE(test2);
    EXPECT_TRUE(test3);
    
    // Test that the class is empty (no runtime overhead)
    EXPECT_EQ(sizeof(ValidateAlignedStorage<8, 4, 8, 4>), 1); // Empty class size
}

// Test documentation examples
TEST_F(AlignedStorageTest, DocumentationExamples) {
    // Examples that might appear in documentation
    
    // Example 1: Basic usage
    EXPECT_NO_THROW(
        ValidateAlignedStorage<sizeof(int), alignof(int), 16, 8> example1;
        (void)example1;
    );

    // Example 2: SIMD vector storage
    EXPECT_NO_THROW(
        ValidateAlignedStorage<16, 4, 16, 16> simdExample; // 4 floats, 16-byte aligned
        (void)simdExample;
    );

    // Example 3: Cache-friendly storage
    EXPECT_NO_THROW(
        ValidateAlignedStorage<32, 8, 64, 64> cacheExample; // Cache line aligned
        (void)cacheExample;
    );
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_ALIGNED_HPP
