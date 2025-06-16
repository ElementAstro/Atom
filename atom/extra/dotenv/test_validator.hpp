// atom/extra/dotenv/test_validator.hpp

#include <gtest/gtest.h>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "validator.hpp"

using namespace dotenv;

class ValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ValidatorTest, ValidationRuleBasic) {
    auto rule = std::make_shared<ValidationRule>(
        "test", [](const std::string& v) { return v == "ok"; }, "Must be ok");
    EXPECT_EQ(rule->getName(), "test");
    EXPECT_EQ(rule->getErrorMessage(), "Must be ok");
    EXPECT_TRUE(rule->validate("ok"));
    EXPECT_FALSE(rule->validate("fail"));
}

TEST_F(ValidatorTest, ValidationSchemaRequiredOptional) {
    ValidationSchema schema;
    schema.required("A").optional("B", "defaultB");
    EXPECT_TRUE(schema.isRequired("A"));
    EXPECT_FALSE(schema.isRequired("B"));
    EXPECT_EQ(schema.getDefault("B"), "defaultB");
    EXPECT_EQ(schema.getDefault("A"), "");
    auto reqs = schema.getRequiredVariables();
    ASSERT_EQ(reqs.size(), 1);
    EXPECT_EQ(reqs[0], "A");
}

TEST_F(ValidatorTest, ValidationSchemaRules) {
    ValidationSchema schema;
    auto rule1 = rules::notEmpty();
    auto rule2 = rules::minLength(3);
    schema.rule("A", rule1).rules("B", {rule1, rule2});
    auto rA = schema.getRules("A");
    auto rB = schema.getRules("B");
    ASSERT_EQ(rA.size(), 1);
    ASSERT_EQ(rB.size(), 2);
    EXPECT_EQ(rA[0]->getName(), "notEmpty");
    EXPECT_EQ(rB[1]->getName(), "minLength");
}

TEST_F(ValidatorTest, BuiltinRuleNotEmpty) {
    auto rule = rules::notEmpty();
    EXPECT_TRUE(rule->validate("abc"));
    EXPECT_FALSE(rule->validate(""));
}

TEST_F(ValidatorTest, BuiltinRuleMinLength) {
    auto rule = rules::minLength(2);
    EXPECT_TRUE(rule->validate("ab"));
    EXPECT_FALSE(rule->validate("a"));
}

TEST_F(ValidatorTest, BuiltinRuleMaxLength) {
    auto rule = rules::maxLength(3);
    EXPECT_TRUE(rule->validate("abc"));
    EXPECT_FALSE(rule->validate("abcd"));
}

TEST_F(ValidatorTest, BuiltinRulePattern) {
    auto rule = rules::pattern(std::regex("^\\d+$"));
    EXPECT_TRUE(rule->validate("12345"));
    EXPECT_FALSE(rule->validate("12a45"));
}

TEST_F(ValidatorTest, BuiltinRuleNumeric) {
    auto rule = rules::numeric();
    EXPECT_TRUE(rule->validate("123.45"));
    EXPECT_TRUE(rule->validate("-0.1"));
    EXPECT_FALSE(rule->validate("abc"));
}

TEST_F(ValidatorTest, BuiltinRuleInteger) {
    auto rule = rules::integer();
    EXPECT_TRUE(rule->validate("123"));
    EXPECT_TRUE(rule->validate("-42"));
    EXPECT_FALSE(rule->validate("1.5"));
    EXPECT_FALSE(rule->validate("abc"));
}

TEST_F(ValidatorTest, BuiltinRuleBoolean) {
    auto rule = rules::boolean();
    EXPECT_TRUE(rule->validate("true"));
    EXPECT_TRUE(rule->validate("FALSE"));
    EXPECT_TRUE(rule->validate("1"));
    EXPECT_TRUE(rule->validate("no"));
    EXPECT_FALSE(rule->validate("maybe"));
}

TEST_F(ValidatorTest, BuiltinRuleUrl) {
    auto rule = rules::url();
    EXPECT_TRUE(rule->validate("http://example.com"));
    EXPECT_TRUE(rule->validate("https://example.com/path"));
    EXPECT_FALSE(rule->validate("ftp://example.com"));
    EXPECT_FALSE(rule->validate("not a url"));
}

TEST_F(ValidatorTest, BuiltinRuleEmail) {
    auto rule = rules::email();
    EXPECT_TRUE(rule->validate("user@example.com"));
    EXPECT_FALSE(rule->validate("user@com"));
    EXPECT_FALSE(rule->validate("not-an-email"));
}

TEST_F(ValidatorTest, BuiltinRuleOneOf) {
    auto rule = rules::oneOf({"a", "b", "c"});
    EXPECT_TRUE(rule->validate("a"));
    EXPECT_FALSE(rule->validate("d"));
}

TEST_F(ValidatorTest, BuiltinRuleCustom) {
    auto rule = rules::custom([](const std::string& v) { return v == "x"; },
                              "Must be x");
    EXPECT_TRUE(rule->validate("x"));
    EXPECT_FALSE(rule->validate("y"));
}

TEST_F(ValidatorTest, ValidatorValidateAllValid) {
    ValidationSchema schema;
    schema.required("A").optional("B", "def");
    schema.rule("A", rules::notEmpty());
    schema.rule("B", rules::minLength(2));
    std::unordered_map<std::string, std::string> env = {{"A", "val"},
                                                        {"B", "xx"}};
    Validator validator;
    auto result = validator.validate(env, schema);
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_EQ(result.processed_vars["A"], "val");
    EXPECT_EQ(result.processed_vars["B"], "xx");
}

TEST_F(ValidatorTest, ValidatorValidateMissingRequired) {
    ValidationSchema schema;
    schema.required("A");
    std::unordered_map<std::string, std::string> env = {{"B", "val"}};
    Validator validator;
    auto result = validator.validate(env, schema);
    EXPECT_FALSE(result.is_valid);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("Required variable 'A'"),
              std::string::npos);
}

TEST_F(ValidatorTest, ValidatorValidateRuleFailure) {
    ValidationSchema schema;
    schema.required("A").rule("A", rules::minLength(5));
    std::unordered_map<std::string, std::string> env = {{"A", "abc"}};
    Validator validator;
    auto result = validator.validate(env, schema);
    EXPECT_FALSE(result.is_valid);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("failed validation"), std::string::npos);
}

TEST_F(ValidatorTest, ValidatorValidateWithDefaults) {
    ValidationSchema schema;
    schema.required("A").optional("B", "defB");
    schema.rule("A", rules::notEmpty());
    std::unordered_map<std::string, std::string> env = {{"A", "val"}};
    Validator validator;
    auto result = validator.validateWithDefaults(env, schema);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(env["B"], "defB");
    EXPECT_EQ(result.processed_vars["B"], "defB");
}

TEST_F(ValidatorTest, ValidatorValidateMultipleRules) {
    ValidationSchema schema;
    schema.required("A").rules("A", {rules::notEmpty(), rules::minLength(2)});
    std::unordered_map<std::string, std::string> env = {{"A", "a"}};
    Validator validator;
    auto result = validator.validate(env, schema);
    EXPECT_FALSE(result.is_valid);
    ASSERT_EQ(result.errors.size(), 1);
    EXPECT_NE(result.errors[0].find("minLength"), std::string::npos);
}

TEST_F(ValidatorTest, ValidatorValidateNoRules) {
    ValidationSchema schema;
    schema.required("A");
    std::unordered_map<std::string, std::string> env = {{"A", "anything"}};
    Validator validator;
    auto result = validator.validate(env, schema);
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
}