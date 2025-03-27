#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <string>

// Include the flatmap header
#include "../atom/type/flatmap.hpp"

// Helper function to measure execution time
template <typename Func>
double measure_time(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    return duration.count();
}

// Custom struct for demonstration
struct UserProfile {
    std::string name;
    int age;
    std::string email;

    UserProfile() : age(0) {}

    UserProfile(const std::string& n, int a, const std::string& e)
        : name(n), age(a), email(e) {}

    friend std::ostream& operator<<(std::ostream& os, const UserProfile& u) {
        return os << "User{name=" << u.name << ", age=" << u.age
                  << ", email=" << u.email << "}";
    }
};

// Example 1: Basic usage of QuickFlatMap
void basic_flat_map_example() {
    std::cout << "\n=== Basic QuickFlatMap Example ===\n";

    // Create a QuickFlatMap with default parameters
    atom::type::QuickFlatMap<std::string, int> scores;

    // Insert elements
    scores.insert({"Alice", 95});
    scores.insert({"Bob", 87});
    scores.insertOrAssign("Charlie", 91);

    // Using operator[]
    scores["David"] = 78;
    scores["Eve"] = 82;

    // Access elements
    std::cout << "Alice's score: " << scores.at("Alice") << "\n";
    std::cout << "Bob's score: " << scores["Bob"] << "\n";

    // Check if key exists
    if (scores.contains("Frank")) {
        std::cout << "Frank's score exists\n";
    } else {
        std::cout << "Frank's score doesn't exist\n";
    }

    // Try to get a value that might not exist
    auto maybeScore = scores.try_get("Grace");
    if (maybeScore) {
        std::cout << "Grace's score: " << *maybeScore << "\n";
    } else {
        std::cout << "Grace's score doesn't exist\n";
    }

    // Modify an existing value
    scores.insertOrAssign("Alice", 98);
    std::cout << "Alice's updated score: " << scores["Alice"] << "\n";

    // Iterate through all entries
    std::cout << "All scores:\n";
    for (const auto& [name, score] : scores) {
        std::cout << "  " << name << ": " << score << "\n";
    }

    // Erase an element
    scores.erase("David");

    // Size and capacity
    std::cout << "Size: " << scores.size() << "\n";
    std::cout << "Capacity: " << scores.capacity() << "\n";

    // Clear the map
    scores.clear();
    std::cout << "Size after clear: " << scores.size() << "\n";
}

// Example 2: QuickFlatMap with thread safety
void thread_safe_flat_map_example() {
    std::cout << "\n=== Thread-safe QuickFlatMap Example ===\n";

    // Create a thread-safe QuickFlatMap
    atom::type::QuickFlatMap<int, std::string, std::less<>,
                             atom::type::ThreadSafetyMode::ReadWrite>
        thread_safe_map(100);

    // Insert some data
    thread_safe_map.insert({1, "One"});
    thread_safe_map.insert({2, "Two"});
    thread_safe_map.insert({3, "Three"});

    // Demonstrate thread-safe reading
    auto safe_read = [&thread_safe_map](int key) {
        return thread_safe_map.with_read_lock([key](const auto& map) {
            std::stringstream ss;
            if (auto it = std::ranges::find_if(
                    map, [key](const auto& pair) { return pair.first == key; });
                it != map.end()) {
                ss << "Found: " << key << " -> " << it->second;
            } else {
                ss << "Key " << key << " not found";
            }
            return ss.str();
        });
    };

    std::cout << safe_read(2) << "\n";
    std::cout << safe_read(4) << "\n";

    // Demonstrate thread-safe writing
    thread_safe_map.with_write_lock([](auto& map) {
        map.push_back({4, "Four"});
        map.push_back({5, "Five"});
        std::cout << "Added two new elements inside write lock\n";
    });

    std::cout << "Map size after write: " << thread_safe_map.size() << "\n";

    // Try to get multiple values atomically
    auto opt_value = thread_safe_map.try_get(3);
    if (opt_value) {
        std::cout << "Value for key 3: " << *opt_value << "\n";
    }
}

// Example 3: Custom comparator and batch operations
void custom_comparator_example() {
    std::cout << "\n=== Custom Comparator Example ===\n";

    // Case-insensitive string comparator
    auto case_insensitive_comp = [](const std::string& a,
                                    const std::string& b) {
        return std::equal(
            a.begin(), a.end(), b.begin(), b.end(),
            [](char a, char b) { return std::tolower(a) == std::tolower(b); });
    };
}

// Example 4: QuickFlatMultiMap usage
void flat_multimap_example() {
    std::cout << "\n=== QuickFlatMultiMap Example ===\n";

    // Create a multimap
    atom::type::QuickFlatMultiMap<std::string, int> tags;

    // Insert multiple values with the same key
    tags.insert({"article", 1001});
    tags.insert({"article", 1002});
    tags.insert({"article", 1003});
    tags.insert({"tutorial", 2001});
    tags.insert({"tutorial", 2002});
    tags.insert({"news", 3001});

    // Count elements with a specific key
    std::cout << "Number of 'article' tags: " << tags.count("article") << "\n";
    std::cout << "Number of 'news' tags: " << tags.count("news") << "\n";

    // Get all values for a key
    auto article_ids = tags.get_all("article");
    std::cout << "All article IDs: ";
    for (int id : article_ids) {
        std::cout << id << " ";
    }
    std::cout << "\n";

    // Using equal range
    auto [begin, end] = tags.equalRange("tutorial");
    std::cout << "Tutorial IDs using equal range: ";
    for (auto it = begin; it != end; ++it) {
        std::cout << it->second << " ";
    }
    std::cout << "\n";

    // Erase all occurrences of a key
    bool erased = tags.erase("article");
    std::cout << "Erased all articles: " << (erased ? "yes" : "no") << "\n";
    std::cout << "Remaining size: " << tags.size() << "\n";
}

// Example 5: Performance comparison with std::map
void performance_comparison() {
    std::cout << "\n=== Performance Comparison ===\n";

    constexpr int NUM_ELEMENTS = 100000;

    // Create containers
    atom::type::QuickFlatMap<int, int> flat_map(NUM_ELEMENTS);
    std::map<int, int> std_map;
    atom::type::QuickFlatMap<int, int, std::less<>,
                             atom::type::ThreadSafetyMode::None, true>
        sorted_flat_map(NUM_ELEMENTS);

    // Insert performance
    std::cout << "Inserting " << NUM_ELEMENTS << " elements...\n";

    double flat_insert_time = measure_time([&]() {
        for (int i = 0; i < NUM_ELEMENTS; ++i) {
            flat_map.insert({i, i * 10});
        }
    });

    double std_insert_time = measure_time([&]() {
        for (int i = 0; i < NUM_ELEMENTS; ++i) {
            std_map.insert({i, i * 10});
        }
    });

    double sorted_insert_time = measure_time([&]() {
        for (int i = 0; i < NUM_ELEMENTS; ++i) {
            sorted_flat_map.insert({i, i * 10});
        }
    });

    std::cout << "Insert time (ms):\n";
    std::cout << "  QuickFlatMap: " << flat_insert_time << "\n";
    std::cout << "  std::map: " << std_insert_time << "\n";
    std::cout << "  Sorted QuickFlatMap: " << sorted_insert_time << "\n";

    // Lookup performance
    constexpr int NUM_LOOKUPS = 10000;
    constexpr int LOOKUP_RANGE = NUM_ELEMENTS - 1;

    std::cout << "Performing " << NUM_LOOKUPS << " random lookups...\n";

    double flat_lookup_time = measure_time([&]() {
        for (int i = 0; i < NUM_LOOKUPS; ++i) {
            int key = rand() % LOOKUP_RANGE;
            auto it = flat_map.find(key);
            if (it == flat_map.end()) {
                std::cerr << "Error: key not found in flat_map\n";
            }
        }
    });

    double std_lookup_time = measure_time([&]() {
        for (int i = 0; i < NUM_LOOKUPS; ++i) {
            int key = rand() % LOOKUP_RANGE;
            auto it = std_map.find(key);
            if (it == std_map.end()) {
                std::cerr << "Error: key not found in std_map\n";
            }
        }
    });

    double sorted_lookup_time = measure_time([&]() {
        for (int i = 0; i < NUM_LOOKUPS; ++i) {
            int key = rand() % LOOKUP_RANGE;
            auto it = sorted_flat_map.find(key);
            if (it == sorted_flat_map.end()) {
                std::cerr << "Error: key not found in sorted_flat_map\n";
            }
        }
    });

    std::cout << "Lookup time (ms):\n";
    std::cout << "  QuickFlatMap: " << flat_lookup_time << "\n";
    std::cout << "  std::map: " << std_lookup_time << "\n";
    std::cout << "  Sorted QuickFlatMap: " << sorted_lookup_time << "\n";
}

// Example 6: Error handling
void error_handling_example() {
    std::cout << "\n=== Error Handling Example ===\n";

    atom::type::QuickFlatMap<std::string, double> values;

    // Try to access non-existent key with at()
    try {
        double val = values.at("missing_key");
        std::cout << "Value: " << val << "\n";
    } catch (const atom::type::exceptions::key_not_found_error& e) {
        std::cout << "Expected error caught: " << e.what() << "\n";
    }

    // Try to reserve too much memory
    try {
        values.reserve(std::numeric_limits<std::size_t>::max());
    } catch (const atom::type::exceptions::container_full_error& e) {
        std::cout << "Expected capacity error caught: " << e.what() << "\n";
    }

    // Demonstrate that safe operations still work after errors
    values["valid_key"] = 42.5;
    std::cout << "After error handling, valid_key = " << values["valid_key"]
              << "\n";
}

// Example 7: Using the sorted vector implementation
void sorted_vector_example() {
    std::cout << "\n=== Sorted Vector Implementation Example ===\n";

    // Create a sorted QuickFlatMap
    atom::type::QuickFlatMap<int, std::string, std::less<>,
                             atom::type::ThreadSafetyMode::None, true>
        sorted_map;

    // Insert elements in random order
    sorted_map.insert({5, "Five"});
    sorted_map.insert({1, "One"});
    sorted_map.insert({3, "Three"});
    sorted_map.insert({2, "Two"});
    sorted_map.insert({4, "Four"});

    // The internal vector should be automatically sorted
    std::cout << "Elements in sorted order:\n";
    for (const auto& [key, value] : sorted_map) {
        std::cout << "  " << key << ": " << value << "\n";
    }

    // Binary search is used for lookups
    auto it = sorted_map.find(3);
    if (it != sorted_map.end()) {
        std::cout << "Found key 3 with value: " << it->second << "\n";
    }

    // Insert more elements
    sorted_map.insertOrAssign(6, "Six");
    sorted_map.insertOrAssign(0, "Zero");

    // Still sorted
    std::cout << "Updated sorted elements:\n";
    for (const auto& [key, value] : sorted_map) {
        std::cout << "  " << key << ": " << value << "\n";
    }
}

int main() {
    std::cout << "QuickFlatMap and QuickFlatMultiMap Usage Examples\n";
    std::cout << "================================================\n";

    // Run all examples
    basic_flat_map_example();
    thread_safe_flat_map_example();
    custom_comparator_example();
    flat_multimap_example();
    performance_comparison();
    error_handling_example();
    sorted_vector_example();

    return 0;
}