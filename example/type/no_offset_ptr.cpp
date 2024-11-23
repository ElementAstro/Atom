#include "atom/type/no_offset_ptr.hpp"

#include <iostream>
#include <string>

struct MyClass {
    MyClass() : x(0), y("") {
        std::cout << "MyClass default constructed" << std::endl;
    }
    MyClass(int x, std::string y) : x(x), y(std::move(y)) {
        std::cout << "MyClass constructed with x: " << x << ", y: " << this->y
                  << std::endl;
    }
    ~MyClass() { std::cout << "MyClass destructed" << std::endl; }
    int x;
    std::string y;
};

int main() {
    // Create an UnshiftedPtr object with default constructor
    UnshiftedPtr<MyClass> ptr1;
    std::cout << "Accessing ptr1: x = " << ptr1->x << ", y = " << ptr1->y
              << std::endl;

    // Create an UnshiftedPtr object with custom constructor arguments
    UnshiftedPtr<MyClass> ptr2(42, "Hello");
    std::cout << "Accessing ptr2: x = " << ptr2->x << ", y = " << ptr2->y
              << std::endl;

    // Reset the managed object with new arguments
    ptr2.reset(100, "World");
    std::cout << "After reset, accessing ptr2: x = " << ptr2->x
              << ", y = " << ptr2->y << std::endl;

    // Emplace a new object in place with new arguments
    ptr2.emplace(200, "New");
    std::cout << "After emplace, accessing ptr2: x = " << ptr2->x
              << ", y = " << ptr2->y << std::endl;

    // Release ownership of the managed object
    MyClass* rawPtr = ptr2.release();
    std::cout << "After release, rawPtr: x = " << rawPtr->x
              << ", y = " << rawPtr->y << std::endl;

    // Check if the managed object has a value
    bool hasValue = ptr2.hasValue();
    std::cout << "ptr2 has value: " << std::boolalpha << hasValue << std::endl;

    // Manually destroy the released object
    rawPtr->~MyClass();

    return 0;
}