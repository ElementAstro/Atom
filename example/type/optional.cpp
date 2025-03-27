#include "../atom/type/optional.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>  // 添加这个头文件来包含 std::sin
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

// A test class to demonstrate Optional with complex types
class Person {
private:
    std::string name_;
    int age_;
    std::string address_;

public:
    // Default constructor
    Person() : name_("Unknown"), age_(0), address_("No Address") {
        std::cout << "Person default constructed" << std::endl;
    }

    // Parameterized constructor
    Person(std::string name, int age, std::string address = "No Address")
        : name_(std::move(name)), age_(age), address_(std::move(address)) {
        std::cout << "Person constructed: " << name_ << ", age " << age_
                  << std::endl;
    }

    // Copy constructor
    Person(const Person& other)
        : name_(other.name_ + " (copy)"),
          age_(other.age_),
          address_(other.address_) {
        std::cout << "Person copied: " << name_ << std::endl;
    }

    // Move constructor
    Person(Person&& other) noexcept
        : name_(std::move(other.name_)),
          age_(other.age_),
          address_(std::move(other.address_)) {
        other.age_ = 0;
        std::cout << "Person moved: " << name_ << std::endl;
    }

    // Destructor
    ~Person() { std::cout << "Person destroyed: " << name_ << std::endl; }

    // Accessors
    const std::string& getName() const { return name_; }
    int getAge() const { return age_; }
    const std::string& getAddress() const { return address_; }

    // Mutators
    void setName(const std::string& name) { name_ = name; }
    void setAge(int age) { age_ = age; }
    void setAddress(const std::string& address) { address_ = address; }

    // For comparison operators
    bool operator==(const Person& other) const {
        return name_ == other.name_ && age_ == other.age_ &&
               address_ == other.address_;
    }

    auto operator<=>(const Person& other) const {
        if (auto cmp = name_ <=> other.name_; cmp != 0)
            return cmp;
        if (auto cmp = age_ <=> other.age_; cmp != 0)
            return cmp;
        return address_ <=> other.address_;
    }
};

// Print function for Optional values
template <typename T>
void printOptional(const atom::type::Optional<T>& opt,
                   const std::string& name) {
    std::cout << name << ": ";
    if (opt.has_value()) {
        std::cout << "has value: " << opt.value() << std::endl;
    } else {
        std::cout << "no value" << std::endl;
    }
}

// Specialized print function for Optional<Person>
void printPersonOptional(const atom::type::Optional<Person>& opt,
                         const std::string& name) {
    std::cout << name << ": ";
    if (opt.has_value()) {
        std::cout << "has value: " << opt->getName() << ", age "
                  << opt->getAge() << std::endl;
    } else {
        std::cout << "no value" << std::endl;
    }
}

// Example 1: Basic Usage
void basicUsageExample() {
    std::cout << "\n=== Example 1: Basic Usage ===\n";

    // Create an empty Optional
    atom::type::Optional<int> emptyOpt;
    printOptional(emptyOpt, "emptyOpt");

    // Create an Optional with a value
    atom::type::Optional<int> intOpt(42);
    printOptional(intOpt, "intOpt");

    // Create using make_optional helper
    auto stringOpt = atom::type::make_optional(std::string("Hello, Optional!"));
    printOptional(stringOpt, "stringOpt");

    // 使用emplace构造Person，而不是直接传参数
    auto personOpt = atom::type::Optional<Person>();
    personOpt.emplace("Alice", 30);
    printPersonOptional(personOpt, "personOpt");

    // Check if an Optional has a value
    std::cout << "emptyOpt has value: " << (emptyOpt.has_value() ? "yes" : "no")
              << std::endl;
    std::cout << "intOpt has value: " << (intOpt.has_value() ? "yes" : "no")
              << std::endl;

    // Using boolean conversion
    if (intOpt) {
        std::cout << "intOpt is truthy (has value)" << std::endl;
    }

    if (!emptyOpt) {
        std::cout << "emptyOpt is falsy (no value)" << std::endl;
    }
}

// Example 2: Accessing Values
void accessingValuesExample() {
    std::cout << "\n=== Example 2: Accessing Values ===\n";

    atom::type::Optional<int> intOpt(42);

    // Using operator*
    std::cout << "Value using operator*: " << *intOpt << std::endl;

    // Using value()
    std::cout << "Value using value(): " << intOpt.value() << std::endl;

    // Using value_or() with a present value
    std::cout << "Value using value_or(99): " << intOpt.value_or(99)
              << std::endl;

    // Creating an empty Optional
    atom::type::Optional<int> emptyOpt;

    // Using value_or() with an empty Optional
    std::cout << "Empty Optional using value_or(99): " << emptyOpt.value_or(99)
              << std::endl;

    // 使用emplace构造Person，而不是直接传参数
    atom::type::Optional<Person> personOpt;
    personOpt.emplace("Bob", 25);
    std::cout << "Person name using operator->: " << personOpt->getName()
              << std::endl;
    std::cout << "Person age using operator->: " << personOpt->getAge()
              << std::endl;

    // Error handling when accessing empty Optional
    std::cout << "Attempting to access empty Optional..." << std::endl;
    try {
        int value = emptyOpt.value();
        std::cout << "This line should not be reached: " << value << std::endl;
    } catch (const atom::type::OptionalAccessError& e) {
        std::cout << "Caught expected exception: " << e.what() << std::endl;
    }
}

// Example 3: Modifying Values
void modifyingValuesExample() {
    std::cout << "\n=== Example 3: Modifying Values ===\n";

    // Create an Optional with an int
    atom::type::Optional<int> intOpt(10);
    printOptional(intOpt, "Initial intOpt");

    // Modify value through dereference
    *intOpt = 20;
    printOptional(intOpt, "After *intOpt = 20");

    // 使用emplace构造Person，而不是直接传参数
    atom::type::Optional<Person> personOpt;
    personOpt.emplace("Charlie", 35);
    printPersonOptional(personOpt, "Initial personOpt");

    // Modify using arrow operator
    personOpt->setAge(36);
    personOpt->setName("Charles");
    printPersonOptional(personOpt, "After modifying person");

    // Reset an Optional (clear its value)
    intOpt.reset();
    printOptional(intOpt, "After reset()");

    // Assign a new value
    intOpt = 30;
    printOptional(intOpt, "After assigning 30");

    // Emplace a new value
    personOpt.emplace("David", 40, "123 Main St");
    printPersonOptional(personOpt, "After emplace()");

    // Assign with nullopt
    intOpt = std::nullopt;
    printOptional(intOpt, "After assigning nullopt");
}

// Example 4: Copy and Move Semantics
void copyMoveExample() {
    std::cout << "\n=== Example 4: Copy and Move Semantics ===\n";

    // 首先创建一个Person对象
    Person eve("Eve", 28);

    // 使用已有的Person对象创建Optional
    atom::type::Optional<Person> original;
    original.emplace(eve);
    printPersonOptional(original, "Original");

    // Copy construction
    std::cout << "Creating copy..." << std::endl;
    atom::type::Optional<Person> copy;
    copy = original;  // 使用赋值而不是复制构造
    printPersonOptional(copy, "Copy");
    printPersonOptional(original, "Original after copy");

    // Move construction
    std::cout << "Creating moved..." << std::endl;
    atom::type::Optional<Person> moved;
    moved = std::move(original);  // 使用移动赋值而不是移动构造
    printPersonOptional(moved, "Moved");
    printPersonOptional(original, "Original after move");

    // Copy assignment
    atom::type::Optional<Person> copyAssign;
    std::cout << "Copy assignment..." << std::endl;
    copyAssign = copy;
    printPersonOptional(copyAssign, "Copy assigned");
    printPersonOptional(copy, "Copy after assignment");

    // Move assignment
    atom::type::Optional<Person> moveAssign;
    std::cout << "Move assignment..." << std::endl;
    moveAssign = std::move(moved);
    printPersonOptional(moveAssign, "Move assigned");
    printPersonOptional(moved, "Moved after assignment");
}

// Example 5: Comparison Operations
void comparisonExample() {
    std::cout << "\n=== Example 5: Comparison Operations ===\n";

    // Create Optionals with values
    atom::type::Optional<int> a(10);
    atom::type::Optional<int> b(20);
    atom::type::Optional<int> c(10);
    atom::type::Optional<int> empty;
    atom::type::Optional<int> alsoEmpty;

    // Equality comparisons
    std::cout << "a == c: " << (a == c ? "true" : "false") << std::endl;
    std::cout << "a == b: " << (a == b ? "true" : "false") << std::endl;
    std::cout << "empty == alsoEmpty: "
              << (empty == alsoEmpty ? "true" : "false") << std::endl;
    std::cout << "a == empty: " << (a == empty ? "true" : "false") << std::endl;

    // Compare with nullopt
    std::cout << "a == std::nullopt: " << (a == std::nullopt ? "true" : "false")
              << std::endl;
    std::cout << "empty == std::nullopt: "
              << (empty == std::nullopt ? "true" : "false") << std::endl;

    // Three-way comparison
    std::cout << "a <=> b is less: "
              << ((a <=> b) == std::strong_ordering::less ? "true" : "false")
              << std::endl;
    std::cout << "b <=> a is greater: "
              << ((b <=> a) == std::strong_ordering::greater ? "true" : "false")
              << std::endl;
    std::cout << "a <=> c is equal: "
              << ((a <=> c) == std::strong_ordering::equal ? "true" : "false")
              << std::endl;
    std::cout << "a <=> empty is greater: "
              << ((a <=> empty) == std::strong_ordering::greater ? "true"
                                                                 : "false")
              << std::endl;
    std::cout << "empty <=> a is less: "
              << ((empty <=> a) == std::strong_ordering::less ? "true"
                                                              : "false")
              << std::endl;
    std::cout << "empty <=> alsoEmpty is equal: "
              << ((empty <=> alsoEmpty) == std::strong_ordering::equal
                      ? "true"
                      : "false")
              << std::endl;

    // Three-way comparison with nullopt
    std::cout << "a <=> std::nullopt is greater: "
              << ((a <=> std::nullopt) == std::strong_ordering::greater
                      ? "true"
                      : "false")
              << std::endl;
    std::cout << "empty <=> std::nullopt is equal: "
              << ((empty <=> std::nullopt) == std::strong_ordering::equal
                      ? "true"
                      : "false")
              << std::endl;
}

// Example 6: Functional Operations
void functionalOperationsExample() {
    std::cout << "\n=== Example 6: Functional Operations ===\n";

    // Create an Optional with a value
    atom::type::Optional<int> intOpt(42);

    // map - transform the value and return a new Optional
    auto doubledOpt = intOpt.map([](int x) { return x * 2; });
    printOptional(doubledOpt, "After map (double)");

    // map on empty Optional
    atom::type::Optional<int> emptyOpt;
    auto emptyDoubledOpt = emptyOpt.map([](int x) { return x * 2; });
    printOptional(emptyDoubledOpt, "map on empty Optional");

    // transform - alias for map
    auto squaredOpt = intOpt.transform([](int x) { return x * x; });
    printOptional(squaredOpt, "After transform (square)");

    // and_then - apply function and return its result
    auto strLengthOpt = atom::type::make_optional(std::string("Hello, World!"));
    int length =
        strLengthOpt.and_then([](const std::string& s) { return s.length(); });
    std::cout << "and_then result: " << length << std::endl;

    // flat_map - alias for and_then
    int length2 =
        strLengthOpt.flat_map([](const std::string& s) { return s.length(); });
    std::cout << "flat_map result: " << length2 << std::endl;

    // or_else - provide default value through function if empty
    int valueOrDefault = emptyOpt.or_else([]() {
        return 100;  // Default value
    });
    std::cout << "or_else on empty Optional: " << valueOrDefault << std::endl;

    // transform_or - transform if has value, otherwise use default
    auto transformOrResult = emptyOpt.transform_or(
        [](int x) { return x * 3; }, 999);  // Default value if empty
    printOptional(transformOrResult, "transform_or on empty Optional");

    auto transformOrResult2 = intOpt.transform_or(
        [](int x) { return x * 3; }, 999);  // Default value not used
    printOptional(transformOrResult2, "transform_or on non-empty Optional");

    // if_has_value - execute function on value for side effects
    intOpt.if_has_value(
        [](int x) { std::cout << "Value is: " << x << std::endl; });

    // 修复未使用的参数警告
    emptyOpt.if_has_value([](int /* x */) {
        std::cout << "This line will not be printed for empty Optional"
                  << std::endl;
    });

    // Chain multiple operations
    auto chain =
        intOpt
            .map([](int x) { return x + 10; })  // 42 + 10 = 52
            .map([](int x) { return x * 2; })   // 52 * 2 = 104
            .transform([](int x) { return std::to_string(x); });  // "104"

    std::cout << "After chaining operations: " << chain.value() << std::endl;
}

// Example 7: Thread Safety
void threadSafetyExample() {
    std::cout << "\n=== Example 7: Thread Safety ===\n";

    // Create a shared Optional
    atom::type::Optional<std::vector<int>> sharedOpt;

    // Function to be run in a thread
    auto threadFunc = [&](int id, int start, int count) {
        for (int i = 0; i < count; ++i) {
            // If empty, initialize with a vector
            if (!sharedOpt.has_value()) {
                std::vector<int> vec;
                sharedOpt = vec;  // Thread-safe assignment
            }

            // Add a value to the vector (thread-safe)
            sharedOpt.if_has_value([id, start, i](std::vector<int>& vec) {
                vec.push_back(start + i);
                std::cout << "Thread " << id << " added " << (start + i)
                          << std::endl;
                // Simulate work
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            });
        }
    };

    // Create threads
    std::vector<std::thread> threads;
    const int numThreads = 3;
    const int countPerThread = 5;

    std::cout << "Starting " << numThreads << " threads..." << std::endl;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(threadFunc, i, i * 100, countPerThread);
    }

    // Join all threads
    for (auto& t : threads) {
        t.join();
    }

    // Print final vector contents
    if (sharedOpt.has_value()) {
        std::cout << "Final vector contents: ";
        for (int val : sharedOpt.value()) {
            std::cout << val << " ";
        }
        std::cout << std::endl;

        std::cout << "Vector size: " << sharedOpt->size()
                  << " (expected: " << numThreads * countPerThread << ")"
                  << std::endl;
    }
}

// Example 8: SIMD Operations
void simdOperationsExample() {
    std::cout << "\n=== Example 8: SIMD Operations ===\n";

    // Create an Optional with a vector for SIMD operations
    std::vector<float> data(1000);
    std::iota(data.begin(), data.end(), 0.0f);  // Fill with 0, 1, 2, ...

    atom::type::Optional<std::vector<float>> vectorOpt(data);

    // Standard map operation (for comparison)
    auto startStd = std::chrono::high_resolution_clock::now();
    auto result1 = vectorOpt.map([](const std::vector<float>& vec) {
        std::vector<float> result(vec.size());
        // Calculate sine of each element
        std::transform(vec.begin(), vec.end(), result.begin(),
                       [](float x) { return std::sin(x); });
        return result;
    });
    auto endStd = std::chrono::high_resolution_clock::now();

    // SIMD map operation (note: in a real implementation this would use SIMD
    // intrinsics)
    auto startSimd = std::chrono::high_resolution_clock::now();
    auto result2 = vectorOpt.simd_map([](const std::vector<float>& vec) {
        std::vector<float> result(vec.size());
        // In a real implementation, this would use SIMD operations
        // For this example, we'll do the same operation as above
        std::transform(vec.begin(), vec.end(), result.begin(),
                       [](float x) { return std::sin(x); });
        return result;
    });
    auto endSimd = std::chrono::high_resolution_clock::now();

    // Calculate durations
    auto stdDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        endStd - startStd);
    auto simdDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        endSimd - startSimd);

    std::cout << "Standard map operation took " << stdDuration.count()
              << " microseconds" << std::endl;
    std::cout << "SIMD map operation took " << simdDuration.count()
              << " microseconds" << std::endl;

    // Verify first few results are same
    if (result1.has_value() && result2.has_value()) {
        std::cout << "First 5 elements of result:" << std::endl;
        // 修复符号类型比较警告
        for (std::size_t i = 0; i < 5 && i < result1->size(); ++i) {
            std::cout << "  Standard: " << std::setprecision(6) << (*result1)[i]
                      << ", SIMD: " << std::setprecision(6) << (*result2)[i]
                      << std::endl;
        }
    }
}

// Example 9: Error Handling
void errorHandlingExample() {
    std::cout << "\n=== Example 9: Error Handling ===\n";

    // Create an Optional with a dividing function
    atom::type::Optional<int> intOpt(42);

    // Function that might throw
    auto divideBy = [](int x, int divisor) {
        if (divisor == 0) {
            throw std::runtime_error("Division by zero");
        }
        return x / divisor;
    };

    // Normal operation
    try {
        auto result = intOpt.map([&](int x) { return divideBy(x, 2); });
        std::cout << "42 / 2 = " << result.value() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
    }

    // Operation that will throw - 修复未使用变量警告
    try {
        intOpt.map([&](int x) { return divideBy(x, 0); });
        std::cout << "This line won't be reached" << std::endl;
    } catch (const atom::type::OptionalOperationError& e) {
        // Our wrapper exception type
        std::cout << "OptionalOperationError caught: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Other exception caught: " << e.what() << std::endl;
    }

    // Accessing an empty Optional
    atom::type::Optional<int> emptyOpt;
    try {
        int value = emptyOpt.value();
        std::cout << "This line won't be reached: " << value << std::endl;
    } catch (const atom::type::OptionalAccessError& e) {
        std::cout << "OptionalAccessError caught: " << e.what() << std::endl;
    }

    // Safe access patterns
    if (emptyOpt.has_value()) {
        std::cout << "Value: " << emptyOpt.value() << std::endl;
    } else {
        std::cout << "Optional is empty, using safe check" << std::endl;
    }

    // Using value_or for safe access
    int safeValue = emptyOpt.value_or(0);
    std::cout << "Safe value using value_or: " << safeValue << std::endl;
}

// Example 10: Advanced Usage Patterns
void advancedUsageExample() {
    std::cout << "\n=== Example 10: Advanced Usage Patterns ===\n";

    // Creating a vector of Optionals
    std::vector<atom::type::Optional<int>> optVector;

    // 手动添加而不是使用make_optional来避免复制构造问题
    atom::type::Optional<int> opt1;
    opt1 = 10;
    optVector.push_back(std::move(opt1));

    optVector.emplace_back();  // Empty

    atom::type::Optional<int> opt2;
    opt2 = 20;
    optVector.push_back(std::move(opt2));

    optVector.emplace_back();  // Empty

    atom::type::Optional<int> opt3;
    opt3 = 30;
    optVector.push_back(std::move(opt3));

    // Filter out empty Optionals and sum the values
    int sum = 0;
    for (const auto& opt : optVector) {
        sum += opt.value_or(0);
    }
    std::cout << "Sum of all values (empty ones replaced with 0): " << sum
              << std::endl;

    // Count non-empty Optionals
    int count = std::count_if(optVector.begin(), optVector.end(),
                              [](const auto& opt) { return opt.has_value(); });
    std::cout << "Number of non-empty Optionals: " << count << std::endl;

    // Calculate average of non-empty Optionals
    if (count > 0) {
        int valueSum = 0;
        for (const auto& opt : optVector) {
            if (opt.has_value()) {
                valueSum += opt.value();
            }
        }
        double average = static_cast<double>(valueSum) / count;
        std::cout << "Average of non-empty values: " << average << std::endl;
    }

    // Using Optional to represent a configuration with defaults
    struct Config {
        std::string serverName = "localhost";
        int port = 8080;
        bool useSSL = false;
    };

    // Parse a "config file" with missing values
    atom::type::Optional<std::string>
        configServerName;                           // No server name specified
    atom::type::Optional<int> configPort(9000);     // Port specified
    atom::type::Optional<bool> configUseSSL(true);  // SSL specified

    // Build config using Optional values
    Config config;
    if (configServerName.has_value()) {
        config.serverName = configServerName.value();
    }
    if (configPort.has_value()) {
        config.port = configPort.value();
    }
    if (configUseSSL.has_value()) {
        config.useSSL = configUseSSL.value();
    }

    std::cout << "Final configuration:" << std::endl;
    std::cout << "  Server: " << config.serverName << std::endl;
    std::cout << "  Port: " << config.port << std::endl;
    std::cout << "  Use SSL: " << (config.useSSL ? "yes" : "no") << std::endl;
}

int main() {
    std::cout << "===== Optional<T> Usage Examples =====\n";

    // Run all examples
    basicUsageExample();
    accessingValuesExample();
    modifyingValuesExample();
    copyMoveExample();
    comparisonExample();
    functionalOperationsExample();
    threadSafetyExample();
    simdOperationsExample();
    errorHandlingExample();
    advancedUsageExample();

    return 0;
}