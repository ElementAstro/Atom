#include "atom/type/args.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace atom;

int main() {
    // Create an Args container
    Args args;

    // Set values for different keys
    args.set("name", std::string("test"));
    args.set("count", 42);
    args.set("pi", 3.14);

    // Get values by key
    std::string name = args.get<std::string>("name");
    int count = args.get<int>("count");
    double pi = args.get<double>("pi");

    std::cout << "Name: " << name << std::endl;
    std::cout << "Count: " << count << std::endl;
    std::cout << "Pi: " << pi << std::endl;

    // Get value by key with default
    int missingValue = args.getOr("missing", 0);
    std::cout << "Missing value (default): " << missingValue << std::endl;

    // Get multiple values by keys
    std::vector<std::string_view> keys = {"name", "count", "missing"};
    auto values = args.get<std::string>(keys);
    for (const auto& value : values) {
        if (value) {
            std::cout << "Value: " << *value << std::endl;
        } else {
            std::cout << "Value: nullopt" << std::endl;
        }
    }

    // Check if a key exists
    bool hasName = args.contains("name");
    std::cout << "Contains 'name': " << std::boolalpha << hasName << std::endl;

    // Remove a key-value pair
    args.remove("name");
    bool hasNameAfterRemove = args.contains("name");
    std::cout << "Contains 'name' after remove: " << std::boolalpha
              << hasNameAfterRemove << std::endl;

    // Clear all key-value pairs
    args.clear();
    std::cout << "Size after clear: " << args.size() << std::endl;

    // Use macros for setting, getting, checking, and removing arguments
    SET_ARGUMENT(args, age, 25);
    int age = GET_ARGUMENT(args, age, int);
    std::cout << "Age: " << age << std::endl;

    bool hasAge = HAS_ARGUMENT(args, age);
    std::cout << "Contains 'age': " << std::boolalpha << hasAge << std::endl;

    REMOVE_ARGUMENT(args, age);
    bool hasAgeAfterRemove = HAS_ARGUMENT(args, age);
    std::cout << "Contains 'age' after remove: " << std::boolalpha
              << hasAgeAfterRemove << std::endl;

    return 0;
}