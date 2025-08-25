#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "atom/type/robin_hood.hpp"

// Helper function to print section headers
void print_header(const std::string& title) {
    std::cout << "\n=== " << title << " ===" << std::endl;
    std::cout << std::string(title.length() + 8, '=') << std::endl;
}

// Custom hash function for demonstration
struct CustomStringHash {
    size_t operator()(const std::string& str) const {
        size_t hash = 0;
        for (char c : str) {
            hash = hash * 31 + static_cast<size_t>(c);
        }
        return hash;
    }
};

// Performance measurement helper
template <typename Func>
double measure_time(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count() / 1000.0;  // Return milliseconds
}

int main() {
    std::cout << "Robin Hood Hash Map Usage Examples" << std::endl;
    std::cout << "==================================" << std::endl;

    // 1. Basic Operations
    print_header("Basic Operations");

    atom::utils::unordered_flat_map<std::string, int> map;

    std::cout << "Created empty map" << std::endl;
    std::cout << "Size: " << map.size() << std::endl;
    std::cout << "Empty: " << (map.empty() ? "Yes" : "No") << std::endl;
    std::cout << "Capacity: " << map.capacity() << std::endl;

    // Insert some values
    map.insert("apple", 1);
    map.insert("banana", 2);
    map.insert("cherry", 3);
    map.emplace("date", 4);

    std::cout << "\nAfter inserting 4 items:" << std::endl;
    std::cout << "Size: " << map.size() << std::endl;
    std::cout << "Load factor: " << map.load_factor() << std::endl;

    // Access values
    std::cout << "\nAccessing values:" << std::endl;
    std::cout << "apple: " << map["apple"] << std::endl;
    std::cout << "banana: " << map.at("banana") << std::endl;

    // Check existence
    std::cout << "\nExistence checks:" << std::endl;
    std::cout << "Contains 'cherry': "
              << (map.contains("cherry") ? "Yes" : "No") << std::endl;
    std::cout << "Contains 'grape': " << (map.contains("grape") ? "Yes" : "No")
              << std::endl;

    // 2. Iterator Usage
    print_header("Iterator Usage");

    std::cout << "Iterating through map:" << std::endl;
    for (const auto& [key, value] : map) {
        std::cout << "  " << key << ": " << value << std::endl;
    }

    std::cout << "\nUsing explicit iterators:" << std::endl;
    for (auto it = map.begin(); it != map.end(); ++it) {
        std::cout << "  " << it->first << " -> " << it->second << std::endl;
    }

    // 3. Find Operations
    print_header("Find Operations");

    auto it = map.find("banana");
    if (it != map.end()) {
        std::cout << "Found 'banana': " << it->second << std::endl;
        it->second = 20;  // Modify value
        std::cout << "Modified 'banana' to: " << map["banana"] << std::endl;
    }

    auto missing = map.find("grape");
    std::cout << "Looking for 'grape': "
              << (missing == map.end() ? "Not found" : "Found") << std::endl;

    // 4. Erase Operations
    print_header("Erase Operations");

    std::cout << "Before erase - Size: " << map.size() << std::endl;

    // Erase by key
    size_t erased = map.erase("cherry");
    std::cout << "Erased 'cherry': " << erased << " items removed" << std::endl;
    std::cout << "After erase - Size: " << map.size() << std::endl;

    // Erase by iterator
    auto to_erase = map.find("date");
    if (to_erase != map.end()) {
        map.erase(to_erase);
        std::cout << "Erased 'date' by iterator" << std::endl;
        std::cout << "Final size: " << map.size() << std::endl;
    }

    // 5. Custom Hash Function
    print_header("Custom Hash Function");

    atom::utils::unordered_flat_map<std::string, int, CustomStringHash>
        custom_map;

    custom_map.insert("test1", 100);
    custom_map.insert("test2", 200);
    custom_map.insert("test3", 300);

    std::cout << "Map with custom hash function:" << std::endl;
    for (const auto& [key, value] : custom_map) {
        std::cout << "  " << key << ": " << value << std::endl;
    }

    // 6. Threading Policies
    print_header("Threading Policies");

    // Unsafe (default)
    atom::utils::unordered_flat_map<int, std::string> unsafe_map;
    unsafe_map.set_threading_policy(
        atom::utils::unordered_flat_map<int,
                                        std::string>::threading_policy::unsafe);

    // Reader-writer lock
    atom::utils::unordered_flat_map<int, std::string> rw_map;
    rw_map.set_threading_policy(
        atom::utils::unordered_flat_map<
            int, std::string>::threading_policy::reader_lock);

    // Full mutex
    atom::utils::unordered_flat_map<int, std::string> mutex_map;
    mutex_map.set_threading_policy(
        atom::utils::unordered_flat_map<int,
                                        std::string>::threading_policy::mutex);

    std::cout << "Created maps with different threading policies:" << std::endl;
    std::cout << "- Unsafe (no synchronization)" << std::endl;
    std::cout << "- Reader-writer lock (concurrent reads)" << std::endl;
    std::cout << "- Full mutex (exclusive access)" << std::endl;

    // Add some data to thread-safe map
    for (int i = 0; i < 10; ++i) {
        rw_map.insert(i, "value_" + std::to_string(i));
    }

    std::cout << "Thread-safe map size: " << rw_map.size() << std::endl;

    // 7. Performance Characteristics
    print_header("Performance Characteristics");

    atom::utils::unordered_flat_map<int, int> perf_map;

    const int num_items = 10000;
    std::vector<int> keys(num_items);
    std::iota(keys.begin(), keys.end(), 0);

    // Shuffle for random access pattern
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(keys.begin(), keys.end(), gen);

    // Measure insertion time
    double insert_time = measure_time([&]() {
        for (int key : keys) {
            perf_map.insert(key, key * key);
        }
    });

    std::cout << "Inserted " << num_items << " items in " << insert_time
              << " ms" << std::endl;
    std::cout << "Final load factor: " << perf_map.load_factor() << std::endl;
    std::cout << "Capacity: " << perf_map.capacity() << std::endl;

    // Measure lookup time
    int found_count = 0;
    double lookup_time = measure_time([&]() {
        for (int key : keys) {
            if (perf_map.contains(key)) {
                found_count++;
            }
        }
    });

    std::cout << "Looked up " << num_items << " items in " << lookup_time
              << " ms" << std::endl;
    std::cout << "Found " << found_count << " items" << std::endl;

    // 8. Memory Management
    print_header("Memory Management");

    atom::utils::unordered_flat_map<std::string, std::vector<int>> memory_map;

    // Reserve capacity to avoid rehashing
    memory_map.reserve(1000);
    std::cout << "Reserved capacity: " << memory_map.capacity() << std::endl;

    // Insert large objects
    for (int i = 0; i < 5; ++i) {
        std::vector<int> large_vector(1000, i);
        memory_map.emplace("vector_" + std::to_string(i),
                           std::move(large_vector));
    }

    std::cout << "Inserted 5 large vectors" << std::endl;
    std::cout << "Map size: " << memory_map.size() << std::endl;
    std::cout << "Load factor: " << memory_map.load_factor() << std::endl;

    // Clear and check memory
    memory_map.clear();
    std::cout << "After clear - Size: " << memory_map.size() << std::endl;
    std::cout << "After clear - Capacity: " << memory_map.capacity()
              << std::endl;

    std::cout << "\nAll Robin Hood hash map examples completed successfully!"
              << std::endl;
    return 0;
}
