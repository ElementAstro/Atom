#include <gtest/gtest.h>
#include "atom/function/bind_first.hpp"

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace atom::meta;

// Test fixture class
class BindFirstTest : public ::testing::Test {
protected:
    // Example class for testing member functions
    struct TestClass {
        int value = 0;

        int addValue(int x) {
            value += x;
            return value;
        }

        int getValue() const { return value; }

        std::string format(const std::string& prefix, int number) const {
            return prefix + std::to_string(number) + std::to_string(value);
        }

        void modifyValue(int newValue) { value = newValue; }

        // Function that throws an exception for testing exception handling
        void throwError() { throw std::runtime_error("Test exception"); }
    };

    // Example functions for testing function pointer binding
    static int addNumbers(int a, int b) { return a + b; }

    static std::string joinStrings(const std::string& str1,
                                   const std::string& str2) {
        return str1 + str2;
    }

    static void modifyByRef(int& value, int delta) { value += delta; }
};

// Test binding a free function
TEST_F(BindFirstTest, BindFreeFunctionPointer) {
    auto boundAdd = bindFirst(&addNumbers, 5);
    EXPECT_EQ(boundAdd(10), 15);

    // Test with a different value
    auto boundAdd2 = bindFirst(&addNumbers, 7);
    EXPECT_EQ(boundAdd2(3), 10);

    // Test with string function
    auto boundJoin = bindFirst(&joinStrings, std::string("Hello, "));
    EXPECT_EQ(boundJoin("World!"), "Hello, World!");
}

// Test binding a member function to an object
TEST_F(BindFirstTest, BindMemberFunction) {
    TestClass obj;
    obj.value = 10;

    auto boundAdd = bindFirst(&TestClass::addValue, obj);
    EXPECT_EQ(boundAdd(5), 15);

    // The original object should not be modified (copy semantics)
    EXPECT_EQ(obj.value, 10);
}

// Test binding a member function to an object reference
TEST_F(BindFirstTest, BindMemberFunctionRef) {
    TestClass obj;
    obj.value = 10;

    auto boundAdd = bindFirst(&TestClass::addValue, std::ref(obj));
    EXPECT_EQ(boundAdd(5), 15);

    // The original object should be modified (reference semantics)
    EXPECT_EQ(obj.value, 15);
}

// Test binding a const member function
TEST_F(BindFirstTest, BindConstMemberFunction) {
    TestClass obj;
    obj.value = 25;

    auto boundGet = bindFirst(&TestClass::getValue, obj);
    EXPECT_EQ(boundGet(), 25);

    obj.value = 30;  // Change the original object
    EXPECT_EQ(boundGet(),
              25);  // Should still return the original value (copy semantics)

    // Now with reference
    auto boundGetRef = bindFirst(&TestClass::getValue, std::ref(obj));
    EXPECT_EQ(boundGetRef(), 30);

    obj.value = 40;
    EXPECT_EQ(boundGetRef(),
              40);  // Should reflect the new value (reference semantics)
}

// Test binding a member function with multiple parameters
TEST_F(BindFirstTest, BindMemberFunctionMultipleParams) {
    TestClass obj;
    obj.value = 42;

    auto boundFormat = bindFirst(&TestClass::format, obj);
    EXPECT_EQ(boundFormat("Test-", 123), "Test-12342");
}

// Test binding to a raw pointer
TEST_F(BindFirstTest, BindToPointer) {
    auto obj = new TestClass();
    obj->value = 15;

    auto boundAdd = bindFirst(&TestClass::addValue, obj);
    EXPECT_EQ(boundAdd(5), 20);

    // The object should be modified (reference semantics)
    EXPECT_EQ(obj->value, 20);

    delete obj;
}

// Test binding to a std::shared_ptr
TEST_F(BindFirstTest, BindToSharedPtr) {
    auto obj = std::make_shared<TestClass>();
    obj->value = 30;

    auto boundAdd = bindFirst(&TestClass::addValue, *obj);
    EXPECT_EQ(boundAdd(10), 40);

    // The object should be modified
    EXPECT_EQ(obj->value, 40);
}

// Test binding to a std::unique_ptr (by reference)
TEST_F(BindFirstTest, BindToUniquePtr) {
    auto obj = std::make_unique<TestClass>();
    obj->value = 25;

    auto boundAdd = bindFirst(&TestClass::addValue, std::ref(*obj));
    EXPECT_EQ(boundAdd(5), 30);

    // The object should be modified
    EXPECT_EQ(obj->value, 30);
}

// Test binding a function object (lambda)
TEST_F(BindFirstTest, BindFunctionObject) {
    auto lambda = [](int x, int y) { return x * y; };

    auto boundLambda = bindFirst(lambda, 10);
    EXPECT_EQ(boundLambda(5), 50);

    // Test with another type of function object
    struct Multiplier {
        int operator()(int x, int y) const { return x * y; }
    };

    Multiplier mult;
    auto boundMult = bindFirst(mult, 4);
    EXPECT_EQ(boundMult(7), 28);
}

// Test binding a std::function
TEST_F(BindFirstTest, BindStdFunction) {
    std::function<int(int, int)> add = [](int a, int b) { return a + b; };

    auto boundAdd = bindFirst(add, 10);
    EXPECT_EQ(boundAdd(5), 15);
}

// Test binding a class member variable
TEST_F(BindFirstTest, BindMember) {
    TestClass obj;
    obj.value = 42;

    auto valueBinder = bindMember(&TestClass::value, obj);
    EXPECT_EQ(valueBinder(), 42);

    // With reference, we can modify the member
    TestClass objRef;
    objRef.value = 55;

    auto valueRefBinder = bindMember(&TestClass::value, std::ref(objRef));
    EXPECT_EQ(valueRefBinder(), 55);

    // Test modification
    valueRefBinder() = 70;
    EXPECT_EQ(objRef.value, 70);
}

// Test binding a static function
TEST_F(BindFirstTest, BindStaticFunction) {
    auto staticBound = bindStatic(&BindFirstTest::addNumbers);
    EXPECT_EQ(staticBound(10, 20), 30);

    auto staticJoin = bindStatic(&BindFirstTest::joinStrings);
    EXPECT_EQ(staticJoin("Hello ", "World"), "Hello World");
}

// Test async binding
TEST_F(BindFirstTest, AsyncBindFirst) {
    TestClass obj;
    obj.value = 5;

    auto future = asyncBindFirst(&TestClass::addValue, std::ref(obj), 10);
    EXPECT_EQ(future.get(), 15);
    EXPECT_EQ(obj.value, 15);
}

// Test exception handling wrapper
TEST_F(BindFirstTest, ExceptionHandling) {
    TestClass obj;

    auto boundWithException = bindFirstWithExceptionHandling(
        &TestClass::throwError, obj, "Custom context");

    try {
        boundWithException();
        FAIL() << "Expected BindingException to be thrown";
    } catch (const BindingException& e) {
        std::string error = e.what();
        EXPECT_TRUE(error.find("Custom context") != std::string::npos);
        EXPECT_TRUE(error.find("Test exception") != std::string::npos);
        // Should also contain source location information
        EXPECT_TRUE(error.find("at ") != std::string::npos);
    } catch (...) {
        FAIL() << "Expected BindingException but got a different exception";
    }
}

// Test thread-safe binding
TEST_F(BindFirstTest, ThreadSafeBinding) {
    auto obj = std::make_shared<TestClass>();
    obj->value = 0;

    auto boundModify = bindFirstThreadSafe(&TestClass::modifyValue, obj);

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&boundModify, i]() { boundModify(i * 10); });
    }

    for (auto& t : threads) {
        t.join();
    }

    // The last thread's value should be set
    EXPECT_EQ(obj->value % 10, 0);  // Last digit should be 0
    EXPECT_GE(obj->value, 0);
    EXPECT_LE(obj->value, 90);
}

// Test binding with by-value modification
TEST_F(BindFirstTest, ByValueModification) {
    TestClass obj;
    obj.value = 10;

    auto boundAdd = bindFirst(&TestClass::addValue, obj);
    EXPECT_EQ(boundAdd(5), 15);  // This returns obj.value + 5
    EXPECT_EQ(boundAdd(5), 20);  // This returns obj.value + 5 + 5

    // Original object should be unaffected
    EXPECT_EQ(obj.value, 10);
}

// Test binding with by-reference modification
TEST_F(BindFirstTest, ByReferenceModification) {
    int value = 10;

    auto boundModify = bindFirst(&BindFirstTest::modifyByRef, std::ref(value));
    boundModify(5);
    EXPECT_EQ(value, 15);

    boundModify(10);
    EXPECT_EQ(value, 25);
}

// Test universal reference binding
TEST_F(BindFirstTest, UniversalReferenceBinding) {
    auto add = [](int a, int b) { return a + b; };

    // Bind with rvalue reference
    auto boundAdd = bindFirst(std::move(add), 10);
    EXPECT_EQ(boundAdd(5), 15);

    // Complex test with perfect forwarding
    auto process = [](std::string& s, const char* suffix) {
        s += suffix;
        return s;
    };

    std::string base = "Hello";
    auto boundProcess = bindFirst(process, std::ref(base));

    EXPECT_EQ(boundProcess(", World"), "Hello, World");
    EXPECT_EQ(base, "Hello, World");  // base is modified

    EXPECT_EQ(boundProcess("!"), "Hello, World!");
    EXPECT_EQ(base, "Hello, World!");  // base is modified again
}

// Test with nested bindings
TEST_F(BindFirstTest, NestedBindings) {
    TestClass obj;
    obj.value = 5;

    // First binding: bind the object to addValue
    auto boundAdd = bindFirst(&TestClass::addValue, std::ref(obj));

    // Second binding: bind the result to a function
    auto processResult = [](int result, int multiplier) {
        return result * multiplier;
    };

    auto processAdd =
        bindFirst(processResult, boundAdd(10));  // Will be 15 (5+10)
    EXPECT_EQ(processAdd(2), 30);                // 15 * 2
    EXPECT_EQ(obj.value, 15);  // obj.value was modified by the first binding
}

// Test with various parameter types
TEST_F(BindFirstTest, VariousParameterTypes) {
    auto func = [](const std::string& str, int num, double d, bool flag) {
        return str + (flag ? " Yes " : " No ") + std::to_string(num) + " " +
               std::to_string(d);
    };

    auto boundFunc = bindFirst(func, std::string("Test:"));
    EXPECT_EQ(boundFunc(42, 3.14, true), "Test: Yes 42 3.140000");
    EXPECT_EQ(boundFunc(100, 2.718, false), "Test: No 100 2.718000");
}

#if __cpp_lib_coroutine
// Test coroutine support if available
#include <coroutine>

// Simple coroutine task type for testing
struct Task {
    struct promise_type {
        int value;

        Task get_return_object() {
            return Task{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        auto initial_suspend() { return std::suspend_never{}; }
        auto final_suspend() noexcept { return std::suspend_always{}; }
        void unhandled_exception() { std::terminate(); }
        void return_value(int v) { value = v; }
    };

    std::coroutine_handle<promise_type> handle;

    ~Task() {
        if (handle)
            handle.destroy();
    }

    int get_result() { return handle.promise().value; }
};

Task test_coroutine() {
    TestClass obj;
    obj.value = 10;

    auto addTen = bindFirst(&TestClass::addValue, obj);
    auto awaitable = makeAwaitable(addTen, 5);

    int result = co_await awaitable;
    co_return result;
}

TEST_F(BindFirstTest, CoroutineSupport) {
    auto task = test_coroutine();
    EXPECT_EQ(task.get_result(), 15);  // 10 + 5
}
#endif
