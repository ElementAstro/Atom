#include "../atom/type/indestructible.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

// A simple class to demonstrate Indestructible functionality
class Resource {
private:
    std::string name_;
    int* data_;
    size_t size_;
    bool moved_ = false;

public:
    // Default constructor
    Resource() : name_("default"), data_(nullptr), size_(0) {
        std::cout << "Resource default constructor called\n";
    }

    // Constructor with name
    Resource(std::string name)
        : name_(std::move(name)), data_(nullptr), size_(0) {
        std::cout << "Resource constructor called for '" << name_ << "'\n";
    }

    // Constructor with name and size
    Resource(std::string name, size_t size)
        : name_(std::move(name)), size_(size) {
        data_ = new int[size_];
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = static_cast<int>(i);
        }
        std::cout << "Resource constructor with data allocation called for '"
                  << name_ << "'\n";
    }

    // Copy constructor
    Resource(const Resource& other)
        : name_(other.name_ + " (copy)"), size_(other.size_) {
        std::cout << "Resource copy constructor called for '" << name_ << "'\n";
        if (other.data_) {
            data_ = new int[size_];
            std::copy(other.data_, other.data_ + size_, data_);
        } else {
            data_ = nullptr;
        }
    }

    // Move constructor
    Resource(Resource&& other) noexcept
        : name_(std::move(other.name_)),
          data_(other.data_),
          size_(other.size_) {
        std::cout << "Resource move constructor called for '" << name_ << "'\n";
        other.data_ = nullptr;
        other.size_ = 0;
        other.moved_ = true;
    }

    // Copy assignment
    Resource& operator=(const Resource& other) {
        if (this != &other) {
            std::cout << "Resource copy assignment called for '" << name_
                      << "' <- '" << other.name_ << "'\n";
            delete[] data_;
            name_ = other.name_ + " (assigned)";
            size_ = other.size_;
            if (other.data_) {
                data_ = new int[size_];
                std::copy(other.data_, other.data_ + size_, data_);
            } else {
                data_ = nullptr;
            }
        }
        return *this;
    }

    // Move assignment
    Resource& operator=(Resource&& other) noexcept {
        if (this != &other) {
            std::cout << "Resource move assignment called for '" << name_
                      << "' <- '" << other.name_ << "'\n";
            delete[] data_;
            name_ = std::move(other.name_);
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.moved_ = true;
        }
        return *this;
    }

    // Destructor
    ~Resource() {
        std::cout << "Resource destructor called for '" << name_ << "' ["
                  << (moved_ ? "moved" : "valid") << "]\n";
        delete[] data_;
    }

    // Accessor methods
    const std::string& getName() const { return name_; }

    size_t getSize() const { return size_; }

    void setName(const std::string& name) { name_ = name; }

    void printData() const {
        std::cout << "Resource '" << name_ << "' data: ";
        if (data_ && size_ > 0) {
            for (size_t i = 0; i < std::min(size_t(5), size_); ++i) {
                std::cout << data_[i] << " ";
            }
            if (size_ > 5) {
                std::cout << "...";
            }
        } else {
            std::cout << "(empty)";
        }
        std::cout << std::endl;
    }
};

// A class with trivial destruction for testing
struct TrivialType {
    int value;

    TrivialType(int v = 0) : value(v) {}

    void increment() { ++value; }
    int getValue() const { return value; }
};

// Example 1: Basic Usage
void basicUsage() {
    std::cout << "\n=== Example 1: Basic Usage ===\n";

    // Create an Indestructible object with in_place construction
    Indestructible<Resource> res1(std::in_place, "Resource1");

    // Access the object using get()
    std::cout << "Resource name: " << res1.get().getName() << std::endl;

    // Access the object using arrow operator
    std::cout << "Resource name via arrow: " << res1->getName() << std::endl;

    // Modify the object
    res1->setName("UpdatedResource1");
    std::cout << "Updated resource name: " << res1.get().getName() << std::endl;

    // Note: res1 will not be destroyed at the end of the scope but its
    // destructor will be called
    std::cout << "Exiting basicUsage function\n";
}

// Example 2: Construction with Different Arguments
void constructionExamples() {
    std::cout << "\n=== Example 2: Construction with Different Arguments ===\n";

    // Default construction
    Indestructible<Resource> res1(std::in_place);
    std::cout << "Default constructed resource: " << res1->getName()
              << std::endl;

    // Construction with a string argument
    Indestructible<Resource> res2(std::in_place, "CustomResource");
    std::cout << "Custom named resource: " << res2->getName() << std::endl;

    // Construction with multiple arguments
    Indestructible<Resource> res3(std::in_place, "DataResource", 10);
    std::cout << "Resource with data, name: " << res3->getName()
              << ", size: " << res3->getSize() << std::endl;
    res3->printData();

    // Construction with trivial type
    Indestructible<TrivialType> trivial(std::in_place, 42);
    std::cout << "Trivial type value: " << trivial->getValue() << std::endl;
}

// Example 3: Copy and Move Semantics
void copyAndMoveExamples() {
    std::cout << "\n=== Example 3: Copy and Move Semantics ===\n";

    // Create an original resource
    Indestructible<Resource> original(std::in_place, "Original", 5);
    original->printData();

    // Copy construction
    Indestructible<Resource> copy = original;
    std::cout << "Copied resource name: " << copy->getName() << std::endl;
    copy->printData();

    // Move construction
    Indestructible<Resource> moved = std::move(original);
    std::cout << "Moved resource name: " << moved->getName() << std::endl;
    moved->printData();
    std::cout << "Original after move, name: " << original->getName()
              << std::endl;
    original->printData();

    // Create another resource for assignment
    Indestructible<Resource> res1(std::in_place, "AssignmentTarget");
    Indestructible<Resource> res2(std::in_place, "MoveTarget");

    // Copy assignment
    res1 = copy;
    std::cout << "After copy assignment, name: " << res1->getName()
              << std::endl;

    // Move assignment
    res2 = std::move(moved);
    std::cout << "After move assignment, name: " << res2->getName()
              << std::endl;
    std::cout << "Source after move assignment, name: " << moved->getName()
              << std::endl;
}

// Example 4: Reset and Emplace
void resetAndEmplaceExamples() {
    std::cout << "\n=== Example 4: Reset and Emplace ===\n";

    // Create an initial resource
    Indestructible<Resource> res(std::in_place, "InitialResource");
    std::cout << "Initial resource name: " << res->getName() << std::endl;

    // Reset the resource with new arguments
    std::cout << "Resetting resource...\n";
    res.reset("ResetResource");
    std::cout << "After reset, name: " << res->getName() << std::endl;

    // Reset with multiple arguments
    std::cout << "Resetting resource with data...\n";
    res.reset("DataResetResource", 8);
    std::cout << "After data reset, name: " << res->getName()
              << ", size: " << res->getSize() << std::endl;
    res->printData();

    // Emplace a new resource (equivalent to reset)
    std::cout << "Emplacing new resource...\n";
    res.emplace("EmplacedResource");
    std::cout << "After emplace, name: " << res->getName() << std::endl;
}

// Example 5: Implicit Conversion
void conversionExamples() {
    std::cout << "\n=== Example 5: Implicit Conversion ===\n";

    // Create an indestructible resource
    Indestructible<Resource> res(std::in_place, "ConversionResource");

    // Use implicit conversion to reference
    const Resource& ref = res;
    std::cout << "Reference from conversion, name: " << ref.getName()
              << std::endl;

    // Function that takes Resource by reference
    auto printResourceName = [](const Resource& r) {
        std::cout << "Resource name in function: " << r.getName() << std::endl;
    };

    // Pass Indestructible to function expecting Resource&
    printResourceName(res);

    // Function that takes Resource by value (copying)
    auto copyResource = [](Resource r) -> Resource {
        std::cout << "In copyResource function, received: " << r.getName()
                  << std::endl;
        return r;
    };

    // Pass Indestructible to function expecting Resource
    Resource copied = copyResource(res);
    std::cout << "Copied resource name: " << copied.getName() << std::endl;
}

// Example 6: Working with Trivial Types
void trivialTypeExamples() {
    std::cout << "\n=== Example 6: Working with Trivial Types ===\n";

    // Create indestructible trivial type
    Indestructible<TrivialType> trivial(std::in_place, 100);
    std::cout << "Initial trivial value: " << trivial->getValue() << std::endl;

    // Modify the value
    trivial->increment();
    trivial->increment();
    std::cout << "After increments: " << trivial->getValue() << std::endl;

    // Copy the indestructible object
    Indestructible<TrivialType> trivialCopy = trivial;
    std::cout << "Copied trivial value: " << trivialCopy->getValue()
              << std::endl;

    // Reset with a new value
    trivial.reset(500);
    std::cout << "After reset: " << trivial->getValue() << std::endl;

    // Convert to reference
    TrivialType& ref = trivial;
    ref.increment();
    std::cout << "After incrementing reference: " << trivial->getValue()
              << std::endl;
}

// Example 7: Using Indestructible with STL Containers
void containerExamples() {
    std::cout
        << "\n=== Example 7: Using Indestructible with STL Containers ===\n";

    // Create a vector of Indestructible<Resource>
    std::vector<Indestructible<Resource>> resources;

    // Add resources to the vector
    std::cout << "Adding resources to vector...\n";
    resources.emplace_back(std::in_place, "VectorResource1");
    resources.emplace_back(std::in_place, "VectorResource2", 3);
    resources.emplace_back(std::in_place, "VectorResource3");

    // Access resources in the vector
    std::cout << "Resources in vector:\n";
    for (size_t i = 0; i < resources.size(); ++i) {
        std::cout << i << ": " << resources[i]->getName();
        resources[i]->printData();
    }

    // Modify a resource in the vector
    resources[1].reset("UpdatedVectorResource", 5);
    std::cout << "After update: " << resources[1]->getName() << std::endl;
    resources[1]->printData();

    // Clear the vector (resources will not be destroyed but destructors will be
    // called)
    std::cout << "Clearing vector...\n";
    resources.clear();
    std::cout << "Vector size after clear: " << resources.size() << std::endl;
}

// Example 8: Using the destruction_guard
void destructionGuardExample() {
    std::cout << "\n=== Example 8: Using destruction_guard ===\n";

    // Allocate memory for a resource without calling the constructor
    void* memory = operator new(sizeof(Resource));

    // Construct the object in-place
    Resource* res = new (memory) Resource("GuardedResource", 4);
    res->printData();

    // Use a destruction guard to ensure destruction
    {
        destruction_guard<Resource> guard(res);
        std::cout << "Resource is guarded, name: " << res->getName()
                  << std::endl;

        // Do operations with the resource
        res->setName("RenamedGuardedResource");
        std::cout << "Updated guarded resource name: " << res->getName()
                  << std::endl;

        // At the end of this block, guard's destructor will be called,
        // which will destroy the resource but not free the memory
        std::cout << "Exiting guard scope...\n";
    }

    // Free the memory
    operator delete(memory);
    std::cout << "Memory freed\n";
}

// Example 9: Advanced Usage - Creating a Singleton
template <typename T>
class Singleton {
private:
    static Indestructible<T> instance_;

    // Ensure the class cannot be instantiated directly
    Singleton() = default;
    ~Singleton() = default;

public:
    static T& getInstance() { return instance_.get(); }
};

// Initialize the static member
template <typename T>
Indestructible<T> Singleton<T>::instance_(std::in_place);

// A singleton class example
class Logger {
private:
    std::string prefix_;
    int logCount_;

public:
    Logger() : prefix_("[LOG]"), logCount_(0) {
        std::cout << "Logger initialized\n";
    }

    void log(const std::string& message) {
        ++logCount_;
        std::cout << prefix_ << " [" << logCount_ << "]: " << message
                  << std::endl;
    }

    void setPrefix(const std::string& prefix) { prefix_ = prefix; }

    int getLogCount() const { return logCount_; }
};

void singletonExample() {
    std::cout << "\n=== Example 9: Singleton Pattern with Indestructible ===\n";

    // Access the singleton instance
    Logger& logger1 = Singleton<Logger>::getInstance();
    logger1.log("First message");
    logger1.log("Second message");

    // Change the prefix
    logger1.setPrefix("[CUSTOM_LOG]");
    logger1.log("Message with custom prefix");

    // Access the singleton from another function or thread would yield the same
    // instance
    Logger& logger2 = Singleton<Logger>::getInstance();
    std::cout << "Log count from second reference: " << logger2.getLogCount()
              << std::endl;
    logger2.log("Message from second reference");

    // Show that it's the same instance
    std::cout << "Log count after all messages: " << logger1.getLogCount()
              << std::endl;
    std::cout << "Addresses of logger1 and logger2: " << &logger1 << " and "
              << &logger2 << " (should be the same)\n";
}

int main() {
    std::cout << "===== Indestructible Class Usage Examples =====\n";

    basicUsage();
    constructionExamples();
    copyAndMoveExamples();
    resetAndEmplaceExamples();
    conversionExamples();
    trivialTypeExamples();
    containerExamples();
    destructionGuardExample();
    singletonExample();

    std::cout << "\nAll examples completed!\n";
    return 0;
}
