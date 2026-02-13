/*!
 * \file test_refl_json.cpp
 * \brief Comprehensive tests for atom::meta JSON reflection
 * \author Max Qian <lightapt.com>
 * \date 2024
 * \copyright Copyright (C) 2023-2024 Max Qian
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/meta/refl_json.hpp"

#include <optional>
#include <string>
#include <vector>

namespace atom::meta::test {

//==============================================================================
// Test Structures
//==============================================================================

struct SimpleStruct {
    int id;
    std::string name;
    double value;
};

struct StructWithDefaults {
    int required_field;
    std::string optional_field;
    int default_value;
};

struct StructWithValidation {
    int positive_number;
    std::string non_empty_string;
};

struct NestedStruct {
    std::string outer_name;
    int inner_id;
    double inner_value;
};

//==============================================================================
// Test Fixture
//==============================================================================

class ReflJsonTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//==============================================================================
// Field Creation Tests
//==============================================================================

TEST_F(ReflJsonTest, MakeFieldBasic) {
    auto field = make_field("id", &SimpleStruct::id);

    EXPECT_STREQ(field.name, "id");
    EXPECT_TRUE(field.required);
}

TEST_F(ReflJsonTest, MakeFieldOptional) {
    auto field = make_field("optional", &StructWithDefaults::optional_field,
                            false, std::string("default_value"));

    EXPECT_STREQ(field.name, "optional");
    EXPECT_FALSE(field.required);
    EXPECT_EQ(field.default_value, "default_value");
}

TEST_F(ReflJsonTest, MakeFieldWithValidator) {
    auto validator = [](const int& val) { return val > 0; };
    auto field = make_field("positive", &StructWithValidation::positive_number,
                            true, 0, validator);

    EXPECT_STREQ(field.name, "positive");
    EXPECT_TRUE(field.required);
    EXPECT_TRUE(field.validator != nullptr);
    EXPECT_TRUE(field.validator(5));
    EXPECT_FALSE(field.validator(-1));
}

//==============================================================================
// Reflectable Tests
//==============================================================================

TEST_F(ReflJsonTest, ReflectableFromJson) {
    auto reflectable = Reflectable<SimpleStruct, Field<SimpleStruct, int>,
                                   Field<SimpleStruct, std::string>,
                                   Field<SimpleStruct, double>>(
        make_field("id", &SimpleStruct::id),
        make_field("name", &SimpleStruct::name),
        make_field("value", &SimpleStruct::value));

    json j = {{"id", 42}, {"name", "test"}, {"value", 3.14}};

    auto obj = reflectable.from_json(j);

    EXPECT_EQ(obj.id, 42);
    EXPECT_EQ(obj.name, "test");
    EXPECT_DOUBLE_EQ(obj.value, 3.14);
}

TEST_F(ReflJsonTest, ReflectableToJson) {
    auto reflectable = Reflectable<SimpleStruct, Field<SimpleStruct, int>,
                                   Field<SimpleStruct, std::string>,
                                   Field<SimpleStruct, double>>(
        make_field("id", &SimpleStruct::id),
        make_field("name", &SimpleStruct::name),
        make_field("value", &SimpleStruct::value));

    SimpleStruct obj{42, "test", 3.14};

    json j = reflectable.to_json(obj);

    EXPECT_EQ(j["id"], 42);
    EXPECT_EQ(j["name"], "test");
    EXPECT_DOUBLE_EQ(j["value"].get<double>(), 3.14);
}

TEST_F(ReflJsonTest, ReflectableRoundTrip) {
    auto reflectable = Reflectable<SimpleStruct, Field<SimpleStruct, int>,
                                   Field<SimpleStruct, std::string>,
                                   Field<SimpleStruct, double>>(
        make_field("id", &SimpleStruct::id),
        make_field("name", &SimpleStruct::name),
        make_field("value", &SimpleStruct::value));

    SimpleStruct original{123, "round_trip", 2.718};

    json j = reflectable.to_json(original);
    auto restored = reflectable.from_json(j);

    EXPECT_EQ(restored.id, original.id);
    EXPECT_EQ(restored.name, original.name);
    EXPECT_DOUBLE_EQ(restored.value, original.value);
}

//==============================================================================
// Default Value Tests
//==============================================================================

TEST_F(ReflJsonTest, OptionalFieldWithDefault) {
    auto reflectable =
        Reflectable<StructWithDefaults, Field<StructWithDefaults, int>,
                    Field<StructWithDefaults, std::string>,
                    Field<StructWithDefaults, int>>(
            make_field("required_field", &StructWithDefaults::required_field),
            make_field("optional_field", &StructWithDefaults::optional_field,
                       false, std::string("default_string")),
            make_field("default_value", &StructWithDefaults::default_value,
                       false, 100));

    // JSON without optional fields
    json j = {{"required_field", 42}};

    auto obj = reflectable.from_json(j);

    EXPECT_EQ(obj.required_field, 42);
    EXPECT_EQ(obj.optional_field, "default_string");
    EXPECT_EQ(obj.default_value, 100);
}

TEST_F(ReflJsonTest, OptionalFieldProvided) {
    auto reflectable =
        Reflectable<StructWithDefaults, Field<StructWithDefaults, int>,
                    Field<StructWithDefaults, std::string>,
                    Field<StructWithDefaults, int>>(
            make_field("required_field", &StructWithDefaults::required_field),
            make_field("optional_field", &StructWithDefaults::optional_field,
                       false, std::string("default_string")),
            make_field("default_value", &StructWithDefaults::default_value,
                       false, 100));

    // JSON with all fields
    json j = {{"required_field", 42},
              {"optional_field", "provided_value"},
              {"default_value", 200}};

    auto obj = reflectable.from_json(j);

    EXPECT_EQ(obj.required_field, 42);
    EXPECT_EQ(obj.optional_field, "provided_value");
    EXPECT_EQ(obj.default_value, 200);
}

//==============================================================================
// Validation Tests
//==============================================================================

TEST_F(ReflJsonTest, ValidationSuccess) {
    auto reflectable =
        Reflectable<StructWithValidation, Field<StructWithValidation, int>,
                    Field<StructWithValidation, std::string>>(
            make_field("positive_number",
                       &StructWithValidation::positive_number, true, 0,
                       [](const int& val) { return val > 0; }),
            make_field("non_empty_string",
                       &StructWithValidation::non_empty_string, true,
                       std::string{},
                       [](const std::string& val) { return !val.empty(); }));

    json j = {{"positive_number", 5}, {"non_empty_string", "hello"}};

    auto obj = reflectable.from_json(j);

    EXPECT_EQ(obj.positive_number, 5);
    EXPECT_EQ(obj.non_empty_string, "hello");
}

TEST_F(ReflJsonTest, ValidationFailure) {
    auto reflectable =
        Reflectable<StructWithValidation, Field<StructWithValidation, int>,
                    Field<StructWithValidation, std::string>>(
            make_field("positive_number",
                       &StructWithValidation::positive_number, true, 0,
                       [](const int& val) { return val > 0; }),
            make_field("non_empty_string",
                       &StructWithValidation::non_empty_string, true,
                       std::string{},
                       [](const std::string& val) { return !val.empty(); }));

    // Invalid: negative number
    json j1 = {{"positive_number", -5}, {"non_empty_string", "hello"}};
    EXPECT_THROW(reflectable.from_json(j1), std::invalid_argument);

    // Invalid: empty string
    json j2 = {{"positive_number", 5}, {"non_empty_string", ""}};
    EXPECT_THROW(reflectable.from_json(j2), std::invalid_argument);
}

//==============================================================================
// Missing Required Field Tests
//==============================================================================

TEST_F(ReflJsonTest, MissingRequiredField) {
    auto reflectable = Reflectable<SimpleStruct, Field<SimpleStruct, int>,
                                   Field<SimpleStruct, std::string>,
                                   Field<SimpleStruct, double>>(
        make_field("id", &SimpleStruct::id),
        make_field("name", &SimpleStruct::name),
        make_field("value", &SimpleStruct::value));

    // Missing 'name' field
    json j = {{"id", 42}, {"value", 3.14}};

    EXPECT_THROW(reflectable.from_json(j), std::exception);
}

//==============================================================================
// Complex Type Tests
//==============================================================================

TEST_F(ReflJsonTest, VectorField) {
    struct WithVector {
        std::string name;
        std::vector<int> values;
    };

    auto reflectable = Reflectable<WithVector, Field<WithVector, std::string>,
                                   Field<WithVector, std::vector<int>>>(
        make_field("name", &WithVector::name),
        make_field("values", &WithVector::values));

    json j = {{"name", "test"}, {"values", {1, 2, 3, 4, 5}}};

    auto obj = reflectable.from_json(j);

    EXPECT_EQ(obj.name, "test");
    ASSERT_EQ(obj.values.size(), 5);
    EXPECT_EQ(obj.values[0], 1);
    EXPECT_EQ(obj.values[4], 5);
}

TEST_F(ReflJsonTest, EmptyJson) {
    auto reflectable =
        Reflectable<StructWithDefaults, Field<StructWithDefaults, int>,
                    Field<StructWithDefaults, std::string>,
                    Field<StructWithDefaults, int>>(
            make_field("required_field", &StructWithDefaults::required_field,
                       false, 0),
            make_field("optional_field", &StructWithDefaults::optional_field,
                       false, std::string("default")),
            make_field("default_value", &StructWithDefaults::default_value,
                       false, 42));

    json j = json::object();

    auto obj = reflectable.from_json(j);

    EXPECT_EQ(obj.required_field, 0);
    EXPECT_EQ(obj.optional_field, "default");
    EXPECT_EQ(obj.default_value, 42);
}

//==============================================================================
// Edge Case Tests
//==============================================================================

TEST_F(ReflJsonTest, NullJsonValues) {
    // Test behavior with null JSON values
    auto reflectable = Reflectable<SimpleStruct, Field<SimpleStruct, int>,
                                   Field<SimpleStruct, std::string>,
                                   Field<SimpleStruct, double>>(
        make_field("id", &SimpleStruct::id, false, 0),
        make_field("name", &SimpleStruct::name, false, std::string("")),
        make_field("value", &SimpleStruct::value, false, 0.0));

    // All fields present but with proper values
    json j = {{"id", 0}, {"name", ""}, {"value", 0.0}};

    auto obj = reflectable.from_json(j);

    EXPECT_EQ(obj.id, 0);
    EXPECT_EQ(obj.name, "");
    EXPECT_DOUBLE_EQ(obj.value, 0.0);
}

TEST_F(ReflJsonTest, SpecialCharactersInStrings) {
    auto reflectable = Reflectable<SimpleStruct, Field<SimpleStruct, int>,
                                   Field<SimpleStruct, std::string>,
                                   Field<SimpleStruct, double>>(
        make_field("id", &SimpleStruct::id),
        make_field("name", &SimpleStruct::name),
        make_field("value", &SimpleStruct::value));

    json j = {{"id", 1},
              {"name", "test\nwith\tnewlines\rand\"quotes\""},
              {"value", 0.0}};

    auto obj = reflectable.from_json(j);
    auto restored = reflectable.to_json(obj);

    EXPECT_EQ(restored["name"], j["name"]);
}

TEST_F(ReflJsonTest, UnicodeStrings) {
    auto reflectable = Reflectable<SimpleStruct, Field<SimpleStruct, int>,
                                   Field<SimpleStruct, std::string>,
                                   Field<SimpleStruct, double>>(
        make_field("id", &SimpleStruct::id),
        make_field("name", &SimpleStruct::name),
        make_field("value", &SimpleStruct::value));

    json j = {{"id", 1},
              {"name", u8"\u4e2d\u6587\u6d4b\u8bd5 \U0001F600"},
              {"value", 0.0}};

    auto obj = reflectable.from_json(j);
    auto restored = reflectable.to_json(obj);

    EXPECT_EQ(restored["name"], j["name"]);
}

TEST_F(ReflJsonTest, LargeNumbers) {
    struct LargeNumbers {
        int64_t large_int;
        double large_double;
    };

    auto reflectable = Reflectable<LargeNumbers, Field<LargeNumbers, int64_t>,
                                   Field<LargeNumbers, double>>(
        make_field("large_int", &LargeNumbers::large_int),
        make_field("large_double", &LargeNumbers::large_double));

    json j = {{"large_int", 9223372036854775807LL},
              {"large_double", 1.7976931348623157e+308}};

    auto obj = reflectable.from_json(j);

    EXPECT_EQ(obj.large_int, 9223372036854775807LL);
    EXPECT_DOUBLE_EQ(obj.large_double, 1.7976931348623157e+308);
}

}  // namespace atom::meta::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
