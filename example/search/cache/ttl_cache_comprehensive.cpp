/**
 * @file ttl_cache_comprehensive.cpp
 * @brief Comprehensive example demonstrating all features of TTLCache
 * @author Atom Search Examples
 * @date 2025-01-25
 *
 * This example demonstrates:
 * - Basic TTL cache operations (put, get, remove)
 * - TTL expiration and automatic cleanup
 * - LRU eviction when cache reaches capacity
 * - Batch operations for efficient handling
 * - Cache statistics and performance monitoring
 * - Advanced configuration options
 * - Thread safety and concurrent access
 * - Move semantics and performance optimization
 * - Error handling and edge cases
 */

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "atom/search/ttl.hpp"

using namespace atom::search;
using namespace std::chrono_literals;

// Helper function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// Custom data structure to demonstrate storing complex objects
struct UserProfile {
    int id;
    std::string name;
    std::string email;

    // For comparison in output
    bool operator==(const UserProfile& other) const {
        return id == other.id && name == other.name && email == other.email;
    }

    std::string toString() const {
        return "UserProfile{id=" + std::to_string(id) + ", name='" + name +
               "', email='" + email + "'}";
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
template <typename Key, typename Value>
void print_stats(const TTLCache<Key, Value>& cache) {
    auto stats = cache.get_statistics();
    std::cout << "Cache Statistics:" << std::endl;
    std::cout << "  Size: " << stats.current_size << "/" << stats.max_capacity
              << std::endl;
    std::cout << "  Hits: " << stats.hits << ", Misses: " << stats.misses
              << std::endl;
    std::cout << "  Hit rate: " << (stats.hit_rate * 100) << "%" << std::endl;
    std::cout << "  Evictions: " << stats.evictions
              << ", Expirations: " << stats.expirations << std::endl;
}

int main() {
    try {
        std::cout << "=== TTLCache Comprehensive Examples ===" << std::endl;

        //----------------------------------------------------------------------
        // 1. Basic TTL Cache Operations
        //----------------------------------------------------------------------
        printSection("1. Basic TTL Cache Operations");

        // Create a TTL cache with 5 second expiry and capacity of 10 items
        TTLCache<std::string, std::string> string_cache(5000ms, 10);
        std::cout << "Created TTLCache with 5-second TTL and capacity of 10"
                  << std::endl;

        // Basic put operation
        std::cout << "\n--- Basic Put/Get Operations ---" << std::endl;
        string_cache.put("key1", "value1");
        string_cache.put("key2", "value2");
        string_cache.put("key3", "value3");

        // Get operation
        auto value1 = string_cache.get("key1");
        if (value1) {
            std::cout << "Retrieved key1: " << *value1 << std::endl;
        }

        // Contains check
        if (string_cache.contains("key2")) {
            std::cout << "Cache contains key2" << std::endl;
        }

        // Non-existent key
        auto value_missing = string_cache.get("key_missing");
        if (!value_missing) {
            std::cout << "Key 'key_missing' not found in cache" << std::endl;
        }

        print_stats(string_cache);

        // Remove operation
        std::cout << "\n--- Remove Operation ---" << std::endl;
        bool removed = string_cache.remove("key1");
        std::cout << "Removed key1: " << (removed ? "yes" : "no") << std::endl;

        if (!string_cache.contains("key1")) {
            std::cout << "Key1 no longer in cache after removal" << std::endl;
        }

        //----------------------------------------------------------------------
        // 2. TTL Expiration Demonstration
        //----------------------------------------------------------------------
        printSection("2. TTL Expiration Demonstration");

        TTLCache<int, std::string> quick_cache(1000ms, 5);  // 1 second TTL
        std::cout << "Created cache with 1-second TTL" << std::endl;

        quick_cache.put(1, "expires soon");
        std::cout << "Added item with 1 second TTL" << std::endl;

        auto item = quick_cache.get(1);
        if (item) {
            std::cout << "Item available immediately: " << *item << std::endl;
        }

        // Check remaining TTL
        auto remaining_ttl = quick_cache.get_remaining_ttl(1);
        if (remaining_ttl) {
            std::cout << "Remaining TTL: " << remaining_ttl->count() << "ms"
                      << std::endl;
        }

        // Wait for expiration
        std::cout << "Waiting for expiration..." << std::endl;
        std::this_thread::sleep_for(1200ms);

        item = quick_cache.get(1);
        if (!item) {
            std::cout << "Item expired and no longer available" << std::endl;
        }

        print_stats(quick_cache);

        //----------------------------------------------------------------------
        // 3. Complex Data Types
        //----------------------------------------------------------------------
        printSection("3. Complex Data Types");

        TTLCache<int, UserProfile> user_cache(10000ms, 100);
        std::cout << "Created cache for UserProfile objects" << std::endl;

        user_cache.put(101, UserProfile{101, "Alice", "alice@example.com"});
        user_cache.put(102, UserProfile{102, "Bob", "bob@example.com"});
        user_cache.put(103, UserProfile{103, "Charlie", "charlie@example.com"});

        auto user = user_cache.get(101);
        if (user) {
            std::cout << "Retrieved user: " << user->toString() << std::endl;
        }

        // Get all keys
        auto keys = user_cache.get_keys();
        std::cout << "Cache contains " << keys.size() << " users:" << std::endl;
        for (const auto& key : keys) {
            std::cout << "  User ID: " << key << std::endl;
        }

        //----------------------------------------------------------------------
        // 4. Batch Operations
        //----------------------------------------------------------------------
        printSection("4. Batch Operations");

        TTLCache<int, std::string> batch_cache(5000ms, 20);
        std::cout << "Created cache for batch operations" << std::endl;

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

        print_stats(batch_cache);

        //----------------------------------------------------------------------
        // 5. LRU Eviction Demonstration
        //----------------------------------------------------------------------
        printSection("5. LRU Eviction Demonstration");

        TTLCache<int, std::string> lru_cache(10000ms, 3);  // Capacity of 3
        std::cout << "Created cache with capacity of 3 for LRU demonstration"
                  << std::endl;

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

        print_stats(lru_cache);

        //----------------------------------------------------------------------
        // 6. Cache Configuration and Resize
        //----------------------------------------------------------------------
        printSection("6. Cache Configuration and Resize");

        TTLCache<std::string, int> config_cache(2000ms, 3);
        std::cout << "Created cache with initial capacity of 3" << std::endl;

        config_cache.put("item1", 100);
        config_cache.put("item2", 200);
        config_cache.put("item3", 300);

        std::cout << "Cache size before resize: " << config_cache.size()
                  << std::endl;

        // Resize operation
        config_cache.resize(5);
        std::cout << "Resized cache to capacity 5" << std::endl;

        config_cache.put("item4", 400);
        config_cache.put("item5", 500);
        std::cout << "Added two more items without eviction" << std::endl;
        std::cout << "Cache size after resize: " << config_cache.size() << "/"
                  << config_cache.capacity() << std::endl;

        print_stats(config_cache);

        //------------------------------------------------------------------
        // 7. Emplace Construction
        //------------------------------------------------------------------
        printSection("7. Emplace Construction");
        {
            TTLCache<std::string, LargeObject> large_cache(2000ms, 3);
            large_cache.emplace("big", std::nullopt, 256);
            std::cout << "Emplaced LargeObject of size 256. size="
                      << large_cache.size() << std::endl;
        }

        //------------------------------------------------------------------
        // 8. Compute Pattern (get_or_compute)
        //------------------------------------------------------------------
        printSection("8. Compute Pattern (get_or_compute)");
        {
            TTLCache<std::string, int> compute_cache(500ms, 10);
            auto v1 = compute_cache.get_or_compute("answer", [] { return 42; });
            std::cout << "Computed 'answer' -> " << v1 << std::endl;
            auto v2 = compute_cache.get_or_compute("answer", [] { return 7; });
            std::cout << "Cached 'answer' -> " << v2 << std::endl;
        }

        //------------------------------------------------------------------
        // 9. TTL Management (update_ttl, get_remaining_ttl)
        //------------------------------------------------------------------
        printSection("9. TTL Management");
        {
            quick_cache.put(2, "will live longer");
            quick_cache.update_ttl(2, 1500ms);
            std::this_thread::sleep_for(1100ms);
            auto r = quick_cache.get_remaining_ttl(2);
            std::cout << "Remaining TTL for key 2: "
                      << (r ? std::to_string(r->count())
                            : std::string("expired"))
                      << "ms" << std::endl;
        }

        //------------------------------------------------------------------
        // 10. Force Cleanup
        //------------------------------------------------------------------
        printSection("10. Force Cleanup");
        {
            TTLCache<int, std::string> temp(200ms, 10);
            temp.put(1, "x");
            std::this_thread::sleep_for(250ms);
            temp.force_cleanup();
            std::cout << "After force_cleanup size=" << temp.size()
                      << std::endl;
        }

        //------------------------------------------------------------------
        // 11. Eviction Callbacks
        //------------------------------------------------------------------
        printSection("11. Eviction Callbacks");
        {
            TTLCache<int, std::string> evic(5000ms, 2);
            evic.set_eviction_callback([](const int& k, const std::string& v,
                                          bool due_to_expiry) {
                std::cout << "Evicted key=" << k << ", value=" << v
                          << ", due_to_expiry="
                          << (due_to_expiry ? "true" : "false") << std::endl;
            });
            evic.put(1, "one");
            evic.put(2, "two");
            evic.put(3, "three");  // triggers LRU eviction
        }

        //------------------------------------------------------------------
        // 12. Configuration Management (update_config, get_config)
        //------------------------------------------------------------------
        printSection("12. Configuration Management");
        {
            auto cfg = config_cache.get_config();
            cfg.enable_statistics = true;
            cfg.enable_automatic_cleanup = true;
            config_cache.update_config(cfg);
            auto cfg2 = config_cache.get_config();
            std::cout << "Config: stats="
                      << (cfg2.enable_statistics ? "on" : "off")
                      << ", auto_cleanup="
                      << (cfg2.enable_automatic_cleanup ? "on" : "off")
                      << std::endl;
        }

        //------------------------------------------------------------------
        // 13. Reserve Capacity
        //------------------------------------------------------------------
        printSection("13. Reserve Capacity");
        {
            config_cache.reserve(100);
            std::cout << "Reserved capacity; capacity now ~ "
                      << config_cache.capacity() << std::endl;
        }

        //------------------------------------------------------------------
        // 14. Move Semantics and Shared Access (get_shared)
        //------------------------------------------------------------------
        printSection("14. Move Semantics and Shared Access");
        {
            TTLCache<int, LargeObject> move_cache(5000ms, 5);
            move_cache.put(7, LargeObject(32));  // move
            auto sp = move_cache.get_shared(7);
            std::cout << "Shared ptr valid: " << (sp ? "yes" : "no")
                      << std::endl;
        }

        //------------------------------------------------------------------
        // 15. Statistics Reset and Thread Safety (brief)
        //------------------------------------------------------------------
        printSection("15. Statistics Reset and Thread Safety");
        {
            TTLCache<int, int> ts_cache(2000ms, 64);
            auto worker = [&ts_cache](int base) {
                for (int i = 0; i < 50; ++i) {
                    ts_cache.put(base + i, i);
                    (void)ts_cache.get(base + i - 1);
                }
            };
            std::thread a(worker, 0), b(worker, 100);
            a.join();
            b.join();
            print_stats(ts_cache);
            ts_cache.reset_statistics();
            auto st = ts_cache.get_statistics();
            std::cout << "After reset, hits=" << st.hits
                      << ", misses=" << st.misses << std::endl;
        }

        std::cout << "\n=== All TTLCache examples completed successfully ==="
                  << std::endl;

    } catch (const TTLCacheException& e) {
        std::cerr << "TTLCache error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << std::endl;
    }

    return 0;
}
