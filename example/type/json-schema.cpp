#include "atom/type/json-schema.hpp"

#include <iostream>

using namespace atom::type;

int main() {
    // Define a JSON schema
    json schema = {
        {"type", "object"},
        {"properties",
         {
             {"name", {{"type", "string"}}},
             {"age", {{"type", "integer"}, {"minimum", 0}}},
             {"email", {{"type", "string"}, {"pattern", R"(^\S+@\S+\.\S+$)"}}},
             {"tags", {{"type", "array"}, {"items", {{"type", "string"}}}}},
         }},
        {"required", {"name", "age"}}};

    // Create a JsonValidator object and set the root schema
    JsonValidator validator;
    validator.setRootSchema(schema);

    // Define a JSON instance that conforms to the schema
    json validInstance = {{"name", "John Doe"},
                          {"age", 30},
                          {"email", "john.doe@example.com"},
                          {"tags", {"developer", "blogger"}}};

    // Validate the JSON instance
    bool isValid = validator.validate(validInstance);
    std::cout << "Valid instance is valid: " << std::boolalpha << isValid
              << std::endl;

    // Define a JSON instance that does not conform to the schema
    json invalidInstance = {{"name", "John Doe"},
                            {"age", -5},
                            {"email", "john.doe@example"},
                            {"tags", {"developer", 123}}};

    // Validate the JSON instance
    bool isInvalid = validator.validate(invalidInstance);
    std::cout << "Invalid instance is valid: " << std::boolalpha << isInvalid
              << std::endl;

    // Get and print validation errors
    const auto& errors = validator.getErrors();
    std::cout << "Validation errors:" << std::endl;
    for (const auto& error : errors) {
        std::cout << "Error: " << error.message << ", Path: " << error.path
                  << std::endl;
    }

    return 0;
}