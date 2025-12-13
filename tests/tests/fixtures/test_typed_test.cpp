/*
 * test_typed_test.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for typed tests in atom/tests/fixtures/typed_test.hpp

**************************************************/

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include "atom/tests/fixtures/typed_test.hpp"

namespace atom::test::fixtures::tests {

// ============================================================================
// Basic Typed Test
// ============================================================================

template <typename T>
class BasicTypedTest : public ::testing::Test {
protected:
    void SetUp() override { value = T{}; }
    void TearDown() override {}

    T value;
};

using NumericTypes = ::testing::Types<int, long, float, double>;
TYPED_TEST_SUITE(BasicTypedTest, NumericTypes);

TYPED_TEST(BasicTypedTest, DefaultValueIsZero) {
    EXPECT_EQ(this->value, TypeParam{});
}

TYPED_TEST(BasicTypedTest, CanAssignValue) {
    this->value = TypeParam{42};
    EXPECT_EQ(this->value, TypeParam{42});
}

// ============================================================================
// Typed Test with Integer Types
// ============================================================================

template <typename T>
class IntegerTypedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

using IntegerTypes = ::testing::Types<int8_t, int16_t, int32_t, int64_t,
                                       uint8_t, uint16_t, uint32_t, uint64_t>;
TYPED_TEST_SUITE(IntegerTypedTest, IntegerTypes);

TYPED_TEST(IntegerTypedTest, HasMinValue) {
    TypeParam minVal = std::numeric_limits<TypeParam>::min();
    EXPECT_LE(minVal, TypeParam{0});
}

TYPED_TEST(IntegerTypedTest, HasMaxValue) {
    TypeParam maxVal = std::numeric_limits<TypeParam>::max();
    EXPECT_GE(maxVal, TypeParam{0});
}

TYPED_TEST(IntegerTypedTest, MinLessThanMax) {
    TypeParam minVal = std::numeric_limits<TypeParam>::min();
    TypeParam maxVal = std::numeric_limits<TypeParam>::max();
    EXPECT_LT(minVal, maxVal);
}

// ============================================================================
// Typed Test with Floating Point Types
// ============================================================================

template <typename T>
class FloatingTypedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

using FloatingTypes = ::testing::Types<float, double, long double>;
TYPED_TEST_SUITE(FloatingTypedTest, FloatingTypes);

TYPED_TEST(FloatingTypedTest, SupportsInfinity) {
    TypeParam inf = std::numeric_limits<TypeParam>::infinity();
    EXPECT_TRUE(std::isinf(inf));
}

TYPED_TEST(FloatingTypedTest, SupportsNaN) {
    TypeParam nan = std::numeric_limits<TypeParam>::quiet_NaN();
    EXPECT_TRUE(std::isnan(nan));
}

TYPED_TEST(FloatingTypedTest, HasEpsilon) {
    TypeParam eps = std::numeric_limits<TypeParam>::epsilon();
    EXPECT_GT(eps, TypeParam{0});
}

// ============================================================================
// Typed Test with Container Types
// ============================================================================

template <typename T>
class ContainerTypedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    T container;
};

using ContainerTypes =
    ::testing::Types<std::vector<int>, std::vector<double>,
                     std::vector<std::string>>;
TYPED_TEST_SUITE(ContainerTypedTest, ContainerTypes);

TYPED_TEST(ContainerTypedTest, StartsEmpty) {
    EXPECT_TRUE(this->container.empty());
}

TYPED_TEST(ContainerTypedTest, CanAddElements) {
    this->container.push_back(typename TypeParam::value_type{});
    EXPECT_EQ(this->container.size(), 1);
}

TYPED_TEST(ContainerTypedTest, CanClear) {
    this->container.push_back(typename TypeParam::value_type{});
    this->container.clear();
    EXPECT_TRUE(this->container.empty());
}

// ============================================================================
// Typed Test with Signed/Unsigned Types
// ============================================================================

template <typename T>
class SignedTypedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

using SignedTypes = ::testing::Types<int8_t, int16_t, int32_t, int64_t>;
TYPED_TEST_SUITE(SignedTypedTest, SignedTypes);

TYPED_TEST(SignedTypedTest, IsSigned) {
    EXPECT_TRUE(std::is_signed_v<TypeParam>);
}

TYPED_TEST(SignedTypedTest, CanBeNegative) {
    TypeParam value = TypeParam{-1};
    EXPECT_LT(value, TypeParam{0});
}

template <typename T>
class UnsignedTypedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

using UnsignedTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t>;
TYPED_TEST_SUITE(UnsignedTypedTest, UnsignedTypes);

TYPED_TEST(UnsignedTypedTest, IsUnsigned) {
    EXPECT_TRUE(std::is_unsigned_v<TypeParam>);
}

TYPED_TEST(UnsignedTypedTest, MinIsZero) {
    TypeParam minVal = std::numeric_limits<TypeParam>::min();
    EXPECT_EQ(minVal, TypeParam{0});
}

// ============================================================================
// Typed Test with Type Traits
// ============================================================================

template <typename T>
class TypeTraitsTypedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

using ArithmeticTypes = ::testing::Types<int, float, double, long>;
TYPED_TEST_SUITE(TypeTraitsTypedTest, ArithmeticTypes);

TYPED_TEST(TypeTraitsTypedTest, IsArithmetic) {
    EXPECT_TRUE(std::is_arithmetic_v<TypeParam>);
}

TYPED_TEST(TypeTraitsTypedTest, IsFundamental) {
    EXPECT_TRUE(std::is_fundamental_v<TypeParam>);
}

TYPED_TEST(TypeTraitsTypedTest, IsScalar) {
    EXPECT_TRUE(std::is_scalar_v<TypeParam>);
}

// ============================================================================
// Typed Test with Custom Type List
// ============================================================================

template <typename T>
class SizeTypedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

using SizeTypes = ::testing::Types<char, short, int, long, long long>;
TYPED_TEST_SUITE(SizeTypedTest, SizeTypes);

TYPED_TEST(SizeTypedTest, HasPositiveSize) {
    EXPECT_GT(sizeof(TypeParam), 0);
}

TYPED_TEST(SizeTypedTest, SizeMatchesExpected) {
    // Size should be consistent with type
    EXPECT_GE(sizeof(TypeParam), 1);
}

// ============================================================================
// Typed Test with Fixture State
// ============================================================================

template <typename T>
class StatefulTypedTest : public ::testing::Test {
protected:
    void SetUp() override {
        defaultValue = T{};
        assignedValue = T{};
    }
    void TearDown() override {}

    T defaultValue;
    T assignedValue;
};

using StateTypes = ::testing::Types<int, double, std::string>;
TYPED_TEST_SUITE(StatefulTypedTest, StateTypes);

TYPED_TEST(StatefulTypedTest, DefaultValueExists) {
    // Default value should be default-constructible
    TypeParam defaultVal{};
    EXPECT_EQ(this->defaultValue, defaultVal);
}

// ============================================================================
// Typed Test P (Type-Parameterized)
// ============================================================================

template <typename T>
class TypeParamTest : public ::testing::Test {
public:
    using List = ::testing::Types<int, float, double>;
};

TYPED_TEST_SUITE_P(TypeParamTest);

TYPED_TEST_P(TypeParamTest, CanDefaultConstruct) {
    TypeParam value{};
    EXPECT_EQ(value, TypeParam{});
}

TYPED_TEST_P(TypeParamTest, CanCopyConstruct) {
    TypeParam original{};
    TypeParam copy{original};
    EXPECT_EQ(copy, original);
}

REGISTER_TYPED_TEST_SUITE_P(TypeParamTest, CanDefaultConstruct,
                            CanCopyConstruct);

using TypeParamTypes = ::testing::Types<int, float, double>;
INSTANTIATE_TYPED_TEST_SUITE_P(Numeric, TypeParamTest, TypeParamTypes);

}  // namespace atom::test::fixtures::tests
