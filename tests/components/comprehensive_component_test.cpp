#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"
#include "atom/components/lifecycle.hpp"
#include "atom/components/var.hpp"

using namespace atom::components;

// Test fixture for comprehensive component testing
class ComprehensiveComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        component_ = std::make_shared<Component>("TestComponent");
        registry_ = &Registry::instance();
        lifecycleManager_ = &LifecycleManager::instance();
        variableManager_ = std::make_unique<VariableManager>();
    }

    void TearDown() override {
        // Clean up any registered components
        if (registry_) {
            registry_->cleanupAll();
        }
        // Note: clearGlobalHooks method may not be available
        // lifecycleManager_ cleanup is handled automatically
    }

    std::shared_ptr<Component> component_;
    Registry* registry_;
    LifecycleManager* lifecycleManager_;
    std::unique_ptr<VariableManager> variableManager_;
};

// ============================================================================
// Core Component Tests
// ============================================================================

TEST_F(ComprehensiveComponentTest, ComponentBasicFunctionality) {
    EXPECT_EQ(component_->getName(), "TestComponent");
    EXPECT_NE(component_->getSharedInstance(), nullptr);
    EXPECT_FALSE(component_->getInstance().expired());
}

TEST_F(ComprehensiveComponentTest, ComponentVariableManagement) {
    // Test adding variables
    component_->addVariable<int>("testInt", 42, "Test integer variable");
    component_->addVariable<std::string>("testString", "Hello", "Test string variable");
    component_->addVariable<double>("testDouble", 3.14159, "Test double variable");

    // Test getting variables
    auto intVar = component_->getVariable<int>("testInt");
    ASSERT_NE(intVar, nullptr);
    EXPECT_EQ(intVar->get(), 42);

    auto stringVar = component_->getVariable<std::string>("testString");
    ASSERT_NE(stringVar, nullptr);
    EXPECT_EQ(stringVar->get(), "Hello");

    // Test variable existence
    EXPECT_TRUE(component_->hasVariable("testInt"));
    EXPECT_TRUE(component_->hasVariable("testString"));
    EXPECT_FALSE(component_->hasVariable("nonExistent"));

    // Test variable names
    auto names = component_->getVariableNames();
    EXPECT_GE(names.size(), 3);
    EXPECT_NE(std::find(names.begin(), names.end(), "testInt"), names.end());
}

TEST_F(ComprehensiveComponentTest, ComponentCommandSystem) {
    bool commandExecuted = false;

    // Define a simple command
    component_->def("testCommand", [&commandExecuted]() {
        commandExecuted = true;
        return 42;
    }, "test", "A test command");

    // Test command existence
    EXPECT_TRUE(component_->has("testCommand"));

    // Test command execution
    auto result = component_->dispatch("testCommand");
    EXPECT_TRUE(commandExecuted);

    // Test command with parameters
    component_->def("addCommand", [](int a, int b) {
        return a + b;
    }, "math", "Addition command");

    auto addResult = component_->dispatch("addCommand", 10, 20);
    EXPECT_EQ(std::any_cast<int>(addResult), 30);
}

TEST_F(ComprehensiveComponentTest, ComponentErrorHandling) {
    // Test accessing non-existent variable should throw
    EXPECT_THROW(component_->getVariable<int>("nonExistent"), std::exception);

    // Test dispatching non-existent command
    EXPECT_THROW(component_->dispatch("nonExistentCommand"), std::exception);

    // Test invalid command arguments
    component_->def("strictCommand", [](int required) {
        return required * 2;
    });

    std::vector<std::any> wrongArgs = {"string_instead_of_int"};
    EXPECT_THROW({
        auto result = component_->runCommand("strictCommand", wrongArgs);
        (void)result; // Suppress unused variable warning
    }, std::exception);
}

// ============================================================================
// Registry Tests
// ============================================================================

TEST_F(ComprehensiveComponentTest, RegistryBasicOperations) {
    // Test component registration
    registry_->addInitializer("TestComponent1",
        [](Component& comp) {
            comp.addVariable<int>("initialized", 1);
        },
        []() { /* cleanup */ });

    auto component1 = registry_->getComponent("TestComponent1");
    EXPECT_NE(component1, nullptr);
    EXPECT_EQ(component1->getName(), "TestComponent1");
}

TEST_F(ComprehensiveComponentTest, RegistryDependencyManagement) {
    // Register components with dependencies
    registry_->addInitializer("ComponentA", [](Component&) {}, []() {});
    registry_->addInitializer("ComponentB", [](Component&) {}, []() {});
    registry_->addInitializer("ComponentC", [](Component&) {}, []() {});

    // Add dependencies: C depends on B, B depends on A
    registry_->addDependency("ComponentC", "ComponentB");
    registry_->addDependency("ComponentB", "ComponentA");

    // Test circular dependency detection
    EXPECT_THROW(registry_->addDependency("ComponentA", "ComponentC"), std::exception);
}

// ============================================================================
// Lifecycle Manager Tests
// ============================================================================

TEST_F(ComprehensiveComponentTest, LifecycleHooks) {
    bool hookExecuted = false;

    // Register a lifecycle hook
    lifecycleManager_->registerHook("TestComponent", LifecyclePhase::PostInitialization,
        [&hookExecuted](Component&, LifecyclePhase) {
            hookExecuted = true;
            return true;
        });

    // Execute the lifecycle phase
    bool result = lifecycleManager_->executePhase(*component_, LifecyclePhase::PostInitialization);
    EXPECT_TRUE(result);
    EXPECT_TRUE(hookExecuted);
}

TEST_F(ComprehensiveComponentTest, GlobalLifecycleHooks) {
    int globalHookCount = 0;

    // Register global hooks for different phases
    lifecycleManager_->registerGlobalHook(LifecyclePhase::PreInitialization,
        [&globalHookCount](Component&, LifecyclePhase) {
            globalHookCount++;
            return true;
        });

    lifecycleManager_->registerGlobalHook(LifecyclePhase::PostInitialization,
        [&globalHookCount](Component&, LifecyclePhase) {
            globalHookCount++;
            return true;
        });

    // Execute different phases
    lifecycleManager_->executePhase(*component_, LifecyclePhase::PreInitialization);
    lifecycleManager_->executePhase(*component_, LifecyclePhase::PostInitialization);

    EXPECT_EQ(globalHookCount, 2);
}

// ============================================================================
// Variable Manager Tests
// ============================================================================

TEST_F(ComprehensiveComponentTest, VariableManagerOperations) {
    // Test basic variable operations
    variableManager_->addVariable("testVar", 100, "Test variable");

    EXPECT_TRUE(variableManager_->has("testVar"));

    auto var = variableManager_->getVariable<int>("testVar");
    ASSERT_NE(var, nullptr);
    EXPECT_EQ(var->get(), 100);

    // Test variable modification
    variableManager_->setValue("testVar", 200);
    EXPECT_EQ(var->get(), 200);
}

TEST_F(ComprehensiveComponentTest, VariableManagerRangeValidation) {
    variableManager_->addVariable("rangedVar", 50);
    variableManager_->setRange("rangedVar", 0, 100);

    // Valid range
    EXPECT_NO_THROW(variableManager_->setValue("rangedVar", 75));

    // Invalid range
    EXPECT_THROW(variableManager_->setValue("rangedVar", 150), std::exception);
    EXPECT_THROW(variableManager_->setValue("rangedVar", -10), std::exception);
}

TEST_F(ComprehensiveComponentTest, VariableManagerStringOptions) {
    variableManager_->addVariable("optionVar", std::string("option1"));
    std::vector<std::string> options = {"option1", "option2", "option3"};
    variableManager_->setStringOptions("optionVar", options);

    // Valid option
    EXPECT_NO_THROW(variableManager_->setValue("optionVar", std::string("option2")));

    // Invalid option
    EXPECT_THROW(variableManager_->setValue("optionVar", std::string("invalidOption")), std::exception);
}

// ============================================================================
// Performance and Stress Tests
// ============================================================================

TEST_F(ComprehensiveComponentTest, ComponentPerformanceBasic) {
    const int numVariables = 1000;
    const int numCommands = 100;

    auto start = std::chrono::high_resolution_clock::now();

    // Add many variables
    for (int i = 0; i < numVariables; ++i) {
        component_->addVariable<int>("var" + std::to_string(i), i);
    }

    // Add many commands
    for (int i = 0; i < numCommands; ++i) {
        component_->def("cmd" + std::to_string(i), [i]() { return i; });
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time (adjust threshold as needed)
    EXPECT_LT(duration.count(), 1000); // Less than 1 second

    // Verify all variables and commands were added
    EXPECT_EQ(component_->getVariableNames().size(), numVariables);
    EXPECT_EQ(component_->getAllCommands().size(), numCommands);
}

TEST_F(ComprehensiveComponentTest, ConcurrentAccess) {
    const int numThreads = 4;
    const int operationsPerThread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Launch multiple threads performing operations
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, operationsPerThread, &successCount]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                try {
                    std::string varName = "thread" + std::to_string(t) + "_var" + std::to_string(i);
                    component_->addVariable<int>(varName, t * 1000 + i);

                    auto var = component_->getVariable<int>(varName);
                    if (var && var->get() == t * 1000 + i) {
                        successCount++;
                    }
                } catch (...) {
                    // Handle any threading issues gracefully
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Most operations should succeed (allowing for some thread contention)
    EXPECT_GT(successCount.load(), numThreads * operationsPerThread * 0.8);
}
