#include <iostream>
#include "atom/search/cache.hpp"

int main() {
    using namespace atom::search;

    // Create a cache with max size 3
    ResourceCache<int> cache(3);

    std::cout << "Initial cache size: " << cache.size() << std::endl;

    // Insert 3 items
    cache.insert("lru_key1", 1, std::chrono::seconds(100));
    std::cout << "After inserting lru_key1, size: " << cache.size() << std::endl;
    std::cout << "Contains lru_key1: " << cache.contains("lru_key1") << std::endl;

    cache.insert("lru_key2", 2, std::chrono::seconds(100));
    std::cout << "After inserting lru_key2, size: " << cache.size() << std::endl;
    std::cout << "Contains lru_key1: " << cache.contains("lru_key1") << std::endl;
    std::cout << "Contains lru_key2: " << cache.contains("lru_key2") << std::endl;

    cache.insert("lru_key3", 3, std::chrono::seconds(100));
    std::cout << "After inserting lru_key3, size: " << cache.size() << std::endl;
    std::cout << "Contains lru_key1: " << cache.contains("lru_key1") << std::endl;
    std::cout << "Contains lru_key2: " << cache.contains("lru_key2") << std::endl;
    std::cout << "Contains lru_key3: " << cache.contains("lru_key3") << std::endl;

    // Access lru_key1 - this should move it to the front
    auto val = cache.get("lru_key1");
    std::cout << "After accessing lru_key1, got value: " << (val ? *val : -1) << std::endl;

    // Insert a new item - this should evict lru_key2 (oldest after lru_key1 was accessed)
    cache.insert("lru_key4", 4, std::chrono::seconds(100));
    std::cout << "After inserting lru_key4, size: " << cache.size() << std::endl;
    std::cout << "Contains lru_key1: " << cache.contains("lru_key1") << std::endl;
    std::cout << "Contains lru_key2: " << cache.contains("lru_key2") << std::endl;
    std::cout << "Contains lru_key3: " << cache.contains("lru_key3") << std::endl;
    std::cout << "Contains lru_key4: " << cache.contains("lru_key4") << std::endl;

    return 0;
}
