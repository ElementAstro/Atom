#include "atom/type/indestructible.hpp"

#include <iostream>
#include <string>

struct MyClass {
    MyClass(int x, std::string y) : x(x), y(std::move(y)) {
        std::cout << "MyClass constructed with x: " << x << ", y: " << this->y
                  << std::endl;
    }
    ~MyClass() { std::cout << "MyClass destructed" << std::endl; }
    MyClass(const MyClass& other) : x(other.x), y(other.y) {
        std::cout << "MyClass copy constructed" << std::endl;
    }
    MyClass(MyClass&& other) noexcept : x(other.x), y(std::move(other.y)) {
        std::cout << "MyClass move constructed" << std::endl;
    }
    MyClass& operator=(const MyClass& other) {
        if (this != &other) {
            x = other.x;
            y = other.y;
            std::cout << "MyClass copy assigned" << std::endl;
        }
        return *this;
    }
    MyClass& operator=(MyClass&& other) noexcept {
        if (this != &other) {
            x = other.x;
            y = std::move(other.y);
            std::cout << "MyClass move assigned" << std::endl;
        }
        return *this;
    }
    int x;
    std::string y;
};

int main() {
    // Create an Indestructible object
    Indestructible<MyClass> indestructible(std::in_place, 42, "Hello");

    // Access the stored object
    std::cout << "Accessing stored object: x = " << indestructible->x
              << ", y = " << indestructible->y << std::endl;

    // Copy construct an Indestructible object
    Indestructible<MyClass> copyConstructed(indestructible);
    std::cout << "Copy constructed object: x = " << copyConstructed->x
              << ", y = " << copyConstructed->y << std::endl;

    // Move construct an Indestructible object
    Indestructible<MyClass> moveConstructed(std::move(indestructible));
    std::cout << "Move constructed object: x = " << moveConstructed->x
              << ", y = " << moveConstructed->y << std::endl;

    // Copy assign an Indestructible object
    Indestructible<MyClass> copyAssigned(std::in_place, 0, "");
    copyAssigned = copyConstructed;
    std::cout << "Copy assigned object: x = " << copyAssigned->x
              << ", y = " << copyAssigned->y << std::endl;

    // Move assign an Indestructible object
    Indestructible<MyClass> moveAssigned(std::in_place, 0, "");
    moveAssigned = std::move(copyConstructed);
    std::cout << "Move assigned object: x = " << moveAssigned->x
              << ", y = " << moveAssigned->y << std::endl;

    // Reset the stored object with new arguments
    moveAssigned.reset(100, "World");
    std::cout << "Reset object: x = " << moveAssigned->x
              << ", y = " << moveAssigned->y << std::endl;

    // Emplace a new object in place with new arguments
    moveAssigned.emplace(200, "New");
    std::cout << "Emplaced object: x = " << moveAssigned->x
              << ", y = " << moveAssigned->y << std::endl;

    // Use destruction_guard to ensure destruction of an object
    {
        MyClass myObject(300, "Guarded");
        destruction_guard<MyClass> guard(&myObject);
    }  // myObject will be destroyed here

    return 0;
}