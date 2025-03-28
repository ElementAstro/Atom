/*
 * linq_examples.cpp
 *
 * This example demonstrates the usage of Enumerable class that provides
 * LINQ-like operations for C++ collections.
 *
 * Copyright (C) 2024 Example User
 */

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "atom/utils/linq.hpp"

// A simple data structure for demonstration
struct Person {
    std::string name;
    int age;
    std::string city;
    double salary;

    // For printing Person objects
    friend std::ostream& operator<<(std::ostream& os, const Person& p) {
        os << "{Name: " << p.name << ", Age: " << p.age << ", City: " << p.city
           << ", Salary: " << p.salary << "}";
        return os;
    }

    // For comparison operations
    bool operator==(const Person& other) const {
        return name == other.name && age == other.age && city == other.city &&
               salary == other.salary;
    }
};

// Hash function for Person to use in unordered collections
namespace std {
template <>
struct hash<Person> {
    std::size_t operator()(const Person& p) const {
        return std::hash<std::string>()(p.name) ^ std::hash<int>()(p.age) ^
               std::hash<std::string>()(p.city) ^ std::hash<double>()(p.salary);
    }
};
}  // namespace std

// Helper function to print vectors
template <typename T>
void printVector(const std::vector<T>& vec, const std::string& label) {
    std::cout << "=== " << label << " ===" << std::endl;
    for (const auto& item : vec) {
        std::cout << item << std::endl;
    }
    std::cout << std::endl;
}

// Helper to print simple vectors in one line
template <typename T>
void printSimpleVector(const std::vector<T>& vec, const std::string& label) {
    std::cout << "=== " << label << " ===" << std::endl;
    std::cout << "[ ";
    for (const auto& item : vec) {
        std::cout << item << " ";
    }
    std::cout << "]" << std::endl << std::endl;
}

// Helper function to demonstrate a specific LINQ operation
template <typename T>
void demonstrateOperation(const std::vector<T>& /* data */,
                          const std::string& operationName,
                          const std::string& description, auto operation) {
    std::cout << "\n=== Demonstrating: " << operationName
              << " ===" << std::endl;
    std::cout << "Description: " << description << std::endl;

    auto result = operation();

    if constexpr (std::is_same_v<decltype(result), bool> ||
                  std::is_same_v<decltype(result), int> ||
                  std::is_same_v<decltype(result), double> ||
                  std::is_same_v<decltype(result), std::optional<T>>) {
        // For scalar results
        std::cout << "Result: " << result << std::endl;
    } else {
        // For collection results
        std::cout << "Results:" << std::endl;
        for (const auto& item : result.toStdVector()) {
            std::cout << "  " << item << std::endl;
        }
    }
    std::cout << std::endl;
}

int main() {
    // ==========================================
    // Sample data preparation
    // ==========================================

    // Simple numeric list
    std::vector<int> numbers = {1, 5, 3, 9, 8, 6, 7, 2, 0, 4, 5, 3, 1, 8};
    printSimpleVector(numbers, "Original Numbers");

    // Collection of strings
    std::vector<std::string> words = {
        "apple", "banana",   "cherry", "date",   "elderberry", "fig",
        "grape", "honeydew", "apple",  "cherry", "kiwi"};
    printSimpleVector(words, "Original Words");

    // Collection of complex objects
    std::vector<Person> people = {
        {"Alice", 25, "New York", 75000.0},
        {"Bob", 30, "Chicago", 82000.0},
        {"Charlie", 35, "New York", 120000.0},
        {"Diana", 28, "San Francisco", 95000.0},
        {"Eve", 40, "Chicago", 110000.0},
        {"Frank", 22, "New York", 65000.0},
        {"Grace", 31, "San Francisco", 105000.0},
        {"Henry", 45, "Chicago", 130000.0},
        {"Ivy", 27, "New York", 78000.0},
    };
    printVector(people, "Original People");

    // Nested data
    std::vector<std::vector<int>> nested = {
        {1, 2, 3}, {4, 5}, {6, 7, 8, 9}, {10}};

    // ==========================================
    // 1. Basic Filtering Operations
    // ==========================================

    // where - filter elements based on predicate
    demonstrateOperation(numbers, "where", "Filter numbers greater than 5",
                         [&]() {
                             return atom::utils::Enumerable(numbers).where(
                                 [](int n) { return n > 5; });
                         });

    // whereI - filter elements based on predicate with index
    demonstrateOperation(words, "whereI", "Filter words at even indices",
                         [&]() {
                             return atom::utils::Enumerable(words).whereI(
                                 [](const std::string& /* word */,
                                    size_t index) { return index % 2 == 0; });
                         });

    // take - take first n elements
    demonstrateOperation(numbers, "take", "Take the first 5 elements", [&]() {
        return atom::utils::Enumerable(numbers).take(5);
    });

    // takeWhile - take elements while condition is true
    demonstrateOperation(numbers, "takeWhile",
                         "Take elements until we find a number greater than 7",
                         [&]() {
                             return atom::utils::Enumerable(numbers).takeWhile(
                                 [](int n) { return n <= 7; });
                         });

    // takeWhileI - take elements while condition with index is true
    demonstrateOperation(
        words, "takeWhileI",
        "Take words until index reaches 5 or word length exceeds 6", [&]() {
            return atom::utils::Enumerable(words).takeWhileI(
                [](const std::string& word, size_t index) {
                    return index < 5 || word.length() <= 6;
                });
        });

    // skip - skip first n elements
    demonstrateOperation(numbers, "skip", "Skip the first 5 elements", [&]() {
        return atom::utils::Enumerable(numbers).skip(5);
    });

    // skipWhile - skip elements while condition is true
    demonstrateOperation(numbers, "skipWhile",
                         "Skip elements until we find a number greater than 5",
                         [&]() {
                             return atom::utils::Enumerable(numbers).skipWhile(
                                 [](int n) { return n <= 5; });
                         });

    // skipWhileI - skip elements while condition with index is true
    demonstrateOperation(
        words, "skipWhileI",
        "Skip words while index is less than 3 or word length is less than 5",
        [&]() {
            return atom::utils::Enumerable(words).skipWhileI(
                [](const std::string& word, size_t index) {
                    return index < 3 || word.length() < 5;
                });
        });

    // ==========================================
    // 2. Ordering Operations
    // ==========================================

    // orderBy - sort elements (natural order)
    demonstrateOperation(
        numbers, "orderBy (natural order)", "Sort numbers in ascending order",
        [&]() { return atom::utils::Enumerable(numbers).orderBy(); });

    // orderBy with transformer - sort elements by a derived value
    demonstrateOperation(people, "orderBy with transformer",
                         "Sort people by their salary in ascending order",
                         [&]() {
                             return atom::utils::Enumerable(people).orderBy(
                                 [](const Person& p) { return p.salary; });
                         });

    // ==========================================
    // 3. Deduplication Operations
    // ==========================================

    // distinct - remove duplicate elements
    demonstrateOperation(
        numbers, "distinct", "Get distinct numbers from the collection",
        [&]() { return atom::utils::Enumerable(numbers).distinct(); });

    // distinct with transformer - remove elements that have duplicate derived
    // values
    demonstrateOperation(people, "distinct with transformer",
                         "Get people with distinct cities", [&]() {
                             return atom::utils::Enumerable(people).distinct(
                                 [](const Person& p) { return p.city; });
                         });

    // ==========================================
    // 4. Collection Manipulation Operations
    // ==========================================

    // append - add elements to the end
    demonstrateOperation(
        numbers, "append", "Append [100, 200, 300] to the numbers", [&]() {
            return atom::utils::Enumerable(numbers).append({100, 200, 300});
        });

    // prepend - add elements to the beginning
    demonstrateOperation(
        numbers, "prepend", "Prepend [-3, -2, -1] to the numbers", [&]() {
            return atom::utils::Enumerable(numbers).prepend({-3, -2, -1});
        });

    // concat - combine two enumerables
    demonstrateOperation(
        words, "concat", "Concatenate words with another list", [&]() {
            std::vector<std::string> extraWords = {"lemon", "mango", "orange"};
            return atom::utils::Enumerable(words).concat(
                atom::utils::Enumerable(extraWords));
        });

    // reverse - reverse the order of elements
    demonstrateOperation(
        numbers, "reverse", "Reverse the order of numbers",
        [&]() { return atom::utils::Enumerable(numbers).reverse(); });

    // ==========================================
    // 5. Transformation Operations
    // ==========================================

    // select - transform each element
    demonstrateOperation(numbers, "select", "Square each number", [&]() {
        return atom::utils::Enumerable(numbers).select<int>(
            [](int n) { return n * n; });
    });

    // selectI - transform each element using also its index
    demonstrateOperation(
        words, "selectI", "Transform each word to show its index", [&]() {
            return atom::utils::Enumerable(words).selectI<std::string>(
                [](const std::string& word, size_t index) {
                    return std::to_string(index) + ": " + word;
                });
        });

    // cast - cast each element to another type
    demonstrateOperation(numbers, "cast", "Cast integers to doubles", [&]() {
        return atom::utils::Enumerable(numbers).cast<double>();
    });

    // groupBy - group elements by a key
    demonstrateOperation(people, "groupBy", "Group people by city", [&]() {
        return atom::utils::Enumerable(people).groupBy<std::string>(
            [](const Person& p) { return p.city; });
    });

    // selectMany - flatten nested collections
    demonstrateOperation(
        nested, "selectMany", "Flatten a nested list of lists", [&]() {
            return atom::utils::Enumerable(nested).selectMany<int>(
                [](const std::vector<int>& sublist) { return sublist; });
        });

    // ==========================================
    // 6. Aggregation Operations
    // ==========================================

    // all - check if all elements satisfy a condition
    std::cout << "\n=== Demonstrating: all ===" << std::endl;
    std::cout << "Check if all numbers are positive: "
              << (atom::utils::Enumerable(numbers).all(
                      [](int n) { return n >= 0; })
                      ? "Yes"
                      : "No")
              << std::endl
              << std::endl;

    // any - check if any element satisfies a condition
    std::cout << "=== Demonstrating: any ===" << std::endl;
    std::cout << "Check if any number is greater than 10: "
              << (atom::utils::Enumerable(numbers).any(
                      [](int n) { return n > 10; })
                      ? "Yes"
                      : "No")
              << std::endl
              << std::endl;

    // sum - calculate sum of elements
    std::cout << "=== Demonstrating: sum ===" << std::endl;
    std::cout << "Sum of all numbers: "
              << atom::utils::Enumerable(numbers).sum() << std::endl
              << std::endl;

    // sum with transformer - calculate sum of derived values
    std::cout << "=== Demonstrating: sum with transformer ===" << std::endl;
    std::cout << "Sum of all salaries: "
              << atom::utils::Enumerable(people).sum<double>(
                     [](const Person& p) { return p.salary; })
              << std::endl
              << std::endl;

    // avg - calculate average of elements
    std::cout << "=== Demonstrating: avg ===" << std::endl;
    std::cout << "Average of all numbers: "
              << atom::utils::Enumerable(numbers).avg() << std::endl
              << std::endl;

    // avg with transformer - calculate average of derived values
    std::cout << "=== Demonstrating: avg with transformer ===" << std::endl;
    std::cout << "Average age: "
              << atom::utils::Enumerable(people).avg<double>(
                     [](const Person& p) { return p.age; })
              << std::endl
              << std::endl;

    // min - find minimum element
    std::cout << "=== Demonstrating: min ===" << std::endl;
    std::cout << "Minimum number: " << atom::utils::Enumerable(numbers).min()
              << std::endl
              << std::endl;

    // min with transformer - find element with minimum derived value
    std::cout << "=== Demonstrating: min with transformer ===" << std::endl;
    std::cout << "Person with minimum age: " << std::endl;
    std::cout << atom::utils::Enumerable(people).min([](const Person& p) {
        return p.age;
    }) << std::endl
              << std::endl;

    // max - find maximum element
    std::cout << "=== Demonstrating: max ===" << std::endl;
    std::cout << "Maximum number: " << atom::utils::Enumerable(numbers).max()
              << std::endl
              << std::endl;

    // max with transformer - find element with maximum derived value
    std::cout << "=== Demonstrating: max with transformer ===" << std::endl;
    std::cout << "Person with maximum salary: " << std::endl;
    std::cout << atom::utils::Enumerable(people).max([](const Person& p) {
        return p.salary;
    }) << std::endl
              << std::endl;

    // count - count elements
    std::cout << "=== Demonstrating: count ===" << std::endl;
    std::cout << "Number of elements: "
              << atom::utils::Enumerable(numbers).count() << std::endl
              << std::endl;

    // count with predicate - count elements that satisfy a condition
    std::cout << "=== Demonstrating: count with predicate ===" << std::endl;
    std::cout << "Number of people from New York: "
              << atom::utils::Enumerable(people).count(
                     [](const Person& p) { return p.city == "New York"; })
              << std::endl
              << std::endl;

    // contains - check if collection contains an element
    std::cout << "=== Demonstrating: contains ===" << std::endl;
    std::cout << "Collection contains 7: "
              << (atom::utils::Enumerable(numbers).contains(7) ? "Yes" : "No")
              << std::endl
              << std::endl;

    // ==========================================
    // 7. Element Access Operations
    // ==========================================

    // elementAt - get element at a specific index
    std::cout << "=== Demonstrating: elementAt ===" << std::endl;
    std::cout << "Element at index 5: "
              << atom::utils::Enumerable(numbers).elementAt(5) << std::endl
              << std::endl;

    // first - get the first element
    std::cout << "=== Demonstrating: first ===" << std::endl;
    std::cout << "First number: " << atom::utils::Enumerable(numbers).first()
              << std::endl
              << std::endl;

    // first with predicate - get first element that satisfies a condition
    std::cout << "=== Demonstrating: first with predicate ===" << std::endl;
    std::cout << "First person from Chicago: " << std::endl;
    std::cout << atom::utils::Enumerable(people).first([](const Person& p) {
        return p.city == "Chicago";
    }) << std::endl
              << std::endl;

    // firstOrDefault - get first element or default if empty
    std::cout << "=== Demonstrating: firstOrDefault ===" << std::endl;
    auto firstElement = atom::utils::Enumerable(numbers).firstOrDefault();
    std::cout << "First element or default: "
              << (firstElement.has_value()
                      ? std::to_string(firstElement.value())
                      : "none")
              << std::endl
              << std::endl;

    // firstOrDefault with predicate - get first element that satisfies a
    // condition or default
    std::cout << "=== Demonstrating: firstOrDefault with predicate ==="
              << std::endl;
    auto firstPersonOver100K = atom::utils::Enumerable(people).firstOrDefault(
        [](const Person& p) { return p.salary > 100000.0; });
    std::cout << "First person with salary > 100K or default: " << std::endl;
    if (firstPersonOver100K.has_value()) {
        std::cout << firstPersonOver100K.value() << std::endl;
    } else {
        std::cout << "No person found" << std::endl;
    }
    std::cout << std::endl;

    // last - get the last element
    std::cout << "=== Demonstrating: last ===" << std::endl;
    std::cout << "Last number: " << atom::utils::Enumerable(numbers).last()
              << std::endl
              << std::endl;

    // last with predicate - get last element that satisfies a condition
    std::cout << "=== Demonstrating: last with predicate ===" << std::endl;
    std::cout << "Last person under 30: " << std::endl;
    std::cout << atom::utils::Enumerable(people).last([](const Person& p) {
        return p.age < 30;
    }) << std::endl
              << std::endl;

    // lastOrDefault - get last element or default if empty
    std::cout << "=== Demonstrating: lastOrDefault ===" << std::endl;
    auto lastElement = atom::utils::Enumerable(numbers).lastOrDefault();
    std::cout << "Last element or default: "
              << (lastElement.has_value() ? std::to_string(lastElement.value())
                                          : "none")
              << std::endl
              << std::endl;

    // lastOrDefault with predicate - get last element that satisfies a
    // condition or default
    std::cout << "=== Demonstrating: lastOrDefault with predicate ==="
              << std::endl;
    auto lastPersonUnder25 = atom::utils::Enumerable(people).lastOrDefault(
        [](const Person& p) { return p.age < 25; });
    std::cout << "Last person with age < 25 or default: " << std::endl;
    if (lastPersonUnder25.has_value()) {
        std::cout << lastPersonUnder25.value() << std::endl;
    } else {
        std::cout << "No person found" << std::endl;
    }
    std::cout << std::endl;

    // ==========================================
    // 8. Conversion Operations
    // ==========================================

    // toStdSet - convert to std::set
    std::cout << "=== Demonstrating: toStdSet ===" << std::endl;
    auto numberSet = atom::utils::Enumerable(numbers).toStdSet();
    std::cout << "Converted to std::set (size=" << numberSet.size() << "): [ ";
    for (const auto& num : numberSet) {
        std::cout << num << " ";
    }
    std::cout << "]" << std::endl << std::endl;

    // toStdList - convert to std::list
    std::cout << "=== Demonstrating: toStdList ===" << std::endl;
    auto wordList = atom::utils::Enumerable(words).toStdList();
    std::cout << "Converted to std::list (size=" << wordList.size() << "): [ ";
    for (const auto& word : wordList) {
        std::cout << word << " ";
    }
    std::cout << "]" << std::endl << std::endl;

    // toStdDeque - convert to std::deque
    std::cout << "=== Demonstrating: toStdDeque ===" << std::endl;
    auto numberDeque = atom::utils::Enumerable(numbers).toStdDeque();
    std::cout << "Converted to std::deque (size=" << numberDeque.size()
              << "): [ ";
    for (const auto& num : numberDeque) {
        std::cout << num << " ";
    }
    std::cout << "]" << std::endl << std::endl;

    // toStdVector - convert to std::vector
    std::cout << "=== Demonstrating: toStdVector ===" << std::endl;
    auto peopleVector = atom::utils::Enumerable(people).toStdVector();
    std::cout << "Converted to std::vector (size=" << peopleVector.size() << ")"
              << std::endl
              << std::endl;

    return 0;
}