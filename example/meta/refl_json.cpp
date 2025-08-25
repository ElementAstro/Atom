/**
 * @file refl_json.cpp
 * @brief Comprehensive example demonstrating JSON reflection capabilities
 *
 * This example shows how to:
 * - Use reflection for JSON serialization and deserialization
 * - Define field metadata with validation and defaults
 * - Handle required and optional fields
 * - Implement custom validators
 * - Work with nested objects and complex types
 * - Demonstrate error handling and validation
 *
 * @author Max Qian
 * @date 2024-12-19
 */

#include <iostream>
#include <optional>
#include <string>
#include <vector>


// Atom Meta JSON reflection headers
#include "atom/meta/refl_json.hpp"

using namespace atom::meta;

// Example structures for demonstration
struct Address {
    std::string street;
    std::string city;
    std::string zipCode;
    std::string country;
};

struct Person {
    std::string name;
    int age;
    std::string email;
    std::optional<Address> address;
    std::vector<std::string> hobbies;
    bool isActive;
};

struct Company {
    std::string name;
    std::string industry;
    int foundedYear;
    std::vector<Person> employees;
    Address headquarters;
};

// Custom validators
auto validateAge = [](const int& age) -> bool {
    return age >= 0 && age <= 150;
};

auto validateEmail = [](const std::string& email) -> bool {
    return email.find('@') != std::string::npos &&
           email.find('.') != std::string::npos;
};

auto validateZipCode = [](const std::string& zip) -> bool {
    return zip.length() >= 5 && zip.length() <= 10;
};

auto validateYear = [](const int& year) -> bool {
    return year >= 1800 && year <= 2030;
};

/**
 * @brief Demonstrates basic JSON reflection with simple structures
 */
void basicJsonReflectionExample() {
    std::cout << "\n=== Basic JSON Reflection Example ===\n";

    try {
        // Define reflection metadata for Address
        auto addressReflection = Reflectable<
            Address, Field<Address, std::string>, Field<Address, std::string>,
            Field<Address, std::string>, Field<Address, std::string>>(
            make_field<Address>("street", &Address::street, true,
                                std::string(""), nullptr),
            make_field<Address>("city", &Address::city, true, std::string(""),
                                nullptr),
            make_field<Address>("zipCode", &Address::zipCode, true,
                                std::string(""), validateZipCode),
            make_field<Address>("country", &Address::country, false,
                                std::string("USA"), nullptr));

        // Create an Address object
        Address addr{"123 Main St", "Anytown", "12345", "USA"};

        // Serialize to JSON
        json addressJson = addressReflection.to_json(addr);
        std::cout << "Address JSON:\n" << addressJson.dump(2) << "\n\n";

        // Deserialize from JSON
        json inputJson = json::parse(R"({
            "street": "456 Oak Ave",
            "city": "Springfield",
            "zipCode": "67890"
        })");

        Address deserializedAddr = addressReflection.from_json(inputJson);
        std::cout << "Deserialized Address:\n";
        std::cout << "  Street: " << deserializedAddr.street << "\n";
        std::cout << "  City: " << deserializedAddr.city << "\n";
        std::cout << "  Zip Code: " << deserializedAddr.zipCode << "\n";
        std::cout << "  Country: " << deserializedAddr.country
                  << " (default)\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic JSON reflection example: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates advanced JSON reflection with validation
 */
void advancedJsonReflectionExample() {
    std::cout << "\n=== Advanced JSON Reflection Example ===\n";

    try {
        // Define reflection metadata for Person with validation
        auto personReflection =
            Reflectable<Person, Field<Person, std::string>, Field<Person, int>,
                        Field<Person, std::string>, Field<Person, bool>>(
                make_field<Person>("name", &Person::name, true, std::string(""),
                                   nullptr),
                make_field<Person>("age", &Person::age, true, 0, validateAge),
                make_field<Person>("email", &Person::email, true,
                                   std::string(""), validateEmail),
                make_field<Person>("isActive", &Person::isActive, false, true,
                                   nullptr));

        // Test valid data
        std::cout << "Testing valid data:\n";
        json validJson = json::parse(R"({
            "name": "John Doe",
            "age": 30,
            "email": "john.doe@example.com",
            "isActive": true
        })");

        Person validPerson = personReflection.from_json(validJson);
        std::cout << "  Successfully created person: " << validPerson.name
                  << ", age " << validPerson.age << "\n";

        // Test serialization
        json serialized = personReflection.to_json(validPerson);
        std::cout << "  Serialized JSON:\n" << serialized.dump(2) << "\n";

        // Test invalid age
        std::cout << "\nTesting invalid age:\n";
        json invalidAgeJson = json::parse(R"({
            "name": "Invalid Person",
            "age": 200,
            "email": "invalid@example.com"
        })");

        try {
            Person invalidPerson = personReflection.from_json(invalidAgeJson);
            std::cout << "  ERROR: Should have failed validation!\n";
        } catch (const std::exception& e) {
            std::cout << "  Correctly caught validation error: " << e.what()
                      << "\n";
        }

        // Test invalid email
        std::cout << "\nTesting invalid email:\n";
        json invalidEmailJson = json::parse(R"({
            "name": "Another Invalid Person",
            "age": 25,
            "email": "not-an-email"
        })");

        try {
            Person invalidPerson = personReflection.from_json(invalidEmailJson);
            std::cout << "  ERROR: Should have failed validation!\n";
        } catch (const std::exception& e) {
            std::cout << "  Correctly caught validation error: " << e.what()
                      << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in advanced JSON reflection example: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates missing required fields handling
 */
void missingFieldsExample() {
    std::cout << "\n=== Missing Required Fields Example ===\n";

    try {
        // Define reflection metadata with required fields
        auto personReflection =
            Reflectable<Person, Field<Person, std::string>, Field<Person, int>,
                        Field<Person, std::string>, Field<Person, bool>>(
                make_field<Person>("name", &Person::name, true, std::string(""),
                                   nullptr),
                make_field<Person>("age", &Person::age, true, 0, validateAge),
                make_field<Person>("email", &Person::email, true,
                                   std::string(""), validateEmail),
                make_field<Person>("isActive", &Person::isActive, false, true,
                                   nullptr));

        // Test missing required field
        std::cout << "Testing missing required field (name):\n";
        json missingNameJson = json::parse(R"({
            "age": 25,
            "email": "test@example.com"
        })");

        try {
            Person person = personReflection.from_json(missingNameJson);
            std::cout << "  ERROR: Should have failed due to missing required "
                         "field!\n";
        } catch (const std::exception& e) {
            std::cout << "  Correctly caught missing field error: " << e.what()
                      << "\n";
        }

        // Test with all required fields present
        std::cout << "\nTesting with all required fields:\n";
        json completeJson = json::parse(R"({
            "name": "Complete Person",
            "age": 25,
            "email": "complete@example.com"
        })");

        Person completePerson = personReflection.from_json(completeJson);
        std::cout << "  Successfully created person: " << completePerson.name
                  << "\n";
        std::cout << "  isActive (default): "
                  << (completePerson.isActive ? "true" : "false") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in missing fields example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates default values and optional fields
 */
void defaultValuesExample() {
    std::cout << "\n=== Default Values Example ===\n";

    try {
        // Define reflection metadata with various default values
        auto addressReflection = Reflectable<
            Address, Field<Address, std::string>, Field<Address, std::string>,
            Field<Address, std::string>, Field<Address, std::string>>(
            make_field<Address>("street", &Address::street, true,
                                std::string(""), nullptr),
            make_field<Address>("city", &Address::city, true, std::string(""),
                                nullptr),
            make_field<Address>("zipCode", &Address::zipCode, false,
                                std::string("00000"), validateZipCode),
            make_field<Address>("country", &Address::country, false,
                                std::string("USA"), nullptr));

        // Test with minimal data (using defaults)
        std::cout << "Testing with minimal data (using defaults):\n";
        json minimalJson = json::parse(R"({
            "street": "123 Test St",
            "city": "Test City"
        })");

        Address addr = addressReflection.from_json(minimalJson);
        std::cout << "  Street: " << addr.street << "\n";
        std::cout << "  City: " << addr.city << "\n";
        std::cout << "  Zip Code: " << addr.zipCode << " (default)\n";
        std::cout << "  Country: " << addr.country << " (default)\n";

        // Test overriding defaults
        std::cout << "\nTesting with overridden defaults:\n";
        json overrideJson = json::parse(R"({
            "street": "456 Override Ave",
            "city": "Override City",
            "zipCode": "12345",
            "country": "Canada"
        })");

        Address overrideAddr = addressReflection.from_json(overrideJson);
        std::cout << "  Street: " << overrideAddr.street << "\n";
        std::cout << "  City: " << overrideAddr.city << "\n";
        std::cout << "  Zip Code: " << overrideAddr.zipCode
                  << " (overridden)\n";
        std::cout << "  Country: " << overrideAddr.country << " (overridden)\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in default values example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates round-trip serialization/deserialization
 */
void roundTripExample() {
    std::cout << "\n=== Round-Trip Serialization Example ===\n";

    try {
        // Define reflection metadata
        auto personReflection =
            Reflectable<Person, Field<Person, std::string>, Field<Person, int>,
                        Field<Person, std::string>, Field<Person, bool>>(
                make_field<Person>("name", &Person::name, true, std::string(""),
                                   nullptr),
                make_field<Person>("age", &Person::age, true, 0, validateAge),
                make_field<Person>("email", &Person::email, true,
                                   std::string(""), validateEmail),
                make_field<Person>("isActive", &Person::isActive, false, true,
                                   nullptr));

        // Create original object
        Person original;
        original.name = "Round Trip Test";
        original.age = 35;
        original.email = "roundtrip@example.com";
        original.isActive = false;

        std::cout << "Original object:\n";
        std::cout << "  Name: " << original.name << "\n";
        std::cout << "  Age: " << original.age << "\n";
        std::cout << "  Email: " << original.email << "\n";
        std::cout << "  Active: " << (original.isActive ? "true" : "false")
                  << "\n";

        // Serialize to JSON
        json serialized = personReflection.to_json(original);
        std::cout << "\nSerialized JSON:\n" << serialized.dump(2) << "\n";

        // Deserialize back to object
        Person deserialized = personReflection.from_json(serialized);
        std::cout << "\nDeserialized object:\n";
        std::cout << "  Name: " << deserialized.name << "\n";
        std::cout << "  Age: " << deserialized.age << "\n";
        std::cout << "  Email: " << deserialized.email << "\n";
        std::cout << "  Active: " << (deserialized.isActive ? "true" : "false")
                  << "\n";

        // Verify round-trip integrity
        bool roundTripSuccess = (original.name == deserialized.name &&
                                 original.age == deserialized.age &&
                                 original.email == deserialized.email &&
                                 original.isActive == deserialized.isActive);

        std::cout << "\nRound-trip integrity: "
                  << (roundTripSuccess ? "PASSED" : "FAILED") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in round-trip example: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating all JSON reflection capabilities
 */
int main() {
    std::cout << "================================================\n";
    std::cout << "  Atom Meta JSON Reflection Examples\n";
    std::cout << "================================================\n";

    try {
        basicJsonReflectionExample();
        advancedJsonReflectionExample();
        missingFieldsExample();
        defaultValuesExample();
        roundTripExample();

        std::cout << "\n=== All JSON Reflection Examples Completed "
                     "Successfully ===\n";
        std::cout << "The JSON reflection system provides:\n";
        std::cout << "  ✓ Automatic JSON serialization/deserialization\n";
        std::cout << "  ✓ Field validation with custom validators\n";
        std::cout << "  ✓ Required and optional field handling\n";
        std::cout << "  ✓ Default value support\n";
        std::cout << "  ✓ Type-safe field mapping\n";
        std::cout << "  ✓ Comprehensive error handling\n";
        std::cout << "  ✓ Round-trip serialization integrity\n";

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
