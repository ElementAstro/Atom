#include "atom/meta/any.hpp"
#include <chrono>
#include <format>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace atom::meta;
using namespace std::chrono_literals;

// Sample struct for demonstrating complex types
struct Person {
    std::string name;
    int age;

    bool operator==(const Person& other) const {
        return name == other.name && age == other.age;
    }
};

// Custom hash function for BoxedValue
struct BoxedValueHash {
    size_t operator()(const BoxedValue& value) const {
        return std::hash<std::string>{}(value.debugString());
    }
};

// Equality comparison for BoxedValue (if not already defined)
bool operator==(const BoxedValue& lhs, const BoxedValue& rhs) {
    return lhs.debugString() == rhs.debugString();
}

// Function that transforms a BoxedValue
void transformValue(BoxedValue& value) {
    if (auto intPtr = value.tryCast<int>()) {
        value = *intPtr * 2;  // Double the integer value
    } else if (auto strPtr = value.tryCast<std::string>()) {
        value = *strPtr + " (transformed)";
    }
}

// Function that works with attributes
void addMetadata(BoxedValue& value, const std::string& source) {
    value.setAttr("source", var(source));
    value.setAttr("processed_at", var(std::chrono::system_clock::now()));
    value.setAttr("version", var(1.0));
}

// Thread-safe access demonstration
void threadAccess(BoxedValue& shared, int id,
                  std::vector<std::string>& results) {
    for (int i = 0; i < 50; i++) {
        // Read value
        auto val = shared.get();

        // Update access count attribute
        if (shared.hasAttr("access_count")) {
            auto countAttr = shared.getAttr("access_count");
            if (auto countPtr = countAttr.tryCast<int>()) {
                shared.setAttr("access_count", var(*countPtr + 1));
                results[id] = std::format("Thread {} - access count: {}", id,
                                          *countPtr + 1);
            }
        }

        // Brief pause to simulate work
        std::this_thread::sleep_for(10ms);
    }
}

int main() {
    std::cout << "=========================================" << std::endl;
    std::cout << "    BoxedValue Comprehensive Examples    " << std::endl;
    std::cout << "=========================================" << std::endl;

    //===========================================
    // 1. Creating BoxedValues
    //===========================================
    std::cout << "\n[1. Creating BoxedValues]\n" << std::endl;

    // Basic creation with different types
    auto intValue = var(42);
    auto doubleValue = var(3.14159);
    auto stringValue = var(std::string("Hello BoxedValue"));
    auto boolValue = var(true);
    auto charValue = var('A');

    // With descriptions
    auto namedValue = varWithDesc(123.456, "Pi multiplied by 39.3");

    // Constant values
    auto constValue = constVar(std::string("This cannot be modified"));

    // Empty/void value
    auto emptyValue = voidVar();

    // Print all created values
    std::cout << "Integer value: " << intValue.debugString() << std::endl;
    std::cout << "Double value: " << doubleValue.debugString() << std::endl;
    std::cout << "String value: " << stringValue.debugString() << std::endl;
    std::cout << "Boolean value: " << boolValue.debugString() << std::endl;
    std::cout << "Char value: " << charValue.debugString() << std::endl;
    std::cout << "Named value: " << namedValue.debugString() << std::endl;
    std::cout << "Constant value: " << constValue.debugString() << std::endl;
    std::cout << "Empty value: " << emptyValue.debugString() << std::endl;

    //===========================================
    // 2. Value Assignment and Modification
    //===========================================
    std::cout << "\n[2. Value Assignment and Modification]\n" << std::endl;

    // Reassign values
    intValue = 100;
    doubleValue = 2.71828;
    stringValue = std::string("Updated string value");

    std::cout << "After reassignment:" << std::endl;
    std::cout << "Integer value: " << intValue.debugString() << std::endl;
    std::cout << "Double value: " << doubleValue.debugString() << std::endl;
    std::cout << "String value: " << stringValue.debugString() << std::endl;

    // Try to modify constant value
    try {
        constValue = "Attempting to modify constant";
        std::cout << "ERROR: Should not reach here!" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected exception when modifying constant: " << e.what()
                  << std::endl;
    }

    // Use our transform function
    std::cout << "\nBefore transform, intValue = " << intValue.debugString()
              << std::endl;
    transformValue(intValue);
    std::cout << "After transform, intValue = " << intValue.debugString()
              << std::endl;

    std::cout << "Before transform, stringValue = " << stringValue.debugString()
              << std::endl;
    transformValue(stringValue);
    std::cout << "After transform, stringValue = " << stringValue.debugString()
              << std::endl;

    //===========================================
    // 3. Type Checking and Casting
    //===========================================
    std::cout << "\n[3. Type Checking and Casting]\n" << std::endl;

    // Type checking
    std::cout << "intValue is of type int? "
              << (intValue.isType<int>() ? "Yes" : "No") << std::endl;
    std::cout << "doubleValue is of type double? "
              << (doubleValue.isType<double>() ? "Yes" : "No") << std::endl;
    std::cout << "stringValue is of type std::string? "
              << (stringValue.isType<std::string>() ? "Yes" : "No")
              << std::endl;

    // Invalid type checks
    std::cout << "intValue is of type std::string? "
              << (intValue.isType<std::string>() ? "Yes" : "No") << std::endl;

    // Get type info
    std::cout << "Type of intValue: " << intValue.getTypeInfo().name()
              << std::endl;
    std::cout << "Type of stringValue: " << stringValue.getTypeInfo().name()
              << std::endl;

    // Successful casting
    if (auto intPtr = intValue.tryCast<int>()) {
        std::cout << "Successfully cast to int: " << *intPtr << std::endl;
    } else {
        std::cout << "Failed to cast to int (unexpected)" << std::endl;
    }

    // Failed casting
    if (auto doublePtr = stringValue.tryCast<double>()) {
        std::cout << "Unexpectedly cast string to double: " << *doublePtr
                  << std::endl;
    } else {
        std::cout << "Failed to cast string to double (expected)" << std::endl;
    }

    // Check if casting is possible
    std::cout << "Can cast intValue to int? "
              << (intValue.canCast<int>() ? "Yes" : "No") << std::endl;
    std::cout << "Can cast intValue to double? "
              << (intValue.canCast<double>() ? "Yes" : "No") << std::endl;
    std::cout << "Can cast stringValue to int? "
              << (stringValue.canCast<int>() ? "Yes" : "No") << std::endl;

    //===========================================
    // 4. References and Value Semantics
    //===========================================
    std::cout << "\n[4. References and Value Semantics]\n" << std::endl;

    // Create a value to reference
    int originalInt = 42;
    std::string originalString = "Original string";

    // Create BoxedValues that reference the originals
    auto intRef = var(std::ref(originalInt));
    auto stringRef = var(std::ref(originalString));

    std::cout << "Original int: " << originalInt << std::endl;
    std::cout << "Original string: " << originalString << std::endl;

    // Check if they are references
    std::cout << "intRef is a reference? " << (intRef.isRef() ? "Yes" : "No")
              << std::endl;
    std::cout << "stringRef is a reference? "
              << (stringRef.isRef() ? "Yes" : "No") << std::endl;

    // Modify through the BoxedValue
    if (auto ptr = intRef.tryCast<int>()) {
        *ptr = 100;
    }

    if (auto ptr = stringRef.tryCast<std::string>()) {
        *ptr = "Modified through reference";
    }

    // Check if originals were modified
    std::cout << "After modification, original int: " << originalInt
              << std::endl;
    std::cout << "After modification, original string: " << originalString
              << std::endl;

    // Non-reference behavior demonstration
    auto intCopy = var(originalInt);
    if (auto ptr = intCopy.tryCast<int>()) {
        *ptr = 200;
        std::cout << "Modified copy to: " << *ptr << std::endl;
    }
    std::cout << "Original int after modifying copy: " << originalInt
              << " (unchanged)" << std::endl;

    //===========================================
    // 5. Attributes System
    //===========================================
    std::cout << "\n[5. Attributes System]\n" << std::endl;

    auto record = var(std::string("Data Record"));

    // Add attributes
    record.setAttr("created", var(std::chrono::system_clock::now()));
    record.setAttr("owner", var(std::string("System Admin")));
    record.setAttr("read_only", var(true));
    record.setAttr("counter", var(0));

    // List all attributes
    std::cout << "Attributes for record:" << std::endl;
    for (const auto& attrName : record.listAttrs()) {
        auto attr = record.getAttr(attrName);
        std::cout << " - " << attrName << ": " << attr.debugString()
                  << std::endl;
    }

    // Check for attribute existence
    std::cout << "Has 'owner' attribute? "
              << (record.hasAttr("owner") ? "Yes" : "No") << std::endl;
    std::cout << "Has 'missing' attribute? "
              << (record.hasAttr("missing") ? "Yes" : "No") << std::endl;

    // Add metadata through our helper function
    addMetadata(record, "example_source");

    std::cout << "\nAfter adding metadata:" << std::endl;
    for (const auto& attrName : record.listAttrs()) {
        auto attr = record.getAttr(attrName);
        std::cout << " - " << attrName << ": " << attr.debugString()
                  << std::endl;
    }

    // Modify attribute
    if (record.hasAttr("counter")) {
        auto counterAttr = record.getAttr("counter");
        if (auto counterPtr = counterAttr.tryCast<int>()) {
            record.setAttr("counter", var(*counterPtr + 1));
        }
    }

    // Delete attribute
    record.removeAttr("read_only");
    std::cout << "\nAfter removing 'read_only' attribute:" << std::endl;
    std::cout << "Has 'read_only' attribute? "
              << (record.hasAttr("read_only") ? "Yes" : "No") << std::endl;

    //===========================================
    // 6. Complex Types
    //===========================================
    std::cout << "\n[6. Complex Types]\n" << std::endl;

    // Create a complex object
    Person alice{"Alice Smith", 28};
    auto personValue = var(alice);

    std::cout << "Person BoxedValue: " << personValue.debugString()
              << std::endl;

    // Access the person object
    if (auto personPtr = personValue.tryCast<Person>()) {
        std::cout << "Name: " << personPtr->name << ", Age: " << personPtr->age
                  << std::endl;
    }

    // Modify the original after storing in BoxedValue
    alice.age = 29;

    // Check if the BoxedValue reflects the change (it shouldn't, as it's a
    // copy)
    if (auto personPtr = personValue.tryCast<Person>()) {
        std::cout << "After modifying original - Name: " << personPtr->name
                  << ", Age: " << personPtr->age << " (should still be 28)"
                  << std::endl;
    }

    // Create reference to complex object
    auto personRef = var(std::ref(alice));

    // Modify through reference
    if (auto personPtr = personRef.tryCast<Person>()) {
        personPtr->name = "Alice Johnson";
        personPtr->age = 30;
    }

    // Check original reflects changes
    std::cout << "Original person after modifying through reference - "
              << "Name: " << alice.name << ", Age: " << alice.age << std::endl;

    // Store a vector in BoxedValue
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    auto vectorValue = var(numbers);

    std::cout << "Vector BoxedValue: " << vectorValue.debugString()
              << std::endl;

    //===========================================
    // 7. Thread Safety
    //===========================================
    std::cout << "\n[7. Thread Safety]\n" << std::endl;

    // Create a shared BoxedValue
    auto sharedValue = var(std::string("Shared resource"));
    sharedValue.setAttr("access_count", var(0));

    // Create threads to access it
    std::vector<std::thread> threads;
    std::vector<std::string> results(5);

    std::cout << "Starting 5 threads to access shared value..." << std::endl;

    for (int i = 0; i < 5; i++) {
        threads.emplace_back(threadAccess, std::ref(sharedValue), i,
                             std::ref(results));
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    // Show results
    std::cout << "\nThread access results:" << std::endl;
    for (const auto& result : results) {
        std::cout << result << std::endl;
    }

    // Check final access count
    if (auto countAttr = sharedValue.getAttr("access_count");
        auto countPtr = countAttr.tryCast<int>()) {
        std::cout << "Final access count: " << *countPtr << std::endl;
    }

    //===========================================
    // 8. Using BoxedValue as Map Keys
    //===========================================
    std::cout << "\n[8. Using BoxedValue as Map Keys]\n" << std::endl;

    // Create a map with BoxedValue as keys
    std::unordered_map<BoxedValue, std::string, BoxedValueHash> valueMap;

    // Add some entries
    valueMap[var(1)] = "One";
    valueMap[var(2)] = "Two";
    valueMap[var("three")] = "String key";
    valueMap[var(true)] = "Boolean key";

    // Create a Person object as key
    Person bob{"Bob Wilson", 45};
    valueMap[var(bob)] = "Person key";

    // Look up values
    std::cout << "Map lookup for 1: " << valueMap[var(1)] << std::endl;
    std::cout << "Map lookup for 2: " << valueMap[var(2)] << std::endl;
    std::cout << "Map lookup for \"three\": " << valueMap[var("three")]
              << std::endl;
    std::cout << "Map lookup for true: " << valueMap[var(true)] << std::endl;
    std::cout << "Map lookup for Person: " << valueMap[var(bob)] << std::endl;

    //===========================================
    // 9. Special State Handling
    //===========================================
    std::cout << "\n[9. Special State Handling]\n" << std::endl;

    // Create null, undefined, void values
    BoxedValue nullValue;  // Default constructor creates a null value
    auto undefValue = makeBoxedValue<void>();  // Create an undefined value

    std::cout << "nullValue is null? " << (nullValue.isNull() ? "Yes" : "No")
              << std::endl;
    std::cout << "undefValue is undefined? "
              << (undefValue.isUndef() ? "Yes" : "No") << std::endl;
    std::cout << "emptyValue is void? " << (emptyValue.isVoid() ? "Yes" : "No")
              << std::endl;

    // Return value flag
    auto returnVal =
        makeBoxedValue(42, true, false);  // Create with returnValue flag
    std::cout << "returnVal is return value? "
              << (returnVal.isReturnValue() ? "Yes" : "No") << std::endl;

    returnVal.resetReturnValue();  // Reset the flag
    std::cout << "After reset, is return value? "
              << (returnVal.isReturnValue() ? "Yes" : "No") << std::endl;

    // Exception handling with null/undef values
    try {
        auto result = nullValue.get();
        std::cout << "Value: " << result << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected exception when accessing null value: "
                  << e.what() << std::endl;
    }

    try {
        nullValue.setAttr("test", var(123));
    } catch (const std::exception& e) {
        std::cout << "Expected exception setting attribute on null value: "
                  << e.what() << std::endl;
    }

    //===========================================
    // 10. Performance Considerations
    //===========================================
    std::cout << "\n[10. Performance Considerations]\n" << std::endl;

    // Small buffer optimization demonstration
    auto smallInt = var(42);                       // Should use small buffer
    auto smallString = var(std::string("Small"));  // Should use small buffer

    // Large data that won't fit in small buffer
    std::vector<int> largeVector(1000, 42);
    auto largeObject = var(largeVector);

    std::string largeString(1000, 'X');
    auto largeStrObj = var(largeString);

    std::cout << "Created various sized objects to demonstrate small buffer "
                 "optimization"
              << std::endl;
    std::cout << "Small int: " << smallInt.debugString() << std::endl;
    std::cout << "Small string: " << smallString.debugString() << std::endl;
    std::cout << "Large vector size: " << largeVector.size() << std::endl;
    std::cout << "Large string size: " << largeString.size() << std::endl;

    // A simple benchmark would normally be included here

    std::cout << "\nAll BoxedValue examples completed successfully!"
              << std::endl;

    return 0;
}