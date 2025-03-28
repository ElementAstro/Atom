/*
 * serialization_example.cpp
 *
 * Copyright (C) 2024 Max Q.
 *
 * Comprehensive example of the atom::utils serialization functionality
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
    Point location;
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

    // Serialize location
    auto locationBytes = serialize(person.location);
    bytes.insert(bytes.end(), locationBytes.begin(), locationBytes.end());

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
    person.location = deserialize<Point>(bytes, offset);
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
            {
                // 8-bit integers
                int8_t int8Value = 42;
                auto int8Bytes = atom::utils::serialize(int8Value);
                printBytes(int8Bytes, "Serialized int8_t (42)");

                uint8_t uint8Value = 200;
                auto uint8Bytes = atom::utils::serialize(uint8Value);
                printBytes(uint8Bytes, "Serialized uint8_t (200)");

                // 16-bit integers
                int16_t int16Value = 12345;
                auto int16Bytes = atom::utils::serialize(int16Value);
                printBytes(int16Bytes, "Serialized int16_t (12345)");

                uint16_t uint16Value = 60000;
                auto uint16Bytes = atom::utils::serialize(uint16Value);
                printBytes(uint16Bytes, "Serialized uint16_t (60000)");

                // 32-bit integers
                int32_t int32Value = 1234567890;
                auto int32Bytes = atom::utils::serialize(int32Value);
                printBytes(int32Bytes, "Serialized int32_t (1234567890)");

                uint32_t uint32Value = 3000000000;
                auto uint32Bytes = atom::utils::serialize(uint32Value);
                printBytes(uint32Bytes, "Serialized uint32_t (3000000000)");

                // 64-bit integers
                int64_t int64Value = 1234567890123456789LL;
                auto int64Bytes = atom::utils::serialize(int64Value);
                printBytes(int64Bytes,
                           "Serialized int64_t (1234567890123456789)");

                uint64_t uint64Value = 18446744073709551000ULL;
                auto uint64Bytes = atom::utils::serialize(uint64Value);
                printBytes(uint64Bytes,
                           "Serialized uint64_t (18446744073709551000)");
            }

            // Floating point serialization
            {
                float floatValue = 3.14159f;
                auto floatBytes = atom::utils::serialize(floatValue);
                printBytes(floatBytes, "Serialized float (3.14159)");

                double doubleValue = 2.71828182845;
                auto doubleBytes = atom::utils::serialize(doubleValue);
                printBytes(doubleBytes, "Serialized double (2.71828182845)");

                // Special floating point values
                float nanValue = std::numeric_limits<float>::quiet_NaN();
                auto nanBytes = atom::utils::serialize(nanValue);
                printBytes(nanBytes, "Serialized NaN");

                float infValue = std::numeric_limits<float>::infinity();
                auto infBytes = atom::utils::serialize(infValue);
                printBytes(infBytes, "Serialized infinity");

                float negInfValue = -std::numeric_limits<float>::infinity();
                auto negInfBytes = atom::utils::serialize(negInfValue);
                printBytes(negInfBytes, "Serialized negative infinity");
            }

            // Boolean serialization
            {
                bool trueValue = true;
                auto trueBytes = atom::utils::serialize(trueValue);
                printBytes(trueBytes, "Serialized bool (true)");

                bool falseValue = false;
                auto falseBytes = atom::utils::serialize(falseValue);
                printBytes(falseBytes, "Serialized bool (false)");
            }

            // Character serialization
            {
                char charValue = 'A';
                auto charBytes = atom::utils::serialize(charValue);
                printBytes(charBytes, "Serialized char ('A')");

                wchar_t wideCharValue = L'Ω';
                auto wideCharBytes = atom::utils::serialize(wideCharValue);
                printBytes(wideCharBytes, "Serialized wchar_t ('Ω')");

                char16_t char16Value = u'✓';
                auto char16Bytes = atom::utils::serialize(char16Value);
                printBytes(char16Bytes, "Serialized char16_t ('✓')");
            }

            // Enum serialization
            {
                enum class SimpleEnum { First, Second, Third };
                SimpleEnum enumValue = SimpleEnum::Second;
                auto enumBytes = atom::utils::serialize(enumValue);
                printBytes(enumBytes, "Serialized enum (SimpleEnum::Second)");

                MessageType msgType = MessageType::Text;
                auto msgTypeBytes = atom::utils::serialize(msgType);
                printBytes(msgTypeBytes,
                           "Serialized MessageType (MessageType::Text)");
            }
        }
        std::cout << std::endl;

        std::cout << "Example 2: Serializing Strings\n";
        {
            // Basic string
            std::string basicString = "Hello, World!";
            auto basicStringBytes = atom::utils::serialize(basicString);
            printBytes(basicStringBytes,
                       "Serialized string (\"Hello, World!\")");

            // Empty string
            std::string emptyString = "";
            auto emptyStringBytes = atom::utils::serialize(emptyString);
            printBytes(emptyStringBytes, "Serialized empty string");

            // String with special characters
            std::string specialString = "Special chars: !@#$%^&*()_+";
            auto specialStringBytes = atom::utils::serialize(specialString);
            printBytes(specialStringBytes,
                       "Serialized string with special characters");

            // Unicode string
            std::string unicodeString =
                "こんにちは世界";  // "Hello World" in Japanese
            auto unicodeBytes = atom::utils::serialize(unicodeString);
            printBytes(unicodeBytes, "Serialized Unicode string");

            // Very long string (showing truncated output)
            std::string longString(1000, 'A');  // 1000 'A' characters
            auto longStringBytes = atom::utils::serialize(longString);
            printBytes(longStringBytes,
                       "Serialized long string (1000 characters)");
        }
        std::cout << std::endl;

        std::cout << "Example 3: Serializing Containers\n";
        {
            // Vector serialization
            {
                // Vector of integers
                std::vector<int> intVector = {1, 2, 3, 4, 5};
                auto intVectorBytes = atom::utils::serialize(intVector);
                printBytes(intVectorBytes,
                           "Serialized vector<int> ({1,2,3,4,5})");

                // Empty vector
                std::vector<int> emptyVector;
                auto emptyVectorBytes = atom::utils::serialize(emptyVector);
                printBytes(emptyVectorBytes, "Serialized empty vector<int>");

                // Vector of floats
                std::vector<float> floatVector = {1.1f, 2.2f, 3.3f, 4.4f};
                auto floatVectorBytes = atom::utils::serialize(floatVector);
                printBytes(floatVectorBytes, "Serialized vector<float>");

                // Vector of strings
                std::vector<std::string> stringVector = {"apple", "banana",
                                                         "cherry"};
                auto stringVectorBytes = atom::utils::serialize(stringVector);
                printBytes(stringVectorBytes, "Serialized vector<string>");

                // Nested vector
                std::vector<std::vector<int>> nestedVector = {
                    {1, 2}, {3, 4}, {5, 6}};
                auto nestedVectorBytes = atom::utils::serialize(nestedVector);
                printBytes(nestedVectorBytes, "Serialized vector<vector<int>>");
            }

            // List serialization
            {
                // List of integers
                std::list<int> intList = {10, 20, 30, 40};
                auto intListBytes = atom::utils::serialize(intList);
                printBytes(intListBytes,
                           "Serialized list<int> ({10,20,30,40})");

                // List of floats
                std::list<float> floatList = {1.1f, 2.2f, 3.3f};
                auto floatListBytes = atom::utils::serialize(floatList);
                printBytes(floatListBytes,
                           "Serialized list<float> ({1.1,2.2,3.3})");

                // Empty list
                std::list<double> emptyList;
                auto emptyListBytes = atom::utils::serialize(emptyList);
                printBytes(emptyListBytes, "Serialized empty list<double>");
            }

            // Map serialization
            {
                // Map of string to int
                std::map<std::string, int> stringIntMap = {
                    {"one", 1}, {"two", 2}, {"three", 3}};
                auto stringIntMapBytes = atom::utils::serialize(stringIntMap);
                printBytes(stringIntMapBytes, "Serialized map<string,int>");

                // Map of int to string
                std::map<int, std::string> intStringMap = {
                    {1, "one"}, {2, "two"}, {3, "three"}};
                auto intStringMapBytes = atom::utils::serialize(intStringMap);
                printBytes(intStringMapBytes, "Serialized map<int,string>");

                // Empty map
                std::map<char, bool> emptyMap;
                auto emptyMapBytes = atom::utils::serialize(emptyMap);
                printBytes(emptyMapBytes, "Serialized empty map");

                // Map with custom type values
                std::map<std::string, Point> pointMap = {{"origin", {0, 0}},
                                                         {"point1", {10, 20}},
                                                         {"point2", {-5, 15}}};

                std::vector<uint8_t> pointMapBytes;
                size_t mapSize = pointMap.size();
                auto sizeBytes = atom::utils::serialize(mapSize);
                pointMapBytes.insert(pointMapBytes.end(), sizeBytes.begin(),
                                     sizeBytes.end());

                for (const auto& [key, value] : pointMap) {
                    auto keyBytes = atom::utils::serialize(key);
                    auto valueBytes = serialize(value);
                    pointMapBytes.insert(pointMapBytes.end(), keyBytes.begin(),
                                         keyBytes.end());
                    pointMapBytes.insert(pointMapBytes.end(),
                                         valueBytes.begin(), valueBytes.end());
                }

                printBytes(pointMapBytes, "Serialized map<string,Point>");
            }
        }
        std::cout << std::endl;

        std::cout << "Example 4: Serializing Optional Values\n";
        {
            // Optional with integer value
            std::optional<int> optWithIntValue = 42;
            auto optWithIntValueBytes = atom::utils::serialize(optWithIntValue);
            printBytes(optWithIntValueBytes,
                       "Serialized optional<int> with value");

            // Optional without value
            std::optional<int> optWithoutValue = std::nullopt;
            auto optWithoutValueBytes = atom::utils::serialize(optWithoutValue);
            printBytes(optWithoutValueBytes,
                       "Serialized optional<int> without value");

            // Optional with string value
            std::optional<std::string> optStringWithValue = "optional string";
            auto optStringBytes = atom::utils::serialize(optStringWithValue);
            printBytes(optStringBytes,
                       "Serialized optional<string> with value");

            // Optional with custom type
            std::optional<Point> optPointWithValue = Point{15, 25};

            std::vector<uint8_t> optPointBytes;
            bool hasValue = optPointWithValue.has_value();
            auto hasValueBytes = atom::utils::serialize(hasValue);
            optPointBytes.insert(optPointBytes.end(), hasValueBytes.begin(),
                                 hasValueBytes.end());

            if (hasValue) {
                auto valueBytes = serialize(optPointWithValue.value());
                optPointBytes.insert(optPointBytes.end(), valueBytes.begin(),
                                     valueBytes.end());
            }

            printBytes(optPointBytes, "Serialized optional<Point> with value");
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

            // Variant with custom type
            std::variant<int, std::string, Point> varPoint = Point{5, 10};

            std::vector<uint8_t> varPointBytes;
            size_t index = varPoint.index();
            auto indexBytes = atom::utils::serialize(index);
            varPointBytes.insert(varPointBytes.end(), indexBytes.begin(),
                                 indexBytes.end());

            std::visit(
                [&varPointBytes](const auto& value) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(value)>,
                                                 Point>) {
                        auto valueBytes = serialize(value);
                        varPointBytes.insert(varPointBytes.end(),
                                             valueBytes.begin(),
                                             valueBytes.end());
                    } else {
                        auto valueBytes = atom::utils::serialize(value);
                        varPointBytes.insert(varPointBytes.end(),
                                             valueBytes.begin(),
                                             valueBytes.end());
                    }
                },
                varPoint);

            printBytes(varPointBytes,
                       "Serialized variant<int,string,Point> with Point");
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
                "John Doe",                            // name
                30,                                    // age
                "john.doe@example.com",                // email (with value)
                {"reading", "hiking", "programming"},  // hobbies
                {100, 200}                             // location
            };
            auto personBytes = serialize(person);
            printBytes(personBytes, "Serialized Person");

            // Person with no email
            Person personNoEmail{"Jane Smith",
                                 25,
                                 std::nullopt,
                                 {"painting", "cycling"},
                                 {50, 150}};
            auto personNoEmailBytes = serialize(personNoEmail);
            printBytes(personNoEmailBytes, "Serialized Person with no email");

            // Create a more complex custom structure
            struct DataRecord {
                std::string id;
                std::map<std::string, std::variant<int, double, std::string>>
                    attributes;
                std::optional<Person> owner;
                std::vector<Point> points;
            };

            // Custom serialization for DataRecord
            auto serializeDataRecord =
                [](const DataRecord& record) -> std::vector<uint8_t> {
                std::vector<uint8_t> bytes;

                // Serialize ID
                auto idBytes = atom::utils::serialize(record.id);
                bytes.insert(bytes.end(), idBytes.begin(), idBytes.end());

                // Serialize attributes map size
                size_t attrSize = record.attributes.size();
                auto attrSizeBytes = atom::utils::serialize(attrSize);
                bytes.insert(bytes.end(), attrSizeBytes.begin(),
                             attrSizeBytes.end());

                // Serialize each attribute
                for (const auto& [key, value] : record.attributes) {
                    // Serialize key
                    auto keyBytes = atom::utils::serialize(key);
                    bytes.insert(bytes.end(), keyBytes.begin(), keyBytes.end());

                    // Serialize variant index
                    size_t varIndex = value.index();
                    auto indexBytes = atom::utils::serialize(varIndex);
                    bytes.insert(bytes.end(), indexBytes.begin(),
                                 indexBytes.end());

                    // Serialize value based on type
                    std::visit(
                        [&bytes](const auto& v) {
                            auto valueBytes = atom::utils::serialize(v);
                            bytes.insert(bytes.end(), valueBytes.begin(),
                                         valueBytes.end());
                        },
                        value);
                }

                // Serialize optional owner
                bool hasOwner = record.owner.has_value();
                auto hasOwnerBytes = atom::utils::serialize(hasOwner);
                bytes.insert(bytes.end(), hasOwnerBytes.begin(),
                             hasOwnerBytes.end());

                if (hasOwner) {
                    auto ownerBytes = serialize(record.owner.value());
                    bytes.insert(bytes.end(), ownerBytes.begin(),
                                 ownerBytes.end());
                }

                // Serialize points vector
                size_t pointsSize = record.points.size();
                auto pointsSizeBytes = atom::utils::serialize(pointsSize);
                bytes.insert(bytes.end(), pointsSizeBytes.begin(),
                             pointsSizeBytes.end());

                for (const auto& p : record.points) {
                    auto pointBytes = serialize(p);
                    bytes.insert(bytes.end(), pointBytes.begin(),
                                 pointBytes.end());
                }

                return bytes;
            };

            // Create a sample record
            DataRecord record{
                "REC-12345",
                {{"count", 42},
                 {"ratio", 0.75},
                 {"description", std::string("Test record")}},
                Person{
                    "Record Owner", 35, "owner@example.com", {"admin"}, {0, 0}},
                {{1, 1}, {2, 2}, {3, 3}}};

            auto recordBytes = serializeDataRecord(record);
            printBytes(recordBytes, "Serialized complex DataRecord");
        }
        std::cout << std::endl;

        std::cout << "Example 7: Deserialization of Basic Types\n";
        {
            // Create serialized data
            int32_t originalInt = 42;
            float originalFloat = 3.14159f;
            bool originalBool = true;
            char originalChar = 'X';

            auto intBytes = atom::utils::serialize(originalInt);
            auto floatBytes = atom::utils::serialize(originalFloat);
            auto boolBytes = atom::utils::serialize(originalBool);
            auto charBytes = atom::utils::serialize(originalChar);

            // Deserialize
            size_t intOffset = 0;
            size_t floatOffset = 0;
            size_t boolOffset = 0;
            size_t charOffset = 0;

            int32_t deserializedInt = atom::utils::deserialize<int32_t>(
                std::span(intBytes), intOffset);
            float deserializedFloat = atom::utils::deserialize<float>(
                std::span(floatBytes), floatOffset);
            bool deserializedBool = atom::utils::deserialize<bool>(
                std::span(boolBytes), boolOffset);
            char deserializedChar = atom::utils::deserialize<char>(
                std::span(charBytes), charOffset);

            std::cout << "Original int32_t: " << originalInt
                      << ", Deserialized: " << deserializedInt << std::endl;
            std::cout << "Original float: " << originalFloat
                      << ", Deserialized: " << deserializedFloat << std::endl;
            std::cout << "Original bool: " << (originalBool ? "true" : "false")
                      << ", Deserialized: "
                      << (deserializedBool ? "true" : "false") << std::endl;
            std::cout << "Original char: '" << originalChar
                      << "', Deserialized: '" << deserializedChar << "'"
                      << std::endl;

            // Deserialize enum
            MessageType originalEnum = MessageType::Text;
            auto enumBytes = atom::utils::serialize(originalEnum);

            size_t enumOffset = 0;
            MessageType deserializedEnum =
                atom::utils::deserialize<MessageType>(std::span(enumBytes),
                                                      enumOffset);

            std::cout
                << "Original enum: MessageType::Text (0), Deserialized value: "
                << static_cast<int>(deserializedEnum) << std::endl;
        }
        std::cout << std::endl;

        std::cout << "Example 8: Deserialization of Strings\n";
        {
            // Serialize various strings
            std::string originalString = "Hello, Serialization!";
            std::string originalEmptyString = "";
            std::string originalUnicodeString =
                "Привет мир";  // "Hello world" in Russian

            auto stringBytes = atom::utils::serialize(originalString);
            auto emptyStringBytes = atom::utils::serialize(originalEmptyString);
            auto unicodeStringBytes =
                atom::utils::serialize(originalUnicodeString);

            // Deserialize
            size_t stringOffset = 0;
            size_t emptyOffset = 0;
            size_t unicodeOffset = 0;

            std::string deserializedString = atom::utils::deserializeString(
                std::span(stringBytes), stringOffset);
            std::string deserializedEmptyString =
                atom::utils::deserializeString(std::span(emptyStringBytes),
                                               emptyOffset);
            std::string deserializedUnicodeString =
                atom::utils::deserializeString(std::span(unicodeStringBytes),
                                               unicodeOffset);

            std::cout << "Original string: \"" << originalString << "\""
                      << std::endl;
            std::cout << "Deserialized string: \"" << deserializedString << "\""
                      << std::endl;

            std::cout << "Original empty string length: "
                      << originalEmptyString.length() << std::endl;
            std::cout << "Deserialized empty string length: "
                      << deserializedEmptyString.length() << std::endl;

            std::cout << "Original Unicode string: \"" << originalUnicodeString
                      << "\"" << std::endl;
            std::cout << "Deserialized Unicode string: \""
                      << deserializedUnicodeString << "\"" << std::endl;
        }
        std::cout << std::endl;

        std::cout << "Example 9: Deserialization of Containers\n";
        {
            // Vector deserialization
            {
                // Vector of integers
                std::vector<int> originalVector = {5, 10, 15, 20, 25};
                auto vectorBytes = atom::utils::serialize(originalVector);

                size_t vecOffset = 0;
                auto deserializedVector = atom::utils::deserializeVector<int>(
                    std::span(vectorBytes), vecOffset);

                std::cout << "Original vector<int>: ";
                for (const auto& val : originalVector)
                    std::cout << val << " ";
                std::cout << std::endl;

                std::cout << "Deserialized vector<int>: ";
                for (const auto& val : deserializedVector)
                    std::cout << val << " ";
                std::cout << std::endl;

                // Empty vector
                std::vector<double> originalEmptyVector;
                auto emptyVectorBytes =
                    atom::utils::serialize(originalEmptyVector);

                size_t emptyVecOffset = 0;
                auto deserializedEmptyVector =
                    atom::utils::deserializeVector<double>(
                        std::span(emptyVectorBytes), emptyVecOffset);

                std::cout << "Original empty vector size: "
                          << originalEmptyVector.size() << std::endl;
                std::cout << "Deserialized empty vector size: "
                          << deserializedEmptyVector.size() << std::endl;

                // Vector of strings
                std::vector<std::string> originalStringVector = {
                    "first", "second", "third"};
                auto stringVectorBytes =
                    atom::utils::serialize(originalStringVector);

                size_t stringVecOffset = 0;
                auto deserializedStringVector =
                    atom::utils::deserializeVector<std::string>(
                        std::span(stringVectorBytes), stringVecOffset);

                std::cout << "Original vector<string>: ";
                for (const auto& val : originalStringVector)
                    std::cout << "\"" << val << "\" ";
                std::cout << std::endl;

                std::cout << "Deserialized vector<string>: ";
                for (const auto& val : deserializedStringVector)
                    std::cout << "\"" << val << "\" ";
                std::cout << std::endl;
            }

            // List deserialization
            {
                // List of doubles
                std::list<double> originalList = {1.1, 2.2, 3.3, 4.4};
                auto listBytes = atom::utils::serialize(originalList);

                size_t listOffset = 0;
                auto deserializedList = atom::utils::deserializeList<double>(
                    std::span(listBytes), listOffset);

                std::cout << "Original list<double>: ";
                for (const auto& val : originalList)
                    std::cout << val << " ";
                std::cout << std::endl;

                std::cout << "Deserialized list<double>: ";
                for (const auto& val : deserializedList)
                    std::cout << val << " ";
                std::cout << std::endl;
            }

            // Map deserialization
            {
                // Map with string keys and int values
                std::map<std::string, int> originalMap = {
                    {"first", 1}, {"second", 2}, {"third", 3}};
                auto mapBytes = atom::utils::serialize(originalMap);

                size_t mapOffset = 0;
                auto deserializedMap =
                    atom::utils::deserializeMap<std::string, int>(
                        std::span(mapBytes), mapOffset);

                std::cout << "Original map<string,int>: ";
                for (auto it = originalMap.begin(); it != originalMap.end();
                     ++it) {
                    if (it != originalMap.begin())
                        std::cout << ", ";
                    std::cout << "\"" << it->first << "\": " << it->second;
                }
                std::cout << std::endl;

                std::cout << "Deserialized map<string,int>: ";
                for (auto it = deserializedMap.begin();
                     it != deserializedMap.end(); ++it) {
                    if (it != deserializedMap.begin())
                        std::cout << ", ";
                    std::cout << "\"" << it->first << "\": " << it->second;
                }
                std::cout << std::endl;

                // Map with int keys and string values
                std::map<int, std::string> originalIntStringMap = {
                    {1, "one"}, {2, "two"}, {3, "three"}};
                auto intStringMapBytes =
                    atom::utils::serialize(originalIntStringMap);

                size_t intStringMapOffset = 0;
                auto deserializedIntStringMap =
                    atom::utils::deserializeMap<int, std::string>(
                        std::span(intStringMapBytes), intStringMapOffset);

                std::cout << "Original map<int,string>: ";
                for (auto it = originalIntStringMap.begin();
                     it != originalIntStringMap.end(); ++it) {
                    if (it != originalIntStringMap.begin())
                        std::cout << ", ";
                    std::cout << it->first << ": \"" << it->second << "\"";
                }
                std::cout << std::endl;

                std::cout << "Deserialized map<int,string>: ";
                for (auto it = deserializedIntStringMap.begin();
                     it != deserializedIntStringMap.end(); ++it) {
                    if (it != deserializedIntStringMap.begin())
                        std::cout << ", ";
                    std::cout << it->first << ": \"" << it->second << "\"";
                }
                std::cout << std::endl;
            }
        }
        std::cout << std::endl;

        std::cout << "Example 10: Deserialization of Optional Values\n";
        {
            // Optional with int value
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

            // Optional with string value
            std::optional<std::string> originalOptString =
                "optional string test";
            auto optStringBytes = atom::utils::serialize(originalOptString);

            size_t optStringOffset = 0;
            auto deserializedOptString =
                atom::utils::deserializeOptional<std::string>(
                    std::span(optStringBytes), optStringOffset);

            std::cout << "Original optional<string> with value: "
                      << (originalOptString.has_value()
                              ? "\"" + *originalOptString + "\""
                              : "nullopt")
                      << std::endl;

            std::cout << "Deserialized optional<string> with value: "
                      << (deserializedOptString.has_value()
                              ? "\"" + *deserializedOptString + "\""
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

            // Variant with bool
            std::variant<int, std::string, bool> originalVarBool = false;
            auto varBoolBytes = atom::utils::serialize(originalVarBool);

            size_t varBoolOffset = 0;
            auto deserializedVarBool =
                atom::utils::deserializeVariant<int, std::string, bool>(
                    std::span(varBoolBytes), varBoolOffset);

            std::cout << "Original variant index: " << originalVarBool.index()
                      << std::endl;
            std::cout << "Deserialized variant index: "
                      << deserializedVarBool.index() << std::endl;
            std::cout << "Deserialized variant value (as bool): "
                      << (std::get<bool>(deserializedVarBool) ? "true"
                                                              : "false")
                      << std::endl;
        }
        std::cout << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
