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
    EXPECT_THROW(reflectable.from_json(j1), atom::error::InvalidArgument);

    // Invalid: empty string
    json j2 = {{"positive_number", 5}, {"non_empty_string", ""}};
    EXPECT_THROW(reflectable.from_json(j2), atom::error::InvalidArgument);
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

    // Note: an ordinary (char-based) literal is required here. A u8"" literal
    // is char8_t-based in C++20/23, which nlohmann::json does not treat as a
    // string (it serializes as an array), breaking get<std::string>().
    json j = {{"id", 1},
              {"name", "\u4e2d\u6587\u6d4b\u8bd5 \U0001F600"},
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

//==============================================================================
// Serializer / Deserializer Transformer Tests (lines 75, 116)
//==============================================================================

TEST_F(ReflJsonTest, FieldWithDeserializer) {
    // Line 75: field.deserializer applied after reading from JSON
    struct WithTransform {
        int raw_value;
        std::string tag;
    };

    auto f_val = make_field<WithTransform, int>(
        "raw_value", &WithTransform::raw_value, true, 0, nullptr,
        nullptr,
        [](const int& v) { return v * 2; });  // deserializer doubles the value
    auto f_tag = make_field("tag", &WithTransform::tag);

    auto reflectable = Reflectable<WithTransform,
                                   Field<WithTransform, int>,
                                   Field<WithTransform, std::string>>(f_val, f_tag);

    json j = {{"raw_value", 5}, {"tag", "hello"}};
    auto obj = reflectable.from_json(j);

    EXPECT_EQ(obj.raw_value, 10);   // 5 * 2
    EXPECT_EQ(obj.tag, "hello");
}

TEST_F(ReflJsonTest, FieldWithSerializer) {
    // Line 116: field.serializer applied before writing to JSON
    struct WithTransform {
        int raw_value;
        std::string tag;
    };

    auto f_val = make_field<WithTransform, int>(
        "raw_value", &WithTransform::raw_value, true, 0, nullptr,
        [](const int& v) { return v + 100; },  // serializer adds 100
        nullptr);
    auto f_tag = make_field("tag", &WithTransform::tag);

    auto reflectable = Reflectable<WithTransform,
                                   Field<WithTransform, int>,
                                   Field<WithTransform, std::string>>(f_val, f_tag);

    WithTransform obj{7, "world"};
    json j = reflectable.to_json(obj);

    EXPECT_EQ(j["raw_value"].get<int>(), 107);  // 7 + 100
    EXPECT_EQ(j["tag"].get<std::string>(), "world");
}

TEST_F(ReflJsonTest, FieldWithBothTransformers) {
    // Both serializer and deserializer on the same field
    struct Encoded {
        int value;
    };

    auto f = make_field<Encoded, int>(
        "value", &Encoded::value, true, 0, nullptr,
        [](const int& v) { return v * 3; },   // serializer
        [](const int& v) { return v - 1; });  // deserializer

    auto reflectable = Reflectable<Encoded, Field<Encoded, int>>(f);

    Encoded src{10};
    json j = reflectable.to_json(src);
    EXPECT_EQ(j["value"].get<int>(), 30);  // 10 * 3

    json j2 = {{"value", 9}};
    auto restored = reflectable.from_json(j2);
    EXPECT_EQ(restored.value, 8);  // 9 - 1
}

//==============================================================================
// Deprecated Field Tests (lines 65, 104, 108)
//==============================================================================

TEST_F(ReflJsonTest, DeprecatedFieldSkippedInToJson) {
    // Lines 104, 108: deprecated field is skipped in to_json by default
    struct WithDeprecated {
        std::string name;
        int legacy_id;
    };

    auto f_name = make_field("name", &WithDeprecated::name);
    auto f_legacy = make_field<WithDeprecated, int>(
        "legacy_id", &WithDeprecated::legacy_id, false, -1)
        .withDeprecated(true);

    auto reflectable = Reflectable<WithDeprecated,
                                   Field<WithDeprecated, std::string>,
                                   Field<WithDeprecated, int>>(f_name, f_legacy);

    WithDeprecated obj{"test_name", 99};

    // Default: include_deprecated=false — deprecated field must be absent
    json j = reflectable.to_json(obj);
    EXPECT_TRUE(j.contains("name"));
    EXPECT_FALSE(j.contains("legacy_id"));

    // include_deprecated=true — deprecated field must appear
    json j2 = reflectable.to_json(obj, true);
    EXPECT_TRUE(j2.contains("name"));
    EXPECT_TRUE(j2.contains("legacy_id"));
    EXPECT_EQ(j2["legacy_id"].get<int>(), 99);
}

TEST_F(ReflJsonTest, DeprecatedFieldSkippedInFromJsonByVersion) {
    // Line 65: deprecated field with version > target_version is skipped
    struct Versioned {
        std::string name;
        int new_field;  // version 2, deprecated
    };

    auto f_name = make_field("name", &Versioned::name);
    // version=2, deprecated=true: should be skipped when target_version=1
    auto f_new = make_field<Versioned, int>(
        "new_field", &Versioned::new_field, false, 42)
        .withVersion(2)
        .withDeprecated(true);

    auto reflectable = Reflectable<Versioned,
                                   Field<Versioned, std::string>,
                                   Field<Versioned, int>>(f_name, f_new);

    // Provide the field in JSON but ask for version 1 — field must be skipped
    json j = {{"name", "hello"}, {"new_field", 999}};
    auto obj = reflectable.from_json(j, 1);  // target_version=1

    EXPECT_EQ(obj.name, "hello");
    // new_field skipped: value stays default-initialized (0, not 999 or 42)
    EXPECT_NE(obj.new_field, 999);

    // At version 2 the field is processed normally
    auto obj2 = reflectable.from_json(j, 2);
    EXPECT_EQ(obj2.new_field, 999);
}

//==============================================================================
// to_json with Metadata (lines 122-132, 138)
//==============================================================================

TEST_F(ReflJsonTest, ToJsonWithMetadata) {
    // Lines 122-132, 138: include_metadata=true path
    struct Meta {
        int count;
        std::string label;
    };

    auto f_count = make_field("count", &Meta::count)
        .withDescription("item count");
    auto f_label = make_field<Meta, std::string>(
        "label", &Meta::label, false, std::string(""))
        .withDeprecated(false);

    auto reflectable = Reflectable<Meta,
                                   Field<Meta, int>,
                                   Field<Meta, std::string>>(f_count, f_label);

    Meta obj{5, "foo"};
    json j = reflectable.to_json(obj, false, true);  // include_metadata=true

    // Regular fields still present
    EXPECT_EQ(j["count"].get<int>(), 5);
    EXPECT_EQ(j["label"].get<std::string>(), "foo");

    // Metadata section populated
    ASSERT_TRUE(j.contains("__metadata__"));
    ASSERT_TRUE(j["__metadata__"].contains("count"));
    EXPECT_EQ(j["__metadata__"]["count"]["description"].get<std::string>(), "item count");
    EXPECT_EQ(j["__metadata__"]["count"]["required"].get<bool>(), true);
    EXPECT_EQ(j["__metadata__"]["count"]["deprecated"].get<bool>(), false);
    EXPECT_EQ(j["__metadata__"]["count"]["version"].get<int>(), 1);

    ASSERT_TRUE(j["__metadata__"].contains("label"));
    EXPECT_EQ(j["__metadata__"]["label"]["required"].get<bool>(), false);
}

TEST_F(ReflJsonTest, ToJsonWithMetadataNoDescription) {
    // Line 125: field.description branch NOT taken when description is nullptr
    struct Simple {
        int x;
    };

    auto f = make_field("x", &Simple::x);  // no description set

    auto reflectable = Reflectable<Simple, Field<Simple, int>>(f);

    Simple obj{42};
    json j = reflectable.to_json(obj, false, true);

    EXPECT_EQ(j["x"].get<int>(), 42);
    ASSERT_TRUE(j.contains("__metadata__"));
    // No "description" key because field.description is nullptr
    EXPECT_FALSE(j["__metadata__"]["x"].contains("description"));
    EXPECT_EQ(j["__metadata__"]["x"]["required"].get<bool>(), true);
}

//==============================================================================
// validate() method tests
//==============================================================================

TEST_F(ReflJsonTest, ValidateMethodNoErrors) {
    auto reflectable =
        Reflectable<StructWithValidation, Field<StructWithValidation, int>,
                    Field<StructWithValidation, std::string>>(
            make_field("positive_number",
                       &StructWithValidation::positive_number, true, 0,
                       [](const int& v) { return v > 0; }),
            make_field("non_empty_string",
                       &StructWithValidation::non_empty_string, true,
                       std::string{},
                       [](const std::string& s) { return !s.empty(); }));

    StructWithValidation obj{10, "ok"};
    auto errors = reflectable.validate(obj);
    EXPECT_TRUE(errors.empty());
}

TEST_F(ReflJsonTest, ValidateMethodWithErrors) {
    auto reflectable =
        Reflectable<StructWithValidation, Field<StructWithValidation, int>,
                    Field<StructWithValidation, std::string>>(
            make_field("positive_number",
                       &StructWithValidation::positive_number, true, 0,
                       [](const int& v) { return v > 0; }),
            make_field("non_empty_string",
                       &StructWithValidation::non_empty_string, true,
                       std::string{},
                       [](const std::string& s) { return !s.empty(); }));

    StructWithValidation obj{-1, ""};
    auto errors = reflectable.validate(obj);
    EXPECT_EQ(errors.size(), 2u);
}

TEST_F(ReflJsonTest, ValidateMethodWithDescription) {
    // Line 149: description branch in validate
    struct Described {
        int value;
    };

    auto f = make_field("value", &Described::value, true, 0,
                        [](const int& v) { return v >= 0; })
        .withDescription("must be non-negative");

    auto reflectable = Reflectable<Described, Field<Described, int>>(f);

    Described bad{-5};
    auto errors = reflectable.validate(bad);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_NE(errors[0].find("must be non-negative"), std::string::npos);
}

TEST_F(ReflJsonTest, ValidateMethodNoValidator) {
    // validate() with no validator set — no errors
    auto reflectable = Reflectable<SimpleStruct,
                                   Field<SimpleStruct, int>,
                                   Field<SimpleStruct, std::string>,
                                   Field<SimpleStruct, double>>(
        make_field("id", &SimpleStruct::id),
        make_field("name", &SimpleStruct::name),
        make_field("value", &SimpleStruct::value));

    SimpleStruct obj{1, "x", 1.0};
    auto errors = reflectable.validate(obj);
    EXPECT_TRUE(errors.empty());
}

//==============================================================================
// get_schema() method tests
//==============================================================================

TEST_F(ReflJsonTest, GetSchemaBasic) {
    auto reflectable = Reflectable<SimpleStruct,
                                   Field<SimpleStruct, int>,
                                   Field<SimpleStruct, std::string>,
                                   Field<SimpleStruct, double>>(
        make_field("id", &SimpleStruct::id),
        make_field("name", &SimpleStruct::name),
        make_field("value", &SimpleStruct::value, false, 0.0));

    json schema = reflectable.get_schema();

    EXPECT_EQ(schema["type"].get<std::string>(), "object");
    ASSERT_TRUE(schema.contains("properties"));
    ASSERT_TRUE(schema["properties"].contains("id"));
    ASSERT_TRUE(schema["properties"].contains("name"));
    ASSERT_TRUE(schema["properties"].contains("value"));

    // Required array should contain "id" and "name" (required=true) but not "value"
    auto& req = schema["required"];
    bool has_id = false, has_name = false, has_value = false;
    for (auto& r : req) {
        if (r.get<std::string>() == "id") has_id = true;
        if (r.get<std::string>() == "name") has_name = true;
        if (r.get<std::string>() == "value") has_value = true;
    }
    EXPECT_TRUE(has_id);
    EXPECT_TRUE(has_name);
    EXPECT_FALSE(has_value);
}

TEST_F(ReflJsonTest, GetSchemaWithDescription) {
    // Line 172: description present in schema
    struct Desc {
        int score;
    };

    auto f = make_field("score", &Desc::score)
        .withDescription("player score");

    auto reflectable = Reflectable<Desc, Field<Desc, int>>(f);
    json schema = reflectable.get_schema();

    ASSERT_TRUE(schema["properties"].contains("score"));
    EXPECT_EQ(schema["properties"]["score"]["description"].get<std::string>(), "player score");
    EXPECT_EQ(schema["properties"]["score"]["deprecated"].get<bool>(), false);
    EXPECT_EQ(schema["properties"]["score"]["version"].get<int>(), 1);
}

TEST_F(ReflJsonTest, GetSchemaWithCustomJsonKey) {
    // get_schema uses getJsonKey() — verify custom key appears in properties
    struct Keyed {
        int internal_id;
    };

    auto f = make_field("internal_id", &Keyed::internal_id)
        .withJsonKey("id");

    auto reflectable = Reflectable<Keyed, Field<Keyed, int>>(f);
    json schema = reflectable.get_schema();

    EXPECT_TRUE(schema["properties"].contains("id"));
    EXPECT_FALSE(schema["properties"].contains("internal_id"));
    auto& req = schema["required"];
    bool found = false;
    for (auto& r : req) {
        if (r.get<std::string>() == "id") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

//==============================================================================
// withJsonKey + custom key round-trip
//==============================================================================

TEST_F(ReflJsonTest, CustomJsonKeyRoundTrip) {
    struct Aliased {
        int user_id;
        std::string display_name;
    };

    auto f_id = make_field("user_id", &Aliased::user_id).withJsonKey("userId");
    auto f_name = make_field("display_name", &Aliased::display_name)
                      .withJsonKey("displayName");

    auto reflectable = Reflectable<Aliased,
                                   Field<Aliased, int>,
                                   Field<Aliased, std::string>>(f_id, f_name);

    Aliased src{42, "Alice"};
    json j = reflectable.to_json(src);

    EXPECT_FALSE(j.contains("user_id"));
    EXPECT_TRUE(j.contains("userId"));
    EXPECT_EQ(j["userId"].get<int>(), 42);
    EXPECT_EQ(j["displayName"].get<std::string>(), "Alice");

    auto restored = reflectable.from_json(j);
    EXPECT_EQ(restored.user_id, 42);
    EXPECT_EQ(restored.display_name, "Alice");
}

//==============================================================================
// Shorthand field factory functions
//==============================================================================

TEST_F(ReflJsonTest, FieldShorthand) {
    // field(), required_field(), optional_field(), deprecated_field()
    struct S {
        int a;
        int b;
        int c;
        int d;
    };

    auto fa = field("a", &S::a);
    EXPECT_STREQ(fa.name, "a");
    EXPECT_TRUE(fa.required);

    auto fb = required_field("b", &S::b);
    EXPECT_STREQ(fb.name, "b");
    EXPECT_TRUE(fb.required);

    auto fc = optional_field("c", &S::c, 77);
    EXPECT_STREQ(fc.name, "c");
    EXPECT_FALSE(fc.required);
    EXPECT_EQ(fc.default_value, 77);

    auto fd = deprecated_field("d", &S::d, 0);
    EXPECT_STREQ(fd.name, "d");
    EXPECT_FALSE(fd.required);
    EXPECT_TRUE(fd.deprecated);
}

//==============================================================================
// Validation error message includes field description (from_json path)
//==============================================================================

TEST_F(ReflJsonTest, ValidationErrorMessageWithDescription) {
    // Line 84: description != nullptr branch in from_json validation error
    struct S {
        int score;
    };

    auto f = make_field("score", &S::score, true, 0,
                        [](const int& v) { return v >= 0; })
        .withDescription("score must be non-negative");

    auto reflectable = Reflectable<S, Field<S, int>>(f);

    json j = {{"score", -1}};
    try {
        reflectable.from_json(j);
        FAIL() << "Expected exception";
    } catch (const std::exception& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("score must be non-negative"), std::string::npos);
    }
}

}  // namespace atom::meta::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
