#include "atom/utils/to_any.hpp"

#include <any>
#include <optional>
#include <string>
#include <vector>

using namespace atom::utils;

int main() {
    // Create a Parser instance
    Parser parser;

    // Parse a literal string into an std::any type
    std::optional<std::any> parsedValue = parser.parseLiteral("123");
    if (parsedValue) {
        parser.print(*parsedValue);
    }

    // Parse a literal string into an std::any type with a default value
    std::any defaultValue = 456;
    std::any parsedWithDefault =
        parser.parseLiteralWithDefault("abc", defaultValue);
    parser.print(parsedWithDefault);

    // Log the parsing result
    parser.logParsing("123", *parsedValue);

    // Convert a vector of strings to a vector of std::any types
    std::vector<std::string> stringVector = {"1", "2", "3"};
    std::vector<std::any> anyVector = parser.convertToAnyVector(stringVector);
    for (const auto& value : anyVector) {
        parser.print(value);
    }

    // Register a custom parser for a specific type
    parser.registerCustomParser(
        "customType", [](const std::string& input) -> std::optional<std::any> {
            if (input == "custom") {
                return std::make_any<std::string>("Custom Parsed Value");
            }
            return std::nullopt;
        });

    // Print the registered custom parsers
    parser.printCustomParsers();

    // Parse a JSON string
    std::string jsonString = R"({"key": "value"})";
    parser.parseJson(jsonString);

    // Parse a CSV string
    std::string csvString = "name,age\nJohn,30\nJane,25";
    parser.parseCsv(csvString);

    return 0;
}