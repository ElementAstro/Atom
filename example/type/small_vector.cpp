#include "atom/type/small_vector.hpp"

#include <iostream>
#include <string>

int main() {
    // Create an empty SmallVector
    SmallVector<int, 5> vec1;
    std::cout << "vec1 size: " << vec1.size() << std::endl;

    // Create a SmallVector with a specific size and value
    SmallVector<int, 5> vec2(3, 42);
    std::cout << "vec2 size: " << vec2.size() << ", values: ";
    for (const auto& val : vec2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Create a SmallVector with an initializer list
    SmallVector<int, 5> vec3 = {1, 2, 3, 4, 5};
    std::cout << "vec3 size: " << vec3.size() << ", values: ";
    for (const auto& val : vec3) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Copy constructor
    SmallVector<int, 5> vec4 = vec3;
    std::cout << "vec4 size: " << vec4.size() << ", values: ";
    for (const auto& val : vec4) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Move constructor
    SmallVector<int, 5> vec5 = std::move(vec4);
    std::cout << "vec5 size: " << vec5.size() << ", values: ";
    for (const auto& val : vec5) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Copy assignment operator
    vec1 = vec3;
    std::cout << "vec1 size after copy assignment: " << vec1.size()
              << ", values: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Move assignment operator
    vec1 = std::move(vec5);
    std::cout << "vec1 size after move assignment: " << vec1.size()
              << ", values: ";
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

    // Insert elements
    vec2.insert(vec2.begin(), 0);
    vec2.insert(vec2.end(), 6);
    std::cout << "vec2 after insert: ";
    for (const auto& val : vec2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    /*
    TODO: Implement these methods
    // Erase elements
    vec2.erase(vec2.begin());
    vec2.erase(--vec2.end());
    std::cout << "vec2 after erase: ";
    for (const auto& val : vec2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    */

    // Push back elements
    vec2.pushBack(7);
    vec2.pushBack(8);
    std::cout << "vec2 after pushBack: ";
    for (const auto& val : vec2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Emplace back elements
    vec2.emplaceBack(9);
    vec2.emplaceBack(10);
    std::cout << "vec2 after emplaceBack: ";
    for (const auto& val : vec2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Pop back element
    vec2.popBack();
    std::cout << "vec2 after popBack: ";
    for (const auto& val : vec2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Resize the vector
    vec2.resize(5, 100);
    std::cout << "vec2 after resize: ";
    for (const auto& val : vec2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

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

    return 0;
}