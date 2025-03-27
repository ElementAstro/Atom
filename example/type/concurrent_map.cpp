#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <thread>

#include "atom/type/concurrent_map.hpp"

// Helper function to print results
template <typename T>
void print_results(const std::string& operation,
                   const std::vector<std::optional<T>>& results) {
    std::cout << operation << " results: " << std::endl;
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "  [" << i << "]: ";
        if (results[i].has_value()) {
            std::cout << results[i].value();
        } else {
            std::cout << "not found";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

// Helper for generating random strings
std::string random_string(size_t length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += alphanum[dis(gen)];
    }
    return result;
}

// A simple computation function to demonstrate thread pool usage
double compute_expensive_operation(int input) {
    // Simulate complex computation
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return std::sqrt(input) * std::log(input + 1);
}

int main() {
    std::cout << "=== Concurrent Map Usage Examples ===" << std::endl
              << std::endl;

    // Create a concurrent map with default thread count and no cache
    atom::type::concurrent_map<std::string, int> map_no_cache(4, 0);
    std::cout << "Created map with " << map_no_cache.get_thread_count()
              << " threads and no cache" << std::endl;

    // Create a concurrent map with custom thread count and cache
    atom::type::concurrent_map<std::string, int> map_with_cache(8, 100);
    std::cout << "Created map with " << map_with_cache.get_thread_count()
              << " threads and cache size 100" << std::endl;

    // 1. Basic insertion and retrieval
    std::cout << "\n=== Basic Operations ===" << std::endl;

    // Insert some values
    map_with_cache.insert("key1", 100);
    map_with_cache.insert("key2", 200);
    map_with_cache.insert("key3", 300);
    std::cout << "Inserted 3 key-value pairs" << std::endl;

    // Find values
    auto value1 = map_with_cache.find("key1");
    auto value2 = map_with_cache.find("key2");
    auto value_not_found = map_with_cache.find("nonexistent");

    std::cout << "Find key1: "
              << (value1.has_value() ? std::to_string(value1.value())
                                     : "not found")
              << std::endl;
    std::cout << "Find key2: "
              << (value2.has_value() ? std::to_string(value2.value())
                                     : "not found")
              << std::endl;
    std::cout << "Find nonexistent: "
              << (value_not_found.has_value()
                      ? std::to_string(value_not_found.value())
                      : "not found")
              << std::endl;

    // Size and empty checks
    std::cout << "Map size: " << map_with_cache.size() << std::endl;
    std::cout << "Is map empty? " << (map_with_cache.empty() ? "Yes" : "No")
              << std::endl;

    // 2. Find or insert
    std::cout << "\n=== Find or Insert ===" << std::endl;

    bool inserted1 =
        map_with_cache.find_or_insert("key1", 999);  // Already exists
    bool inserted2 = map_with_cache.find_or_insert("key4", 400);  // New key

    std::cout << "Find or insert key1 (already exists): "
              << (inserted1 ? "Inserted" : "Not inserted") << std::endl;
    std::cout << "Find or insert key4 (new): "
              << (inserted2 ? "Inserted" : "Not inserted") << std::endl;
    std::cout << "Map size after find_or_insert: " << map_with_cache.size()
              << std::endl;

    // 3. Batch operations
    std::cout << "\n=== Batch Operations ===" << std::endl;

    // Batch find
    std::vector<std::string> batch_keys = {"key1", "key2", "nonexistent",
                                           "key4"};
    auto batch_results = map_with_cache.batch_find(batch_keys);
    print_results("Batch find", batch_results);

    // Batch update
    std::vector<std::pair<std::string, int>> batch_updates = {
        {"key1", 1000},
        {"key2", 2000},
        {"key5", 5000},  // New key
        {"key6", 6000}   // New key
    };
    map_with_cache.batch_update(batch_updates);
    std::cout << "Performed batch update of 4 key-value pairs" << std::endl;

    // Verify batch update with another batch find
    std::vector<std::string> verify_keys = {"key1", "key2", "key5", "key6"};
    auto verify_results = map_with_cache.batch_find(verify_keys);
    print_results("After batch update", verify_results);

    // Batch erase
    std::vector<std::string> keys_to_erase = {"key1", "key5", "nonexistent"};
    size_t erased_count = map_with_cache.batch_erase(keys_to_erase);
    std::cout << "Batch erase: Removed " << erased_count << " keys"
              << std::endl;
    std::cout << "Map size after batch erase: " << map_with_cache.size()
              << std::endl;

    // 4. Range query
    std::cout << "\n=== Range Query ===" << std::endl;

    // Add some alphabetically ordered keys for range query
    map_with_cache.insert("a1", 1);
    map_with_cache.insert("b2", 2);
    map_with_cache.insert("c3", 3);
    map_with_cache.insert("d4", 4);
    map_with_cache.insert("e5", 5);

    auto range_results = map_with_cache.range_query("b2", "d4");
    std::cout << "Range query from 'b2' to 'd4' returned "
              << range_results.size() << " items:" << std::endl;
    for (const auto& [key, value] : range_results) {
        std::cout << "  " << key << ": " << value << std::endl;
    }

    // 5. Thread pool operations
    std::cout << "\n=== Thread Pool Operations ===" << std::endl;

    // Submit tasks to the thread pool
    std::vector<std::future<double>> futures;
    for (int i = 1; i <= 10; ++i) {
        futures.push_back(
            map_with_cache.submit(compute_expensive_operation, i * 10));
    }

    // Collect and print results
    std::cout << "Thread pool computation results:" << std::endl;
    for (size_t i = 0; i < futures.size(); ++i) {
        std::cout << "  Task " << i << " result: " << std::fixed
                  << std::setprecision(4) << futures[i].get() << std::endl;
    }

    // 6. Adjust thread pool size
    std::cout << "\n=== Adjusting Thread Pool Size ===" << std::endl;

    std::cout << "Current thread count: " << map_with_cache.get_thread_count()
              << std::endl;
    map_with_cache.adjust_thread_pool_size(4);
    std::cout << "After adjustment, thread count: "
              << map_with_cache.get_thread_count() << std::endl;

    // 7. Cache operations
    std::cout << "\n=== Cache Operations ===" << std::endl;

    std::cout << "Has cache: " << (map_with_cache.has_cache() ? "Yes" : "No")
              << std::endl;
    std::cout << "No-cache map has cache: "
              << (map_no_cache.has_cache() ? "Yes" : "No") << std::endl;

    // Change cache size
    map_no_cache.set_cache_size(50);
    std::cout << "After setting cache size, no-cache map has cache: "
              << (map_no_cache.has_cache() ? "Yes" : "No") << std::endl;

    // Disable cache
    map_with_cache.set_cache_size(0);
    std::cout << "After disabling cache, map_with_cache has cache: "
              << (map_with_cache.has_cache() ? "Yes" : "No") << std::endl;

    // 8. Map merging
    std::cout << "\n=== Map Merging ===" << std::endl;

    atom::type::concurrent_map<std::string, int> map1(2, 20);
    atom::type::concurrent_map<std::string, int> map2(2, 20);

    // Populate first map
    map1.insert("apple", 1);
    map1.insert("banana", 2);
    map1.insert("common", 100);

    // Populate second map
    map2.insert("cherry", 3);
    map2.insert("date", 4);
    map2.insert("common", 200);  // Common key with different value

    std::cout << "Map1 size before merge: " << map1.size() << std::endl;
    std::cout << "Map2 size: " << map2.size() << std::endl;

    // Merge map2 into map1
    map1.merge(map2);
    std::cout << "Map1 size after merge: " << map1.size() << std::endl;

    // Check the value of the common key
    auto common_value = map1.find("common");
    std::cout << "After merge, 'common' has value: " << common_value.value()
              << std::endl;

    // 9. Performance test with larger dataset
    std::cout << "\n=== Performance Test ===" << std::endl;

    // Create a map with cache for performance comparison
    atom::type::concurrent_map<std::string, std::string> perf_map_with_cache(
        8, 1000);

    // Generate random data
    const size_t num_items = 10000;
    std::vector<std::pair<std::string, std::string>> test_data;
    test_data.reserve(num_items);

    std::cout << "Generating " << num_items << " random key-value pairs..."
              << std::endl;
    for (size_t i = 0; i < num_items; ++i) {
        test_data.emplace_back("key_" + random_string(10),  // Random key
                               random_string(100)           // Random value
        );
    }

    // Measure insertion time
    auto start_time = std::chrono::high_resolution_clock::now();

    for (const auto& [key, value] : test_data) {
        perf_map_with_cache.insert(key, value);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> insert_time =
        end_time - start_time;

    std::cout << "Time to insert " << num_items
              << " items: " << insert_time.count() << " ms" << std::endl;

    // Measure batch find time (with cache hits)
    std::vector<std::string> perf_keys;
    perf_keys.reserve(1000);

    // Mix of existing and non-existing keys
    for (size_t i = 0; i < 500; ++i) {
        perf_keys.push_back(
            test_data[i].first);  // Existing keys for cache hits
    }
    for (size_t i = 0; i < 500; ++i) {
        perf_keys.push_back("nonexistent_" +
                            random_string(10));  // Non-existing keys
    }

    start_time = std::chrono::high_resolution_clock::now();
    auto perf_results = perf_map_with_cache.batch_find(perf_keys);
    end_time = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> find_time = end_time - start_time;
    std::cout << "Time for batch_find of 1000 mixed keys: " << find_time.count()
              << " ms" << std::endl;

    // Count hits and misses
    size_t hits = 0;
    for (const auto& result : perf_results) {
        if (result.has_value())
            hits++;
    }

    std::cout << "Found " << hits << " out of " << perf_keys.size() << " keys"
              << std::endl;

    // 10. Clear operation
    std::cout << "\n=== Clear Operation ===" << std::endl;

    std::cout << "Map size before clear: " << perf_map_with_cache.size()
              << std::endl;
    perf_map_with_cache.clear();
    std::cout << "Map size after clear: " << perf_map_with_cache.size()
              << std::endl;

    std::cout << "\nAll examples completed successfully!" << std::endl;

    return 0;
}