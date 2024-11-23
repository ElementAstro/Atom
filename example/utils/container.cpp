#include "atom/utils/container.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

using namespace atom::utils;

int main() {
    // Example containers
    std::vector<int> container1 = {1, 2, 3, 4, 5};
    std::vector<int> container2 = {4, 5, 6, 7, 8};

    // Check if one container is a subset of another container
    bool isSubsetResult = isSubset(container1, container2);
    std::cout << "Container1 is a subset of Container2: " << std::boolalpha
              << isSubsetResult << std::endl;

    // Check if a container contains a specific element
    bool containsResult = contains(container1, 3);
    std::cout << "Container1 contains 3: " << std::boolalpha << containsResult
              << std::endl;

    // Convert a container to an unordered_set for fast lookup
    std::unordered_set<int> unorderedSet = toUnorderedSet(container1);
    std::cout << "Unordered set: ";
    for (const auto& elem : unorderedSet) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Check if one container is a subset of another container using linear
    // search
    bool isSubsetLinearSearchResult =
        isSubsetLinearSearch(container1, container2);
    std::cout << "Container1 is a subset of Container2 (linear search): "
              << std::boolalpha << isSubsetLinearSearchResult << std::endl;

    // Check if one container is a subset of another container using an
    // unordered_set for fast lookup
    bool isSubsetWithHashSetResult =
        isSubsetWithHashSet(container1, container2);
    std::cout << "Container1 is a subset of Container2 (hash set): "
              << std::boolalpha << isSubsetWithHashSetResult << std::endl;

    // Get the intersection of two containers
    auto intersectionResult = intersection(container1, container2);
    std::cout << "Intersection of Container1 and Container2: ";
    for (const auto& elem : intersectionResult) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Get the union of two containers
    auto unionResult = unionSet(container1, container2);
    std::cout << "Union of Container1 and Container2: ";
    for (const auto& elem : unionResult) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Get the difference of two containers (container1 - container2)
    auto differenceResult = difference(container1, container2);
    std::cout << "Difference of Container1 and Container2: ";
    for (const auto& elem : differenceResult) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Get the symmetric difference of two containers
    auto symmetricDifferenceResult =
        symmetricDifference(container1, container2);
    std::cout << "Symmetric difference of Container1 and Container2: ";
    for (const auto& elem : symmetricDifferenceResult) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Check if two containers are equal
    bool isEqualResult = isEqual(container1, container2);
    std::cout << "Container1 is equal to Container2: " << std::boolalpha
              << isEqualResult << std::endl;

    // Apply a member function to each element in a container and store the
    // results
    struct Example {
        int value;
        int getValue() const { return value; }
    };
    std::vector<Example> exampleContainer = {{1}, {2}, {3}};
    auto appliedResult = applyAndStore(exampleContainer, &Example::getValue);
    std::cout << "Applied member function results: ";
    for (const auto& elem : appliedResult) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Transform a container to a vector by applying a member function to each
    // element
    auto transformedResult =
        transformToVector(exampleContainer, &Example::getValue);
    std::cout << "Transformed to vector results: ";
    for (const auto& elem : transformedResult) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Remove duplicate elements from a container
    std::vector<int> duplicateContainer = {1, 2, 2, 3, 3, 3, 4};
    auto uniqueResult = unique(duplicateContainer);
    std::cout << "Unique elements: ";
    for (const auto& elem : uniqueResult) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Flatten a container of containers into a single container
    std::vector<std::vector<int>> nestedContainer = {{1, 2}, {3, 4}, {5}};
    auto flattenedResult = flatten(nestedContainer);
    std::cout << "Flattened container: ";
    for (const auto& elem : flattenedResult) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Zip two containers into a container of pairs
    std::vector<std::string> container3 = {"a", "b", "c"};
    auto zippedResult = zip(container1, container3);
    std::cout << "Zipped container: ";
    for (const auto& [first, second] : zippedResult) {
        std::cout << "(" << first << ", " << second << ") ";
    }
    std::cout << std::endl;

    // Compute the Cartesian product of two containers
    auto cartesianProductResult = cartesianProduct(container1, container3);
    std::cout << "Cartesian product: ";
    for (const auto& [first, second] : cartesianProductResult) {
        std::cout << "(" << first << ", " << second << ") ";
    }
    std::cout << std::endl;

    // Filter elements in a container based on a predicate
    auto filteredResult =
        filter(container1, [](int value) { return value % 2 == 0; });
    std::cout << "Filtered container (even numbers): ";
    for (const auto& elem : filteredResult) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Partition a container into two containers based on a predicate
    auto [truePart, falsePart] =
        partition(container1, [](int value) { return value % 2 == 0; });
    std::cout << "Partitioned container (even numbers): ";
    for (const auto& elem : truePart) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;
    std::cout << "Partitioned container (odd numbers): ";
    for (const auto& elem : falsePart) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Find the first element in a container that satisfies a predicate
    auto foundElement = findIf(container1, [](int value) { return value > 3; });
    if (foundElement) {
        std::cout << "First element greater than 3: " << *foundElement
                  << std::endl;
    } else {
        std::cout << "No element greater than 3 found." << std::endl;
    }

    return 0;
}