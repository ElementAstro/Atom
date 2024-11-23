#include "atom/type/static_vector.hpp"

#include <iostream>

int main() {
    // Create an empty StaticVector
    StaticVector<int, 5> vec1;
    std::cout << "vec1 size: " << vec1.size() << std::endl;

    // Create a StaticVector with an initializer list
    StaticVector<int, 5> vec2 = {1, 2, 3, 4, 5};
    std::cout << "vec2 size: " << vec2.size() << ", values: ";
    for (const auto& val : vec2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Copy constructor
    StaticVector<int, 5> vec3 = vec2;
    std::cout << "vec3 size: " << vec3.size() << ", values: ";
    for (const auto& val : vec3) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Move constructor
    StaticVector<int, 5> vec4 = std::move(vec3);
    std::cout << "vec4 size: " << vec4.size() << ", values: ";
    for (const auto& val : vec4) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Copy assignment operator
    vec1 = vec2;
    std::cout << "vec1 size after copy assignment: " << vec1.size()
              << ", values: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Move assignment operator
    vec1 = std::move(vec4);
    std::cout << "vec1 size after move assignment: " << vec1.size()
              << ", values: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Push back elements
    vec1.pushBack(6);
    vec1.pushBack(7);
    std::cout << "vec1 after pushBack: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Emplace back elements
    vec1.emplaceBack(8);
    vec1.emplaceBack(9);
    std::cout << "vec1 after emplaceBack: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Pop back element
    vec1.popBack();
    std::cout << "vec1 after popBack: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Access elements with at()
    std::cout << "vec1 at(2): " << vec1.at(2) << std::endl;

    // Access elements with operator[]
    std::cout << "vec1[2]: " << vec1[2] << std::endl;

    // Access front and back elements
    std::cout << "vec1 front: " << vec1.front() << std::endl;
    std::cout << "vec1 back: " << vec1.back() << std::endl;

    // Check if vector is empty
    std::cout << "vec1 is empty: " << std::boolalpha << vec1.empty()
              << std::endl;

    // Clear the vector
    vec1.clear();
    std::cout << "vec1 size after clear: " << vec1.size() << std::endl;

    // Swap vectors
    vec1.swap(vec2);
    std::cout << "vec1 after swap: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    std::cout << "vec2 after swap: ";
    for (const auto& val : vec2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Check equality
    bool isEqual = (vec1 == vec2);
    std::cout << "vec1 and vec2 are equal: " << std::boolalpha << isEqual
              << std::endl;

    // Three-way comparison
    auto cmp = vec1 <=> vec2;
    if (cmp < 0) {
        std::cout << "vec1 is less than vec2" << std::endl;
    } else if (cmp > 0) {
        std::cout << "vec1 is greater than vec2" << std::endl;
    } else {
        std::cout << "vec1 is equal to vec2" << std::endl;
    }

    return 0;
}