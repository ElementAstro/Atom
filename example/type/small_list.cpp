#include "atom/type/small_list.hpp"

#include <iostream>
#include <string>

using namespace atom::type;

int main() {
    // Create an empty SmallList
    SmallList<int> list1;
    std::cout << "list1 size: " << list1.size() << std::endl;

    // Create a SmallList with initializer list
    SmallList<int> list2 = {1, 2, 3, 4, 5};
    std::cout << "list2 size: " << list2.size() << std::endl;

    // Copy constructor
    SmallList<int> list3 = list2;
    std::cout << "list3 size: " << list3.size() << std::endl;

    // Move constructor
    SmallList<int> list4 = std::move(list3);
    std::cout << "list4 size: " << list4.size() << std::endl;

    // Copy assignment operator
    list1 = list2;
    std::cout << "list1 size after copy assignment: " << list1.size()
              << std::endl;

    // Move assignment operator
    list1 = std::move(list4);
    std::cout << "list1 size after move assignment: " << list1.size()
              << std::endl;

    // Push back elements
    list1.pushBack(6);
    list1.pushBack(7);
    std::cout << "list1 after pushBack: ";
    for (const auto& val : list1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Push front elements
    list1.pushFront(0);
    list1.pushFront(-1);
    std::cout << "list1 after pushFront: ";
    for (const auto& val : list1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Pop back element
    list1.popBack();
    std::cout << "list1 after popBack: ";
    for (const auto& val : list1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Pop front element
    list1.popFront();
    std::cout << "list1 after popFront: ";
    for (const auto& val : list1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Access front and back elements
    std::cout << "list1 front: " << list1.front() << std::endl;
    std::cout << "list1 back: " << list1.back() << std::endl;

    // Check if list is empty
    std::cout << "list1 is empty: " << std::boolalpha << list1.empty()
              << std::endl;

    // Clear the list
    list1.clear();
    std::cout << "list1 size after clear: " << list1.size() << std::endl;

    // Insert elements
    list2.insert(list2.begin(), 0);
    list2.insert(list2.end(), 6);
    std::cout << "list2 after insert: ";
    for (const auto& val : list2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Erase elements
    list2.erase(list2.begin());
    list2.erase(--list2.end());
    std::cout << "list2 after erase: ";
    for (const auto& val : list2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Remove elements with specific value
    list2.remove(3);
    std::cout << "list2 after remove(3): ";
    for (const auto& val : list2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Remove consecutive duplicate elements
    list2.pushBack(4);
    list2.pushBack(4);
    list2.unique();
    std::cout << "list2 after unique: ";
    for (const auto& val : list2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Sort the list
    list2.pushBack(2);
    list2.pushBack(1);
    list2.sort();
    std::cout << "list2 after sort: ";
    for (const auto& val : list2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Swap lists
    list1.swap(list2);
    std::cout << "list1 after swap: ";
    for (const auto& val : list1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    std::cout << "list2 after swap: ";
    for (const auto& val : list2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Emplace back elements
    list1.emplaceBack(8);
    list1.emplaceBack(9);
    std::cout << "list1 after emplaceBack: ";
    for (const auto& val : list1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Emplace front elements
    list1.emplaceFront(-2);
    list1.emplaceFront(-3);
    std::cout << "list1 after emplaceFront: ";
    for (const auto& val : list1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Emplace elements at specific position
    list1.emplace(++list1.begin(), 0);
    std::cout << "list1 after emplace: ";
    for (const auto& val : list1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    return 0;
}