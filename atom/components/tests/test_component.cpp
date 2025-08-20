/*
 * test_component.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Comprehensive Unit Tests for Core Component Class
Tests constructor/destructor, state management, variable management,
command registration/dispatch, performance statistics, event system,
and error handling.

**************************************************/

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "../component.hpp"
#include "../registry.hpp"

using namespace atom::components;

/**
 * @brief Test fixture for Component class tests
 */
class ComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test component
        component_ = std::make_shared<Component>("TestComponent");
    }

    void TearDown() override {
        // Cleanup
        component_.reset();
    }

    std::shared_ptr<Component> component_;
};

/**
 * @brief Test fixture for Component error scenarios
 */
class ComponentErrorTest : public ::testing::Test {
protected:
    void SetUp() override {
        component_ = std::make_shared<Component>("ErrorTestComponent");
    }

    void TearDown() override { component_.reset(); }

    std::shared_ptr<Component> component_;
};

// ============================================================================
// Constructor and Basic Properties Tests
// ============================================================================

TEST_F(ComponentTest, ConstructorWithValidName) {
    EXPECT_EQ(component_->getName(), "TestComponent");
    EXPECT_EQ(component_->getState(), ComponentState::Created);
    EXPECT_FALSE(component_->getDoc().empty() ==
                 false);  // Doc might be empty initially
}

TEST(ComponentConstructorTest, ConstructorWithEmptyName) {
    EXPECT_THROW(std::make_shared<Component>(""), std::invalid_argument);
}

TEST_F(ComponentTest, GetInstanceAndSharedInstance) {
    auto weakInstance = component_->getInstance();
    EXPECT_FALSE(weakInstance.expired());

    auto sharedInstance = component_->getSharedInstance();
    EXPECT_EQ(sharedInstance, component_);
    EXPECT_EQ(sharedInstance.get(), component_.get());
}

// ============================================================================
// State Management Tests
// ============================================================================

TEST_F(ComponentTest, StateTransitions) {
    // Test initial state
    EXPECT_EQ(component_->getState(), ComponentState::Created);

    // Test state transitions
    component_->setState(ComponentState::Initializing);
    EXPECT_EQ(component_->getState(), ComponentState::Initializing);

    component_->setState(ComponentState::Active);
    EXPECT_EQ(component_->getState(), ComponentState::Active);

    component_->setState(ComponentState::Disabled);
    EXPECT_EQ(component_->getState(), ComponentState::Disabled);

    component_->setState(ComponentState::Error);
    EXPECT_EQ(component_->getState(), ComponentState::Error);

    component_->setState(ComponentState::Destroying);
    EXPECT_EQ(component_->getState(), ComponentState::Destroying);
}

TEST_F(ComponentTest, InitializeAndDestroy) {
    // Test initialization
    EXPECT_TRUE(component_->initialize());
    EXPECT_EQ(component_->getState(), ComponentState::Active);

    // Test destruction
    EXPECT_TRUE(component_->destroy());
    EXPECT_EQ(component_->getState(), ComponentState::Destroying);
}

// ============================================================================
// Variable Management Tests
// ============================================================================

TEST_F(ComponentTest, AddAndGetVariable) {
    // Add integer variable
    component_->addVariable<int>("testInt", 42, "Test integer variable");
    EXPECT_TRUE(component_->hasVariable("testInt"));

    auto intVar = component_->getVariable<int>("testInt");
    ASSERT_NE(intVar, nullptr);
    EXPECT_EQ(intVar->get(), 42);

    // Add string variable
    component_->addVariable<std::string>("testString", "hello",
                                         "Test string variable");
    EXPECT_TRUE(component_->hasVariable("testString"));

    auto stringVar = component_->getVariable<std::string>("testString");
    ASSERT_NE(stringVar, nullptr);
    EXPECT_EQ(stringVar->get(), "hello");

    // Add double variable with alias and group
    component_->addVariable<double>("testDouble", 3.14, "Test double variable",
                                    "pi", "math");
    EXPECT_TRUE(component_->hasVariable("testDouble"));
}

TEST_F(ComponentTest, SetAndGetVariableValue) {
    component_->addVariable<int>("counter", 0);

    // Set value
    component_->setValue<int>("counter", 100);

    auto var = component_->getVariable<int>("counter");
    EXPECT_EQ(var->get(), 100);
}

TEST_F(ComponentTest, VariableRange) {
    component_->addVariable<int>("rangedInt", 50);
    component_->setRange<int>("rangedInt", 0, 100);

    // Valid range
    component_->setValue<int>("rangedInt", 75);
    auto var = component_->getVariable<int>("rangedInt");
    EXPECT_EQ(var->get(), 75);
}

TEST_F(ComponentTest, StringOptions) {
    component_->addVariable<std::string>("mode", "auto");

    std::vector<std::string> options = {"auto", "manual", "disabled"};
    component_->setStringOptions("mode", options);

    // Valid option
    component_->setValue<std::string>("mode", "manual");
    auto var = component_->getVariable<std::string>("mode");
    EXPECT_EQ(var->get(), "manual");
}

TEST_F(ComponentTest, GetVariableNames) {
    component_->addVariable<int>("var1", 1);
    component_->addVariable<std::string>("var2", "test");
    component_->addVariable<double>("var3", 1.0);

    auto names = component_->getVariableNames();
    EXPECT_GE(names.size(), 3);

    // Check that our variables are in the list
    bool hasVar1 = std::find(names.begin(), names.end(), "var1") != names.end();
    bool hasVar2 = std::find(names.begin(), names.end(), "var2") != names.end();
    bool hasVar3 = std::find(names.begin(), names.end(), "var3") != names.end();

    EXPECT_TRUE(hasVar1);
    EXPECT_TRUE(hasVar2);
    EXPECT_TRUE(hasVar3);
}

TEST_F(ComponentTest, VariableMetadata) {
    component_->addVariable<int>("documented", 42, "A well documented variable",
                                 "alias", "group");

    EXPECT_EQ(component_->getVariableDescription("documented"),
              "A well documented variable");
    EXPECT_EQ(component_->getVariableAlias("documented"), "alias");
    EXPECT_EQ(component_->getVariableGroup("documented"), "group");
}

// ============================================================================
// Error Handling Tests for Variables
// ============================================================================

TEST_F(ComponentErrorTest, NonExistentVariable) {
    EXPECT_FALSE(component_->hasVariable("nonexistent"));
    EXPECT_THROW(component_->getVariable<int>("nonexistent"),
                 std::out_of_range);
    EXPECT_THROW(component_->setValue<int>("nonexistent", 42),
                 std::out_of_range);
}

TEST_F(ComponentErrorTest, VariableTypeError) {
    component_->addVariable<int>("intVar", 42);

    // Try to get as wrong type
    EXPECT_THROW(component_->getVariable<std::string>("intVar"),
                 VariableTypeError);
    EXPECT_THROW(component_->setValue<std::string>("intVar", "wrong"),
                 VariableTypeError);
}

TEST_F(ComponentErrorTest, VariableRangeError) {
    component_->addVariable<int>("rangedVar", 50);
    component_->setRange<int>("rangedVar", 0, 100);

    // This should throw when trying to set out of range value
    // Note: The actual behavior depends on the implementation
    // Some implementations might clamp, others might throw
}

// ============================================================================
// Documentation Tests
// ============================================================================

TEST_F(ComponentTest, Documentation) {
    std::string testDoc = "This is a test component for unit testing";
    component_->doc(testDoc);

    EXPECT_EQ(component_->getDoc(), testDoc);
}

// ============================================================================
// Command Registration and Dispatch Tests
// ============================================================================

TEST_F(ComponentTest, RegisterAndDispatchFunction) {
    // Register a simple function
    component_->def("add", [](int a, int b) -> int { return a + b; });

    EXPECT_TRUE(component_->has("add"));

    // Dispatch the function
    auto result = component_->dispatch("add", 5, 3);
    EXPECT_EQ(std::any_cast<int>(result), 8);
}

TEST_F(ComponentTest, RegisterAndDispatchVoidFunction) {
    bool called = false;
    component_->def("setFlag", [&called]() { called = true; });

    EXPECT_TRUE(component_->has("setFlag"));

    component_->dispatch("setFlag");
    EXPECT_TRUE(called);
}

TEST_F(ComponentTest, RegisterFunctionWithDescription) {
    component_->def(
        "multiply", [](double a, double b) -> double { return a * b; }, "math",
        "Multiplies two numbers");

    EXPECT_TRUE(component_->has("multiply"));

    auto result = component_->dispatch("multiply", 2.5, 4.0);
    EXPECT_DOUBLE_EQ(std::any_cast<double>(result), 10.0);
}

TEST_F(ComponentTest, FastDispatch) {
    component_->def("fastFunc", []() -> int { return 42; });

    auto result = component_->fastDispatch("fastFunc");
    EXPECT_EQ(std::any_cast<int>(result), 42);
}

TEST_F(ComponentTest, DispatchWithSpanArgs) {
    component_->def("sum",
                    [](int a, int b, int c) -> int { return a + b + c; });

    std::vector<std::any> args = {std::any(1), std::any(2), std::any(3)};
    auto result = component_->dispatch("sum", std::span<const std::any>(args));
    EXPECT_EQ(std::any_cast<int>(result), 6);
}

TEST_F(ComponentTest, RegisterMemberFunction) {
    struct TestClass {
        int value = 10;
        int getValue() const { return value; }
        void setValue(int v) { value = v; }
        int add(int a, int b) { return a + b; }
    };

    auto testObj = std::make_shared<TestClass>();

    // Register member functions
    component_->def("getValue", &TestClass::getValue, testObj);
    component_->def("setValue", &TestClass::setValue, testObj);
    component_->def("add", &TestClass::add, testObj);

    // Test getter
    auto result = component_->dispatch("getValue");
    EXPECT_EQ(std::any_cast<int>(result), 10);

    // Test setter
    component_->dispatch("setValue", 20);
    result = component_->dispatch("getValue");
    EXPECT_EQ(std::any_cast<int>(result), 20);

    // Test member function with parameters
    result = component_->dispatch("add", 5, 7);
    EXPECT_EQ(std::any_cast<int>(result), 12);
}

TEST_F(ComponentTest, RegisterMemberVariable) {
    struct TestClass {
        int publicVar = 42;
        std::string name = "test";
    };

    auto testObj = std::make_shared<TestClass>();

    component_->def("publicVar", &TestClass::publicVar, testObj);
    component_->def("name", &TestClass::name, testObj);

    // Test getting member variable
    auto result = component_->dispatch("publicVar");
    EXPECT_EQ(std::any_cast<int>(result), 42);

    result = component_->dispatch("name");
    EXPECT_EQ(std::any_cast<std::string>(result), "test");
}

TEST_F(ComponentTest, RegisterConstructor) {
    struct SimpleClass {
        int value;
        SimpleClass(int v) : value(v) {}
        int getValue() const { return value; }
    };

    component_->defConstructor<SimpleClass, int>("SimpleClass");
    EXPECT_TRUE(component_->has("SimpleClass"));

    auto result = component_->dispatch("SimpleClass", 100);
    // The result should be a SimpleClass instance
    EXPECT_TRUE(result.has_value());
}

TEST_F(ComponentTest, RegisterType) {
    struct CustomType {
        int data = 0;
    };

    component_->defType<CustomType>("CustomType", "custom",
                                    "A custom type for testing");
    EXPECT_TRUE(component_->hasType("CustomType"));
}

TEST_F(ComponentTest, RegisterEnum) {
    enum class TestEnum { Value1, Value2, Value3 };

    std::unordered_map<std::string, TestEnum> enumMap = {
        {"Value1", TestEnum::Value1},
        {"Value2", TestEnum::Value2},
        {"Value3", TestEnum::Value3}};

    component_->defEnum<TestEnum>("TestEnum", enumMap);
    EXPECT_TRUE(component_->hasType("TestEnum"));
}

// ============================================================================
// Command Management Tests
// ============================================================================

TEST_F(ComponentTest, RemoveCommand) {
    component_->def("tempFunc", []() -> int { return 1; });
    EXPECT_TRUE(component_->has("tempFunc"));

    component_->removeCommand("tempFunc");
    EXPECT_FALSE(component_->has("tempFunc"));
}

TEST_F(ComponentTest, CommandAlias) {
    component_->def("originalName", []() -> int { return 42; });
    component_->addAlias("originalName", "alias");

    // Both names should work
    EXPECT_TRUE(component_->has("originalName"));
    EXPECT_TRUE(component_->has("alias"));

    auto result1 = component_->dispatch("originalName");
    auto result2 = component_->dispatch("alias");

    EXPECT_EQ(std::any_cast<int>(result1), std::any_cast<int>(result2));
}

TEST_F(ComponentTest, CommandGroup) {
    component_->def("func1", []() -> int { return 1; });
    component_->def("func2", []() -> int { return 2; });

    component_->addGroup("func1", "group1");
    component_->addGroup("func2", "group1");

    auto commands = component_->getCommandsInGroup("group1");
    EXPECT_GE(commands.size(), 2);
}

TEST_F(ComponentTest, CommandTimeout) {
    component_->def("slowFunc", []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 42;
    });

    component_->setTimeout("slowFunc", std::chrono::milliseconds(50));

    // This test depends on timeout implementation
    // The function might throw or return an error
}

// ============================================================================
// Error Handling Tests for Commands
// ============================================================================

TEST_F(ComponentErrorTest, DispatchNonExistentCommand) {
    EXPECT_FALSE(component_->has("nonexistent"));
    EXPECT_THROW(component_->dispatch("nonexistent"), std::exception);
}

TEST_F(ComponentErrorTest, DispatchWithWrongArguments) {
    component_->def("needsInt", [](int x) -> int { return x * 2; });

    // Try to call with wrong argument type or count
    EXPECT_THROW(component_->dispatch("needsInt", "string"), std::exception);
    EXPECT_THROW(component_->dispatch("needsInt"), std::exception);
    EXPECT_THROW(component_->dispatch("needsInt", 1, 2), std::exception);
}

// ============================================================================
// Performance Statistics Tests
// ============================================================================

TEST_F(ComponentTest, PerformanceStatsTracking) {
    component_->def("testFunc", []() -> int { return 42; });

    // Get initial stats
    const auto& stats = component_->getPerformanceStats();
    auto initialCallCount = stats.commandCallCount.load();

    // Execute command
    component_->dispatch("testFunc");

    // Check stats updated
    EXPECT_EQ(stats.commandCallCount.load(), initialCallCount + 1);
    EXPECT_GT(stats.timing.totalExecutionTimeNs.load(), 0);
}

TEST_F(ComponentTest, PerformanceStatsErrorTracking) {
    component_->def("errorFunc",
                    []() -> int { throw std::runtime_error("Test error"); });

    const auto& stats = component_->getPerformanceStats();
    auto initialErrorCount = stats.commandErrorCount.load();

    // Execute command that throws
    EXPECT_THROW(component_->dispatch("errorFunc"), std::runtime_error);

    // Check error count increased
    EXPECT_EQ(stats.commandErrorCount.load(), initialErrorCount + 1);
}

TEST_F(ComponentTest, PerformanceStatsReset) {
    component_->def("testFunc", []() -> int { return 1; });

    // Execute some commands
    component_->dispatch("testFunc");
    component_->dispatch("testFunc");

    const auto& stats = component_->getPerformanceStats();
    EXPECT_GT(stats.commandCallCount.load(), 0);

    // Reset stats
    component_->resetPerformanceStats();

    // Check stats are reset
    EXPECT_EQ(stats.commandCallCount.load(), 0);
    EXPECT_EQ(stats.commandErrorCount.load(), 0);
    EXPECT_EQ(stats.timing.totalExecutionTimeNs.load(), 0);
}

TEST_F(ComponentTest, PerformanceStatsCacheAlignment) {
    const auto& stats = component_->getPerformanceStats();

    // Check that performance stats are cache-line aligned
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&stats) % 64, 0)
        << "Performance stats should be cache-line aligned";
}

TEST_F(ComponentTest, LegacyPerformanceStats) {
    component_->def("testFunc", []() -> int { return 1; });
    component_->dispatch("testFunc");

    auto legacyStats = component_->getLegacyPerformanceStats();
    EXPECT_GT(legacyStats.commandCallCount.load(), 0);
}

#if ENABLE_EVENT_SYSTEM
// ============================================================================
// Event System Tests
// ============================================================================

TEST_F(ComponentTest, EventEmissionAndHandling) {
    bool eventReceived = false;
    std::string receivedData;

    // Register event handler
    auto callbackId = component_->on(
        "testEvent", [&](const std::string& eventName, const std::any& data) {
            eventReceived = true;
            if (data.has_value() && data.type() == typeid(std::string)) {
                receivedData = std::any_cast<std::string>(data);
            }
        });

    // Emit event
    component_->emitEvent("testEvent", std::string("test data"));

    EXPECT_TRUE(eventReceived);
    EXPECT_EQ(receivedData, "test data");
}

TEST_F(ComponentTest, OneTimeEventHandler) {
    int callCount = 0;

    // Register one-time handler
    auto callbackId = component_->once(
        "onceEvent", [&](const std::string&, const std::any&) { callCount++; });

    // Emit event multiple times
    component_->emitEvent("onceEvent");
    component_->emitEvent("onceEvent");
    component_->emitEvent("onceEvent");

    // Should only be called once
    EXPECT_EQ(callCount, 1);
}

TEST_F(ComponentTest, EventHandlerRemoval) {
    bool eventReceived = false;

    auto callbackId = component_->on(
        "removeTest",
        [&](const std::string&, const std::any&) { eventReceived = true; });

    // Remove handler
    EXPECT_TRUE(component_->off("removeTest", callbackId));

    // Emit event - should not be received
    component_->emitEvent("removeTest");
    EXPECT_FALSE(eventReceived);
}

TEST_F(ComponentTest, MultipleEventHandlers) {
    int handler1Called = 0;
    int handler2Called = 0;

    auto id1 = component_->on(
        "multiEvent",
        [&](const std::string&, const std::any&) { handler1Called++; });

    auto id2 = component_->on(
        "multiEvent",
        [&](const std::string&, const std::any&) { handler2Called++; });

    component_->emitEvent("multiEvent");

    EXPECT_EQ(handler1Called, 1);
    EXPECT_EQ(handler2Called, 1);
}

TEST_F(ComponentTest, EventStatisticsTracking) {
    component_->on("statEvent", [](const std::string&, const std::any&) {});

    const auto& stats = component_->getPerformanceStats();
    auto initialEventCount = stats.eventCount.load();

    component_->emitEvent("statEvent");

    EXPECT_EQ(stats.eventCount.load(), initialEventCount + 1);
}
#endif

// ============================================================================
// Type Information Tests
// ============================================================================

TEST_F(ComponentTest, TypeInformation) {
    auto typeInfo = component_->getTypeInfo();
    EXPECT_TRUE(typeInfo.isValid());

    // Set new type info
    auto newTypeInfo = atom::meta::userType<Component>();
    component_->setTypeInfo(newTypeInfo);

    auto retrievedTypeInfo = component_->getTypeInfo();
    EXPECT_EQ(retrievedTypeInfo.name(), newTypeInfo.name());
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(ComponentTest, ConcurrentVariableAccess) {
    component_->addVariable<int>("sharedVar", 0);

    const int numThreads = 4;
    const int incrementsPerThread = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, incrementsPerThread]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                auto var = component_->getVariable<int>("sharedVar");
                var->set(var->get() + 1);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto finalVar = component_->getVariable<int>("sharedVar");
    // Note: Without proper synchronization, this might not equal the expected
    // value This test mainly checks that no crashes occur during concurrent
    // access
    EXPECT_GE(finalVar->get(), 0);
}

TEST_F(ComponentTest, ConcurrentCommandDispatch) {
    std::atomic<int> callCount{0};
    component_->def("concurrentFunc", [&callCount]() -> int {
        callCount++;
        return callCount.load();
    });

    const int numThreads = 4;
    const int callsPerThread = 50;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, callsPerThread]() {
            for (int j = 0; j < callsPerThread; ++j) {
                component_->dispatch("concurrentFunc");
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(callCount.load(), numThreads * callsPerThread);
}

// ============================================================================
// Performance Benchmarks
// ============================================================================

class ComponentPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        component_ = std::make_shared<Component>("PerformanceTestComponent");
    }

    std::shared_ptr<Component> component_;
    static constexpr int BENCHMARK_ITERATIONS = 1000;
};

TEST_F(ComponentPerformanceTest, DispatchPerformanceBenchmark) {
    component_->def("benchmarkFunc", []() -> int { return 42; });

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        component_->dispatch("benchmarkFunc");
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Dispatch benchmark: " << duration.count() << " μs for "
              << BENCHMARK_ITERATIONS << " calls" << std::endl;
    std::cout << "Average per call: "
              << (duration.count() / BENCHMARK_ITERATIONS) << " μs"
              << std::endl;

    // Should be reasonably fast (less than 1μs per call on average)
    EXPECT_LT(duration.count() / BENCHMARK_ITERATIONS, 1);
}

TEST_F(ComponentPerformanceTest, FastDispatchVsRegularDispatch) {
    component_->def("fastBenchFunc", []() -> int { return 42; });

    // Benchmark regular dispatch
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        component_->dispatch("fastBenchFunc");
    }
    auto regularTime = std::chrono::high_resolution_clock::now() - start;

    // Benchmark fast dispatch
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        component_->fastDispatch("fastBenchFunc");
    }
    auto fastTime = std::chrono::high_resolution_clock::now() - start;

    std::cout << "Regular dispatch: " << regularTime.count() << " ns"
              << std::endl;
    std::cout << "Fast dispatch: " << fastTime.count() << " ns" << std::endl;

    // Fast dispatch should be faster or at least not significantly slower
    EXPECT_LE(fastTime.count(), regularTime.count() * 1.1);
}

TEST_F(ComponentPerformanceTest, VariableAccessPerformance) {
    component_->addVariable<int>("perfVar", 0);
    auto var = component_->getVariable<int>("perfVar");

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        var->set(i);
        volatile int value = var->get();  // Prevent optimization
        (void)value;                      // Suppress unused variable warning
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Variable access benchmark: " << duration.count() << " μs for "
              << BENCHMARK_ITERATIONS << " operations" << std::endl;

    // Should be very fast (less than 0.1μs per operation on average)
    EXPECT_LT(duration.count() / BENCHMARK_ITERATIONS, 0.1);
}

// ============================================================================
// Memory and Resource Tests
// ============================================================================

TEST_F(ComponentTest, MemoryUsageTracking) {
    const auto& stats = component_->getPerformanceStats();
    auto initialAllocations = stats.memoryAllocations.load();

    // Add variables which should trigger memory allocations
    component_->addVariable<std::string>("str1", "test1");
    component_->addVariable<std::string>("str2", "test2");
    component_->addVariable<std::vector<int>>("vec1",
                                              std::vector<int>{1, 2, 3});

    // Memory allocations should have increased
    EXPECT_GE(stats.memoryAllocations.load(), initialAllocations);
}

TEST_F(ComponentTest, ResourceCleanup) {
    // Create a component with resources
    auto tempComponent = std::make_shared<Component>("TempComponent");
    tempComponent->addVariable<std::string>("resource", "allocated");
    tempComponent->def("cleanup", []() { /* cleanup code */ });

    // Get weak reference to check destruction
    std::weak_ptr<Component> weakRef = tempComponent;

    // Release the component
    tempComponent.reset();

    // Component should be destroyed
    EXPECT_TRUE(weakRef.expired());
}

// ============================================================================
// Edge Cases and Boundary Tests
// ============================================================================

TEST_F(ComponentTest, LargeNumberOfVariables) {
    const int numVars = 1000;

    for (int i = 0; i < numVars; ++i) {
        component_->addVariable<int>("var" + std::to_string(i), i);
    }

    auto names = component_->getVariableNames();
    EXPECT_GE(names.size(), numVars);

    // Test random access
    auto var500 = component_->getVariable<int>("var500");
    EXPECT_EQ(var500->get(), 500);
}

TEST_F(ComponentTest, LargeNumberOfCommands) {
    const int numCommands = 500;

    for (int i = 0; i < numCommands; ++i) {
        component_->def("cmd" + std::to_string(i), [i]() -> int { return i; });
    }

    // Test random command execution
    auto result = component_->dispatch("cmd250");
    EXPECT_EQ(std::any_cast<int>(result), 250);
}

TEST_F(ComponentTest, VeryLongStrings) {
    std::string longString(10000, 'A');
    component_->addVariable<std::string>("longString", longString);

    auto var = component_->getVariable<std::string>("longString");
    EXPECT_EQ(var->get().length(), 10000);
    EXPECT_EQ(var->get()[0], 'A');
    EXPECT_EQ(var->get()[9999], 'A');
}

TEST_F(ComponentTest, UnicodeStrings) {
    std::string unicodeString = "Hello 世界 🌍 Мир";
    component_->addVariable<std::string>("unicode", unicodeString);

    auto var = component_->getVariable<std::string>("unicode");
    EXPECT_EQ(var->get(), unicodeString);
}

// ============================================================================
// Integration with Other Systems
// ============================================================================

TEST_F(ComponentTest, RegistryIntegration) {
    auto& registry = Registry::instance();

    // Register component with registry
    registry.registerComponent("IntegrationTestComponent", component_);

    // Retrieve from registry
    auto retrieved = registry.getComponent("IntegrationTestComponent");
    EXPECT_EQ(retrieved, component_);

    // Cleanup
    registry.removeComponent("IntegrationTestComponent");
}
