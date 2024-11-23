#include "atom/type/pod_vector.hpp"

#include <iostream>

using namespace atom::type;

int main() {
    // Create a PodVector with default constructor
    PodVector<int> vec1;
    std::cout << "vec1 size: " << vec1.size()
              << ", capacity: " << vec1.capacity() << std::endl;

    // Create a PodVector with initializer list
    PodVector<int> vec2 = {1, 2, 3, 4, 5};
    std::cout << "vec2 size: " << vec2.size()
              << ", capacity: " << vec2.capacity() << std::endl;

    // Create a PodVector with a specific size
    PodVector<int> vec3(10);
    std::cout << "vec3 size: " << vec3.size()
              << ", capacity: " << vec3.capacity() << std::endl;

    // Push back elements
    vec1.pushBack(10);
    vec1.pushBack(20);
    std::cout << "vec1 after pushBack: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Emplace back elements
    vec1.emplaceBack(30);
    vec1.emplaceBack(40);
    std::cout << "vec1 after emplaceBack: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Reserve capacity
    vec1.reserve(20);
    std::cout << "vec1 size: " << vec1.size()
              << ", capacity: " << vec1.capacity() << std::endl;

    // Pop back elements
    vec1.popBack();
    std::cout << "vec1 after popBack: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Pop back and return element
    int poppedValue = vec1.popxBack();
    std::cout << "Popped value: " << poppedValue << std::endl;
    std::cout << "vec1 after popxBack: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Extend with another PodVector
    vec1.extend(vec2);
    std::cout << "vec1 after extend with vec2: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Extend with a range
    int arr[] = {50, 60, 70};
    vec1.extend(arr, arr + 3);
    std::cout << "vec1 after extend with range: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Access elements with operator[]
    std::cout << "vec1[0]: " << vec1[0] << std::endl;

    // Insert element at a specific position
    vec1.insert(2, 25);
    std::cout << "vec1 after insert at position 2: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Erase element at a specific position
    vec1.erase(2);
    std::cout << "vec1 after erase at position 2: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Reverse the PodVector
    vec1.reverse();
    std::cout << "vec1 after reverse: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Resize the PodVector
    vec1.resize(5);
    std::cout << "vec1 after resize to 5: ";
    for (const auto& val : vec1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Detach the PodVector
    auto [data, size] = vec1.detach();
    std::cout << "Detached data size: " << size << std::endl;
    std::cout << "Detached data: ";
    for (int i = 0; i < size; ++i) {
        std::cout << data[i] << " ";
    }
    std::cout << std::endl;

    // Clear the PodVector
    vec1.clear();
    std::cout << "vec1 after clear, size: " << vec1.size() << std::endl;

    return 0;
}