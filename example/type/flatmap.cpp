#include "atom/type/flatmap.hpp"

#include <iostream>
#include <string>

using namespace atom::type;

int main() {
    // Create a QuickFlatMap object
    QuickFlatMap<std::string, int> map;

    // Insert a key-value pair into the map
    map.insert({"key1", 100});
    std::cout << "Inserted key1 with value 100" << std::endl;

    // Insert or assign a key-value pair into the map
    map.insertOrAssign("key2", 200);
    std::cout << "Inserted or assigned key2 with value 200" << std::endl;

    // Find a value by key
    auto it = map.find("key1");
    if (it != map.end()) {
        std::cout << "Found key1 with value: " << it->second << std::endl;
    } else {
        std::cout << "Key1 not found" << std::endl;
    }

    // Access a value by key using operator[]
    int value = map["key2"];
    std::cout << "Value of key2: " << value << std::endl;

    // Access a value by key using at()
    try {
        int valueAt = map.at("key1");
        std::cout << "Value of key1: " << valueAt << std::endl;
    } catch (const std::out_of_range& e) {
        std::cout << e.what() << std::endl;
    }

    // Check if the map contains a key
    bool containsKey1 = map.contains("key1");
    std::cout << "Contains key1: " << std::boolalpha << containsKey1
              << std::endl;

    // Erase a key-value pair by key
    bool erased = map.erase("key1");
    std::cout << "Erased key1: " << std::boolalpha << erased << std::endl;

    // Check the size of the map
    std::size_t size = map.size();
    std::cout << "Size of map: " << size << std::endl;

    // Check if the map is empty
    bool isEmpty = map.empty();
    std::cout << "Map is empty: " << std::boolalpha << isEmpty << std::endl;

    // Iterate over the map
    std::cout << "Map contents:" << std::endl;
    for (const auto& [key, value] : map) {
        std::cout << "Key: " << key << ", Value: " << value << std::endl;
    }

    // Create a QuickFlatMultiMap object
    QuickFlatMultiMap<std::string, int> multiMap;

    // Insert multiple values for the same key
    multiMap.insert({"key1", 100});
    multiMap.insert({"key1", 200});
    std::cout << "Inserted multiple values for key1" << std::endl;

    // Find a value by key in the multimap
    auto multiIt = multiMap.find("key1");
    if (multiIt != multiMap.end()) {
        std::cout << "Found key1 with value: " << multiIt->second << std::endl;
    } else {
        std::cout << "Key1 not found in multimap" << std::endl;
    }

    // Access a value by key using operator[]
    int multiValue = multiMap["key2"];
    std::cout << "Value of key2 in multimap: " << multiValue << std::endl;

    // Access a value by key using at()
    try {
        int multiValueAt = multiMap.at("key1");
        std::cout << "Value of key1 in multimap: " << multiValueAt << std::endl;
    } catch (const std::out_of_range& e) {
        std::cout << e.what() << std::endl;
    }

    // Check if the multimap contains a key
    bool multiContainsKey1 = multiMap.contains("key1");
    std::cout << "Multimap contains key1: " << std::boolalpha
              << multiContainsKey1 << std::endl;

    // Erase a key-value pair by key in the multimap
    bool multiErased = multiMap.erase("key1");
    std::cout << "Erased key1 in multimap: " << std::boolalpha << multiErased
              << std::endl;

    // Check the size of the multimap
    std::size_t multiSize = multiMap.size();
    std::cout << "Size of multimap: " << multiSize << std::endl;

    // Check if the multimap is empty
    bool multiIsEmpty = multiMap.empty();
    std::cout << "Multimap is empty: " << std::boolalpha << multiIsEmpty
              << std::endl;

    // Iterate over the multimap
    std::cout << "Multimap contents:" << std::endl;
    for (const auto& [key, value] : multiMap) {
        std::cout << "Key: " << key << ", Value: " << value << std::endl;
    }

    return 0;
}