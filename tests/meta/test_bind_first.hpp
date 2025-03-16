// filepath: /home/max/Atom-1/atom/meta/test_bind_first.hpp
// atom/meta/test_bind_first.hpp
#ifndef ATOM_TEST_BIND_FIRST_HPP
#define ATOM_TEST_BIND_FIRST_HPP

#include <gtest/gtest.h>
#include "atom/meta/bind_first.hpp"

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace atom::test {

// Free functions for testing
int add(int a, int b) { return a + b; }
std::string concatenate(const std::string& prefix, const std::string& suffix) {
    return prefix + suffix;
}
void incrementCounter(int& counter) { counter++; }
int multiply(int a, int b, int c) { return a * b * c; }
int throwingFunction(int value) {
    if (value < 0) {
        throw std::runtime_error("Negative value not allowed");
    }
    return value * 2;
}

// Test classes
class TestClass {
public:
    int value{0};

    // Non-const member functions
    int addToValue(int amount) {
        value += amount;
        return value;
    }

    // Const member functions
    int getValue() const { return value; }
    int multiplyBy(int factor) const { return value * factor; }

    // Member function that throws
    int divideBy(int divisor) const {
        if (divisor == 0) {
            throw std::runtime_error("Division by zero");
        }
        return value / divisor;
    }

    // Member variables for testing bindMember
    int memberVar{42};
    std::string name{"TestClass"};
};

// Function object class
class Adder {
public:
    int operator()(int a, int b) const { return a + b; }
};

// Test fixture
class BindFirstTest : public ::testing::Test {
protected:
    TestClass testObj;
    std::shared_ptr<TestClass> testObjPtr = std::make_shared<TestClass>();

    void SetUp() override {
        testObj.value = 10;
        testObjPtr->value = 10;
    }
};

// Test binding function pointers
TEST_F(BindFirstTest, BindFunctionPointer) {
    // Bind the first parameter of the add function
    auto add5 = meta::bindFirst(add, 5);
    EXPECT_EQ(add5(10), 15);
    EXPECT_EQ(add5(20), 25);

    // Test with string concatenation
    auto prefixHello = meta::bindFirst(concatenate, std::string("Hello, "));
    EXPECT_EQ(prefixHello("world"), "Hello, world");
    EXPECT_EQ(prefixHello("C++"), "Hello, C++");
}

// Test binding to non-const member functions
TEST_F(BindFirstTest, BindNonConstMemberFunction) {
    // Bind object to member function
    auto addToTestObj = meta::bindFirst(&TestClass::addToValue, testObj);

    // Call the bound function
    EXPECT_EQ(addToTestObj(5), 15);  // value becomes 15
    EXPECT_EQ(addToTestObj(3), 18);  // value becomes 18

    // Verify the original object was modified
    EXPECT_EQ(testObj.value, 18);
}

// Test binding to const member functions
TEST_F(BindFirstTest, BindConstMemberFunction) {
    // Bind object to const member function
    auto getTestObjValue = meta::bindFirst(&TestClass::getValue, testObj);
    auto multiplyTestObjBy = meta::bindFirst(&TestClass::multiplyBy, testObj);

    // Call the bound functions
    EXPECT_EQ(getTestObjValue(), 10);
    EXPECT_EQ(multiplyTestObjBy(3), 30);

    // Verify the original object was not modified
    EXPECT_EQ(testObj.value, 10);
}

// Test binding with std::reference_wrapper
TEST_F(BindFirstTest, BindWithReferenceWrapper) {
    // Create a reference wrapper to the test object
    std::reference_wrapper<TestClass> testObjRef(testObj);

    // Bind the reference wrapper to a member function
    auto addToTestObjRef = meta::bindFirst(&TestClass::addToValue, testObjRef);

    // Call the bound function
    EXPECT_EQ(addToTestObjRef(5), 15);  // value becomes 15

    // Verify the original object was modified
    EXPECT_EQ(testObj.value, 15);
}

// Test binding with std::function
TEST_F(BindFirstTest, BindWithStdFunction) {
    // Create a std::function
    std::function<int(int, int)> addFunc = add;

    // Bind the first parameter
    auto add5 = meta::bindFirst(addFunc, 5);

    // Call the bound function
    EXPECT_EQ(add5(10), 15);
    EXPECT_EQ(add5(20), 25);
}

// Test binding with function objects
TEST_F(BindFirstTest, BindWithFunctionObject) {
    // Create a function object
    Adder adder;

    // Bind the first parameter
    auto add5 = meta::bindFirst(adder, 5);

    // Call the bound function
    EXPECT_EQ(add5(10), 15);
    EXPECT_EQ(add5(20), 25);
}

// Test binding with lambdas
TEST_F(BindFirstTest, BindWithLambda) {
    // Create a lambda
    auto multiplyLambda = [](int x, int y) { return x * y; };

    // Bind the first parameter
    auto multiplyBy10 = meta::bindFirst(multiplyLambda, 10);

    // Call the bound function
    EXPECT_EQ(multiplyBy10(5), 50);
    EXPECT_EQ(multiplyBy10(7), 70);
}

// Test binding member variables
TEST_F(BindFirstTest, BindMemberVariable) {
    // Bind to member variable
    auto getMemberVar = meta::bindMember(&TestClass::memberVar, testObj);
    auto getName = meta::bindMember(&TestClass::name, testObj);

    // Read the bound member variables
    EXPECT_EQ(getMemberVar(), 42);
    EXPECT_EQ(getName(), "TestClass");

    // Modify the bound member variables
    getMemberVar() = 100;
    getName() = "Modified";

    // Verify the changes
    EXPECT_EQ(testObj.memberVar, 100);
    EXPECT_EQ(testObj.name, "Modified");
}

// Test binding static functions
TEST_F(BindFirstTest, BindStaticFunction) {
    // Bind a static function
    auto boundAdd = meta::bindStatic(add);

    // Call the bound function
    EXPECT_EQ(boundAdd(5, 10), 15);
    EXPECT_EQ(boundAdd(20, 30), 50);
}

// Test async binding
TEST_F(BindFirstTest, AsyncBinding) {
    // Call a function asynchronously
    auto future = meta::asyncBindFirst(add, 10, 20);

    // Wait for the result
    EXPECT_EQ(future.get(), 30);
}

// Test binding with shared_ptr thread-safety
TEST_F(BindFirstTest, ThreadSafeBinding) {
    // Bind member function to shared_ptr
    auto threadSafeAdd =
        meta::bindFirstThreadSafe(&TestClass::addToValue, testObjPtr);

    // Create multiple threads to call the bound function
    std::vector<std::thread> threads;
    std::vector<int> results(5);

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&threadSafeAdd, &results, i]() {
            results[i] = threadSafeAdd(1);  // Each thread adds 1
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // The final value should reflect all additions
    EXPECT_EQ(testObjPtr->value, 15);  // 10 + 5*1 = 15

    // Results should be sequential increments (though the exact order is not
    // guaranteed)
    std::sort(results.begin(), results.end());
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(results[i], 11 + i);  // 11, 12, 13, 14, 15
    }
}

// Test exception handling in bound functions
TEST_F(BindFirstTest, ExceptionHandling) {
    // Create a function that might throw
    auto divideFunc = meta::bindFirst(&TestClass::divideBy, testObj);

    // Test with valid input
    EXPECT_EQ(divideFunc(2), 5);  // 10/2 = 5

    // Test with input that causes exception
    EXPECT_THROW(divideFunc(0), std::runtime_error);

    // Test with bindFirstWithExceptionHandling
    auto safeDivide = meta::bindFirstWithExceptionHandling(
        &TestClass::divideBy, testObj, "Division operation");

    // Test with valid input
    EXPECT_EQ(safeDivide(2), 5);

    // Test with input that causes exception - should be wrapped
    try {
        safeDivide(0);
        FAIL() << "Expected meta::BindingException";
    } catch (const meta::BindingException& e) {
        std::string errorMsg = e.what();
        EXPECT_TRUE(errorMsg.find("Division operation") != std::string::npos);
        EXPECT_TRUE(errorMsg.find("Division by zero") != std::string::npos);
    }
}

// Test binding with perfect forwarding
TEST_F(BindFirstTest, PerfectForwarding) {
    // Create a function that accepts various parameter types
    auto forwardingFunc = [](const std::string& str, int val, double d) {
        return str + "-" + std::to_string(val) + "-" + std::to_string(d);
    };

    // Bind the first parameter
    auto boundFunc = meta::bindFirst(forwardingFunc, std::string("test"));

    // Call with remaining parameters
    EXPECT_EQ(boundFunc(42, 3.14), "test-42-3.140000");
}

// Test binding with temporary objects
TEST_F(BindFirstTest, BindingTemporaries) {
    // Bind with a temporary string
    auto prefixTemp = meta::bindFirst(concatenate, std::string("Temp: "));

    // Call the bound function
    EXPECT_EQ(prefixTemp("value"), "Temp: value");
}

// Test BindingFunctor utility
TEST_F(BindFirstTest, BindingFunctor) {
    // Create a BindingFunctor for the add function
    meta::BindingFunctor<int (*)(int, int)> functor{add};

    // Call the functor
    EXPECT_EQ(functor(5, 10), 15);
}

// Test with larger function signatures
TEST_F(BindFirstTest, LargerFunctionSignatures) {
    // Create a function with multiple parameters
    auto boundMultiply = meta::bindFirst(multiply, 2);

    // Call the bound function
    EXPECT_EQ(boundMultiply(3, 4), 24);  // 2 * 3 * 4 = 24
}

// Test binding to mutable lambdas
TEST_F(BindFirstTest, MutableLambdas) {
    int counter = 0;

    // Create a mutable lambda that captures by reference
    auto incrementLambda = [&counter](int step, int multiplier) mutable {
        counter += step * multiplier;
        return counter;
    };

    // Bind the lambda with first parameter
    auto incrementBy5 = meta::bindFirst(incrementLambda, 5);

    // Call the bound function
    EXPECT_EQ(incrementBy5(2), 10);  // 0 + 5*2 = 10
    EXPECT_EQ(incrementBy5(3), 25);  // 10 + 5*3 = 25

    // Verify the counter was updated
    EXPECT_EQ(counter, 25);
}

#if __cpp_lib_coroutine
// Only compile these tests if coroutines are supported
// Test awaitable creation
TEST_F(BindFirstTest, AwaitableCreation) {
    // Since we can't use co_await in a regular function,
    // we'll just test that the awaitable is created correctly
    auto func = [](int val) { return val * 2; };

    // Create an awaitable
    auto awaitable = meta::makeAwaitable(func, 5);

    // Check that it has the expected methods
    EXPECT_FALSE(awaitable.await_ready());

    // Check the result
    EXPECT_EQ(awaitable.await_resume(), 10);
}
#endif

}  // namespace atom::test

#endif  // ATOM_TEST_BIND_FIRST_HPP