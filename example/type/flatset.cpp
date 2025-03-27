#include "../atom/type/flatset.hpp"

#include <chrono>
#include <iostream>
#include <set>
#include <string>

// Helper function to print a set
template <typename T>
void print_set(const atom::type::FlatSet<T>& set, const std::string& name) {
    std::cout << name << " (size " << set.size() << "): ";
    for (const auto& value : set) {
        std::cout << value << " ";
    }
    std::cout << std::endl;
}

// Helper function to measure execution time
template <typename Func>
double measure_execution_time(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// Custom data type for demonstration
struct Person {
    std::string name;
    int age;

    Person(std::string n, int a) : name(std::move(n)), age(a) {}

    // Required for sorting and comparison
    bool operator<(const Person& other) const {
        return name < other.name || (name == other.name && age < other.age);
    }

    // For demonstration purposes
    friend std::ostream& operator<<(std::ostream& os, const Person& p) {
        return os << "{" << p.name << ", " << p.age << "}";
    }
};

// Custom comparison function for numeric types
struct DescendingCompare {
    template <typename T>
    bool operator()(const T& a, const T& b) const {
        return a > b;
    }
};

// Example 1: Basic FlatSet Operations
void basic_operations() {
    std::cout << "\n=== Example 1: Basic FlatSet Operations ===\n";

    // Create a FlatSet of integers
    atom::type::FlatSet<int> numbers;

    // Insert elements
    numbers.insert(10);
    numbers.insert(20);
    auto [it, success] = numbers.insert(30);
    std::cout << "Inserted 30: " << (success ? "yes" : "no")
              << ", value at iterator: " << *it << std::endl;

    // Insert a duplicate element
    auto [it2, success2] = numbers.insert(10);
    std::cout << "Inserted 10 again: " << (success2 ? "yes" : "no")
              << ", value at iterator: " << *it2 << std::endl;

    // Insert multiple elements
    numbers.insert({5, 15, 25, 35});

    // Print the set
    print_set(numbers, "Numbers set");

    // Check if set contains an element
    std::cout << "Contains 20? " << (numbers.contains(20) ? "yes" : "no")
              << std::endl;
    std::cout << "Contains 40? " << (numbers.contains(40) ? "yes" : "no")
              << std::endl;

    // Find an element
    auto findIt = numbers.find(15);
    if (findIt != numbers.end()) {
        std::cout << "Found 15 in the set\n";
    }

    // Count elements
    std::cout << "Count of 10: " << numbers.count(10) << std::endl;
    std::cout << "Count of 40: " << numbers.count(40) << std::endl;

    // Erase elements
    size_t erased = numbers.erase(10);
    std::cout << "Erased " << erased << " occurrences of 10\n";

    // Using iterators to erase
    auto it3 = numbers.find(5);
    if (it3 != numbers.end()) {
        numbers.erase(it3);
        std::cout << "Erased 5 using iterator\n";
    }

    print_set(numbers, "Numbers set after erasure");

    // Clear the set
    numbers.clear();
    std::cout << "After clear, size: " << numbers.size()
              << ", empty: " << (numbers.empty() ? "yes" : "no") << std::endl;
}

// Example 2: Different Construction Methods
void construction_methods() {
    std::cout << "\n=== Example 2: Different Construction Methods ===\n";

    // Default constructor
    atom::type::FlatSet<std::string> set1;
    set1.insert({"apple", "banana", "cherry"});
    print_set(set1, "Set1 (default constructor)");

    // Constructor with custom comparator
    atom::type::FlatSet<int, DescendingCompare> set2(DescendingCompare{});
    set2.insert({1, 3, 5, 2, 4});
    std::cout << "Set2 (custom comparator, descending order): ";
    for (const auto& value : set2) {
        std::cout << value << " ";
    }
    std::cout << std::endl;

    // Constructor from iterator range
    std::vector<double> vec = {1.1, 2.2, 3.3, 4.4, 5.5};
    atom::type::FlatSet<double> set3(vec.begin(), vec.end());
    print_set(set3, "Set3 (from iterator range)");

    // Constructor from initializer list
    atom::type::FlatSet<char> set4({'a', 'b', 'c', 'd', 'a', 'b'});
    print_set(set4, "Set4 (from initializer list with duplicates)");

    // Copy constructor
    atom::type::FlatSet<std::string> set5(set1);
    print_set(set5, "Set5 (copy of Set1)");

    // Move constructor
    atom::type::FlatSet<std::string> set6(std::move(set5));
    print_set(set6, "Set6 (moved from Set5)");
    std::cout << "Set5 after move, size: " << set5.size() << std::endl;
}

// Example 3: Using Custom Types
void custom_types_example() {
    std::cout << "\n=== Example 3: Using Custom Types ===\n";

    // Create a FlatSet of Person objects
    atom::type::FlatSet<Person> people;

    // Insert elements
    people.insert(Person("Alice", 30));
    people.insert(Person("Bob", 25));
    people.insert(Person("Charlie", 35));

    // Insert duplicate (same name and age)
    auto [it, success] = people.insert(Person("Bob", 25));
    std::cout << "Inserted duplicate Bob: " << (success ? "yes" : "no")
              << std::endl;

    // Insert different age with same name (not a duplicate in our comparison)
    auto [it2, success2] = people.insert(Person("Bob", 30));
    std::cout << "Inserted same name different age: "
              << (success2 ? "yes" : "no") << std::endl;

    // Print the set
    std::cout << "People set: ";
    for (const auto& person : people) {
        std::cout << person << " ";
    }
    std::cout << std::endl;

    // Find a person
    Person searchPerson("Alice", 30);
    auto findIt = people.find(searchPerson);
    if (findIt != people.end()) {
        std::cout << "Found: " << *findIt << std::endl;
    }

    // Emplace
    auto [it3, success3] = people.emplace("David", 40);
    std::cout << "Emplaced David: " << (success3 ? "yes" : "no")
              << ", value: " << *it3 << std::endl;
}

// Example 4: Iterators and Traversal
void iterators_example() {
    std::cout << "\n=== Example 4: Iterators and Traversal ===\n";

    atom::type::FlatSet<int> numbers = {10, 20, 30, 40, 50};

    // Forward iteration
    std::cout << "Forward iteration: ";
    for (auto it = numbers.begin(); it != numbers.end(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    // Const iteration
    const auto& const_numbers = numbers;
    std::cout << "Const iteration: ";
    for (auto it = const_numbers.cbegin(); it != const_numbers.cend(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    // Reverse iteration
    std::cout << "Reverse iteration: ";
    for (auto it = numbers.rbegin(); it != numbers.rend(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    // Const reverse iteration
    std::cout << "Const reverse iteration: ";
    for (auto it = const_numbers.crbegin(); it != const_numbers.crend(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    // Range-based for loop
    std::cout << "Range-based for loop: ";
    for (const auto& value : numbers) {
        std::cout << value << " ";
    }
    std::cout << std::endl;

    // Using view() method for ranges
    std::cout << "Using view(): ";
    for (const auto& value : numbers.view()) {
        std::cout << value << " ";
    }
    std::cout << std::endl;
}

// Example 5: Advanced Insert Operations
void advanced_insert() {
    std::cout << "\n=== Example 5: Advanced Insert Operations ===\n";

    atom::type::FlatSet<int> numbers = {10, 20, 30, 40, 50};

    // Insert with hint
    auto hint = numbers.find(20);
    auto it = numbers.insert(hint, 15);
    std::cout << "Inserted 15 with hint, resulting value: " << *it << std::endl;

    // Bad hint (will be ignored but still work)
    auto bad_hint = numbers.end();  // Not optimal for inserting 25
    auto it2 = numbers.insert(bad_hint, 25);
    std::cout << "Inserted 25 with bad hint, resulting value: " << *it2
              << std::endl;

    print_set(numbers, "Numbers after hint insertions");

    // Bulk insert from vector
    std::vector<int> to_insert = {5, 35, 45, 55};
    numbers.insert(to_insert.begin(), to_insert.end());
    print_set(numbers, "After bulk insert from vector");

    // Emplace hint
    auto hint3 = numbers.find(35);
    auto it3 = numbers.emplace_hint(hint3, 32);
    std::cout << "Emplaced 32 with hint, resulting value: " << *it3
              << std::endl;

    print_set(numbers, "Final set after all insertions");
}

// Example 6: Bounds and Range Operations
void bounds_and_ranges() {
    std::cout << "\n=== Example 6: Bounds and Range Operations ===\n";

    atom::type::FlatSet<int> numbers = {10, 20, 30, 40, 50,
                                        60, 70, 80, 90, 100};

    // Lower bound (first element >= value)
    auto lb = numbers.lowerBound(45);
    std::cout << "Lower bound of 45: "
              << (lb != numbers.end() ? std::to_string(*lb) : "end")
              << std::endl;

    // Upper bound (first element > value)
    auto ub = numbers.upperBound(40);
    std::cout << "Upper bound of 40: "
              << (ub != numbers.end() ? std::to_string(*ub) : "end")
              << std::endl;

    // Equal range for existing element
    auto [first, last] = numbers.equalRange(50);
    std::cout << "Equal range for 50: ";
    if (first != numbers.end()) {
        std::cout << "first = " << *first;
        if (last != numbers.end()) {
            std::cout << ", last = " << *last;
        } else {
            std::cout << ", last = end";
        }
    } else {
        std::cout << "not found";
    }
    std::cout << std::endl;

    // Equal range for non-existing element
    auto [first2, last2] = numbers.equalRange(55);
    std::cout << "Equal range for 55: ";
    if (first2 != numbers.end()) {
        std::cout << "first = " << *first2;
        if (last2 != numbers.end()) {
            std::cout << ", last = " << *last2;
        } else {
            std::cout << ", last = end";
        }
    } else {
        std::cout << "not found";
    }
    std::cout << std::endl;

    // Extract a subrange
    std::cout << "Elements between 30 and 70: ";
    auto start = numbers.lowerBound(30);
    auto end = numbers.upperBound(70);
    for (auto it = start; it != end; ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;
}

// Example 7: Memory Management and Performance
void memory_and_performance() {
    std::cout << "\n=== Example 7: Memory Management and Performance ===\n";

    // Create an empty set with reservation
    atom::type::FlatSet<int> numbers;
    numbers.reserve(1000);
    std::cout << "Initial capacity after reserve(1000): " << numbers.capacity()
              << std::endl;

    // Add elements
    for (int i = 0; i < 500; ++i) {
        numbers.insert(i);
    }

    std::cout << "Size after inserting 500 elements: " << numbers.size()
              << std::endl;
    std::cout << "Capacity after insertions: " << numbers.capacity()
              << std::endl;

    // Shrink to fit
    numbers.shrink_to_fit();
    std::cout << "Capacity after shrink_to_fit(): " << numbers.capacity()
              << std::endl;

    // Performance comparison with std::set
    constexpr int BENCHMARK_SIZE = 10000;

    // Lambda to create and fill a FlatSet
    auto buildFlatSet = []() {
        atom::type::FlatSet<int> set;
        set.reserve(BENCHMARK_SIZE);
        for (int i = 0; i < BENCHMARK_SIZE; ++i) {
            set.insert(i);
        }
        return set;
    };

    // Lambda to create and fill a std::set
    auto buildStdSet = []() {
        std::set<int> set;
        for (int i = 0; i < BENCHMARK_SIZE; ++i) {
            set.insert(i);
        }
        return set;
    };

    // Measure insertion time
    double flatSetInsertTime =
        measure_execution_time([&]() { buildFlatSet(); });
    double stdSetInsertTime = measure_execution_time([&]() { buildStdSet(); });

    std::cout << "Time to insert " << BENCHMARK_SIZE
              << " elements (ms):" << std::endl;
    std::cout << "  FlatSet: " << flatSetInsertTime << std::endl;
    std::cout << "  std::set: " << stdSetInsertTime << std::endl;

    // Create sets for search benchmark
    auto flatSet = buildFlatSet();
    auto stdSet = buildStdSet();

    // Measure lookup time (1000 random lookups)
    constexpr int LOOKUP_COUNT = 1000;

    double flatSetLookupTime = measure_execution_time([&]() {
        for (int i = 0; i < LOOKUP_COUNT; ++i) {
            int value = rand() % BENCHMARK_SIZE;
            flatSet.find(value);
        }
    });

    double stdSetLookupTime = measure_execution_time([&]() {
        for (int i = 0; i < LOOKUP_COUNT; ++i) {
            int value = rand() % BENCHMARK_SIZE;
            auto result = stdSet.find(value);
            (void)result;
        }
    });

    std::cout << "Time for " << LOOKUP_COUNT
              << " random lookups (ms):" << std::endl;
    std::cout << "  FlatSet: " << flatSetLookupTime << std::endl;
    std::cout << "  std::set: " << stdSetLookupTime << std::endl;

    // Check max size
    std::cout << "Max size: " << flatSet.max_size() << std::endl;
}

// Example 8: Set Operations
void set_operations() {
    std::cout << "\n=== Example 8: Set Operations ===\n";

    atom::type::FlatSet<int> set1 = {1, 3, 5, 7, 9};
    atom::type::FlatSet<int> set2 = {1, 2, 5, 8, 9};

    print_set(set1, "Set1");
    print_set(set2, "Set2");

    // Union
    atom::type::FlatSet<int> set_union;

    // Merge all elements from both sets
    std::vector<int> union_values;
    union_values.reserve(set1.size() + set2.size());

    // Insert all elements from both sets
    for (const auto& value : set1) {
        union_values.push_back(value);
    }
    for (const auto& value : set2) {
        union_values.push_back(value);
    }

    // Create union set
    set_union =
        atom::type::FlatSet<int>(union_values.begin(), union_values.end());
    print_set(set_union, "Union");

    // Intersection
    atom::type::FlatSet<int> set_intersection;
    for (const auto& value : set1) {
        if (set2.contains(value)) {
            set_intersection.insert(value);
        }
    }
    print_set(set_intersection, "Intersection");

    // Difference (elements in set1 but not in set2)
    atom::type::FlatSet<int> set_difference;
    for (const auto& value : set1) {
        if (!set2.contains(value)) {
            set_difference.insert(value);
        }
    }
    print_set(set_difference, "Difference (set1 - set2)");

    // Symmetric difference (elements in either set, but not in both)
    atom::type::FlatSet<int> set_symmetric_diff;
    for (const auto& value : set1) {
        if (!set2.contains(value)) {
            set_symmetric_diff.insert(value);
        }
    }
    for (const auto& value : set2) {
        if (!set1.contains(value)) {
            set_symmetric_diff.insert(value);
        }
    }
    print_set(set_symmetric_diff, "Symmetric difference");

    // Check if set1 is subset of union
    bool is_subset = true;
    for (const auto& value : set1) {
        if (!set_union.contains(value)) {
            is_subset = false;
            break;
        }
    }
    std::cout << "Set1 is subset of Union: " << (is_subset ? "yes" : "no")
              << std::endl;

    // Set comparison operators
    atom::type::FlatSet<int> set1_copy = set1;
    std::cout << "set1 == set1_copy: " << (set1 == set1_copy ? "true" : "false")
              << std::endl;
    std::cout << "set1 != set2: " << (set1 != set2 ? "true" : "false")
              << std::endl;
    std::cout << "set1 < set2: " << (set1 < set2 ? "true" : "false")
              << std::endl;
    std::cout << "set1 <= set1_copy: " << (set1 <= set1_copy ? "true" : "false")
              << std::endl;
}

// Example 9: Error Handling
void error_handling() {
    std::cout << "\n=== Example 9: Error Handling ===\n";

    atom::type::FlatSet<int> numbers = {10, 20, 30};

    try {
        // Attempt to erase using an invalid iterator
        auto it = numbers.end();
        numbers.erase(it);
        std::cout << "This line should not be reached\n";
    } catch (const std::invalid_argument& e) {
        std::cout << "Caught expected exception: " << e.what() << std::endl;
    }

    try {
        // Attempt to erase with an invalid range
        auto first = numbers.find(20);
        auto last = first;
        std::advance(last, -1);  // Invalid range (last < first)
        numbers.erase(first, last);
        std::cout << "This line should not be reached\n";
    } catch (const std::invalid_argument& e) {
        std::cout << "Caught expected exception: " << e.what() << std::endl;
    }

    try {
        // Try to insert with an invalid hint
        atom::type::FlatSet<int> other_set = {5, 15, 25};
        auto invalid_hint = other_set.begin();  // Iterator from a different set
        // This won't compile as iterators from different containers are not
        // comparable numbers.insert(invalid_hint, 15);

        // Instead, demonstrate with an out-of-range hint
        auto it = numbers.end();
        it = it + 10;  // Invalid: past-the-end iterator
        numbers.insert(it, 25);
    } catch (const std::invalid_argument& e) {
        std::cout << "Caught expected exception: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught unexpected exception: " << e.what() << std::endl;
    }

    print_set(numbers, "Numbers after error handling");
}

int main() {
    std::cout << "===== FlatSet Usage Examples =====\n";

    // Run all examples
    basic_operations();
    construction_methods();
    custom_types_example();
    iterators_example();
    advanced_insert();
    bounds_and_ranges();
    memory_and_performance();
    set_operations();
    error_handling();

    return 0;
}