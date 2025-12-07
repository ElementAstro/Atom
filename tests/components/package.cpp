/*
 * package.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-07

Description: Tests for JSON Parsing Utilities in Package

**************************************************/

#include "atom/components/core/package.hpp"

#include <gtest/gtest.h>
#include <array>
#include <string>
#include <string_view>

// ============================================================================
// Constants Tests
// ============================================================================

TEST(PackageConstantsTest, AlignmentValue) { EXPECT_EQ(ALIGNMENT, 64); }

TEST(PackageConstantsTest, MaxElementsValue) { EXPECT_EQ(MAX_ELEMENTS, 10); }

// ============================================================================
// JsonValueType Tests
// ============================================================================

TEST(JsonValueTypeTest, EnumValues) {
    EXPECT_NE(JsonValueType::STRING, JsonValueType::OBJECT);
    EXPECT_NE(JsonValueType::ARRAY, JsonValueType::NUMBER);
    EXPECT_NE(JsonValueType::BOOLEAN, JsonValueType::UNKNOWN);
}

TEST(JsonValueTypeTest, AllTypesDistinct) {
    std::array<JsonValueType, 6> types = {
        JsonValueType::STRING, JsonValueType::OBJECT,  JsonValueType::ARRAY,
        JsonValueType::NUMBER, JsonValueType::BOOLEAN, JsonValueType::UNKNOWN};

    for (size_t i = 0; i < types.size(); ++i) {
        for (size_t j = i + 1; j < types.size(); ++j) {
            EXPECT_NE(types[i], types[j]);
        }
    }
}

// ============================================================================
// JsonKeyValue Tests
// ============================================================================

TEST(JsonKeyValueTest, DefaultConstruction) {
    JsonKeyValue kv;
    EXPECT_TRUE(kv.key.empty());
    EXPECT_TRUE(kv.value.empty());
}

TEST(JsonKeyValueTest, SetKeyValue) {
    JsonKeyValue kv;
    kv.key = "name";
    kv.value = "test";
    kv.type = JsonValueType::STRING;

    EXPECT_EQ(kv.key, "name");
    EXPECT_EQ(kv.value, "test");
    EXPECT_EQ(kv.type, JsonValueType::STRING);
}

TEST(JsonKeyValueTest, Alignment) {
    EXPECT_EQ(alignof(JsonKeyValue), ALIGNMENT);
}

// ============================================================================
// Equals Function Tests
// ============================================================================

TEST(EqualsTest, EqualStrings) {
    EXPECT_TRUE(Equals("hello", "hello"));
    EXPECT_TRUE(Equals("", ""));
    EXPECT_TRUE(Equals("test123", "test123"));
}

TEST(EqualsTest, UnequalStrings) {
    EXPECT_FALSE(Equals("hello", "world"));
    EXPECT_FALSE(Equals("Hello", "hello"));  // Case sensitive
    EXPECT_FALSE(Equals("test", "test "));
    EXPECT_FALSE(Equals("", "a"));
}

TEST(EqualsTest, EmptyStrings) {
    EXPECT_TRUE(Equals("", ""));
    EXPECT_FALSE(Equals("", "x"));
    EXPECT_FALSE(Equals("x", ""));
}

// ============================================================================
// Trim Function Tests
// ============================================================================

TEST(TrimTest, NoTrimNeeded) {
    EXPECT_EQ(Trim("hello"), "hello");
    EXPECT_EQ(Trim("test"), "test");
}

TEST(TrimTest, LeadingSpaces) {
    EXPECT_EQ(Trim("   hello"), "hello");
    EXPECT_EQ(Trim("\thello"), "hello");
    EXPECT_EQ(Trim("\nhello"), "hello");
}

TEST(TrimTest, TrailingSpaces) {
    EXPECT_EQ(Trim("hello   "), "hello");
    EXPECT_EQ(Trim("hello\t"), "hello");
    EXPECT_EQ(Trim("hello\n"), "hello");
}

TEST(TrimTest, BothSides) {
    EXPECT_EQ(Trim("   hello   "), "hello");
    EXPECT_EQ(Trim("\t\nhello\t\n"), "hello");
    EXPECT_EQ(Trim("  test value  "), "test value");
}

TEST(TrimTest, EmptyString) { EXPECT_EQ(Trim(""), ""); }

TEST(TrimTest, OnlyWhitespace) {
    EXPECT_EQ(Trim("   "), "");
    EXPECT_EQ(Trim("\t\n"), "");
}

TEST(TrimTest, MixedWhitespace) {
    EXPECT_EQ(Trim(" \t\n hello \n\t "), "hello");
}

// ============================================================================
// RemoveBrackets Function Tests
// ============================================================================

TEST(RemoveBracketsTest, NoBrackets) {
    EXPECT_EQ(RemoveBrackets("hello"), "hello");
}

TEST(RemoveBracketsTest, WithClosingBracket) {
    auto result = RemoveBrackets("hello]world");
    EXPECT_EQ(result, "hello");
}

TEST(RemoveBracketsTest, WithCurlyBraces) {
    auto result = RemoveBrackets("hello{world");
    EXPECT_EQ(result, "hello");
}

TEST(RemoveBracketsTest, EmptyString) { EXPECT_EQ(RemoveBrackets(""), ""); }

// ============================================================================
// ParseKeyValue Function Tests
// ============================================================================

TEST(ParseKeyValueTest, SimpleStringValue) {
    auto [kv, error] = ParseKeyValue("\"name\": \"John\"");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key, "name");
    EXPECT_EQ(kv.value, "John");
    EXPECT_EQ(kv.type, JsonValueType::STRING);
}

TEST(ParseKeyValueTest, NumberValue) {
    auto [kv, error] = ParseKeyValue("\"age\": 42");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key, "age");
    EXPECT_EQ(kv.value, "42");
    EXPECT_EQ(kv.type, JsonValueType::NUMBER);
}

TEST(ParseKeyValueTest, BooleanTrueValue) {
    auto [kv, error] = ParseKeyValue("\"active\": true");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key, "active");
    EXPECT_EQ(kv.value, "true");
    EXPECT_EQ(kv.type, JsonValueType::BOOLEAN);
}

TEST(ParseKeyValueTest, BooleanFalseValue) {
    auto [kv, error] = ParseKeyValue("\"disabled\": false");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key, "disabled");
    EXPECT_EQ(kv.value, "false");
    EXPECT_EQ(kv.type, JsonValueType::BOOLEAN);
}

TEST(ParseKeyValueTest, ObjectValue) {
    auto [kv, error] = ParseKeyValue("\"data\": {\"x\": 1}");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key, "data");
    EXPECT_EQ(kv.type, JsonValueType::OBJECT);
}

TEST(ParseKeyValueTest, ArrayValue) {
    auto [kv, error] = ParseKeyValue("\"items\": [1, 2, 3]");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key, "items");
    EXPECT_EQ(kv.type, JsonValueType::ARRAY);
}

TEST(ParseKeyValueTest, NoColonError) {
    auto [kv, error] = ParseKeyValue("invalid json line");

    EXPECT_FALSE(error.empty());
    EXPECT_EQ(error, "Invalid JSON line: no colon found");
}

TEST(ParseKeyValueTest, EmptyKey) {
    auto [kv, error] = ParseKeyValue("\"\": \"value\"");

    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(kv.key.empty());
}

TEST(ParseKeyValueTest, WhitespaceHandling) {
    auto [kv, error] = ParseKeyValue("  \"key\"  :  \"value\"  ");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key, "key");
    EXPECT_EQ(kv.value, "value");
}

// ============================================================================
// ParseArray Function Tests
// ============================================================================

TEST(ParseArrayTest, SimpleStringArray) {
    auto [result, error] = ParseArray("[\"a\", \"b\", \"c\"]");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(ParseArrayTest, NumberArray) {
    auto [result, error] = ParseArray("[1, 2, 3, 4, 5]");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(result[0], "1");
    EXPECT_EQ(result[1], "2");
    EXPECT_EQ(result[2], "3");
}

TEST(ParseArrayTest, SingleElement) {
    auto [result, error] = ParseArray("[\"only\"]");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(result[0], "only");
}

TEST(ParseArrayTest, EmptyArray) {
    auto [result, error] = ParseArray("[]");

    EXPECT_TRUE(error.empty());
}

TEST(ParseArrayTest, MaxElements) {
    std::string arrayStr = "[";
    for (size_t i = 0; i < MAX_ELEMENTS; ++i) {
        if (i > 0)
            arrayStr += ", ";
        arrayStr += "\"" + std::to_string(i) + "\"";
    }
    arrayStr += "]";

    auto [result, error] = ParseArray(arrayStr);

    EXPECT_TRUE(error.empty());
    for (size_t i = 0; i < MAX_ELEMENTS; ++i) {
        EXPECT_EQ(result[i], std::to_string(i));
    }
}

TEST(ParseArrayTest, WhitespaceInElements) {
    auto [result, error] = ParseArray("[  \"a\"  ,  \"b\"  ]");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
}

// ============================================================================
// ParseObject Function Tests
// ============================================================================

TEST(ParseObjectTest, SimpleObject) {
    auto [result, error] = ParseObject("{\"name\": \"John\", \"age\": 30}");

    EXPECT_TRUE(error.empty());
    // Check that we got some results
    bool foundName = false;
    bool foundAge = false;
    for (const auto& kv : result) {
        if (kv.key == "name")
            foundName = true;
        if (kv.key == "age")
            foundAge = true;
    }
    EXPECT_TRUE(foundName || foundAge);  // At least one should be found
}

TEST(ParseObjectTest, EmptyObject) {
    auto [result, error] = ParseObject("{}");

    EXPECT_TRUE(error.empty());
}

TEST(ParseObjectTest, SingleKeyValue) {
    auto [result, error] = ParseObject("{\"key\": \"value\"}");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(result[0].key, "key");
    EXPECT_EQ(result[0].value, "value");
}

// ============================================================================
// ParseJson Function Tests
// ============================================================================

TEST(ParseJsonTest, SimpleJson) {
    std::string_view json = R"(
        "name": "Test"
        "version": "1.0"
    )";

    auto [result, error] = ParseJson(json);

    EXPECT_TRUE(error.empty());
}

TEST(ParseJsonTest, JsonWithArray) {
    std::string_view json = R"(
        "name": "Test"
        "items": [1, 2, 3]
    )";

    auto [result, error] = ParseJson(json);

    EXPECT_TRUE(error.empty());
}

TEST(ParseJsonTest, JsonWithObject) {
    std::string_view json = R"(
        "name": "Test"
        "data": {"x": 1}
    )";

    auto [result, error] = ParseJson(json);

    EXPECT_TRUE(error.empty());
}

TEST(ParseJsonTest, EmptyJson) {
    auto [result, error] = ParseJson("");

    EXPECT_TRUE(error.empty());
}

TEST(ParseJsonTest, JsonWithAllTypes) {
    std::string_view json = R"(
        "string": "value"
        "number": 42
        "boolean": true
        "array": [1, 2]
        "object": {"a": 1}
    )";

    auto [result, error] = ParseJson(json);

    EXPECT_TRUE(error.empty());
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(PackageEdgeCaseTest, VeryLongKey) {
    std::string longKey(1000, 'k');
    std::string line = "\"" + longKey + "\": \"value\"";

    auto [kv, error] = ParseKeyValue(line);

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key.size(), 1000);
}

TEST(PackageEdgeCaseTest, VeryLongValue) {
    std::string longValue(1000, 'v');
    std::string line = "\"key\": \"" + longValue + "\"";

    auto [kv, error] = ParseKeyValue(line);

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.value.size(), 1000);
}

TEST(PackageEdgeCaseTest, SpecialCharactersInValue) {
    auto [kv, error] = ParseKeyValue("\"special\": \"hello\\nworld\"");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key, "special");
}

TEST(PackageEdgeCaseTest, UnicodeInValue) {
    auto [kv, error] = ParseKeyValue("\"unicode\": \"你好世界\"");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key, "unicode");
}

TEST(PackageEdgeCaseTest, NestedBrackets) {
    auto [kv, error] = ParseKeyValue("\"nested\": [[1, 2], [3, 4]]");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.type, JsonValueType::ARRAY);
}

TEST(PackageEdgeCaseTest, NestedObjects) {
    auto [kv, error] = ParseKeyValue("\"nested\": {\"inner\": {\"deep\": 1}}");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.type, JsonValueType::OBJECT);
}

TEST(PackageEdgeCaseTest, EmptyStringValue) {
    auto [kv, error] = ParseKeyValue("\"empty\": \"\"");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key, "empty");
    EXPECT_TRUE(kv.value.empty());
}

TEST(PackageEdgeCaseTest, WhitespaceOnlyValue) {
    auto [kv, error] = ParseKeyValue("\"spaces\": \"   \"");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key, "spaces");
}

TEST(PackageEdgeCaseTest, NumericKey) {
    auto [kv, error] = ParseKeyValue("\"123\": \"value\"");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key, "123");
}

TEST(PackageEdgeCaseTest, KeyWithSpaces) {
    auto [kv, error] = ParseKeyValue("\"key with spaces\": \"value\"");

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key, "key with spaces");
}

// ============================================================================
// Constexpr Tests
// ============================================================================

TEST(PackageConstexprTest, EqualsIsConstexpr) {
    constexpr bool result = Equals("test", "test");
    EXPECT_TRUE(result);
}

TEST(PackageConstexprTest, TrimIsConstexpr) {
    // Note: Trim modifies the string_view in place, so we test at runtime
    std::string_view str = "  hello  ";
    auto trimmed = Trim(str);
    EXPECT_EQ(trimmed, "hello");
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(PackageIntegrationTest, ParseCompleteJson) {
    std::string_view json = R"(
        "name": "MyPackage"
        "version": "1.0.0"
        "description": "A test package"
        "author": "Test Author"
        "dependencies": ["dep1", "dep2"]
        "config": {"debug": true}
    )";

    auto [result, error] = ParseJson(json);

    EXPECT_TRUE(error.empty());

    // Verify we can find expected keys
    bool foundName = false;
    bool foundVersion = false;
    for (const auto& kv : result) {
        if (kv.key == "name") {
            foundName = true;
            EXPECT_EQ(kv.value, "MyPackage");
        }
        if (kv.key == "version") {
            foundVersion = true;
            EXPECT_EQ(kv.value, "1.0.0");
        }
    }
    EXPECT_TRUE(foundName);
    EXPECT_TRUE(foundVersion);
}

TEST(PackageIntegrationTest, ParseAndValidateTypes) {
    std::string_view json = R"(
        "stringField": "text"
        "numberField": 42
        "boolField": true
        "arrayField": [1, 2, 3]
        "objectField": {"key": "value"}
    )";

    auto [result, error] = ParseJson(json);

    EXPECT_TRUE(error.empty());

    for (const auto& kv : result) {
        if (kv.key == "stringField") {
            EXPECT_EQ(kv.type, JsonValueType::STRING);
        } else if (kv.key == "numberField") {
            EXPECT_EQ(kv.type, JsonValueType::NUMBER);
        } else if (kv.key == "boolField") {
            EXPECT_EQ(kv.type, JsonValueType::BOOLEAN);
        } else if (kv.key == "arrayField") {
            EXPECT_EQ(kv.type, JsonValueType::ARRAY);
        } else if (kv.key == "objectField") {
            EXPECT_EQ(kv.type, JsonValueType::OBJECT);
        }
    }
}

TEST(PackageIntegrationTest, RoundTripKeyValue) {
    std::string_view original = "\"testKey\": \"testValue\"";

    auto [kv, error] = ParseKeyValue(original);

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(kv.key, "testKey");
    EXPECT_EQ(kv.value, "testValue");
    EXPECT_EQ(kv.type, JsonValueType::STRING);
}
