#include <iostream>
#include <string>
#include <vector>

#include "atom/type/rjson.hpp"

// Helper function to print section headersvoid print_header(const std::string&
// title) {
std::cout << "\n=== " << title << " ===" << std::endl;
std::cout << std::string(title.length() + 8, '=') << std::endl;
}

// Helper function to print JSON typestd::string
// type_to_string(atom::type::JsonValue::Type type) {
switch (type) {
    case atom::type::JsonValue::Type::Null:
        return "Null";
    case atom::type::JsonValue::Type::String:
        return "String";
    case atom::type::JsonValue::Type::Number:
        return "Number";
    case atom::type::JsonValue::Type::Bool:
        return "Bool";
    case atom::type::JsonValue::Type::Object:
        return "Object";
    case atom::type::JsonValue::Type::Array:
        return "Array";
    default:
        return "Unknown";
}
}

// Helper function to print JsonValue recursivelyvoid print_json_value(const
// atom::type::JsonValue& value, int indent = 0) {
std::string spaces(indent * 2, ' ');

switch (value.type()) {
    case atom::type::JsonValue::Type::Null:
        std::cout << spaces << "null";
        break;
    case atom::type::JsonValue::Type::String:
        std::cout << spaces << "\"" << value.asString() << "\"";
        break;
    case atom::type::JsonValue::Type::Number:
        std::cout << spaces << value.asNumber();
        break;
    case atom::type::JsonValue::Type::Bool:
        std::cout << spaces << (value.asBool() ? "true" : "false");
        break;
    case atom::type::JsonValue::Type::Object: {
        std::cout << spaces << "{" << std::endl;
        const auto& obj = value.asObject();
        bool first = true;
        for (const auto& [key, val] : obj) {
            if (!first)
                std::cout << "," << std::endl;
            std::cout << spaces << "  \"" << key << "\": ";
            if (val.type() == atom::type::JsonValue::Type::Object ||
                val.type() == atom::type::JsonValue::Type::Array) {
                std::cout << std::endl;
                print_json_value(val, indent + 2);
            } else {
                print_json_value(val, 0);
            }
            first = false;
        }
        std::cout << std::endl << spaces << "}";
        break;
    }
    case atom::type::JsonValue::Type::Array: {
        std::cout << spaces << "[" << std::endl;
        const auto& arr = value.asArray();
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0)
                std::cout << "," << std::endl;
            print_json_value(arr[i], indent + 1);
        }
        std::cout << std::endl << spaces << "]";
        break;
    }
}
}

int main() {
    std::cout << "RJSON (Rapid JSON) Usage Examples" << std::endl;
    std::cout << "==================================" << std::endl;

    // 1. Basic Value Creation
    print_header("Basic Value Creation");

    // Create different types of JSON values
    atom::type::JsonValue null_value;
    atom::type::JsonValue string_value("Hello, World!");
    atom::type::JsonValue number_value(42.5);
    atom::type::JsonValue bool_value(true);

    std::cout << "Created basic JSON values:" << std::endl;
    std::cout << "Null value type: " << type_to_string(null_value.type())
              << std::endl;
    std::cout << "String value: \"" << string_value.asString()
              << "\" (type: " << type_to_string(string_value.type()) << ")"
              << std::endl;
    std::cout << "Number value: " << number_value.asNumber()
              << " (type: " << type_to_string(number_value.type()) << ")"
              << std::endl;
    std::cout << "Bool value: " << (bool_value.asBool() ? "true" : "false")
              << " (type: " << type_to_string(bool_value.type()) << ")"
              << std::endl;

    // 2. Object Creation and Manipulation
    print_header("Object Creation and Manipulation");

    atom::type::JsonObject person_obj;
    person_obj["name"] = atom::type::JsonValue("John Doe");
    person_obj["age"] = atom::type::JsonValue(30.0);
    person_obj["is_employed"] = atom::type::JsonValue(true);
    person_obj["spouse"] = atom::type::JsonValue();  // null value

    atom::type::JsonValue person(person_obj);

    std::cout << "Created person object:" << std::endl;
    print_json_value(person);
    std::cout << std::endl;

    // Access object properties
    std::cout << "\nAccessing object properties:" << std::endl;
    const auto& obj = person.asObject();
    std::cout << "Name: " << obj.at("name").asString() << std::endl;
    std::cout << "Age: " << obj.at("age").asNumber() << std::endl;
    std::cout << "Employed: " << (obj.at("is_employed").asBool() ? "Yes" : "No")
              << std::endl;
    std::cout << "Spouse: " << type_to_string(obj.at("spouse").type())
              << std::endl;

    // 3. Array Creation and Manipulation
    print_header("Array Creation and Manipulation");

    atom::type::JsonArray numbers_array;
    numbers_array.push_back(atom::type::JsonValue(1.0));
    numbers_array.push_back(atom::type::JsonValue(2.0));
    numbers_array.push_back(atom::type::JsonValue(3.0));
    numbers_array.push_back(atom::type::JsonValue(4.0));
    numbers_array.push_back(atom::type::JsonValue(5.0));

    atom::type::JsonValue numbers(numbers_array);

    std::cout << "Created numbers array:" << std::endl;
    print_json_value(numbers);
    std::cout << std::endl;

    // Mixed array
    atom::type::JsonArray mixed_array;
    mixed_array.push_back(atom::type::JsonValue("text"));
    mixed_array.push_back(atom::type::JsonValue(42.0));
    mixed_array.push_back(atom::type::JsonValue(true));
    mixed_array.push_back(atom::type::JsonValue());  // null

    atom::type::JsonValue mixed(mixed_array);

    std::cout << "\nCreated mixed array:" << std::endl;
    print_json_value(mixed);
    std::cout << std::endl;

    // 4. Nested Structures
    print_header("Nested Structures");

    // Create a complex nested structure
    atom::type::JsonObject address_obj;
    address_obj["street"] = atom::type::JsonValue("123 Main St");
    address_obj["city"] = atom::type::JsonValue("Anytown");
    address_obj["zip"] = atom::type::JsonValue(12345.0);

    atom::type::JsonArray hobbies_array;
    hobbies_array.push_back(atom::type::JsonValue("reading"));
    hobbies_array.push_back(atom::type::JsonValue("swimming"));
    hobbies_array.push_back(atom::type::JsonValue("coding"));

    atom::type::JsonObject complex_person;
    complex_person["name"] = atom::type::JsonValue("Alice Johnson");
    complex_person["age"] = atom::type::JsonValue(28.0);
    complex_person["address"] = atom::type::JsonValue(address_obj);
    complex_person["hobbies"] = atom::type::JsonValue(hobbies_array);

    atom::type::JsonValue complex_json(complex_person);

    std::cout << "Created complex nested structure:" << std::endl;
    print_json_value(complex_json);
    std::cout << std::endl;

    // 5. JSON Parsing
    print_header("JSON Parsing");

    // Simple JSON strings
    std::string simple_json = R"({"name": "Bob", "age": 25, "active": true})";
    std::string array_json = R"([1, 2, 3, "hello", null, false])";
    std::string nested_json = R"({
        "user": {
            "id": 123,
            "profile": {
                "name": "Charlie",
                "settings": [true, false, null]
            }
        }
    })";

    try {
        std::cout << "Parsing simple JSON:" << std::endl;
        auto parsed_simple = atom::type::JsonParser::parse(simple_json);
        print_json_value(parsed_simple);
        std::cout << std::endl;

        std::cout << "\nParsing array JSON:" << std::endl;
        auto parsed_array = atom::type::JsonParser::parse(array_json);
        print_json_value(parsed_array);
        std::cout << std::endl;

        std::cout << "\nParsing nested JSON:" << std::endl;
        auto parsed_nested = atom::type::JsonParser::parse(nested_json);
        print_json_value(parsed_nested);
        std::cout << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Parsing error: " << e.what() << std::endl;
    }

    // 6. Error Handling
    print_header("Error Handling");

    std::cout << "Testing error handling:" << std::endl;

    // Type mismatch errors
    try {
        std::cout << "Trying to access string as number: ";
        double wrong = string_value.asNumber();
        std::cout << wrong << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ Caught expected error: " << e.what() << std::endl;
    }

    try {
        std::cout << "Trying to access number as object: ";
        const auto& wrong = number_value.asObject();
        std::cout << "Size: " << wrong.size() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ Caught expected error: " << e.what() << std::endl;
    }

    // Invalid JSON parsing
    try {
        std::cout << "Parsing invalid JSON: ";
        std::string invalid_json = R"({"name": "test", "age": })";
        auto parsed = atom::type::JsonParser::parse(invalid_json);
        std::cout << "Unexpectedly succeeded" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ Caught expected parsing error: " << e.what()
                  << std::endl;
    }

    std::cout << "\nAll RJSON examples completed successfully!" << std::endl;
    return 0;
}
