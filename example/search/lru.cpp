#include "atom/search/lru.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace atom::search;

int main() {
    // Create a ThreadSafeLRUCache with a maximum size of 3
    ThreadSafeLRUCache<std::string, std::string> cache(3);

    // Insert items into the cache
    cache.put("key1", "value1");
    cache.put("key2", "value2");
    cache.put("key3", "value3");

    // Retrieve an item from the cache
    auto value = cache.get("key1");
    if (value) {
        std::cout << "Retrieved value: " << *value << std::endl;
    }

    // Insert another item, causing the least recently used item to be evicted
    cache.put("key4", "value4");

    // Try to retrieve an evicted item
    value = cache.get("key2");
    if (!value) {
        std::cout << "key2 was evicted" << std::endl;
    }

    // Erase an item from the cache
    cache.erase("key3");

    // Clear all items from the cache
    cache.clear();

    // Insert items with a time-to-live (TTL) of 5 seconds
    cache.put("key5", "value5", std::chrono::seconds(5));
    cache.put("key6", "value6", std::chrono::seconds(5));

    // Wait for 6 seconds to let the items expire
    std::this_thread::sleep_for(std::chrono::seconds(6));

    // Try to retrieve an expired item
    value = cache.get("key5");
    if (!value) {
        std::cout << "key5 has expired" << std::endl;
    }

    // Retrieve all keys in the cache
    auto keys = cache.keys();
    std::cout << "Keys in cache: ";
    for (const auto& key : keys) {
        std::cout << key << " ";
    }
    std::cout << std::endl;

    // Insert items again
    cache.put("key7", "value7");
    cache.put("key8", "value8");

    // Pop the least recently used item
    auto lruItem = cache.popLru();
    if (lruItem) {
        std::cout << "Popped LRU item: " << lruItem->first << " -> "
                  << lruItem->second << std::endl;
    }

    // Resize the cache to a new maximum size
    cache.resize(2);

    // Get the current size of the cache
    std::cout << "Cache size: " << cache.size() << std::endl;

    // Get the current load factor of the cache
    std::cout << "Cache load factor: " << cache.loadFactor() << std::endl;

    // Set a callback function to be called when a new item is inserted
    cache.setInsertCallback([](const std::string& key,
                               const std::string& value) {
        std::cout << "Inserted item: " << key << " -> " << value << std::endl;
    });

    // Set a callback function to be called when an item is erased
    cache.setEraseCallback([](const std::string& key) {
        std::cout << "Erased item: " << key << std::endl;
    });

    // Set a callback function to be called when the cache is cleared
    cache.setClearCallback([]() { std::cout << "Cache cleared" << std::endl; });

    // Insert an item to trigger the insert callback
    cache.put("key9", "value9");

    // Erase an item to trigger the erase callback
    cache.erase("key9");

    // Clear the cache to trigger the clear callback
    cache.clear();

    // Get the hit rate of the cache
    std::cout << "Cache hit rate: " << cache.hitRate() << std::endl;

    // Save the cache contents to a file
    cache.put("key10", "value10");
    cache.saveToFile("cache.dat");

    // Load cache contents from a file
    cache.loadFromFile("cache.dat");

    // Retrieve an item to verify it was loaded correctly
    value = cache.get("key10");
    if (value) {
        std::cout << "Loaded value: " << *value << std::endl;
    }

    return 0;
}