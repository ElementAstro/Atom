#include "atom/search/ttl.hpp"

#include <iostream>
#include <thread>

using namespace atom::search;

int main() {
    // Create a TTLCache with a TTL of 5 seconds and a maximum capacity of 3
    TTLCache<std::string, std::string> cache(std::chrono::seconds(5), 3);

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

    // Wait for 6 seconds to let the items expire
    std::this_thread::sleep_for(std::chrono::seconds(6));

    // Try to retrieve an expired item
    value = cache.get("key1");
    if (!value) {
        std::cout << "key1 has expired" << std::endl;
    }

    // Perform cache cleanup by removing expired items
    cache.cleanup();

    // Get the cache hit rate
    std::cout << "Cache hit rate: " << cache.hitRate() << std::endl;

    // Get the current number of items in the cache
    std::cout << "Cache size: " << cache.size() << std::endl;

    // Clear all items from the cache
    cache.clear();
    std::cout << "Cache cleared" << std::endl;

    // Verify the cache is empty
    std::cout << "Cache size after clear: " << cache.size() << std::endl;

    return 0;
}