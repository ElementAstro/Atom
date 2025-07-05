/**
 * @file vany_example.cpp
 * @brief Comprehensive examples of using the Any class
 * @author Example Author
 * @date 2025-03-23
 */

#include <algorithm>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/meta/vany.hpp"

// Class that will be used to demonstrate custom class behavior with Any
class Person {
private:
    std::string name_;
    int age_;

public:
    Person(std::string name, int age) : name_(std::move(name)), age_(age) {}

    std::string getName() const { return name_; }
    int getAge() const { return age_; }

    // Provide custom string representation
    friend std::ostream& operator<<(std::ostream& os, const Person& person) {
        os << "Person{name='" << person.name_ << "', age=" << person.age_
           << "}";
        return os;
    }

    // Provide custom equality operator
    bool operator==(const Person& other) const {
        return name_ == other.name_ && age_ == other.age_;
    }
};

// Large class that won't fit in the small object optimization buffer
class LargeClass {
private:
    std::array<double, 100> data_;
    std::string name_;

public:
    LargeClass(std::string name) : name_(std::move(name)) {
        // Initialize with some values
        for (size_t i = 0; i < data_.size(); ++i) {
            data_[i] = static_cast<double>(i);
        }
    }

    std::string getName() const { return name_; }

    double sum() const {
        return std::accumulate(data_.begin(), data_.end(), 0.0);
    }

    friend std::ostream& operator<<(std::ostream& os, const LargeClass& obj) {
        os << "LargeClass{name='" << obj.name_
           << "', data size=" << obj.data_.size() << "}";
        return os;
    }

    bool operator==(const LargeClass& other) const {
        return name_ == other.name_ && data_ == other.data_;
    }
};

// Non-copyable class to test move semantics
class NonCopyable {
private:
    std::unique_ptr<int> value_;

public:
    explicit NonCopyable(int val) : value_(std::make_unique<int>(val)) {}
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) noexcept = default;
    NonCopyable& operator=(NonCopyable&&) noexcept = default;

    int getValue() const { return *value_; }

    friend std::ostream& operator<<(std::ostream& os, const NonCopyable& obj) {
        os << "NonCopyable{value=" << *obj.value_ << "}";
        return os;
    }
};

// Custom container class to test iteration
template <typename T>
class CustomContainer {
private:
    std::vector<T> data_;

public:
    CustomContainer() = default;

    void add(T value) { data_.push_back(std::move(value)); }

    auto begin() const { return data_.begin(); }
    auto end() const { return data_.end(); }

    friend std::ostream& operator<<(std::ostream& os,
                                    const CustomContainer& container) {
        os << "CustomContainer{";
        for (size_t i = 0; i < container.data_.size(); ++i) {
            if (i > 0)
                os << ", ";
            os << container.data_[i];
        }
        os << "}";
        return os;
    }
};

// Utility function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

int main() {
    std::cout << "ANY CLASS COMPREHENSIVE EXAMPLES\n";
    std::cout << "================================\n";

    //--------------------------------------------------------------------------
    // 1. Basic Usage with Primitive Types
    //--------------------------------------------------------------------------
    printSection("Basic Usage with Primitive Types");

    // Create Any objects with different types
    atom::meta::Any intValue(42);
    atom::meta::Any floatValue(3.14f);
    atom::meta::Any doubleValue(2.71828);
    atom::meta::Any boolValue(true);
    atom::meta::Any charValue('A');

    // Convert to string and display
    std::cout << "int value: " << intValue.toString() << "\n";
    std::cout << "float value: " << floatValue.toString() << "\n";
    std::cout << "double value: " << doubleValue.toString() << "\n";
    std::cout << "bool value: " << boolValue.toString() << "\n";
    std::cout << "char value: " << charValue.toString() << "\n";

    // Check types
    std::cout << "\nType information:\n";
    std::cout << "intValue type: " << intValue.type().name() << "\n";
    std::cout << "floatValue type: " << floatValue.type().name() << "\n";
    std::cout << "doubleValue type: " << doubleValue.type().name() << "\n";
    std::cout << "boolValue type: " << boolValue.type().name() << "\n";
    std::cout << "charValue type: " << charValue.type().name() << "\n";

    //--------------------------------------------------------------------------
    // 2. Working with Strings
    //--------------------------------------------------------------------------
    printSection("Working with Strings");

    // Create Any objects with string types
    atom::meta::Any stringValue("Hello, world!");
    atom::meta::Any stdStringValue(std::string("C++ standard string"));

    // Display string values
    std::cout << "C-string value: " << stringValue.toString() << "\n";
    std::cout << "std::string value: " << stdStringValue.toString() << "\n";

    // Check types
    std::cout << "\nType information:\n";
    std::cout << "stringValue type: " << stringValue.type().name() << "\n";
    std::cout << "stdStringValue type: " << stdStringValue.type().name()
              << "\n";

    //--------------------------------------------------------------------------
    // 3. Custom Classes
    //--------------------------------------------------------------------------
    printSection("Custom Classes");

    // Create Any objects with custom class
    Person alice("Alice", 30);
    atom::meta::Any personValue(alice);

    // Display custom class value
    std::cout << "Person value: " << personValue.toString() << "\n";

    // Check type
    std::cout << "\nType information:\n";
    std::cout << "personValue type: " << personValue.type().name() << "\n";

    //--------------------------------------------------------------------------
    // 4. Small vs Large Objects (Small Buffer Optimization)
    //--------------------------------------------------------------------------
    printSection("Small vs Large Objects (Small Buffer Optimization)");

    // Create small and large objects
    atom::meta::Any smallIntValue(123);
    atom::meta::Any smallStringValue(std::string("small"));
    LargeClass largeObj("large object");
    atom::meta::Any largeValue(largeObj);

    // Access and display object properties
    std::cout << "Small int value: " << smallIntValue.toString() << "\n";
    std::cout << "Small string value: " << smallStringValue.toString() << "\n";
    std::cout << "Large object value: " << largeValue.toString() << "\n";

    // Check if objects are stored inline (internal implementation detail)
    std::cout << "\nObject storage information (implementation detail):\n";
    std::cout << "Small int is_small_: "
              << (smallIntValue.isSmallObject() ? "Yes" : "No") << "\n";
    std::cout << "Small string is_small_: "
              << (smallStringValue.isSmallObject() ? "Yes" : "No") << "\n";
    std::cout << "Large object is_small_: "
              << (largeValue.isSmallObject() ? "Yes" : "No") << "\n";

    //--------------------------------------------------------------------------
    // 5. Copy and Move Semantics
    //--------------------------------------------------------------------------
    printSection("Copy and Move Semantics");

    // Copy constructor
    atom::meta::Any originalValue(42);
    atom::meta::Any copiedValue(originalValue);

    std::cout << "Original value: " << originalValue.toString() << "\n";
    std::cout << "Copied value: " << copiedValue.toString() << "\n";

    // Verify they're independent
    originalValue = 100;
    std::cout << "After modifying original:\n";
    std::cout << "  Original value: " << originalValue.toString() << "\n";
    std::cout << "  Copied value: " << copiedValue.toString() << "\n";

    // Move constructor
    atom::meta::Any sourceValue(std::string("Move me"));
    atom::meta::Any movedValue(std::move(sourceValue));

    std::cout << "\nMoved value: " << movedValue.toString() << "\n";
    std::cout << "Source value after move: "
              << (sourceValue.empty() ? "empty" : sourceValue.toString())
              << "\n";

    // Copy assignment
    atom::meta::Any target1;
    target1 = copiedValue;
    std::cout << "\nTarget1 after copy assignment: " << target1.toString()
              << "\n";

    // Move assignment
    atom::meta::Any target2;
    target2 = std::move(movedValue);
    std::cout << "Target2 after move assignment: " << target2.toString()
              << "\n";
    std::cout << "Source after move assignment: "
              << (movedValue.empty() ? "empty" : movedValue.toString()) << "\n";

    //--------------------------------------------------------------------------
    // 6. Non-Copyable Types
    //--------------------------------------------------------------------------
    printSection("Non-Copyable Types");

    // Create a non-copyable object
    NonCopyable nonCopyableObj(42);

    // Store it in Any (must be moved)
    atom::meta::Any nonCopyableValue(std::move(nonCopyableObj));
    std::cout << "Non-copyable value: " << nonCopyableValue.toString() << "\n";

    // Move it to another Any
    atom::meta::Any anotherNonCopyableValue(std::move(nonCopyableValue));
    std::cout << "Moved non-copyable value: "
              << anotherNonCopyableValue.toString() << "\n";
    std::cout << "Original after move: "
              << (nonCopyableValue.empty() ? "empty"
                                           : nonCopyableValue.toString())
              << "\n";

    //--------------------------------------------------------------------------
    // 7. Empty Any and Reset
    //--------------------------------------------------------------------------
    printSection("Empty Any and Reset");

    // Default constructor creates empty Any
    atom::meta::Any emptyValue;
    std::cout << "Is empty value empty? " << (emptyValue.empty() ? "Yes" : "No")
              << "\n";

    // Fill it and check again
    emptyValue = 42;
    std::cout << "After assignment, is it empty? "
              << (emptyValue.empty() ? "Yes" : "No") << "\n";
    std::cout << "Value: " << emptyValue.toString() << "\n";

    // Reset and check
    emptyValue.reset();
    std::cout << "After reset, is it empty? "
              << (emptyValue.empty() ? "Yes" : "No") << "\n";

    //--------------------------------------------------------------------------
    // 8. Type Checking and Casting
    //--------------------------------------------------------------------------
    printSection("Type Checking and Casting");

    atom::meta::Any value(42);

    // Check if the Any contains a specific type
    bool isInt = value.is<int>();
    bool isString = value.is<std::string>();

    std::cout << "Is value an int? " << (isInt ? "Yes" : "No") << "\n";
    std::cout << "Is value a string? " << (isString ? "Yes" : "No") << "\n";

    // Safe casting
    try {
        int intVal = value.cast<int>();
        std::cout << "Successfully cast to int: " << intVal << "\n";

        // This will throw an exception
        std::string strVal = value.cast<std::string>();
        std::cout << "Successfully cast to string: " << strVal << "\n";
    } catch (const std::exception& e) {
        std::cout << "Exception during cast: " << e.what() << "\n";
    }

    // Unchecked cast (be careful!)
    int unsafeInt = value.unsafeCast<int>();
    std::cout << "Unchecked cast to int: " << unsafeInt << "\n";

    //--------------------------------------------------------------------------
    // 9. Containers in Any
    //--------------------------------------------------------------------------
    printSection("Containers in Any");

    // Create containers
    std::vector<int> intVector = {1, 2, 3, 4, 5};
    std::list<std::string> stringList = {"one", "two", "three"};
    std::map<std::string, int> stringIntMap = {
        {"one", 1}, {"two", 2}, {"three", 3}};

    // Store containers in Any
    atom::meta::Any vectorValue(intVector);
    atom::meta::Any listValue(stringList);
    atom::meta::Any mapValue(stringIntMap);

    // Display container values
    std::cout << "Vector: " << vectorValue.toString() << "\n";
    std::cout << "List: " << listValue.toString() << "\n";
    std::cout << "Map: " << mapValue.toString() << "\n";

    // Iterate through containers using foreach
    std::cout << "\nIterating through vector:\n";
    vectorValue.foreach ([](const atom::meta::Any& item) {
        std::cout << "  Item: " << item.toString() << "\n";
    });

    std::cout << "\nIterating through list:\n";
    listValue.foreach ([](const atom::meta::Any& item) {
        std::cout << "  Item: " << item.toString() << "\n";
    });

    std::cout << "\nIterating through map:\n";
    mapValue.foreach ([](const atom::meta::Any& item) {
        std::cout << "  Item: " << item.toString() << "\n";
    });

    // Custom container
    CustomContainer<int> customContainer;
    customContainer.add(10);
    customContainer.add(20);
    customContainer.add(30);

    atom::meta::Any customContainerValue(customContainer);
    std::cout << "\nCustom container: " << customContainerValue.toString()
              << "\n";

    std::cout << "Iterating through custom container:\n";
    customContainerValue.foreach ([](const atom::meta::Any& item) {
        std::cout << "  Item: " << item.toString() << "\n";
    });

    //--------------------------------------------------------------------------
    // 10. Compare and Equality
    //--------------------------------------------------------------------------
    printSection("Compare and Equality");

    atom::meta::Any a(42);
    atom::meta::Any b(42);
    atom::meta::Any c(43);
    atom::meta::Any d("42");

    bool a_equals_b = (a == b);
    bool a_equals_c = (a == c);
    bool a_equals_d = (a == d);

    std::cout << "a(42) == b(42): " << (a_equals_b ? "equal" : "not equal")
              << "\n";
    std::cout << "a(42) == c(43): " << (a_equals_c ? "equal" : "not equal")
              << "\n";
    std::cout << "a(42) == d(\"42\"): " << (a_equals_d ? "equal" : "not equal")
              << "\n";

    // Compare custom objects
    Person person1("John", 30);
    Person person2("John", 30);
    Person person3("Jane", 25);

    atom::meta::Any personA(person1);
    atom::meta::Any personB(person2);
    atom::meta::Any personC(person3);

    bool personA_equals_personB = (personA == personB);
    bool personA_equals_personC = (personA == personC);

    std::cout << "\nPerson comparison:\n";
    std::cout << "personA == personB (same data): "
              << (personA_equals_personB ? "equal" : "not equal") << "\n";
    std::cout << "personA == personC (different data): "
              << (personA_equals_personC ? "equal" : "not equal") << "\n";

    //--------------------------------------------------------------------------
    // 11. Hashing Support
    //--------------------------------------------------------------------------
    printSection("Hashing Support");

    atom::meta::Any hashInt1(42);
    atom::meta::Any hashInt2(42);
    atom::meta::Any hashInt3(43);
    atom::meta::Any hashString("hash me");

    size_t hashValue1 = hashInt1.hash();
    size_t hashValue2 = hashInt2.hash();
    size_t hashValue3 = hashInt3.hash();
    size_t hashValueStr = hashString.hash();

    std::cout << "Hash of 42 (first): " << hashValue1 << "\n";
    std::cout << "Hash of 42 (second): " << hashValue2 << "\n";
    std::cout << "Hash of 43: " << hashValue3 << "\n";
    std::cout << "Hash of \"hash me\": " << hashValueStr << "\n";

    // Using Any in unordered_map
    std::unordered_map<size_t, std::string> hashMap;
    hashMap[hashValue1] = "First 42";
    hashMap[hashValue3] = "The value 43";
    hashMap[hashValueStr] = "String value";

    std::cout << "\nLooking up values in hash map:\n";
    std::cout << "Value for hash of first 42: " << hashMap[hashValue1] << "\n";
    std::cout << "Value for hash of second 42: " << hashMap[hashValue2] << "\n";
    std::cout << "Value for hash of 43: " << hashMap[hashValue3] << "\n";
    std::cout << "Value for hash of \"hash me\": " << hashMap[hashValueStr]
              << "\n";

    //--------------------------------------------------------------------------
    // 12. Invoke Method
    //--------------------------------------------------------------------------
    printSection("Invoke Method");

    atom::meta::Any invokeInt(42);
    atom::meta::Any invokeString(std::string("call me"));
    atom::meta::Any invokePerson(Person("Bob", 25));

    // Use invoke to access the contained value in a type-safe way
    invokeInt.invoke([](const void* ptr) {
        const int* intPtr = static_cast<const int*>(ptr);
        std::cout << "Invoked with int: " << *intPtr << "\n";
    });

    invokeString.invoke([](const void* ptr) {
        const std::string* strPtr = static_cast<const std::string*>(ptr);
        std::cout << "Invoked with string: " << *strPtr << "\n";
    });

    invokePerson.invoke([](const void* ptr) {
        const Person* personPtr = static_cast<const Person*>(ptr);
        std::cout << "Invoked with Person: " << personPtr->getName() << ", age "
                  << personPtr->getAge() << "\n";
    });

    //--------------------------------------------------------------------------
    // 13. Swap Method
    //--------------------------------------------------------------------------
    printSection("Swap Method");

    atom::meta::Any swap1(100);
    atom::meta::Any swap2(std::string("swap me"));

    std::cout << "Before swap:\n";
    std::cout << "  swap1: " << swap1.toString()
              << " (type: " << swap1.type().name() << ")\n";
    std::cout << "  swap2: " << swap2.toString()
              << " (type: " << swap2.type().name() << ")\n";

    swap1.swap(swap2);

    std::cout << "\nAfter swap:\n";
    std::cout << "  swap1: " << swap1.toString()
              << " (type: " << swap1.type().name() << ")\n";
    std::cout << "  swap2: " << swap2.toString()
              << " (type: " << swap2.type().name() << ")\n";

    // Swap with empty Any
    atom::meta::Any empty;
    atom::meta::Any nonEmpty(42);

    std::cout << "\nBefore swap with empty:\n";
    std::cout << "  empty: " << (empty.empty() ? "empty" : empty.toString())
              << "\n";
    std::cout << "  nonEmpty: "
              << (nonEmpty.empty() ? "empty" : nonEmpty.toString()) << "\n";

    empty.swap(nonEmpty);

    std::cout << "\nAfter swap with empty:\n";
    std::cout << "  empty: " << (empty.empty() ? "empty" : empty.toString())
              << "\n";
    std::cout << "  nonEmpty: "
              << (nonEmpty.empty() ? "empty" : nonEmpty.toString()) << "\n";

    //--------------------------------------------------------------------------
    // 14. Error Handling
    //--------------------------------------------------------------------------
    printSection("Error Handling");

    atom::meta::Any errorValue(42);

    try {
        // Try to cast to wrong type
        std::string wrongCast = errorValue.cast<std::string>();
        std::cout << "This should not print: " << wrongCast << "\n";
    } catch (const std::exception& e) {
        std::cout << "Expected exception on wrong cast: " << e.what() << "\n";
    }

    try {
        // Try to iterate non-iterable
        errorValue.foreach ([](const atom::meta::Any& item) {
            std::cout << "This should not print: " << item.toString() << "\n";
        });
    } catch (const std::exception& e) {
        std::cout << "Expected exception on foreach with non-iterable: "
                  << e.what() << "\n";
    }

    try {
        // Large allocation to potentially trigger bad_alloc
        std::vector<int> hugeVector(
            1000000000, 1);  // Might not actually fail on modern systems
        atom::meta::Any hugeValue(hugeVector);
        std::cout << "Created huge value successfully\n";
    } catch (const std::bad_alloc& e) {
        std::cout << "Bad allocation exception: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cout << "Other exception during huge allocation: " << e.what()
                  << "\n";
    }

    std::cout << "\nAll Any examples completed successfully!\n";
    return 0;
}
