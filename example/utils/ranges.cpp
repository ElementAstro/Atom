#include "atom/utils/ranges.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace atom::utils;

int main() {
    // Example vector of integers
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Filter and transform elements in a range
    auto filteredTransformed = filterAndTransform(
        numbers, [](int x) { return x % 2 == 0; }, [](int x) { return x * 2; });
    std::cout << "Filtered and transformed: ";
    for (auto x : filteredTransformed) {
        std::cout << x << " ";
    }
    std::cout << std::endl;

    // Find an element in a range
    auto foundElement = findElement(numbers, 5);
    if (foundElement) {
        std::cout << "Found element: " << *foundElement << std::endl;
    } else {
        std::cout << "Element not found" << std::endl;
    }

    // Group and aggregate elements in a range
    std::vector<std::pair<std::string, int>> data = {{"apple", 2},
                                                     {"banana", 3},
                                                     {"apple", 1},
                                                     {"cherry", 4},
                                                     {"banana", 1}};
    auto groupedAggregated = groupAndAggregate(
        data, [](const auto& pair) { return pair.first; },
        [](const auto& pair) { return pair.second; });
    std::cout << "Grouped and aggregated: ";
    for (const auto& [key, value] : groupedAggregated) {
        std::cout << key << ": " << value << " ";
    }
    std::cout << std::endl;

    // Drop the first n elements from a range
    auto dropped = drop(numbers, 3);
    std::cout << "Dropped first 3 elements: ";
    for (auto x : dropped) {
        std::cout << x << " ";
    }
    std::cout << std::endl;

    // Take the first n elements from a range
    auto taken = take(numbers, 3);
    std::cout << "Taken first 3 elements: ";
    for (auto x : taken) {
        std::cout << x << " ";
    }
    std::cout << std::endl;

    // Take elements from a range while a predicate is true
    auto takenWhile = takeWhile(numbers, [](int x) { return x < 6; });
    std::cout << "Taken while less than 6: ";
    for (auto x : takenWhile) {
        std::cout << x << " ";
    }
    std::cout << std::endl;

    // Drop elements from a range while a predicate is true
    auto droppedWhile = dropWhile(numbers, [](int x) { return x <= 2; });
    std::cout << "Dropped while less than or equal to 2: ";
    for (auto x : droppedWhile) {
        std::cout << x << " ";
    }
    std::cout << std::endl;

    // Reverse the elements in a range
    auto reversed = reverse(numbers);
    std::cout << "Reversed elements: ";
    for (auto x : reversed) {
        std::cout << x << " ";
    }
    std::cout << std::endl;

    // Accumulate the elements in a range
    int sum = accumulate(numbers, 0, std::plus<>{});
    std::cout << "Sum of elements: " << sum << std::endl;

    // Slice a range into a new range
    auto sliced = slice(numbers.begin(), numbers.end(), 2, 4);
    std::cout << "Sliced elements: ";
    for (auto x : sliced) {
        std::cout << x << " ";
    }
    std::cout << std::endl;

    // Slice a container into a new container
    auto slicedContainer = slice(numbers, 2, 5);
    std::cout << "Sliced container: ";
    for (auto x : slicedContainer) {
        std::cout << x << " ";
    }
    std::cout << std::endl;

    // Merge two ranges
    std::vector<int> numbers1 = {1, 3, 5, 7};
    std::vector<int> numbers2 = {2, 4, 6, 8};
    auto merged = MergeViewImpl{}(numbers1, numbers2);
    std::cout << "Merged ranges: ";
    for (auto x : merged) {
        std::cout << x << " ";
    }
    std::cout << std::endl;

    // Zip multiple ranges
    std::vector<std::string> strings = {"one", "two", "three", "four"};
    auto zipped = ZipViewImpl{}(numbers, strings);
    std::cout << "Zipped ranges: ";
    for (const auto& [num, str] : zipped) {
        std::cout << "(" << num << ", " << str << ") ";
    }
    std::cout << std::endl;

    // Chunk a range into smaller ranges
    auto chunked = ChunkViewImpl{}(numbers, 3);
    std::cout << "Chunked ranges: ";
    for (const auto& chunk : chunked) {
        std::cout << "[";
        for (auto x : chunk) {
            std::cout << x << " ";
        }
        std::cout << "] ";
    }
    std::cout << std::endl;

    // Filter elements in a range
    auto filtered = FilterViewImpl{}(numbers, [](int x) { return x % 2 == 0; });
    std::cout << "Filtered elements: ";
    for (auto x : filtered) {
        std::cout << x << " ";
    }
    std::cout << std::endl;

    // Transform elements in a range
    auto transformed =
        TransformViewImpl{}(numbers, [](int x) { return x * 2; });
    std::cout << "Transformed elements: ";
    for (auto x : transformed) {
        std::cout << x << " ";
    }
    std::cout << std::endl;

    // Get adjacent pairs of elements in a range
    auto adjacent = AdjacentViewImpl{}(numbers);
    std::cout << "Adjacent pairs: ";
    for (const auto& [a, b] : adjacent) {
        std::cout << "(" << a << ", " << b << ") ";
    }
    std::cout << std::endl;

    // Convert a range to a vector
    auto vectorResult = toVector(numbers);
    std::cout << "Converted to vector: ";
    for (auto x : vectorResult) {
        std::cout << x << " ";
    }
    std::cout << std::endl;

    return 0;
}