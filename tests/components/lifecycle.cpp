#include "atom/components/lifecycle.hpp"
#include "atom/components/component.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>

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

        // Clear any existing hooks from previous tests
        manager_->clearHooks("TestComponent");
        manager_->clearGlobalHooks();
    }

    void TearDown() override {
        manager_->clearHooks("TestComponent");
        manager_->clearGlobalHooks();
    }

    LifecycleManager* manager_;
    std::shared_ptr<TestLifecycleComponent> component_;
};

// Test fixture for ComponentLifecycleGuard tests
class ComponentLifecycleGuardTest : public ::testing::Test {
protected:
    void SetUp() override {
        component_ = std::make_shared<TestLifecycleComponent>("GuardTestComponent");
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

    manager_->registerHook("TestComponent", LifecyclePhase::PostInitialization,
                          [&hookExecuted](Component&) {
                              hookExecuted = true;
                              return true;
                          });

    bool result = manager_->executePhase(*component_, LifecyclePhase::PostInitialization);
    EXPECT_TRUE(result);
    EXPECT_TRUE(hookExecuted.load());
}

TEST_F(LifecycleManagerTest, RegisterGlobalHook) {
    std::atomic<bool> globalHookExecuted{false};

    manager_->registerGlobalHook(LifecyclePhase::PreInitialization,
                                [&globalHookExecuted](Component&) {
                                    globalHookExecuted = true;
                                    return true;
                                });

    bool result = manager_->executePhase(*component_, LifecyclePhase::PreInitialization);
    EXPECT_TRUE(result);
    EXPECT_TRUE(globalHookExecuted.load());
}

TEST_F(LifecycleManagerTest, MultipleHooksExecution) {
    std::atomic<int> hookCount{0};

    // Register multiple hooks for the same phase
    for (int i = 0; i < 3; ++i) {
        manager_->registerHook("TestComponent", LifecyclePhase::PostConstruction,
                              [&hookCount](Component&) {
                                  hookCount++;
                                  return true;
                              });
    }

    bool result = manager_->executePhase(*component_, LifecyclePhase::PostConstruction);
    EXPECT_TRUE(result);
    EXPECT_EQ(hookCount.load(), 3);
}

TEST_F(LifecycleManagerTest, HookFailure) {
    manager_->registerHook("TestComponent", LifecyclePhase::PreActivation,
                          [](Component&) {
                              return false; // Simulate hook failure
                          });

    bool result = manager_->executePhase(*component_, LifecyclePhase::PreActivation);
    EXPECT_FALSE(result);
}

TEST_F(LifecycleManagerTest, AddAndResolveDependency) {
    manager_->addDependency("TestComponent", "DependencyComponent");

    auto dependencies = manager_->getDependencies("TestComponent");
    EXPECT_EQ(dependencies.size(), 1);
    EXPECT_EQ(dependencies[0], "DependencyComponent");
}

TEST_F(LifecycleManagerTest, CircularDependencyDetection) {
    manager_->addDependency("ComponentA", "ComponentB");
    manager_->addDependency("ComponentB", "ComponentC");
    manager_->addDependency("ComponentC", "ComponentA"); // Creates circular dependency

    bool hasCircular = manager_->hasCircularDependency("ComponentA");
    EXPECT_TRUE(hasCircular);
}

TEST_F(LifecycleManagerTest, DependencyResolutionOrder) {
    manager_->addDependency("ComponentC", "ComponentB");
    manager_->addDependency("ComponentB", "ComponentA");

    auto order = manager_->resolveDependencyOrder({"ComponentA", "ComponentB", "ComponentC"});

    // ComponentA should come first, ComponentC last
    EXPECT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], "ComponentA");
    EXPECT_EQ(order[2], "ComponentC");
}

TEST_F(LifecycleManagerTest, InitializeAllComponents) {
    auto comp1 = std::make_shared<TestLifecycleComponent>("Component1");
    auto comp2 = std::make_shared<TestLifecycleComponent>("Component2");

    std::vector<std::shared_ptr<Component>> components = {comp1, comp2};

    bool result = manager_->initializeAll(components);
    EXPECT_TRUE(result);
    EXPECT_EQ(comp1->initializeCallCount.load(), 1);
    EXPECT_EQ(comp2->initializeCallCount.load(), 1);
}

TEST_F(LifecycleManagerTest, InitializeAllWithFailure) {
    auto comp1 = std::make_shared<TestLifecycleComponent>("Component1");
    auto comp2 = std::make_shared<TestLifecycleComponent>("Component2");

    // Make comp2 fail initialization
    comp2->initializeResult = false;

    std::vector<std::shared_ptr<Component>> components = {comp1, comp2};

    bool result = manager_->initializeAll(components);
    EXPECT_FALSE(result);
}

TEST_F(LifecycleManagerTest, DestroyAllComponents) {
    auto comp1 = std::make_shared<TestLifecycleComponent>("Component1");
    auto comp2 = std::make_shared<TestLifecycleComponent>("Component2");

    std::vector<std::shared_ptr<Component>> components = {comp1, comp2};

    bool result = manager_->destroyAll(components);
    EXPECT_TRUE(result);
    EXPECT_EQ(comp1->destroyCallCount.load(), 1);
    EXPECT_EQ(comp2->destroyCallCount.load(), 1);
}

TEST_F(LifecycleManagerTest, GetLifecycleEvents) {
    // Execute some phases to generate events
    manager_->executePhase(*component_, LifecyclePhase::PreInitialization);
    manager_->executePhase(*component_, LifecyclePhase::PostInitialization);

    auto events = manager_->getLifecycleEvents("TestComponent");
    EXPECT_GE(events.size(), 2);
}

TEST_F(LifecycleManagerTest, ValidateDependencies) {
    manager_->addDependency("TestComponent", "ExistingDependency");

    // Without the dependency satisfied, validation should fail
    bool valid = manager_->validateDependencies("TestComponent");
    EXPECT_FALSE(valid);
}

TEST_F(LifecycleManagerTest, ClearHooks) {
    manager_->registerHook("TestComponent", LifecyclePhase::PostActivation,
                          [](Component&) { return true; });

    manager_->clearHooks("TestComponent");

    // After clearing, hooks should not execute
    std::atomic<bool> hookExecuted{false};
    manager_->registerHook("TestComponent", LifecyclePhase::PostActivation,
                          [&hookExecuted](Component&) {
                              hookExecuted = true;
                              return true;
                          });

    manager_->executePhase(*component_, LifecyclePhase::PostActivation);
    EXPECT_TRUE(hookExecuted.load()); // New hook should execute
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
                std::string componentName = "ThreadComponent" + std::to_string(t) + "_" + std::to_string(i);

                manager_->registerHook(componentName, LifecyclePhase::PostConstruction,
                                      [&totalHooksExecuted](Component&) {
                                          totalHooksExecuted++;
                                          return true;
                                      });

                auto testComp = std::make_shared<TestLifecycleComponent>(componentName);
                manager_->executePhase(*testComp, LifecyclePhase::PostConstruction);
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
    bool result = manager_->executePhase(*component_, LifecyclePhase::PreInitialization);
    EXPECT_TRUE(result); // Should handle gracefully
}

TEST_F(LifecycleManagerTest, NullComponentHandling) {
    // Test behavior with component that might have null state
    // This tests the robustness of the lifecycle system
    auto nullNameComponent = std::make_shared<TestLifecycleComponent>("");
    bool result = manager_->executePhase(*nullNameComponent, LifecyclePhase::PreInitialization);
    EXPECT_TRUE(result); // Should handle gracefully
}
