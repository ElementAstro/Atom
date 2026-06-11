/*!
 * \file test_refl_yaml.cpp
 * \brief Comprehensive tests for atom::meta YAML reflection
 * \author Max Qian <lightapt.com>
 * \date 2024
 * \copyright Copyright (C) 2023-2024 Max Qian
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Only compile if yaml-cpp is available
#if __has_include(<yaml-cpp/yaml.h>)

#include "atom/meta/refl_yaml.hpp"

#include <optional>
#include <string>
#include <vector>

namespace atom::meta::test {

//==============================================================================
// Test Structures
//==============================================================================

struct SimpleYamlStruct {
    int id;
    std::string name;
    double value;
};

struct YamlStructWithDefaults {
    int required_field;
    std::string optional_field;
    int default_value;
};

struct YamlStructWithValidation {
    int positive_number;
    std::string non_empty_string;
};

//==============================================================================
// Test Fixture
//==============================================================================

class ReflYamlTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//==============================================================================
// Field Creation Tests
//==============================================================================

TEST_F(ReflYamlTest, MakeFieldBasic) {
    auto field = make_field("id", &SimpleYamlStruct::id);

    EXPECT_STREQ(field.name, "id");
    EXPECT_TRUE(field.required);
}

TEST_F(ReflYamlTest, MakeFieldOptional) {
    auto field = make_field("optional", &YamlStructWithDefaults::optional_field,
                            false, std::string("default_value"));

    EXPECT_STREQ(field.name, "optional");
    EXPECT_FALSE(field.required);
    EXPECT_EQ(field.default_value, "default_value");
}

TEST_F(ReflYamlTest, MakeFieldWithValidator) {
    auto validator = [](const int& val) { return val > 0; };
    auto field =
        make_field("positive", &YamlStructWithValidation::positive_number, true,
                   0, validator);

    EXPECT_STREQ(field.name, "positive");
    EXPECT_TRUE(field.required);
    EXPECT_TRUE(field.validator != nullptr);
    EXPECT_TRUE(field.validator(5));
    EXPECT_FALSE(field.validator(-1));
}

//==============================================================================
// Reflectable Tests
//==============================================================================

TEST_F(ReflYamlTest, ReflectableFromYaml) {
    auto reflectable =
        Reflectable<SimpleYamlStruct, Field<SimpleYamlStruct, int>,
                    Field<SimpleYamlStruct, std::string>,
                    Field<SimpleYamlStruct, double>>(
            make_field("id", &SimpleYamlStruct::id),
            make_field("name", &SimpleYamlStruct::name),
            make_field("value", &SimpleYamlStruct::value));

    YAML::Node node;
    node["id"] = 42;
    node["name"] = "test";
    node["value"] = 3.14;

    auto obj = reflectable.from_yaml(node);

    EXPECT_EQ(obj.id, 42);
    EXPECT_EQ(obj.name, "test");
    EXPECT_DOUBLE_EQ(obj.value, 3.14);
}

TEST_F(ReflYamlTest, ReflectableToYaml) {
    auto reflectable =
        Reflectable<SimpleYamlStruct, Field<SimpleYamlStruct, int>,
                    Field<SimpleYamlStruct, std::string>,
                    Field<SimpleYamlStruct, double>>(
            make_field("id", &SimpleYamlStruct::id),
            make_field("name", &SimpleYamlStruct::name),
            make_field("value", &SimpleYamlStruct::value));

    SimpleYamlStruct obj{42, "test", 3.14};

    YAML::Node node = reflectable.to_yaml(obj);

    EXPECT_EQ(node["id"].as<int>(), 42);
    EXPECT_EQ(node["name"].as<std::string>(), "test");
    EXPECT_DOUBLE_EQ(node["value"].as<double>(), 3.14);
}

TEST_F(ReflYamlTest, ReflectableRoundTrip) {
    auto reflectable =
        Reflectable<SimpleYamlStruct, Field<SimpleYamlStruct, int>,
                    Field<SimpleYamlStruct, std::string>,
                    Field<SimpleYamlStruct, double>>(
            make_field("id", &SimpleYamlStruct::id),
            make_field("name", &SimpleYamlStruct::name),
            make_field("value", &SimpleYamlStruct::value));

    SimpleYamlStruct original{123, "round_trip", 2.718};

    YAML::Node node = reflectable.to_yaml(original);
    auto restored = reflectable.from_yaml(node);

    EXPECT_EQ(restored.id, original.id);
    EXPECT_EQ(restored.name, original.name);
    EXPECT_DOUBLE_EQ(restored.value, original.value);
}

//==============================================================================
// Default Value Tests
//==============================================================================

TEST_F(ReflYamlTest, OptionalFieldWithDefault) {
    auto reflectable =
        Reflectable<YamlStructWithDefaults, Field<YamlStructWithDefaults, int>,
                    Field<YamlStructWithDefaults, std::string>,
                    Field<YamlStructWithDefaults, int>>(
            make_field("required_field",
                       &YamlStructWithDefaults::required_field),
            make_field("optional_field",
                       &YamlStructWithDefaults::optional_field, false,
                       std::string("default_string")),
            make_field("default_value", &YamlStructWithDefaults::default_value,
                       false, 100));

    // YAML without optional fields
    YAML::Node node;
    node["required_field"] = 42;

    auto obj = reflectable.from_yaml(node);

    EXPECT_EQ(obj.required_field, 42);
    EXPECT_EQ(obj.optional_field, "default_string");
    EXPECT_EQ(obj.default_value, 100);
}

TEST_F(ReflYamlTest, OptionalFieldProvided) {
    auto reflectable =
        Reflectable<YamlStructWithDefaults, Field<YamlStructWithDefaults, int>,
                    Field<YamlStructWithDefaults, std::string>,
                    Field<YamlStructWithDefaults, int>>(
            make_field("required_field",
                       &YamlStructWithDefaults::required_field),
            make_field("optional_field",
                       &YamlStructWithDefaults::optional_field, false,
                       std::string("default_string")),
            make_field("default_value", &YamlStructWithDefaults::default_value,
                       false, 100));

    // YAML with all fields
    YAML::Node node;
    node["required_field"] = 42;
    node["optional_field"] = "provided_value";
    node["default_value"] = 200;

    auto obj = reflectable.from_yaml(node);

    EXPECT_EQ(obj.required_field, 42);
    EXPECT_EQ(obj.optional_field, "provided_value");
    EXPECT_EQ(obj.default_value, 200);
}

//==============================================================================
// Validation Tests
//==============================================================================

TEST_F(ReflYamlTest, ValidationSuccess) {
    auto reflectable =
        Reflectable<YamlStructWithValidation,
                    Field<YamlStructWithValidation, int>,
                    Field<YamlStructWithValidation, std::string>>(
            make_field("positive_number",
                       &YamlStructWithValidation::positive_number, true, 0,
                       [](const int& val) { return val > 0; }),
            make_field("non_empty_string",
                       &YamlStructWithValidation::non_empty_string, true,
                       std::string{},
                       [](const std::string& val) { return !val.empty(); }));

    YAML::Node node;
    node["positive_number"] = 5;
    node["non_empty_string"] = "hello";

    auto obj = reflectable.from_yaml(node);

    EXPECT_EQ(obj.positive_number, 5);
    EXPECT_EQ(obj.non_empty_string, "hello");
}

TEST_F(ReflYamlTest, ValidationFailure) {
    auto reflectable =
        Reflectable<YamlStructWithValidation,
                    Field<YamlStructWithValidation, int>,
                    Field<YamlStructWithValidation, std::string>>(
            make_field("positive_number",
                       &YamlStructWithValidation::positive_number, true, 0,
                       [](const int& val) { return val > 0; }),
            make_field("non_empty_string",
                       &YamlStructWithValidation::non_empty_string, true,
                       std::string{},
                       [](const std::string& val) { return !val.empty(); }));

    // Invalid: negative number
    YAML::Node node1;
    node1["positive_number"] = -5;
    node1["non_empty_string"] = "hello";
    EXPECT_THROW(reflectable.from_yaml(node1), atom::error::InvalidArgument);

    // Invalid: empty string
    YAML::Node node2;
    node2["positive_number"] = 5;
    node2["non_empty_string"] = "";
    EXPECT_THROW(reflectable.from_yaml(node2), atom::error::InvalidArgument);
}

//==============================================================================
// Missing Required Field Tests
//==============================================================================

TEST_F(ReflYamlTest, MissingRequiredField) {
    auto reflectable =
        Reflectable<SimpleYamlStruct, Field<SimpleYamlStruct, int>,
                    Field<SimpleYamlStruct, std::string>,
                    Field<SimpleYamlStruct, double>>(
            make_field("id", &SimpleYamlStruct::id),
            make_field("name", &SimpleYamlStruct::name),
            make_field("value", &SimpleYamlStruct::value));

    // Missing 'name' field
    YAML::Node node;
    node["id"] = 42;
    node["value"] = 3.14;

    EXPECT_THROW(reflectable.from_yaml(node), std::exception);
}

//==============================================================================
// YAML String Parsing Tests
//==============================================================================

TEST_F(ReflYamlTest, ParseYamlString) {
    auto reflectable =
        Reflectable<SimpleYamlStruct, Field<SimpleYamlStruct, int>,
                    Field<SimpleYamlStruct, std::string>,
                    Field<SimpleYamlStruct, double>>(
            make_field("id", &SimpleYamlStruct::id),
            make_field("name", &SimpleYamlStruct::name),
            make_field("value", &SimpleYamlStruct::value));

    std::string yamlStr = R"(
id: 42
name: test
value: 3.14
)";

    YAML::Node node = YAML::Load(yamlStr);
    auto obj = reflectable.from_yaml(node);

    EXPECT_EQ(obj.id, 42);
    EXPECT_EQ(obj.name, "test");
    EXPECT_DOUBLE_EQ(obj.value, 3.14);
}

TEST_F(ReflYamlTest, EmitYamlString) {
    auto reflectable =
        Reflectable<SimpleYamlStruct, Field<SimpleYamlStruct, int>,
                    Field<SimpleYamlStruct, std::string>,
                    Field<SimpleYamlStruct, double>>(
            make_field("id", &SimpleYamlStruct::id),
            make_field("name", &SimpleYamlStruct::name),
            make_field("value", &SimpleYamlStruct::value));

    SimpleYamlStruct obj{42, "test", 3.14};
    YAML::Node node = reflectable.to_yaml(obj);

    YAML::Emitter emitter;
    emitter << node;
    std::string yamlStr = emitter.c_str();

    EXPECT_FALSE(yamlStr.empty());
    EXPECT_NE(yamlStr.find("id"), std::string::npos);
    EXPECT_NE(yamlStr.find("name"), std::string::npos);
    EXPECT_NE(yamlStr.find("value"), std::string::npos);
}

//==============================================================================
// Complex Type Tests
//==============================================================================

TEST_F(ReflYamlTest, VectorField) {
    struct WithVector {
        std::string name;
        std::vector<int> values;
    };

    auto reflectable = Reflectable<WithVector, Field<WithVector, std::string>,
                                   Field<WithVector, std::vector<int>>>(
        make_field("name", &WithVector::name),
        make_field("values", &WithVector::values));

    YAML::Node node;
    node["name"] = "test";
    node["values"].push_back(1);
    node["values"].push_back(2);
    node["values"].push_back(3);
    node["values"].push_back(4);
    node["values"].push_back(5);

    auto obj = reflectable.from_yaml(node);

    EXPECT_EQ(obj.name, "test");
    ASSERT_EQ(obj.values.size(), 5);
    EXPECT_EQ(obj.values[0], 1);
    EXPECT_EQ(obj.values[4], 5);
}

//==============================================================================
// Edge Case Tests
//==============================================================================

TEST_F(ReflYamlTest, SpecialCharactersInStrings) {
    auto reflectable =
        Reflectable<SimpleYamlStruct, Field<SimpleYamlStruct, int>,
                    Field<SimpleYamlStruct, std::string>,
                    Field<SimpleYamlStruct, double>>(
            make_field("id", &SimpleYamlStruct::id),
            make_field("name", &SimpleYamlStruct::name),
            make_field("value", &SimpleYamlStruct::value));

    YAML::Node node;
    node["id"] = 1;
    node["name"] = "test with: colons and # hashes";
    node["value"] = 0.0;

    auto obj = reflectable.from_yaml(node);
    auto restored = reflectable.to_yaml(obj);

    EXPECT_EQ(restored["name"].as<std::string>(),
              node["name"].as<std::string>());
}

TEST_F(ReflYamlTest, MultilineStrings) {
    auto reflectable =
        Reflectable<SimpleYamlStruct, Field<SimpleYamlStruct, int>,
                    Field<SimpleYamlStruct, std::string>,
                    Field<SimpleYamlStruct, double>>(
            make_field("id", &SimpleYamlStruct::id),
            make_field("name", &SimpleYamlStruct::name),
            make_field("value", &SimpleYamlStruct::value));

    std::string yamlStr = R"(
id: 1
name: |
  This is a
  multiline string
value: 0.0
)";

    YAML::Node node = YAML::Load(yamlStr);
    auto obj = reflectable.from_yaml(node);

    EXPECT_NE(obj.name.find("multiline"), std::string::npos);
}

TEST_F(ReflYamlTest, UnicodeStrings) {
    auto reflectable =
        Reflectable<SimpleYamlStruct, Field<SimpleYamlStruct, int>,
                    Field<SimpleYamlStruct, std::string>,
                    Field<SimpleYamlStruct, double>>(
            make_field("id", &SimpleYamlStruct::id),
            make_field("name", &SimpleYamlStruct::name),
            make_field("value", &SimpleYamlStruct::value));

    YAML::Node node;
    node["id"] = 1;
    // Plain narrow literal: u8"" yields char8_t in C++20+, which yaml-cpp
    // cannot encode; the execution charset is UTF-8 anyway.
    node["name"] = "\u4e2d\u6587\u6d4b\u8bd5";
    node["value"] = 0.0;

    auto obj = reflectable.from_yaml(node);
    auto restored = reflectable.to_yaml(obj);

    EXPECT_EQ(restored["name"].as<std::string>(),
              node["name"].as<std::string>());
}

TEST_F(ReflYamlTest, BooleanFields) {
    struct WithBool {
        bool flag;
        std::string name;
    };

    auto reflectable = Reflectable<WithBool, Field<WithBool, bool>,
                                   Field<WithBool, std::string>>(
        make_field("flag", &WithBool::flag),
        make_field("name", &WithBool::name));

    // Test with 'true'
    YAML::Node node1;
    node1["flag"] = true;
    node1["name"] = "test";

    auto obj1 = reflectable.from_yaml(node1);
    EXPECT_TRUE(obj1.flag);

    // Test with 'false'
    YAML::Node node2;
    node2["flag"] = false;
    node2["name"] = "test";

    auto obj2 = reflectable.from_yaml(node2);
    EXPECT_FALSE(obj2.flag);
}

}  // namespace atom::meta::test

#else
// Stub test when yaml-cpp is not available
namespace atom::meta::test {
TEST(ReflYamlTest, YamlCppNotAvailable) {
    GTEST_SKIP() << "yaml-cpp is not available, skipping YAML reflection tests";
}
}  // namespace atom::meta::test
#endif

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
