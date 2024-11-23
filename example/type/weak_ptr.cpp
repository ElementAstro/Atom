#include "atom/type/weak_ptr.hpp"

#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using namespace atom::type;

int main() {
    // Create a shared pointer
    auto sharedPtr = std::make_shared<int>(42);

    // Create an EnhancedWeakPtr from the shared pointer
    EnhancedWeakPtr<int> weakPtr(sharedPtr);

    // Lock the weak pointer and get a shared pointer
    if (auto lockedPtr = weakPtr.lock()) {
        std::cout << "Locked value: " << *lockedPtr << std::endl;
    } else {
        std::cout << "Failed to lock weak pointer" << std::endl;
    }

    // Check if the managed object has expired
    bool isExpired = weakPtr.expired();
    std::cout << "Is expired: " << std::boolalpha << isExpired << std::endl;

    // Reset the weak pointer
    weakPtr.reset();
    isExpired = weakPtr.expired();
    std::cout << "Is expired after reset: " << std::boolalpha << isExpired
              << std::endl;

    // Create a new shared pointer and EnhancedWeakPtr
    sharedPtr = std::make_shared<int>(100);
    weakPtr = EnhancedWeakPtr<int>(sharedPtr);

    // Execute a function with a locked shared pointer
    weakPtr.withLock([](int& value) {
        std::cout << "Value inside withLock: " << value << std::endl;
    });

    // Wait for the managed object to become available or for a timeout
    bool available = weakPtr.waitFor(std::chrono::seconds(1));
    std::cout << "Object available: " << std::boolalpha << available
              << std::endl;

    // Get the use count of the managed object
    long useCount = weakPtr.useCount();
    std::cout << "Use count: " << useCount << std::endl;

    // Get the total number of EnhancedWeakPtr instances
    size_t totalInstances = EnhancedWeakPtr<int>::getTotalInstances();
    std::cout << "Total instances: " << totalInstances << std::endl;

    // Try to lock the weak pointer and execute one of two functions based on
    // success or failure
    weakPtr.tryLockOrElse(
        [](int& value) { std::cout << "Success: " << value << std::endl; },
        []() { std::cout << "Failure: object expired" << std::endl; });

    // Try to lock the weak pointer periodically until success or a maximum
    // number of attempts
    auto lockedPtr = weakPtr.tryLockPeriodic(std::chrono::milliseconds(100), 5);
    if (lockedPtr) {
        std::cout << "Locked value after periodic attempts: " << *lockedPtr
                  << std::endl;
    } else {
        std::cout << "Failed to lock weak pointer after periodic attempts"
                  << std::endl;
    }

    // Notify all waiting threads
    weakPtr.notifyAll();

    // Get the number of lock attempts
    size_t lockAttempts = weakPtr.getLockAttempts();
    std::cout << "Lock attempts: " << lockAttempts << std::endl;

    // Asynchronously lock the weak pointer
    auto future = weakPtr.asyncLock();
    lockedPtr = future.get();
    if (lockedPtr) {
        std::cout << "Locked value from asyncLock: " << *lockedPtr << std::endl;
    } else {
        std::cout << "Failed to lock weak pointer from asyncLock" << std::endl;
    }

    // Wait until a predicate is satisfied or the managed object expires
    bool predicateSatisfied = weakPtr.waitUntil([]() { return true; });
    std::cout << "Predicate satisfied: " << std::boolalpha << predicateSatisfied
              << std::endl;

    // Cast the weak pointer to a different type
    EnhancedWeakPtr<void> voidWeakPtr = weakPtr.cast<void>();
    if (auto voidLockedPtr = voidWeakPtr.lock()) {
        std::cout << "Successfully casted and locked weak pointer" << std::endl;
    } else {
        std::cout << "Failed to cast and lock weak pointer" << std::endl;
    }

    // Create a group of EnhancedWeakPtr from a vector of shared pointers
    std::vector<std::shared_ptr<int>> sharedPtrs = {std::make_shared<int>(1),
                                                    std::make_shared<int>(2)};
    auto weakPtrs = createWeakPtrGroup(sharedPtrs);

    // Perform a batch operation on a vector of EnhancedWeakPtr
    batchOperation(weakPtrs, [](int& value) {
        std::cout << "Batch operation value: " << value << std::endl;
    });

    return 0;
}