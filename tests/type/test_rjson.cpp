// filepath: d:\msys64\home\qwdma\Atom\tests\type\test_rjson.hpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>


#include "atom/type/rjson.hpp"

using namespace atom::type;

class JsonValueTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Helper function to create a sample JsonObject
JsonObject createSampleObject() {
    JsonObject obj;
    obj["string"] = JsonValue(std::string("Hello, world!"));
    obj["number"] = JsonValue(42.5);
    obj["boolean"] = JsonValue(true);

    JsonArray arr;
    arr.push_back(JsonValue(1.0));
    arr.push_back(JsonValue(2.0));
    arr.push_back(JsonValue(3.0));
    obj["array"] = JsonValue(arr);

    JsonObject nested;
    nested["key"] = JsonValue(std::string("value"));
    obj["object"] = JsonValue(nested);

    return obj;
}

// JsonValue Construction Tests
TEST_F(JsonValueTest, DefaultConstructor) {
    JsonValue value;
    EXPECT_EQ(value.type(), JsonValue::Type::Null);
}

TEST_F(JsonValueTest, StringConstructor) {
    JsonValue value(std::string("test"));
    EXPECT_EQ(value.type(), JsonValue::Type::String);
    EXPECT_EQ(value.asString(), "test");
}

TEST_F(JsonValueTest, NumberConstructor) {
    JsonValue value(123.45);
    EXPECT_EQ(value.type(), JsonValue::Type::Number);
    EXPECT_DOUBLE_EQ(value.asNumber(), 123.45);
}

TEST_F(JsonValueTest, BooleanConstructor) {
    JsonValue valueTrue(true);
    EXPECT_EQ(valueTrue.type(), JsonValue::Type::Bool);
    EXPECT_TRUE(valueTrue.asBool());

    JsonValue valueFalse(false);
    EXPECT_EQ(valueFalse.type(), JsonValue::Type::Bool);
    EXPECT_FALSE(valueFalse.asBool());
}

TEST_F(JsonValueTest, ObjectConstructor) {
    JsonObject obj;
    obj["key"] = JsonValue(std::string("value"));

    JsonValue value(obj);
    EXPECT_EQ(value.type(), JsonValue::Type::Object);
    EXPECT_EQ(value.asObject().size(), 1);
    EXPECT_EQ(value.asObject().at("key").asString(), "value");
}

TEST_F(JsonValueTest, ArrayConstructor) {
    JsonArray arr;
    arr.push_back(JsonValue(1.0));
    arr.push_back(JsonValue(2.0));

    JsonValue value(arr);
    EXPECT_EQ(value.type(), JsonValue::Type::Array);
    EXPECT_EQ(value.asArray().size(), 2);
    EXPECT_DOUBLE_EQ(value.asArray()[0].asNumber(), 1.0);
    EXPECT_DOUBLE_EQ(value.asArray()[1].asNumber(), 2.0);
}

// Type Getters and Value Access
TEST_F(JsonValueTest, TypeMethod) {
    EXPECT_EQ(JsonValue().type(), JsonValue::Type::Null);
    EXPECT_EQ(JsonValue(std::string("test")).type(), JsonValue::Type::String);
    EXPECT_EQ(JsonValue(42.0).type(), JsonValue::Type::Number);
    EXPECT_EQ(JsonValue(true).type(), JsonValue::Type::Bool);
    EXPECT_EQ(JsonValue(JsonObject{}).type(), JsonValue::Type::Object);
    EXPECT_EQ(JsonValue(JsonArray{}).type(), JsonValue::Type::Array);
}

TEST_F(JsonValueTest, AsStringMethod) {
    JsonValue value(std::string("test"));
    EXPECT_EQ(value.asString(), "test");

    // Should throw when used on wrong type
    EXPECT_THROW(JsonValue(42.0).asString(), std::bad_variant_access);
}

TEST_F(JsonValueTest, AsNumberMethod) {
    JsonValue value(42.5);
    EXPECT_DOUBLE_EQ(value.asNumber(), 42.5);

    // Should throw when used on wrong type
    EXPECT_THROW(JsonValue(std::string("test")).asNumber(),
                 std::bad_variant_access);
}

TEST_F(JsonValueTest, AsBoolMethod) {
    JsonValue value(true);
    EXPECT_TRUE(value.asBool());

    // Should throw when used on wrong type
    EXPECT_THROW(JsonValue(42.0).asBool(), std::bad_variant_access);
}

TEST_F(JsonValueTest, AsObjectMethod) {
    JsonObject obj;
    obj["key"] = JsonValue(std::string("value"));

    JsonValue value(obj);
    const JsonObject& result = value.asObject();
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result.at("key").asString(), "value");

    // Should throw when used on wrong type
    EXPECT_THROW(JsonValue(42.0).asObject(), std::bad_variant_access);
}

TEST_F(JsonValueTest, AsArrayMethod) {
    JsonArray arr;
    arr.push_back(JsonValue(1.0));
    arr.push_back(JsonValue(2.0));

    JsonValue value(arr);
    const JsonArray& result = value.asArray();
    EXPECT_EQ(result.size(), 2);
    EXPECT_DOUBLE_EQ(result[0].asNumber(), 1.0);
    EXPECT_DOUBLE_EQ(result[1].asNumber(), 2.0);

    // Should throw when used on wrong type
    EXPECT_THROW(JsonValue(42.0).asArray(), std::bad_variant_access);
}

// Operator[] Tests
TEST_F(JsonValueTest, StringIndexOperator) {
    JsonObject obj = createSampleObject();
    JsonValue value(obj);

    EXPECT_EQ(value["string"].asString(), "Hello, world!");
    EXPECT_DOUBLE_EQ(value["number"].asNumber(), 42.5);
    EXPECT_TRUE(value["boolean"].asBool());
    EXPECT_EQ(value["array"].asArray().size(), 3);
    EXPECT_EQ(value["object"].asObject().size(), 1);

    // Should throw when key doesn't exist
    EXPECT_THROW(value["nonexistent"], std::out_of_range);

    // Should throw when used on wrong type
    EXPECT_THROW(JsonValue(42.0)["key"], std::bad_variant_access);
}

TEST_F(JsonValueTest, NumericIndexOperator) {
    JsonArray arr;
    arr.push_back(JsonValue(std::string("first")));
    arr.push_back(JsonValue(2.0));
    arr.push_back(JsonValue(true));

    JsonValue value(arr);

    EXPECT_EQ(value[0].asString(), "first");
    EXPECT_DOUBLE_EQ(value[1].asNumber(), 2.0);
    EXPECT_TRUE(value[2].asBool());

    // Should throw when index is out of bounds
    EXPECT_THROW(value[3], std::out_of_range);

    // Should throw when used on wrong type
    EXPECT_THROW(JsonValue(42.0)[0], std::bad_variant_access);
}

// ToString Tests
TEST_F(JsonValueTest, ToStringNullValue) {
    JsonValue value;
    EXPECT_EQ(value.toString(), "null");
}

TEST_F(JsonValueTest, ToStringStringValue) {
    JsonValue value(std::string("test"));
    EXPECT_EQ(value.toString(), "\"test\"");

    // String with special characters
    JsonValue valueSpecial(std::string("line1\nline2"));
    EXPECT_EQ(valueSpecial.toString(), "\"line1\\nline2\"");
}

TEST_F(JsonValueTest, ToStringNumberValue) {
    JsonValue valueInt(42.0);
    EXPECT_EQ(valueInt.toString(), "42");

    JsonValue valueFloat(42.5);
    EXPECT_EQ(valueFloat.toString(), "42.5");
}

TEST_F(JsonValueTest, ToStringBooleanValue) {
    JsonValue valueTrue(true);
    EXPECT_EQ(valueTrue.toString(), "true");

    JsonValue valueFalse(false);
    EXPECT_EQ(valueFalse.toString(), "false");
}

TEST_F(JsonValueTest, ToStringArrayValue) {
    JsonArray arr;
    arr.push_back(JsonValue(1.0));
    arr.push_back(JsonValue(std::string("test")));
    arr.push_back(JsonValue(true));

    JsonValue value(arr);
    EXPECT_EQ(value.toString(), "[1,\"test\",true]");
}

TEST_F(JsonValueTest, ToStringObjectValue) {
    JsonObject obj;
    obj["number"] = JsonValue(42.0);
    obj["string"] = JsonValue(std::string("test"));
    obj["bool"] = JsonValue(true);

    JsonValue value(obj);
    // Object keys can be in any order, so we need to check for each key-value
    // pair
    std::string str = value.toString();
    EXPECT_TRUE(str.find("\"number\":42") != std::string::npos);
    EXPECT_TRUE(str.find("\"string\":\"test\"") != std::string::npos);
    EXPECT_TRUE(str.find("\"bool\":true") != std::string::npos);
}

// JsonParser Tests
class JsonParserTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(JsonParserTest, ParseNull) {
    JsonValue value = JsonParser::parse("null");
    EXPECT_EQ(value.type(), JsonValue::Type::Null);
}

TEST_F(JsonParserTest, ParseBooleans) {
    JsonValue valueTrue = JsonParser::parse("true");
    EXPECT_EQ(valueTrue.type(), JsonValue::Type::Bool);
    EXPECT_TRUE(valueTrue.asBool());

    JsonValue valueFalse = JsonParser::parse("false");
    EXPECT_EQ(valueFalse.type(), JsonValue::Type::Bool);
    EXPECT_FALSE(valueFalse.asBool());
}

TEST_F(JsonParserTest, ParseNumbers) {
    // Integer
    JsonValue valueInt = JsonParser::parse("42");
    EXPECT_EQ(valueInt.type(), JsonValue::Type::Number);
    EXPECT_DOUBLE_EQ(valueInt.asNumber(), 42.0);

    // Float
    JsonValue valueFloat = JsonParser::parse("42.5");
    EXPECT_EQ(valueFloat.type(), JsonValue::Type::Number);
    EXPECT_DOUBLE_EQ(valueFloat.asNumber(), 42.5);

    // Negative
    JsonValue valueNegative = JsonParser::parse("-42.5");
    EXPECT_EQ(valueNegative.type(), JsonValue::Type::Number);
    EXPECT_DOUBLE_EQ(valueNegative.asNumber(), -42.5);

    // Scientific notation
    JsonValue valueScientific = JsonParser::parse("1.23e4");
    EXPECT_EQ(valueScientific.type(), JsonValue::Type::Number);
    EXPECT_DOUBLE_EQ(valueScientific.asNumber(), 12300.0);
}

TEST_F(JsonParserTest, ParseStrings) {
    // Simple string
    JsonValue valueSimple = JsonParser::parse("\"Hello, world!\"");
    EXPECT_EQ(valueSimple.type(), JsonValue::Type::String);
    EXPECT_EQ(valueSimple.asString(), "Hello, world!");

    // String with escaped characters
    JsonValue valueEscaped = JsonParser::parse("\"Hello\\nWorld\\t!\"");
    EXPECT_EQ(valueEscaped.type(), JsonValue::Type::String);
    EXPECT_EQ(valueEscaped.asString(), "Hello\nWorld\t!");

    // String with Unicode escapes (if supported)
    // JsonValue valueUnicode = JsonParser::parse("\"Hello \\u0057orld!\"");
    // EXPECT_EQ(valueUnicode.type(), JsonValue::Type::String);
    // EXPECT_EQ(valueUnicode.asString(), "Hello World!");
}

TEST_F(JsonParserTest, ParseArrays) {
    // Empty array
    JsonValue valueEmpty = JsonParser::parse("[]");
    EXPECT_EQ(valueEmpty.type(), JsonValue::Type::Array);
    EXPECT_EQ(valueEmpty.asArray().size(), 0);

    // Array with single element
    JsonValue valueSingle = JsonParser::parse("[42]");
    EXPECT_EQ(valueSingle.type(), JsonValue::Type::Array);
    EXPECT_EQ(valueSingle.asArray().size(), 1);
    EXPECT_DOUBLE_EQ(valueSingle.asArray()[0].asNumber(), 42.0);

    // Array with multiple elements of different types
    JsonValue valueMulti = JsonParser::parse("[42, \"test\", true, null]");
    EXPECT_EQ(valueMulti.type(), JsonValue::Type::Array);
    EXPECT_EQ(valueMulti.asArray().size(), 4);
    EXPECT_DOUBLE_EQ(valueMulti.asArray()[0].asNumber(), 42.0);
    EXPECT_EQ(valueMulti.asArray()[1].asString(), "test");
    EXPECT_TRUE(valueMulti.asArray()[2].asBool());
    EXPECT_EQ(valueMulti.asArray()[3].type(), JsonValue::Type::Null);

    // Nested arrays
    JsonValue valueNested = JsonParser::parse("[[1, 2], [3, 4]]");
    EXPECT_EQ(valueNested.type(), JsonValue::Type::Array);
    EXPECT_EQ(valueNested.asArray().size(), 2);
    EXPECT_EQ(valueNested.asArray()[0].asArray().size(), 2);
    EXPECT_EQ(valueNested.asArray()[1].asArray().size(), 2);
    EXPECT_DOUBLE_EQ(valueNested.asArray()[0].asArray()[0].asNumber(), 1.0);
    EXPECT_DOUBLE_EQ(valueNested.asArray()[0].asArray()[1].asNumber(), 2.0);
    EXPECT_DOUBLE_EQ(valueNested.asArray()[1].asArray()[0].asNumber(), 3.0);
    EXPECT_DOUBLE_EQ(valueNested.asArray()[1].asArray()[1].asNumber(), 4.0);
}

TEST_F(JsonParserTest, ParseObjects) {
    // Empty object
    JsonValue valueEmpty = JsonParser::parse("{}");
    EXPECT_EQ(valueEmpty.type(), JsonValue::Type::Object);
    EXPECT_EQ(valueEmpty.asObject().size(), 0);

    // Object with single key-value pair
    JsonValue valueSingle = JsonParser::parse("{\"key\": 42}");
    EXPECT_EQ(valueSingle.type(), JsonValue::Type::Object);
    EXPECT_EQ(valueSingle.asObject().size(), 1);
    EXPECT_DOUBLE_EQ(valueSingle.asObject().at("key").asNumber(), 42.0);

    // Object with multiple key-value pairs of different types
    JsonValue valueMulti = JsonParser::parse(
        "{\"number\": 42, \"string\": \"test\", \"bool\": true, \"null\": "
        "null}");
    EXPECT_EQ(valueMulti.type(), JsonValue::Type::Object);
    EXPECT_EQ(valueMulti.asObject().size(), 4);
    EXPECT_DOUBLE_EQ(valueMulti.asObject().at("number").asNumber(), 42.0);
    EXPECT_EQ(valueMulti.asObject().at("string").asString(), "test");
    EXPECT_TRUE(valueMulti.asObject().at("bool").asBool());
    EXPECT_EQ(valueMulti.asObject().at("null").type(), JsonValue::Type::Null);

    // Nested objects
    JsonValue valueNested = JsonParser::parse("{\"outer\": {\"inner\": 42}}");
    EXPECT_EQ(valueNested.type(), JsonValue::Type::Object);
    EXPECT_EQ(valueNested.asObject().size(), 1);
    EXPECT_EQ(valueNested.asObject().at("outer").type(),
              JsonValue::Type::Object);
    EXPECT_EQ(valueNested.asObject().at("outer").asObject().size(), 1);
    EXPECT_DOUBLE_EQ(
        valueNested.asObject().at("outer").asObject().at("inner").asNumber(),
        42.0);
}

TEST_F(JsonParserTest, ParseComplex) {
    std::string json = R"(
    {
        "string": "Hello, world!",
        "number": 42.5,
        "boolean": true,
        "null": null,
        "array": [1, 2, 3, 4, 5],
        "object": {
            "nestedString": "Nested value",
            "nestedArray": [true, false]
        }
    }
    )";

    JsonValue value = JsonParser::parse(json);
    EXPECT_EQ(value.type(), JsonValue::Type::Object);
    EXPECT_EQ(value.asObject().size(), 6);

    EXPECT_EQ(value.asObject().at("string").asString(), "Hello, world!");
    EXPECT_DOUBLE_EQ(value.asObject().at("number").asNumber(), 42.5);
    EXPECT_TRUE(value.asObject().at("boolean").asBool());
    EXPECT_EQ(value.asObject().at("null").type(), JsonValue::Type::Null);

    // Check array
    const JsonArray& array = value.asObject().at("array").asArray();
    EXPECT_EQ(array.size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_DOUBLE_EQ(array[i].asNumber(), i + 1);
    }

    // Check nested object
    const JsonObject& nestedObj = value.asObject().at("object").asObject();
    EXPECT_EQ(nestedObj.size(), 2);
    EXPECT_EQ(nestedObj.at("nestedString").asString(), "Nested value");

    const JsonArray& nestedArray = nestedObj.at("nestedArray").asArray();
    EXPECT_EQ(nestedArray.size(), 2);
    EXPECT_TRUE(nestedArray[0].asBool());
    EXPECT_FALSE(nestedArray[1].asBool());
}

TEST_F(JsonParserTest, ParseWithWhitespace) {
    std::string json = R"(
    {
        "key1": 42,
        "key2": "value"
    }
    )";

    JsonValue value = JsonParser::parse(json);
    EXPECT_EQ(value.type(), JsonValue::Type::Object);
    EXPECT_EQ(value.asObject().size(), 2);
    EXPECT_DOUBLE_EQ(value.asObject().at("key1").asNumber(), 42.0);
    EXPECT_EQ(value.asObject().at("key2").asString(), "value");
}

TEST_F(JsonParserTest, ParseInvalidJson) {
    // Missing closing quotation mark
    EXPECT_THROW(JsonParser::parse("\"Hello"), std::runtime_error);

    // Invalid number format
    EXPECT_THROW(JsonParser::parse("42."), std::runtime_error);

    // Invalid object format (missing value)
    EXPECT_THROW(JsonParser::parse("{\"key\": }"), std::runtime_error);

    // Invalid object format (missing comma)
    EXPECT_THROW(JsonParser::parse("{\"key1\": 42 \"key2\": 43}"),
                 std::runtime_error);

    // Invalid array format (missing comma)
    EXPECT_THROW(JsonParser::parse("[1 2 3]"), std::runtime_error);
}

// Round-trip Tests
TEST_F(JsonParserTest, RoundtripSimpleValues) {
    // String
    {
        const std::string original = "\"Hello, world!\"";
        JsonValue value = JsonParser::parse(original);
        std::string serialized = value.toString();
        JsonValue reparsed = JsonParser::parse(serialized);

        EXPECT_EQ(value.type(), reparsed.type());
        EXPECT_EQ(value.asString(), reparsed.asString());
    }

    // Number
    {
        const std::string original = "42.5";
        JsonValue value = JsonParser::parse(original);
        std::string serialized = value.toString();
        JsonValue reparsed = JsonParser::parse(serialized);

        EXPECT_EQ(value.type(), reparsed.type());
        EXPECT_DOUBLE_EQ(value.asNumber(), reparsed.asNumber());
    }

    // Boolean
    {
        const std::string original = "true";
        JsonValue value = JsonParser::parse(original);
        std::string serialized = value.toString();
        JsonValue reparsed = JsonParser::parse(serialized);

        EXPECT_EQ(value.type(), reparsed.type());
        EXPECT_EQ(value.asBool(), reparsed.asBool());
    }

    // Null
    {
        const std::string original = "null";
        JsonValue value = JsonParser::parse(original);
        std::string serialized = value.toString();
        JsonValue reparsed = JsonParser::parse(serialized);

        EXPECT_EQ(value.type(), reparsed.type());
    }
}

TEST_F(JsonParserTest, RoundtripComplexStructure) {
    const std::string original = R"(
    {
        "string": "Hello, world!",
        "number": 42.5,
        "boolean": true,
        "null": null,
        "array": [1, 2, 3],
        "object": {
            "key": "value"
        }
    }
    )";

    JsonValue value = JsonParser::parse(original);
    std::string serialized = value.toString();
    JsonValue reparsed = JsonParser::parse(serialized);

    // Check that all values are preserved
    EXPECT_EQ(value.asObject().at("string").asString(),
              reparsed.asObject().at("string").asString());
    EXPECT_DOUBLE_EQ(value.asObject().at("number").asNumber(),
                     reparsed.asObject().at("number").asNumber());
    EXPECT_EQ(value.asObject().at("boolean").asBool(),
              reparsed.asObject().at("boolean").asBool());
    EXPECT_EQ(value.asObject().at("null").type(),
              reparsed.asObject().at("null").type());

    // Check array
    EXPECT_EQ(value.asObject().at("array").asArray().size(),
              reparsed.asObject().at("array").asArray().size());
    for (size_t i = 0; i < value.asObject().at("array").asArray().size(); ++i) {
        EXPECT_DOUBLE_EQ(
            value.asObject().at("array").asArray()[i].asNumber(),
            reparsed.asObject().at("array").asArray()[i].asNumber());
    }

    // Check nested object
    EXPECT_EQ(value.asObject().at("object").asObject().at("key").asString(),
              reparsed.asObject().at("object").asObject().at("key").asString());
}
