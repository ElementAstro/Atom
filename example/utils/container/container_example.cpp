/**
 * @file container_example.cpp
 * @brief Comprehensive examples for atom::utils container utilities
 *
 * This example demonstrates all container utility functions including:
 * - Set operations (isSubset, intersection, union, difference, symmetricDifference)
 * - Container manipulation (contains, unique, flatten, zip, cartesianProduct)
 * - Filtering and partitioning (filter, partition, findIf)
 * - Transformation (applyAndStore, transformToVector)
 * - Equality checking (isEqual)
 */

#include "atom/utils/container/container.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace atom::utils;

// Helper function to print vectors
template <typename Container>
void printVector(const std::string& label, const Container& vec) {
    std::cout << label << ": [";
    bool first = true;
    for (const auto& item : vec) {
        if (!first) std::cout << ", ";
        std::cout << item;
        first = false;
    }
    std::cout << "]" << std::endl;
}

// Helper function to print pairs
template <typename T1, typename T2>
void printPairs(const std::string& label,
                const std::vector<std::pair<T1, T2>>& pairs) {
    std::cout << label << ": [";
    bool first = true;
    for (const auto& [a, b] : pairs) {
        if (!first) std::cout << ", ";
        std::cout << "(" << a << ", " << b << ")";
        first = false;
    }
    std::cout << "]" << std::endl;
}

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

// ============================================
// 1. Set Operations Examples
// ============================================
void demonstrateSetOperations() {
    printSection("1. Set Operations");

    Vector<int> setA = {1, 2, 3, 4, 5};
    Vector<int> setB = {3, 4, 5, 6, 7};
    Vector<int> subset = {2, 3, 4};

    printVector("Set A", setA);
    printVector("Set B", setB);
    printVector("Subset", subset);

    // isSubset - Check if one container is a subset of another
    std::cout << "\n--- isSubset ---" << std::endl;
    bool isSubsetResult = isSubset(subset, setA);
    std::cout << "Is {2,3,4} a subset of A? " << (isSubsetResult ? "Yes" : "No")
              << std::endl;

    bool notSubset = isSubset(setB, setA);
    std::cout << "Is B a subset of A? " << (notSubset ? "Yes" : "No")
              << std::endl;

    // isSubsetLinearSearch - Linear search version
    std::cout << "\n--- isSubsetLinearSearch ---" << std::endl;
    bool linearResult = isSubsetLinearSearch(subset, setA);
    std::cout << "Linear search: Is {2,3,4} a subset of A? "
              << (linearResult ? "Yes" : "No") << std::endl;

    // isSubsetWithHashSet - Hash set version (same as isSubset)
    std::cout << "\n--- isSubsetWithHashSet ---" << std::endl;
    bool hashResult = isSubsetWithHashSet(subset, setA);
    std::cout << "Hash set: Is {2,3,4} a subset of A? "
              << (hashResult ? "Yes" : "No") << std::endl;

    // intersection - Elements in both containers
    std::cout << "\n--- intersection ---" << std::endl;
    auto intersectionResult = intersection(setA, setB);
    printVector("A ∩ B", intersectionResult);

    // unionSet - Elements in either container
    std::cout << "\n--- unionSet ---" << std::endl;
    auto unionResult = unionSet(setA, setB);
    printVector("A ∪ B", unionResult);

    // difference - Elements in first but not second
    std::cout << "\n--- difference ---" << std::endl;
    auto diffAB = difference(setA, setB);
    auto diffBA = difference(setB, setA);
    printVector("A - B", diffAB);
    printVector("B - A", diffBA);

    // symmetricDifference - Elements in either but not both
    std::cout << "\n--- symmetricDifference ---" << std::endl;
    auto symDiff = symmetricDifference(setA, setB);
    printVector("A △ B (symmetric difference)", symDiff);

    // isEqual - Check if containers have same elements (any order)
    std::cout << "\n--- isEqual ---" << std::endl;
    Vector<int> shuffled = {5, 3, 1, 4, 2};
    bool equalResult = isEqual(setA, shuffled);
    std::cout << "Is {1,2,3,4,5} equal to {5,3,1,4,2}? "
              << (equalResult ? "Yes" : "No") << std::endl;
}

// ============================================
// 2. Container Manipulation Examples
// ============================================
void demonstrateContainerManipulation() {
    printSection("2. Container Manipulation");

    Vector<int> numbers = {1, 2, 3, 4, 5};
    Vector<char> letters = {'a', 'b', 'c', 'd', 'e'};

    // contains - Check if container contains a value
    std::cout << "--- contains ---" << std::endl;
    std::cout << "Does {1,2,3,4,5} contain 3? "
              << (contains(numbers, 3) ? "Yes" : "No") << std::endl;
    std::cout << "Does {1,2,3,4,5} contain 10? "
              << (contains(numbers, 10) ? "Yes" : "No") << std::endl;

    // toHashSet / toUnorderedSet - Convert to hash set
    std::cout << "\n--- toHashSet / toUnorderedSet ---" << std::endl;
    Vector<int> withDuplicates = {1, 2, 2, 3, 3, 3, 4};
    auto hashSet = toHashSet(withDuplicates);
    std::cout << "Original: ";
    printVector("", withDuplicates);
    std::cout << "As HashSet (unique elements): size = " << hashSet.size()
              << std::endl;

    // unique - Remove duplicate elements
    std::cout << "\n--- unique ---" << std::endl;
    auto uniqueResult = unique(withDuplicates);
    printVector("Unique elements", uniqueResult);

    // flatten - Flatten nested containers
    std::cout << "\n--- flatten ---" << std::endl;
    Vector<Vector<int>> nested = {{1, 2, 3}, {4, 5}, {6, 7, 8, 9}};
    std::cout << "Nested: [[1,2,3], [4,5], [6,7,8,9]]" << std::endl;
    auto flattened = flatten(nested);
    printVector("Flattened", flattened);

    // zip - Combine two containers into pairs
    std::cout << "\n--- zip ---" << std::endl;
    auto zipped = zip(numbers, letters);
    printPairs("Zipped (numbers, letters)", zipped);

    // zip with different sizes
    Vector<int> shortNumbers = {1, 2, 3};
    auto zippedShort = zip(shortNumbers, letters);
    printPairs("Zipped (short numbers, letters)", zippedShort);

    // cartesianProduct - All combinations of elements
    std::cout << "\n--- cartesianProduct ---" << std::endl;
    Vector<int> small1 = {1, 2};
    Vector<char> small2 = {'a', 'b', 'c'};
    auto product = cartesianProduct(small1, small2);
    printPairs("Cartesian product of {1,2} x {a,b,c}", product);
}

// ============================================
// 3. Filtering and Partitioning Examples
// ============================================
void demonstrateFilteringAndPartitioning() {
    printSection("3. Filtering and Partitioning");

    Vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    printVector("Original numbers", numbers);

    // filter - Keep elements matching predicate
    std::cout << "\n--- filter ---" << std::endl;
    auto evenNumbers = filter(numbers, [](int x) { return x % 2 == 0; });
    printVector("Even numbers", evenNumbers);

    auto greaterThan5 = filter(numbers, [](int x) { return x > 5; });
    printVector("Numbers > 5", greaterThan5);

    // partition - Split into two groups based on predicate
    std::cout << "\n--- partition ---" << std::endl;
    auto [evens, odds] = partition(numbers, [](int x) { return x % 2 == 0; });
    printVector("Even partition", evens);
    printVector("Odd partition", odds);

    auto [small, large] = partition(numbers, [](int x) { return x <= 5; });
    printVector("Small (<=5)", small);
    printVector("Large (>5)", large);

    // findIf - Find first element matching predicate
    std::cout << "\n--- findIf ---" << std::endl;
    auto firstEven = findIf(numbers, [](int x) { return x % 2 == 0; });
    if (firstEven) {
        std::cout << "First even number: " << *firstEven << std::endl;
    }

    auto firstGreater100 = findIf(numbers, [](int x) { return x > 100; });
    if (firstGreater100) {
        std::cout << "First number > 100: " << *firstGreater100 << std::endl;
    } else {
        std::cout << "No number > 100 found" << std::endl;
    }

    // Find with complex predicate
    auto firstDivisibleBy3 = findIf(numbers, [](int x) { return x % 3 == 0; });
    if (firstDivisibleBy3) {
        std::cout << "First number divisible by 3: " << *firstDivisibleBy3
                  << std::endl;
    }
}

// ============================================
// 4. Transformation Examples
// ============================================
void demonstrateTransformation() {
    printSection("4. Transformation");

    // Sample data structure
    struct Person {
        std::string name;
        int age;
        double salary;
    };

    Vector<Person> people = {{"Alice", 30, 75000.0},
                             {"Bob", 25, 60000.0},
                             {"Charlie", 35, 90000.0},
                             {"Diana", 28, 70000.0}};

    std::cout << "People data:" << std::endl;
    for (const auto& p : people) {
        std::cout << "  " << p.name << ", age " << p.age << ", salary $"
                  << p.salary << std::endl;
    }

    // applyAndStore - Extract member values
    std::cout << "\n--- applyAndStore ---" << std::endl;
    auto ages = applyAndStore(people, &Person::age);
    printVector("Ages", ages);

    auto salaries = applyAndStore(people, &Person::salary);
    std::cout << "Salaries: [";
    bool first = true;
    for (const auto& s : salaries) {
        if (!first) std::cout << ", ";
        std::cout << "$" << s;
        first = false;
    }
    std::cout << "]" << std::endl;

    // transformToVector - Transform using member function
    std::cout << "\n--- transformToVector ---" << std::endl;
    Vector<std::string> strings = {"hello", "world", "example", "test"};
    printVector("Strings", strings);

    auto lengths = transformToVector(strings, &std::string::size);
    std::cout << "String lengths: [";
    first = true;
    for (const auto& len : lengths) {
        if (!first) std::cout << ", ";
        std::cout << len;
        first = false;
    }
    std::cout << "]" << std::endl;
}

// ============================================
// 5. String Literal Operator Example
// ============================================
void demonstrateStringLiteralOperator() {
    printSection("5. String Literal Operator");

    // Using the _vec literal operator
    auto fruits = "apple, banana, cherry, date, elderberry"_vec;
    std::cout << "Created vector from string literal:" << std::endl;
    std::cout << "  \"apple, banana, cherry, date, elderberry\"_vec" << std::endl;
    std::cout << "Result: [";
    bool first = true;
    for (const auto& fruit : fruits) {
        if (!first) std::cout << ", ";
        std::cout << "\"" << fruit << "\"";
        first = false;
    }
    std::cout << "]" << std::endl;

    // Another example with spaces
    auto colors = "red, green, blue"_vec;
    std::cout << "\nColors: [";
    first = true;
    for (const auto& color : colors) {
        if (!first) std::cout << ", ";
        std::cout << "\"" << color << "\"";
        first = false;
    }
    std::cout << "]" << std::endl;
}

// ============================================
// 6. Complex Use Cases
// ============================================
void demonstrateComplexUseCases() {
    printSection("6. Complex Use Cases");

    // Use case 1: Data pipeline
    std::cout << "--- Data Pipeline Example ---" << std::endl;
    Vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    printVector("Original data", data);

    // Filter -> Partition -> Process
    auto filtered = filter(data, [](int x) { return x > 3; });
    printVector("After filter (>3)", filtered);

    auto [divisibleBy2, notDivisibleBy2] =
        partition(filtered, [](int x) { return x % 2 == 0; });
    printVector("Divisible by 2", divisibleBy2);
    printVector("Not divisible by 2", notDivisibleBy2);

    // Use case 2: Set operations for data analysis
    std::cout << "\n--- Set Operations for Data Analysis ---" << std::endl;
    Vector<std::string> usersA = {"alice", "bob", "charlie", "diana"};
    Vector<std::string> usersB = {"bob", "diana", "eve", "frank"};

    printVector("Users in System A", usersA);
    printVector("Users in System B", usersB);

    auto commonUsers = intersection(usersA, usersB);
    printVector("Common users (in both)", commonUsers);

    auto allUsers = unionSet(usersA, usersB);
    printVector("All unique users", allUsers);

    auto onlyInA = difference(usersA, usersB);
    printVector("Only in System A", onlyInA);

    auto onlyInB = difference(usersB, usersA);
    printVector("Only in System B", onlyInB);

    // Use case 3: Combining data from multiple sources
    std::cout << "\n--- Combining Data Sources ---" << std::endl;
    Vector<int> ids = {1, 2, 3, 4};
    Vector<std::string> names = {"Product A", "Product B", "Product C",
                                  "Product D"};

    auto combined = zip(ids, names);
    std::cout << "Combined product data:" << std::endl;
    for (const auto& [id, name] : combined) {
        std::cout << "  ID: " << id << " -> " << name << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Container Utilities Examples" << std::endl;
    std::cout << "  atom::utils::container" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateSetOperations();
        demonstrateContainerManipulation();
        demonstrateFilteringAndPartitioning();
        demonstrateTransformation();
        demonstrateStringLiteralOperator();
        demonstrateComplexUseCases();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All container examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
