/**
 * Comprehensive examples for atom::meta::concept utilities
 *
 * This file demonstrates all concept categories from atom/meta/concept.hpp:
 * 1. Function Concepts
 * 2. Object Concepts
 * 3. Type Concepts
 * 4. Container Concepts
 * 5. Multi-threading Concepts
 * 6. Asynchronous Concepts
 */

#include "atom/meta/concept.hpp"

#include <algorithm>
#include <complex>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>

// Helper function to print concept check results
template <bool Result>
void printConceptCheck(const std::string& conceptName,
                       const std::string& typeName) {
    std::cout << conceptName << " check for " << typeName << ": "
              << (Result ? "Satisfied" : "Not satisfied") << std::endl;
}

// -----------------------------------------------------------------------------
// Function Concept Examples
// -----------------------------------------------------------------------------

// Example function to test function concepts
int add(int a, int b) { return a + b; }

// Noexcept function example
void noexceptFunc() noexcept {}

// Class with operator()
class Functor {
public:
    int operator()(int a, int b) const { return a * b; }
};

// Class with noexcept operator()
class NoexceptFunctor {
public:
    int operator()(int a, int b) const noexcept { return a * b; }
};

// Member function for testing
class TestClass {
public:
    int multiply(int a, int b) const { return a * b; }
};

// Function to demonstrate function concepts
void testFunctionConcepts() {
    std::cout << "=== Function Concepts Tests ===" << std::endl;

    // Test Invocable
    printConceptCheck<Invocable<decltype(add), int, int>>("Invocable",
                                                          "add(int, int)");

    // Test InvocableR
    printConceptCheck<InvocableR<decltype(add), int, int, int>>(
        "InvocableR<int>", "add(int, int)");
    printConceptCheck<InvocableR<decltype(add), float, int, int>>(
        "InvocableR<float>", "add(int, int)");

    // Test NothrowInvocable
    printConceptCheck<NothrowInvocable<decltype(noexceptFunc)>>(
        "NothrowInvocable", "noexceptFunc()");
    printConceptCheck<NothrowInvocable<decltype(add), int, int>>(
        "NothrowInvocable", "add(int, int)");

    // Test NothrowInvocableR
    printConceptCheck<NothrowInvocableR<decltype(noexceptFunc), void>>(
        "NothrowInvocableR<void>", "noexceptFunc()");

    // Test FunctionPointer
    printConceptCheck<FunctionPointer<decltype(&add)>>("FunctionPointer",
                                                       "&add");

    // Test MemberFunctionPointer
    printConceptCheck<MemberFunctionPointer<decltype(&TestClass::multiply)>>(
        "MemberFunctionPointer", "&TestClass::multiply");

    // Test Callable
    Functor functor;
    printConceptCheck<Callable<Functor>>("Callable", "Functor");
    int result = functor(5, 3);  // Actually use functor
    std::cout << "Functor result: " << result << std::endl;
    printConceptCheck<Callable<decltype(add)>>("Callable", "add function");

    // Test CallableReturns
    printConceptCheck<CallableReturns<Functor, int, int, int>>(
        "CallableReturns<int>", "Functor");

    // Test CallableNoexcept
    NoexceptFunctor noexceptFunctor;
    printConceptCheck<CallableNoexcept<NoexceptFunctor, int, int>>(
        "CallableNoexcept", "NoexceptFunctor");
    result = noexceptFunctor(4, 2);  // Actually use noexceptFunctor
    std::cout << "NoexceptFunctor result: " << result << std::endl;

    // Test StdFunction
    std::function<int(int, int)> stdFunc = add;
    printConceptCheck<StdFunction<decltype(stdFunc)>>(
        "StdFunction", "std::function<int(int, int)>");

    std::cout << std::endl;
}

// -----------------------------------------------------------------------------
// Object Concept Examples
// -----------------------------------------------------------------------------

// Class that satisfies Relocatable
class RelocatableClass {
public:
    RelocatableClass() = default;
    RelocatableClass(RelocatableClass&&) noexcept = default;
    RelocatableClass& operator=(RelocatableClass&&) noexcept = default;
};

// Class that is not Relocatable
class NonRelocatableClass {
public:
    NonRelocatableClass() = default;
    NonRelocatableClass(NonRelocatableClass&&) = default;  // Not noexcept
};

// Class for testing equality and comparison
class ComparableClass {
private:
    int value;

public:
    ComparableClass(int v) : value(v) {}

    bool operator==(const ComparableClass& other) const {
        return value == other.value;
    }

    bool operator!=(const ComparableClass& other) const {
        return value != other.value;
    }

    bool operator<(const ComparableClass& other) const {
        return value < other.value;
    }
};

// Class for testing Hashable
class HashableClass {
private:
    int value;

public:
    HashableClass(int v) : value(v) {}

    bool operator==(const HashableClass& other) const {
        return value == other.value;
    }

    int getValue() const { return value; }
};

// Custom hash function for HashableClass
namespace std {
template <>
struct hash<HashableClass> {
    std::size_t operator()(const HashableClass& obj) const {
        return std::hash<int>()(obj.getValue());
    }
};
}  // namespace std

// Function to demonstrate object concepts
void testObjectConcepts() {
    std::cout << "=== Object Concepts Tests ===" << std::endl;

    // Test Relocatable
    printConceptCheck<Relocatable<RelocatableClass>>("Relocatable",
                                                     "RelocatableClass");
    printConceptCheck<Relocatable<NonRelocatableClass>>("Relocatable",
                                                        "NonRelocatableClass");

    // Test DefaultConstructible
    printConceptCheck<DefaultConstructible<RelocatableClass>>(
        "DefaultConstructible", "RelocatableClass");

    // Test CopyConstructible
    printConceptCheck<CopyConstructible<std::string>>("CopyConstructible",
                                                      "std::string");

    // Test CopyAssignable
    printConceptCheck<CopyAssignable<std::string>>("CopyAssignable",
                                                   "std::string");

    // Test MoveAssignable
    printConceptCheck<MoveAssignable<std::string>>("MoveAssignable",
                                                   "std::string");

    // Test EqualityComparable
    printConceptCheck<EqualityComparable<ComparableClass>>("EqualityComparable",
                                                           "ComparableClass");

    // Test LessThanComparable
    printConceptCheck<LessThanComparable<ComparableClass>>("LessThanComparable",
                                                           "ComparableClass");

    // Test Hashable
    printConceptCheck<Hashable<HashableClass>>("Hashable", "HashableClass");

    // Test Swappable
    printConceptCheck<Swappable<std::string>>("Swappable", "std::string");

    // Test Copyable
    printConceptCheck<Copyable<std::string>>("Copyable", "std::string");

    // Test Destructible
    printConceptCheck<Destructible<std::string>>("Destructible", "std::string");

    std::cout << std::endl;
}

// -----------------------------------------------------------------------------
// Type Concept Examples
// -----------------------------------------------------------------------------

// Enum for testing
enum Color { Red, Green, Blue };

// Function to demonstrate type concepts
void testTypeConcepts() {
    std::cout << "=== Type Concepts Tests ===" << std::endl;

    // Test Arithmetic
    printConceptCheck<Arithmetic<int>>("Arithmetic", "int");
    printConceptCheck<Arithmetic<std::string>>("Arithmetic", "std::string");

    // Test Integral
    printConceptCheck<Integral<int>>("Integral", "int");
    printConceptCheck<Integral<float>>("Integral", "float");

    // Test FloatingPoint
    printConceptCheck<FloatingPoint<float>>("FloatingPoint", "float");
    printConceptCheck<FloatingPoint<int>>("FloatingPoint", "int");

    // Test SignedInteger
    printConceptCheck<SignedInteger<int>>("SignedInteger", "int");
    printConceptCheck<SignedInteger<unsigned int>>("SignedInteger",
                                                   "unsigned int");

    // Test UnsignedInteger
    printConceptCheck<UnsignedInteger<unsigned int>>("UnsignedInteger",
                                                     "unsigned int");
    printConceptCheck<UnsignedInteger<int>>("UnsignedInteger", "int");

    // Test Number
    printConceptCheck<Number<float>>("Number", "float");
    printConceptCheck<Number<std::string>>("Number", "std::string");

    // Test ComplexNumber
    std::complex<double> complexNum(1.0, 2.0);
    printConceptCheck<ComplexNumber<decltype(complexNum)>>(
        "ComplexNumber", "std::complex<double>");

    // Test Char
    printConceptCheck<Char<char>>("Char", "char");
    printConceptCheck<Char<int>>("Char", "int");

    // Test WChar
    printConceptCheck<WChar<wchar_t>>("WChar", "wchar_t");

    // Test Char16
    printConceptCheck<Char16<char16_t>>("Char16", "char16_t");

    // Test Char32
    printConceptCheck<Char32<char32_t>>("Char32", "char32_t");

    // Test AnyChar
    printConceptCheck<AnyChar<char>>("AnyChar", "char");
    printConceptCheck<AnyChar<char16_t>>("AnyChar", "char16_t");

    // Test StringType
    printConceptCheck<StringType<std::string>>("StringType", "std::string");
    printConceptCheck<StringType<std::string_view>>("StringType",
                                                    "std::string_view");

    // Test IsBuiltIn
    printConceptCheck<IsBuiltIn<int>>("IsBuiltIn", "int");
    printConceptCheck<IsBuiltIn<std::string>>("IsBuiltIn", "std::string");
    printConceptCheck<IsBuiltIn<Color>>("IsBuiltIn", "Color enum");

    // Test Enum
    printConceptCheck<Enum<Color>>("Enum", "Color");
    printConceptCheck<Enum<int>>("Enum", "int");

    // Test Pointer
    int* ptr = nullptr;
    printConceptCheck<Pointer<decltype(ptr)>>("Pointer", "int*");
    printConceptCheck<Pointer<int>>("Pointer", "int");

    // Test UniquePointer
    std::unique_ptr<int> uniquePtr = std::make_unique<int>(42);
    printConceptCheck<UniquePointer<decltype(uniquePtr)>>(
        "UniquePointer", "std::unique_ptr<int>");

    // Test SharedPointer
    std::shared_ptr<int> sharedPtr = std::make_shared<int>(42);
    printConceptCheck<SharedPointer<decltype(sharedPtr)>>(
        "SharedPointer", "std::shared_ptr<int>");

    // Test WeakPointer
    std::weak_ptr<int> weakPtr = sharedPtr;
    printConceptCheck<WeakPointer<decltype(weakPtr)>>("WeakPointer",
                                                      "std::weak_ptr<int>");

    // Test SmartPointer
    printConceptCheck<SmartPointer<decltype(uniquePtr)>>(
        "SmartPointer", "std::unique_ptr<int>");
    printConceptCheck<SmartPointer<decltype(sharedPtr)>>(
        "SmartPointer", "std::shared_ptr<int>");
    printConceptCheck<SmartPointer<int*>>("SmartPointer", "int*");

    // Test Reference
    int value = 42;
    int& ref = value;
    printConceptCheck<Reference<decltype(ref)>>("Reference", "int&");

    // Test LvalueReference
    printConceptCheck<LvalueReference<decltype(ref)>>("LvalueReference",
                                                      "int&");

    // Test RvalueReference
    auto&& movedValue = std::move(value);  // Use auto instead of explicit type
    printConceptCheck<RvalueReference<decltype(movedValue)>>("RvalueReference",
                                                             "moved int");

    // Test Const
    const int constValue = 42;
    const int& constRef = constValue;
    printConceptCheck<Const<decltype(constRef)>>("Const", "const int&");
    printConceptCheck<Const<decltype(ref)>>("Const", "int&");

    // Test Trivial
    printConceptCheck<Trivial<int>>("Trivial", "int");
    printConceptCheck<Trivial<std::string>>("Trivial", "std::string");

    // Test TriviallyConstructible
    printConceptCheck<TriviallyConstructible<int>>("TriviallyConstructible",
                                                   "int");
    printConceptCheck<TriviallyConstructible<std::string>>(
        "TriviallyConstructible", "std::string");

    // Test TriviallyCopyable
    printConceptCheck<TriviallyCopyable<int>>("TriviallyCopyable", "int");
    printConceptCheck<TriviallyCopyable<std::string>>("TriviallyCopyable",
                                                      "std::string");

    std::cout << std::endl;
}

// -----------------------------------------------------------------------------
// Container Concept Examples
// -----------------------------------------------------------------------------

// Custom container class that satisfies Iterable but not Container
class BasicIterable {
private:
    std::vector<int> data_;

public:
    BasicIterable() : data_{1, 2, 3, 4, 5} {}

    auto begin() { return data_.begin(); }
    auto end() { return data_.end(); }
};

// Custom string-like container
class StringLike {
private:
    std::string data_;

public:
    using value_type = char;

    StringLike(const std::string& s) : data_(s) {}

    void push_back(char c) { data_.push_back(c); }
    size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    auto begin() { return data_.begin(); }
    auto end() { return data_.end(); }
};

// Function to demonstrate container concepts
void testContainerConcepts() {
    std::cout << "=== Container Concepts Tests ===" << std::endl;

    // Test Iterable
    std::vector<int> vec = {1, 2, 3, 4, 5};
    BasicIterable basicIter;

    printConceptCheck<Iterable<decltype(vec)>>("Iterable", "std::vector<int>");
    printConceptCheck<Iterable<BasicIterable>>("Iterable", "BasicIterable");

    // Test Container
    printConceptCheck<Container<decltype(vec)>>("Container",
                                                "std::vector<int>");
    printConceptCheck<Container<BasicIterable>>("Container", "BasicIterable");

    // Test StringContainer
    std::string str = "test";
    StringLike strLike("test");

    printConceptCheck<StringContainer<decltype(str)>>("StringContainer",
                                                      "std::string");
    printConceptCheck<StringContainer<StringLike>>("StringContainer",
                                                   "StringLike");

    // Test NumberContainer
    std::vector<int> numContainer{1, 2, 3};  // Initialize with values

    printConceptCheck<NumberContainer<std::vector<int>>>("NumberContainer",
                                                         "std::vector<int>");

    // Fix template argument issue
    printConceptCheck<NumberContainer<decltype(numContainer)>>(
        "NumberContainer", "numContainer type");

    // Test AssociativeContainer
    std::map<int, std::string> map;

    printConceptCheck<AssociativeContainer<decltype(map)>>(
        "AssociativeContainer", "std::map<int, std::string>");
    printConceptCheck<AssociativeContainer<decltype(vec)>>(
        "AssociativeContainer", "std::vector<int>");

    // Test Iterator
    auto vecIter = vec.begin();

    printConceptCheck<Iterator<decltype(vecIter)>>(
        "Iterator", "std::vector<int>::iterator");

    // Test NotSequenceContainer
    std::set<int> set = {1, 2, 3};

    printConceptCheck<NotSequenceContainer<decltype(set)>>(
        "NotSequenceContainer", "std::set<int>");
    printConceptCheck<NotSequenceContainer<decltype(vec)>>(
        "NotSequenceContainer", "std::vector<int>");

    // Test NotAssociativeOrSequenceContainer
    printConceptCheck<NotAssociativeOrSequenceContainer<decltype(set)>>(
        "NotAssociativeOrSequenceContainer", "std::set<int>");

    // Test String
    printConceptCheck<String<std::string>>("String", "std::string");
    printConceptCheck<String<decltype(vec)>>("String", "std::vector<int>");

    std::cout << std::endl;
}

// -----------------------------------------------------------------------------
// Multi-threading Concept Examples
// -----------------------------------------------------------------------------

// Custom lockable class
class SimpleLock {
private:
    bool locked_ = false;

public:
    void lock() { locked_ = true; }
    void unlock() { locked_ = false; }
};

// Custom shared lockable class
class SimpleSharedLock {
private:
    int readers_ = 0;
    bool writer_locked_ = false;

public:
    void lock() { writer_locked_ = true; }
    void unlock() { writer_locked_ = false; }
    void lock_shared() { readers_++; }
    void unlock_shared() { readers_--; }
};

// Custom mutex class
class SimpleMutex : public SimpleLock {
public:
    bool try_lock() { return !locked_; }

private:
    bool locked_ = false;
};

// Custom shared mutex class
class SimpleSharedMutex : public SimpleSharedLock {
public:
    bool try_lock() { return !writer_locked_; }
    bool try_lock_shared() { return !writer_locked_; }

private:
    bool writer_locked_ = false;
};

// Function to demonstrate multi-threading concepts
void testMultiThreadingConcepts() {
    std::cout << "=== Multi-threading Concepts Tests ===" << std::endl;

    // Test Lockable
    SimpleLock simpleLock;
    simpleLock.lock();  // Actually use simpleLock
    simpleLock.unlock();
    printConceptCheck<Lockable<SimpleLock>>("Lockable", "SimpleLock");

    std::mutex stdMutex;
    std::lock_guard<std::mutex> guard(stdMutex);  // Actually use stdMutex
    printConceptCheck<Lockable<std::mutex>>("Lockable", "std::mutex");

    // Test SharedLockable
    SimpleSharedLock simpleSharedLock;
    simpleSharedLock.lock_shared();  // Actually use simpleSharedLock
    simpleSharedLock.unlock_shared();
    printConceptCheck<SharedLockable<SimpleSharedLock>>("SharedLockable",
                                                        "SimpleSharedLock");
    std::shared_mutex stdSharedMutex;
    std::shared_lock<std::shared_mutex> sharedGuard(
        stdSharedMutex);  // Actually use stdSharedMutex
    printConceptCheck<SharedLockable<std::shared_mutex>>("SharedLockable",
                                                         "std::shared_mutex");

    // Test Mutex
    SimpleMutex simpleMutex;
    simpleMutex.try_lock();  // Actually use simpleMutex
    simpleMutex.unlock();
    printConceptCheck<Mutex<SimpleMutex>>("Mutex", "SimpleMutex");
    printConceptCheck<Mutex<std::mutex>>("Mutex", "std::mutex");

    // Test SharedMutex
    SimpleSharedMutex simpleSharedMutex;
    simpleSharedMutex.try_lock_shared();  // Actually use simpleSharedMutex
    simpleSharedMutex.unlock_shared();
    printConceptCheck<SharedMutex<SimpleSharedMutex>>("SharedMutex",
                                                      "SimpleSharedMutex");
    printConceptCheck<SharedMutex<std::shared_mutex>>("SharedMutex",
                                                      "std::shared_mutex");

    std::cout << std::endl;
}

// -----------------------------------------------------------------------------
// Asynchronous Concept Examples
// -----------------------------------------------------------------------------

// Custom future-like class
template <typename T>
class SimpleFuture {
private:
    T value_;

public:
    using value_type = T;

    SimpleFuture(T val) : value_(val) {}

    T get() { return value_; }
    void wait() {}
};

// Custom promise-like class
template <typename T>
class SimplePromise {
private:
    T value_;

public:
    using value_type = T;

    void set_value(T val) { value_ = val; }
    void set_exception(std::exception_ptr) {}
};

// Function to demonstrate asynchronous concepts
void testAsynchronousConcepts() {
    std::cout << "=== Asynchronous Concepts Tests ===" << std::endl;

    // Test Future
    std::future<int> stdFuture =
        std::async(std::launch::deferred, [] { return 42; });
    SimpleFuture<int> simpleFuture(42);

    printConceptCheck<Future<decltype(stdFuture)>>("Future",
                                                   "std::future<int>");
    printConceptCheck<Future<SimpleFuture<int>>>("Future", "SimpleFuture<int>");

    // Test Promise
    std::promise<int> stdPromise;
    SimplePromise<int> simplePromise;
    simplePromise.set_value(42);  // Actually use simplePromise

    printConceptCheck<Promise<decltype(stdPromise)>>("Promise",
                                                     "std::promise<int>");
    printConceptCheck<Promise<SimplePromise<int>>>("Promise",
                                                   "SimplePromise<int>");

    // Test AsyncResult
    printConceptCheck<AsyncResult<decltype(stdFuture)>>("AsyncResult",
                                                        "std::future<int>");
    printConceptCheck<AsyncResult<decltype(stdPromise)>>("AsyncResult",
                                                         "std::promise<int>");

    std::cout << std::endl;
}

// -----------------------------------------------------------------------------
// Practical Examples
// -----------------------------------------------------------------------------

// Generic function that works only on arithmetic types
template <typename T>
    requires Arithmetic<T>
T average(const std::vector<T>& values) {
    if (values.empty())
        return T{};
    return std::accumulate(values.begin(), values.end(), T{}) /
           static_cast<T>(values.size());
}

// Function that requires a container with iterators
template <Container T>
auto findMax(const T& container) {
    if (container.begin() == container.end()) {
        throw std::runtime_error("Empty container");
    }
    return *std::max_element(container.begin(), container.end());
}

// Function that requires a callable returning a specific type
template <typename Func, typename... Args>
    requires CallableReturns<Func, int, Args...>
int safeCall(Func&& func, Args&&... args) {
    try {
        return std::invoke(std::forward<Func>(func),
                           std::forward<Args>(args)...);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
}

// Function that works only on smart pointers
template <typename T>
    requires SmartPointer<T>
void useResource(T ptr) {
    if (ptr) {
        std::cout << "Resource is valid" << std::endl;
    } else {
        std::cout << "Resource is not valid" << std::endl;
    }
}

// Function to demonstrate practical usage of concepts
void testPracticalExamples() {
    std::cout << "=== Practical Examples ===" << std::endl;

    // Test average function with arithmetic types
    std::vector<int> intValues = {1, 2, 3, 4, 5};
    std::vector<double> doubleValues = {1.5, 2.5, 3.5};

    std::cout << "Int average: " << average(intValues) << std::endl;
    std::cout << "Double average: " << average(doubleValues) << std::endl;

    // Uncomment to see compilation error
    // std::vector<std::string> stringValues = {"a", "b", "c"};
    // average(stringValues);  // Error: 'average' requires arithmetic type

    // Test findMax function with containers
    std::cout << "Max int value: " << findMax(intValues) << std::endl;
    std::cout << "Max double value: " << findMax(doubleValues) << std::endl;

    std::map<int, std::string> testMap = {{1, "one"}, {2, "two"}, {3, "three"}};
    try {
        auto maxPair = findMax(testMap);
        std::cout << "Max map key: " << maxPair.first << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    // Test safeCall with callable returning int
    auto safeResult = safeCall(add, 5, 7);
    std::cout << "Safe call result: " << safeResult << std::endl;

    // Test useResource with smart pointers
    auto uniquePtr = std::make_unique<int>(42);
    auto sharedPtr = std::make_shared<int>(100);
    std::weak_ptr<int> weakPtr = sharedPtr;

    useResource(std::move(uniquePtr));  // Now uniquePtr is null
    useResource(sharedPtr);             // Still valid

    // Convert weak_ptr to shared_ptr before using
    if (auto lockedPtr = weakPtr.lock()) {
        useResource(lockedPtr);
    }

    // Uncomment to see compilation error
    // int* rawPtr = new int(200);
    // useResource(rawPtr);  // Error: raw pointer does not satisfy SmartPointer
    // concept delete rawPtr;

    std::cout << std::endl;
}

// Main function to run all examples
int main() {
    std::cout << "======================================================="
              << std::endl;
    std::cout << "   Concept Utilities Comprehensive Examples            "
              << std::endl;
    std::cout << "======================================================="
              << std::endl
              << std::endl;

    testFunctionConcepts();
    testObjectConcepts();
    testTypeConcepts();
    testContainerConcepts();
    testMultiThreadingConcepts();
    testAsynchronousConcepts();
    testPracticalExamples();

    std::cout << "All examples completed successfully!" << std::endl;
    return 0;
}
