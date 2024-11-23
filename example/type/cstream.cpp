#include "atom/type/cstream.hpp"

#include <iostream>
#include <vector>

using namespace atom::type;

int main() {
    // Create a vector of integers
    std::vector<int> vec = {1, 2, 3, 4, 5};

    // Create a cstream from the vector
    auto stream = makeStream(vec);

    // Sort the vector in descending order
    stream.sorted(std::greater<int>());
    std::cout << "Sorted vector: ";
    for (const auto& val : vec) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Transform the vector by multiplying each element by 2
    auto transformedStream =
        stream.transform<std::vector<int>>([](int x) { return x * 2; });
    std::cout << "Transformed vector: ";
    for (const auto& val : transformedStream.get()) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Remove elements greater than 6
    transformedStream.remove([](int x) { return x > 6; });
    std::cout << "After remove: ";
    for (const auto& val : transformedStream.get()) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Filter elements that are even
    transformedStream.filter([](int x) { return x % 2 == 0; });
    std::cout << "Filtered vector: ";
    for (const auto& val : transformedStream.get()) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Accumulate the elements of the vector
    int sum = transformedStream.accumulate();
    std::cout << "Sum of elements: " << sum << std::endl;

    // Apply a function to each element of the vector
    transformedStream.forEach(
        [](int x) { std::cout << "Element: " << x << std::endl; });

    // Check if all elements are positive
    bool allPositive = transformedStream.all([](int x) { return x > 0; });
    std::cout << "All elements are positive: " << std::boolalpha << allPositive
              << std::endl;

    // Check if any element is greater than 4
    bool anyGreaterThanFour =
        transformedStream.any([](int x) { return x > 4; });
    std::cout << "Any element greater than 4: " << std::boolalpha
              << anyGreaterThanFour << std::endl;

    // Check if no elements are negative
    bool noneNegative = transformedStream.none([](int x) { return x < 0; });
    std::cout << "No elements are negative: " << std::boolalpha << noneNegative
              << std::endl;

    // Get the size of the vector
    std::size_t size = transformedStream.size();
    std::cout << "Size of vector: " << size << std::endl;

    // Count the number of elements greater than 2
    std::size_t countGreaterThanTwo =
        transformedStream.count([](int x) { return x > 2; });
    std::cout << "Count of elements greater than 2: " << countGreaterThanTwo
              << std::endl;

    // Check if the vector contains the value 4
    bool containsFour = transformedStream.contains(4);
    std::cout << "Contains 4: " << std::boolalpha << containsFour << std::endl;

    // Get the minimum element in the vector
    int minElement = transformedStream.min();
    std::cout << "Minimum element: " << minElement << std::endl;

    // Get the maximum element in the vector
    int maxElement = transformedStream.max();
    std::cout << "Maximum element: " << maxElement << std::endl;

    // Calculate the mean of the elements in the vector
    double meanValue = transformedStream.mean();
    std::cout << "Mean value: " << meanValue << std::endl;

    // Get the first element in the vector
    auto firstElement = transformedStream.first();
    if (firstElement) {
        std::cout << "First element: " << *firstElement << std::endl;
    } else {
        std::cout << "Vector is empty" << std::endl;
    }

    // Get the first element greater than 2
    auto firstGreaterThanTwo =
        transformedStream.first([](int x) { return x > 2; });
    if (firstGreaterThanTwo) {
        std::cout << "First element greater than 2: " << *firstGreaterThanTwo
                  << std::endl;
    } else {
        std::cout << "No element greater than 2" << std::endl;
    }

    // Map the elements of the vector to their squares
    auto mappedStream = transformedStream.map([](int x) { return x * x; });
    std::cout << "Mapped vector (squares): ";
    for (const auto& val : mappedStream.get()) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Flat map the elements of the vector to vectors of their digits
    auto flatMappedStream = transformedStream.flatMap([](int x) {
        std::vector<int> digits;
        while (x > 0) {
            digits.push_back(x % 10);
            x /= 10;
        }
        std::reverse(digits.begin(), digits.end());
        return digits;
    });
    std::cout << "Flat mapped vector (digits): ";
    for (const auto& val : flatMappedStream.get()) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Remove duplicate elements from the vector
    transformedStream.distinct();
    std::cout << "Distinct vector: ";
    for (const auto& val : transformedStream.get()) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Reverse the elements of the vector
    transformedStream.reverse();
    std::cout << "Reversed vector: ";
    for (const auto& val : transformedStream.get()) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    return 0;
}