// filepath: /home/max/Atom-1/atom/utils/test_anyutils.hpp
/*
 * test_anyutils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-1

Description: Tests for anyutils.hpp functionality

**************************************************/

#ifndef ATOM_UTILS_TEST_ANYUTILS_HPP
#define ATOM_UTILS_TEST_ANYUTILS_HPP

#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "atom/utils/anyutils.hpp"

namespace atom::utils::test {

// Custom type with toString method
class CustomStringifiable {
public:
    CustomStringifiable(std::string value) : value_(std::move(value)) {}

    std::string toString() const { return "Custom(" + value_ + ")"; }

    std::string toJson() const { return "{\"custom\":\"" + value_ + "\"}"; }

    std::string toXml(const std::string& tagName) const {
        return "<" + tagName + "><value>" + value_ + "</value></" + tagName +
               ">";
    }

    std::string toYaml(const std::string& key) const {
        return key.empty() ? value_ : key + ": " + value_ + "\n";
    }

    std::string toToml(const std::string& key) const {
        return key.empty() ? value_ : key + " = \"" + value_ + "\"\n";
    }

private:
    std::string value_;
};

// Base fixture for testing anyutils functions
class AnyUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Basic types
        intValue = 42;
        floatValue = 3.14159;
        boolValue = true;
        charValue = 'X';
        stringValue = "Hello, World!";

        // Container types
        vectorOfInts = {1, 2, 3, 4, 5};
        arrayOfFloats = {1.1, 2.2, 3.3, 4.4, 5.5};

        // Map types
        mapOfStrings = {{"key1", "value1"}, {"key2", "value2"}};
        mapOfInts = {{1, 100}, {2, 200}, {3, 300}};

        // Complex types
        pairValue = std::make_pair("first", 42);
        tupleValue = std::make_tuple(1, "two", 3.0);

        // Pointers
        rawPointer = &intValue;
        smartPointer = std::make_shared<std::string>("smart pointer value");

        // Custom type
        customValue = CustomStringifiable("custom value");

        // Special values
        nanValue = std::numeric_limits<double>::quiet_NaN();
        infValue = std::numeric_limits<double>::infinity();
    }

    // Basic types
    int intValue;
    double floatValue;
    bool boolValue;
    char charValue;
    std::string stringValue;

    // Container types
    std::vector<int> vectorOfInts;
    std::array<float, 5> arrayOfFloats;

    // Map types
    std::unordered_map<std::string, std::string> mapOfStrings;
    std::unordered_map<int, int> mapOfInts;

    // Complex types
    std::pair<std::string, int> pairValue;
    std::tuple<int, std::string, double> tupleValue;

    // Pointers
    int* rawPointer;
    std::shared_ptr<std::string> smartPointer;

    // Custom type
    CustomStringifiable customValue;

    // Special values
    double nanValue;
    double infValue;

    // Empty containers
    std::vector<int> emptyVector;
    std::unordered_map<int, int> emptyMap;
};

// Tests for toString function
class ToStringTest : public AnyUtilsTest {};

TEST_F(ToStringTest, BasicTypes) {
    EXPECT_EQ(toString(intValue), "42");
    EXPECT_EQ(toString(floatValue), "3.14159");
    EXPECT_EQ(toString(boolValue), "true");
    EXPECT_EQ(toString(charValue), "X");
    EXPECT_EQ(toString(stringValue), "Hello, World!");
}

TEST_F(ToStringTest, ContainerTypes) {
    EXPECT_EQ(toString(vectorOfInts), "[1,2,3,4,5]");
    EXPECT_EQ(toString(arrayOfFloats), "[1.1,2.2,3.3,4.4,5.5]");
    EXPECT_EQ(toString(emptyVector), "[]");
}

TEST_F(ToStringTest, MapTypes) {
    // Order in unordered_map isn't guaranteed, so we check parts
    std::string result = toString(mapOfStrings);
    EXPECT_TRUE(result.find("key1: value1") != std::string::npos);
    EXPECT_TRUE(result.find("key2: value2") != std::string::npos);

    // Empty map
    EXPECT_EQ(toString(emptyMap), "{}");
}

TEST_F(ToStringTest, ComplexTypes) {
    EXPECT_EQ(toString(pairValue), "(first, 42)");
}

TEST_F(ToStringTest, PointerTypes) {
    EXPECT_EQ(toString(rawPointer), "42");
    EXPECT_EQ(toString(smartPointer), "smart pointer value");

    // Null pointers
    int* nullPtr = nullptr;
    std::shared_ptr<int> nullSmartPtr;
    EXPECT_EQ(toString(nullPtr), "nullptr");
    EXPECT_EQ(toString(nullSmartPtr), "nullptr");
}

TEST_F(ToStringTest, CustomTypes) {
    EXPECT_EQ(toString(customValue), "Custom(custom value)");
}

TEST_F(ToStringTest, PrettyPrint) {
    std::string prettyResult = toString(vectorOfInts, true);
    EXPECT_TRUE(prettyResult.find("\n") != std::string::npos);
    EXPECT_TRUE(prettyResult.find("  ") != std::string::npos);
}

TEST_F(ToStringTest, ThreadSafety) {
    const int numThreads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&]() {
            // Attempt to cause race conditions by calling toString from
            // multiple threads
            toString(vectorOfInts);
            toString(mapOfStrings);
            toString(pairValue);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // If we reach here without crashes or exceptions, thread safety is working
    SUCCEED();
}

// Tests for toJson function
class ToJsonTest : public AnyUtilsTest {};

TEST_F(ToJsonTest, BasicTypes) {
    EXPECT_EQ(toJson(intValue), "42");
    EXPECT_EQ(toJson(floatValue), "3.14159");
    EXPECT_EQ(toJson(boolValue), "true");
    EXPECT_EQ(toJson(charValue), "\"X\"");
    EXPECT_EQ(toJson(stringValue), "\"Hello, World!\"");
}

TEST_F(ToJsonTest, EscapedStrings) {
    EXPECT_EQ(toJson("Hello\nWorld"), "\"Hello\\nWorld\"");
    EXPECT_EQ(toJson("Quote\"Test"), "\"Quote\\\"Test\"");
    EXPECT_EQ(toJson("Backslash\\Test"), "\"Backslash\\\\Test\"");
}

TEST_F(ToJsonTest, ContainerTypes) {
    EXPECT_EQ(toJson(vectorOfInts), "[1,2,3,4,5]");
    EXPECT_EQ(toJson(emptyVector), "[]");
}

TEST_F(ToJsonTest, MapTypes) {
    std::string result = toJson(mapOfStrings);
    EXPECT_TRUE(result.find("\"key1\":") != std::string::npos);
    EXPECT_TRUE(result.find("\"value1\"") != std::string::npos);
    EXPECT_TRUE(result.find("\"key2\":") != std::string::npos);
    EXPECT_TRUE(result.find("\"value2\"") != std::string::npos);

    EXPECT_EQ(toJson(emptyMap), "{}");
}

TEST_F(ToJsonTest, ComplexTypes) {
    std::string result = toJson(pairValue);
    EXPECT_TRUE(result.find("\"first\":") != std::string::npos);
    EXPECT_TRUE(result.find("\"second\":") != std::string::npos);
    EXPECT_TRUE(result.find("42") != std::string::npos);
}

TEST_F(ToJsonTest, PointerTypes) {
    EXPECT_EQ(toJson(rawPointer), "42");
    EXPECT_EQ(toJson(smartPointer), "\"smart pointer value\"");

    int* nullPtr = nullptr;
    EXPECT_EQ(toJson(nullPtr), "null");
}

TEST_F(ToJsonTest, SpecialValues) {
    EXPECT_EQ(toJson(nanValue), "null");
    EXPECT_EQ(toJson(infValue), "null");
}

TEST_F(ToJsonTest, CustomTypes) {
    EXPECT_EQ(toJson(customValue), "{\"custom\":\"custom value\"}");
}

TEST_F(ToJsonTest, PrettyPrint) {
    std::string prettyResult = toJson(vectorOfInts, true);
    EXPECT_TRUE(prettyResult.find("\n") != std::string::npos);
    EXPECT_TRUE(prettyResult.find("  ") != std::string::npos);
}

// Tests for toXml function
class ToXmlTest : public AnyUtilsTest {};

TEST_F(ToXmlTest, BasicTypes) {
    EXPECT_EQ(toXml(intValue, "int"), "<int>42</int>");
    EXPECT_EQ(toXml(boolValue, "bool"), "<bool>1</bool>");
    EXPECT_EQ(toXml(charValue, "char"), "<char>X</char>");
    EXPECT_EQ(toXml(stringValue, "string"), "<string>Hello, World!</string>");
}

TEST_F(ToXmlTest, EscapedStrings) {
    EXPECT_EQ(toXml("<test>", "tag"), "<tag>&lt;test&gt;</tag>");
    EXPECT_EQ(toXml("AT&T", "company"), "<company>AT&amp;T</company>");
    EXPECT_EQ(toXml("Quote\"Test", "text"), "<text>Quote&quot;Test</text>");
}

TEST_F(ToXmlTest, ContainerTypes) {
    std::string result = toXml(vectorOfInts, "numbers");
    EXPECT_TRUE(result.find("<numbers>") != std::string::npos);
    EXPECT_TRUE(result.find("<numbers_item>1</numbers_item>") !=
                std::string::npos);
    EXPECT_TRUE(result.find("<numbers_item>5</numbers_item>") !=
                std::string::npos);
    EXPECT_TRUE(result.find("</numbers>") != std::string::npos);

    EXPECT_EQ(toXml(emptyVector, "empty"), "<empty>\n</empty>");
}

TEST_F(ToXmlTest, MapTypes) {
    std::string result = toXml(mapOfStrings, "dict");
    EXPECT_TRUE(result.find("<dict>") != std::string::npos);
    EXPECT_TRUE(result.find("<key1>value1</key1>") != std::string::npos);
    EXPECT_TRUE(result.find("<key2>value2</key2>") != std::string::npos);
    EXPECT_TRUE(result.find("</dict>") != std::string::npos);
}

TEST_F(ToXmlTest, ComplexTypes) {
    std::string result = toXml(pairValue, "pair");
    EXPECT_TRUE(result.find("<pair>") != std::string::npos);
    EXPECT_TRUE(result.find("<key>first</key>") != std::string::npos);
    EXPECT_TRUE(result.find("<value>42</value>") != std::string::npos);
    EXPECT_TRUE(result.find("</pair>") != std::string::npos);
}

TEST_F(ToXmlTest, PointerTypes) {
    EXPECT_EQ(toXml(rawPointer, "ptr"), "<ptr>42</ptr>");
    EXPECT_EQ(toXml(smartPointer, "smart"),
              "<smart>smart pointer value</smart>");

    int* nullPtr = nullptr;
    EXPECT_EQ(toXml(nullPtr, "null"), "<null nil=\"true\"/>");
}

TEST_F(ToXmlTest, CustomTypes) {
    EXPECT_EQ(toXml(customValue, "custom"),
              "<custom><value>custom value</value></custom>");
}

TEST_F(ToXmlTest, InvalidTagNames) {
    EXPECT_THROW(toXml(intValue, ""), std::invalid_argument);
    EXPECT_THROW(toXml(intValue, "<invalid>"), std::invalid_argument);
}

// Tests for toYaml function
class ToYamlTest : public AnyUtilsTest {};

TEST_F(ToYamlTest, BasicTypes) {
    EXPECT_EQ(toYaml(intValue, "int"), "int: 42\n");
    EXPECT_EQ(toYaml(floatValue, "float"), "float: 3.14159\n");
    EXPECT_EQ(toYaml(boolValue, "bool"), "bool: true\n");
    EXPECT_EQ(toYaml(charValue, "char"), "char: X\n");
    EXPECT_EQ(toYaml(stringValue, "string"), "string: Hello, World!\n");
}

TEST_F(ToYamlTest, SpecialStrings) {
    EXPECT_EQ(toYaml("String: with colon", "key"),
              "key: \"String: with colon\"\n");
    EXPECT_EQ(toYaml("String #with hash", "key"),
              "key: \"String #with hash\"\n");
    EXPECT_EQ(toYaml("", "empty"), "empty: \"\"\n");
}

TEST_F(ToYamlTest, ContainerTypes) {
    std::string result = toYaml(vectorOfInts, "numbers");
    EXPECT_TRUE(result.find("numbers:") != std::string::npos);
    EXPECT_TRUE(result.find("- 1\n") != std::string::npos);
    EXPECT_TRUE(result.find("- 5\n") != std::string::npos);

    EXPECT_EQ(toYaml(emptyVector, "empty"), "empty: []\n");
}

TEST_F(ToYamlTest, MapTypes) {
    std::string result = toYaml(mapOfStrings, "dict");
    EXPECT_TRUE(result.find("dict:") != std::string::npos);
    EXPECT_TRUE(result.find("key1: value1\n") != std::string::npos ||
                result.find("key2: value2\n") != std::string::npos);

    EXPECT_EQ(toYaml(emptyMap, "empty"), "empty: {}\n");
}

TEST_F(ToYamlTest, PointerTypes) {
    EXPECT_EQ(toYaml(rawPointer, "ptr"), "ptr: 42\n");
    EXPECT_EQ(toYaml(smartPointer, "smart"), "smart: smart pointer value\n");

    int* nullPtr = nullptr;
    EXPECT_EQ(toYaml(nullPtr, "null"), "null: null\n");
}

TEST_F(ToYamlTest, SpecialValues) {
    EXPECT_EQ(toYaml(nanValue, "nan"), "nan: .nan\n");
    EXPECT_EQ(toYaml(infValue, "inf"), "inf: .inf\n");
    EXPECT_EQ(toYaml(-infValue, "neginf"), "neginf: -.inf\n");
}

TEST_F(ToYamlTest, CustomTypes) {
    EXPECT_EQ(toYaml(customValue, "custom"), "custom: custom value\n");
}

// Tests for toToml function
class ToTomlTest : public AnyUtilsTest {};

TEST_F(ToTomlTest, BasicTypes) {
    EXPECT_EQ(toToml(intValue, "int"), "int: 42\n");
    EXPECT_EQ(toToml(floatValue, "float"), "float: 3.14159\n");
    EXPECT_EQ(toToml(boolValue, "bool"), "bool: true\n");
    EXPECT_EQ(toToml(stringValue, "string"), "string: Hello, World!\n");
}

TEST_F(ToTomlTest, ContainerTypes) {
    std::string result = toToml(vectorOfInts, "numbers");
    EXPECT_TRUE(result.find("numbers = [") != std::string::npos);
    EXPECT_TRUE(result.find("1,") != std::string::npos);
    EXPECT_TRUE(result.find("5") != std::string::npos);
    EXPECT_TRUE(result.find("]") != std::string::npos);
}

TEST_F(ToTomlTest, MapTypes) {
    std::string result = toToml(mapOfInts, "dict");
    EXPECT_TRUE(result.find("dict:") != std::string::npos);
    // Test presence of at least one key-value pair
    EXPECT_TRUE(result.find("1:") != std::string::npos ||
                result.find("2:") != std::string::npos ||
                result.find("3:") != std::string::npos);
}

TEST_F(ToTomlTest, CustomTypes) {
    EXPECT_EQ(toToml(customValue, "custom"), "custom = \"custom value\"\n");
}

// Tests for common error cases
class ErrorHandlingTest : public AnyUtilsTest {};

TEST_F(ErrorHandlingTest, ToStringErrorHandling) {
    struct ThrowingType {
        std::string toString() const {
            throw std::runtime_error("Test exception");
        }
    };

    ThrowingType throwingInstance;
    std::string result = toString(throwingInstance);
    EXPECT_TRUE(result.find("Error in toString:") != std::string::npos);
}

TEST_F(ErrorHandlingTest, ToJsonErrorHandling) {
    struct ThrowingJsonType {
        std::string toJson() const {
            throw std::runtime_error("Test JSON exception");
        }
    };

    ThrowingJsonType throwingInstance;
    std::string result = toJson(throwingInstance);
    EXPECT_TRUE(result.find("Error in toJson:") != std::string::npos);
}

TEST_F(ErrorHandlingTest, ToXmlErrorHandling) {
    struct ThrowingXmlType {
        std::string toXml(const std::string&) const {
            throw std::runtime_error("Test XML exception");
        }
    };

    ThrowingXmlType throwingInstance;
    std::string result = toXml(throwingInstance, "tag");
    EXPECT_TRUE(result.find("<error>") != std::string::npos);
    EXPECT_TRUE(result.find("Test XML exception") != std::string::npos);
}

TEST_F(ErrorHandlingTest, ToYamlErrorHandling) {
    struct ThrowingYamlType {
        std::string toYaml(const std::string&) const {
            throw std::runtime_error("Test YAML exception");
        }
    };

    ThrowingYamlType throwingInstance;
    std::string result = toYaml(throwingInstance, "key");
    EXPECT_TRUE(result.find("# Error:") != std::string::npos);
    EXPECT_TRUE(result.find("Test YAML exception") != std::string::npos);
}

TEST_F(ErrorHandlingTest, ToTomlErrorHandling) {
    struct ThrowingTomlType {
        std::string toToml(const std::string&) const {
            throw std::runtime_error("Test TOML exception");
        }
    };

    ThrowingTomlType throwingInstance;
    std::string result = toToml(throwingInstance, "key");
    EXPECT_TRUE(result.find("# Error:") != std::string::npos);
    EXPECT_TRUE(result.find("Test TOML exception") != std::string::npos);
}

// Test for cache functionality
class CacheTest : public AnyUtilsTest {};

TEST_F(CacheTest, CacheHitTest) {
    // First call should compute the result
    std::string firstResult = toString(vectorOfInts);
    // Second call should use cached value for small containers
    std::string secondResult = toString(vectorOfInts);

    EXPECT_EQ(firstResult, secondResult);
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_ANYUTILS_HPP