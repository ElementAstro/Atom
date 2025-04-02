#include <gmock/gmock.h>
#include <gtest/gtest.h>


#include "atom/type/json-schema.hpp"

using namespace atom::type;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Pair;

class JsonValidatorTest : public ::testing::Test {
protected:
    JsonValidator validator;
};

// Basic Type Validation Tests
TEST_F(JsonValidatorTest, BasicTypeValidation) {
    json schema = {{"type", "string"}};
    validator.setRootSchema(schema);

    EXPECT_TRUE(validator.validate(json("test")));
    EXPECT_FALSE(validator.validate(json(42)));

    const auto& errors = validator.getErrors();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("Type mismatch"));
    EXPECT_THAT(errors[0].message, HasSubstr("string"));
}

TEST_F(JsonValidatorTest, MultipleTypesValidation) {
    json schema = {{"type", json::array({"string", "number"})}};
    validator.setRootSchema(schema);

    EXPECT_TRUE(validator.validate(json("test")));
    EXPECT_TRUE(validator.validate(json(42)));
    EXPECT_FALSE(validator.validate(json(true)));

    const auto& errors = validator.getErrors();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("Type mismatch"));
    EXPECT_THAT(errors[0].message, HasSubstr("[string, number]"));
}

// String Validation Tests
TEST_F(JsonValidatorTest, StringValidation) {
    json schema = {{"type", "string"},
                   {"minLength", 3},
                   {"maxLength", 10},
                   {"pattern", "^[a-z]+$"}};
    validator.setRootSchema(schema);

    EXPECT_TRUE(validator.validate(json("test")));

    // Too short
    EXPECT_FALSE(validator.validate(json("ab")));

    // Too long
    EXPECT_FALSE(validator.validate(json("abcdefghijk")));

    // Doesn't match pattern
    EXPECT_FALSE(validator.validate(json("Test123")));

    const auto& errors = validator.getErrors();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("pattern"));
}

// Number Validation Tests
TEST_F(JsonValidatorTest, NumberValidation) {
    json schema = {{"type", "number"}, {"minimum", 0}, {"maximum", 100}};
    validator.setRootSchema(schema);

    EXPECT_TRUE(validator.validate(json(42)));
    EXPECT_TRUE(validator.validate(json(0)));
    EXPECT_TRUE(validator.validate(json(100)));

    // Below minimum
    EXPECT_FALSE(validator.validate(json(-1)));

    // Above maximum
    EXPECT_FALSE(validator.validate(json(101)));

    const auto& errors = validator.getErrors();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("greater than maximum"));
}

// Integer Validation
TEST_F(JsonValidatorTest, IntegerValidation) {
    json schema = {{"type", "integer"}};
    validator.setRootSchema(schema);

    EXPECT_TRUE(validator.validate(json(42)));
    EXPECT_FALSE(validator.validate(json(42.5)));

    const auto& errors = validator.getErrors();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("Type mismatch"));
}

// Enum Validation
TEST_F(JsonValidatorTest, EnumValidation) {
    json schema = {{"enum", json::array({"red", "green", "blue"})}};
    validator.setRootSchema(schema);

    EXPECT_TRUE(validator.validate(json("red")));
    EXPECT_TRUE(validator.validate(json("green")));
    EXPECT_TRUE(validator.validate(json("blue")));
    EXPECT_FALSE(validator.validate(json("yellow")));

    const auto& errors = validator.getErrors();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("enum range"));
}

// Const Validation
TEST_F(JsonValidatorTest, ConstValidation) {
    json schema = {{"const", "fixed-value"}};
    validator.setRootSchema(schema);

    EXPECT_TRUE(validator.validate(json("fixed-value")));
    EXPECT_FALSE(validator.validate(json("other-value")));

    const auto& errors = validator.getErrors();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("const value"));
}

// Object Validation Tests
TEST_F(JsonValidatorTest, ObjectValidation) {
    json schema = {{"type", "object"},
                   {"required", json::array({"name", "age"})},
                   {"properties",
                    {{"name", {{"type", "string"}}},
                     {"age", {{"type", "integer"}}},
                     {"email", {{"type", "string"}}}}}};
    validator.setRootSchema(schema);

    json valid_instance = {{"name", "John"}, {"age", 30}};
    EXPECT_TRUE(validator.validate(valid_instance));

    json missing_required = {{"name", "John"}};
    EXPECT_FALSE(validator.validate(missing_required));

    json wrong_type = {{"name", "John"}, {"age", "thirty"}};
    EXPECT_FALSE(validator.validate(wrong_type));

    const auto& errors = validator.getErrors();
    ASSERT_GE(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("Type mismatch"));
    EXPECT_THAT(errors[0].path, HasSubstr("age"));
}

// Array Validation Tests
TEST_F(JsonValidatorTest, ArrayValidation) {
    json schema = {{"type", "array"},
                   {"items", {{"type", "string"}}},
                   {"minItems", 1},
                   {"maxItems", 3},
                   {"uniqueItems", true}};
    validator.setRootSchema(schema);

    json valid_instance = json::array({"a", "b", "c"});
    EXPECT_TRUE(validator.validate(valid_instance));

    // Too many items
    json too_many = json::array({"a", "b", "c", "d"});
    EXPECT_FALSE(validator.validate(too_many));

    // Empty array
    json too_few = json::array();
    EXPECT_FALSE(validator.validate(too_few));

    // Non-unique items
    json non_unique = json::array({"a", "a", "b"});
    EXPECT_FALSE(validator.validate(non_unique));

    // Wrong item type
    json wrong_type = json::array({"a", 1, "c"});
    EXPECT_FALSE(validator.validate(wrong_type));

    const auto& errors = validator.getErrors();
    ASSERT_GE(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("Type mismatch"));
    EXPECT_THAT(errors[0].path, HasSubstr("[1]"));
}

// Dependencies Validation
TEST_F(JsonValidatorTest, DependenciesValidation) {
    json schema = {
        {"type", "object"},
        {"dependencies", {{"credit_card", json::array({"billing_address"})}}}};
    validator.setRootSchema(schema);

    json valid_instance = {{"name", "John"},
                           {"credit_card", "1234-5678-9012-3456"},
                           {"billing_address", "123 Main St"}};
    EXPECT_TRUE(validator.validate(valid_instance));

    json missing_dependency = {{"name", "John"},
                               {"credit_card", "1234-5678-9012-3456"}};
    EXPECT_FALSE(validator.validate(missing_dependency));

    const auto& errors = validator.getErrors();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("Missing dependency"));
    EXPECT_THAT(errors[0].message, HasSubstr("billing_address"));
}

// Schema Composition Tests
TEST_F(JsonValidatorTest, AllOfValidation) {
    json schema = {
        {"allOf", json::array({{{"type", "object"}},
                               {{"required", json::array({"name"})}}})}};
    validator.setRootSchema(schema);

    json valid_instance = {{"name", "John"}};
    EXPECT_TRUE(validator.validate(valid_instance));

    json invalid_instance = {{"age", 30}};
    EXPECT_FALSE(validator.validate(invalid_instance));

    const auto& errors = validator.getErrors();
    ASSERT_GE(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("Missing required field"));
    EXPECT_THAT(errors[0].message, HasSubstr("name"));
}

TEST_F(JsonValidatorTest, AnyOfValidation) {
    json schema = {
        {"anyOf", json::array({{{"type", "string"}}, {{"type", "integer"}}})}};
    validator.setRootSchema(schema);

    EXPECT_TRUE(validator.validate(json("test")));
    EXPECT_TRUE(validator.validate(json(42)));
    EXPECT_FALSE(validator.validate(json(true)));

    const auto& errors = validator.getErrors();
    ASSERT_GE(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("anyOf"));
}

TEST_F(JsonValidatorTest, OneOfValidation) {
    json schema = {
        {"oneOf", json::array({{{"type", "string"}}, {{"type", "number"}}})}};
    validator.setRootSchema(schema);

    EXPECT_TRUE(validator.validate(json("test")));
    EXPECT_TRUE(validator.validate(json(42)));

    // This should match both schemas
    json schema_double_match = {
        {"oneOf", json::array({{{"type", "number"}}, {{"type", "integer"}}})}};
    validator.setRootSchema(schema_double_match);
    EXPECT_FALSE(validator.validate(json(42)));

    const auto& errors = validator.getErrors();
    ASSERT_GE(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("exactly one"));
    EXPECT_THAT(errors[0].message, HasSubstr("oneOf"));
}

TEST_F(JsonValidatorTest, NotValidation) {
    json schema = {{"not", {{"type", "integer"}}}};
    validator.setRootSchema(schema);

    EXPECT_TRUE(validator.validate(json("test")));
    EXPECT_FALSE(validator.validate(json(42)));

    const auto& errors = validator.getErrors();
    ASSERT_GE(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("matches schema in not"));
}

// Complex Schema Tests
TEST_F(JsonValidatorTest, ComplexPersonSchema) {
    json schema = {
        {"type", "object"},
        {"required", json::array({"name", "age"})},
        {"properties",
         {{"name", {{"type", "string"}, {"minLength", 2}}},
          {"age", {{"type", "integer"}, {"minimum", 18}}},
          {"email",
           {{"type", "string"},
            {"pattern", "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"}}},
          {"address",
           {{"type", "object"},
            {"properties",
             {{"street", {{"type", "string"}}},
              {"city", {{"type", "string"}}},
              {"zipCode", {{"type", "string"}}}}},
            {"required", json::array({"street", "city"})}}},
          {"phoneNumbers",
           {{"type", "array"},
            {"items",
             {{"type", "object"},
              {"properties",
               {{"type", {{"enum", json::array({"home", "work", "mobile"})}}},
                {"number", {{"type", "string"}}}}},
              {"required", json::array({"type", "number"})}}}}}}}};
    validator.setRootSchema(schema);

    json valid_instance = {
        {"name", "John Doe"},
        {"age", 30},
        {"email", "john.doe@example.com"},
        {"address", {{"street", "123 Main St"}, {"city", "Anytown"}}},
        {"phoneNumbers",
         json::array({{{"type", "home"}, {"number", "555-1234"}},
                      {{"type", "mobile"}, {"number", "555-5678"}}})}};
    EXPECT_TRUE(validator.validate(valid_instance));

    // Missing required field
    json missing_required = valid_instance;
    missing_required.erase("age");
    EXPECT_FALSE(validator.validate(missing_required));

    // Invalid age
    json invalid_age = valid_instance;
    invalid_age["age"] = 15;
    EXPECT_FALSE(validator.validate(invalid_age));

    // Invalid email
    json invalid_email = valid_instance;
    invalid_email["email"] = "not-an-email";
    EXPECT_FALSE(validator.validate(invalid_email));

    // Missing required in nested object
    json invalid_address = valid_instance;
    invalid_address["address"].erase("city");
    EXPECT_FALSE(validator.validate(invalid_address));

    // Invalid phone number type
    json invalid_phone = valid_instance;
    invalid_phone["phoneNumbers"][0]["type"] = "unknown";
    EXPECT_FALSE(validator.validate(invalid_phone));

    const auto& errors = validator.getErrors();
    ASSERT_GE(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("enum range"));
    EXPECT_THAT(errors[0].path, HasSubstr("phoneNumbers[0].type"));
}

// Reset/Clear Tests
TEST_F(JsonValidatorTest, ErrorsResetBetweenValidations) {
    json schema = {{"type", "string"}};
    validator.setRootSchema(schema);

    EXPECT_FALSE(validator.validate(json(42)));
    ASSERT_GE(validator.getErrors().size(), 1);

    EXPECT_TRUE(validator.validate(json("test")));
    EXPECT_TRUE(validator.getErrors().empty());
}

TEST_F(JsonValidatorTest, ErrorsClearedOnSchemaSet) {
    json schema = {{"type", "string"}};
    validator.setRootSchema(schema);

    EXPECT_FALSE(validator.validate(json(42)));
    ASSERT_GE(validator.getErrors().size(), 1);

    validator.setRootSchema(schema);
    EXPECT_TRUE(validator.getErrors().empty());
}

// Edge Cases
TEST_F(JsonValidatorTest, EmptySchema) {
    json schema = json::object();
    validator.setRootSchema(schema);

    // An empty schema should validate anything
    EXPECT_TRUE(validator.validate(json(42)));
    EXPECT_TRUE(validator.validate(json("test")));
    EXPECT_TRUE(validator.validate(json::object()));
    EXPECT_TRUE(validator.validate(json::array()));
    EXPECT_TRUE(validator.validate(json(true)));
    EXPECT_TRUE(validator.validate(json(nullptr)));
}

TEST_F(JsonValidatorTest, NullValidation) {
    json schema = {{"type", "null"}};
    validator.setRootSchema(schema);

    EXPECT_TRUE(validator.validate(json(nullptr)));
    EXPECT_FALSE(validator.validate(json(42)));
    EXPECT_FALSE(validator.validate(json("test")));

    const auto& errors = validator.getErrors();
    ASSERT_GE(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("Type mismatch"));
    EXPECT_THAT(errors[0].message, HasSubstr("null"));
}

TEST_F(JsonValidatorTest, BooleanValidation) {
    json schema = {{"type", "boolean"}};
    validator.setRootSchema(schema);

    EXPECT_TRUE(validator.validate(json(true)));
    EXPECT_TRUE(validator.validate(json(false)));
    EXPECT_FALSE(validator.validate(json(42)));
    EXPECT_FALSE(validator.validate(json("true")));

    const auto& errors = validator.getErrors();
    ASSERT_GE(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("Type mismatch"));
    EXPECT_THAT(errors[0].message, HasSubstr("boolean"));
}

// Error Path Tests
TEST_F(JsonValidatorTest, ErrorPathReporting) {
    json schema = {
        {"type", "object"},
        {"properties",
         {{"user",
           {{"type", "object"},
            {"properties",
             {{"name", {{"type", "string"}}},
              {"scores",
               {{"type", "array"}, {"items", {{"type", "integer"}}}}}}}}}}}};
    validator.setRootSchema(schema);

    json invalid_instance = {
        {"user",
         {
             {"name", 123},                          // Should be string
             {"scores", json::array({1, "two", 3})}  // Should all be integers
         }}};
    EXPECT_FALSE(validator.validate(invalid_instance));

    const auto& errors = validator.getErrors();
    ASSERT_GE(errors.size(), 2);

    bool found_name_error = false;
    bool found_scores_error = false;

    for (const auto& error : errors) {
        if (error.path == "user.name") {
            found_name_error = true;
            EXPECT_THAT(error.message, HasSubstr("Type mismatch"));
            EXPECT_THAT(error.message, HasSubstr("string"));
        } else if (error.path == "user.scores[1]") {
            found_scores_error = true;
            EXPECT_THAT(error.message, HasSubstr("Type mismatch"));
            EXPECT_THAT(error.message, HasSubstr("integer"));
        }
    }

    EXPECT_TRUE(found_name_error);
    EXPECT_TRUE(found_scores_error);
}

// Schema Dependency Test
TEST_F(JsonValidatorTest, SchemaDependency) {
    json schema = {{"type", "object"},
                   {"dependencies",
                    {{"credit_card",
                      {{"properties",
                        {{"billing_address", {{"type", "string"}}},
                         {"security_code", {{"type", "string"}}}}},
                       {"required",
                        json::array({"billing_address", "security_code"})}}}}}};
    validator.setRootSchema(schema);

    json valid_instance = {{"name", "John"},
                           {"credit_card", "1234-5678-9012-3456"},
                           {"billing_address", "123 Main St"},
                           {"security_code", "123"}};
    EXPECT_TRUE(validator.validate(valid_instance));

    json missing_schema_deps = {
        {"name", "John"},
        {"credit_card", "1234-5678-9012-3456"},
        {"billing_address", "123 Main St"}  // Missing security_code
    };
    EXPECT_FALSE(validator.validate(missing_schema_deps));

    json instance_without_trigger = {
        {"name", "John"}  // No credit_card, so dependencies not triggered
    };
    EXPECT_TRUE(validator.validate(instance_without_trigger));

    const auto& errors = validator.getErrors();
    ASSERT_GE(errors.size(), 1);
    EXPECT_THAT(errors[0].message, HasSubstr("Missing required field"));
    EXPECT_THAT(errors[0].message, HasSubstr("security_code"));
}