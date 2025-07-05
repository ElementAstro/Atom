/*
 * ranges_example.cpp
 *
 * Copyright (C) 2024 Max Q.
 *
 * Example usage of the atom::utils ranges utilities
 */

#include "atom/utils/ranges.hpp"

#include <iostream>
#include <string>
#include <vector>

// Helper function to print containers
template <typename Container>
void printContainer(const Container& container, const std::string& label) {
    std::cout << label << ": [";
    bool first = true;
    for (const auto& item : container) {
        if (!first) {
            std::cout << ", ";
        }
        std::cout << item;
        first = false;
    }
    std::cout << "]\n";
}

// Helper function to print pairs
template <typename T, typename U>
std::ostream& operator<<(std::ostream& os, const std::pair<T, U>& pair) {
    os << "(" << pair.first << ", " << pair.second << ")";
    return os;
}

// Helper function to print tuples
template <typename... Ts>
std::ostream& operator<<(std::ostream& os, const std::tuple<Ts...>& tuple) {
    os << "(";
    std::apply(
        [&os](const auto&... args) {
            size_t i = 0;
            ((os << (i++ ? ", " : "") << args), ...);
        },
        tuple);
    os << ")";
    return os;
}

int main() {
    std::cout << "=== Atom Range Utilities Examples ===\n\n";

    // Basic data for examples
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<std::string> fruits = {"apple", "banana", "cherry", "date",
                                       "elderberry"};
    std::vector<std::pair<std::string, int>> items = {
        {"apple", 5}, {"banana", 3}, {"cherry", 8},
        {"apple", 2}, {"date", 4},   {"banana", 1}};

    std::cout << "Example 1: filterAndTransform\n";
    // Filter even numbers and multiply by 2
    auto filtered_transformed = atom::utils::filterAndTransform(
        numbers, [](int x) { return x % 2 == 0; },  // Filter predicate
        [](int x) { return x * 2; }                 // Transform function
    );

    std::cout << "Even numbers doubled: ";
    for (int val : filtered_transformed) {
        std::cout << val << " ";
    }
    std::cout << "\n\n";

    std::cout << "Example 2: findElement\n";
    // Find an element in the container
    auto found_element = atom::utils::findElement(fruits, "cherry");
    if (found_element) {
        std::cout << "Found element: " << *found_element << "\n";
    } else {
        std::cout << "Element not found\n";
    }

    auto not_found = atom::utils::findElement(fruits, "mango");
    if (not_found) {
        std::cout << "Found element: " << *not_found << "\n";
    } else {
        std::cout << "Element 'mango' not found\n";
    }
    std::cout << "\n";

    std::cout << "Example 3: groupAndAggregate\n";
    // Group items by name and sum their quantities
    auto grouped_items = atom::utils::groupAndAggregate(
        items, [](const auto& item) { return item.first; },  // Key selector
        [](const auto& item) { return item.second; }  // Value to aggregate
    );

    std::cout << "Grouped items by name with summed quantities:\n";
    for (const auto& [name, quantity] : grouped_items) {
        std::cout << "  " << name << ": " << quantity << "\n";
    }
    std::cout << "\n";

    std::cout << "Example 4: drop and take\n";
    // Drop the first 3 elements
    auto skipped = atom::utils::drop(numbers, 3);
    std::cout << "After dropping first 3 elements: ";
    for (int val : skipped) {
        std::cout << val << " ";
    }
    std::cout << "\n";

    // Take the first 4 elements
    auto taken = atom::utils::take(numbers, 4);
    std::cout << "Taking first 4 elements: ";
    for (int val : taken) {
        std::cout << val << " ";
    }
    std::cout << "\n\n";

    std::cout << "Example 5: takeWhile and dropWhile\n";
    // Take elements while less than 6
    auto taken_while =
        atom::utils::takeWhile(numbers, [](int x) { return x < 6; });
    std::cout << "Elements taken while < 6: ";
    for (int val : taken_while) {
        std::cout << val << " ";
    }
    std::cout << "\n";

    // Drop elements while less than 6
    auto dropped_while =
        atom::utils::dropWhile(numbers, [](int x) { return x < 6; });
    std::cout << "Elements remaining after dropping while < 6: ";
    for (int val : dropped_while) {
        std::cout << val << " ";
    }
    std::cout << "\n\n";

    std::cout << "Example 6: reverse\n";
    // Reverse a range
    auto reversed = atom::utils::reverse(numbers);
    std::cout << "Reversed numbers: ";
    for (int val : reversed) {
        std::cout << val << " ";
    }
    std::cout << "\n\n";

    std::cout << "Example 7: accumulate\n";
    // Calculate sum using accumulate
    auto sum = atom::utils::accumulate(numbers, 0, std::plus<>());
    std::cout << "Sum of numbers: " << sum << "\n";

    // Calculate product using accumulate
    auto product = atom::utils::accumulate(numbers, 1, std::multiplies<>());
    std::cout << "Product of numbers: " << product << "\n\n";

    std::cout << "Example 8: slice\n";
    // Slice using iterators
    auto sliced_iter = atom::utils::slice(numbers.begin(), numbers.end(), 2, 4);
    std::cout << "Slice from index 2 with length 4: ";
    for (int val : sliced_iter) {
        std::cout << val << " ";
    }
    std::cout << "\n";

    // Slice using container
    auto sliced_container = atom::utils::slice(numbers, 3, 7);
    std::cout << "Slice from index 3 to 7: ";
    for (int val : sliced_container) {
        std::cout << val << " ";
    }
    std::cout << "\n\n";

    std::cout << "Example 9: Generator usage\n";
    // Using a simple generator function
    auto fibonacci_generator = [](int n) -> atom::utils::generator<int> {
        int a = 0, b = 1;
        for (int i = 0; i < n; ++i) {
            co_yield a;
            int next = a + b;
            a = b;
            b = next;
        }
    };

    std::cout << "First 10 Fibonacci numbers: ";
    for (int fib : fibonacci_generator(10)) {
        std::cout << fib << " ";
    }
    std::cout << "\n\n";

    std::cout << "Example 10: MergeViewImpl\n";
    // Merge two sorted ranges
    std::vector<int> sorted1 = {1, 3, 5, 7, 9};
    std::vector<int> sorted2 = {2, 4, 6, 8, 10};

    atom::utils::MergeViewImpl mergeView;
    auto merged = mergeView(sorted1, sorted2);

    std::cout << "Merged sorted sequences: ";
    for (int val : merged) {
        std::cout << val << " ";
    }
    std::cout << "\n\n";

    std::cout << "Example 11: ZipViewImpl\n";
    // Zip multiple sequences together
    std::vector<int> ids = {1, 2, 3, 4};
    std::vector<std::string> names = {"Alice", "Bob", "Charlie", "David"};
    std::vector<double> scores = {92.5, 87.3, 95.1, 82.7};

    atom::utils::ZipViewImpl zipView;
    auto zipped = zipView(ids, names, scores);

    std::cout << "Zipped sequences (id, name, score):\n";
    for (const auto& item : zipped) {
        std::cout << "  " << item << "\n";
    }
    std::cout << "\n";

    std::cout << "Example 12: ChunkViewImpl\n";
    // Split a sequence into chunks
    std::vector<int> sequence = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

    atom::utils::ChunkViewImpl chunkView;
    auto chunked = chunkView(sequence, 3);

    std::cout << "Sequence chunked into groups of 3:\n";
    for (const auto& chunk : chunked) {
        std::cout << "  ";
        for (int val : chunk) {
            std::cout << val << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n";

    std::cout << "Example 13: FilterViewImpl\n";
    // Filter elements using FilterViewImpl
    atom::utils::FilterViewImpl filterView;
    auto filtered = filterView(
        numbers, [](int x) { return x % 2 == 1; });  // Filter odd numbers

    std::cout << "Odd numbers using FilterViewImpl: ";
    for (int val : filtered) {
        std::cout << val << " ";
    }
    std::cout << "\n\n";

    std::cout << "Example 14: TransformViewImpl\n";
    // Transform elements using TransformViewImpl
    atom::utils::TransformViewImpl transformView;
    auto transformed = transformView(
        numbers, [](int x) { return x * x; });  // Square each number

    std::cout << "Squared numbers using TransformViewImpl: ";
    for (int val : transformed) {
        std::cout << val << " ";
    }
    std::cout << "\n\n";

    std::cout << "Example 15: AdjacentViewImpl\n";
    // Get adjacent pairs
    atom::utils::AdjacentViewImpl adjacentView;
    auto adjacent_pairs = adjacentView(numbers);

    std::cout << "Adjacent pairs: ";
    for (const auto& pair : adjacent_pairs) {
        std::cout << pair << " ";
    }
    std::cout << "\n\n";

    std::cout << "Example 16: toVector\n";
    // Convert a range to vector
    auto doubled = transformView(numbers, [](int x) { return x * 2; });
    auto doubled_vec = atom::utils::toVector(doubled);

    std::cout << "Transformed range converted to vector: ";
    for (int val : doubled_vec) {
        std::cout << val << " ";
    }
    std::cout << "\n\n";

    std::cout << "Example 17: Chaining multiple operations\n";
    // Chain multiple range operations together
    auto complex_operation = atom::utils::filterAndTransform(
        numbers, [](int x) { return x > 3; },  // Filter values > 3
        [](int x) { return x * x; }            // Square the values
    );

    auto result_vector = atom::utils::toVector(complex_operation);
    std::cout << "Result of filtering values > 3 and squaring them: ";
    for (int val : result_vector) {
        std::cout << val << " ";
    }
    std::cout << "\n\n";

    return 0;
}
