/**
 * Comprehensive examples for atom::meta::constructor utilities
 *
 * This file demonstrates all constructor functionality from
 * atom/meta/constructor.hpp:
 * 1. Basic constructors (default, shared, copy, move)
 * 2. Advanced constructors (async, lazy, singleton)
 * 3. Safety and validation features
 * 4. Builder pattern
 * 5. Binding methods and properties
 */

#include "atom/meta/constructor.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace atom::meta;
using namespace std::chrono_literals;

// Forward declarations
class SimpleClass;
class ComplexClass;
class ThreadSafeClass;
class ImmutableClass;
class ValidationClass;
class BuilderPatternClass;

// Section 1: Basic class for simple constructor examples
class SimpleClass {
private:
    std::string name_;
    int value_;

public:
    // Default constructor
    SimpleClass() : name_("Default"), value_(0) {
        std::cout << "SimpleClass default constructor called" << std::endl;
    }

    // Parameterized constructor
    SimpleClass(std::string name, int value)
        : name_(std::move(name)), value_(value) {
        std::cout << "SimpleClass parameterized constructor called for "
                  << name_ << std::endl;
    }

    // Copy constructor
    SimpleClass(const SimpleClass& other)
        : name_(other.name_), value_(other.value_) {
        std::cout << "SimpleClass copy constructor called for " << name_
                  << std::endl;
    }

    // Move constructor
    SimpleClass(SimpleClass&& other) noexcept
        : name_(std::move(other.name_)), value_(other.value_) {
        std::cout << "SimpleClass move constructor called" << std::endl;
        other.value_ = 0;
    }

    // Methods
    std::string getName() const { return name_; }
    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }

    // Utility method for demonstration
    void describe() const {
        std::cout << "SimpleClass: name=" << name_ << ", value=" << value_
                  << std::endl;
    }

    // Destructor
    ~SimpleClass() {
        std::cout << "SimpleClass destructor called for " << name_ << std::endl;
    }
};

// Section 2: Complex class with initialization requirements
class ComplexClass {
private:
    std::vector<std::string> data_;
    bool initialized_;
    std::shared_ptr<SimpleClass> dependency_;

public:
    // Constructors
    ComplexClass() : initialized_(false) {
        std::cout << "ComplexClass default constructor called" << std::endl;
    }

    explicit ComplexClass(std::initializer_list<std::string> items)
        : data_(items), initialized_(true), dependency_(nullptr) {
        std::cout << "ComplexClass initializer list constructor called with "
                  << items.size() << std::endl;
    }

    ComplexClass(std::shared_ptr<SimpleClass> dependency)
        : initialized_(true), dependency_(std::move(dependency)) {
        std::cout << "ComplexClass dependency constructor called" << std::endl;
    }

    // Methods
    bool initialize() {
        if (!initialized_) {
            std::cout << "ComplexClass being initialized" << std::endl;
            initialized_ = true;
            return true;
        }
        return false;
    }

    void addItem(const std::string& item) { data_.push_back(item); }

    size_t getItemCount() const { return data_.size(); }

    void describe() const {
        std::cout << "ComplexClass: " << data_.size() << " items, "
                  << (initialized_ ? "initialized" : "not initialized");
        if (dependency_) {
            std::cout << ", dependency: " << dependency_->getName();
        }
        std::cout << std::endl;
    }

    // Destructor
    ~ComplexClass() {
        std::cout << "ComplexClass destructor called" << std::endl;
    }
};

// Section 3: Thread-safe class for concurrency examples
class ThreadSafeClass {
private:
    std::mutex mutex_;
    int counter_;
    std::string name_;

public:
    ThreadSafeClass() : counter_(0), name_("Default") {
        std::cout << "ThreadSafeClass default constructor called" << std::endl;
    }

    ThreadSafeClass(const std::string& name, int initialCount)
        : counter_(initialCount), name_(name) {
        std::cout << "ThreadSafeClass parameterized constructor called for "
                  << name_ << std::endl;
    }

    // Thread-safe operation
    int increment() {
        std::lock_guard lock(mutex_);
        return ++counter_;
    }

    // Thread-safe getter
    int getCount() const {
        std::lock_guard lock(mutex_);
        return counter_;
    }

    // Thread-safe operation with delay (for testing async)
    int incrementWithDelay() {
        std::lock_guard lock(mutex_);
        std::this_thread::sleep_for(100ms);  // Simulate work
        return ++counter_;
    }

    std::string getName() const { return name_; }

    // Destructor
    ~ThreadSafeClass() {
        std::cout << "ThreadSafeClass destructor called for " << name_
                  << std::endl;
    }
};

// Section 4: Immutable class that can't be modified after construction
class ImmutableClass {
private:
    const std::string id_;
    const int value_;

public:
    ImmutableClass(std::string id, int value)
        : id_(std::move(id)), value_(value) {
        std::cout << "ImmutableClass constructor called for " << id_
                  << std::endl;
    }

    // Only getters, no setters
    std::string getId() const { return id_; }
    int getValue() const { return value_; }

    void describe() const {
        std::cout << "ImmutableClass: id=" << id_ << ", value=" << value_
                  << std::endl;
    }

    ~ImmutableClass() {
        std::cout << "ImmutableClass destructor called for " << id_
                  << std::endl;
    }
};

// Section 5: Class with validation requirements
class ValidationClass {
private:
    std::string email_;
    int age_;
    std::string code_;

public:
    ValidationClass(std::string email, int age, std::string code)
        : email_(std::move(email)), age_(age), code_(std::move(code)) {
        std::cout << "ValidationClass constructor called" << std::endl;
    }

    // 验证方法
    static bool isValidEmail(const std::string& email) {
        return email.find('@') != std::string::npos &&
               email.find('.') != std::string::npos;
    }

    static bool isValidAge(int age) { return age >= 0 && age <= 120; }

    static bool isValidCode(const std::string& code) {
        return code.length() == 6;
    }

    std::string getEmail() const { return email_; }
    int getAge() const { return age_; }
    std::string getCode() const { return code_; }

    void describe() const {
        std::cout << "ValidationClass: email=" << email_ << ", age=" << age_
                  << ", code=" << code_ << std::endl;
    }

    ~ValidationClass() {
        std::cout << "ValidationClass destructor called" << std::endl;
    }
};

// Section 6: Class designed for builder pattern
class BuilderPatternClass {
private:
    std::string name_;
    int id_;
    std::string description_;
    bool active_;
    std::vector<std::string> tags_;

public:
    BuilderPatternClass()
        : name_(""), id_(0), description_(""), active_(false) {
        std::cout << "BuilderPatternClass default constructor called"
                  << std::endl;
    }

    // 改为使用公共方法而不是直接访问成员
    BuilderPatternClass& setName(const std::string& name) {
        name_ = name;
        return *this;
    }

    BuilderPatternClass& setId(int id) {
        id_ = id;
        return *this;
    }

    BuilderPatternClass& setDescription(const std::string& desc) {
        description_ = desc;
        return *this;
    }

    BuilderPatternClass& setActive(bool active) {
        active_ = active;
        return *this;
    }

    BuilderPatternClass& addTag(const std::string& tag) {
        tags_.push_back(tag);
        return *this;
    }

    // Getters
    std::string getName() const { return name_; }
    int getId() const { return id_; }
    std::string getDescription() const { return description_; }
    bool isActive() const { return active_; }
    const std::vector<std::string>& getTags() const { return tags_; }

    void describe() const {
        std::cout << "BuilderPatternClass: name=" << name_ << ", id=" << id_
                  << ", description=" << description_
                  << ", active=" << (active_ ? "true" : "false") << ", tags=[";

        for (size_t i = 0; i < tags_.size(); ++i) {
            if (i > 0)
                std::cout << ", ";
            std::cout << tags_[i];
        }
        std::cout << "]" << std::endl;
    }

    ~BuilderPatternClass() {
        std::cout << "BuilderPatternClass destructor called for " << name_
                  << std::endl;
    }
};

// Main function with comprehensive examples
int main() {
    std::cout << "======================================================="
              << std::endl;
    std::cout << "   Constructor Utilities Comprehensive Examples         "
              << std::endl;
    std::cout << "======================================================="
              << std::endl
              << std::endl;

    // PART 1: Basic Constructor Examples
    std::cout << "PART 1: Basic Constructor Examples" << std::endl;
    std::cout << "--------------------------------" << std::endl;

    std::cout << "\n1.1: Default Constructor" << std::endl;
    std::cout << "--------------------" << std::endl;

    // Using default constructor
    auto defaultCtor = defaultConstructor<SimpleClass>();
    SimpleClass simple1 = defaultCtor();
    simple1.describe();

    std::cout << "\n1.2: Parameterized Constructor" << std::endl;
    std::cout << "---------------------------" << std::endl;

    // Using parameterized constructor
    auto paramCtor = constructor<SimpleClass, std::string, int>();
    auto simple2 = paramCtor("Custom", 42);
    simple2->describe();

    std::cout << "\n1.3: Copy Constructor" << std::endl;
    std::cout << "------------------" << std::endl;

    // Using copy constructor
    SimpleClass original("Original", 100);
    auto copyCtor = constructor<SimpleClass>();
    SimpleClass copy = copyCtor(original);
    copy.describe();

    std::cout << "\n1.4: Move Constructor" << std::endl;
    std::cout << "------------------" << std::endl;

    // Using move constructor
    auto moveCtor = buildMoveConstructor<SimpleClass>();
    SimpleClass moved = moveCtor(SimpleClass("Temporary", 200));
    moved.describe();

    std::cout << "\n1.5: Shared Constructor" << std::endl;
    std::cout << "-------------------" << std::endl;

    // Using shared constructor
    auto sharedCtor = constructor<std::shared_ptr<SimpleClass>>();
    auto sharedSimple = sharedCtor("Shared", 300);
    sharedSimple->describe();

    // PART 2: Advanced Constructor Examples
    std::cout << "\nPART 2: Advanced Constructor Examples" << std::endl;
    std::cout << "----------------------------------" << std::endl;

    std::cout << "\n2.1: Safe Constructor with Error Handling" << std::endl;
    std::cout << "------------------------------------" << std::endl;

    // Using safe constructor
    auto safeCtor = safeConstructor<SimpleClass>();
    auto result1 = safeCtor("Safe", 400);
    if (result1) {
        result1.value().describe();
    }

    // Example that would throw an exception
    try {
        SimpleClass* nullPtr = nullptr;
        nullPtr->describe();  // This would cause an exception
    } catch (const std::exception& e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
    }

    std::cout << "\n2.2: Validated Constructor" << std::endl;
    std::cout << "-----------------------" << std::endl;

    // Using validated constructor
    auto validatorFunc = [](const std::string& email, int age,
                            const std::string& code) -> bool {
        return ValidationClass::isValidEmail(email) &&
               ValidationClass::isValidAge(age) &&
               ValidationClass::isValidCode(code);
    };

    auto validatedCtor =
        buildValidatedSharedConstructor<ValidationClass, std::string, int,
                                        std::string>(validatorFunc);

    // Valid parameters
    auto validResult = validatedCtor("user@example.com", 30, "123456");
    if (validResult.isValid()) {
        validResult.getValue()->describe();
    }

    // Invalid parameters
    auto invalidResult = validatedCtor("invalid-email", 150, "12345");
    if (!invalidResult.isValid()) {
        std::cout << "Validation failed as expected: "
                  << invalidResult.error.value_or("Unknown error") << std::endl;
    }

    std::cout << "\n2.3: Async Constructor" << std::endl;
    std::cout << "-------------------" << std::endl;

    // Using async constructor
    auto asyncCtor = asyncConstructor<ThreadSafeClass, std::string, int>();

    std::cout << "Starting async construction..." << std::endl;
    auto futureObj = asyncCtor("AsyncWorker", 0);

    std::cout << "Doing other work while constructing..." << std::endl;
    std::this_thread::sleep_for(50ms);

    auto asyncObj = futureObj.get();
    std::cout << "Async object created: " << asyncObj->getName()
              << ", count: " << asyncObj->getCount() << std::endl;

    // Increment in multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&asyncObj]() {
            int newValue = asyncObj->incrementWithDelay();
            std::cout << "Thread incremented to: " << newValue << std::endl;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Final count: " << asyncObj->getCount() << std::endl;

    std::cout << "\n2.4: Lazy Constructor" << std::endl;
    std::cout << "------------------" << std::endl;

    // Using lazy constructor
    auto lazyCtor = lazyConstructor<SimpleClass, std::string, int>();

    std::cout << "Lazy constructor defined but not called yet" << std::endl;

    std::cout << "First access triggers construction:" << std::endl;
    auto& lazyObj = lazyCtor("LazyObject", 500);
    lazyObj.describe();

    std::cout << "Second access reuses the same instance:" << std::endl;
    auto& lazyObjRef = lazyCtor(
        "IgnoredParams", 999);  // Parameters are ignored on subsequent calls
    lazyObjRef.describe();

    // Verify it's the same object by modifying it
    lazyObjRef.setValue(501);
    std::cout << "After modification: ";
    lazyObj.describe();

    std::cout << "\n2.5: Singleton Constructor" << std::endl;
    std::cout << "----------------------" << std::endl;

    // Using singleton constructor
    auto singletonCtor =
        singletonConstructor<SimpleClass, true>();  // Thread-safe version

    std::cout << "Getting first singleton instance:" << std::endl;
    auto singleton1 = singletonCtor();
    singleton1->describe();

    std::cout << "Getting second singleton instance (should be same object):"
              << std::endl;
    auto singleton2 = singletonCtor();

    // Verify they're the same object
    singleton2->setValue(600);
    std::cout << "After modifying second reference:" << std::endl;
    singleton1->describe();

    // Verify addresses are the same
    std::cout << "Singleton1 address: " << singleton1.get()
              << ", Singleton2 address: " << singleton2.get() << std::endl;

    std::cout << "\n2.6: Factory Constructor" << std::endl;
    std::cout << "--------------------" << std::endl;

    // Using factory constructor
    auto factoryCtor = factoryConstructor<ComplexClass>();

    // Default construction
    auto defaultComplex = factoryCtor();
    defaultComplex->describe();

    // Dependency injection construction
    auto simpleDep = std::make_shared<SimpleClass>("Dependency", 700);
    auto complexWithDep = factoryCtor(simpleDep);
    complexWithDep->describe();

    // Method chaining after construction
    auto complex = factoryCtor();
    complex->initialize();
    complex->addItem("Item 1");
    complex->addItem("Item 2");
    std::cout << "Item count: " << complex->getItemCount() << std::endl;

    // PART 3: Binding Methods and Properties
    std::cout << "\nPART 3: Binding Methods and Properties" << std::endl;
    std::cout << "----------------------------------" << std::endl;

    std::cout << "\n3.1: Binding Member Functions" << std::endl;
    std::cout << "-------------------------" << std::endl;

    SimpleClass instance("Instance", 800);

    // Binding a member function
    auto describeBound = bindMemberFunction(&SimpleClass::describe);
    describeBound(instance);

    // Binding a const member function
    auto getNameBound = bindConstMemberFunction(&SimpleClass::getName);
    std::cout << "Name via bound function: " << getNameBound(instance)
              << std::endl;

    std::cout << "\n3.2: Binding Static Functions" << std::endl;
    std::cout << "-------------------------" << std::endl;

    // Binding a static function
    auto isValidEmailBound = bindStaticFunction(&ValidationClass::isValidEmail);
    std::cout << "Is 'user@example.com' valid? "
              << (isValidEmailBound("user@example.com") ? "Yes" : "No")
              << std::endl;
    std::cout << "Is 'invalid-email' valid? "
              << (isValidEmailBound("invalid-email") ? "Yes" : "No")
              << std::endl;

    // Binding a lambda as a static function
    auto sumFunc = bindStaticFunction([](int a, int b) { return a + b; });
    std::cout << "5 + 7 = " << sumFunc(5, 7) << std::endl;

    // PART 4: Builder Pattern
    std::cout << "\nPART 4: Builder Pattern" << std::endl;
    std::cout << "--------------------" << std::endl;

    // Using builder pattern
    auto builder = makeBuilder<BuilderPatternClass>();

    auto product =
        builder.with(&BuilderPatternClass::name_, "ProductX")
            .with(&BuilderPatternClass::id_, 1001)
            .with(&BuilderPatternClass::description_, "A fantastic product")
            .with(&BuilderPatternClass::active_, true)
            .call(&BuilderPatternClass::addTag, "featured")
            .call(&BuilderPatternClass::addTag, "new")
            .call(&BuilderPatternClass::addTag, "limited")
            .build();

    product->describe();

    // PART 5: Custom Constructors
    std::cout << "\nPART 5: Custom Constructors" << std::endl;
    std::cout << "----------------------" << std::endl;

    // Custom constructor logic
    auto customCtor = customConstructor<ImmutableClass>(
        [](const std::string& prefix, int id) {
            std::string uniqueId = prefix + "-" + std::to_string(id);
            return ImmutableClass(uniqueId, id * 100);
        });

    auto customObj = customCtor("PROD", 42);
    customObj.describe();

    // Safe custom constructor with error handling
    auto safeCustomCtor = safeCustomConstructor<SimpleClass>(
        [](const std::string& name, int value) {
            if (value < 0) {
                throw std::invalid_argument("Value cannot be negative");
            }
            return SimpleClass(name, value);
        });

    auto goodResult = safeCustomCtor("Valid", 100);
    if (goodResult.isValid()) {
        goodResult.getValue().describe();
    }

    auto badResult = safeCustomCtor("Invalid", -100);
    if (!badResult.isValid()) {
        std::cout << "Error: " << badResult.error.value_or("Unknown error")
                  << std::endl;
    }

    std::cout << "\nAll examples completed successfully!" << std::endl;
    return 0;
}