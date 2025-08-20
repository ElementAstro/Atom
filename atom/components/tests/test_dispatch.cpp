/*
 * test_dispatch.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Comprehensive Unit Tests for Dispatch System
Tests function registration, argument conversion, return value handling,
error propagation, and performance optimization.

**************************************************/

#include <gtest/gtest.h>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "../component.hpp"
#include "../dispatch.hpp"

using namespace atom::components;

/**
 * @brief Test fixture for Dispatch system tests
 */
class DispatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        dispatcher_ = std::make_unique<CommandDispatcher>();
    }

    void TearDown() override { dispatcher_.reset(); }

    std::unique_ptr<CommandDispatcher> dispatcher_;
};

/**
 * @brief Test fixture for performance benchmarks
 */
class DispatchPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        dispatcher_ = std::make_unique<CommandDispatcher>();
    }

    void TearDown() override { dispatcher_.reset(); }

    std::unique_ptr<CommandDispatcher> dispatcher_;
    static constexpr int BENCHMARK_ITERATIONS = 1000;
};

// ============================================================================
// Basic Function Registration Tests
// ============================================================================

TEST_F(DispatchTest, RegisterSimpleFunction) {
    auto simpleFunc = []() -> int { return 42; };

    dispatcher_->registerFunction("simpleFunc", simpleFunc);
    EXPECT_TRUE(dispatcher_->hasFunction("simpleFunc"));

    auto result = dispatcher_->dispatch("simpleFunc");
    EXPECT_EQ(std::any_cast<int>(result), 42);
}

TEST_F(DispatchTest, RegisterFunctionWithArguments) {
    auto addFunc = [](int a, int b) -> int { return a + b; };

    dispatcher_->registerFunction("add", addFunc);
    EXPECT_TRUE(dispatcher_->hasFunction("add"));

    auto result = dispatcher_->dispatch("add", 5, 3);
    EXPECT_EQ(std::any_cast<int>(result), 8);
}

TEST_F(DispatchTest, RegisterFunctionWithMultipleTypes) {
    auto mixedFunc = [](int i, double d, const std::string& s) -> std::string {
        return s + "_" + std::to_string(i) + "_" + std::to_string(d);
    };

    dispatcher_->registerFunction("mixed", mixedFunc);

    auto result = dispatcher_->dispatch("mixed", 42, 3.14, std::string("test"));
    EXPECT_EQ(std::any_cast<std::string>(result), "test_42_3.140000");
}

TEST_F(DispatchTest, RegisterVoidFunction) {
    bool called = false;
    auto voidFunc = [&called]() { called = true; };

    dispatcher_->registerFunction("voidFunc", voidFunc);

    auto result = dispatcher_->dispatch("voidFunc");
    EXPECT_TRUE(called);
    // Void functions should return empty std::any or some default value
}

// ============================================================================
// Member Function Registration Tests
// ============================================================================

TEST_F(DispatchTest, RegisterMemberFunction) {
    struct TestClass {
        int value = 10;
        int getValue() const { return value; }
        void setValue(int v) { value = v; }
        int multiply(int factor) { return value * factor; }
    };

    auto testObj = std::make_shared<TestClass>();

    dispatcher_->registerMemberFunction("getValue", &TestClass::getValue,
                                        testObj);
    dispatcher_->registerMemberFunction("setValue", &TestClass::setValue,
                                        testObj);
    dispatcher_->registerMemberFunction("multiply", &TestClass::multiply,
                                        testObj);

    // Test getter
    auto result = dispatcher_->dispatch("getValue");
    EXPECT_EQ(std::any_cast<int>(result), 10);

    // Test setter
    dispatcher_->dispatch("setValue", 20);
    result = dispatcher_->dispatch("getValue");
    EXPECT_EQ(std::any_cast<int>(result), 20);

    // Test member function with parameters
    result = dispatcher_->dispatch("multiply", 3);
    EXPECT_EQ(std::any_cast<int>(result), 60);
}

TEST_F(DispatchTest, RegisterConstMemberFunction) {
    struct TestClass {
        int value = 42;
        int getConstValue() const { return value; }
        std::string getConstString() const { return "const"; }
    };

    auto testObj = std::make_shared<TestClass>();

    dispatcher_->registerMemberFunction("getConstValue",
                                        &TestClass::getConstValue, testObj);
    dispatcher_->registerMemberFunction("getConstString",
                                        &TestClass::getConstString, testObj);

    auto intResult = dispatcher_->dispatch("getConstValue");
    EXPECT_EQ(std::any_cast<int>(intResult), 42);

    auto stringResult = dispatcher_->dispatch("getConstString");
    EXPECT_EQ(std::any_cast<std::string>(stringResult), "const");
}

// ============================================================================
// Argument Conversion Tests
// ============================================================================

TEST_F(DispatchTest, ArgumentTypeConversion) {
    auto convertFunc = [](double d) -> int { return static_cast<int>(d); };

    dispatcher_->registerFunction("convert", convertFunc);

    // Pass int, should convert to double
    auto result = dispatcher_->dispatch("convert", 42);
    EXPECT_EQ(std::any_cast<int>(result), 42);

    // Pass double
    result = dispatcher_->dispatch("convert", 3.14);
    EXPECT_EQ(std::any_cast<int>(result), 3);
}

TEST_F(DispatchTest, StringArgumentConversion) {
    auto stringFunc = [](const std::string& s) -> size_t { return s.length(); };

    dispatcher_->registerFunction("stringLength", stringFunc);

    // Pass string literal
    auto result = dispatcher_->dispatch("stringLength", std::string("hello"));
    EXPECT_EQ(std::any_cast<size_t>(result), 5);

    // Pass const char*
    result = dispatcher_->dispatch("stringLength", "world");
    EXPECT_EQ(std::any_cast<size_t>(result), 5);
}

// ============================================================================
// Return Value Handling Tests
// ============================================================================

TEST_F(DispatchTest, DifferentReturnTypes) {
    dispatcher_->registerFunction("returnInt", []() -> int { return 42; });
    dispatcher_->registerFunction("returnDouble",
                                  []() -> double { return 3.14; });
    dispatcher_->registerFunction("returnString",
                                  []() -> std::string { return "hello"; });
    dispatcher_->registerFunction("returnBool", []() -> bool { return true; });

    auto intResult = dispatcher_->dispatch("returnInt");
    EXPECT_EQ(std::any_cast<int>(intResult), 42);

    auto doubleResult = dispatcher_->dispatch("returnDouble");
    EXPECT_DOUBLE_EQ(std::any_cast<double>(doubleResult), 3.14);

    auto stringResult = dispatcher_->dispatch("returnString");
    EXPECT_EQ(std::any_cast<std::string>(stringResult), "hello");

    auto boolResult = dispatcher_->dispatch("returnBool");
    EXPECT_EQ(std::any_cast<bool>(boolResult), true);
}

TEST_F(DispatchTest, ComplexReturnTypes) {
    auto vectorFunc = []() -> std::vector<int> { return {1, 2, 3, 4, 5}; };

    dispatcher_->registerFunction("returnVector", vectorFunc);

    auto result = dispatcher_->dispatch("returnVector");
    auto vector = std::any_cast<std::vector<int>>(result);

    EXPECT_EQ(vector.size(), 5);
    EXPECT_EQ(vector[0], 1);
    EXPECT_EQ(vector[4], 5);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(DispatchTest, DispatchNonExistentFunction) {
    EXPECT_FALSE(dispatcher_->hasFunction("nonExistent"));
    EXPECT_THROW(dispatcher_->dispatch("nonExistent"), std::exception);
}

TEST_F(DispatchTest, DispatchWithWrongArgumentCount) {
    auto twoArgFunc = [](int a, int b) -> int { return a + b; };
    dispatcher_->registerFunction("twoArgs", twoArgFunc);

    // Too few arguments
    EXPECT_THROW(dispatcher_->dispatch("twoArgs", 5), std::exception);

    // Too many arguments
    EXPECT_THROW(dispatcher_->dispatch("twoArgs", 5, 3, 7), std::exception);
}

TEST_F(DispatchTest, DispatchWithWrongArgumentTypes) {
    auto intFunc = [](int x) -> int { return x * 2; };
    dispatcher_->registerFunction("intFunc", intFunc);

    // Try to pass incompatible type
    EXPECT_THROW(dispatcher_->dispatch("intFunc", std::string("not_int")),
                 std::exception);
}

TEST_F(DispatchTest, FunctionThrowsException) {
    auto throwingFunc = []() -> int {
        throw std::runtime_error("Function error");
    };

    dispatcher_->registerFunction("throwingFunc", throwingFunc);

    EXPECT_THROW(dispatcher_->dispatch("throwingFunc"), std::runtime_error);
}

// ============================================================================
// Function Overloading Tests
// ============================================================================

TEST_F(DispatchTest, FunctionOverloading) {
    // Register functions with same name but different signatures
    auto intFunc = [](int x) -> std::string {
        return "int: " + std::to_string(x);
    };
    auto doubleFunc = [](double x) -> std::string {
        return "double: " + std::to_string(x);
    };
    auto stringFunc = [](const std::string& x) -> std::string {
        return "string: " + x;
    };

    dispatcher_->registerFunction("overloaded", intFunc);
    dispatcher_->registerFunction("overloaded", doubleFunc);
    dispatcher_->registerFunction("overloaded", stringFunc);

    // Dispatch should select correct overload based on argument types
    auto intResult = dispatcher_->dispatch("overloaded", 42);
    EXPECT_EQ(std::any_cast<std::string>(intResult), "int: 42");

    auto doubleResult = dispatcher_->dispatch("overloaded", 3.14);
    EXPECT_EQ(std::any_cast<std::string>(doubleResult), "double: 3.140000");

    auto stringResult =
        dispatcher_->dispatch("overloaded", std::string("test"));
    EXPECT_EQ(std::any_cast<std::string>(stringResult), "string: test");
}

// ============================================================================
// Function Removal Tests
// ============================================================================

TEST_F(DispatchTest, RemoveFunction) {
    auto testFunc = []() -> int { return 42; };

    dispatcher_->registerFunction("removableFunc", testFunc);
    EXPECT_TRUE(dispatcher_->hasFunction("removableFunc"));

    dispatcher_->removeFunction("removableFunc");
    EXPECT_FALSE(dispatcher_->hasFunction("removableFunc"));

    EXPECT_THROW(dispatcher_->dispatch("removableFunc"), std::exception);
}

TEST_F(DispatchTest, RemoveNonExistentFunction) {
    // Should not throw when removing non-existent function
    EXPECT_NO_THROW(dispatcher_->removeFunction("nonExistent"));
}

// ============================================================================
// Function Listing Tests
// ============================================================================

TEST_F(DispatchTest, GetFunctionNames) {
    dispatcher_->registerFunction("func1", []() -> int { return 1; });
    dispatcher_->registerFunction("func2", []() -> int { return 2; });
    dispatcher_->registerFunction("func3", []() -> int { return 3; });

    auto names = dispatcher_->getFunctionNames();
    EXPECT_GE(names.size(), 3);

    bool hasFunc1 =
        std::find(names.begin(), names.end(), "func1") != names.end();
    bool hasFunc2 =
        std::find(names.begin(), names.end(), "func2") != names.end();
    bool hasFunc3 =
        std::find(names.begin(), names.end(), "func3") != names.end();

    EXPECT_TRUE(hasFunc1);
    EXPECT_TRUE(hasFunc2);
    EXPECT_TRUE(hasFunc3);
}

// ============================================================================
// Performance Benchmark Tests
// ============================================================================

TEST_F(DispatchPerformanceTest, SimpleDispatchPerformance) {
    auto simpleFunc = []() -> int { return 42; };
    dispatcher_->registerFunction("simpleFunc", simpleFunc);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        auto result = dispatcher_->dispatch("simpleFunc");
        volatile int value = std::any_cast<int>(result);
        (void)value;  // Prevent optimization
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Simple dispatch benchmark: " << duration.count() << " μs for "
              << BENCHMARK_ITERATIONS << " calls" << std::endl;
    std::cout << "Average per call: "
              << (duration.count() / BENCHMARK_ITERATIONS) << " μs"
              << std::endl;

    // Should be reasonably fast (less than 10μs per call on average)
    EXPECT_LT(duration.count() / BENCHMARK_ITERATIONS, 10);
}

TEST_F(DispatchPerformanceTest, ArgumentDispatchPerformance) {
    auto addFunc = [](int a, int b) -> int { return a + b; };
    dispatcher_->registerFunction("add", addFunc);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        auto result = dispatcher_->dispatch("add", i, i + 1);
        volatile int value = std::any_cast<int>(result);
        (void)value;  // Prevent optimization
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Argument dispatch benchmark: " << duration.count()
              << " μs for " << BENCHMARK_ITERATIONS << " calls" << std::endl;
    std::cout << "Average per call: "
              << (duration.count() / BENCHMARK_ITERATIONS) << " μs"
              << std::endl;

    // Should be reasonably fast
    EXPECT_LT(duration.count() / BENCHMARK_ITERATIONS, 15);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(DispatchTest, ConcurrentDispatch) {
    auto threadSafeFunc = [](int x) -> int { return x * x; };
    dispatcher_->registerFunction("square", threadSafeFunc);

    const int numThreads = 4;
    const int callsPerThread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &successCount, callsPerThread, t]() {
            for (int i = 0; i < callsPerThread; ++i) {
                try {
                    auto result =
                        dispatcher_->dispatch("square", t * callsPerThread + i);
                    int value = std::any_cast<int>(result);
                    int expected =
                        (t * callsPerThread + i) * (t * callsPerThread + i);
                    if (value == expected) {
                        successCount++;
                    }
                } catch (...) {
                    // Some calls might fail due to race conditions
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Most calls should succeed
    EXPECT_GT(successCount.load(), numThreads * callsPerThread * 0.9);
}
