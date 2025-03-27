#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "atom/search/ttl.hpp"

using namespace atom::search;
using namespace std::chrono_literals;

// Custom data structure to demonstrate storing complex objects
struct UserProfile {
    int id;
    std::string name;
    std::string email;

    // For comparison in output
    bool operator==(const UserProfile& other) const {
        return id == other.id && name == other.name && email == other.email;
    }
};

// Example struct with move semantics
struct LargeObject {
    std::vector<int> data;

    LargeObject(size_t size) : data(size) {
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<int>(i);
        }
    }
};

// Print cache statistics helper function
void print_stats(const TTLCache<std::string, std::string>& cache) {
    std::cout << "Cache size: " << cache.size() << "/" << cache.capacity()
              << ", Hit rate: " << cache.hitRate() * 100 << "%" << std::endl;
}

int main() {
    try {
        std::cout << "=== TTLCache Basic Usage Examples ===" << std::endl;

        // Create a TTL cache with 5 second expiry and capacity of 10 items
        TTLCache<std::string, std::string> string_cache(5000ms, 10);

        // Basic put operation
        std::cout << "\n--- Basic Operations ---" << std::endl;
        string_cache.put("key1", "value1");
        string_cache.put("key2", "value2");

        // Get operation
        auto value1 = string_cache.get("key1");
        if (value1) {
            std::cout << "Retrieved value: " << *value1 << std::endl;
        }

        // Contains check
        if (string_cache.contains("key2")) {
            std::cout << "Cache contains key2" << std::endl;
        }

        // Non-existent key
        auto value3 = string_cache.get("key3");
        if (!value3) {
            std::cout << "Key3 not found in cache" << std::endl;
        }

        print_stats(string_cache);

        // Remove operation
        std::cout << "\n--- Remove Operation ---" << std::endl;
        bool removed = string_cache.remove("key1");
        std::cout << "Removed key1: " << (removed ? "yes" : "no") << std::endl;

        if (!string_cache.contains("key1")) {
            std::cout << "Key1 no longer in cache after removal" << std::endl;
        }

        // TTL expiration demonstration
        std::cout << "\n--- TTL Expiration ---" << std::endl;
        TTLCache<int, std::string> quick_cache(1000ms, 5);  // 1 second TTL

        quick_cache.put(1, "expires soon");
        std::cout << "Added item with 1 second TTL" << std::endl;

        auto item = quick_cache.get(1);
        if (item) {
            std::cout << "Item available immediately: " << *item << std::endl;
        }

        // Wait for expiration
        std::cout << "Waiting for expiration..." << std::endl;
        std::this_thread::sleep_for(1200ms);

        item = quick_cache.get(1);
        if (!item) {
            std::cout << "Item expired and no longer available" << std::endl;
        }

        // Complex data types
        std::cout << "\n--- Complex Data Types ---" << std::endl;
        TTLCache<int, UserProfile> user_cache(10000ms, 100);

        user_cache.put(101, UserProfile{101, "Alice", "alice@example.com"});
        user_cache.put(102, UserProfile{102, "Bob", "bob@example.com"});

        auto user = user_cache.get(101);
        if (user) {
            std::cout << "Retrieved user: ID=" << user->id
                      << ", Name=" << user->name << ", Email=" << user->email
                      << std::endl;
        }

        // Batch operations
        std::cout << "\n--- Batch Operations ---" << std::endl;
        TTLCache<int, std::string> batch_cache(5000ms, 20);

        // Batch put
        std::vector<std::pair<int, std::string>> items_to_add = {{1, "Item 1"},
                                                                 {2, "Item 2"},
                                                                 {3, "Item 3"},
                                                                 {4, "Item 4"},
                                                                 {5, "Item 5"}};

        batch_cache.batch_put(items_to_add);
        std::cout << "Added " << items_to_add.size() << " items in batch"
                  << std::endl;

        // Batch get
        std::vector<int> keys_to_get = {1, 3, 5, 7};  // 7 doesn't exist
        auto results = batch_cache.batch_get(keys_to_get);

        std::cout << "Batch get results:" << std::endl;
        for (size_t i = 0; i < keys_to_get.size(); ++i) {
            std::cout << "Key " << keys_to_get[i] << ": ";
            if (results[i]) {
                std::cout << *results[i] << std::endl;
            } else {
                std::cout << "not found" << std::endl;
            }
        }

        // LRU Eviction demonstration
        std::cout << "\n--- LRU Eviction ---" << std::endl;
        TTLCache<int, std::string> lru_cache(10000ms, 3);  // Capacity of 3

        lru_cache.put(1, "First");
        lru_cache.put(2, "Second");
        lru_cache.put(3, "Third");

        std::cout << "Added 3 items to cache with capacity 3" << std::endl;

        // Access key 1 to make it most recently used
        lru_cache.get(1);
        std::cout << "Accessed key 1, making it most recently used"
                  << std::endl;

        // Add a new item, causing eviction of least recently used (key 2)
        lru_cache.put(4, "Fourth");
        std::cout << "Added key 4, should evict least recently used item"
                  << std::endl;

        if (!lru_cache.contains(2)) {
            std::cout << "Key 2 was evicted as expected" << std::endl;
        }

        if (lru_cache.contains(1) && lru_cache.contains(3) &&
            lru_cache.contains(4)) {
            std::cout << "Keys 1, 3, and 4 are still in the cache" << std::endl;
        }

        // Resize operation
        std::cout << "\n--- Resize Operation ---" << std::endl;
        lru_cache.resize(5);
        std::cout << "Resized cache from 3 to 5 items" << std::endl;

        lru_cache.put(5, "Fifth");
        lru_cache.put(6, "Sixth");
        std::cout << "Added two more items without eviction" << std::endl;
        std::cout << "Cache size: " << lru_cache.size() << "/"
                  << lru_cache.capacity() << std::endl;

        // Clear operation
        std::cout << "\n--- Clear Operation ---" << std::endl;
        std::cout << "Before clear - cache size: " << lru_cache.size()
                  << std::endl;
        lru_cache.clear();
        std::cout << "After clear - cache size: " << lru_cache.size()
                  << std::endl;

        // Manual cleanup
        std::cout << "\n--- Manual Cleanup ---" << std::endl;
        TTLCache<std::string, int> cleanup_cache(2000ms, 10);

        cleanup_cache.put("item1", 100);
        cleanup_cache.put("item2", 200);

        std::cout << "Added items and waiting for them to expire..."
                  << std::endl;
        std::this_thread::sleep_for(2500ms);

        std::cout << "Before manual cleanup - size: " << cleanup_cache.size()
                  << std::endl;
        cleanup_cache.force_cleanup();
        std::cout << "After manual cleanup - size: " << cleanup_cache.size()
                  << std::endl;

        // Shared pointer access
        std::cout << "\n--- Shared Pointer Access ---" << std::endl;
        TTLCache<int, LargeObject> large_object_cache(5000ms, 5);

        large_object_cache.put(1, LargeObject(10000));
        std::cout << "Added large object to cache" << std::endl;

        auto shared_obj = large_object_cache.get_shared(1);
        if (shared_obj) {
            std::cout << "Retrieved large object as shared_ptr" << std::endl;
            std::cout << "First few values: " << shared_obj->data[0] << ", "
                      << shared_obj->data[1] << ", " << shared_obj->data[2]
                      << std::endl;
        }

        // Move semantics
        std::cout << "\n--- Move Semantics ---" << std::endl;
        TTLCache<int, std::vector<int>> move_cache(5000ms, 5);

        std::vector<int> large_vector(1000, 42);
        std::cout << "Vector size before move: " << large_vector.size()
                  << std::endl;

        // Move the vector into the cache
        move_cache.put(1, std::move(large_vector));
        std::cout << "Vector size after move: " << large_vector.size()
                  << std::endl;

        // Cache-to-cache move constructor
        std::cout << "\n--- Move Constructor ---" << std::endl;
        TTLCache<std::string, std::string> src_cache(5000ms, 5);
        src_cache.put("key1", "value1");
        src_cache.put("key2", "value2");

        std::cout << "Source cache size before move: " << src_cache.size()
                  << std::endl;

        // Move construct a new cache
        TTLCache<std::string, std::string> dst_cache(std::move(src_cache));

        std::cout << "Destination cache size after move: " << dst_cache.size()
                  << std::endl;

        auto moved_value = dst_cache.get("key1");
        if (moved_value) {
            std::cout << "Successfully retrieved value from moved cache: "
                      << *moved_value << std::endl;
        }

        std::cout << "\n=== All examples completed successfully ==="
                  << std::endl;

    } catch (const TTLCacheException& e) {
        std::cerr << "TTLCache error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << std::endl;
    }

    return 0;
}