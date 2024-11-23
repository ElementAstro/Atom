#include "atom/async/safetype.hpp"

#include <iostream>
#include <thread>
#include <vector>

using namespace atom::async;

void testLockFreeStack() {
    LockFreeStack<int> stack;

    // Push elements onto the stack
    stack.push(1);
    stack.push(2);
    stack.push(3);

    // Pop elements from the stack
    auto element = stack.pop();
    if (element) {
        std::cout << "Popped element: " << *element << std::endl;
    }

    // Get the top element without removing it
    auto topElement = stack.top();
    if (topElement) {
        std::cout << "Top element: " << *topElement << std::endl;
    }

    // Check if the stack is empty
    std::cout << "Is stack empty? " << (stack.empty() ? "Yes" : "No")
              << std::endl;

    // Get the approximate size of the stack
    std::cout << "Stack size: " << stack.size() << std::endl;
}

void testLockFreeHashTable() {
    LockFreeHashTable<int, std::string> hashTable;

    // Insert elements into the hash table
    hashTable.insert(1, "One");
    hashTable.insert(2, "Two");
    hashTable.insert(3, "Three");

    // Find elements in the hash table
    auto value = hashTable.find(2);
    if (value) {
        std::cout << "Found value: " << *value << std::endl;
    }

    // Erase an element from the hash table
    hashTable.erase(2);

    // Check if the hash table is empty
    std::cout << "Is hash table empty? " << (hashTable.empty() ? "Yes" : "No")
              << std::endl;

    // Get the size of the hash table
    std::cout << "Hash table size: " << hashTable.size() << std::endl;

    // Iterate over the hash table
    for (auto it = hashTable.begin(); it != hashTable.end(); ++it) {
        std::cout << "Key: " << it->first << ", Value: " << it->second
                  << std::endl;
    }
}

void testThreadSafeVector() {
    ThreadSafeVector<int> vector;

    // Push elements into the vector
    vector.pushBack(1);
    vector.pushBack(2);
    vector.pushBack(3);

    // Pop an element from the vector
    auto element = vector.popBack();
    if (element) {
        std::cout << "Popped element: " << *element << std::endl;
    }

    // Access elements by index
    auto value = vector.at(1);
    if (value) {
        std::cout << "Element at index 1: " << *value << std::endl;
    }

    // Check if the vector is empty
    std::cout << "Is vector empty? " << (vector.empty() ? "Yes" : "No")
              << std::endl;

    // Get the size and capacity of the vector
    std::cout << "Vector size: " << vector.getSize() << std::endl;
    std::cout << "Vector capacity: " << vector.getCapacity() << std::endl;

    // Clear the vector
    vector.clear();
    std::cout << "Vector cleared. Is vector empty? "
              << (vector.empty() ? "Yes" : "No") << std::endl;
}

void testLockFreeList() {
    LockFreeList<int> list;

    // Push elements to the front of the list
    list.pushFront(1);
    list.pushFront(2);
    list.pushFront(3);

    // Pop elements from the front of the list
    auto element = list.popFront();
    if (element) {
        std::cout << "Popped element: " << *element << std::endl;
    }

    // Check if the list is empty
    std::cout << "Is list empty? " << (list.empty() ? "Yes" : "No")
              << std::endl;

    // Iterate over the list
    for (auto it = list.begin(); it != list.end(); ++it) {
        std::cout << "List element: " << *it << std::endl;
    }
}

int main() {
    std::cout << "Testing LockFreeStack:" << std::endl;
    testLockFreeStack();

    std::cout << "\nTesting LockFreeHashTable:" << std::endl;
    testLockFreeHashTable();

    std::cout << "\nTesting ThreadSafeVector:" << std::endl;
    testThreadSafeVector();

    std::cout << "\nTesting LockFreeList:" << std::endl;
    testLockFreeList();

    return 0;
}