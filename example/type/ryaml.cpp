#include <iostream>
#include <string>
#include <vector>

#include "atom/type/ryaml.hpp"

// Helper function to print section headers
void print_header(const std::string& title) {
    std::cout << "\n=== " << title << " ===" << std::endl;
    std::cout << std::string(title.length() + 8, '=') << std::endl;
}

// Helper function to print YAML type
std::string type_to_string(atom::type::YamlValue::Type type) {
    switch (type) {
        case atom::type::YamlValue::Type::Null:
            return "Null";
        case atom::type::YamlValue::Type::String:
            return "String";
        case atom::type::YamlValue::Type::Number:
            return "Number";
        case atom::type::YamlValue::Type::Bool:
            return "Bool";
        case atom::type::YamlValue::Type::Object:
            return "Object";
        case atom::type::YamlValue::Type::Array:
            return "Array";
        default:
            return "Unknown";
    }
}

// Helper function to print YamlValue recursively
void print_yaml_value(const atom::type::YamlValue& value, int indent = 0) {
    std::string spaces(indent * 2, ' ');

    switch (value.type()) {
        case atom::type::YamlValue::Type::Null:
            std::cout << spaces << "null";
            break;
        case atom::type::YamlValue::Type::String:
            std::cout << spaces << "\"" << value.asString() << "\"";
            break;
        case atom::type::YamlValue::Type::Number:
            std::cout << spaces << value.asNumber();
            break;
        case atom::type::YamlValue::Type::Bool:
            std::cout << spaces << (value.asBool() ? "true" : "false");
            break;
        case atom::type::YamlValue::Type::Object: {
            const auto& obj = value.asObject();
            for (const auto& [key, val] : obj) {
                std::cout << spaces << key << ": ";
                if (val.type() == atom::type::YamlValue::Type::Object ||
                    val.type() == atom::type::YamlValue::Type::Array) {
                    std::cout << std::endl;
                    print_yaml_value(val, indent + 1);
                } else {
                    print_yaml_value(val, 0);
                    std::cout << std::endl;
                }
            }
            break;
        }
        case atom::type::YamlValue::Type::Array: {
            const auto& arr = value.asArray();
            for (const auto& item : arr) {
                std::cout << spaces << "- ";
                if (item.type() == atom::type::YamlValue::Type::Object ||
                    item.type() == atom::type::YamlValue::Type::Array) {
                    std::cout << std::endl;
                    print_yaml_value(item, indent + 1);
                } else {
                    print_yaml_value(item, 0);
                    std::cout << std::endl;
                }
            }
            break;
        }
    }
}

int main() {
    std::cout << "RYAML (Rapid YAML) Usage Examples" << std::endl;
    std::cout << "==================================" << std::endl;

    // 1. Basic Value Creation
    print_header("Basic Value Creation");

    // Create different types of YAML values
    atom::type::YamlValue null_value;
    atom::type::YamlValue string_value("Hello, YAML!");
    atom::type::YamlValue number_value(42.5);
    atom::type::YamlValue bool_value(true);

    std::cout << "Created basic YAML values:" << std::endl;
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

    atom::type::YamlObject person_obj;
    person_obj["name"] = atom::type::YamlValue("John Doe");
    person_obj["age"] = atom::type::YamlValue(30.0);
    person_obj["is_employed"] = atom::type::YamlValue(true);
    person_obj["spouse"] = atom::type::YamlValue();  // null value

    atom::type::YamlValue person(person_obj);

    std::cout << "Created person object:" << std::endl;
    print_yaml_value(person);

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

    atom::type::YamlArray numbers_array;
    numbers_array.push_back(atom::type::YamlValue(1.0));
    numbers_array.push_back(atom::type::YamlValue(2.0));
    numbers_array.push_back(atom::type::YamlValue(3.0));
    numbers_array.push_back(atom::type::YamlValue(4.0));
    numbers_array.push_back(atom::type::YamlValue(5.0));

    atom::type::YamlValue numbers(numbers_array);

    std::cout << "Created numbers array:" << std::endl;
    print_yaml_value(numbers);

    // Mixed array
    atom::type::YamlArray mixed_array;
    mixed_array.push_back(atom::type::YamlValue("text"));
    mixed_array.push_back(atom::type::YamlValue(42.0));
    mixed_array.push_back(atom::type::YamlValue(true));
    mixed_array.push_back(atom::type::YamlValue());  // null

    atom::type::YamlValue mixed(mixed_array);

    std::cout << "\nCreated mixed array:" << std::endl;
    print_yaml_value(mixed);

    // 4. Nested Structures
    print_header("Nested Structures");

    // Create a complex nested structure
    atom::type::YamlObject address_obj;
    address_obj["street"] = atom::type::YamlValue("123 Main St");
    address_obj["city"] = atom::type::YamlValue("Anytown");
    address_obj["zip"] = atom::type::YamlValue(12345.0);

    atom::type::YamlArray hobbies_array;
    hobbies_array.push_back(atom::type::YamlValue("reading"));
    hobbies_array.push_back(atom::type::YamlValue("swimming"));
    hobbies_array.push_back(atom::type::YamlValue("coding"));

    atom::type::YamlObject complex_person;
    complex_person["name"] = atom::type::YamlValue("Alice Johnson");
    complex_person["age"] = atom::type::YamlValue(28.0);
    complex_person["address"] = atom::type::YamlValue(address_obj);
    complex_person["hobbies"] = atom::type::YamlValue(hobbies_array);

    atom::type::YamlValue complex_yaml(complex_person);

    std::cout << "Created complex nested structure:" << std::endl;
    print_yaml_value(complex_yaml);

    // 5. YAML Document Creation and Serialization
    print_header("YAML Document Creation and Serialization");

    atom::type::YamlDocument doc;
    doc.setRoot(complex_yaml);

    std::cout << "Created YAML document and serializing to string:"
              << std::endl;
    try {
        std::string yaml_string = doc.to_yaml();
        std::cout << yaml_string << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Serialization error: " << e.what() << std::endl;
    }

    // 6. YAML Parsing
    print_header("YAML Parsing");

    // Simple YAML strings
    std::string simple_yaml = R"(
name: Bob
age: 25
active: true
)";

    std::string array_yaml = R"(
- 1
- 2
- 3
- hello
- null
- false
)";

    std::string nested_yaml = R"(
user:
  id: 123
  profile:
    name: Charlie
    settings:
      - true
      - false
      - null
)";

    try {
        std::cout << "Parsing simple YAML:" << std::endl;
        auto parsed_simple = atom::type::YamlParser::parse(simple_yaml);
        print_yaml_value(parsed_simple);

        std::cout << "\nParsing array YAML:" << std::endl;
        auto parsed_array = atom::type::YamlParser::parse(array_yaml);
        print_yaml_value(parsed_array);

        std::cout << "\nParsing nested YAML:" << std::endl;
        auto parsed_nested = atom::type::YamlParser::parse(nested_yaml);
        print_yaml_value(parsed_nested);

    } catch (const std::exception& e) {
        std::cout << "Parsing error: " << e.what() << std::endl;
    }

    // 7. Configuration File Example
    print_header("Configuration File Example");

    std::string config_yaml = R"(
database:
  host: localhost
  port: 5432
  name: myapp
  credentials:
    username: admin
    password: secret

server:
  host: 0.0.0.0
  port: 8080
  ssl: true
  
features:
  - authentication
  - logging
  - caching
  - monitoring

logging:
  level: info
  file: /var/log/myapp.log
  max_size: 100MB
)";

    try {
        std::cout << "Parsing configuration YAML:" << std::endl;
        auto config = atom::type::YamlParser::parse(config_yaml);

        // Access configuration values
        const auto& config_obj = config.asObject();
        const auto& db_config = config_obj.at("database").asObject();
        const auto& server_config = config_obj.at("server").asObject();
        const auto& features = config_obj.at("features").asArray();

        std::cout << "\nConfiguration summary:" << std::endl;
        std::cout << "Database host: " << db_config.at("host").asString()
                  << std::endl;
        std::cout << "Database port: " << db_config.at("port").asNumber()
                  << std::endl;
        std::cout << "Server port: " << server_config.at("port").asNumber()
                  << std::endl;
        std::cout << "SSL enabled: "
                  << (server_config.at("ssl").asBool() ? "Yes" : "No")
                  << std::endl;

        std::cout << "Features: ";
        for (const auto& feature : features) {
            std::cout << feature.asString() << " ";
        }
        std::cout << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Configuration parsing error: " << e.what() << std::endl;
    }

    std::cout << "\nAll RYAML examples completed successfully!" << std::endl;
    return 0;
}
