#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "atom/type/json.hpp"

using json = nlohmann::json;

// Helper function to print section headersvoid print_header(const std::string&
// title) {
std::cout << "\n=== " << title << " ===" << std::endl;
std::cout << std::string(title.length() + 8, '=') << std::endl;
}

// Custom struct for serialization examplestruct Person {
std::string name;
int age;
std::string email;
std::vector<std::string> hobbies;
}
;

// JSON serialization for Personvoid to_json(json& j, const Person& p) {
j = json{{"name", p.name},
         {"age", p.age},
         {"email", p.email},
         {"hobbies", p.hobbies}};
}

// JSON deserialization for Personvoid from_json(const json& j, Person& p) {
j.at("name").get_to(p.name);
j.at("age").get_to(p.age);
j.at("email").get_to(p.email);
j.at("hobbies").get_to(p.hobbies);
}

int main() {
    std::cout << "Nlohmann JSON Library Usage Examples" << std::endl;
    std::cout << "====================================" << std::endl;

    // 1. Basic JSON Creation and Types
    print_header("Basic JSON Creation and Types");

    // Create different JSON types
    json null_value = nullptr;
    json bool_value = true;
    json number_int = 42;
    json number_float = 3.14159;
    json string_value = "Hello, JSON!";
    json array_value = {1, 2, 3, 4, 5};
    json object_value = {{"name", "John"}, {"age", 30}, {"city", "New York"}};

    std::cout << "Created different JSON types:" << std::endl;
    std::cout << "Null: " << null_value << " (type: " << null_value.type_name()
              << ")" << std::endl;
    std::cout << "Bool: " << bool_value << " (type: " << bool_value.type_name()
              << ")" << std::endl;
    std::cout << "Integer: " << number_int
              << " (type: " << number_int.type_name() << ")" << std::endl;
    std::cout << "Float: " << number_float
              << " (type: " << number_float.type_name() << ")" << std::endl;
    std::cout << "String: " << string_value
              << " (type: " << string_value.type_name() << ")" << std::endl;
    std::cout << "Array: " << array_value
              << " (type: " << array_value.type_name() << ")" << std::endl;
    std::cout << "Object: " << object_value.dump(2)
              << " (type: " << object_value.type_name() << ")" << std::endl;

    // 2. Object Manipulation
    print_header("Object Manipulation");

    json person = {
        {"name", "Alice Johnson"},
        {"age", 28},
        {"email", "alice@example.com"},
        {"address",
         {{"street", "123 Main St"}, {"city", "Boston"}, {"zip", "02101"}}},
        {"hobbies", {"reading", "swimming", "coding"}}};

    std::cout << "Created person object:" << std::endl;
    std::cout << person.dump(2) << std::endl;

    // Access and modify values
    std::cout << "\nAccessing values:" << std::endl;
    std::cout << "Name: " << person["name"] << std::endl;
    std::cout << "Age: " << person["age"] << std::endl;
    std::cout << "City: " << person["address"]["city"] << std::endl;
    std::cout << "First hobby: " << person["hobbies"][0] << std::endl;

    // Modify values
    person["age"] = 29;
    person["address"]["city"] = "Cambridge";
    person["hobbies"].push_back("photography");

    std::cout << "\nAfter modifications:" << std::endl;
    std::cout << "Age: " << person["age"] << std::endl;
    std::cout << "City: " << person["address"]["city"] << std::endl;
    std::cout << "Hobbies: " << person["hobbies"] << std::endl;

    // 3. Array Operations
    print_header("Array Operations");

    json numbers = {10, 20, 30, 40, 50};
    std::cout << "Original array: " << numbers << std::endl;

    // Add elements
    numbers.push_back(60);
    numbers.insert(numbers.begin(), 0);

    std::cout << "After adding elements: " << numbers << std::endl;
    std::cout << "Array size: " << numbers.size() << std::endl;

    // Iterate through array
    std::cout << "Iterating through array: ";
    for (const auto& num : numbers) {
        std::cout << num << " ";
    }
    std::cout << std::endl;

    // Remove elements
    numbers.erase(numbers.begin());  // Remove first element
    numbers.pop_back();              // Remove last element

    std::cout << "After removing elements: " << numbers << std::endl;

    // 4. JSON Parsing and Serialization
    print_header("JSON Parsing and Serialization");

    // Parse from string
    std::string json_string = R"({
        "product": "Laptop",
        "price": 999.99,
        "in_stock": true,
        "specifications": {
            "cpu": "Intel i7",
            "ram": "16GB",
            "storage": "512GB SSD"
        },
        "reviews": [
            {"rating": 5, "comment": "Excellent!"},
            {"rating": 4, "comment": "Good value"}
        ]
    })";

    try {
        json product = json::parse(json_string);
        std::cout << "Parsed JSON:" << std::endl;
        std::cout << product.dump(2) << std::endl;

        // Access nested values
        std::cout << "\nProduct details:" << std::endl;
        std::cout << "Name: " << product["product"] << std::endl;
        std::cout << "Price: $" << product["price"] << std::endl;
        std::cout << "CPU: " << product["specifications"]["cpu"] << std::endl;
        std::cout << "Average rating: "
                  << (product["reviews"][0]["rating"].get<int>() +
                      product["reviews"][1]["rating"].get<int>()) /
                         2.0
                  << std::endl;

    } catch (const json::parse_error& e) {
        std::cout << "Parse error: " << e.what() << std::endl;
    }

    // 5. Custom Object Serialization
    print_header("Custom Object Serialization");

    // Create Person objects
    Person person1{"Bob Smith", 35, "bob@example.com", {"hiking", "cooking"}};
    Person person2{"Carol Davis",
                   42,
                   "carol@example.com",
                   {"painting", "gardening", "yoga"}};

    // Convert to JSON
    json json_person1 = person1;
    json json_person2 = person2;

    std::cout << "Person 1 as JSON:" << std::endl;
    std::cout << json_person1.dump(2) << std::endl;

    std::cout << "\nPerson 2 as JSON:" << std::endl;
    std::cout << json_person2.dump(2) << std::endl;

    // Create array of people
    json people_array = {person1, person2};
    std::cout << "\nArray of people:" << std::endl;
    std::cout << people_array.dump(2) << std::endl;

    // Convert back from JSON
    Person restored_person = json_person1.get<Person>();
    std::cout << "\nRestored person:" << std::endl;
    std::cout << "Name: " << restored_person.name << std::endl;
    std::cout << "Age: " << restored_person.age << std::endl;
    std::cout << "Email: " << restored_person.email << std::endl;
    std::cout << "Hobbies: ";
    for (const auto& hobby : restored_person.hobbies) {
        std::cout << hobby << " ";
    }
    std::cout << std::endl;

    // 6. JSON Pointer and Patch
    print_header("JSON Pointer and Patch");

    json data = {{"users",
                  {{{"id", 1}, {"name", "Alice"}, {"active", true}},
                   {{"id", 2}, {"name", "Bob"}, {"active", false}},
                   {{"id", 3}, {"name", "Charlie"}, {"active", true}}}},
                 {"settings", {{"theme", "dark"}, {"notifications", true}}}};

    std::cout << "Original data:" << std::endl;
    std::cout << data.dump(2) << std::endl;

    // Use JSON pointers to access nested data
    std::cout << "\nUsing JSON pointers:" << std::endl;
    std::cout << "First user name: " << data["/users/0/name"_json_pointer]
              << std::endl;
    std::cout << "Theme setting: " << data["/settings/theme"_json_pointer]
              << std::endl;

    // Modify using JSON pointer
    data["/users/1/active"_json_pointer] = true;
    data["/settings/theme"_json_pointer] = "light";

    std::cout << "\nAfter modifications:" << std::endl;
    std::cout << "Bob's status: " << data["/users/1/active"_json_pointer]
              << std::endl;
    std::cout << "New theme: " << data["/settings/theme"_json_pointer]
              << std::endl;

    // 7. Error Handling and Type Checking
    print_header("Error Handling and Type Checking");

    json test_data = {{"string_field", "hello"},
                      {"number_field", 42},
                      {"bool_field", true},
                      {"null_field", nullptr}};

    std::cout << "Testing type checks and safe access:" << std::endl;

    // Type checking
    std::cout << "string_field is string: "
              << test_data["string_field"].is_string() << std::endl;
    std::cout << "number_field is number: "
              << test_data["number_field"].is_number() << std::endl;
    std::cout << "bool_field is boolean: "
              << test_data["bool_field"].is_boolean() << std::endl;
    std::cout << "null_field is null: " << test_data["null_field"].is_null()
              << std::endl;

    // Safe access with error handling
    try {
        std::string str_val = test_data["string_field"].get<std::string>();
        std::cout << "String value: " << str_val << std::endl;
    } catch (const json::type_error& e) {
        std::cout << "Type error: " << e.what() << std::endl;
    }

    try {
        // This should cause a type error
        int wrong_type = test_data["string_field"].get<int>();
        std::cout << "This shouldn't print: " << wrong_type << std::endl;
    } catch (const json::type_error& e) {
        std::cout << "✓ Caught expected type error: " << e.what() << std::endl;
    }

    // Check for key existence
    std::cout << "Has 'string_field': " << test_data.contains("string_field")
              << std::endl;
    std::cout << "Has 'missing_field': " << test_data.contains("missing_field")
              << std::endl;

    std::cout << "\nAll JSON examples completed successfully!" << std::endl;
    return 0;
}
