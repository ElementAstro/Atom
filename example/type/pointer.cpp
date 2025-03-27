#include "../atom/type/pointer.hpp"
#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Sample class to demonstrate PointerSentinel functionality
class Person {
private:
    std::string name_;
    int age_;
    bool active_;

public:
    Person(std::string name, int age)
        : name_(std::move(name)), age_(age), active_(true) {
        std::cout << "Person constructed: " << name_ << ", age " << age_
                  << std::endl;
    }

    ~Person() { std::cout << "Person destroyed: " << name_ << std::endl; }

    void setName(const std::string& name) { name_ = name; }
    void setAge(int age) { age_ = age; }
    void setActive(bool active) { active_ = active; }

    std::string getName() const { return name_; }
    int getAge() const { return age_; }
    bool isActive() const { return active_; }

    void celebrate() {
        age_++;
        std::cout << name_ << " is now " << age_ << " years old!" << std::endl;
    }

    std::string toString() const {
        return name_ + " (age: " + std::to_string(age_) + ", " +
               (active_ ? "active" : "inactive") + ")";
    }
};

// Base class for inheritance demonstration
class Entity {
protected:
    int id_;
    std::string type_;

public:
    Entity(int id, std::string type) : id_(id), type_(std::move(type)) {
        std::cout << "Entity constructed: ID=" << id_ << ", Type=" << type_
                  << std::endl;
    }

    virtual ~Entity() {
        std::cout << "Entity destroyed: ID=" << id_ << std::endl;
    }

    int getId() const { return id_; }
    std::string getType() const { return type_; }
    virtual std::string describe() const {
        return "Entity " + std::to_string(id_) + " of type " + type_;
    }
};

// Derived class for inheritance demonstration
class Player : public Entity {
private:
    std::string name_;
    int score_;

public:
    Player(int id, std::string name, int score)
        : Entity(id, "Player"), name_(std::move(name)), score_(score) {
        std::cout << "Player constructed: " << name_ << " with score " << score_
                  << std::endl;
    }

    ~Player() override {
        std::cout << "Player destroyed: " << name_ << std::endl;
    }

    void addScore(int points) {
        score_ += points;
        std::cout << name_ << "'s score increased to " << score_ << std::endl;
    }

    std::string getName() const { return name_; }
    int getScore() const { return score_; }

    std::string describe() const override {
        return "Player " + name_ + " (ID:" + std::to_string(getId()) +
               ") with score " + std::to_string(score_);
    }
};

// Function to perform SIMD-like operations on an array
void processArraySIMD(int* data, size_t size) {
    // Simulate SIMD processing by operating on chunks of data
    std::cout << "Processing array with SIMD-like operations..." << std::endl;

    // Process in chunks of 4 (simulating SIMD)
    for (size_t i = 0; i < size; i += 4) {
        size_t chunk_size = std::min(size_t(4), size - i);
        std::cout << "  Processing elements " << i << " to "
                  << (i + chunk_size - 1) << ": ";

        // Apply operation to each element in chunk
        for (size_t j = 0; j < chunk_size; j++) {
            data[i + j] *= 2;  // Double each value
            std::cout << data[i + j] << " ";
        }
        std::cout << std::endl;
    }
}

// Example 1: Basic construction and access
void basicConstructionExample() {
    std::cout << "\n=== Example 1: Basic Construction and Access ==="
              << std::endl;

    // Create various pointer types
    auto shared_person = std::make_shared<Person>("Alice", 30);
    auto unique_person = std::make_unique<Person>("Bob", 25);
    Person* raw_person = new Person("Charlie", 40);
    auto shared_person2 = std::make_shared<Person>("David", 35);
    std::weak_ptr<Person> weak_person = shared_person2;

    std::cout << "\nCreating PointerSentinel instances:" << std::endl;

    // Create PointerSentinel objects
    PointerSentinel<Person> sentinel1(shared_person);
    PointerSentinel<Person> sentinel2(std::move(unique_person));
    PointerSentinel<Person> sentinel3(raw_person);
    PointerSentinel<Person> sentinel4(weak_person);

    std::cout << "\nAccessing pointer values:" << std::endl;

    // Access the pointers
    std::cout << "sentinel1 points to: " << sentinel1.get()->getName()
              << std::endl;
    std::cout << "sentinel2 points to: " << sentinel2.get()->getName()
              << std::endl;
    std::cout << "sentinel3 points to: " << sentinel3.get()->getName()
              << std::endl;
    std::cout << "sentinel4 points to: " << sentinel4.get()->getName()
              << std::endl;

    // Check validity
    std::cout << "\nChecking validity:" << std::endl;
    std::cout << "sentinel1 is valid: " << (sentinel1.is_valid() ? "yes" : "no")
              << std::endl;
    std::cout << "sentinel2 is valid: " << (sentinel2.is_valid() ? "yes" : "no")
              << std::endl;

    // Using get_noexcept
    std::cout << "\nUsing get_noexcept:" << std::endl;
    Person* p1 = sentinel1.get_noexcept();
    if (p1) {
        std::cout << "p1 points to: " << p1->getName() << std::endl;
    }

    // Try to create an invalid sentinel with null pointer
    std::cout << "\nTrying to create invalid sentinels:" << std::endl;
    try {
        std::shared_ptr<Person> null_ptr;
        PointerSentinel<Person> invalid_sentinel(null_ptr);
    } catch (const PointerException& e) {
        std::cout << "Expected exception: " << e.what() << std::endl;
    }

    // Clear shared_person2 to make weak_ptr expire
    std::cout << "\nTesting weak_ptr expiration:" << std::endl;
    shared_person2.reset();
    std::cout << "Original weak_ptr expired: " << weak_person.expired()
              << std::endl;

    try {
        std::cout
            << "Trying to access through sentinel4 after weak_ptr expiration..."
            << std::endl;
        sentinel4.get();
    } catch (const PointerException& e) {
        std::cout << "Expected exception: " << e.what() << std::endl;
    }
}

// Example 2: Copy and Move Semantics
void copyMoveExample() {
    std::cout << "\n=== Example 2: Copy and Move Semantics ===" << std::endl;

    // Create original pointers and sentinels
    auto shared_person = std::make_shared<Person>("Eve", 28);
    auto unique_person = std::make_unique<Person>("Frank", 32);
    Person* raw_person = new Person("Grace", 45);

    PointerSentinel<Person> original1(shared_person);
    PointerSentinel<Person> original2(std::move(unique_person));
    PointerSentinel<Person> original3(raw_person);

    std::cout << "\nTesting copy construction:" << std::endl;
    PointerSentinel<Person> copy1(original1);
    PointerSentinel<Person> copy3(original3);

    std::cout << "Original1 points to: " << original1.get()->getName()
              << std::endl;
    std::cout << "Copy1 points to: " << copy1.get()->getName() << std::endl;

    std::cout << "Original3 points to: " << original3.get()->getName()
              << std::endl;
    std::cout << "Copy3 points to: " << copy3.get()->getName() << std::endl;

    // Modify through copy to show they're separate objects
    copy1.get()->setName("Eve (modified through copy)");
    copy3.get()->setName("Grace (modified through copy)");

    std::cout << "\nAfter modification through copies:" << std::endl;
    std::cout << "Original1 now points to: " << original1.get()->getName()
              << std::endl;
    std::cout << "Copy1 now points to: " << copy1.get()->getName() << std::endl;
    std::cout << "Original3 now points to: " << original3.get()->getName()
              << std::endl;
    std::cout << "Copy3 now points to: " << copy3.get()->getName() << std::endl;

    std::cout << "\nTesting move construction:" << std::endl;
    PointerSentinel<Person> moved2(std::move(original2));
    std::cout << "Original2 is valid after move: "
              << (original2.is_valid() ? "yes" : "no") << std::endl;
    std::cout << "Moved2 is valid: " << (moved2.is_valid() ? "yes" : "no")
              << std::endl;
    std::cout << "Moved2 points to: " << moved2.get()->getName() << std::endl;

    std::cout << "\nTesting copy assignment:" << std::endl;
    PointerSentinel<Person> assigned;
    assigned = copy1;
    std::cout << "Assigned is valid: " << (assigned.is_valid() ? "yes" : "no")
              << std::endl;
    std::cout << "Assigned points to: " << assigned.get()->getName()
              << std::endl;

    std::cout << "\nTesting move assignment:" << std::endl;
    PointerSentinel<Person> moved_assigned;
    moved_assigned = std::move(moved2);
    std::cout << "Moved2 is valid after move assignment: "
              << (moved2.is_valid() ? "yes" : "no") << std::endl;
    std::cout << "Moved_assigned is valid: "
              << (moved_assigned.is_valid() ? "yes" : "no") << std::endl;
    std::cout << "Moved_assigned points to: " << moved_assigned.get()->getName()
              << std::endl;
}

// Example 3: Invoking Methods
void invokingMethodsExample() {
    std::cout << "\n=== Example 3: Invoking Methods ===" << std::endl;

    auto person = std::make_shared<Person>("Hannah", 29);
    PointerSentinel<Person> sentinel(person);

    std::cout << "\nInvoking methods directly:" << std::endl;

    // Invoke methods with varying return types
    std::string name = sentinel.invoke(&Person::getName);
    std::cout << "Name: " << name << std::endl;

    int age = sentinel.invoke(&Person::getAge);
    std::cout << "Age: " << age << std::endl;

    // Invoke method with parameters
    sentinel.invoke(&Person::setAge, 30);
    std::cout << "New age: " << sentinel.invoke(&Person::getAge) << std::endl;

    // Invoke void method
    sentinel.invoke(&Person::celebrate);
    std::cout << "Age after celebration: " << sentinel.invoke(&Person::getAge)
              << std::endl;

    std::cout << "\nUsing apply with lambda functions:" << std::endl;

    // Use apply with a lambda that returns a value
    std::string info = sentinel.apply([](Person* p) {
        return p->getName() + " is " + std::to_string(p->getAge()) +
               " years old";
    });
    std::cout << "Info: " << info << std::endl;

    // Use applyVoid with a lambda that modifies the object
    sentinel.applyVoid([](Person* p) {
        p->setName(p->getName() + " Smith");
        p->setActive(false);
    });

    std::cout << "After applyVoid:" << std::endl;
    std::cout << "Name: " << sentinel.invoke(&Person::getName) << std::endl;
    std::cout << "Active: "
              << (sentinel.invoke(&Person::isActive) ? "yes" : "no")
              << std::endl;

    // Try to invoke on an invalid pointer
    std::cout << "\nTesting error handling during invocation:" << std::endl;
    auto temp_person = std::make_shared<Person>("Temporary", 20);
    std::weak_ptr<Person> weak_temp = temp_person;
    PointerSentinel<Person> weak_sentinel(weak_temp);

    // Make the weak_ptr expire
    temp_person.reset();

    try {
        weak_sentinel.invoke(&Person::getName);
    } catch (const PointerException& e) {
        std::cout << "Expected exception: " << e.what() << std::endl;
    }
}

// Example 4: Type Conversion
void typeConversionExample() {
    std::cout << "\n=== Example 4: Type Conversion ===" << std::endl;

    // Create a Player instance (derived from Entity)
    auto player = std::make_shared<Player>(1, "Isaac", 100);
    PointerSentinel<Player> player_sentinel(player);

    std::cout << "\nOriginal player info:" << std::endl;
    std::cout << "Player: " << player_sentinel.invoke(&Player::describe)
              << std::endl;

    std::cout << "\nConverting Player pointer to Entity pointer:" << std::endl;
    PointerSentinel<Entity> entity_sentinel =
        player_sentinel.convert_to<Entity>();

    std::cout << "Entity: " << entity_sentinel.invoke(&Entity::describe)
              << std::endl;
    std::cout << "ID: " << entity_sentinel.invoke(&Entity::getId) << std::endl;
    std::cout << "Type: " << entity_sentinel.invoke(&Entity::getType)
              << std::endl;

    // Try invalid conversion
    std::cout << "\nTesting invalid conversion:" << std::endl;
    auto person = std::make_shared<Person>("Jack", 33);
    PointerSentinel<Person> person_sentinel(person);

    try {
        // This should fail at compile time due to static_assert
        // PointerSentinel<Entity> invalid =
        // person_sentinel.convert_to<Entity>();
        std::cout << "Conversion not attempted due to static_assert"
                  << std::endl;
    } catch (...) {
        std::cout << "Exception thrown during invalid conversion" << std::endl;
    }
}

// Example 5: Asynchronous Operations
void asyncOperationsExample() {
    std::cout << "\n=== Example 5: Asynchronous Operations ===" << std::endl;

    auto person = std::make_shared<Person>("Kelly", 26);
    PointerSentinel<Person> sentinel(person);

    std::cout << "\nStarting asynchronous operation..." << std::endl;

    // Apply an operation asynchronously that takes some time
    auto future = sentinel.apply_async([](Person* p) {
        std::cout << "Async task started for " << p->getName() << std::endl;

        // Simulate work
        for (int i = 0; i < 3; i++) {
            std::cout << "Async task working... (" << (i + 1) << "/3)"
                      << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Modify the person in the background
        p->celebrate();
        p->setName(p->getName() + " (processed asynchronously)");

        std::cout << "Async task completed" << std::endl;
        return "Processed " + p->getName() + " successfully";
    });

    std::cout << "Main thread continues execution while async task runs..."
              << std::endl;
    std::cout << "Doing other work in main thread..." << std::endl;

    // Wait for and retrieve the result
    std::cout << "\nWaiting for async result..." << std::endl;
    std::string result = future.get();
    std::cout << "Async result: " << result << std::endl;

    std::cout << "\nAfter async operation:" << std::endl;
    std::cout << "Name: " << sentinel.invoke(&Person::getName) << std::endl;
    std::cout << "Age: " << sentinel.invoke(&Person::getAge) << std::endl;
}

// Example 6: SIMD-Like Operations
void simdOperationsExample() {
    std::cout << "\n=== Example 6: SIMD-Like Operations ===" << std::endl;

    // Create an array of integers
    const size_t array_size = 10;
    int* data = new int[array_size];

    // Initialize the array
    for (size_t i = 0; i < array_size; i++) {
        data[i] = static_cast<int>(i + 1);
    }

    // Create a pointer sentinel for the array
    PointerSentinel<int> array_sentinel(data);

    std::cout << "\nInitial array values:" << std::endl;
    for (size_t i = 0; i < array_size; i++) {
        std::cout << data[i] << " ";
    }
    std::cout << std::endl;

    // Apply SIMD-like operations to the array
    array_sentinel.apply_simd(processArraySIMD, array_size);

    std::cout << "\nArray values after SIMD processing:" << std::endl;
    for (size_t i = 0; i < array_size; i++) {
        std::cout << data[i] << " ";
    }
    std::cout << std::endl;
}

// Example 7: Error Handling and Safety
void errorHandlingExample() {
    std::cout << "\n=== Example 7: Error Handling and Safety ===" << std::endl;

    // Test with null pointers
    std::cout << "\nTesting null pointer handling:" << std::endl;

    try {
        std::shared_ptr<Person> null_shared;
        PointerSentinel<Person> sentinel(null_shared);
    } catch (const PointerException& e) {
        std::cout << "Expected exception (shared_ptr): " << e.what()
                  << std::endl;
    }

    try {
        std::unique_ptr<Person> null_unique;
        PointerSentinel<Person> sentinel(std::move(null_unique));
    } catch (const PointerException& e) {
        std::cout << "Expected exception (unique_ptr): " << e.what()
                  << std::endl;
    }

    try {
        Person* null_raw = nullptr;
        PointerSentinel<Person> sentinel(null_raw);
    } catch (const PointerException& e) {
        std::cout << "Expected exception (raw pointer): " << e.what()
                  << std::endl;
    }

    // Test with expired weak_ptr
    std::cout << "\nTesting expired weak_ptr handling:" << std::endl;

    {
        auto temp_shared = std::make_shared<Person>("Temporary", 25);
        std::weak_ptr<Person> weak_temp = temp_shared;

        // Let the shared_ptr go out of scope
    }

    std::weak_ptr<Person> expired_weak;

    try {
        PointerSentinel<Person> sentinel(expired_weak);
    } catch (const PointerException& e) {
        std::cout << "Expected exception (expired weak_ptr): " << e.what()
                  << std::endl;
    }

    // Test thread safety
    std::cout << "\nTesting thread safety:" << std::endl;

    auto shared_person = std::make_shared<Person>("Liam", 30);
    PointerSentinel<Person> shared_sentinel(shared_person);

    // Create multiple threads that access the same sentinel
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; i++) {
        threads.push_back(std::thread([&shared_sentinel, i]() {
            try {
                std::this_thread::sleep_for(std::chrono::milliseconds(10 * i));
                std::string name = shared_sentinel.invoke(&Person::getName);
                std::cout << "Thread " << i << " read name: " << name
                          << std::endl;

                // Modify and read in the same thread
                shared_sentinel.invoke(&Person::setName,
                                       "Liam-" + std::to_string(i));
                std::string new_name = shared_sentinel.invoke(&Person::getName);
                std::cout << "Thread " << i << " updated name to: " << new_name
                          << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Thread " << i << " caught exception: " << e.what()
                          << std::endl;
            }
        }));
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "\nFinal name after thread operations: "
              << shared_sentinel.invoke(&Person::getName) << std::endl;
}

// Example 8: Working with Raw Pointers
void rawPointerExample() {
    std::cout << "\n=== Example 8: Working with Raw Pointers ===" << std::endl;

    // Create a raw pointer and manage it with PointerSentinel
    Person* raw_person = new Person("Martin", 42);

    {
        std::cout << "\nCreating PointerSentinel for raw pointer:" << std::endl;
        PointerSentinel<Person> sentinel(raw_person);

        std::cout << "Working with the sentinel..." << std::endl;
        sentinel.invoke(&Person::celebrate);
        std::cout << "Age after celebration: "
                  << sentinel.invoke(&Person::getAge) << std::endl;

        // The sentinel will clean up the raw pointer when it goes out of scope
        std::cout << "\nSentinel going out of scope now..." << std::endl;
    }

    std::cout << "The raw pointer was automatically deleted by the sentinel"
              << std::endl;

    // Demonstrate detachment prevention
    std::cout << "\nDemonstrating detachment prevention:" << std::endl;

    Person* detached_person = new Person("Nathan", 38);
    PointerSentinel<Person> sentinel1(detached_person);

    try {
        // This is not allowed as it would lead to a double delete
        Person* stolen_ptr = sentinel1.get();
        std::cout << "Got pointer: " << stolen_ptr->getName() << std::endl;
        std::cout << "Warning: The pointer is still managed by the sentinel!"
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }
}

// Example 9: Complex Scenarios
void complexScenariosExample() {
    std::cout << "\n=== Example 9: Complex Scenarios ===" << std::endl;

    // Chain of operations
    std::cout << "\nChaining operations:" << std::endl;

    auto person = std::make_shared<Person>("Olivia", 27);
    PointerSentinel<Person> sentinel(person);

    std::string result = sentinel.apply([](Person* p) {
        // First operation: increment age
        p->setAge(p->getAge() + 1);

        // Second operation: modify name based on age
        if (p->getAge() >= 28) {
            p->setName(p->getName() + " (Adult)");
        } else {
            p->setName(p->getName() + " (Young)");
        }

        // Third operation: calculate some value based on person state
        std::string status = p->isActive() ? "active" : "inactive";
        return p->getName() + " is " + std::to_string(p->getAge()) +
               " years old and " + status;
    });

    std::cout << "Result of chained operations: " << result << std::endl;

    // Creating a collection of PointerSentinels
    std::cout << "\nWorking with collections of PointerSentinels:" << std::endl;

    std::vector<PointerSentinel<Person>> people;

    // Add different types of pointers
    people.emplace_back(std::make_shared<Person>("Paul", 31));

    auto unique_person = std::make_unique<Person>("Quinn", 29);
    people.emplace_back(std::move(unique_person));

    people.emplace_back(new Person("Rachel", 33));

    // Operate on all people in the collection
    std::cout << "\nPeople in collection:" << std::endl;
    for (int i = 0; i < people.size(); i++) {
        std::string name = people[i].invoke(&Person::getName);
        int age = people[i].invoke(&Person::getAge);
        std::cout << i + 1 << ". " << name << ", age " << age << std::endl;

        // Make everyone celebrate
        people[i].invoke(&Person::celebrate);
    }

    std::cout << "\nUpdated ages after celebration:" << std::endl;
    for (int i = 0; i < people.size(); i++) {
        std::string name = people[i].invoke(&Person::getName);
        int age = people[i].invoke(&Person::getAge);
        std::cout << i + 1 << ". " << name << ", age " << age << std::endl;
    }
}

// Example 10: Performance and Memory Management
void performanceExample() {
    std::cout << "\n=== Example 10: Performance and Memory Management ==="
              << std::endl;

    const int NUM_ITERATIONS = 1000000;
    const int NUM_POINTERS = 5;

    std::cout << "\nAllocating " << NUM_POINTERS << " pointers..." << std::endl;

    // Create various pointers
    std::vector<PointerSentinel<int>> pointers;
    for (int i = 0; i < NUM_POINTERS; i++) {
        if (i % 3 == 0) {
            pointers.emplace_back(std::make_shared<int>(i));
        } else if (i % 3 == 1) {
            pointers.emplace_back(std::make_unique<int>(i));
        } else {
            pointers.emplace_back(new int(i));
        }
    }

    // Measure time to access pointers
    std::cout << "Measuring performance of " << NUM_ITERATIONS
              << " pointer accesses..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    int sum = 0;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        int index = i % NUM_POINTERS;
        int value = *(pointers[index].get_noexcept());
        sum += value;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Sum result: " << sum << std::endl;
    std::cout << "Time taken: " << duration.count() / 1000.0 << " ms"
              << std::endl;
    std::cout << "Average time per access: "
              << static_cast<double>(duration.count()) / NUM_ITERATIONS
              << " microseconds" << std::endl;

    // Memory usage demonstration
    std::cout << "\nDemonstrating memory management:" << std::endl;

    {
        std::cout << "Creating scope with local pointers..." << std::endl;

        // These pointers will be cleaned up when the scope ends
        PointerSentinel<Person> scope_ptr1(new Person("Sam", 35));
        PointerSentinel<Person> scope_ptr2(new Person("Taylor", 28));

        std::cout << "About to leave scope..." << std::endl;
    }
    std::cout << "Scope ended, pointers automatically cleaned up" << std::endl;
}

int main() {
    std::cout << "===== PointerSentinel<T> Usage Examples =====" << std::endl;

    try {
        // Run all examples
        basicConstructionExample();
        copyMoveExample();
        invokingMethodsExample();
        typeConversionExample();
        asyncOperationsExample();
        simdOperationsExample();
        errorHandlingExample();
        rawPointerExample();
        complexScenariosExample();
        performanceExample();

        std::cout << "\nAll examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error in examples: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}