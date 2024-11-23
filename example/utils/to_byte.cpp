#include "atom/utils/to_byte.hpp"

#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <variant>
#include <vector>

using namespace atom::utils;

int main() {
    // Serialize an integer
    int intValue = 42;
    std::vector<uint8_t> intBytes = serialize(intValue);
    std::cout << "Serialized integer: ";
    for (auto byte : intBytes) {
        std::cout << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;

    // Serialize a string
    std::string strValue = "Hello, World!";
    std::vector<uint8_t> strBytes = serialize(strValue);
    std::cout << "Serialized string: ";
    for (auto byte : strBytes) {
        std::cout << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;

    // Serialize a vector of integers
    std::vector<int> vecValue = {1, 2, 3, 4, 5};
    std::vector<uint8_t> vecBytes = serialize(vecValue);
    std::cout << "Serialized vector: ";
    for (auto byte : vecBytes) {
        std::cout << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;

    // Serialize a list of strings
    std::list<std::string> listValue = {"one", "two", "three"};
    std::vector<uint8_t> listBytes = serialize(listValue);
    std::cout << "Serialized list: ";
    for (auto byte : listBytes) {
        std::cout << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;

    // Serialize a map of string to integer
    std::map<std::string, int> mapValue = {{"key1", 1}, {"key2", 2}};
    std::vector<uint8_t> mapBytes = serialize(mapValue);
    std::cout << "Serialized map: ";
    for (auto byte : mapBytes) {
        std::cout << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;

    // Serialize an optional integer
    std::optional<int> optValue = 123;
    std::vector<uint8_t> optBytes = serialize(optValue);
    std::cout << "Serialized optional: ";
    for (auto byte : optBytes) {
        std::cout << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;

    // Serialize a variant of int and string
    std::variant<int, std::string> varValue = "variant string";
    std::vector<uint8_t> varBytes = serialize(varValue);
    std::cout << "Serialized variant: ";
    for (auto byte : varBytes) {
        std::cout << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;

    // Deserialize an integer
    size_t offset = 0;
    int deserializedInt = deserialize<int>(intBytes, offset);
    std::cout << "Deserialized integer: " << deserializedInt << std::endl;

    // Deserialize a string
    offset = 0;
    std::string deserializedStr = deserializeString(strBytes, offset);
    std::cout << "Deserialized string: " << deserializedStr << std::endl;

    // Deserialize a vector of integers
    offset = 0;
    std::vector<int> deserializedVec = deserializeVector<int>(vecBytes, offset);
    std::cout << "Deserialized vector: ";
    for (auto val : deserializedVec) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Deserialize a list of strings
    offset = 0;
    std::list<std::string> deserializedList =
        deserializeList<std::string>(listBytes, offset);
    std::cout << "Deserialized list: ";
    for (const auto& val : deserializedList) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Deserialize a map of string to integer
    offset = 0;
    std::map<std::string, int> deserializedMap =
        deserializeMap<std::string, int>(mapBytes, offset);
    std::cout << "Deserialized map: ";
    for (const auto& [key, val] : deserializedMap) {
        std::cout << "{" << key << ": " << val << "} ";
    }
    std::cout << std::endl;

    // Deserialize an optional integer
    offset = 0;
    std::optional<int> deserializedOpt =
        deserializeOptional<int>(optBytes, offset);
    if (deserializedOpt) {
        std::cout << "Deserialized optional: " << *deserializedOpt << std::endl;
    } else {
        std::cout << "Deserialized optional: nullopt" << std::endl;
    }

    // Deserialize a variant of int and string
    offset = 0;
    std::variant<int, std::string> deserializedVar =
        deserializeVariant<int, std::string>(varBytes, offset);
    if (std::holds_alternative<int>(deserializedVar)) {
        std::cout << "Deserialized variant (int): "
                  << std::get<int>(deserializedVar) << std::endl;
    } else if (std::holds_alternative<std::string>(deserializedVar)) {
        std::cout << "Deserialized variant (string): "
                  << std::get<std::string>(deserializedVar) << std::endl;
    }

    // Save serialized data to a file
    saveToFile(intBytes, "int_data.bin");

    // Load serialized data from a file
    std::vector<uint8_t> loadedData = loadFromFile("int_data.bin");
    std::cout << "Loaded data: ";
    for (auto byte : loadedData) {
        std::cout << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;

    return 0;
}