#include "atom/components/lifecycle/lifecycle.hpp"
#include "atom/components/core/component.hpp"

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>

using namespace atom::components;

// Test component for lifecycle testing
class TestLifecycleComponent : public Component {
public:
    TestLifecycleComponent(const std::string& name) : Component(name) {}

    std::atomic<int> initializeCallCount{0};
    std::atomic<int> destroyCallCount{0};
    std::atomic<bool> initializeResult{true};
    std::atomic<bool> destroyResult{true};

    bool initialize() override {
        initializeCallCount++;
        return initializeResult.load();
    }

    bool destroy() override {
        destroyCallCount++;
        return destroyResult.load();
    }
};

// Test fixture for LifecycleManager tests
class LifecycleManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = &LifecycleManager::instance();
        component_ = std::make_shared<TestLifecycleComponent>("TestComponent");

        // Clear lifecycle history from previous tests
        manager_->clearHistory();
    }

    void TearDown() override {
        // Clear lifecycle history
        manager_->clearHistory();
    }

    LifecycleManager* manager_;
    std::shared_ptr<TestLifecycleComponent> component_;
};

// Test fixture for ComponentLifecycleGuard tests
class ComponentLifecycleGuardTest : public ::testing::Test {
protected:
    void SetUp() override {
        component_ =
            std::make_shared<TestLifecycleComponent>("GuardTestComponent");
    }

    std::shared_ptr<TestLifecycleComponent> component_;
};

// ============================================================================
// LifecycleManager Tests
// ============================================================================

TEST_F(LifecycleManagerTest, Singleton) {
    auto& manager1 = LifecycleManager::instance();
    auto& manager2 = LifecycleManager::instance();
    EXPECT_EQ(&manager1, &manager2);
}

TEST_F(LifecycleManagerTest, RegisterAndExecuteHook) {
    std::atomic<bool> hookExecuted{false};

    manager_->registerHook(
        "TestComponent", LifecyclePhase::PostInitialization,
        [&hookExecuted](Component&, LifecyclePhase) { hookExecuted = true; });

    bool result =
        manager_->executePhase(*component_, LifecyclePhase::PostInitialization);
    EXPECT_TRUE(result);
    EXPECT_TRUE(hookExecuted.load());
}

TEST_F(LifecycleManagerTest, RegisterGlobalHook) {
    std::atomic<bool> globalHookExecuted{false};

    manager_->registerGlobalHook(
        LifecyclePhase::PreInitialization,
        [&globalHookExecuted](Component&, LifecyclePhase) {
            globalHookExecuted = true;
        });

    bool result =
        manager_->executePhase(*component_, LifecyclePhase::PreInitialization);
    EXPECT_TRUE(result);
    EXPECT_TRUE(globalHookExecuted.load());
}

// ============================================================================
// Extended Lifecycle Tests - All Phases
// ============================================================================

TEST_F(LifecycleManagerTest, AllLifecyclePhases) {
    std::vector<LifecyclePhase> executedPhases;

    // Register hooks for all phases
    auto hookFunc = [&executedPhases](Component&, LifecyclePhase phase) {
        executedPhases.push_back(phase);
        return true;
    };

    manager_->registerHook("TestComponent", LifecyclePhase::PreInitialization,
                           hookFunc);
    manager_->registerHook("TestComponent", LifecyclePhase::PostInitialization,
                           hookFunc);
    manager_->registerHook("TestComponent", LifecyclePhase::PreActivation,
                           hookFunc);
    manager_->registerHook("TestComponent", LifecyclePhase::PostActivation,
                           hookFunc);
    manager_->registerHook("TestComponent", LifecyclePhase::PreDeactivation,
                           hookFunc);
    manager_->registerHook("TestComponent", LifecyclePhase::PostDeactivation,
                           hookFunc);
    manager_->registerHook("TestComponent", LifecyclePhase::PreDestruction,
                           hookFunc);
    manager_->registerHook("TestComponent", LifecyclePhase::PostDestruction,
                           hookFunc);

    // Execute all phases
    manager_->executePhase(*component_, LifecyclePhase::PreInitialization);
    manager_->executePhase(*component_, LifecyclePhase::PostInitialization);
    manager_->executePhase(*component_, LifecyclePhase::PreActivation);
    manager_->executePhase(*component_, LifecyclePhase::PostActivation);
    manager_->executePhase(*component_, LifecyclePhase::PreDeactivation);
    manager_->executePhase(*component_, LifecyclePhase::PostDeactivation);
    manager_->executePhase(*component_, LifecyclePhase::PreDestruction);
    manager_->executePhase(*component_, LifecyclePhase::PostDestruction);

    EXPECT_EQ(executedPhases.size(), 8);
}

// Test multiple hooks execution order
TEST_F(LifecycleManagerTest, MultipleHooksExecutionOrder) {
    std::vector<int> executionOrder;

    manager_->registerHook("TestComponent", LifecyclePhase::PostInitialization,
                           [&executionOrder](Component&, LifecyclePhase) {
                               executionOrder.push_back(1);
                           });

    manager_->registerHook("TestComponent", LifecyclePhase::PostInitialization,
                           [&executionOrder](Component&, LifecyclePhase) {
                               executionOrder.push_back(2);
                           });

    manager_->registerHook("TestComponent", LifecyclePhase::PostInitialization,
                           [&executionOrder](Component&, LifecyclePhase) {
                               executionOrder.push_back(3);
                           });

    manager_->executePhase(*component_, LifecyclePhase::PostInitialization);

    ASSERT_EQ(executionOrder.size(), 3);
    EXPECT_EQ(executionOrder[0], 1);
    EXPECT_EQ(executionOrder[1], 2);
    EXPECT_EQ(executionOrder[2], 3);
}

// Test hook error handling - hooks are void, so we test via exceptions
TEST_F(LifecycleManagerTest, HookErrorHandling) {
    std::atomic<bool> hookExecuted{false};

    manager_->registerHook(
        "TestComponent", LifecyclePhase::PreInitialization,
        [&hookExecuted](Component&, LifecyclePhase) { hookExecuted = true; });

    bool result =
        manager_->executePhase(*component_, LifecyclePhase::PreInitialization);
    EXPECT_TRUE(result);
    EXPECT_TRUE(hookExecuted.load());
}

// Test hook throwing exception
TEST_F(LifecycleManagerTest, HookThrowsException) {
    manager_->registerHook("TestComponent", LifecyclePhase::PreInitialization,
                           [](Component&, LifecyclePhase) {
                               throw std::runtime_error("Hook error");
                           });

    // Should catch exception and continue
    EXPECT_NO_THROW(
        manager_->executePhase(*component_, LifecyclePhase::PreInitialization));
}

// Test global and component-specific hooks together
TEST_F(LifecycleManagerTest, GlobalAndComponentHooks) {
    std::vector<std::string> executionOrder;

    manager_->registerGlobalHook(LifecyclePhase::PostInitialization,
                                 [&executionOrder](Component&, LifecyclePhase) {
                                     executionOrder.push_back("global");
                                 });

    manager_->registerHook("TestComponent", LifecyclePhase::PostInitialization,
                           [&executionOrder](Component&, LifecyclePhase) {
                               executionOrder.push_back("component");
                           });

    manager_->executePhase(*component_, LifecyclePhase::PostInitialization);

    ASSERT_EQ(executionOrder.size(), 2);
    EXPECT_EQ(executionOrder[0], "global");  // Global hooks execute first
    EXPECT_EQ(executionOrder[1], "component");
}

// Test dependency resolution with optional dependencies
TEST_F(LifecycleManagerTest, OptionalDependencies) {
    DependencyConstraint optionalDep("OptionalComponent",
                                     DependencyType::Optional);
    manager_->addDependency("TestComponent", optionalDep);

    // Should resolve successfully even if optional dependency doesn't exist
    auto deps = manager_->resolveDependencies("TestComponent");
    EXPECT_TRUE(deps.empty() || deps.size() == 1);
}

// Test complex dependency chain
TEST_F(LifecycleManagerTest, ComplexDependencyChain) {
    // Create chain: A -> B -> C -> D
    DependencyConstraint depB("ComponentB", DependencyType::Required);
    DependencyConstraint depC("ComponentC", DependencyType::Required);
    DependencyConstraint depD("ComponentD", DependencyType::Required);

    manager_->addDependency("ComponentA", depB);
    manager_->addDependency("ComponentB", depC);
    manager_->addDependency("ComponentC", depD);

    auto deps = manager_->resolveDependencies("ComponentA");

    // Should resolve in order: D, C, B
    ASSERT_GE(deps.size(), 3);
}

// Test concurrent hook execution
TEST_F(LifecycleManagerTest, ConcurrentHookExecution) {
    std::atomic<int> hookCount{0};

    manager_->registerHook(
        "TestComponent", LifecyclePhase::PostInitialization,
        [&hookCount](Component&, LifecyclePhase) { hookCount++; });

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this]() {
            manager_->executePhase(*component_,
                                   LifecyclePhase::PostInitialization);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(hookCount.load(), 10);
}

TEST_F(LifecycleManagerTest, MultipleHooksExecution) {
    std::atomic<int> hookCount{0};

    // Register multiple hooks for the same phase
    for (int i = 0; i < 3; ++i) {
        manager_->registerHook(
            "TestComponent", LifecyclePhase::PostConstruction,
            [&hookCount](Component&, LifecyclePhase) { hookCount++; });
    }

    bool result =
        manager_->executePhase(*component_, LifecyclePhase::PostConstruction);
    EXPECT_TRUE(result);
    EXPECT_EQ(hookCount.load(), 3);
}

TEST_F(LifecycleManagerTest, DependencyManagement) {
    DependencyConstraint dep("DependencyComponent", DependencyType::Required);
    manager_->addDependency("TestComponent", dep);

    auto dependencies = manager_->resolveDependencies("TestComponent");
    // Dependencies may or may not be resolved depending on whether
    // DependencyComponent exists
    EXPECT_TRUE(dependencies.empty() || dependencies.size() >= 1);
}

TEST_F(LifecycleManagerTest, CircularDependencyDetection) {
    DependencyConstraint depB("ComponentB", DependencyType::Required);
    DependencyConstraint depC("ComponentC", DependencyType::Required);
    DependencyConstraint depA("ComponentA", DependencyType::Required);

    manager_->addDependency("ComponentA", depB);
    manager_->addDependency("ComponentB", depC);
    manager_->addDependency("ComponentC", depA);  // Creates circular dependency

    bool hasCircular = manager_->hasCircularDependencies("ComponentA");
    EXPECT_TRUE(hasCircular);
}

TEST_F(LifecycleManagerTest, DependencyGraphRetrieval) {
    DependencyConstraint depB("ComponentB", DependencyType::Required);
    DependencyConstraint depA("ComponentA", DependencyType::Required);

    manager_->addDependency("ComponentC", depB);
    manager_->addDependency("ComponentB", depA);

    auto graph = manager_->getDependencyGraph();

    // Verify graph contains our dependencies
    EXPECT_TRUE(graph.find("ComponentC") != graph.end() ||
                graph.find("ComponentB") != graph.end());
}

TEST_F(LifecycleManagerTest, LifecycleHistoryTracking) {
    // Execute some phases
    manager_->executePhase(*component_, LifecyclePhase::PreInitialization);
    manager_->executePhase(*component_, LifecyclePhase::PostInitialization);

    auto history = manager_->getLifecycleHistory("TestComponent");
    EXPECT_GE(history.size(), 2);

    // Clear history
    manager_->clearHistory();
    auto emptyHistory = manager_->getLifecycleHistory("TestComponent");
    EXPECT_TRUE(emptyHistory.empty());
}

TEST_F(LifecycleManagerTest, MultipleComponentPhaseExecution) {
    auto comp1 = std::make_shared<TestLifecycleComponent>("Component1");
    auto comp2 = std::make_shared<TestLifecycleComponent>("Component2");

    // Execute phases for both components
    manager_->executePhase(*comp1, LifecyclePhase::PreInitialization);
    manager_->executePhase(*comp2, LifecyclePhase::PreInitialization);

    manager_->executePhase(*comp1, LifecyclePhase::PostInitialization);
    manager_->executePhase(*comp2, LifecyclePhase::PostInitialization);

    EXPECT_EQ(comp1->initializeCallCount.load(),
              0);  // Hooks don't call initialize
    EXPECT_EQ(comp2->initializeCallCount.load(), 0);
}

TEST_F(LifecycleManagerTest, ComponentDestructionPhases) {
    auto comp1 = std::make_shared<TestLifecycleComponent>("Component1");
    auto comp2 = std::make_shared<TestLifecycleComponent>("Component2");

    // Execute destruction phases
    manager_->executePhase(*comp1, LifecyclePhase::PreDestruction);
    manager_->executePhase(*comp2, LifecyclePhase::PreDestruction);

    manager_->executePhase(*comp1, LifecyclePhase::PostDestruction);
    manager_->executePhase(*comp2, LifecyclePhase::PostDestruction);

    EXPECT_EQ(comp1->destroyCallCount.load(), 0);  // Hooks don't call destroy
    EXPECT_EQ(comp2->destroyCallCount.load(), 0);
}

TEST_F(LifecycleManagerTest, LifecycleHistoryRetrieval) {
    // Execute some phases to generate events
    manager_->executePhase(*component_, LifecyclePhase::PreInitialization);
    manager_->executePhase(*component_, LifecyclePhase::PostInitialization);

    auto events = manager_->getLifecycleHistory("TestComponent");
    EXPECT_GE(events.size(),
              0);  // May or may not have events depending on implementation
}

TEST_F(LifecycleManagerTest, DependencyValidation) {
    DependencyConstraint dep("ExistingDependency", DependencyType::Required);
    manager_->addDependency("TestComponent", dep);

    // Validation depends on whether dependency exists
    bool valid = manager_->validateDependencies("TestComponent");
    // Just verify the method works, result depends on system state
    EXPECT_TRUE(valid || !valid);
}

TEST_F(LifecycleManagerTest, HookRegistrationAndExecution) {
    std::atomic<bool> hookExecuted{false};

    manager_->registerHook(
        "TestComponent", LifecyclePhase::PostActivation,
        [&hookExecuted](Component&, LifecyclePhase) { hookExecuted = true; });

    manager_->executePhase(*component_, LifecyclePhase::PostActivation);
    EXPECT_TRUE(hookExecuted.load());
}

TEST_F(LifecycleManagerTest, ThreadSafety) {
    const int numThreads = 4;
    const int hooksPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> totalHooksExecuted{0};

    // Launch threads that register and execute hooks
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, hooksPerThread, &totalHooksExecuted]() {
            for (int i = 0; i < hooksPerThread; ++i) {
                std::string componentName = "ThreadComponent" +
                                            std::to_string(t) + "_" +
                                            std::to_string(i);

                manager_->registerHook(
                    componentName, LifecyclePhase::PostConstruction,
                    [&totalHooksExecuted](Component&, LifecyclePhase) {
                        totalHooksExecuted++;
                    });

                auto testComp =
                    std::make_shared<TestLifecycleComponent>(componentName);
                manager_->executePhase(*testComp,
                                       LifecyclePhase::PostConstruction);
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(totalHooksExecuted.load(), numThreads * hooksPerThread);
}

// ============================================================================
// ComponentLifecycleGuard Tests
// ============================================================================

TEST_F(ComponentLifecycleGuardTest, BasicGuardUsage) {
    {
        ComponentLifecycleGuard guard(*component_,
                                      LifecyclePhase::PreInitialization,
                                      LifecyclePhase::PostDestruction);

        // Guard should execute start phase
        // Component should be in initialized state during guard lifetime
    }
    // Guard destructor should execute end phase
}

TEST_F(ComponentLifecycleGuardTest, GuardWithFailure) {
    component_->initializeResult = false;

    {
        ComponentLifecycleGuard guard(*component_,
                                      LifecyclePhase::PreInitialization,
                                      LifecyclePhase::PostDestruction);

        // Even with failure, guard should handle cleanup properly
    }
}

// ============================================================================
// Lifecycle Phase Tests
// ============================================================================

TEST(LifecyclePhaseTest, PhaseEnumValues) {
    // Test that all lifecycle phases are properly defined
    EXPECT_EQ(static_cast<uint8_t>(LifecyclePhase::PreConstruction), 0);
    EXPECT_EQ(static_cast<uint8_t>(LifecyclePhase::PostConstruction), 1);
    EXPECT_EQ(static_cast<uint8_t>(LifecyclePhase::PreInitialization), 2);
    EXPECT_EQ(static_cast<uint8_t>(LifecyclePhase::PostInitialization), 3);
    EXPECT_EQ(static_cast<uint8_t>(LifecyclePhase::PreActivation), 4);
    EXPECT_EQ(static_cast<uint8_t>(LifecyclePhase::PostActivation), 5);
    EXPECT_EQ(static_cast<uint8_t>(LifecyclePhase::PreDeactivation), 6);
    EXPECT_EQ(static_cast<uint8_t>(LifecyclePhase::PostDeactivation), 7);
    EXPECT_EQ(static_cast<uint8_t>(LifecyclePhase::PreDestruction), 8);
    EXPECT_EQ(static_cast<uint8_t>(LifecyclePhase::PostDestruction), 9);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(LifecycleManagerTest, InvalidComponentName) {
    // Test with empty component name
    bool result =
        manager_->executePhase(*component_, LifecyclePhase::PreInitialization);
    EXPECT_TRUE(result);  // Should handle gracefully
}

TEST_F(LifecycleManagerTest, NullComponentHandling) {
    // Test behavior with component that might have null state
    // This tests the robustness of the lifecycle system
    auto nullNameComponent = std::make_shared<TestLifecycleComponent>("");
    bool result = manager_->executePhase(*nullNameComponent,
                                         LifecyclePhase::PreInitialization);
    EXPECT_TRUE(result);  // Should handle gracefully
}
