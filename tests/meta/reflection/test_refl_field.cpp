/*!
 * \file test_refl_field.cpp
 * \brief Comprehensive tests for atom::meta FieldBase (shared reflection field)
 * \author Max Qian <lightapt.com>
 * \date 2024
 * \copyright Copyright (C) 2023-2024 Max Qian
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/meta/refl_field.hpp"

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace atom::meta::test {

//==============================================================================
// Test Structures
//==============================================================================

struct Person {
    int age;
    std::string name;
    std::vector<int> scores;
};

// A derived field type that adds its own deducing-this builder. Used to verify
// that the FieldBase builders preserve the *derived* type when chaining, which
// is the entire reason the builders use C++23 deducing this.
template <typename T, typename MemberType>
struct DerivedField : FieldBase<T, MemberType> {
    using Base = FieldBase<T, MemberType>;
    using Base::Base;

    const char* tag = nullptr;

    template <typename Self>
    auto&& withTag(this Self&& self, const char* t) {
        self.tag = t;
        return std::forward<Self>(self);
    }
};

//==============================================================================
// Construction
//==============================================================================

TEST(ReflFieldTest, MinimalConstructionDefaults) {
    FieldBase<Person, int> field("age", &Person::age);

    EXPECT_STREQ(field.name, "age");
    EXPECT_EQ(field.member, &Person::age);
    EXPECT_TRUE(field.required);
    EXPECT_EQ(field.default_value, 0);
    EXPECT_FALSE(static_cast<bool>(field.validator));
    EXPECT_EQ(field.description, nullptr);
    EXPECT_FALSE(field.deprecated);
    EXPECT_EQ(field.version, 1);
}

TEST(ReflFieldTest, FullConstruction) {
    auto validator = [](const int& v) { return v >= 0; };
    FieldBase<Person, int> field("age", &Person::age, /*required=*/false,
                                 /*def=*/42, validator);

    EXPECT_STREQ(field.name, "age");
    EXPECT_FALSE(field.required);
    EXPECT_EQ(field.default_value, 42);
    EXPECT_TRUE(static_cast<bool>(field.validator));
}

TEST(ReflFieldTest, StringMemberType) {
    FieldBase<Person, std::string> field("name", &Person::name, true,
                                         std::string{"unknown"});

    EXPECT_STREQ(field.name, "name");
    EXPECT_EQ(field.member, &Person::name);
    EXPECT_EQ(field.default_value, "unknown");
}

TEST(ReflFieldTest, ContainerMemberTypeMovesDefault) {
    std::vector<int> def{1, 2, 3};
    FieldBase<Person, std::vector<int>> field("scores", &Person::scores, false,
                                              std::move(def));

    EXPECT_EQ(field.member, &Person::scores);
    EXPECT_THAT(field.default_value, ::testing::ElementsAre(1, 2, 3));
}

//==============================================================================
// Member pointer binding
//==============================================================================

TEST(ReflFieldTest, MemberPointerReadsAndWritesInstance) {
    FieldBase<Person, int> field("age", &Person::age);

    Person p{.age = 10, .name = "x", .scores = {}};
    EXPECT_EQ(p.*(field.member), 10);

    p.*(field.member) = 99;
    EXPECT_EQ(p.age, 99);
}

//==============================================================================
// validate()
//==============================================================================

TEST(ReflFieldTest, ValidateTrueWithoutValidator) {
    FieldBase<Person, int> field("age", &Person::age);
    EXPECT_TRUE(field.validate(-100));
    EXPECT_TRUE(field.validate(0));
    EXPECT_TRUE(field.validate(100));
}

TEST(ReflFieldTest, ValidateUsesValidatorResult) {
    FieldBase<Person, int> field("age", &Person::age, true, 0,
                                 [](const int& v) { return v >= 0; });

    EXPECT_TRUE(field.validate(0));
    EXPECT_TRUE(field.validate(123));
    EXPECT_FALSE(field.validate(-1));
}

TEST(ReflFieldTest, ValidateWithStatefulValidator) {
    int calls = 0;
    FieldBase<Person, std::string> field(
        "name", &Person::name, true, std::string{},
        [&calls](const std::string& s) {
            ++calls;
            return !s.empty();
        });

    EXPECT_FALSE(field.validate(""));
    EXPECT_TRUE(field.validate("ok"));
    EXPECT_EQ(calls, 2);
}

//==============================================================================
// Builders (deducing this)
//==============================================================================

TEST(ReflFieldTest, WithDescriptionSetsAndReturnsSelf) {
    FieldBase<Person, int> field("age", &Person::age);
    auto&& ref = field.withDescription("the person's age");

    EXPECT_STREQ(field.description, "the person's age");
    EXPECT_EQ(&ref, &field);  // returns reference to the same object
}

TEST(ReflFieldTest, WithDeprecatedDefaultsToTrue) {
    FieldBase<Person, int> field("age", &Person::age);

    field.withDeprecated();
    EXPECT_TRUE(field.deprecated);

    field.withDeprecated(false);
    EXPECT_FALSE(field.deprecated);
}

TEST(ReflFieldTest, WithVersionSetsValue) {
    FieldBase<Person, int> field("age", &Person::age);
    field.withVersion(7);
    EXPECT_EQ(field.version, 7);
}

TEST(ReflFieldTest, BuildersChainOnBase) {
    FieldBase<Person, int> field("age", &Person::age);
    field.withDescription("d").withDeprecated(true).withVersion(3);

    EXPECT_STREQ(field.description, "d");
    EXPECT_TRUE(field.deprecated);
    EXPECT_EQ(field.version, 3);
}

TEST(ReflFieldTest, BuildersOnRvaluePreserveValues) {
    auto field = FieldBase<Person, int>("age", &Person::age)
                     .withDescription("rv")
                     .withVersion(9);
    EXPECT_STREQ(field.description, "rv");
    EXPECT_EQ(field.version, 9);
}

//==============================================================================
// Deducing-this preserves the derived type through chaining
//==============================================================================

TEST(ReflFieldTest, DerivedBuilderChainKeepsDerivedType) {
    DerivedField<Person, int> field("age", &Person::age);

    // withDescription is declared on the base but, thanks to deducing this,
    // returns a reference to the *derived* type, so withTag remains callable.
    field.withDescription("derived").withTag("primary");

    EXPECT_STREQ(field.description, "derived");
    EXPECT_STREQ(field.tag, "primary");
}

TEST(ReflFieldTest, DerivedBuilderReturnTypeIsDerived) {
    DerivedField<Person, int> field("age", &Person::age);
    using Returned = decltype(field.withVersion(1));
    static_assert(
        std::is_same_v<std::remove_reference_t<Returned>,
                       DerivedField<Person, int>>,
        "deducing-this builder must return the derived field type");
    EXPECT_EQ(field.version, 1);
}

TEST(ReflFieldTest, DerivedFieldInheritsValidate) {
    DerivedField<Person, int> field("age", &Person::age, true, 0,
                                    [](const int& v) { return v < 10; });
    EXPECT_TRUE(field.validate(5));
    EXPECT_FALSE(field.validate(20));
}

}  // namespace atom::meta::test
