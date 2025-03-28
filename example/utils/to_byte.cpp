/*
 * serialization_example.cpp
 *
 * Copyright (C) 2024 Max Q.
 *
 * Example usage of the atom::utils serialization functions
 */

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "atom/utils/to_byte.hpp"

// Helper function to print bytes in a readable hex format
void printBytes(const std::vector<uint8_t>& bytes, const std::string& label) {
    std::cout << label << " [" << bytes.size() << " bytes]: ";

    // Print up to first 32 bytes
    size_t displayCount = std::min(bytes.size(), size_t(32));
    for (size_t i = 0; i < displayCount; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(bytes[i]) << " ";
    }

    if (bytes.size() > 32) {
        std::cout << "...";
    }

    std::cout << std::dec << std::endl;
}

// Simple custom structs for serialization examples
struct Point {
    int x;
    int y;
};

// Custom serialization function for Point
auto serialize(const Point& point) -> std::vector<uint8_t> {
    std::vector<uint8_t> bytes;
    auto xBytes = atom::utils::serialize(point.x);
    auto yBytes = atom::utils::serialize(point.y);
    bytes.insert(bytes.end(), xBytes.begin(), xBytes.end());
    bytes.insert(bytes.end(), yBytes.begin(), yBytes.end());
    return bytes;
}

// Custom deserialization function for Point
template <>
auto atom::utils::deserialize<Point>(const std::span<const uint8_t>& bytes,
                                     size_t& offset) -> Point {
    Point point;
    point.x = deserialize<int>(bytes, offset);
    point.y = deserialize<int>(bytes, offset);
    return point;
}

// A more complex structure with nested elements
struct Person {
    std::string name;
    int age;
    std::optional<std::string> email;
    std::vector<std::string> hobbies;
};

// Custom serialization function for Person
auto serialize(const Person& person) -> std::vector<uint8_t> {
    std::vector<uint8_t> bytes;

    // Serialize name
    auto nameBytes = atom::utils::serialize(person.name);
    bytes.insert(bytes.end(), nameBytes.begin(), nameBytes.end());

    // Serialize age
    auto ageBytes = atom::utils::serialize(person.age);
    bytes.insert(bytes.end(), ageBytes.begin(), ageBytes.end());

    // Serialize optional email
    auto emailBytes = atom::utils::serialize(person.email);
    bytes.insert(bytes.end(), emailBytes.begin(), emailBytes.end());

    // Serialize hobbies vector
    auto hobbiesBytes = atom::utils::serialize(person.hobbies);
    bytes.insert(bytes.end(), hobbiesBytes.begin(), hobbiesBytes.end());

    return bytes;
}

// Custom deserialization function for Person
template <>
auto atom::utils::deserialize<Person>(const std::span<const uint8_t>& bytes,
                                      size_t& offset) -> Person {
    Person person;
    person.name = deserializeString(bytes, offset);
    person.age = deserialize<int>(bytes, offset);
    person.email = deserializeOptional<std::string>(bytes, offset);
    person.hobbies = deserializeVector<std::string>(bytes, offset);
    return person;
}

// Enum for variant example
enum class MessageType { Text, Number, Boolean };

int main() {
    std::cout << "=== Byte Serialization Comprehensive Example ===\n\n";

    try {
        std::cout << "Example 1: Serializing Basic Types\n";
        {
            // Integer serialization
            int intValue = 42;
            auto intBytes = atom::utils::serialize(intValue);
            printBytes(intBytes, "Serialized int (42)");

            // Float serialization
            float floatValue = 3.14159f;
            auto floatBytes = atom::utils::serialize(floatValue);
            printBytes(floatBytes, "Serialized float (3.14159)");

            // Double serialization
            double doubleValue = 2.71828182845;
            auto doubleBytes = atom::utils::serialize(doubleValue);
            printBytes(doubleBytes, "Serialized double (2.71828182845)");

            // Boolean serialization
            bool boolValue = true;
            auto boolBytes = atom::utils::serialize(boolValue);
            printBytes(boolBytes, "Serialized bool (true)");

            // Character serialization
            char charValue = 'A';
            auto charBytes = atom::utils::serialize(charValue);
            printBytes(charBytes, "Serialized char ('A')");

            // Enum serialization
            MessageType enumValue = MessageType::Text;
            auto enumBytes = atom::utils::serialize(enumValue);
            printBytes(enumBytes, "Serialized enum (MessageType::Text)");
        }
        std::cout << std::endl;

        std::cout << "Example 2: Serializing Strings\n";
        {
            // String serialization
            std::string stringValue = "Hello, World!";
            auto stringBytes = atom::utils::serialize(stringValue);
            printBytes(stringBytes, "Serialized string (\"Hello, World!\")");

            // Empty string
            std::string emptyString = "";
            auto emptyStringBytes = atom::utils::serialize(emptyString);
            printBytes(emptyStringBytes, "Serialized empty string");

            // Unicode string
            std::string unicodeString =
                "こんにちは世界";  // "Hello World" in Japanese
            auto unicodeBytes = atom::utils::serialize(unicodeString);
            printBytes(unicodeBytes, "Serialized Unicode string");
        }
        std::cout << std::endl;

        std::cout << "Example 3: Serializing Containers\n";
        {
            // Vector serialization
            std::vector<int> intVector = {1, 2, 3, 4, 5};
            auto vectorBytes = atom::utils::serialize(intVector);
            printBytes(vectorBytes, "Serialized vector<int> ({1,2,3,4,5})");

            // List serialization
            std::list<float> floatList = {1.1f, 2.2f, 3.3f};
            auto listBytes = atom::utils::serialize(floatList);
            printBytes(listBytes, "Serialized list<float> ({1.1,2.2,3.3})");

            // Map serialization
            std::map<std::string, int> stringIntMap = {
                {"one", 1}, {"two", 2}, {"three", 3}};
            auto mapBytes = atom::utils::serialize(stringIntMap);
            printBytes(mapBytes, "Serialized map<string,int>");

            // Vector of strings
            std::vector<std::string> stringVector = {"apple", "banana",
                                                     "cherry"};
            auto stringVecBytes = atom::utils::serialize(stringVector);
            printBytes(stringVecBytes, "Serialized vector<string>");
        }
        std::cout << std::endl;

        std::cout << "Example 4: Serializing Optional Values\n";
        {
            // Optional with value
            std::optional<int> optWithValue = 42;
            auto optWithValueBytes = atom::utils::serialize(optWithValue);
            printBytes(optWithValueBytes,
                       "Serialized optional<int> with value");

            // Optional without value
            std::optional<int> optWithoutValue = std::nullopt;
            auto optWithoutValueBytes = atom::utils::serialize(optWithoutValue);
            printBytes(optWithoutValueBytes,
                       "Serialized optional<int> without value");

            // Optional string with value
            std::optional<std::string> optStringWithValue = "optional string";
            auto optStringBytes = atom::utils::serialize(optStringWithValue);
            printBytes(optStringBytes,
                       "Serialized optional<string> with value");
        }
        std::cout << std::endl;

        std::cout << "Example 5: Serializing Variants\n";
        {
            // Variant with int
            std::variant<int, std::string, bool> varInt = 42;
            auto varIntBytes = atom::utils::serialize(varInt);
            printBytes(varIntBytes,
                       "Serialized variant<int,string,bool> with int");

            // Variant with string
            std::variant<int, std::string, bool> varString =
                std::string("variant string");
            auto varStringBytes = atom::utils::serialize(varString);
            printBytes(varStringBytes,
                       "Serialized variant<int,string,bool> with string");

            // Variant with bool
            std::variant<int, std::string, bool> varBool = true;
            auto varBoolBytes = atom::utils::serialize(varBool);
            printBytes(varBoolBytes,
                       "Serialized variant<int,string,bool> with bool");
        }
        std::cout << std::endl;

        std::cout << "Example 6: Custom Type Serialization\n";
        {
            // Point serialization
            Point point{10, 20};
            auto pointBytes = serialize(point);
            printBytes(pointBytes, "Serialized Point(10, 20)");

            // Person serialization
            Person person{
                "John Doe",                           // name
                30,                                   // age
                "john.doe@example.com",               // email (with value)
                {"reading", "hiking", "programming"}  // hobbies
            };
            auto personBytes = serialize(person);
            printBytes(personBytes, "Serialized Person");

            // Person with no email
            Person personNoEmail{
                "Jane Smith", 25, std::nullopt, {"painting", "cycling"}};
            auto personNoEmailBytes = serialize(personNoEmail);
            printBytes(personNoEmailBytes, "Serialized Person with no email");
        }
        std::cout << std::endl;

        std::cout << "Example 7: Deserialization of Basic Types\n";
        {
            // Create serialized data
            int originalInt = 42;
            float originalFloat = 3.14159f;
            bool originalBool = true;

            auto intBytes = atom::utils::serialize(originalInt);
            auto floatBytes = atom::utils::serialize(originalFloat);
            auto boolBytes = atom::utils::serialize(originalBool);

            // Deserialize
            size_t intOffset = 0;
            size_t floatOffset = 0;
            size_t boolOffset = 0;

            int deserializedInt =
                atom::utils::deserialize<int>(std::span(intBytes), intOffset);
            float deserializedFloat = atom::utils::deserialize<float>(
                std::span(floatBytes), floatOffset);
            bool deserializedBool = atom::utils::deserialize<bool>(
                std::span(boolBytes), boolOffset);

            std::cout << "Original int: " << originalInt
                      << ", Deserialized: " << deserializedInt << std::endl;
            std::cout << "Original float: " << originalFloat
                      << ", Deserialized: " << deserializedFloat << std::endl;
            std::cout << "Original bool: " << (originalBool ? "true" : "false")
                      << ", Deserialized: "
                      << (deserializedBool ? "true" : "false") << std::endl;
        }
        std::cout << std::endl;

        std::cout << "Example 8: Deserialization of Strings\n";
        {
            // Serialize
            std::string originalString = "Hello, Serialization!";
            auto stringBytes = atom::utils::serialize(originalString);

            // Deserialize
            size_t offset = 0;
            std::string deserializedString =
                atom::utils::deserializeString(std::span(stringBytes), offset);

            std::cout << "Original string: \"" << originalString << "\""
                      << std::endl;
            std::cout << "Deserialized string: \"" << deserializedString << "\""
                      << std::endl;
        }
        std::cout << std::endl;

        std::cout << "Example 9: Deserialization of Containers\n";
        {
            // Vector
            std::vector<int> originalVector = {5, 10, 15, 20, 25};
            auto vectorBytes = atom::utils::serialize(originalVector);

            size_t vecOffset = 0;
            auto deserializedVector = atom::utils::deserializeVector<int>(
                std::span(vectorBytes), vecOffset);

            std::cout << "Original vector: ";
            for (const auto& val : originalVector)
                std::cout << val << " ";
            std::cout << std::endl;

            std::cout << "Deserialized vector: ";
            for (const auto& val : deserializedVector)
                std::cout << val << " ";
            std::cout << std::endl;

            // List
            std::list<double> originalList = {1.1, 2.2, 3.3, 4.4};
            auto listBytes = atom::utils::serialize(originalList);

            size_t listOffset = 0;
            auto deserializedList = atom::utils::deserializeList<double>(
                std::span(listBytes), listOffset);

            std::cout << "Original list: ";
            for (const auto& val : originalList)
                std::cout << val << " ";
            std::cout << std::endl;

            std::cout << "Deserialized list: ";
            for (const auto& val : deserializedList)
                std::cout << val << " ";
            std::cout << std::endl;

            // Map
            std::map<std::string, int> originalMap = {
                {"first", 1}, {"second", 2}, {"third", 3}};
            auto mapBytes = atom::utils::serialize(originalMap);

            size_t mapOffset = 0;
            auto deserializedMap =
                atom::utils::deserializeMap<std::string, int>(
                    std::span(mapBytes), mapOffset);

            std::cout << "Original map: {";
            for (auto it = originalMap.begin(); it != originalMap.end(); ++it) {
                if (it != originalMap.begin())
                    std::cout << ", ";
                std::cout << "\"" << it->first << "\": " << it->second;
            }
            std::cout << "}" << std::endl;

            std::cout << "Deserialized map: {";
            for (auto it = deserializedMap.begin(); it != deserializedMap.end();
                 ++it) {
                if (it != deserializedMap.begin())
                    std::cout << ", ";
                std::cout << "\"" << it->first << "\": " << it->second;
            }
            std::cout << "}" << std::endl;
        }
        std::cout << std::endl;

        std::cout << "Example 10: Deserialization of Optional Values\n";
        {
            // Optional with value
            std::optional<int> originalOptWithValue = 42;
            auto optWithValueBytes =
                atom::utils::serialize(originalOptWithValue);

            size_t optWithValueOffset = 0;
            auto deserializedOptWithValue =
                atom::utils::deserializeOptional<int>(
                    std::span(optWithValueBytes), optWithValueOffset);

            std::cout << "Original optional<int> with value: "
                      << (originalOptWithValue.has_value()
                              ? std::to_string(*originalOptWithValue)
                              : "nullopt")
                      << std::endl;

            std::cout << "Deserialized optional<int> with value: "
                      << (deserializedOptWithValue.has_value()
                              ? std::to_string(*deserializedOptWithValue)
                              : "nullopt")
                      << std::endl;

            // Optional without value
            std::optional<int> originalOptWithoutValue = std::nullopt;
            auto optWithoutValueBytes =
                atom::utils::serialize(originalOptWithoutValue);

            size_t optWithoutValueOffset = 0;
            auto deserializedOptWithoutValue =
                atom::utils::deserializeOptional<int>(
                    std::span(optWithoutValueBytes), optWithoutValueOffset);

            std::cout << "Original optional<int> without value: "
                      << (originalOptWithoutValue.has_value()
                              ? std::to_string(*originalOptWithoutValue)
                              : "nullopt")
                      << std::endl;

            std::cout << "Deserialized optional<int> without value: "
                      << (deserializedOptWithoutValue.has_value()
                              ? std::to_string(*deserializedOptWithoutValue)
                              : "nullopt")
                      << std::endl;
        }
        std::cout << std::endl;

        std::cout << "Example 11: Deserialization of Variants\n";
        {
            // Variant with int
            std::variant<int, std::string, bool> originalVarInt = 42;
            auto varIntBytes = atom::utils::serialize(originalVarInt);

            size_t varIntOffset = 0;
            auto deserializedVarInt =
                atom::utils::deserializeVariant<int, std::string, bool>(
                    std::span(varIntBytes), varIntOffset);

            std::cout << "Original variant index: " << originalVarInt.index()
                      << std::endl;
            std::cout << "Deserialized variant index: "
                      << deserializedVarInt.index() << std::endl;
            std::cout << "Deserialized variant value (as int): "
                      << std::get<int>(deserializedVarInt) << std::endl;

            // Variant with string
            std::variant<int, std::string, bool> originalVarStr =
                std::string("variant test");
            auto varStrBytes = atom::utils::serialize(originalVarStr);

            size_t varStrOffset = 0;
            auto deserializedVarStr =
                atom::utils::deserializeVariant<int, std::string, bool>(
                    std::span(varStrBytes), varStrOffset);

            std::cout << "Original variant index: " << originalVarStr.index()
                      << std::endl;
            std::cout << "Deserialized variant index: "
                      << deserializedVarStr.index() << std::endl;
            std::cout << "Deserialized variant value (as string): \""
                      << std::get<std::string>(deserializedVarStr) << "\""
                      << std::endl;
        }
        std::cout << std::endl;

        std::cout << "Example 12: Deserialization of Custom Types\n";
        {
            // Point
            Point originalPoint{25, 35};
            auto pointBytes = serialize(originalPoint);

            size_t pointOffset = 0;
            Point deserializedPoint = atom::utils::deserialize<Point>(
                std::span(pointBytes), pointOffset);

            std::cout << "Original Point: (" << originalPoint.x << ", "
                      << originalPoint.y << ")" << std::endl;
            std::cout << "Deserialized Point: (" << deserializedPoint.x << ", "
                      << deserializedPoint.y << ")" << std::endl;

            // Person
            Person originalPerson{"Alice Johnson",
                                  28,
                                  "alice@example.com",
                                  {"music", "cooking", "travel"}};
            auto personBytes = serialize(originalPerson);

            size_t personOffset = 0;
            Person deserializedPerson = atom::utils::deserialize<Person>(
                std::span(personBytes), personOffset);

            std::cout << "Original Person: " << originalPerson.name << ", "
                      << originalPerson.age << " years old" << std::endl;
            std::cout << "  Email: "
                      << (originalPerson.email.has_value()
                              ? *originalPerson.email
                              : "none")
                      << std::endl;
            std::cout << "  Hobbies: ";
            for (size_t i = 0; i < originalPerson.hobbies.size(); ++i) {
                if (i > 0)
                    std::cout << ", ";
                std::cout << originalPerson.hobbies[i];
            }
            std::cout << std::endl;

            std::cout << "Deserialized Person: " << deserializedPerson.name
                      << ", " << deserializedPerson.age << " years old"
                      << std::endl;
            std::cout << "  Email: "
                      << (deserializedPerson.email.has_value()
                              ? *deserializedPerson.email
                              : "none")
                      << std::endl;
            std::cout << "  Hobbies: ";
            for (size_t i = 0; i < deserializedPerson.hobbies.size(); ++i) {
                if (i > 0)
                    std::cout << ", ";
                std::cout << deserializedPerson.hobbies[i];
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;

        std::cout << "Example 13: File I/O with Serialized Data\n";
        {
            // Create complex data structure
            std::map<std::string,
                     std::variant<int, std::string, std::vector<double>>>
                complexData;

            complexData["user_id"] = 12345;
            complexData["username"] = std::string("serialization_master");
            complexData["scores"] = std::vector<double>{98.5, 87.3, 92.8, 95.1};

            // Serialize the complex data
            std::vector<uint8_t> bytes;
            size_t mapSize = complexData.size();
            auto sizeBytes = atom::utils::serialize(mapSize);
            bytes.insert(bytes.end(), sizeBytes.begin(), sizeBytes.end());

            for (const auto& [key, value] : complexData) {
                // Serialize key
                auto keyBytes = atom::utils::serialize(key);
                bytes.insert(bytes.end(), keyBytes.begin(), keyBytes.end());

                // Serialize value type indicator
                size_t type = value.index();
                auto typeBytes = atom::utils::serialize(type);
                bytes.insert(bytes.end(), typeBytes.begin(), typeBytes.end());

                // Serialize value based on type
                std::visit(
                    [&bytes](const auto& v) {
                        auto valueBytes = atom::utils::serialize(v);
                        bytes.insert(bytes.end(), valueBytes.begin(),
                                     valueBytes.end());
                    },
                    value);
            }

            // Save to file
            const std::string filename = "serialization_example.bin";
            try {
                atom::utils::saveToFile(bytes, filename);
                std::cout << "Successfully saved data to " << filename
                          << std::endl;

                // Load from file
                auto loadedBytes = atom::utils::loadFromFile(filename);
                std::cout << "Successfully loaded " << loadedBytes.size()
                          << " bytes from file" << std::endl;

                // Verify data matches
                bool dataMatches = (bytes == loadedBytes);
                std::cout << "Loaded data "
                          << (dataMatches ? "matches" : "does not match")
                          << " original data" << std::endl;

                // Clean up
                std::remove(filename.c_str());
                std::cout << "Removed test file" << std::endl;
            } catch (const atom::utils::SerializationException& e) {
                std::cerr << "File operation failed: " << e.what() << std::endl;
            }
        }
        std::cout << std::endl;

        std::cout << "Example 14: Error Handling\n";
        {
            // Create an incomplete byte array
            std::vector<uint8_t> invalidBytes = {
                0x01, 0x02, 0x03};  // Not enough bytes for an int

            try {
                size_t offset = 0;
                int value = atom::utils::deserialize<int>(
                    std::span(invalidBytes), offset);
                std::cout << "This should not be reached. Value: " << value
                          << std::endl;
            } catch (const atom::utils::SerializationException& e) {
                std::cout << "Expected exception caught: " << e.what()
                          << std::endl;
            }

            // Invalid variant index
            std::vector<uint8_t> invalidVariantBytes = {
                0x05, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00};  // Index 5 (too large)

            try {
                size_t offset = 0;
                auto variant =
                    atom::utils::deserializeVariant<int, std::string, bool>(
                        std::span(invalidVariantBytes), offset);
                std::cout << "This should not be reached. Variant index: "
                          << variant.index() << std::endl;
            } catch (const atom::utils::SerializationException& e) {
                std::cout << "Expected exception caught: " << e.what()
                          << std::endl;
            }

            // Invalid file operations
            try {
                atom::utils::loadFromFile("non_existent_file.bin");
                std::cout << "This should not be reached." << std::endl;
            } catch (const atom::utils::SerializationException& e) {
                std::cout << "Expected exception caught: " << e.what()
                          << std::endl;
            }
        }
        std::cout << std::endl;
    } catch (const atom::utils::SerializationException& e) {
        std::cerr << "Serialization error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}