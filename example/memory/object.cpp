#include "atom/memory/object.hpp"

#include <iostream>

using namespace atom::memory;

// Define a simple Resettable type
class MyObject {
public:
    void reset() {
        // Reset the object state
        data = 0;
    }

    void setData(int value) { data = value; }

    int getData() const { return data; }

private:
    int data = 0;
};

int main() {
    // Create an ObjectPool for MyObject with a maximum size of 5 and prefill
    // with 2 objects
    ObjectPool<MyObject> pool(5, 2);

    // Acquire an object from the pool
    auto obj1 = pool.acquire();
    obj1->setData(42);
    std::cout << "Acquired object with data: " << obj1->getData() << std::endl;

    // Acquire another object from the pool
    auto obj2 = pool.acquire();
    obj2->setData(84);
    std::cout << "Acquired another object with data: " << obj2->getData()
              << std::endl;

    // Release the first object back to the pool
    obj1.reset();
    std::cout << "Released the first object back to the pool." << std::endl;

    // Try to acquire an object with a timeout
    auto obj3 = pool.tryAcquireFor(std::chrono::seconds(1));
    if (obj3) {
        std::cout << "Acquired object with data: " << (*obj3)->getData()
                  << std::endl;
    } else {
        std::cout << "Failed to acquire object within the timeout."
                  << std::endl;
    }

    // Get the number of available objects in the pool
    size_t available = pool.available();
    std::cout << "Number of available objects: " << available << std::endl;

    // Get the current size of the pool
    size_t size = pool.size();
    std::cout << "Current pool size: " << size << std::endl;

    // Clear all objects from the pool
    pool.clear();
    std::cout << "Cleared all objects from the pool." << std::endl;

    // Resize the pool to a new maximum size
    pool.resize(10);
    std::cout << "Resized the pool to a new maximum size of 10." << std::endl;

    // Prefill the pool with 3 objects
    pool.prefill(3);
    std::cout << "Prefilled the pool with 3 objects." << std::endl;

    // Apply a function to all objects in the pool
    pool.applyToAll([](MyObject& obj) { obj.setData(100); });
    std::cout << "Applied function to all objects in the pool." << std::endl;

    // Get the current number of in-use objects
    size_t inUseCount = pool.inUseCount();
    std::cout << "Number of in-use objects: " << inUseCount << std::endl;

    return 0;
}