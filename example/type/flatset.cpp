#include "atom/type/flatset.hpp"

#include <iostream>
#include <string>

using namespace atom::type;

int main() {
    // Create a FlatSet object
    FlatSet<int> set;

    // Insert elements into the set
    set.insert(10);
    set.insert(20);
    set.insert(30);
    std::cout << "Inserted 10, 20, 30 into the set" << std::endl;

    // Insert elements using an initializer list
    set.insert({40, 50, 60});
    std::cout << "Inserted 40, 50, 60 into the set" << std::endl;

    // Check if the set contains an element
    bool contains20 = set.contains(20);
    std::cout << "Set contains 20: " << std::boolalpha << contains20
              << std::endl;

    // Find an element in the set
    auto it = set.find(30);
    if (it != set.end()) {
        std::cout << "Found 30 in the set" << std::endl;
    } else {
        std::cout << "30 not found in the set" << std::endl;
    }

    // Erase an element from the set
    set.erase(20);
    std::cout << "Erased 20 from the set" << std::endl;

    // Check the size of the set
    std::size_t size = set.size();
    std::cout << "Size of the set: " << size << std::endl;

    // Check if the set is empty
    bool isEmpty = set.empty();
    std::cout << "Set is empty: " << std::boolalpha << isEmpty << std::endl;

    // Iterate over the set
    std::cout << "Set contents:" << std::endl;
    for (const auto& value : set) {
        std::cout << value << " ";
    }
    std::cout << std::endl;

    // Use lowerBound to find the first element not less than a given value
    auto lower = set.lowerBound(25);
    if (lower != set.end()) {
        std::cout << "Lower bound of 25: " << *lower << std::endl;
    } else {
        std::cout << "No element not less than 25" << std::endl;
    }

    // Use upperBound to find the first element greater than a given value
    auto upper = set.upperBound(25);
    if (upper != set.end()) {
        std::cout << "Upper bound of 25: " << *upper << std::endl;
    } else {
        std::cout << "No element greater than 25" << std::endl;
    }

    // Use equalRange to find the range of elements equal to a given value
    auto [equalFirst, equalLast] = set.equalRange(30);
    std::cout << "Equal range of 30: ";
    for (auto it = equalFirst; it != equalLast; ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    // Swap two FlatSets
    FlatSet<int> otherSet = {70, 80, 90};
    set.swap(otherSet);
    std::cout << "Swapped sets" << std::endl;

    // Iterate over the swapped set
    std::cout << "Set contents after swap:" << std::endl;
    for (const auto& value : set) {
        std::cout << value << " ";
    }
    std::cout << std::endl;

    // Use emplace to construct and insert an element
    set.emplace(100);
    std::cout << "Emplaced 100 into the set" << std::endl;

    // Use emplace_hint to construct and insert an element with a hint
    set.emplace_hint(set.begin(), 110);
    std::cout << "Emplaced 110 into the set with a hint" << std::endl;

    // Iterate over the set after emplace
    std::cout << "Set contents after emplace:" << std::endl;
    for (const auto& value : set) {
        std::cout << value << " ";
    }
    std::cout << std::endl;

    return 0;
}