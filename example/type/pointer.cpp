#include "atom/type/pointer.hpp"

#include <iostream>
#include <memory>

struct MyClass {
    void sayHello() const { std::cout << "Hello from MyClass!" << std::endl; }
    void setValue(int val) { value = val; }
    int getValue() const { return value; }

private:
    int value = 0;
};

int main() {
    // Create a PointerSentinel with a raw pointer
    MyClass rawObj;
    PointerSentinel<MyClass> ptr1(&rawObj);
    ptr1.apply(&MyClass::sayHello);

    // Create a PointerSentinel with a shared pointer
    auto sharedPtr = std::make_shared<MyClass>();
    PointerSentinel<MyClass> ptr2(sharedPtr);
    ptr2.apply(&MyClass::sayHello);

    // Create a PointerSentinel with a unique pointer
    auto uniquePtr = std::make_unique<MyClass>();
    PointerSentinel<MyClass> ptr3(std::move(uniquePtr));
    ptr3.apply(&MyClass::sayHello);

    // Create a PointerSentinel with a weak pointer
    std::weak_ptr<MyClass> weakPtr(sharedPtr);
    PointerSentinel<MyClass> ptr4(weakPtr);
    ptr4.apply(&MyClass::sayHello);

    // Use invoke to call a member function
    ptr2.invoke(&MyClass::setValue, 42);
    int value = ptr2.invoke(&MyClass::getValue);
    std::cout << "Value: " << value << std::endl;

    // Use applyVoid to call a member function that returns void
    ptr3.applyVoid(&MyClass::setValue, 100);
    int newValue = ptr3.invoke(&MyClass::getValue);
    std::cout << "New Value: " << newValue << std::endl;

    // Get the raw pointer from the PointerSentinel
    MyClass* rawPtr = ptr1.get();
    rawPtr->sayHello();

    return 0;
}