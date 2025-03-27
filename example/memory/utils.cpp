/**
 * @file utils_example.cpp
 * @brief Comprehensive examples of using the memory utilities from
 * atom/memory/utils.hpp
 * @author Example Author
 * @date 2025-03-23
 */

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "atom/memory/utils.hpp"

// Custom class to demonstrate memory utilities
class TestObject {
public:
    // Default constructor
    TestObject() : id_(-1), name_("Default") {
        std::cout << "TestObject default constructed: " << toString()
                  << std::endl;
    }

    // Constructor with parameters
    TestObject(int id, const std::string& name) : id_(id), name_(name) {
        std::cout << "TestObject constructed: " << toString() << std::endl;
    }

    // Constructor with just id
    explicit TestObject(int id)
        : id_(id), name_("Unnamed-" + std::to_string(id)) {
        std::cout << "TestObject constructed with ID: " << toString()
                  << std::endl;
    }

    // Copy constructor
    TestObject(const TestObject& other)
        : id_(other.id_), name_(other.name_ + " (copy)") {
        std::cout << "TestObject copy constructed: " << toString() << std::endl;
    }

    // Move constructor
    TestObject(TestObject&& other) noexcept
        : id_(other.id_), name_(std::move(other.name_)) {
        other.id_ = -1;
        std::cout << "TestObject move constructed: " << toString() << std::endl;
    }

    // Destructor
    ~TestObject() {
        std::cout << "TestObject destroyed: " << toString() << std::endl;
    }

    // Utility method to get string representation
    std::string toString() const {
        return "[ID: " + std::to_string(id_) + ", Name: " + name_ + "]";
    }

    // Getters
    int getId() const { return id_; }
    const std::string& getName() const { return name_; }

    // Setters
    void setId(int id) { id_ = id; }
    void setName(const std::string& name) { name_ = name; }

private:
    int id_;
    std::string name_;
};

// Custom deleter for demonstration
struct CustomDeleter {
    void operator()(TestObject* obj) const {
        std::cout << "CustomDeleter called for: " << obj->toString()
                  << std::endl;
        delete obj;
    }
};

// Singleton class for demonstration
class MySingleton {
public:
    MySingleton() { std::cout << "MySingleton constructed" << std::endl; }

    ~MySingleton() { std::cout << "MySingleton destroyed" << std::endl; }

    void doSomething() {
        std::cout << "MySingleton is doing something..." << std::endl;
    }

    int getValue() const { return value_; }

    void setValue(int value) { value_ = value; }

private:
    int value_ = 42;
};

// Helper function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// Function to demonstrate multi-threaded singleton access
void testSingletonInThread(int threadId) {
    std::cout << "Thread " << threadId << " starting" << std::endl;

    // Simulate some work before accessing the singleton
    std::this_thread::sleep_for(std::chrono::milliseconds(10 * threadId));

    // Get singleton instance
    auto instance =
        atom::memory::ThreadSafeSingleton<MySingleton>::getInstance();

    // Use the singleton
    std::cout << "Thread " << threadId
              << " got singleton, value = " << instance->getValue()
              << std::endl;

    // Modify the singleton (to demonstrate shared state)
    instance->setValue(instance->getValue() + threadId);
    std::cout << "Thread " << threadId << " updated value to "
              << instance->getValue() << std::endl;

    // Simulate more work
    std::this_thread::sleep_for(std::chrono::milliseconds(5 * threadId));

    // Access the singleton again
    auto instance2 =
        atom::memory::ThreadSafeSingleton<MySingleton>::getInstance();
    std::cout << "Thread " << threadId
              << " got singleton again, value = " << instance2->getValue()
              << std::endl;
}

int main() {
    std::cout << "MEMORY UTILITIES COMPREHENSIVE EXAMPLES\n";
    std::cout << "======================================\n";

    //--------------------------------------------------------------------------
    // 1. Basic Memory Configuration
    //--------------------------------------------------------------------------
    printSection("1. Basic Memory Configuration");

    std::cout << "Default alignment: " << atom::memory::Config::DefaultAlignment
              << std::endl;
    std::cout << "Memory tracking enabled: "
              << (atom::memory::Config::EnableMemoryTracking ? "Yes" : "No")
              << std::endl;

    //--------------------------------------------------------------------------
    // 2. Type Trait Utilities
    //--------------------------------------------------------------------------
    printSection("2. Type Trait Utilities");

    // Testing IsConstructible
    std::cout
        << "TestObject is constructible with (int, string): "
        << (atom::memory::IsConstructible<TestObject, int, std::string>::value
                ? "Yes"
                : "No")
        << std::endl;

    std::cout << "TestObject is constructible with (int): "
              << (atom::memory::IsConstructible<TestObject, int>::value ? "Yes"
                                                                        : "No")
              << std::endl;

    std::cout << "TestObject is constructible with (): "
              << (atom::memory::IsConstructible<TestObject>::value ? "Yes"
                                                                   : "No")
              << std::endl;

    std::cout << "TestObject is constructible with (double): "
              << (atom::memory::IsConstructible<TestObject, double>::value
                      ? "Yes"
                      : "No")
              << std::endl;

    std::cout << "TestObject is constructible with (string): "
              << (atom::memory::IsConstructible<TestObject, std::string>::value
                      ? "Yes"
                      : "No")
              << std::endl;

    //--------------------------------------------------------------------------
    // 3. Smart Pointer Creation with makeShared and makeUnique
    //--------------------------------------------------------------------------
    printSection("3. Smart Pointer Creation with makeShared and makeUnique");

    // Using makeShared
    std::cout << "Creating shared_ptr objects using makeShared..." << std::endl;

    auto sharedObj1 = atom::memory::makeShared<TestObject>();
    std::cout << "sharedObj1: " << sharedObj1->toString() << std::endl;

    auto sharedObj2 = atom::memory::makeShared<TestObject>(1, "Object Two");
    std::cout << "sharedObj2: " << sharedObj2->toString() << std::endl;

    auto sharedObj3 = atom::memory::makeShared<TestObject>(3);
    std::cout << "sharedObj3: " << sharedObj3->toString() << std::endl;

    // Compile-time error check for invalid constructor arguments
    // Uncomment to see compile error:
    // auto sharedObjInvalid = atom::memory::makeShared<TestObject>("invalid");

    // Using makeUnique
    std::cout << "\nCreating unique_ptr objects using makeUnique..."
              << std::endl;

    auto uniqueObj1 = atom::memory::makeUnique<TestObject>();
    std::cout << "uniqueObj1: " << uniqueObj1->toString() << std::endl;

    auto uniqueObj2 =
        atom::memory::makeUnique<TestObject>(2, "Object Two Unique");
    std::cout << "uniqueObj2: " << uniqueObj2->toString() << std::endl;

    auto uniqueObj3 = atom::memory::makeUnique<TestObject>(3);
    std::cout << "uniqueObj3: " << uniqueObj3->toString() << std::endl;

    //--------------------------------------------------------------------------
    // 4. Custom Deleters with makeSharedWithDeleter and makeUniqueWithDeleter
    //--------------------------------------------------------------------------
    printSection(
        "4. Custom Deleters with makeSharedWithDeleter and "
        "makeUniqueWithDeleter");

    // Using makeSharedWithDeleter
    std::cout << "Creating shared_ptr with custom deleter..." << std::endl;
    {
        auto sharedWithDeleter =
            atom::memory::makeSharedWithDeleter<TestObject>(
                CustomDeleter{}, 4, "Shared Object With Deleter");

        std::cout << "sharedWithDeleter: " << sharedWithDeleter->toString()
                  << std::endl;
        std::cout << "Object will be deleted by CustomDeleter when going out "
                     "of scope..."
                  << std::endl;
    }
    std::cout << "sharedWithDeleter has been destroyed\n" << std::endl;

    // Using makeUniqueWithDeleter
    std::cout << "Creating unique_ptr with custom deleter..." << std::endl;
    {
        auto uniqueWithDeleter =
            atom::memory::makeUniqueWithDeleter<TestObject>(
                CustomDeleter{}, 5, "Unique Object With Deleter");

        std::cout << "uniqueWithDeleter: " << uniqueWithDeleter->toString()
                  << std::endl;
        std::cout << "Object will be deleted by CustomDeleter when going out "
                     "of scope..."
                  << std::endl;
    }
    std::cout << "uniqueWithDeleter has been destroyed\n" << std::endl;

    // Using lambda as custom deleter
    std::cout << "Creating smart pointer with lambda deleter..." << std::endl;
    {
        auto lambdaDeleter = [](TestObject* obj) {
            std::cout << "Lambda deleter called for: " << obj->toString()
                      << std::endl;
            delete obj;
        };

        auto withLambdaDeleter =
            atom::memory::makeSharedWithDeleter<TestObject>(
                lambdaDeleter, 6, "Object With Lambda Deleter");

        std::cout << "withLambdaDeleter: " << withLambdaDeleter->toString()
                  << std::endl;
        std::cout << "Object will be deleted by lambda deleter when going out "
                     "of scope..."
                  << std::endl;
    }
    std::cout << "withLambdaDeleter has been destroyed" << std::endl;

    //--------------------------------------------------------------------------
    // 5. Array Smart Pointers with makeSharedArray and makeUniqueArray
    //--------------------------------------------------------------------------
    printSection(
        "5. Array Smart Pointers with makeSharedArray and makeUniqueArray");

    // Using makeSharedArray
    std::cout << "Creating shared_ptr array..." << std::endl;
    {
        auto sharedArray = atom::memory::makeSharedArray<int>(5);

        // Initialize array values
        for (int i = 0; i < 5; ++i) {
            sharedArray[i] = i * 10;
        }

        // Print array values
        std::cout << "Shared array values: ";
        for (int i = 0; i < 5; ++i) {
            std::cout << sharedArray[i] << " ";
        }
        std::cout << std::endl;
    }

    // Using makeUniqueArray
    std::cout << "\nCreating unique_ptr array..." << std::endl;
    {
        auto uniqueArray = atom::memory::makeUniqueArray<double>(5);

        // Initialize array values
        for (int i = 0; i < 5; ++i) {
            uniqueArray[i] = i * 1.5;
        }

        // Print array values
        std::cout << "Unique array values: ";
        for (int i = 0; i < 5; ++i) {
            std::cout << uniqueArray[i] << " ";
        }
        std::cout << std::endl;
    }

    // Creating array of custom objects
    std::cout << "\nCreating array of custom objects..." << std::endl;
    {
        // Note: This requires TestObject to be default constructible
        auto objectArray = atom::memory::makeSharedArray<TestObject>(3);

        // Initialize array elements
        for (int i = 0; i < 3; ++i) {
            objectArray[i].setId(i + 10);
            objectArray[i].setName("Array Element " + std::to_string(i));
        }

        // Print array elements
        std::cout << "Object array elements:" << std::endl;
        for (int i = 0; i < 3; ++i) {
            std::cout << "  " << objectArray[i].toString() << std::endl;
        }
    }
    std::cout << "Object array has been destroyed" << std::endl;

    //--------------------------------------------------------------------------
    // 6. Thread-Safe Singleton Pattern
    //--------------------------------------------------------------------------
    printSection("6. Thread-Safe Singleton Pattern");

    // Access singleton from main thread
    std::cout << "Accessing singleton from main thread..." << std::endl;
    auto singleton =
        atom::memory::ThreadSafeSingleton<MySingleton>::getInstance();
    singleton->doSomething();
    std::cout << "Initial singleton value: " << singleton->getValue()
              << std::endl;

    // Test singleton in multiple threads
    std::cout << "\nTesting singleton in multiple threads..." << std::endl;

    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(testSingletonInThread, i);
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Access singleton again from main thread
    std::cout << "\nAccessing singleton again from main thread..." << std::endl;
    auto singleton2 =
        atom::memory::ThreadSafeSingleton<MySingleton>::getInstance();
    std::cout << "Final singleton value: " << singleton2->getValue()
              << std::endl;

    //--------------------------------------------------------------------------
    // 7. Weak Pointer Utilities
    //--------------------------------------------------------------------------
    printSection("7. Weak Pointer Utilities");

    // Create a shared_ptr to be tracked by weak_ptr
    std::cout << "Creating a shared_ptr and a weak_ptr..." << std::endl;
    std::weak_ptr<TestObject> weakObj;

    {
        auto sharedObj =
            atom::memory::makeShared<TestObject>(7, "Object for Weak Pointer");
        std::cout << "sharedObj created: " << sharedObj->toString()
                  << std::endl;

        // Store a weak reference
        weakObj = sharedObj;

        // Test lockWeak when the object is still valid
        std::cout << "\nTesting lockWeak when object is still valid..."
                  << std::endl;
        auto lockedObj = atom::memory::lockWeak(weakObj);

        if (lockedObj) {
            std::cout << "Successfully locked weak_ptr: "
                      << lockedObj->toString() << std::endl;
        } else {
            std::cout << "Failed to lock weak_ptr (this shouldn't happen)"
                      << std::endl;
        }

        // Object will be destroyed when leaving scope
        std::cout << "\nLetting shared_ptr go out of scope..." << std::endl;
    }

    // Test lockWeak when the object is no longer valid
    std::cout << "\nTesting lockWeak when object is no longer valid..."
              << std::endl;
    auto lockedObj = atom::memory::lockWeak(weakObj);

    if (lockedObj) {
        std::cout << "Successfully locked weak_ptr (this shouldn't happen)"
                  << std::endl;
    } else {
        std::cout << "Failed to lock weak_ptr: The object has been destroyed"
                  << std::endl;
    }

    // Test lockWeakOrCreate when object is invalid
    std::cout << "\nTesting lockWeakOrCreate when object is invalid..."
              << std::endl;
    auto lockedOrCreatedObj =
        atom::memory::lockWeakOrCreate(weakObj, 8, "Newly Created Object");

    if (lockedOrCreatedObj) {
        std::cout << "Successfully created new object: "
                  << lockedOrCreatedObj->toString() << std::endl;
    } else {
        std::cout << "Failed to create object (this shouldn't happen)"
                  << std::endl;
    }

    // Test lockWeakOrCreate again, should return the same object
    std::cout
        << "\nTesting lockWeakOrCreate again, should return existing object..."
        << std::endl;
    auto anotherLockedObj = atom::memory::lockWeakOrCreate(
        weakObj, 9, "This Should Not Be Created");

    if (anotherLockedObj) {
        std::cout << "Successfully locked existing object: "
                  << anotherLockedObj->toString() << std::endl;

        // Verify it's the same object
        if (anotherLockedObj == lockedOrCreatedObj) {
            std::cout << "Verified: Same object instance was returned"
                      << std::endl;
        } else {
            std::cout << "Error: Different object instance was returned"
                      << std::endl;
        }
    } else {
        std::cout << "Failed to lock object (this shouldn't happen)"
                  << std::endl;
    }

    //--------------------------------------------------------------------------
    // 8. Combining Multiple Utilities
    //--------------------------------------------------------------------------
    printSection("8. Combining Multiple Utilities");

    // Create a shared_ptr with custom deleter, store weak reference, then use
    // utilities
    std::cout << "Demonstrating combination of utilities..." << std::endl;

    std::weak_ptr<TestObject> complexWeakObj;

    {
        // Create shared_ptr with custom deleter
        auto complexSharedObj = atom::memory::makeSharedWithDeleter<TestObject>(
            [](TestObject* obj) {
                std::cout << "Combined example: Lambda deleter called for "
                          << obj->toString() << std::endl;
                delete obj;
            },
            10, "Complex Combined Example");

        // Store weak reference
        complexWeakObj = complexSharedObj;

        // Use the object
        std::cout << "Original object: " << complexSharedObj->toString()
                  << std::endl;
        complexSharedObj->setName(complexSharedObj->getName() + " (Modified)");
        std::cout << "Modified object: " << complexSharedObj->toString()
                  << std::endl;

        // Lock weak reference while object is valid
        auto lockedComplexObj = atom::memory::lockWeak(complexWeakObj);
        if (lockedComplexObj) {
            std::cout << "Successfully locked object: "
                      << lockedComplexObj->toString() << std::endl;
        }

        // Shared_ptr will go out of scope and trigger custom deleter
        std::cout << "\nLetting complex shared_ptr go out of scope..."
                  << std::endl;
    }

    // Try to recover the object
    std::cout << "\nAttempting to recover destroyed object..." << std::endl;
    auto recoveredObj =
        atom::memory::lockWeakOrCreate(complexWeakObj, 11, "Recovered Object");

    std::cout << "Created new object since original was destroyed: "
              << recoveredObj->toString() << std::endl;

    //--------------------------------------------------------------------------
    // Summary
    //--------------------------------------------------------------------------
    printSection("Summary");

    std::cout << "This example demonstrated the following utilities:"
              << std::endl;
    std::cout << "  1. Basic memory configuration constants" << std::endl;
    std::cout << "  2. Type trait utilities for compile-time validation"
              << std::endl;
    std::cout << "  3. Smart pointer creation with makeShared and makeUnique"
              << std::endl;
    std::cout << "  4. Custom deleters with makeSharedWithDeleter and "
                 "makeUniqueWithDeleter"
              << std::endl;
    std::cout
        << "  5. Array smart pointers with makeSharedArray and makeUniqueArray"
        << std::endl;
    std::cout << "  6. Thread-safe singleton pattern" << std::endl;
    std::cout << "  7. Weak pointer utilities (lockWeak and lockWeakOrCreate)"
              << std::endl;
    std::cout << "  8. Combined usage of multiple utilities" << std::endl;

    return 0;
}