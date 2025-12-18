/**
 * @file ranges_example.cpp
 * @brief Comprehensive examples for atom::utils ranges utilities
 *
 * This example demonstrates all C++20 ranges utility functions including:
 * - Basic range operations (filterAndTransform, findElement, drop, take)
 * - Conditional operations (takeWhile, dropWhile)
 * - Aggregation (groupAndAggregate, accumulate)
 * - Conversion (toVector, slice)
 * - Coroutine generators (Generator, merge, zip, chunk, filter, transform)
 * - Advanced operations (adjacent, enumerate, flatten)
 */

#include "atom/utils/container/ranges.hpp"

#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

template <typename T>
void printVector(const std::string& label, const std::vector<T>& vec) {
    std::cout << label << ": [";
    bool first = true;
    for (const auto& item : vec) {
        if (!first)
            std::cout << ", ";
        std::cout << item;
        first = false;
    }
    std::cout << "]" << std::endl;
}

// ============================================
// 1. Basic Range Operations
// ============================================
void demonstrateBasicOperations() {
    printSection("1. Basic Range Operations");

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    printVector("Original numbers", numbers);

    // filterAndTransform - Filter and transform in one operation
    std::cout << "\n--- filterAndTransform ---" << std::endl;
    auto evenSquared = filterAndTransform(
        numbers, [](int x) { return x % 2 == 0; }, [](int x) { return x * x; });
    std::cout << "Even numbers squared: [";
    bool first = true;
    for (auto val : evenSquared) {
        if (!first)
            std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "]" << std::endl;

    // findElement - Find element in range
    std::cout << "\n--- findElement ---" << std::endl;
    auto found5 = findElement(numbers, 5);
    if (found5) {
        std::cout << "Found element: " << *found5 << std::endl;
    }

    auto found100 = findElement(numbers, 100);
    if (!found100) {
        std::cout << "Element 100 not found" << std::endl;
    }

    // drop - Skip first n elements
    std::cout << "\n--- drop ---" << std::endl;
    auto dropped = drop(numbers, 3);
    std::cout << "After dropping first 3: [";
    first = true;
    for (auto val : dropped) {
        if (!first)
            std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "]" << std::endl;

    // take - Take first n elements
    std::cout << "\n--- take ---" << std::endl;
    auto taken = take(numbers, 5);
    std::cout << "First 5 elements: [";
    first = true;
    for (auto val : taken) {
        if (!first)
            std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "]" << std::endl;

    // reverse - Reverse the range
    std::cout << "\n--- reverse ---" << std::endl;
    auto reversed = reverse(numbers);
    std::cout << "Reversed: [";
    first = true;
    for (auto val : reversed) {
        if (!first)
            std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "]" << std::endl;
}

// ============================================
// 2. Conditional Operations
// ============================================
void demonstrateConditionalOperations() {
    printSection("2. Conditional Operations");

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    printVector("Original numbers", numbers);

    // takeWhile - Take elements while condition is true
    std::cout << "\n--- takeWhile ---" << std::endl;
    auto takenWhile = takeWhile(numbers, [](int x) { return x < 6; });
    std::cout << "Take while < 6: [";
    bool first = true;
    for (auto val : takenWhile) {
        if (!first)
            std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "]" << std::endl;

    // dropWhile - Drop elements while condition is true
    std::cout << "\n--- dropWhile ---" << std::endl;
    auto droppedWhile = dropWhile(numbers, [](int x) { return x < 6; });
    std::cout << "Drop while < 6: [";
    first = true;
    for (auto val : droppedWhile) {
        if (!first)
            std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "]" << std::endl;

    // Combined operations
    std::cout << "\n--- Combined: dropWhile + takeWhile ---" << std::endl;
    auto combined = takeWhile(dropWhile(numbers, [](int x) { return x < 4; }),
                              [](int x) { return x < 8; });
    std::cout << "Drop while < 4, then take while < 8: [";
    first = true;
    for (auto val : combined) {
        if (!first)
            std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "]" << std::endl;
}

// ============================================
// 3. Aggregation Operations
// ============================================
void demonstrateAggregation() {
    printSection("3. Aggregation Operations");

    // groupAndAggregate example
    std::cout << "--- groupAndAggregate ---" << std::endl;

    struct Sale {
        std::string category;
        double amount;
    };

    std::vector<Sale> sales = {{"Electronics", 1200.0}, {"Clothing", 300.0},
                               {"Electronics", 800.0},  {"Food", 150.0},
                               {"Clothing", 450.0},     {"Food", 200.0},
                               {"Electronics", 500.0}};

    std::cout << "Sales data:" << std::endl;
    for (const auto& sale : sales) {
        std::cout << "  " << sale.category << ": $" << sale.amount << std::endl;
    }

    auto grouped = groupAndAggregate(
        sales, [](const Sale& s) { return s.category; },
        [](const Sale& s) { return s.amount; });

    std::cout << "\nTotal by category:" << std::endl;
    for (const auto& [category, total] : grouped) {
        std::cout << "  " << category << ": $" << total << std::endl;
    }

    // accumulate example
    std::cout << "\n--- accumulate ---" << std::endl;
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    printVector("Numbers", numbers);

    auto sum = accumulate(numbers, 0, std::plus<int>{});
    std::cout << "Sum: " << sum << std::endl;

    auto product = accumulate(numbers, 1, std::multiplies<int>{});
    std::cout << "Product: " << product << std::endl;

    auto maxVal = accumulate(numbers, numbers[0],
                             [](int a, int b) { return std::max(a, b); });
    std::cout << "Max: " << maxVal << std::endl;
}

// ============================================
// 4. Conversion Operations
// ============================================
void demonstrateConversion() {
    printSection("4. Conversion Operations");

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    printVector("Original numbers", numbers);

    // slice - Extract a portion of the container
    std::cout << "\n--- slice ---" << std::endl;
    auto sliced = slice(numbers, 2, 7);
    printVector("Slice [2, 7)", sliced);

    auto slicedFromStart = slice(numbers, 0, 4);
    printVector("Slice [0, 4)", slicedFromStart);

    auto slicedToEnd = slice(numbers, 6, 100);  // Beyond end is handled
    printVector("Slice [6, end)", slicedToEnd);

    // toVector - Convert range to vector
    std::cout << "\n--- toVector ---" << std::endl;
    auto filtered = filterAndTransform(
        numbers, [](int x) { return x % 2 == 0; }, [](int x) { return x; });
    auto asVector = toVector(filtered);
    printVector("Filtered range as vector", asVector);
}

// ============================================
// 5. Generator Examples (Coroutines)
// ============================================
void demonstrateGenerators() {
    printSection("5. Coroutine Generators");

    // merge - Merge two sorted ranges
    std::cout << "--- merge ---" << std::endl;
    std::vector<int> sorted1 = {1, 3, 5, 7, 9};
    std::vector<int> sorted2 = {2, 4, 6, 8, 10};
    printVector("Sorted range 1", sorted1);
    printVector("Sorted range 2", sorted2);

    std::cout << "Merged: [";
    bool first = true;
    for (auto val : merge(sorted1, sorted2)) {
        if (!first)
            std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "]" << std::endl;

    // zip - Zip multiple ranges together
    std::cout << "\n--- zip (generator) ---" << std::endl;
    std::vector<int> ids = {1, 2, 3, 4};
    std::vector<std::string> names = {"Alice", "Bob", "Charlie", "Diana"};
    std::vector<double> scores = {95.5, 87.3, 92.1, 88.7};

    std::cout << "Zipped (id, name, score):" << std::endl;
    for (auto [id, name, score] : zip(ids, names, scores)) {
        std::cout << "  ID: " << id << ", Name: " << name
                  << ", Score: " << score << std::endl;
    }

    // chunk - Split into fixed-size groups
    std::cout << "\n--- chunk ---" << std::endl;
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    printVector("Data", data);

    std::cout << "Chunked into groups of 3:" << std::endl;
    int chunkNum = 1;
    for (auto chunkVec : chunk(data, 3)) {
        std::cout << "  Chunk " << chunkNum++ << ": [";
        first = true;
        for (auto val : chunkVec) {
            if (!first)
                std::cout << ", ";
            std::cout << val;
            first = false;
        }
        std::cout << "]" << std::endl;
    }

    // filter (generator version)
    std::cout << "\n--- filter (generator) ---" << std::endl;
    std::cout << "Even numbers (lazy): [";
    first = true;
    for (auto val : filter(data, [](int x) { return x % 2 == 0; })) {
        if (!first)
            std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "]" << std::endl;

    // transform (generator version)
    std::cout << "\n--- transform (generator) ---" << std::endl;
    std::cout << "Squared values (lazy): [";
    first = true;
    for (auto val : transform(data, [](int x) { return x * x; })) {
        if (!first)
            std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "]" << std::endl;
}

// ============================================
// 6. Advanced Operations
// ============================================
void demonstrateAdvancedOperations() {
    printSection("6. Advanced Operations");

    std::vector<int> numbers = {1, 2, 3, 4, 5};
    printVector("Numbers", numbers);

    // adjacent - Create pairs of adjacent elements
    std::cout << "\n--- adjacent ---" << std::endl;
    std::cout << "Adjacent pairs: [";
    bool first = true;
    for (auto [a, b] : adjacent(numbers)) {
        if (!first)
            std::cout << ", ";
        std::cout << "(" << a << ", " << b << ")";
        first = false;
    }
    std::cout << "]" << std::endl;

    // enumerate - Add indices to elements
    std::cout << "\n--- enumerate ---" << std::endl;
    std::vector<std::string> fruits = {"apple", "banana", "cherry", "date"};
    std::cout << "Enumerated fruits:" << std::endl;
    for (auto [index, fruit] : enumerate(fruits)) {
        std::cout << "  [" << index << "] " << fruit << std::endl;
    }

    // flatten - Flatten nested ranges
    std::cout << "\n--- flatten ---" << std::endl;
    std::vector<std::vector<int>> nested = {{1, 2, 3}, {4, 5}, {6, 7, 8, 9}};
    std::cout << "Nested: [[1,2,3], [4,5], [6,7,8,9]]" << std::endl;
    std::cout << "Flattened: [";
    first = true;
    for (auto val : flatten(nested)) {
        if (!first)
            std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "]" << std::endl;
}

// ============================================
// 7. Complex Use Cases
// ============================================
void demonstrateComplexUseCases() {
    printSection("7. Complex Use Cases");

    // Use case 1: Data processing pipeline
    std::cout << "--- Data Processing Pipeline ---" << std::endl;
    std::vector<int> rawData = {-5, 3, -2, 8, 0, -1, 7, 4, -3, 6};
    printVector("Raw data", rawData);

    // Filter positive -> Take first 5 -> Transform to squares
    std::cout << "Pipeline: filter positive -> take 5 -> square" << std::endl;
    auto positive = filterAndTransform(
        rawData, [](int x) { return x > 0; }, [](int x) { return x; });
    auto firstFive = take(positive, 5);
    std::cout << "Result: [";
    bool first = true;
    for (auto val : firstFive) {
        if (!first)
            std::cout << ", ";
        std::cout << val * val;
        first = false;
    }
    std::cout << "]" << std::endl;

    // Use case 2: Batch processing with chunks
    std::cout << "\n--- Batch Processing ---" << std::endl;
    std::vector<int> items = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    printVector("Items to process", items);

    std::cout << "Processing in batches of 4:" << std::endl;
    int batchNum = 1;
    for (auto batch : chunk(items, 4)) {
        int batchSum = 0;
        for (auto item : batch) {
            batchSum += item;
        }
        std::cout << "  Batch " << batchNum++ << " sum: " << batchSum
                  << std::endl;
    }

    // Use case 3: Difference calculation with adjacent
    std::cout << "\n--- Calculating Differences ---" << std::endl;
    std::vector<double> measurements = {10.0, 12.5, 11.8, 14.2, 13.5, 15.0};
    std::cout << "Measurements: [";
    first = true;
    for (auto m : measurements) {
        if (!first)
            std::cout << ", ";
        std::cout << m;
        first = false;
    }
    std::cout << "]" << std::endl;

    std::cout << "Changes between consecutive measurements:" << std::endl;
    int step = 1;
    for (auto [prev, curr] : adjacent(measurements)) {
        double change = curr - prev;
        std::cout << "  Step " << step++ << ": " << (change >= 0 ? "+" : "")
                  << change << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Ranges Utilities Examples" << std::endl;
    std::cout << "  atom::utils::ranges" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateBasicOperations();
        demonstrateConditionalOperations();
        demonstrateAggregation();
        demonstrateConversion();
        demonstrateGenerators();
        demonstrateAdvancedOperations();
        demonstrateComplexUseCases();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All ranges examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
