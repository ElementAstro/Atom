#include "atom/type/no_offset_ptr.hpp"

#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

// Sample class to demonstrate UnshiftedPtr usage
class Resource {
private:
    std::string name_;
    int value_;
    bool* destroyed_flag_ = nullptr;

public:
    // Default constructor
    Resource() : name_("DefaultResource"), value_(0) {
        std::cout << "Resource default constructed: " << name_ << std::endl;
    }

    // Parameterized constructor
    Resource(std::string name, int value, bool* destroyed_flag = nullptr)
        : name_(std::move(name)),
          value_(value),
          destroyed_flag_(destroyed_flag) {
        std::cout << "Resource constructed: " << name_ << ", value: " << value_
                  << std::endl;
    }

    // Copy constructor
    Resource(const Resource& other)
        : name_(other.name_ + " (copy)"),
          value_(other.value_),
          destroyed_flag_(other.destroyed_flag_) {
        std::cout << "Resource copied: " << name_ << std::endl;
    }

    // Move constructor
    Resource(Resource&& other) noexcept
        : name_(std::move(other.name_)),
          value_(other.value_),
          destroyed_flag_(other.destroyed_flag_) {
        other.destroyed_flag_ = nullptr;
        std::cout << "Resource moved: " << name_ << std::endl;
    }

    // Destructor
    ~Resource() {
        std::cout << "Resource destroyed: " << name_ << std::endl;
        if (destroyed_flag_) {
            *destroyed_flag_ = true;
        }
    }

    // Getter and setters
    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    // Method to update resource
    void update(int delta) {
        value_ += delta;
        std::cout << "Resource updated: " << name_ << ", new value: " << value_
                  << std::endl;
    }
};

// Example 1: Basic usage
void basic_usage_example() {
    std::cout << "\n=== Example 1: Basic Usage ===\n";

    // Create UnshiftedPtr with default constructor
    atom::UnshiftedPtr<Resource> default_resource;
    std::cout << "Default resource name: " << default_resource->getName()
              << std::endl;
    std::cout << "Default resource value: " << default_resource->getValue()
              << std::endl;

    // Create UnshiftedPtr with custom parameters
    atom::UnshiftedPtr<Resource> custom_resource("CustomResource", 42);
    std::cout << "Custom resource name: " << custom_resource->getName()
              << std::endl;
    std::cout << "Custom resource value: " << custom_resource->getValue()
              << std::endl;

    // Access and modify the resource using operator->
    custom_resource->setValue(100);
    std::cout << "Updated value: " << custom_resource->getValue() << std::endl;

    // Access using dereference operator
    (*custom_resource).update(50);
    std::cout << "Value after update: " << custom_resource->getValue()
              << std::endl;

    // Check if the pointer has a value
    std::cout << "Has value: " << (custom_resource.has_value() ? "yes" : "no")
              << std::endl;

    // Using boolean conversion
    if (custom_resource) {
        std::cout << "Custom resource exists" << std::endl;
    }

    // Example with a primitive type
    atom::UnshiftedPtr<int> int_ptr(123);
    std::cout << "Int value: " << *int_ptr << std::endl;
    *int_ptr += 77;
    std::cout << "Updated int value: " << *int_ptr << std::endl;

    // The resources will be automatically destroyed when they go out of scope
    std::cout << "Exiting basic usage example..." << std::endl;
}

// Example 2: Reset and Emplace
void reset_emplace_example() {
    std::cout << "\n=== Example 2: Reset and Emplace ===\n";

    atom::UnshiftedPtr<Resource> resource("InitialResource", 10);

    // Reset the resource with new parameters
    std::cout << "Resetting resource..." << std::endl;
    resource.reset("ResetResource", 20);
    std::cout << "After reset - Name: " << resource->getName()
              << ", Value: " << resource->getValue() << std::endl;

    // Emplace (equivalent to reset)
    std::cout << "Emplacing resource..." << std::endl;
    resource.emplace("EmplacedResource", 30);
    std::cout << "After emplace - Name: " << resource->getName()
              << ", Value: " << resource->getValue() << std::endl;

    // Reset with different number of parameters
    std::cout << "Resetting with default values..." << std::endl;
    resource.reset();
    std::cout << "After reset to default - Name: " << resource->getName()
              << ", Value: " << resource->getValue() << std::endl;

    // Apply a function if the resource exists
    resource.apply_if([](Resource& r) { r.update(100); });
}

// Example 3: Thread Safety with Mutex
void thread_safety_mutex_example() {
    std::cout << "\n=== Example 3: Thread Safety with Mutex ===\n";

    atom::ThreadSafeUnshiftedPtr<Resource> shared_resource("SharedResource", 0);

    // Create multiple threads that update the resource
    std::vector<std::thread> threads;
    const int num_threads = 5;
    const int updates_per_thread = 10;

    std::cout << "Starting " << num_threads << " threads with "
              << updates_per_thread << " updates each..." << std::endl;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&shared_resource, i, updates_per_thread]() {
            for (int j = 0; j < updates_per_thread; ++j) {
                // Apply thread-safe updates
                shared_resource.apply_if([i, j](Resource& r) {
                    r.update(1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(
                        5));  // Small delay to increase thread interleaving
                });
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "All threads completed" << std::endl;
    std::cout << "Final resource value: " << shared_resource->getValue()
              << std::endl;
    std::cout << "Expected value: " << (num_threads * updates_per_thread)
              << std::endl;
}

// Example 4: Lock-Free Atomic Operations
void lock_free_atomic_example() {
    std::cout << "\n=== Example 4: Lock-Free Atomic Operations ===\n";

    atom::LockFreeUnshiftedPtr<int> atomic_counter(0);

    // Create multiple threads that increment the counter
    std::vector<std::thread> threads;
    const int num_threads = 10;
    const int increments_per_thread = 1000;

    std::cout << "Starting " << num_threads << " threads with "
              << increments_per_thread
              << " increments each using atomic operations..." << std::endl;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&atomic_counter, increments_per_thread]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                atomic_counter.apply_if([](int& value) { ++value; });
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "All threads completed" << std::endl;
    std::cout << "Final counter value: " << *atomic_counter << std::endl;
    std::cout << "Expected value: " << (num_threads * increments_per_thread)
              << std::endl;
}

// Example 5: Move Semantics
void move_semantics_example() {
    std::cout << "\n=== Example 5: Move Semantics ===\n";

    atom::UnshiftedPtr<Resource> original("OriginalResource", 100);
    std::cout << "Original resource created" << std::endl;

    // Move construction
    atom::UnshiftedPtr<Resource> moved(std::move(original));
    std::cout << "After move construction:" << std::endl;
    std::cout << "Moved resource name: " << moved->getName() << std::endl;

    // Check if original still has a value (it shouldn't)
    std::cout << "Original has value: " << (original.has_value() ? "yes" : "no")
              << std::endl;

    // Create another resource to demonstrate move assignment
    atom::UnshiftedPtr<Resource> another("AnotherResource", 200);
    std::cout << "Another resource created" << std::endl;

    // Move assignment
    another = std::move(moved);
    std::cout << "After move assignment:" << std::endl;
    std::cout << "Another resource name: " << another->getName() << std::endl;

    // Check if moved still has a value (it shouldn't)
    std::cout << "Moved has value: " << (moved.has_value() ? "yes" : "no")
              << std::endl;
}

// Example 6: Error Handling
void error_handling_example() {
    std::cout << "\n=== Example 6: Error Handling ===\n";

    // Create a resource
    atom::UnshiftedPtr<Resource> resource("ErrorResource", 50);

    // Release ownership without destroying
    Resource* raw_ptr = resource.release();
    std::cout << "Resource released, raw pointer: " << raw_ptr << std::endl;
    std::cout << "UnshiftedPtr has value: "
              << (resource.has_value() ? "yes" : "no") << std::endl;

    // Accessing after release should throw
    try {
        std::cout << "Attempting to access released resource..." << std::endl;
        resource->getValue();  // This should throw
        std::cout << "This line shouldn't be reached" << std::endl;
    } catch (const atom::unshifted_ptr_error& e) {
        std::cout << "Caught expected exception: " << e.what() << std::endl;
    }

    // We need to manually destroy the released resource
    std::cout << "Manually destroying the released resource..." << std::endl;
    delete raw_ptr;

    // Creating a new resource
    atom::UnshiftedPtr<Resource> safe_resource("SafeResource", 60);

    // Using get_safe() to avoid exceptions
    if (Resource* ptr = safe_resource.get_safe()) {
        std::cout << "Safe access succeeded: " << ptr->getName() << std::endl;
    } else {
        std::cout << "Safe access failed (shouldn't happen here)" << std::endl;
    }

    // Release and check again
    safe_resource.release();
    if (Resource* ptr = safe_resource.get_safe()) {
        std::cout << "Safe access succeeded after release (shouldn't happen)"
                  << std::endl;
    } else {
        std::cout << "Safe access correctly returned nullptr after release"
                  << std::endl;
    }
}

// Example 7: Lifetime Monitoring
void lifetime_monitoring_example() {
    std::cout << "\n=== Example 7: Lifetime Monitoring ===\n";

    bool resource_destroyed = false;

    // Create a nested scope
    {
        std::cout << "Entering nested scope..." << std::endl;
        atom::UnshiftedPtr<Resource> monitored_resource("MonitoredResource", 75,
                                                        &resource_destroyed);

        std::cout << "Resource initialized, monitoring destruction..."
                  << std::endl;
        std::cout << "Resource destroyed: "
                  << (resource_destroyed ? "yes" : "no") << std::endl;

        // End of scope will trigger destruction
        std::cout << "Exiting nested scope..." << std::endl;
    }

    // Check if resource was destroyed
    std::cout << "After scope exit, resource destroyed: "
              << (resource_destroyed ? "yes" : "no") << std::endl;

    // Test reset destroying previous object
    resource_destroyed = false;
    atom::UnshiftedPtr<Resource> resource_to_reset("ResourceToReset", 80,
                                                   &resource_destroyed);
    std::cout << "Created resource to reset" << std::endl;
    std::cout << "Resource destroyed before reset: "
              << (resource_destroyed ? "yes" : "no") << std::endl;

    // Reset should destroy previous object
    resource_to_reset.reset("ResetResource", 90);
    std::cout << "After reset, original resource destroyed: "
              << (resource_destroyed ? "yes" : "no") << std::endl;
}

// Example 8: Complex Types
void complex_types_example() {
    std::cout << "\n=== Example 8: Complex Types ===\n";

    // UnshiftedPtr with a vector
    atom::UnshiftedPtr<std::vector<int>> vector_ptr;

    // Use the vector
    vector_ptr->push_back(10);
    vector_ptr->push_back(20);
    vector_ptr->push_back(30);

    std::cout << "Vector contents: ";
    for (int value : *vector_ptr) {
        std::cout << value << " ";
    }
    std::cout << std::endl;

    // UnshiftedPtr with a map
    atom::UnshiftedPtr<std::map<std::string, int>> map_ptr;

    // Use the map
    (*map_ptr)["one"] = 1;
    (*map_ptr)["two"] = 2;
    (*map_ptr)["three"] = 3;

    std::cout << "Map contents:" << std::endl;
    for (const auto& [key, value] : *map_ptr) {
        std::cout << key << ": " << value << std::endl;
    }

    // Demonstrate reset with complex type
    std::cout << "Resetting vector..." << std::endl;
    vector_ptr.reset(std::vector<int>{100, 200, 300, 400});

    std::cout << "Vector contents after reset: ";
    for (int value : *vector_ptr) {
        std::cout << value << " ";
    }
    std::cout << std::endl;
}

// Example 9: UnshiftedPtr with Primitives
void primitive_types_example() {
    std::cout << "\n=== Example 9: UnshiftedPtr with Primitives ===\n";

    // Integer
    atom::UnshiftedPtr<int> int_ptr(42);
    std::cout << "Integer value: " << *int_ptr << std::endl;
    *int_ptr = 100;
    std::cout << "Updated integer value: " << *int_ptr << std::endl;

    // Double
    atom::UnshiftedPtr<double> double_ptr(3.14159);
    std::cout << "Double value: " << *double_ptr << std::endl;
    *double_ptr *= 2;
    std::cout << "Doubled value: " << *double_ptr << std::endl;

    // Boolean
    atom::UnshiftedPtr<bool> bool_ptr(true);
    std::cout << "Boolean value: " << (*bool_ptr ? "true" : "false")
              << std::endl;
    *bool_ptr = !(*bool_ptr);
    std::cout << "Toggled boolean value: " << (*bool_ptr ? "true" : "false")
              << std::endl;

    // Character
    atom::UnshiftedPtr<char> char_ptr('A');
    std::cout << "Character value: " << *char_ptr << std::endl;
    *char_ptr = 'Z';
    std::cout << "Updated character value: " << *char_ptr << std::endl;
}

// Example 10: Compare different thread safety policies
void thread_safety_policy_comparison() {
    std::cout << "\n=== Example 10: Thread Safety Policy Comparison ===\n";

    // Test performance differences between thread safety policies
    const int iterations = 1000000;

    auto measure_time = [](const std::string& name, auto func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << name << " took " << duration.count() << " microseconds"
                  << std::endl;
    };

    // Non-thread safe version
    measure_time("No thread safety", [iterations]() {
        atom::UnshiftedPtr<int> counter(0);
        for (int i = 0; i < iterations; ++i) {
            counter.apply_if([](int& value) { ++value; });
        }
        std::cout << "  Final value: " << *counter << std::endl;
    });

    // Mutex-based thread safety
    measure_time("Mutex thread safety", [iterations]() {
        atom::ThreadSafeUnshiftedPtr<int> counter(0);
        for (int i = 0; i < iterations; ++i) {
            counter.apply_if([](int& value) { ++value; });
        }
        std::cout << "  Final value: " << *counter << std::endl;
    });

    // Atomic thread safety
    measure_time("Atomic thread safety", [iterations]() {
        atom::LockFreeUnshiftedPtr<int> counter(0);
        for (int i = 0; i < iterations; ++i) {
            counter.apply_if([](int& value) { ++value; });
        }
        std::cout << "  Final value: " << *counter << std::endl;
    });
}

int main() {
    std::cout << "===== UnshiftedPtr Usage Examples =====\n";

    // Run all examples
    basic_usage_example();
    reset_emplace_example();
    thread_safety_mutex_example();
    lock_free_atomic_example();
    move_semantics_example();
    error_handling_example();
    lifetime_monitoring_example();
    complex_types_example();
    primitive_types_example();
    thread_safety_policy_comparison();

    std::cout << "\nAll examples completed successfully!\n";
    return 0;
}
