#include "atom/utils/linq.hpp"

#include <iostream>
#include <vector>

using namespace atom::utils;

int main() {
    // Example vector of integers
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Create an Enumerable instance
    Enumerable<int> enumerable(numbers);

    // Filter elements based on a predicate
    auto evenNumbers = enumerable.where([](int n) { return n % 2 == 0; });
    std::cout << "Even numbers: ";
    evenNumbers.print();

    // Reduce elements to a single value
    int sum = enumerable.reduce(0, std::plus<>());
    std::cout << "Sum: " << sum << std::endl;

    // Filter elements with index-based predicate
    auto indexedFilter =
        enumerable.whereI([](int n, size_t index) { return index % 2 == 0; });
    std::cout << "Indexed filter: ";
    indexedFilter.print();

    // Take the first N elements
    auto firstThree = enumerable.take(3);
    std::cout << "First three elements: ";
    firstThree.print();

    // Take elements while a predicate is true
    auto takeWhileLessThanFive =
        enumerable.takeWhile([](int n) { return n < 5; });
    std::cout << "Take while less than 5: ";
    takeWhileLessThanFive.print();

    // Skip the first N elements
    auto skipFirstThree = enumerable.skip(3);
    std::cout << "Skip first three elements: ";
    skipFirstThree.print();

    // Skip elements while a predicate is true
    auto skipWhileLessThanFive =
        enumerable.skipWhile([](int n) { return n < 5; });
    std::cout << "Skip while less than 5: ";
    skipWhileLessThanFive.print();

    // Order elements
    auto ordered = enumerable.orderBy();
    std::cout << "Ordered elements: ";
    ordered.print();

    // Order elements by a transformed value
    auto orderedByNegation = enumerable.orderBy([](int n) { return -n; });
    std::cout << "Ordered by negation: ";
    orderedByNegation.print();

    // Get distinct elements
    std::vector<int> duplicates = {1, 2, 2, 3, 3, 3, 4, 4, 4, 4};
    Enumerable<int> duplicatesEnumerable(duplicates);
    auto distinctElements = duplicatesEnumerable.distinct();
    std::cout << "Distinct elements: ";
    distinctElements.print();

    // Append elements to the enumerable
    auto appended = enumerable.append({11, 12, 13});
    std::cout << "Appended elements: ";
    appended.print();

    // Prepend elements to the enumerable
    auto prepended = enumerable.prepend({-2, -1, 0});
    std::cout << "Prepended elements: ";
    prepended.print();

    // Concatenate two enumerables
    auto concatenated = enumerable.concat(duplicatesEnumerable);
    std::cout << "Concatenated elements: ";
    concatenated.print();

    // Reverse elements
    auto reversed = enumerable.reverse();
    std::cout << "Reversed elements: ";
    reversed.print();

    // Cast elements to another type
    auto casted = enumerable.cast<double>();
    std::cout << "Casted elements: ";
    casted.print();

    // Transform elements using a function
    auto squared = enumerable.select<int>([](int n) { return n * n; });
    std::cout << "Squared elements: ";
    squared.print();

    // Group elements by a transformed value
    auto grouped = enumerable.groupBy<int>([](int n) { return n % 3; });
    std::cout << "Grouped elements: ";
    grouped.print();

    // Flatten nested vectors
    std::vector<std::vector<int>> nested = {{1, 2}, {3, 4}, {5, 6}};
    auto flattened = Enumerable(nested).selectMany<int>(
        [](const std::vector<int>& v) { return v; });
    std::cout << "Flattened elements: ";
    flattened.print();

    // Check if all elements satisfy a predicate
    bool allEven = evenNumbers.all([](int n) { return n % 2 == 0; });
    std::cout << "All even: " << std::boolalpha << allEven << std::endl;

    // Check if any element satisfies a predicate
    bool anyEven = enumerable.any([](int n) { return n % 2 == 0; });
    std::cout << "Any even: " << std::boolalpha << anyEven << std::endl;

    // Calculate the sum of elements
    int totalSum = enumerable.sum();
    std::cout << "Total sum: " << totalSum << std::endl;

    // Calculate the average of elements
    double average = enumerable.avg();
    std::cout << "Average: " << average << std::endl;

    // Find the minimum element
    int minElement = enumerable.min();
    std::cout << "Min element: " << minElement << std::endl;

    // Find the maximum element
    int maxElement = enumerable.max();
    std::cout << "Max element: " << maxElement << std::endl;

    // Count the number of elements
    size_t count = enumerable.count();
    std::cout << "Count: " << count << std::endl;

    // Check if the enumerable contains a specific value
    bool containsFive = enumerable.contains(5);
    std::cout << "Contains 5: " << std::boolalpha << containsFive << std::endl;

    // Get the element at a specific index
    int elementAtIndex = enumerable.elementAt(3);
    std::cout << "Element at index 3: " << elementAtIndex << std::endl;

    // Get the first element
    int firstElement = enumerable.first();
    std::cout << "First element: " << firstElement << std::endl;

    // Get the last element
    int lastElement = enumerable.last();
    std::cout << "Last element: " << lastElement << std::endl;

    // Convert to std::set
    auto stdSet = enumerable.toStdSet();
    std::cout << "Converted to std::set: ";
    for (const auto& elem : stdSet) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Convert to std::list
    auto stdList = enumerable.toStdList();
    std::cout << "Converted to std::list: ";
    for (const auto& elem : stdList) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Convert to std::deque
    auto stdDeque = enumerable.toStdDeque();
    std::cout << "Converted to std::deque: ";
    for (const auto& elem : stdDeque) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Convert to std::vector
    auto stdVector = enumerable.toStdVector();
    std::cout << "Converted to std::vector: ";
    for (const auto& elem : stdVector) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    return 0;
}