/*
 * test_lifecycle.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Comprehensive Unit Tests for Lifecycle Management
Tests hook registration/execution, dependency resolution, circular
dependency detection, event history, and validation mechanisms.

**************************************************/

#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "../component.hpp"
#include "../lifecycle.hpp"

using namespace atom::components;

/**
 * @brief Test fixture for LifecycleManager tests
 */
class LifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        lifecycleManager_ = &LifecycleManager::instance();

        // Clear any existing state
        lifecycleManager_->clearHistory();

        // Create test components
        component1_ = std::make_shared<Component>("TestComponent1");
        component2_ = std::make_shared<Component>("TestComponent2");
        component3_ = std::make_shared<Component>("TestComponent3");
    }

    void TearDown() override {
        // Clear history and reset state
        lifecycleManager_->clearHistory();
    }

    LifecycleManager* lifecycleManager_;
    std::shared_ptr<Component> component1_;
    std::shared_ptr<Component> component2_;
    std::shared_ptr<Component> component3_;
};

/**
 * @brief Test fixture for dependency resolution tests
 */
class DependencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        lifecycleManager_ = &LifecycleManager::instance();
        lifecycleManager_->clearHistory();

        // Create components for dependency testing
        compA_ = std::make_shared<Component>("ComponentA");
        compB_ = std::make_shared<Component>("ComponentB");
        compC_ = std::make_shared<Component>("ComponentC");
        compD_ = std::make_shared<Component>("ComponentD");
    }

    void TearDown() override { lifecycleManager_->clearHistory(); }

    LifecycleManager* lifecycleManager_;
    std::shared_ptr<Component> compA_;
    std::shared_ptr<Component> compB_;
    std::shared_ptr<Component> compC_;
    std::shared_ptr<Component> compD_;
};

// ============================================================================
// Singleton Pattern Tests
// ============================================================================

TEST(LifecycleManagerSingletonTest, SingletonInstance) {
    auto& instance1 = LifecycleManager::instance();
    auto& instance2 = LifecycleManager::instance();

    EXPECT_EQ(&instance1, &instance2);
}

// ============================================================================
// Hook Registration and Execution Tests
// ============================================================================

TEST_F(LifecycleTest, RegisterAndExecuteHook) {
    bool hookCalled = false;
    std::string capturedComponentName;
    LifecyclePhase capturedPhase;

    auto hook = [&](Component& component, LifecyclePhase phase) {
        hookCalled = true;
        capturedComponentName = component.getName();
        capturedPhase = phase;
    };

    lifecycleManager_->registerHook("TestComponent1",
                                    LifecyclePhase::PreInitialization, hook);

    bool result = lifecycleManager_->executePhase(
        *component1_, LifecyclePhase::PreInitialization);

    EXPECT_TRUE(result);
    EXPECT_TRUE(hookCalled);
    EXPECT_EQ(capturedComponentName, "TestComponent1");
    EXPECT_EQ(capturedPhase, LifecyclePhase::PreInitialization);
}

TEST_F(LifecycleTest, MultipleHooksForSamePhase) {
    int hookCallCount = 0;
    std::vector<int> callOrder;

    auto hook1 = [&](Component&, LifecyclePhase) {
        hookCallCount++;
        callOrder.push_back(1);
    };

    auto hook2 = [&](Component&, LifecyclePhase) {
        hookCallCount++;
        callOrder.push_back(2);
    };

    auto hook3 = [&](Component&, LifecyclePhase) {
        hookCallCount++;
        callOrder.push_back(3);
    };

    lifecycleManager_->registerHook("TestComponent1",
                                    LifecyclePhase::PostConstruction, hook1);
    lifecycleManager_->registerHook("TestComponent1",
                                    LifecyclePhase::PostConstruction, hook2);
    lifecycleManager_->registerHook("TestComponent1",
                                    LifecyclePhase::PostConstruction, hook3);

    lifecycleManager_->executePhase(*component1_,
                                    LifecyclePhase::PostConstruction);

    EXPECT_EQ(hookCallCount, 3);
    EXPECT_EQ(callOrder.size(), 3);
    // Hooks should be called in registration order
    EXPECT_EQ(callOrder[0], 1);
    EXPECT_EQ(callOrder[1], 2);
    EXPECT_EQ(callOrder[2], 3);
}

TEST_F(LifecycleTest, GlobalHooks) {
    int globalHookCallCount = 0;
    std::vector<std::string> componentsProcessed;

    auto globalHook = [&](Component& component, LifecyclePhase phase) {
        globalHookCallCount++;
        componentsProcessed.push_back(component.getName());
    };

    lifecycleManager_->registerGlobalHook(LifecyclePhase::PreActivation,
                                          globalHook);

    // Execute phase on multiple components
    lifecycleManager_->executePhase(*component1_,
                                    LifecyclePhase::PreActivation);
    lifecycleManager_->executePhase(*component2_,
                                    LifecyclePhase::PreActivation);
    lifecycleManager_->executePhase(*component3_,
                                    LifecyclePhase::PreActivation);

    EXPECT_EQ(globalHookCallCount, 3);
    EXPECT_EQ(componentsProcessed.size(), 3);

    // Check that all components were processed
    EXPECT_TRUE(std::find(componentsProcessed.begin(),
                          componentsProcessed.end(),
                          "TestComponent1") != componentsProcessed.end());
    EXPECT_TRUE(std::find(componentsProcessed.begin(),
                          componentsProcessed.end(),
                          "TestComponent2") != componentsProcessed.end());
    EXPECT_TRUE(std::find(componentsProcessed.begin(),
                          componentsProcessed.end(),
                          "TestComponent3") != componentsProcessed.end());
}

TEST_F(LifecycleTest, HookExecutionOrder) {
    std::vector<std::string> executionOrder;

    // Register global hook
    auto globalHook = [&](Component&, LifecyclePhase) {
        executionOrder.push_back("global");
    };

    // Register component-specific hook
    auto componentHook = [&](Component&, LifecyclePhase) {
        executionOrder.push_back("component");
    };

    lifecycleManager_->registerGlobalHook(LifecyclePhase::PostInitialization,
                                          globalHook);
    lifecycleManager_->registerHook(
        "TestComponent1", LifecyclePhase::PostInitialization, componentHook);

    lifecycleManager_->executePhase(*component1_,
                                    LifecyclePhase::PostInitialization);

    EXPECT_EQ(executionOrder.size(), 2);
    // Global hooks should execute before component-specific hooks
    EXPECT_EQ(executionOrder[0], "global");
    EXPECT_EQ(executionOrder[1], "component");
}

// ============================================================================
// Dependency Management Tests
// ============================================================================

TEST_F(DependencyTest, SimpleDependency) {
    // ComponentA depends on ComponentB
    DependencyConstraint dependency("ComponentB", DependencyType::Required);
    lifecycleManager_->addDependency("ComponentA", dependency);

    auto dependencies = lifecycleManager_->resolveDependencies("ComponentA");

    EXPECT_GE(dependencies.size(), 1);
    auto it = std::find(dependencies.begin(), dependencies.end(), "ComponentB");
    EXPECT_NE(it, dependencies.end());

    // ComponentB should come before ComponentA in the dependency order
    auto posA =
        std::find(dependencies.begin(), dependencies.end(), "ComponentA");
    auto posB =
        std::find(dependencies.begin(), dependencies.end(), "ComponentB");
    if (posA != dependencies.end() && posB != dependencies.end()) {
        EXPECT_LT(std::distance(dependencies.begin(), posB),
                  std::distance(dependencies.begin(), posA));
    }
}

TEST_F(DependencyTest, ChainedDependencies) {
    // A depends on B, B depends on C
    DependencyConstraint depAB("ComponentB", DependencyType::Required);
    DependencyConstraint depBC("ComponentC", DependencyType::Required);

    lifecycleManager_->addDependency("ComponentA", depAB);
    lifecycleManager_->addDependency("ComponentB", depBC);

    auto dependencies = lifecycleManager_->resolveDependencies("ComponentA");

    // Should include both B and C
    auto hasB = std::find(dependencies.begin(), dependencies.end(),
                          "ComponentB") != dependencies.end();
    auto hasC = std::find(dependencies.begin(), dependencies.end(),
                          "ComponentC") != dependencies.end();

    EXPECT_TRUE(hasB);
    EXPECT_TRUE(hasC);

    // Order should be C, B, A
    auto posA =
        std::find(dependencies.begin(), dependencies.end(), "ComponentA");
    auto posB =
        std::find(dependencies.begin(), dependencies.end(), "ComponentB");
    auto posC =
        std::find(dependencies.begin(), dependencies.end(), "ComponentC");

    if (posA != dependencies.end() && posB != dependencies.end() &&
        posC != dependencies.end()) {
        EXPECT_LT(std::distance(dependencies.begin(), posC),
                  std::distance(dependencies.begin(), posB));
        EXPECT_LT(std::distance(dependencies.begin(), posB),
                  std::distance(dependencies.begin(), posA));
    }
}

TEST_F(DependencyTest, OptionalDependency) {
    // ComponentA has optional dependency on ComponentB
    DependencyConstraint optionalDep("ComponentB", DependencyType::Optional);
    lifecycleManager_->addDependency("ComponentA", optionalDep);

    auto dependencies = lifecycleManager_->resolveDependencies("ComponentA");

    // Should not fail even if ComponentB doesn't exist
    EXPECT_NO_THROW(lifecycleManager_->resolveDependencies("ComponentA"));
}

TEST_F(DependencyTest, WeakDependency) {
    // ComponentA has weak dependency on ComponentB
    DependencyConstraint weakDep("ComponentB", DependencyType::Weak);
    lifecycleManager_->addDependency("ComponentA", weakDep);

    auto dependencies = lifecycleManager_->resolveDependencies("ComponentA");

    // Weak dependencies should be handled gracefully
    EXPECT_NO_THROW(lifecycleManager_->resolveDependencies("ComponentA"));
}

// ============================================================================
// Circular Dependency Detection Tests
// ============================================================================

TEST_F(DependencyTest, CircularDependencyDetection) {
    // Create circular dependency: A -> B -> C -> A
    DependencyConstraint depAB("ComponentB", DependencyType::Required);
    DependencyConstraint depBC("ComponentC", DependencyType::Required);
    DependencyConstraint depCA("ComponentA", DependencyType::Required);

    lifecycleManager_->addDependency("ComponentA", depAB);
    lifecycleManager_->addDependency("ComponentB", depBC);
    lifecycleManager_->addDependency("ComponentC", depCA);

    // Should detect circular dependency
    EXPECT_TRUE(lifecycleManager_->hasCircularDependencies("ComponentA"));
    EXPECT_TRUE(lifecycleManager_->hasCircularDependencies("ComponentB"));
    EXPECT_TRUE(lifecycleManager_->hasCircularDependencies("ComponentC"));
}

TEST_F(DependencyTest, SelfDependency) {
    // Component depends on itself
    DependencyConstraint selfDep("ComponentA", DependencyType::Required);
    lifecycleManager_->addDependency("ComponentA", selfDep);

    EXPECT_TRUE(lifecycleManager_->hasCircularDependencies("ComponentA"));
}

TEST_F(DependencyTest, ComplexDependencyGraph) {
    // Create complex dependency graph without cycles
    // A depends on B and C
    // B depends on D
    // C depends on D
    // D has no dependencies

    lifecycleManager_->addDependency(
        "ComponentA",
        DependencyConstraint("ComponentB", DependencyType::Required));
    lifecycleManager_->addDependency(
        "ComponentA",
        DependencyConstraint("ComponentC", DependencyType::Required));
    lifecycleManager_->addDependency(
        "ComponentB",
        DependencyConstraint("ComponentD", DependencyType::Required));
    lifecycleManager_->addDependency(
        "ComponentC",
        DependencyConstraint("ComponentD", DependencyType::Required));

    // Should not detect circular dependencies
    EXPECT_FALSE(lifecycleManager_->hasCircularDependencies("ComponentA"));
    EXPECT_FALSE(lifecycleManager_->hasCircularDependencies("ComponentB"));
    EXPECT_FALSE(lifecycleManager_->hasCircularDependencies("ComponentC"));
    EXPECT_FALSE(lifecycleManager_->hasCircularDependencies("ComponentD"));

    // Dependency resolution should work
    auto dependencies = lifecycleManager_->resolveDependencies("ComponentA");

    // Should include all components
    auto hasA = std::find(dependencies.begin(), dependencies.end(),
                          "ComponentA") != dependencies.end();
    auto hasB = std::find(dependencies.begin(), dependencies.end(),
                          "ComponentB") != dependencies.end();
    auto hasC = std::find(dependencies.begin(), dependencies.end(),
                          "ComponentC") != dependencies.end();
    auto hasD = std::find(dependencies.begin(), dependencies.end(),
                          "ComponentD") != dependencies.end();

    EXPECT_TRUE(hasA);
    EXPECT_TRUE(hasB);
    EXPECT_TRUE(hasC);
    EXPECT_TRUE(hasD);

    // D should come first, A should come last
    auto posA =
        std::find(dependencies.begin(), dependencies.end(), "ComponentA");
    auto posD =
        std::find(dependencies.begin(), dependencies.end(), "ComponentD");

    if (posA != dependencies.end() && posD != dependencies.end()) {
        EXPECT_LT(std::distance(dependencies.begin(), posD),
                  std::distance(dependencies.begin(), posA));
    }
}

// ============================================================================
// Dependency Validation Tests
// ============================================================================

TEST_F(DependencyTest, ValidateDependencies) {
    // Add valid dependencies
    lifecycleManager_->addDependency(
        "ComponentA",
        DependencyConstraint("ComponentB", DependencyType::Required));
    lifecycleManager_->addDependency(
        "ComponentB",
        DependencyConstraint("ComponentC", DependencyType::Required));

    // Validation should pass for valid dependency chain
    EXPECT_TRUE(lifecycleManager_->validateDependencies("ComponentA"));
    EXPECT_TRUE(lifecycleManager_->validateDependencies("ComponentB"));
    EXPECT_TRUE(lifecycleManager_->validateDependencies("ComponentC"));
}

TEST_F(DependencyTest, ValidateDependenciesWithMissing) {
    // Add dependency on non-existent component
    lifecycleManager_->addDependency(
        "ComponentA",
        DependencyConstraint("NonExistentComponent", DependencyType::Required));

    // Validation should fail for missing required dependency
    EXPECT_FALSE(lifecycleManager_->validateDependencies("ComponentA"));
}

TEST_F(DependencyTest, ValidateDependenciesWithOptional) {
    // Add optional dependency on non-existent component
    lifecycleManager_->addDependency(
        "ComponentA",
        DependencyConstraint("NonExistentComponent", DependencyType::Optional));

    // Validation should pass for missing optional dependency
    EXPECT_TRUE(lifecycleManager_->validateDependencies("ComponentA"));
}

TEST_F(DependencyTest, DependencyConstraintWithValidator) {
    bool validatorCalled = false;

    DependencyConstraint constraint("ComponentB", DependencyType::Required);
    constraint.validator = [&validatorCalled](const Component& comp) -> bool {
        validatorCalled = true;
        return comp.getName() == "ComponentB";
    };

    lifecycleManager_->addDependency("ComponentA", constraint);

    // Validation should call the custom validator
    lifecycleManager_->validateDependencies("ComponentA");
    EXPECT_TRUE(validatorCalled);
}

TEST_F(DependencyTest, DependencyConstraintWithVersion) {
    DependencyConstraint constraint("ComponentB", DependencyType::Required);
    constraint.version = ">=1.0.0";

    lifecycleManager_->addDependency("ComponentA", constraint);

    // Should not crash with version constraint
    EXPECT_NO_THROW(lifecycleManager_->validateDependencies("ComponentA"));
}

TEST_F(DependencyTest, DependencyConstraintWithTimeout) {
    DependencyConstraint constraint("ComponentB", DependencyType::Required);
    constraint.timeout = std::chrono::milliseconds(100);

    lifecycleManager_->addDependency("ComponentA", constraint);

    // Should handle timeout constraint
    EXPECT_NO_THROW(lifecycleManager_->validateDependencies("ComponentA"));
}

// ============================================================================
// Event History Tests
// ============================================================================

TEST_F(LifecycleTest, EventHistoryTracking) {
    // Execute some lifecycle phases
    lifecycleManager_->executePhase(*component1_,
                                    LifecyclePhase::PreConstruction);
    lifecycleManager_->executePhase(*component1_,
                                    LifecyclePhase::PostConstruction);
    lifecycleManager_->executePhase(*component2_,
                                    LifecyclePhase::PreInitialization);

    // Get history
    auto history = lifecycleManager_->getLifecycleHistory();

    EXPECT_GE(history.size(), 3);

    // Check that events are recorded
    bool hasPreConstruction = false;
    bool hasPostConstruction = false;
    bool hasPreInitialization = false;

    for (const auto& event : history) {
        if (event.componentName == "TestComponent1" &&
            event.phase == LifecyclePhase::PreConstruction) {
            hasPreConstruction = true;
            EXPECT_TRUE(event.success);
            EXPECT_GT(event.duration.count(), 0);
        }
        if (event.componentName == "TestComponent1" &&
            event.phase == LifecyclePhase::PostConstruction) {
            hasPostConstruction = true;
        }
        if (event.componentName == "TestComponent2" &&
            event.phase == LifecyclePhase::PreInitialization) {
            hasPreInitialization = true;
        }
    }

    EXPECT_TRUE(hasPreConstruction);
    EXPECT_TRUE(hasPostConstruction);
    EXPECT_TRUE(hasPreInitialization);
}

TEST_F(LifecycleTest, EventHistoryForSpecificComponent) {
    // Execute phases for multiple components
    lifecycleManager_->executePhase(*component1_,
                                    LifecyclePhase::PreConstruction);
    lifecycleManager_->executePhase(*component2_,
                                    LifecyclePhase::PreConstruction);
    lifecycleManager_->executePhase(*component1_,
                                    LifecyclePhase::PostConstruction);

    // Get history for specific component
    auto component1History =
        lifecycleManager_->getLifecycleHistory("TestComponent1");

    // Should only contain events for TestComponent1
    for (const auto& event : component1History) {
        EXPECT_EQ(event.componentName, "TestComponent1");
    }

    EXPECT_GE(component1History.size(), 2);
}

TEST_F(LifecycleTest, EventHistoryTimestamps) {
    auto beforeTime = std::chrono::steady_clock::now();

    lifecycleManager_->executePhase(*component1_,
                                    LifecyclePhase::PreConstruction);

    auto afterTime = std::chrono::steady_clock::now();

    auto history = lifecycleManager_->getLifecycleHistory("TestComponent1");
    ASSERT_GE(history.size(), 1);

    auto& event = history.back();
    EXPECT_GE(event.timestamp, beforeTime);
    EXPECT_LE(event.timestamp, afterTime);
}

TEST_F(LifecycleTest, ClearHistory) {
    // Execute some phases
    lifecycleManager_->executePhase(*component1_,
                                    LifecyclePhase::PreConstruction);
    lifecycleManager_->executePhase(*component2_,
                                    LifecyclePhase::PreConstruction);

    auto historyBefore = lifecycleManager_->getLifecycleHistory();
    EXPECT_GT(historyBefore.size(), 0);

    // Clear history
    lifecycleManager_->clearHistory();

    auto historyAfter = lifecycleManager_->getLifecycleHistory();
    EXPECT_EQ(historyAfter.size(), 0);
}

// ============================================================================
// Dependency Graph Tests
// ============================================================================

TEST_F(DependencyTest, GetDependencyGraph) {
    // Create dependency relationships
    lifecycleManager_->addDependency(
        "ComponentA",
        DependencyConstraint("ComponentB", DependencyType::Required));
    lifecycleManager_->addDependency(
        "ComponentA",
        DependencyConstraint("ComponentC", DependencyType::Required));
    lifecycleManager_->addDependency(
        "ComponentB",
        DependencyConstraint("ComponentD", DependencyType::Required));

    auto graph = lifecycleManager_->getDependencyGraph();

    // Check that dependencies are correctly represented
    EXPECT_TRUE(graph.find("ComponentA") != graph.end());
    EXPECT_TRUE(graph.find("ComponentB") != graph.end());

    auto& aDeps = graph["ComponentA"];
    EXPECT_TRUE(std::find(aDeps.begin(), aDeps.end(), "ComponentB") !=
                aDeps.end());
    EXPECT_TRUE(std::find(aDeps.begin(), aDeps.end(), "ComponentC") !=
                aDeps.end());

    auto& bDeps = graph["ComponentB"];
    EXPECT_TRUE(std::find(bDeps.begin(), bDeps.end(), "ComponentD") !=
                bDeps.end());
}

// ============================================================================
// Lifecycle Guard Tests
// ============================================================================

TEST_F(LifecycleTest, ComponentLifecycleGuard) {
    bool preHookCalled = false;
    bool postHookCalled = false;

    // Register hooks for start and end phases
    lifecycleManager_->registerHook(
        "TestComponent1", LifecyclePhase::PreActivation,
        [&](Component&, LifecyclePhase) { preHookCalled = true; });

    lifecycleManager_->registerHook(
        "TestComponent1", LifecyclePhase::PostDeactivation,
        [&](Component&, LifecyclePhase) { postHookCalled = true; });

    {
        // Create lifecycle guard
        ComponentLifecycleGuard guard(*component1_,
                                      LifecyclePhase::PreActivation,
                                      LifecyclePhase::PostDeactivation);

        EXPECT_TRUE(preHookCalled);
        EXPECT_FALSE(postHookCalled);
    }  // Guard destructor should execute end phase

    EXPECT_TRUE(postHookCalled);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(LifecycleTest, HookExecutionError) {
    bool errorHookCalled = false;
    bool normalHookCalled = false;

    // Register hook that throws
    auto errorHook = [&](Component&, LifecyclePhase) {
        errorHookCalled = true;
        throw std::runtime_error("Hook error");
    };

    // Register normal hook
    auto normalHook = [&](Component&, LifecyclePhase) {
        normalHookCalled = true;
    };

    lifecycleManager_->registerHook(
        "TestComponent1", LifecyclePhase::PreInitialization, errorHook);
    lifecycleManager_->registerHook(
        "TestComponent1", LifecyclePhase::PreInitialization, normalHook);

    // Execute phase - should handle error gracefully
    bool result = lifecycleManager_->executePhase(
        *component1_, LifecyclePhase::PreInitialization);

    EXPECT_TRUE(errorHookCalled);
    // Depending on implementation, normal hook might or might not be called
    // after error The important thing is that execution doesn't crash
}

TEST_F(LifecycleTest, EventHistoryWithErrors) {
    // Register hook that throws
    auto errorHook = [](Component&, LifecyclePhase) {
        throw std::runtime_error("Test error");
    };

    lifecycleManager_->registerHook(
        "TestComponent1", LifecyclePhase::PreInitialization, errorHook);

    // Execute phase
    lifecycleManager_->executePhase(*component1_,
                                    LifecyclePhase::PreInitialization);

    // Check that error is recorded in history
    auto history = lifecycleManager_->getLifecycleHistory("TestComponent1");
    ASSERT_GE(history.size(), 1);

    auto& event = history.back();
    EXPECT_EQ(event.componentName, "TestComponent1");
    EXPECT_EQ(event.phase, LifecyclePhase::PreInitialization);
    EXPECT_FALSE(event.success);
    EXPECT_FALSE(event.errorMessage.empty());
}
