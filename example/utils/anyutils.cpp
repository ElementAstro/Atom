/**
 * @file anyutils_example.cpp
 * @brief Comprehensive examples demonstrating AnyUtils functionality
 *
 * This example demonstrates serialization functionality for various data types:
 * - toString(): Converts objects to string representation
 * - toJson(): Converts objects to JSON format
 * - toXml(): Converts objects to XML format
 * - toYaml(): Converts objects to YAML format
 * - toToml(): Converts objects to TOML format
 */

#include "atom/utils/anyutils.hpp"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Define a custom class to demonstrate the extension points
class Person {
private:
    std::string name_;
    int age_;
    std::vector<std::string> hobbies_;

public:
    Person(const std::string& name, int age,
           const std::vector<std::string>& hobbies)
        : name_(name), age_(age), hobbies_(hobbies) {}

    // Custom toString method for Person class
    std::string toString() const {
        return name_ + " (" + std::to_string(age_) + " years)";
    }

    // Custom toJson method for Person class
    std::string toJson() const {
        std::stringstream ss;
        ss << "{\"name\":\"" << name_ << "\",\"age\":" << age_
           << ",\"hobbies\":";
        ss << ::toJson(hobbies_, false) << "}";
        return ss.str();
    }

    // Custom toXml method for Person class
    std::string toXml(const std::string& tag) const {
        std::string personTag = tag.empty() ? "person" : tag;
        std::stringstream ss;
        ss << "<" << personTag << ">";
        ss << "<name>" << name_ << "</name>";
        ss << "<age>" << age_ << "</age>";
        ss << ::toXml(hobbies_, "hobbies");
        ss << "</" << personTag << ">";
        return ss.str();
    }

    // Custom toYaml method for Person class
    std::string toYaml(const std::string& key) const {
        std::stringstream ss;
        if (!key.empty()) {
            ss << key << ":\n";
            ss << "  name: \"" << name_ << "\"\n";
            ss << "  age: " << age_ << "\n";
            ss << "  hobbies:\n";
            for (const auto& hobby : hobbies_) {
                ss << "    - \"" << hobby << "\"\n";
            }
        } else {
            ss << "name: \"" << name_ << "\"\n";
            ss << "age: " << age_ << "\n";
            ss << "hobbies:\n";
            for (const auto& hobby : hobbies_) {
                ss << "  - \"" << hobby << "\"\n";
            }
        }
        return ss.str();
    }

    // Custom toToml method for Person class
    std::string toToml(const std::string& key) const {
        std::stringstream ss;
        if (!key.empty()) {
            ss << "[" << key << "]\n";
        }
        ss << "name = \"" << name_ << "\"\n";
        ss << "age = " << age_ << "\n";
        ss << "hobbies = [";
        for (size_t i = 0; i < hobbies_.size(); ++i) {
            if (i > 0)
                ss << ", ";
            ss << "\"" << hobbies_[i] << "\"";
        }
        ss << "]\n";
        return ss.str();
    }
};

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================" << std::endl;
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

int main() {
    try {
        std::cout << "==========================================" << std::endl;
        std::cout << "  AnyUtils Serialization Demo" << std::endl;
        std::cout << "==========================================" << std::endl;

        // ============================
        // Example 1: Basic Types
        // ============================
        printSection("1. Basic Types");

        // String serialization
        printSubsection("String Serialization");
        std::string stringValue = "Hello, World!";
        std::cout << "toString: " << toString(stringValue) << std::endl;
        std::cout << "toJson: " << toJson(stringValue) << std::endl;
        std::cout << "toXml: " << toXml(stringValue, "greeting") << std::endl;
        std::cout << "toYaml: " << toYaml(stringValue, "greeting") << std::endl;
        std::cout << "toToml: " << toToml(stringValue, "greeting") << std::endl;

        // Numeric serialization
        printSubsection("Numeric Serialization");
        int intValue = 42;
        double doubleValue = 3.14159;
        std::cout << "toString (int): " << toString(intValue) << std::endl;
        std::cout << "toJson (int): " << toJson(intValue) << std::endl;
        std::cout << "toXml (int): " << toXml(intValue, "answer") << std::endl;
        std::cout << "toYaml (int): " << toYaml(intValue, "answer")
                  << std::endl;
        std::cout << "toToml (int): " << toToml(intValue, "answer")
                  << std::endl;
        std::cout << std::endl;
        std::cout << "toString (double): " << toString(doubleValue)
                  << std::endl;
        std::cout << "toJson (double): " << toJson(doubleValue) << std::endl;
        std::cout << "toXml (double): " << toXml(doubleValue, "pi")
                  << std::endl;
        std::cout << "toYaml (double): " << toYaml(doubleValue, "pi")
                  << std::endl;
        std::cout << "toToml (double): " << toToml(doubleValue, "pi")
                  << std::endl;

        // Special floating point values
        printSubsection("Special Floating Point Values");
        double nanValue = std::numeric_limits<double>::quiet_NaN();
        double infValue = std::numeric_limits<double>::infinity();
        double negInfValue = -std::numeric_limits<double>::infinity();

        std::cout << "toString (NaN): " << toString(nanValue) << std::endl;
        std::cout << "toJson (NaN): " << toJson(nanValue) << std::endl;
        std::cout << "toYaml (NaN): " << toYaml(nanValue, "nan_value")
                  << std::endl;
        std::cout << "toToml (NaN): " << toToml(nanValue, "nan_value")
                  << std::endl;
        std::cout << std::endl;

        std::cout << "toString (Infinity): " << toString(infValue) << std::endl;
        std::cout << "toJson (Infinity): " << toJson(infValue) << std::endl;
        std::cout << "toYaml (Infinity): " << toYaml(infValue, "inf_value")
                  << std::endl;
        std::cout << "toToml (Infinity): " << toToml(infValue, "inf_value")
                  << std::endl;
        std::cout << std::endl;

        std::cout << "toString (-Infinity): " << toString(negInfValue)
                  << std::endl;
        std::cout << "toJson (-Infinity): " << toJson(negInfValue) << std::endl;
        std::cout << "toYaml (-Infinity): "
                  << toYaml(negInfValue, "neg_inf_value") << std::endl;
        std::cout << "toToml (-Infinity): "
                  << toToml(negInfValue, "neg_inf_value") << std::endl;

        // Boolean serialization
        printSubsection("Boolean Serialization");
        bool boolTrue = true;
        bool boolFalse = false;
        std::cout << "toString (true): " << toString(boolTrue) << std::endl;
        std::cout << "toJson (true): " << toJson(boolTrue) << std::endl;
        std::cout << "toXml (true): " << toXml(boolTrue, "flag") << std::endl;
        std::cout << "toYaml (true): " << toYaml(boolTrue, "flag") << std::endl;
        std::cout << "toToml (true): " << toToml(boolTrue, "flag") << std::endl;
        std::cout << std::endl;

        std::cout << "toString (false): " << toString(boolFalse) << std::endl;
        std::cout << "toJson (false): " << toJson(boolFalse) << std::endl;
        std::cout << "toXml (false): " << toXml(boolFalse, "active")
                  << std::endl;
        std::cout << "toYaml (false): " << toYaml(boolFalse, "active")
                  << std::endl;
        std::cout << "toToml (false): " << toToml(boolFalse, "active")
                  << std::endl;

        // Character serialization
        printSubsection("Character Serialization");
        char charValue = 'A';
        char specialChar = '\n';
        std::cout << "toString (char): " << toString(charValue) << std::endl;
        std::cout << "toJson (char): " << toJson(charValue) << std::endl;
        std::cout << "toXml (char): " << toXml(charValue, "letter")
                  << std::endl;
        std::cout << "toYaml (char): " << toYaml(charValue, "letter")
                  << std::endl;
        std::cout << "toToml (char): " << toToml(charValue, "letter")
                  << std::endl;
        std::cout << std::endl;

        std::cout << "toJson (special char): " << toJson(specialChar)
                  << std::endl;
        std::cout << "toXml (special char): " << toXml(specialChar, "newline")
                  << std::endl;

        // ============================
        // Example 2: Containers
        // ============================
        printSection("2. Containers");

        // Vector serialization
        printSubsection("Vector Serialization");
        std::vector<int> intVector = {1, 2, 3, 4, 5};
        std::cout << "toString (vector): " << toString(intVector) << std::endl;
        std::cout << "toString (vector, pretty): " << toString(intVector, true)
                  << std::endl;
        std::cout << "toJson (vector): " << toJson(intVector) << std::endl;
        std::cout << "toJson (vector, pretty): " << toJson(intVector, true)
                  << std::endl;
        std::cout << "toXml (vector): " << toXml(intVector, "numbers")
                  << std::endl;
        std::cout << "toYaml (vector): " << toYaml(intVector, "numbers")
                  << std::endl;
        std::cout << "toToml (vector): " << toToml(intVector, "numbers")
                  << std::endl;

        // Nested containers
        printSubsection("Nested Containers");
        std::vector<std::vector<int>> nestedVector = {{1, 2}, {3, 4}, {5, 6}};
        std::cout << "toString (nested vector): " << toString(nestedVector)
                  << std::endl;
        std::cout << "toString (nested vector, pretty): "
                  << toString(nestedVector, true) << std::endl;
        std::cout << "toJson (nested vector): " << toJson(nestedVector)
                  << std::endl;
        std::cout << "toJson (nested vector, pretty): "
                  << toJson(nestedVector, true) << std::endl;
        std::cout << "toXml (nested vector): " << toXml(nestedVector, "matrix")
                  << std::endl;
        std::cout << "toYaml (nested vector): "
                  << toYaml(nestedVector, "matrix") << std::endl;
        std::cout << "toToml (nested vector): "
                  << toToml(nestedVector, "matrix") << std::endl;

        // Empty container serialization
        printSubsection("Empty Container Serialization");
        std::vector<int> emptyVector;
        std::cout << "toString (empty vector): " << toString(emptyVector)
                  << std::endl;
        std::cout << "toJson (empty vector): " << toJson(emptyVector)
                  << std::endl;
        std::cout << "toXml (empty vector): "
                  << toXml(emptyVector, "empty_list") << std::endl;
        std::cout << "toYaml (empty vector): "
                  << toYaml(emptyVector, "empty_list") << std::endl;
        std::cout << "toToml (empty vector): "
                  << toToml(emptyVector, "empty_list") << std::endl;

        // Mixed type container (using strings as elements)
        printSubsection("Mixed Type Container");
        std::vector<std::string> stringVector = {"hello", "world", "123",
                                                 "true"};
        std::cout << "toString (string vector): " << toString(stringVector)
                  << std::endl;
        std::cout << "toJson (string vector): " << toJson(stringVector)
                  << std::endl;
        std::cout << "toXml (string vector): " << toXml(stringVector, "words")
                  << std::endl;
        std::cout << "toYaml (string vector): " << toYaml(stringVector, "words")
                  << std::endl;
        std::cout << "toToml (string vector): " << toToml(stringVector, "words")
                  << std::endl;

        // ============================
        // Example 3: Maps
        // ============================
        printSection("3. Maps");

        // Basic map serialization
        printSubsection("Basic Map Serialization");
        std::unordered_map<std::string, int> simpleMap = {
            {"one", 1}, {"two", 2}, {"three", 3}};
        std::cout << "toString (map): " << toString(simpleMap) << std::endl;
        std::cout << "toString (map, pretty): " << toString(simpleMap, true)
                  << std::endl;
        std::cout << "toJson (map): " << toJson(simpleMap) << std::endl;
        std::cout << "toJson (map, pretty): " << toJson(simpleMap, true)
                  << std::endl;
        std::cout << "toXml (map): " << toXml(simpleMap, "counts") << std::endl;
        std::cout << "toYaml (map): " << toYaml(simpleMap, "counts")
                  << std::endl;
        std::cout << "toToml (map): " << toToml(simpleMap, "counts")
                  << std::endl;

        // Map with special characters in keys
        printSubsection("Map with Special Keys");
        std::unordered_map<std::string, int> specialKeysMap = {
            {"normal", 1},
            {"with space", 2},
            {"with:colon", 3},
            {"with\nnewline", 4}};
        std::cout << "toJson (special keys map): " << toJson(specialKeysMap)
                  << std::endl;
        std::cout << "toXml (special keys map): "
                  << toXml(specialKeysMap, "special_map") << std::endl;
        std::cout << "toYaml (special keys map): "
                  << toYaml(specialKeysMap, "special_map") << std::endl;
        std::cout << "toToml (special keys map): "
                  << toToml(specialKeysMap, "special_map") << std::endl;

        // Nested map serialization
        printSubsection("Nested Map Serialization");
        std::unordered_map<std::string, std::unordered_map<std::string, int>>
            nestedMap = {{"math", {{"algebra", 90}, {"geometry", 85}}},
                         {"science", {{"physics", 88}, {"chemistry", 92}}}};
        std::cout << "toJson (nested map): " << toJson(nestedMap, true)
                  << std::endl;
        std::cout << "toYaml (nested map): " << toYaml(nestedMap, "grades")
                  << std::endl;
        std::cout << "toToml (nested map): " << toToml(nestedMap, "grades")
                  << std::endl;

        // Map with non-string keys
        printSubsection("Map with Non-String Keys");
        std::unordered_map<int, std::string> intKeyMap = {
            {1, "one"}, {2, "two"}, {3, "three"}};
        std::cout << "toString (int key map): " << toString(intKeyMap)
                  << std::endl;
        std::cout << "toJson (int key map): " << toJson(intKeyMap) << std::endl;
        std::cout << "toXml (int key map): " << toXml(intKeyMap, "numbers")
                  << std::endl;
        std::cout << "toYaml (int key map): " << toYaml(intKeyMap, "numbers")
                  << std::endl;
        std::cout << "toToml (int key map): " << toToml(intKeyMap, "numbers")
                  << std::endl;

        // ============================
        // Example 4: Pairs and Tuples
        // ============================
        printSection("4. Pairs and Tuples");

        // Pair serialization
        printSubsection("Pair Serialization");
        std::pair<std::string, int> simplePair = {"answer", 42};
        std::cout << "toString (pair): " << toString(simplePair) << std::endl;
        std::cout << "toJson (pair): " << toJson(simplePair) << std::endl;
        std::cout << "toJson (pair, pretty): " << toJson(simplePair, true)
                  << std::endl;
        std::cout << "toXml (pair): " << toXml(simplePair, "key_value")
                  << std::endl;
        std::cout << "toYaml (pair): " << toYaml(simplePair, "key_value")
                  << std::endl;
        std::cout << "toToml (pair): " << toToml(simplePair, "key_value")
                  << std::endl;

        // Nested pair serialization
        printSubsection("Nested Pair Serialization");
        std::pair<std::string, std::pair<int, double>> nestedPair = {
            "data", {42, 3.14}};
        std::cout << "toString (nested pair): " << toString(nestedPair)
                  << std::endl;
        std::cout << "toJson (nested pair): " << toJson(nestedPair)
                  << std::endl;
        std::cout << "toXml (nested pair): "
                  << toXml(nestedPair, "nested_key_value") << std::endl;

        // Tuple serialization
        printSubsection("Tuple Serialization");
        std::tuple<int, std::string, double> simpleTuple = {1, "hello", 3.14};
        // Tuples have custom toYaml and toToml implementations only
        std::cout << "toYaml (tuple): " << toYaml(simpleTuple, "my_tuple")
                  << std::endl;
        std::cout << "toToml (tuple): " << toToml(simpleTuple, "my_tuple")
                  << std::endl;

        // ============================
        // Example 5: Pointers and Smart Pointers
        // ============================
        printSection("5. Pointers and Smart Pointers");

        // Raw pointer serialization
        printSubsection("Raw Pointer Serialization");
        int rawValue = 42;
        int* rawPtr = &rawValue;
        std::cout << "toString (raw pointer): " << toString(rawPtr)
                  << std::endl;
        std::cout << "toJson (raw pointer): " << toJson(rawPtr) << std::endl;
        std::cout << "toXml (raw pointer): " << toXml(rawPtr, "pointer_value")
                  << std::endl;
        std::cout << "toYaml (raw pointer): " << toYaml(rawPtr, "pointer_value")
                  << std::endl;
        std::cout << "toToml (raw pointer): " << toToml(rawPtr, "pointer_value")
                  << std::endl;

        // Nullptr serialization
        printSubsection("Nullptr Serialization");
        int* nullPtr = nullptr;
        std::cout << "toString (nullptr): " << toString(nullPtr) << std::endl;
        std::cout << "toJson (nullptr): " << toJson(nullPtr) << std::endl;
        std::cout << "toXml (nullptr): " << toXml(nullPtr, "null_pointer")
                  << std::endl;
        std::cout << "toYaml (nullptr): " << toYaml(nullPtr, "null_pointer")
                  << std::endl;
        std::cout << "toToml (nullptr): " << toToml(nullPtr, "null_pointer")
                  << std::endl;

        // Unique pointer serialization
        printSubsection("Unique Pointer Serialization");
        auto uniquePtr = std::make_unique<int>(42);
        std::cout << "toString (unique_ptr): " << toString(uniquePtr)
                  << std::endl;
        std::cout << "toJson (unique_ptr): " << toJson(uniquePtr) << std::endl;
        std::cout << "toXml (unique_ptr): "
                  << toXml(uniquePtr, "unique_pointer") << std::endl;
        std::cout << "toYaml (unique_ptr): "
                  << toYaml(uniquePtr, "unique_pointer") << std::endl;
        std::cout << "toToml (unique_ptr): "
                  << toToml(uniquePtr, "unique_pointer") << std::endl;

        // Shared pointer serialization
        printSubsection("Shared Pointer Serialization");
        auto sharedPtr = std::make_shared<std::string>("Hello, World!");
        std::cout << "toString (shared_ptr): " << toString(sharedPtr)
                  << std::endl;
        std::cout << "toJson (shared_ptr): " << toJson(sharedPtr) << std::endl;
        std::cout << "toXml (shared_ptr): "
                  << toXml(sharedPtr, "shared_pointer") << std::endl;
        std::cout << "toYaml (shared_ptr): "
                  << toYaml(sharedPtr, "shared_pointer") << std::endl;
        std::cout << "toToml (shared_ptr): "
                  << toToml(sharedPtr, "shared_pointer") << std::endl;

        // Container of pointers
        printSubsection("Container of Pointers");
        std::vector<std::shared_ptr<int>> pointerVector;
        for (int i = 0; i < 3; ++i) {
            pointerVector.push_back(std::make_shared<int>(i * 10));
        }
        std::cout << "toJson (vector of pointers): " << toJson(pointerVector)
                  << std::endl;
        std::cout << "toXml (vector of pointers): "
                  << toXml(pointerVector, "pointer_list") << std::endl;
        std::cout << "toYaml (vector of pointers): "
                  << toYaml(pointerVector, "pointer_list") << std::endl;

        // ============================
        // Example 6: Custom Types
        // ============================
        printSection("6. Custom Types");

        // Custom class serialization using extension methods
        printSubsection("Custom Class Serialization");
        Person person("John Doe", 30, {"reading", "hiking", "coding"});
        std::cout << "toString (Person): " << toString(person) << std::endl;
        std::cout << "toJson (Person): " << toJson(person) << std::endl;
        std::cout << "toXml (Person): " << toXml(person, "employee")
                  << std::endl;
        std::cout << "toYaml (Person): " << toYaml(person, "employee")
                  << std::endl;
        std::cout << "toToml (Person): " << toToml(person, "employee")
                  << std::endl;

        // Container of custom objects
        printSubsection("Container of Custom Objects");
        std::vector<Person> people = {
            Person("Alice Smith", 28, {"painting", "music"}),
            Person("Bob Johnson", 35, {"sports", "cooking"})};
        std::cout << "toJson (vector of Person): " << toJson(people, true)
                  << std::endl;
        std::cout << "toXml (vector of Person): " << toXml(people, "employees")
                  << std::endl;
        std::cout << "toYaml (vector of Person): "
                  << toYaml(people, "employees") << std::endl;

        // Map with custom objects
        printSubsection("Map with Custom Objects");
        std::unordered_map<std::string, Person> personMap = {
            {"manager", Person("Jane Wilson", 42, {"leadership", "strategy"})},
            {"developer", Person("Dave Brown", 27, {"coding", "gaming"})}};
        std::cout << "toJson (map of Person): " << toJson(personMap, true)
                  << std::endl;
        std::cout << "toXml (map of Person): " << toXml(personMap, "staff")
                  << std::endl;
        std::cout << "toYaml (map of Person): " << toYaml(personMap, "staff")
                  << std::endl;

        // ============================
        // Example 7: Error Handling
        // ============================
        printSection("7. Error Handling");

        // Invalid XML tag name
        printSubsection("Invalid XML Tag Name");
        try {
            std::cout << "toXml with invalid tag: "
                      << toXml(stringValue, "invalid<tag>") << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Caught exception: " << e.what() << std::endl;
        }

        // Empty XML tag name
        printSubsection("Empty XML Tag Name");
        try {
            std::cout << "toXml with empty tag: " << toXml(stringValue, "")
                      << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Caught exception: " << e.what() << std::endl;
        }

        // XML with special characters
        printSubsection("XML with Special Characters");
        std::string specialString =
            "Text with <tags> & \"quotes\" and 'apostrophes'";
        std::cout << "toXml with special characters: "
                  << toXml(specialString, "special") << std::endl;

        // ============================
        // Example 8: Performance Features
        // ============================
        printSection("8. Performance Features");

        // Caching for repeated conversions
        printSubsection("Caching");
        std::vector<int> cachingVector = {1, 2, 3, 4, 5};

        std::cout << "First toString call: " << toString(cachingVector)
                  << std::endl;
        // Second call should use the cache
        std::cout << "Second toString call: " << toString(cachingVector)
                  << std::endl;

        // Parallel processing for large collections
        printSubsection("Parallel Processing");
        std::cout << "Creating a large vector for parallel processing "
                     "demonstration..."
                  << std::endl;

        // Create a large vector to demonstrate parallel processing
        std::vector<int> largeVector;
        for (int i = 0; i < 2000; ++i) {
            largeVector.push_back(i);
        }

        std::cout << "Converting large vector to JSON (will use parallel "
                     "processing)..."
                  << std::endl;
        std::string jsonResult = toJson(largeVector);
        std::cout << "JSON result length: " << jsonResult.length()
                  << " characters" << std::endl;
        std::cout << "JSON result preview: " << jsonResult.substr(0, 50)
                  << "..." << std::endl;

        // Batch processing XML
        printSubsection("Batch Processing");
        std::cout
            << "Converting large vector to XML (will use batch processing)..."
            << std::endl;
        std::string xmlResult = toXml(largeVector, "large_numbers");
        std::cout << "XML result length: " << xmlResult.length()
                  << " characters" << std::endl;
        std::cout << "XML result preview: " << xmlResult.substr(0, 50) << "..."
                  << std::endl;

        std::cout << "\nAll examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
