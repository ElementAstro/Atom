#include "atom/type/auto_table.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace atom::type;

int main() {
    // Create a CountingHashTable object
    CountingHashTable<std::string, int> table;

    // Insert a key-value pair into the hash table
    table.insert("key1", 100);
    std::cout << "Inserted key1 with value 100" << std::endl;

    // Insert multiple key-value pairs into the hash table
    std::vector<std::pair<std::string, int>> items = {{"key2", 200},
                                                      {"key3", 300}};
    table.insertBatch(items);
    std::cout << "Inserted key2 with value 200 and key3 with value 300"
              << std::endl;

    // Retrieve the value associated with a given key
    auto value = table.get("key1");
    if (value) {
        std::cout << "Value of key1: " << *value << std::endl;
    } else {
        std::cout << "Key1 not found" << std::endl;
    }

    // Retrieve the access count for a given key
    auto accessCount = table.getAccessCount("key1");
    if (accessCount) {
        std::cout << "Access count of key1: " << *accessCount << std::endl;
    } else {
        std::cout << "Key1 not found" << std::endl;
    }

    // Retrieve the values associated with multiple keys
    std::vector<std::string> keys = {"key1", "key2", "key4"};
    auto values = table.getBatch(keys);
    for (size_t i = 0; i < keys.size(); ++i) {
        if (values[i]) {
            std::cout << "Value of " << keys[i] << ": " << *values[i]
                      << std::endl;
        } else {
            std::cout << keys[i] << " not found" << std::endl;
        }
    }

    // Erase the entry associated with a given key
    bool erased = table.erase("key1");
    std::cout << "Erased key1: " << std::boolalpha << erased << std::endl;

    // Clear all entries in the hash table
    table.clear();
    std::cout << "Cleared all entries in the hash table" << std::endl;

    // Insert entries again for further operations
    table.insert("key1", 100);
    table.insert("key2", 200);
    table.insert("key3", 300);

    // Retrieve all entries in the hash table
    auto allEntries = table.getAllEntries();
    std::cout << "All entries in the hash table:" << std::endl;
    for (const auto& [key, entryData] : allEntries) {
        std::cout << "Key: " << key << ", Value: " << entryData.value
                  << ", Count: " << entryData.count << std::endl;
    }

    // Sort the entries in the hash table by their access count in descending
    // order
    table.sortEntriesByCountDesc();
    std::cout << "Sorted entries by access count in descending order"
              << std::endl;

    // Retrieve the top N entries with the highest access counts
    auto topEntries = table.getTopNEntries(2);
    std::cout << "Top 2 entries with the highest access counts:" << std::endl;
    for (const auto& [key, entryData] : topEntries) {
        std::cout << "Key: " << key << ", Value: " << entryData.value
                  << ", Count: " << entryData.count << std::endl;
    }

    // Start automatic sorting of the hash table entries at regular intervals
    table.startAutoSorting(std::chrono::milliseconds(5000));
    std::cout << "Started automatic sorting of the hash table entries"
              << std::endl;

    // Stop automatic sorting of the hash table entries
    table.stopAutoSorting();
    std::cout << "Stopped automatic sorting of the hash table entries"
              << std::endl;

    // Serialize the hash table to a JSON object
    json j = table.serializeToJson();
    std::cout << "Serialized hash table to JSON: " << j.dump() << std::endl;

    // Deserialize the hash table from a JSON object
    table.deserializeFromJson(j);
    std::cout << "Deserialized hash table from JSON" << std::endl;

    return 0;
}