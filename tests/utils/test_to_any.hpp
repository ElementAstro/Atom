// filepath: /home/max/Atom-1/atom/utils/test_to_any.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <any>
#include <chrono>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "to_any.hpp"

// Mock for LOG_F to avoid actual logging during tests
#define LOG_F(level, ...) ((void)0)

using namespace atom::utils;
using ::testing::HasSubstr;

// Helper functions for checking any types
template <typename T>
bool anyContainsType(const std::any& value) {
    return value.type() == typeid(T);
}

template <typename T>
T anyGetValue(const std::any& value) {
    return std::any_cast<T>(value);
}

class ParserTest : public ::testing::Test {
protected:
    void SetUp() override { parser = std::make_unique<Parser>(); }

    void TearDown() override { parser.reset(); }

    std::unique_ptr<Parser> parser;
};

// Test basic functionality of parseLiteral with various input types
TEST_F(ParserTest, ParseLiteralBasicTypes) {
    // Test integer parsing
    auto result = parser->parseLiteral("42");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<int>(*result));
    EXPECT_EQ(anyGetValue<int>(*result), 42);

    // Test boolean parsing
    result = parser->parseLiteral("true");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<bool>(*result));
    EXPECT_EQ(anyGetValue<bool>(*result), true);

    result = parser->parseLiteral("false");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<bool>(*result));
    EXPECT_EQ(anyGetValue<bool>(*result), false);

    // Test string parsing
    result = parser->parseLiteral("hello world");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<std::string>(*result));
    EXPECT_EQ(anyGetValue<std::string>(*result), "hello world");

    // Test character parsing
    result = parser->parseLiteral("a");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<char>(*result));
    EXPECT_EQ(anyGetValue<char>(*result), 'a');

    // Test floating point parsing
    result = parser->parseLiteral("3.14");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<double>(*result));
    EXPECT_DOUBLE_EQ(anyGetValue<double>(*result), 3.14);
}

// Test parseLiteral with edge cases
TEST_F(ParserTest, ParseLiteralEdgeCases) {
    // Empty input should throw
    EXPECT_THROW(parser->parseLiteral(""), ParserException);

    // Whitespace only input
    auto result = parser->parseLiteral("   ");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<std::string>(*result));
    EXPECT_EQ(anyGetValue<std::string>(*result), "");

    // Integer with whitespace
    result = parser->parseLiteral("   42   ");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<int>(*result));
    EXPECT_EQ(anyGetValue<int>(*result), 42);

    // Boolean with whitespace
    result = parser->parseLiteral("  true  ");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<bool>(*result));
    EXPECT_EQ(anyGetValue<bool>(*result), true);

    // Mixed case boolean (should be treated as string)
    result = parser->parseLiteral("True");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<std::string>(*result));
    EXPECT_EQ(anyGetValue<std::string>(*result), "True");
}

// Test parseLiteral with numeric edge cases
TEST_F(ParserTest, ParseLiteralNumericEdgeCases) {
    // Large integers
    auto result = parser->parseLiteral("2147483647");  // INT_MAX
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<int>(*result));
    EXPECT_EQ(anyGetValue<int>(*result), 2147483647);

    result = parser->parseLiteral("2147483648");  // INT_MAX + 1
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<long>(*result) ||
                anyContainsType<long long>(*result));

    // Negative integers
    result = parser->parseLiteral("-42");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<int>(*result));
    EXPECT_EQ(anyGetValue<int>(*result), -42);

    // Floating point with exponent
    result = parser->parseLiteral("1.23e4");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<double>(*result));
    EXPECT_DOUBLE_EQ(anyGetValue<double>(*result), 12300.0);

    result = parser->parseLiteral("1.23E-4");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<double>(*result));
    EXPECT_DOUBLE_EQ(anyGetValue<double>(*result), 0.000123);

    // Large floating point
    result = parser->parseLiteral("1.7976931348623157e308");  // approx DBL_MAX
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<double>(*result));
}

// Test parseLiteral with invalid/malformed inputs
TEST_F(ParserTest, ParseLiteralInvalidInputs) {
    // Malformed number
    auto result = parser->parseLiteral("42abc");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<std::string>(*result));
    EXPECT_EQ(anyGetValue<std::string>(*result), "42abc");

    // Malformed floating point
    result = parser->parseLiteral("3.14.15");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<std::string>(*result));
    EXPECT_EQ(anyGetValue<std::string>(*result), "3.14.15");

    // Leading decimal point
    result = parser->parseLiteral(".123");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<double>(*result));
    EXPECT_DOUBLE_EQ(anyGetValue<double>(*result), 0.123);

    // Multiple decimal points
    result = parser->parseLiteral("1.2.3");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<std::string>(*result));
    EXPECT_EQ(anyGetValue<std::string>(*result), "1.2.3");

    // Malformed boolean
    result = parser->parseLiteral("truee");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<std::string>(*result));
    EXPECT_EQ(anyGetValue<std::string>(*result), "truee");
}

// Test datetime parsing
TEST_F(ParserTest, ParseLiteralDateTime) {
    // ISO format
    auto result = parser->parseLiteral("2023-01-01 12:30:45");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(
        anyContainsType<std::chrono::system_clock::time_point>(*result));

    // Alternative format
    result = parser->parseLiteral("2023/01/01 12:30:45");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(
        anyContainsType<std::chrono::system_clock::time_point>(*result));

    // Invalid date format
    result = parser->parseLiteral("2023-13-01 12:30:45");  // Invalid month
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<std::string>(*result));
    EXPECT_EQ(anyGetValue<std::string>(*result), "2023-13-01 12:30:45");
}

// Test parseLiteral with vectors
TEST_F(ParserTest, ParseLiteralVectors) {
    // Integer vector
    auto result = parser->parseLiteral("1,2,3,4,5");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<std::vector<int>>(*result));

    auto vec = anyGetValue<std::vector<int>>(*result);
    ASSERT_EQ(vec.size(), 5);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);
    EXPECT_EQ(vec[3], 4);
    EXPECT_EQ(vec[4], 5);

    // Empty vector
    result = parser->parseLiteral("");
    EXPECT_THROW(parser->parseLiteral(""), ParserException);

    // Mixed types in vector
    result = parser->parseLiteral("1,2,abc,4,5");
    EXPECT_FALSE(result.has_value());  // Should fail to parse as vector<int>
}

// Test parseLiteral with sets
TEST_F(ParserTest, ParseLiteralSets) {
    // Float set
    auto result = parser->parseLiteral("1.1,2.2,3.3,4.4,5.5");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<std::set<float>>(*result) ||
                anyContainsType<std::vector<float>>(*result));
}

// Test parseLiteral with maps
TEST_F(ParserTest, ParseLiteralMaps) {
    // String to int map
    auto result = parser->parseLiteral("key1:1,key2:2,key3:3");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE((anyContainsType<std::map<std::string, int>>(*result)));

    auto map = anyGetValue<std::map<std::string, int>>(*result);
    ASSERT_EQ(map.size(), 3);
    EXPECT_EQ(map["key1"], 1);
    EXPECT_EQ(map["key2"], 2);
    EXPECT_EQ(map["key3"], 3);

    // Malformed map
    result = parser->parseLiteral("key1:1,key2,key3:3");
    EXPECT_FALSE(result.has_value());  // Should fail to parse
}

// Test parseLiteralWithDefault
TEST_F(ParserTest, ParseLiteralWithDefault) {
    // Successful parse
    auto result = parser->parseLiteralWithDefault("42", std::string("default"));
    EXPECT_TRUE(anyContainsType<int>(result));
    EXPECT_EQ(anyGetValue<int>(result), 42);

    // Failed parse
    result = parser->parseLiteralWithDefault("", std::string("default"));
    EXPECT_TRUE(anyContainsType<std::string>(result));
    EXPECT_EQ(anyGetValue<std::string>(result), "default");

    // Edge case - empty but with spaces
    result = parser->parseLiteralWithDefault("   ", std::string("default"));
    EXPECT_TRUE(anyContainsType<std::string>(result));
    EXPECT_EQ(anyGetValue<std::string>(result), "");
}

// Test print method
TEST_F(ParserTest, Print) {
    // This is primarily a logging test, but we just make sure it doesn't crash
    std::any testValue = 42;
    EXPECT_NO_THROW(parser->print(testValue));

    testValue = std::string("test string");
    EXPECT_NO_THROW(parser->print(testValue));

    testValue = true;
    EXPECT_NO_THROW(parser->print(testValue));

    // Empty value
    testValue = std::any();
    EXPECT_NO_THROW(parser->print(testValue));
}

// Test logParsing method
TEST_F(ParserTest, LogParsing) {
    // Again, primarily logging test
    std::string input = "42";
    std::any result = 42;
    EXPECT_NO_THROW(parser->logParsing(input, result));

    // Complex type
    std::vector<int> vec = {1, 2, 3};
    std::any vecResult = vec;
    EXPECT_NO_THROW(parser->logParsing("vector input", vecResult));
}

// Test custom parser registration and use
TEST_F(ParserTest, CustomParser) {
    // Register a custom parser for "hex:" prefix
    parser->registerCustomParser(
        "hex:", [](std::string_view input) -> std::optional<std::any> {
            // Extract the hex part after "hex:"
            auto pos = input.find("hex:");
            if (pos == std::string_view::npos)
                return std::nullopt;

            std::string_view hexStr = input.substr(pos + 4);
            try {
                size_t processed;
                int value = std::stoi(std::string(hexStr), &processed, 16);
                if (processed == hexStr.size()) {
                    return value;
                }
            } catch (...) {
                // Conversion failed
            }
            return std::nullopt;
        });

    // Parse with the custom parser
    auto result = parser->parseLiteral("hex:1A");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<int>(*result));
    EXPECT_EQ(anyGetValue<int>(*result), 26);  // 0x1A = 26

    result = parser->parseLiteral("hex:FF");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<int>(*result));
    EXPECT_EQ(anyGetValue<int>(*result), 255);  // 0xFF = 255

    // Invalid hex should fall back to standard parsing
    result = parser->parseLiteral("hex:ZZ");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<std::string>(*result));
}

// Test custom parser with invalid parameters
TEST_F(ParserTest, CustomParserInvalidParams) {
    // Register with empty type
    EXPECT_THROW(
        parser->registerCustomParser(
            "", [](std::string_view) -> std::optional<std::any> { return 1; }),
        ParserException);

    // Register with null function
    Parser::CustomParserFunc nullFunc;
    EXPECT_THROW(parser->registerCustomParser("type", nullFunc),
                 ParserException);
}

// Test JSON parsing functionality
TEST_F(ParserTest, ParseJson) {
    std::string validJson = R"({
        "name": "John",
        "age": 30,
        "isEmployee": true,
        "address": {
            "street": "123 Main St",
            "city": "Anytown"
        }
    })";

    EXPECT_NO_THROW(parser->parseJson(validJson));

    // Empty JSON
    EXPECT_THROW(parser->parseJson(""), ParserException);

    // Invalid JSON
    std::string invalidJson = R"({
        "name": "John",
        "age": 30,
        "isEmployee": true,
        "address": {
            "street": "123 Main St",
            "city": "Anytown",
        }  // <- Extra comma
    })";

    EXPECT_THROW(parser->parseJson(invalidJson), ParserException);
}

// Test CSV parsing functionality
TEST_F(ParserTest, ParseCsv) {
    std::string validCsv = "name,age,city\nJohn,30,New York\nJane,25,Boston";
    EXPECT_NO_THROW(parser->parseCsv(validCsv));

    // Empty CSV
    EXPECT_THROW(parser->parseCsv(""), ParserException);

    // CSV with different delimiter
    std::string semicolonCsv =
        "name;age;city\nJohn;30;New York\nJane;25;Boston";
    EXPECT_NO_THROW(parser->parseCsv(semicolonCsv, ';'));
}

// Test printCustomParsers
TEST_F(ParserTest, PrintCustomParsers) {
    // Register a few custom parsers
    parser->registerCustomParser(
        "type1", [](std::string_view) -> std::optional<std::any> { return 1; });
    parser->registerCustomParser(
        "type2", [](std::string_view) -> std::optional<std::any> { return 2; });

    // Just check it doesn't throw
    EXPECT_NO_THROW(parser->printCustomParsers());
}

// Test parallel parsing
TEST_F(ParserTest, ParseParallel) {
    std::vector<std::string> inputs = {
        "42",       "3.14", "true", "hello world", "2023-01-01 12:30:45",
        "1,2,3,4,5"};

    auto results = parser->parseParallel(inputs);

    ASSERT_EQ(results.size(), inputs.size());
    EXPECT_TRUE(anyContainsType<int>(results[0]));
    EXPECT_EQ(anyGetValue<int>(results[0]), 42);

    EXPECT_TRUE(anyContainsType<double>(results[1]));
    EXPECT_DOUBLE_EQ(anyGetValue<double>(results[1]), 3.14);

    EXPECT_TRUE(anyContainsType<bool>(results[2]));
    EXPECT_EQ(anyGetValue<bool>(results[2]), true);

    EXPECT_TRUE(anyContainsType<std::string>(results[3]));
    EXPECT_EQ(anyGetValue<std::string>(results[3]), "hello world");

    EXPECT_TRUE(
        anyContainsType<std::chrono::system_clock::time_point>(results[4]));

    EXPECT_TRUE(anyContainsType<std::vector<int>>(results[5]));
    auto vec = anyGetValue<std::vector<int>>(results[5]);
    ASSERT_EQ(vec.size(), 5);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[4], 5);
}

// Test convertToAnyVector
TEST_F(ParserTest, ConvertToAnyVector) {
    std::vector<std::string_view> inputs = {"42", "3.14", "true",
                                            "hello world"};

    auto results = parser->convertToAnyVector(inputs);

    ASSERT_EQ(results.size(), inputs.size());
    EXPECT_TRUE(anyContainsType<int>(results[0]));
    EXPECT_EQ(anyGetValue<int>(results[0]), 42);

    EXPECT_TRUE(anyContainsType<double>(results[1]));
    EXPECT_DOUBLE_EQ(anyGetValue<double>(results[1]), 3.14);

    EXPECT_TRUE(anyContainsType<bool>(results[2]));
    EXPECT_EQ(anyGetValue<bool>(results[2]), true);

    EXPECT_TRUE(anyContainsType<std::string>(results[3]));
    EXPECT_EQ(anyGetValue<std::string>(results[3]), "hello world");
}

// Test thread safety
TEST_F(ParserTest, ThreadSafety) {
    // Create multiple threads that all parse through the same parser
    constexpr int numThreads = 10;
    constexpr int numParsesPerThread = 100;

    std::vector<std::thread> threads;
    std::atomic<int> successCount = 0;

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([this, &successCount]() {
            try {
                for (int j = 0; j < numParsesPerThread; j++) {
                    // Alternate between different parse types
                    if (j % 4 == 0) {
                        auto result = parser->parseLiteral("42");
                        if (result && anyContainsType<int>(*result)) {
                            successCount++;
                        }
                    } else if (j % 4 == 1) {
                        auto result = parser->parseLiteral("3.14");
                        if (result && anyContainsType<double>(*result)) {
                            successCount++;
                        }
                    } else if (j % 4 == 2) {
                        auto result = parser->parseLiteral("true");
                        if (result && anyContainsType<bool>(*result)) {
                            successCount++;
                        }
                    } else {
                        auto result = parser->parseLiteral("test string");
                        if (result && anyContainsType<std::string>(*result)) {
                            successCount++;
                        }
                    }
                }
            } catch (const std::exception& e) {
                // Ignore exceptions in threads
            }
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // If everything worked correctly, we should have all successes
    EXPECT_EQ(successCount, numThreads * numParsesPerThread);
}

// Test parseLiteral with special characters
TEST_F(ParserTest, ParseLiteralSpecialChars) {
    auto result = parser->parseLiteral("!@#$%");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<std::string>(*result));
    EXPECT_EQ(anyGetValue<std::string>(*result), "!@#$%");

    // Unicode characters
    result = parser->parseLiteral("こんにちは");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<std::string>(*result));
    EXPECT_EQ(anyGetValue<std::string>(*result), "こんにちは");
}

// Test concurrent calls to parseLiteral and other methods
TEST_F(ParserTest, ConcurrentParseLiteral) {
    std::thread t1([&]() {
        for (int i = 0; i < 100; i++) {
            try {
                parser->parseLiteral("42");
            } catch (...) {
                // Ignore exceptions due to concurrent access
            }
        }
    });

    std::thread t2([&]() {
        for (int i = 0; i < 100; i++) {
            try {
                parser->registerCustomParser(
                    "type" + std::to_string(i),
                    [](std::string_view) -> std::optional<std::any> {
                        return 1;
                    });
            } catch (...) {
                // Ignore exceptions due to concurrent access
            }
        }
    });

    std::thread t3([&]() {
        for (int i = 0; i < 100; i++) {
            try {
                parser->printCustomParsers();
            } catch (...) {
                // Ignore exceptions due to concurrent access
            }
        }
    });

    t1.join();
    t2.join();
    t3.join();

    // If we get here without deadlocks or crashes, the test passes
    SUCCEED();
}

// Test performance of parseLiteral with large inputs
TEST_F(ParserTest, ParseLiteralPerformance) {
    // Generate a long string
    std::string longString(10000, 'a');

    auto start = std::chrono::high_resolution_clock::now();
    auto result = parser->parseLiteral(longString);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Just make sure it completes and returns a value (no specific performance
    // assertion)
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(anyContainsType<std::string>(*result));

    // Log the time for information purposes
    std::cout << "Parsed 10,000 character string in " << duration.count()
              << "ms" << std::endl;
}

// Test the isProcessing flag to ensure it prevents concurrent calls
TEST_F(ParserTest, IsProcessingFlag) {
    // Start a thread that takes a long time to parse
    std::atomic<bool> threadStarted = false;
    std::thread t1([&]() {
        // Try to parse something that will take time
        threadStarted = true;
        try {
            // Generate a very long string
            std::string veryLongString(1000000, 'a');
            parser->parseLiteral(veryLongString);
        } catch (const ParserException&) {
            // Ignore, might be interrupted
        }
    });

    // Wait for thread to start
    while (!threadStarted) {
        std::this_thread::yield();
    }

    // Give it a moment to actually start the parsing
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Try to parse from main thread, should throw
    EXPECT_THROW(parser->parseLiteral("42"), ParserException);

    t1.join();
}
