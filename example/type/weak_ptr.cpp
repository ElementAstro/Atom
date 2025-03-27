#include "../atom/type/weak_ptr.hpp"
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include <functional>
#include <atomic>
#include <cassert>
#include <iomanip>
#include <sstream>

using namespace atom::type;
using namespace std::chrono_literals;

// Simple test classes
class TestObject {
private:
    int id_;
    std::string name_;
    mutable std::atomic<int> access_count_{0};

public:
    TestObject(int id, std::string name) : id_(id), name_(std::move(name)) {
        std::cout << "TestObject #" << id_ << " (" << name_ << ") constructed" << std::endl;
    }
    
    ~TestObject() {
        std::cout << "TestObject #" << id_ << " (" << name_ << ") destroyed" << std::endl;
    }
    
    int getId() const { 
        access_count_++;
        return id_; 
    }
    
    std::string getName() const { 
        access_count_++;
        return name_; 
    }
    
    void setName(const std::string& name) {
        access_count_++;
        name_ = name;
    }
    
    int getAccessCount() const {
        return access_count_.load();
    }
    
    void performOperation() const {
        access_count_++;
        std::cout << "Operation performed on TestObject #" << id_ << " (" << name_ << ")" << std::endl;
    }
};

// Derived class for testing cast functionality
class DerivedObject : public TestObject {
private:
    double extra_data_;

public:
    DerivedObject(int id, std::string name, double extra_data) 
        : TestObject(id, std::move(name)), extra_data_(extra_data) {
        std::cout << "DerivedObject with extra_data=" << extra_data_ << " constructed" << std::endl;
    }
    
    ~DerivedObject() {
        std::cout << "DerivedObject with extra_data=" << extra_data_ << " destroyed" << std::endl;
    }
    
    double getExtraData() const {
        return extra_data_;
    }
    
    void setExtraData(double value) {
        extra_data_ = value;
    }
};

// Helper function to print headers
void printSection(const std::string& title) {
    std::cout << "\n===== " << title << " =====\n";
}

// Helper function to print subsection headers
void printSubSection(const std::string& title) {
    std::cout << "\n----- " << title << " -----\n";
}

// Example 1: Basic Usage
void basicUsageExample() {
    printSection("Basic Usage");

    // Create a shared_ptr to a TestObject
    auto shared = std::make_shared<TestObject>(1, "Basic Test");
    
    printSubSection("Construction and State Checking");
    // Create an EnhancedWeakPtr from the shared_ptr
    EnhancedWeakPtr<TestObject> weak(shared);
    
    // Check if the weak pointer is expired
    std::cout << "Is weak pointer expired? " << (weak.expired() ? "Yes" : "No") << std::endl;
    
    // Get the use count
    std::cout << "Use count: " << weak.useCount() << std::endl;
    
    printSubSection("Locking the Weak Pointer");
    // Lock the weak pointer to get a shared_ptr
    if (auto locked = weak.lock()) {
        std::cout << "Successfully locked weak pointer" << std::endl;
        std::cout << "Object data: " << locked->getId() << ", " << locked->getName() << std::endl;
    } else {
        std::cout << "Failed to lock weak pointer" << std::endl;
    }
    
    printSubSection("Handling Expiration");
    // Make the weak pointer expire by resetting the original shared_ptr
    std::cout << "Resetting original shared_ptr..." << std::endl;
    shared.reset();
    
    // Check if the weak pointer is now expired
    std::cout << "Is weak pointer expired? " << (weak.expired() ? "Yes" : "No") << std::endl;
    
    // Try to lock an expired weak pointer
    if (auto locked = weak.lock()) {
        std::cout << "Successfully locked weak pointer (shouldn't happen)" << std::endl;
    } else {
        std::cout << "Failed to lock expired weak pointer (expected)" << std::endl;
    }
    
    printSubSection("Manual Reset");
    // Create a new shared_ptr and weak_ptr
    shared = std::make_shared<TestObject>(2, "Reset Test");
    EnhancedWeakPtr<TestObject> resetWeak(shared);
    
    // Reset the weak pointer manually
    std::cout << "Manually resetting weak pointer..." << std::endl;
    resetWeak.reset();
    
    // Verify it's expired even though the shared_ptr is still valid
    std::cout << "Is weak pointer expired after reset? " << (resetWeak.expired() ? "Yes" : "No") << std::endl;
    std::cout << "Original shared_ptr use count: " << shared.use_count() << std::endl;
    
    printSubSection("Getting Lock Attempts");
    EnhancedWeakPtr<TestObject> lockCounter(shared);
    
    // Perform several lock attempts
    for (int i = 0; i < 5; i++) {
        auto locked = lockCounter.lock();
    }
    
    std::cout << "Number of lock attempts: " << lockCounter.getLockAttempts() << std::endl;
}

// Example 2: Advanced Locking Techniques
void advancedLockingExample() {
    printSection("Advanced Locking Techniques");
    
    // Create a shared_ptr to a TestObject
    auto shared = std::make_shared<TestObject>(3, "Advanced Lock Test");
    
    // Create an EnhancedWeakPtr from the shared_ptr
    EnhancedWeakPtr<TestObject> weak(shared);
    
    printSubSection("Using withLock for Safe Access");
    // Use withLock to safely access the object
    auto result = weak.withLock([](TestObject& obj) {
        std::cout << "Accessing object with ID: " << obj.getId() << std::endl;
        return obj.getName();
    });
    
    if (result) {
        std::cout << "withLock returned: " << *result << std::endl;
    } else {
        std::cout << "withLock failed to access the object" << std::endl;
    }
    
    // Use withLock with a void return type
    bool success = weak.withLock([](TestObject& obj) {
        std::cout << "Performing void operation on object: " << obj.getName() << std::endl;
        obj.setName("Updated Name");
    });
    
    std::cout << "Void operation success: " << (success ? "Yes" : "No") << std::endl;
    
    // Verify the name was updated
    std::string name = weak.withLock([](TestObject& obj) { return obj.getName(); }).value_or("Unknown");
    std::cout << "Updated name: " << name << std::endl;
    
    printSubSection("tryLockOrElse Method");
    // Use tryLockOrElse to handle both success and failure cases
    auto nameOrDefault = weak.tryLockOrElse(
        // Success case
        [](TestObject& obj) {
            return "Object name: " + obj.getName();
        },
        // Failure case
        []() {
            return "Object not available";
        }
    );
    
    std::cout << "tryLockOrElse result: " << nameOrDefault << std::endl;
    
    printSubSection("Periodic Lock Attempts");
    // Use tryLockPeriodic to attempt locking periodically
    std::cout << "Attempting periodic locks (should succeed immediately)..." << std::endl;
    auto periodicLock = weak.tryLockPeriodic(100ms, 5);
    
    if (periodicLock) {
        std::cout << "Successfully obtained lock periodically for: " << periodicLock->getName() << std::endl;
    } else {
        std::cout << "Failed to obtain lock after periodic attempts" << std::endl;
    }
    
    // Make the object expire
    shared.reset();
    
    // Try periodic locking on an expired pointer
    std::cout << "Attempting periodic locks on expired pointer..." << std::endl;
    auto failedLock = weak.tryLockPeriodic(50ms, 3);
    
    if (failedLock) {
        std::cout << "Unexpectedly obtained lock" << std::endl;
    } else {
        std::cout << "Failed to obtain lock after 3 attempts (expected)" << std::endl;
    }
}

// Example 3: Asynchronous Operations
void asynchronousOperationsExample() {
    printSection("Asynchronous Operations");
    
    // Create a shared_ptr to a TestObject
    auto shared = std::make_shared<TestObject>(4, "Async Test");
    
    // Create an EnhancedWeakPtr from the shared_ptr
    EnhancedWeakPtr<TestObject> weak(shared);
    
    printSubSection("Async Lock");
    // Perform an asynchronous lock
    std::cout << "Starting async lock operation..." << std::endl;
    auto future = weak.asyncLock();
    
    // Do some other work
    std::cout << "Doing other work while lock is in progress..." << std::endl;
    std::this_thread::sleep_for(100ms);
    
    // Get the result of the async lock
    auto asyncLocked = future.get();
    if (asyncLocked) {
        std::cout << "Async lock successful for object: " << asyncLocked->getName() << std::endl;
    } else {
        std::cout << "Async lock failed" << std::endl;
    }
    
    printSubSection("Waiting with Timeout");
    // Set up a condition to wait for
    std::atomic<bool> condition{false};
    
    // Start a thread that will set the condition after a delay
    std::thread conditionThread([&]() {
        std::this_thread::sleep_for(300ms);
        std::cout << "Setting condition to true" << std::endl;
        condition.store(true);
    });
    
    // Wait for the object with timeout
    bool waitResult = weak.waitFor(500ms);
    std::cout << "waitFor result: " << (waitResult ? "Object available" : "Timeout or object expired") << std::endl;
    
    // Wait until the condition is true
    bool predResult = weak.waitUntil([&]() { return condition.load(); });
    std::cout << "waitUntil result: " << (predResult ? "Condition met and object available" : "Object expired") << std::endl;
    
    // Cleanup
    if (conditionThread.joinable()) {
        conditionThread.join();
    }
    
    printSubSection("Notification Mechanism");
    // Create a thread that waits for notification
    std::atomic<bool> notified{false};
    std::thread waitingThread([&]() {
        std::cout << "Thread waiting for notification..." << std::endl;
        weak.waitFor(1s);  // This will wait until notified or timeout
        notified.store(true);
        std::cout << "Thread received notification or timed out" << std::endl;
    });
    
    // Sleep briefly to ensure the thread starts waiting
    std::this_thread::sleep_for(100ms);
    
    // Send notification
    std::cout << "Sending notification to waiting threads..." << std::endl;
    weak.notifyAll();
    
    // Join the thread
    if (waitingThread.joinable()) {
        waitingThread.join();
    }
    
    std::cout << "Was thread notified? " << (notified.load() ? "Yes" : "No") << std::endl;
}

// Example 4: Type Casting and Special Operations
void typeCastingExample() {
    printSection("Type Casting and Special Operations");
    
    // Create a shared_ptr to a DerivedObject
    auto derivedShared = std::make_shared<DerivedObject>(5, "Derived Test", 3.14159);
    
    // Create an EnhancedWeakPtr to the base type
    EnhancedWeakPtr<TestObject> baseWeak(derivedShared);
    
    printSubSection("Type Casting");
    // Cast the weak pointer to the derived type
    auto derivedWeak = baseWeak.cast<DerivedObject>();
    
    // Test if the cast worked
    auto result = derivedWeak.withLock([](DerivedObject& obj) {
        std::cout << "Successfully cast to derived type" << std::endl;
        std::cout << "Base properties - ID: " << obj.getId() << ", Name: " << obj.getName() << std::endl;
        std::cout << "Derived property - Extra data: " << obj.getExtraData() << std::endl;
        return obj.getExtraData();
    });
    
    if (result) {
        std::cout << "Cast and lock succeeded, extra data value: " << *result << std::endl;
    } else {
        std::cout << "Cast or lock failed" << std::endl;
    }
    
    printSubSection("Weak Pointer to Shared Pointer");
    // Get the underlying weak_ptr
    std::weak_ptr<TestObject> stdWeakPtr = baseWeak.getWeakPtr();
    std::cout << "Standard weak_ptr use count: " << stdWeakPtr.use_count() << std::endl;
    
    // Create a shared_ptr from the weak_ptr
    auto createdShared = baseWeak.createShared();
    if (createdShared) {
        std::cout << "Successfully created shared_ptr from weak_ptr" << std::endl;
        std::cout << "Created shared_ptr use count: " << createdShared.use_count() << std::endl;
    } else {
        std::cout << "Failed to create shared_ptr (object expired)" << std::endl;
    }
    
    printSubSection("Total Instances Tracking");
    // Get the total number of EnhancedWeakPtr instances
    size_t beforeCount = EnhancedWeakPtr<TestObject>::getTotalInstances();
    std::cout << "Total EnhancedWeakPtr instances before: " << beforeCount << std::endl;
    
    // Create more instances
    {
        EnhancedWeakPtr<TestObject> temp1(derivedShared);
        EnhancedWeakPtr<TestObject> temp2(derivedShared);
        
        size_t duringCount = EnhancedWeakPtr<TestObject>::getTotalInstances();
        std::cout << "Total EnhancedWeakPtr instances during: " << duringCount << std::endl;
        assert(duringCount > beforeCount);
    }
    
    size_t afterCount = EnhancedWeakPtr<TestObject>::getTotalInstances();
    std::cout << "Total EnhancedWeakPtr instances after: " << afterCount << std::endl;
    assert(afterCount == beforeCount);
    
    printSubSection("Equality Comparison");
    // Create two weak pointers to the same object
    EnhancedWeakPtr<TestObject> weak1(derivedShared);
    EnhancedWeakPtr<TestObject> weak2(derivedShared);
    
    // Create a weak pointer to a different object
    auto differentShared = std::make_shared<TestObject>(6, "Different Test");
    EnhancedWeakPtr<TestObject> weak3(differentShared);
    
    // Compare weak pointers
    std::cout << "weak1 == weak2: " << (weak1 == weak2 ? "true" : "false") << std::endl;
    std::cout << "weak1 == weak3: " << (weak1 == weak3 ? "true" : "false") << std::endl;
}

// Example 5: Void Specialization
void voidSpecializationExample() {
    printSection("Void Specialization");
    
    // Create a shared_ptr<void> from a concrete type
    auto original = std::make_shared<TestObject>(7, "Void Test");
    std::shared_ptr<void> voidShared = original;
    
    // Create an EnhancedWeakPtr<void> from the shared_ptr
    EnhancedWeakPtr<void> voidWeak(voidShared);
    
    printSubSection("Basic Operations with void Type");
    // Check if the weak pointer is expired
    std::cout << "Is void weak pointer expired? " << (voidWeak.expired() ? "Yes" : "No") << std::endl;
    
    // Get the use count
    std::cout << "Use count: " << voidWeak.useCount() << std::endl;
    
    // Lock the void weak pointer
    if (auto locked = voidWeak.lock()) {
        std::cout << "Successfully locked void weak pointer" << std::endl;
    } else {
        std::cout << "Failed to lock void weak pointer" << std::endl;
    }
    
    printSubSection("withLock for void Type");
    // Use withLock with void return type
    bool success = voidWeak.withLock([]() {
        std::cout << "Performing void operation on void pointer" << std::endl;
    });
    
    std::cout << "Void operation success: " << (success ? "Yes" : "No") << std::endl;
    
    // Use withLock with non-void return type
    auto result = voidWeak.withLock([]() {
        return std::string("Data from void pointer operation");
    });
    
    if (result) {
        std::cout << "withLock on void pointer returned: " << *result << std::endl;
    } else {
        std::cout << "withLock on void pointer failed" << std::endl;
    }
    
    printSubSection("tryLockOrElse with void Type");
    // Use tryLockOrElse with void pointer
    auto resultOrDefault = voidWeak.tryLockOrElse(
        // Success case
        []() {
            return "Successfully accessed void pointer";
        },
        // Failure case
        []() {
            return "Failed to access void pointer";
        }
    );
    
    std::cout << "tryLockOrElse result: " << resultOrDefault << std::endl;
    
    printSubSection("Casting from void Type");
    // Cast the void weak pointer back to the original type
    auto castBack = voidWeak.cast<TestObject>();
    
    // Use withLock on the cast pointer
    auto name = castBack.withLock([](TestObject& obj) {
        return obj.getName();
    });
    
    if (name) {
        std::cout << "Successfully cast back from void to TestObject: " << *name << std::endl;
    } else {
        std::cout << "Failed to cast back from void to TestObject" << std::endl;
    }
    
    // Clean up
    original.reset();
    
    // Verify both weak pointers are now expired
    std::cout << "Original weak ptr expired: " << (voidWeak.expired() ? "Yes" : "No") << std::endl;
    std::cout << "Cast weak ptr expired: " << (castBack.expired() ? "Yes" : "No") << std::endl;
}

// Example 6: Group Operations
void groupOperationsExample() {
    printSection("Group Operations");
    
    // Create a vector of shared pointers
    std::vector<std::shared_ptr<TestObject>> sharedPtrs;
    for (int i = 0; i < 5; ++i) {
        sharedPtrs.push_back(std::make_shared<TestObject>(
            100 + i, "Group-" + std::to_string(i)));
    }
    
    printSubSection("Creating Weak Pointer Group");
    // Create a group of weak pointers
    auto weakPtrGroup = createWeakPtrGroup(sharedPtrs);
    std::cout << "Created weak pointer group with " << weakPtrGroup.size() << " elements" << std::endl;
    
    printSubSection("Batch Operations");
    // Perform a batch operation on the group
    std::cout << "Performing batch operation on the group..." << std::endl;
    batchOperation(weakPtrGroup, [](TestObject& obj) {
        std::cout << "Batch operation on object #" << obj.getId() << " - " << obj.getName() << std::endl;
        obj.performOperation();
    });
    
    printSubSection("Individual Access After Batch");
    // Access individual elements after batch operation
    for (size_t i = 0; i < weakPtrGroup.size(); ++i) {
        weakPtrGroup[i].withLock([i](TestObject& obj) {
            std::cout << "Element " << i << " - ID: " << obj.getId() 
                      << ", Name: " << obj.getName() 
                      << ", Access count: " << obj.getAccessCount() << std::endl;
        });
    }
    
    printSubSection("Handling Expired Group Members");
    // Make some of the shared pointers expire
    std::cout << "Expiring elements 1 and 3..." << std::endl;
    sharedPtrs[1].reset();
    sharedPtrs[3].reset();
    
    // Try to access all elements including expired ones
    std::cout << "Trying to access all elements after expiration:" << std::endl;
    for (size_t i = 0; i < weakPtrGroup.size(); ++i) {
        bool accessed = weakPtrGroup[i].withLock([i](TestObject& obj) {
            std::cout << "Element " << i << " - Successfully accessed object #" 
                      << obj.getId() << std::endl;
        });
        
        if (!accessed) {
            std::cout << "Element " << i << " - Failed to access (expired)" << std::endl;
        }
    }
    
    printSubSection("Batch Operation with Expiry Handling");
    // Perform another batch operation with explicit handling
    std::cout << "Performing batch operation with expiry checks:" << std::endl;
    
    size_t successCount = 0;
    for (const auto& weakPtr : weakPtrGroup) {
        bool success = weakPtr.withLock([](TestObject& obj) {
            std::cout << "Processing object #" << obj.getId() << std::endl;
            obj.performOperation();
        });
        
        if (success) {
            successCount++;
        }
    }
    
    std::cout << "Successfully processed " << successCount << " out of " 
              << weakPtrGroup.size() << " objects" << std::endl;
}

// Example 7: Multi-threading Scenarios
void multiThreadingExample() {
    printSection("Multi-threading Scenarios");
    
    // Create a shared pointer that will be accessed from multiple threads
    auto shared = std::make_shared<TestObject>(200, "Thread-Test");
    EnhancedWeakPtr<TestObject> weak(shared);
    
    printSubSection("Concurrent Access");
    // Set up a flag for coordination
    std::atomic<bool> shouldContinue{true};
    
    // Track the total operations performed
    std::atomic<int> totalOperations{0};
    
    // Start multiple reader threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&weak, &shouldContinue, &totalOperations, i]() {
            std::cout << "Thread " << i << " started" << std::endl;
            int localCount = 0;
            
            while (shouldContinue.load()) {
                // Try to access the object
                weak.withLock([i, &localCount](TestObject& obj) {
                    localCount++;
                    std::cout << "Thread " << i << " accessing object #" 
                              << obj.getId() << ", local count: " << localCount << std::endl;
                    
                    // Simulate some work
                    std::this_thread::sleep_for(50ms);
                });
                
                // Small delay between attempts
                std::this_thread::sleep_for(20ms);
            }
            
            // Update the total count
            totalOperations.fetch_add(localCount);
            std::cout << "Thread " << i << " finished, local operations: " << localCount << std::endl;
        });
    }
    
    // Let the threads run for a while
    std::this_thread::sleep_for(500ms);
    
    printSubSection("Object Expiration During Thread Execution");
    // Reset the shared pointer while threads are running
    std::cout << "Resetting shared pointer while threads are accessing it..." << std::endl;
    shared.reset();
    
    // Let the threads continue for a bit after expiration
    std::this_thread::sleep_for(300ms);
    
    // Signal threads to stop
    std::cout << "Signaling threads to stop..." << std::endl;
    shouldContinue.store(false);
    
    // Wait for all threads to complete
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    std::cout << "All threads completed. Total operations: " << totalOperations.load() << std::endl;
    std::cout << "Lock attempts recorded: " << weak.getLockAttempts() << std::endl;
    
    printSubSection("Coordination with Condition Variables");
    // Create a new shared pointer
    shared = std::make_shared<TestObject>(201, "CV-Test");
    EnhancedWeakPtr<TestObject> cvWeak(shared);
    
    // Create a waiter thread
    std::thread waiterThread([&cvWeak]() {
        std::cout << "Waiter thread waiting for object to become available..." << std::endl;
        bool success = cvWeak.waitFor(2s);
        std::cout << "Waiter thread done. Object available: " << (success ? "Yes" : "No") << std::endl;
    });
    
    // Create a notifier thread
    std::thread notifierThread([&cvWeak]() {
        std::cout << "Notifier thread sleeping before notification..." << std::endl;
        std::this_thread::sleep_for(500ms);
        std::cout << "Notifier thread sending notification..." << std::endl;
        cvWeak.notifyAll();
    });
    
    // Wait for threads to complete
    if (waiterThread.joinable()) waiterThread.join();
    if (notifierThread.joinable()) notifierThread.join();
}

// Example 8: Error Handling and Edge Cases
void errorHandlingExample() {
    printSection("Error Handling and Edge Cases");
    
    printSubSection("Construction and Assignment");
    // Default construction
    EnhancedWeakPtr<TestObject> defaultWeak;
    std::cout << "Default constructed weak ptr expired: " << (defaultWeak.expired() ? "Yes" : "No") << std::endl;
    
    // Construction from nullptr or empty shared_ptr
    std::shared_ptr<TestObject> nullShared;
    EnhancedWeakPtr<TestObject> nullWeak(nullShared);
    std::cout << "Null constructed weak ptr expired: " << (nullWeak.expired() ? "Yes" : "No") << std::endl;
    
    // Copy construction
    EnhancedWeakPtr<TestObject> copyWeak = nullWeak;
    std::cout << "Copy constructed weak ptr expired: " << (copyWeak.expired() ? "Yes" : "No") << std::endl;
    
    // Move construction
    EnhancedWeakPtr<TestObject> moveWeak = std::move(copyWeak);
    std::cout << "Move constructed weak ptr expired: " << (moveWeak.expired() ? "Yes" : "No") << std::endl;
    
    printSubSection("Edge Cases in Locking");
    // Create temporary object then let it expire
    EnhancedWeakPtr<TestObject> tempWeak;
    {
        auto tempShared = std::make_shared<TestObject>(300, "Temporary");
        tempWeak = EnhancedWeakPtr<TestObject>(tempShared);
        std::cout << "Temporary weak ptr expired (inside scope): " << (tempWeak.expired() ? "Yes" : "No") << std::endl;
    }
    std::cout << "Temporary weak ptr expired (outside scope): " << (tempWeak.expired() ? "Yes" : "No") << std::endl;
    
    // Try to lock expired pointer
    auto locked = tempWeak.lock();
    std::cout << "Lock result on expired pointer: " << (locked ? "Succeeded (unexpected)" : "Failed (expected)") << std::endl;
    
    // Try to use withLock on expired pointer
    bool success = tempWeak.withLock([](TestObject& obj) {
        std::cout << "This should not print" << std::endl;
    });
    std::cout << "withLock on expired pointer: " << (success ? "Succeeded (unexpected)" : "Failed (expected)") << std::endl;
    
    printSubSection("Validation in Boost Mode");
#ifdef ATOM_USE_BOOST
    // Create a valid pointer
    auto validShared = std::make_shared<TestObject>(301, "Valid");
    EnhancedWeakPtr<TestObject> validWeak(validShared);
    
    try {
        std::cout << "Validating valid pointer..." << std::endl;
        validWeak.validate();
        std::cout << "Validation successful" << std::endl;
    } catch (const EnhancedWeakPtrException& e) {
        std::cout << "Unexpected exception: " << e.what() << std::endl;
    }
    
    // Make it expire
    validShared.reset();
    
    try {
        std::cout << "Validating expired pointer..." << std::endl;
        validWeak.validate();
        std::cout << "Validation unexpectedly passed" << std::endl;
    } catch (const EnhancedWeakPtrException& e) {
        std::cout << "Expected exception caught: " << e.what() << std::endl;
    }
#else
    std::cout << "Boost is not enabled, validation functionality not available" << std::endl;
#endif

    printSubSection("Race Conditions and Thread Safety");
    // Create an object that will be contested
    auto contestedShared = std::make_shared<TestObject>(302, "Contested");
    EnhancedWeakPtr<TestObject> contestedWeak(contestedShared);
    
    // Create multiple threads that will race to access and possibly reset
    std::vector<std::thread> racingThreads;
    std::atomic<int> successfulAccesses{0};
    std::atomic<int> failedAccesses{0};
    
    // This will be set by one of the threads
    std::atomic<bool> hasReset{false};
    
    for (int i = 0; i < 10; ++i) {
        racingThreads.emplace_back([&contestedWeak, &successfulAccesses, &failedAccesses, &hasReset, i]() {
            // Random delay to increase chance of race conditions
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 50));
            
            // Thread 5 will reset the pointer
            if (i == 5 && !hasReset.load()) {
                hasReset.store(true);
                std::cout << "Thread " << i << " resetting weak pointer" << std::endl;
                contestedWeak.reset();
            }
            
            // All threads try to access
            bool success = contestedWeak.withLock([i](TestObject& obj) {
                std::cout << "Thread " << i << " successfully accessed object #" << obj.getId() << std::endl;
            });
            
            if (success) {
                successfulAccesses++;
            } else {
                failedAccesses++;
                std::cout << "Thread " << i << " failed to access object" << std::endl;
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& t : racingThreads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    std::cout << "Race condition test completed." << std::endl;
    std::cout << "Successful accesses: " << successfulAccesses.load() << std::endl;
    std::cout << "Failed accesses: " << failedAccesses.load() << std::endl;
}

int main() {
    std::cout << "===============================================" << std::endl;
    std::cout << "   EnhancedWeakPtr Comprehensive Examples      " << std::endl;
    std::cout << "===============================================" << std::endl;
    
    // Run all examples
    try {
        basicUsageExample();
        advancedLockingExample();
        asynchronousOperationsExample();
        typeCastingExample();
        voidSpecializationExample();
        groupOperationsExample();
        multiThreadingExample();
        errorHandlingExample();
        
        std::cout << "\nAll examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "An unexpected error occurred: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}