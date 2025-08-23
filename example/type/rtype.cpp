#include "atom/type/rtype.hpp"
#include "atom/type/rjson.hpp"
#include "atom/type/ryaml.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <map>

using namespace atom::type;

struct Address {
    std::string street;
    std::string city;
    int zip;
};

struct Person {
    std::string name;
    int age;
    bool isEmployed;
    std::vector<std::string> skills;
    Address address;
};

int main() {
    // Define reflection for Address
    auto addressReflection = Reflectable<Address>(
        make_field("street", "Street name", &Address::street),
        make_field("city", "City name", &Address::city),
        make_field("zip", "ZIP code", &Address::zip)
    );

    // Define reflection for Person
    auto personReflection = Reflectable<Person>(
        make_field("name", "Name of the person", &Person::name),
        make_field("age", "Age of the person", &Person::age),
        make_field("isEmployed", "Employment status", &Person::isEmployed),
        make_field("skills", "Skills of the person", &Person::skills),
        make_field("address", "Address of the person", &Person::address, addressReflection)
    );

    // Create a JSON object representing a person
    JsonObject personJson = {
        {"name", "John Doe"},
        {"age", 30},
        {"isEmployed", true},
        {"skills", JsonArray{"C++", "Python", "Java"}},
        {"address", JsonObject{
            {"street", "123 Main St"},
            {"city", "Anytown"},
            {"zip", 12345}
        }}
    };

    // Convert JSON to Person object
    Person person = personReflection.from_json(personJson);
    std::cout << "Person name: " << person.name << std::endl;
    std::cout << "Person age: " << person.age << std::endl;
    std::cout << "Person isEmployed: " << std::boolalpha << person.isEmployed << std::endl;
    std::cout << "Person skills: ";
    for (const auto& skill : person.skills) {
        std::cout << skill << " ";
    }
    std::cout << std::endl;
    std::cout << "Person address: " << person.address.street << ", " << person.address.city << ", " << person.address.zip << std::endl;

    // Convert Person object back to JSON
    JsonObject newPersonJson = personReflection.to_json(person);
    std::cout << "Person JSON: " << newPersonJson.dump() << std::endl;

    // Create a YAML object representing a person
    YamlObject personYaml = {
        {"name", "Jane Doe"},
        {"age", 25},
        {"isEmployed", false},
        {"skills", YamlArray{"JavaScript", "HTML", "CSS"}},
        {"address", YamlObject{
            {"street", "456 Elm St"},
            {"city", "Othertown"},
            {"zip", 67890}
        }}
    };

    // Convert YAML to Person object
    Person personFromYaml = personReflection.from_yaml(personYaml);
    std::cout << "Person from YAML name: " << personFromYaml.name << std::endl;
    std::cout << "Person from YAML age: " << personFromYaml.age << std::endl;
    std::cout << "Person from YAML isEmployed: " << std::boolalpha << personFromYaml.isEmployed << std::endl;
    std::cout << "Person from YAML skills: ";
    for (const auto& skill : personFromYaml.skills) {
        std::cout << skill << " ";
    }
    std::cout << std::endl;
    std::cout << "Person from YAML address: " << personFromYaml.address.street << ", " << personFromYaml.address.city << ", " << personFromYaml.address.zip << std::endl;

    // Convert Person object back to YAML
    YamlObject newPersonYaml = personReflection.to_yaml(personFromYaml);
    std::cout << "Person YAML: " << newPersonYaml.dump() << std::endl;

    return 0;
}
