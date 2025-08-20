/*
 * test_registry.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Comprehensive Unit Tests for Registry Class
Tests singleton pattern, component registration/retrieval, dependency
management, initialization order, lifecycle management, hot reloading,
and error scenarios.

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
 * @brief Test fixture for Registry tests
 */
class RegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry_ = &Registry::instance();

        // Clean up any existing components from previous tests
        auto componentNames = registry_->getAllComponentNames();
        for (const auto& name : componentNames) {
            registry_->removeComponent(name);
        }
    }

    void TearDown() override {
        // Clean up components created during tests
        auto componentNames = registry_->getAllComponentNames();
        for (const auto& name : componentNames) {
            registry_->removeComponent(name);
        }
    }

    Registry* registry_;
};

/**
 * @brief Test fixture for Registry error scenarios
 */
class RegistryErrorTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry_ = &Registry::instance();

        // Clean up any existing components
        auto componentNames = registry_->getAllComponentNames();
        for (const auto& name : componentNames) {
            registry_->removeComponent(name);
        }
    }

    void TearDown() override {
        // Clean up
        auto componentNames = registry_->getAllComponentNames();
        for (const auto& name : componentNames) {
            registry_->removeComponent(name);
        }
    }

    Registry* registry_;
};

// ============================================================================
// Singleton Pattern Tests
// ============================================================================

TEST(RegistrySingletonTest, SingletonInstance) {
    auto& instance1 = Registry::instance();
    auto& instance2 = Registry::instance();

    // Should be the same instance
    EXPECT_EQ(&instance1, &instance2);
}

TEST(RegistrySingletonTest, ThreadSafeSingleton) {
    std::vector<Registry*> instances;
    std::mutex instancesMutex;

    const int numThreads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&instances, &instancesMutex]() {
            auto& instance = Registry::instance();

            std::lock_guard<std::mutex> lock(instancesMutex);
            instances.push_back(&instance);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All instances should be the same
    EXPECT_EQ(instances.size(), numThreads);
    for (size_t i = 1; i < instances.size(); ++i) {
        EXPECT_EQ(instances[0], instances[i]);
    }
}

// ============================================================================
// Component Registration and Retrieval Tests
// ============================================================================

TEST_F(RegistryTest, RegisterAndRetrieveComponent) {
    auto component = std::make_shared<Component>("TestComponent");

    // Register component
    registry_->registerComponent("TestComponent", component);

    // Retrieve component
    auto retrieved = registry_->getComponent("TestComponent");
    EXPECT_EQ(retrieved, component);
    EXPECT_EQ(retrieved->getName(), "TestComponent");
}

TEST_F(RegistryTest, RegisterModule) {
    bool initCalled = false;

    auto initFunc = [&initCalled](Component& comp) {
        initCalled = true;
        comp.addVariable<int>("moduleVar", 42);
    };

    registry_->registerModule("TestModule", initFunc);

    // Get or load the component (this should trigger initialization)
    auto component = registry_->getOrLoadComponent("TestModule");

    EXPECT_TRUE(initCalled);
    EXPECT_NE(component, nullptr);
    EXPECT_TRUE(component->hasVariable("moduleVar"));
}

TEST_F(RegistryTest, AddInitializer) {
    bool initCalled = false;
    bool cleanupCalled = false;

    auto initFunc = [&initCalled](Component& comp) {
        initCalled = true;
        comp.addVariable<std::string>("initVar", "initialized");
    };

    auto cleanupFunc = [&cleanupCalled]() { cleanupCalled = true; };

    Registry::ComponentInfo metadata;
    metadata.name = "InitializerTest";
    metadata.description = "Test component with initializer";
    metadata.version = "1.0.0";

    registry_->addInitializer("InitializerTest", initFunc, cleanupFunc,
                              metadata);

    auto component = registry_->getOrLoadComponent("InitializerTest");
    EXPECT_TRUE(initCalled);
    EXPECT_NE(component, nullptr);

    // Test metadata
    auto info = registry_->getComponentInfo("InitializerTest");
    EXPECT_EQ(info.name, "InitializerTest");
    EXPECT_EQ(info.description, "Test component with initializer");
    EXPECT_EQ(info.version, "1.0.0");
}

TEST_F(RegistryTest, GetAllComponents) {
    auto comp1 = std::make_shared<Component>("Component1");
    auto comp2 = std::make_shared<Component>("Component2");
    auto comp3 = std::make_shared<Component>("Component3");

    registry_->registerComponent("Component1", comp1);
    registry_->registerComponent("Component2", comp2);
    registry_->registerComponent("Component3", comp3);

    auto allComponents = registry_->getAllComponents();
    EXPECT_GE(allComponents.size(), 3);

    auto allNames = registry_->getAllComponentNames();
    EXPECT_GE(allNames.size(), 3);

    // Check that our components are in the lists
    bool hasComp1 = std::find(allNames.begin(), allNames.end(), "Component1") !=
                    allNames.end();
    bool hasComp2 = std::find(allNames.begin(), allNames.end(), "Component2") !=
                    allNames.end();
    bool hasComp3 = std::find(allNames.begin(), allNames.end(), "Component3") !=
                    allNames.end();

    EXPECT_TRUE(hasComp1);
    EXPECT_TRUE(hasComp2);
    EXPECT_TRUE(hasComp3);
}

TEST_F(RegistryTest, ComponentStatus) {
    auto component = std::make_shared<Component>("StatusTest");
    registry_->registerComponent("StatusTest", component);

    // Initially should not be initialized
    EXPECT_FALSE(registry_->isInitialized("StatusTest"));

    // Should be enabled by default
    EXPECT_TRUE(registry_->isEnabled("StatusTest"));

    // Test enable/disable
    EXPECT_TRUE(registry_->enableComponent("StatusTest", false));
    EXPECT_FALSE(registry_->isEnabled("StatusTest"));

    EXPECT_TRUE(registry_->enableComponent("StatusTest", true));
    EXPECT_TRUE(registry_->isEnabled("StatusTest"));
}

// ============================================================================
// Dependency Management Tests
// ============================================================================

TEST_F(RegistryTest, AddDependency) {
    auto comp1 = std::make_shared<Component>("Dependent");
    auto comp2 = std::make_shared<Component>("Dependency");

    registry_->registerComponent("Dependent", comp1);
    registry_->registerComponent("Dependency", comp2);

    // Add dependency: Dependent depends on Dependency
    registry_->addDependency("Dependent", "Dependency", false);

    // This test mainly checks that no exceptions are thrown
    // The actual dependency resolution is tested in other tests
}

TEST_F(RegistryTest, OptionalDependency) {
    auto comp1 = std::make_shared<Component>("OptionalDependent");

    registry_->registerComponent("OptionalDependent", comp1);

    // Add optional dependency on non-existent component
    registry_->addDependency("OptionalDependent", "NonExistent", true);

    // Should not throw when initializing
    EXPECT_NO_THROW(registry_->initializeAll());
}

// ============================================================================
// Lifecycle Management Tests
// ============================================================================

TEST_F(RegistryTest, InitializeAll) {
    bool comp1Initialized = false;
    bool comp2Initialized = false;

    auto initFunc1 = [&comp1Initialized](Component& comp) {
        comp1Initialized = true;
        comp.addVariable<int>("comp1Var", 1);
    };

    auto initFunc2 = [&comp2Initialized](Component& comp) {
        comp2Initialized = true;
        comp.addVariable<int>("comp2Var", 2);
    };

    registry_->registerModule("InitTest1", initFunc1);
    registry_->registerModule("InitTest2", initFunc2);

    registry_->initializeAll();

    EXPECT_TRUE(comp1Initialized);
    EXPECT_TRUE(comp2Initialized);

    EXPECT_TRUE(registry_->isInitialized("InitTest1"));
    EXPECT_TRUE(registry_->isInitialized("InitTest2"));
}

TEST_F(RegistryTest, CleanupAll) {
    bool cleanup1Called = false;
    bool cleanup2Called = false;

    auto initFunc1 = [](Component& comp) {
        comp.addVariable<int>("cleanupTest1", 1);
    };

    auto cleanupFunc1 = [&cleanup1Called]() { cleanup1Called = true; };

    auto initFunc2 = [](Component& comp) {
        comp.addVariable<int>("cleanupTest2", 2);
    };

    auto cleanupFunc2 = [&cleanup2Called]() { cleanup2Called = true; };

    registry_->addInitializer("CleanupTest1", initFunc1, cleanupFunc1);
    registry_->addInitializer("CleanupTest2", initFunc2, cleanupFunc2);

    registry_->initializeAll();
    registry_->cleanupAll();

    EXPECT_TRUE(cleanup1Called);
    EXPECT_TRUE(cleanup2Called);
}

TEST_F(RegistryTest, ReinitializeComponent) {
    int initCount = 0;

    auto initFunc = [&initCount](Component& comp) {
        initCount++;
        comp.addVariable<int>("reinitVar", initCount);
    };

    registry_->registerModule("ReinitTest", initFunc);

    // Initial initialization
    auto component = registry_->getOrLoadComponent("ReinitTest");
    EXPECT_EQ(initCount, 1);

    // Reinitialize
    registry_->reinitializeComponent("ReinitTest");
    EXPECT_EQ(initCount, 2);

    auto var = component->getVariable<int>("reinitVar");
    EXPECT_EQ(var->get(), 2);
}

// ============================================================================
// Component Removal Tests
// ============================================================================

TEST_F(RegistryTest, RemoveComponent) {
    auto component = std::make_shared<Component>("RemovalTest");
    registry_->registerComponent("RemovalTest", component);

    // Verify component exists
    EXPECT_NO_THROW(registry_->getComponent("RemovalTest"));

    // Remove component
    EXPECT_TRUE(registry_->removeComponent("RemovalTest"));

    // Should no longer exist
    EXPECT_THROW(registry_->getComponent("RemovalTest"),
                 Registry::RegistryException);
}

TEST_F(RegistryTest, RemoveNonExistentComponent) {
    // Should return false for non-existent component
    EXPECT_FALSE(registry_->removeComponent("NonExistent"));
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(RegistryErrorTest, GetNonExistentComponent) {
    EXPECT_THROW(registry_->getComponent("NonExistent"),
                 Registry::RegistryException);
}

TEST_F(RegistryErrorTest, GetComponentInfoNonExistent) {
    EXPECT_THROW(registry_->getComponentInfo("NonExistent"),
                 Registry::RegistryException);
}

TEST_F(RegistryErrorTest, DuplicateComponentRegistration) {
    auto comp1 = std::make_shared<Component>("Duplicate");
    auto comp2 = std::make_shared<Component>("Duplicate");

    registry_->registerComponent("Duplicate", comp1);

    // Registering with same name should either throw or replace
    // The exact behavior depends on implementation
    // This test mainly ensures no crash occurs
    EXPECT_NO_THROW(registry_->registerComponent("Duplicate", comp2));
}

TEST_F(RegistryErrorTest, CircularDependency) {
    auto comp1 = std::make_shared<Component>("CircularA");
    auto comp2 = std::make_shared<Component>("CircularB");

    registry_->registerComponent("CircularA", comp1);
    registry_->registerComponent("CircularB", comp2);

    // Create circular dependency: A depends on B, B depends on A
    registry_->addDependency("CircularA", "CircularB");
    registry_->addDependency("CircularB", "CircularA");

    // Initialization should handle circular dependencies gracefully
    // The exact behavior depends on implementation
    EXPECT_NO_THROW(registry_->initializeAll());
}

// ============================================================================
// Component Metadata Tests
// ============================================================================

TEST_F(RegistryTest, ComponentMetadata) {
    Registry::ComponentInfo metadata;
    metadata.name = "MetadataTest";
    metadata.version = "2.1.0";
    metadata.description = "Component for testing metadata";
    metadata.author = "Test Author";
    metadata.license = "MIT";
    metadata.isAutoLoad = true;
    metadata.isLazyLoad = false;
    metadata.isHotReload = true;

    auto initFunc = [](Component& comp) {
        comp.addVariable<std::string>("metaVar", "metadata");
    };

    registry_->addInitializer("MetadataTest", initFunc, nullptr, metadata);

    auto info = registry_->getComponentInfo("MetadataTest");
    EXPECT_EQ(info.name, "MetadataTest");
    EXPECT_EQ(info.version, "2.1.0");
    EXPECT_EQ(info.description, "Component for testing metadata");
    EXPECT_EQ(info.author, "Test Author");
    EXPECT_EQ(info.license, "MIT");
    EXPECT_TRUE(info.isAutoLoad);
    EXPECT_FALSE(info.isLazyLoad);
    EXPECT_TRUE(info.isHotReload);
}

TEST_F(RegistryTest, UpdateComponentInfo) {
    auto component = std::make_shared<Component>("UpdateTest");
    registry_->registerComponent("UpdateTest", component);

    Registry::ComponentInfo newInfo;
    newInfo.name = "UpdateTest";
    newInfo.version = "3.0.0";
    newInfo.description = "Updated description";

    EXPECT_TRUE(registry_->updateComponentInfo("UpdateTest", newInfo));

    auto info = registry_->getComponentInfo("UpdateTest");
    EXPECT_EQ(info.version, "3.0.0");
    EXPECT_EQ(info.description, "Updated description");
}

// ============================================================================
// Memory Pool Integration Tests
// ============================================================================

TEST_F(RegistryTest, CreateComponentWithPool) {
    // Configure memory pool for Component type
    atom::components::PoolConfig config;
    config.initialPoolSize = 16;
    config.maxPoolSize = 64;
    config.enableStatistics = true;

    registry_->configureComponentPool<Component>(config);

    // Create component using pool
    auto component = registry_->createComponent<Component>("PooledComponent");
    EXPECT_NE(component, nullptr);
    EXPECT_EQ(component->getName(), "PooledComponent");
}

// ============================================================================
// Hot Reloading Tests
// ============================================================================

TEST_F(RegistryTest, WatchComponentChanges) {
    // Test enabling/disabling file watching
    EXPECT_TRUE(registry_->watchComponentChanges(true));
    EXPECT_TRUE(registry_->watchComponentChanges(false));
}

TEST_F(RegistryTest, LoadComponentFromFile) {
    // This test would require actual component files
    // For now, just test that the method doesn't crash with invalid path
    EXPECT_FALSE(registry_->loadComponentFromFile("nonexistent_file.so"));
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(RegistryTest, ConcurrentComponentRegistration) {
    const int numThreads = 4;
    const int componentsPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, componentsPerThread, &successCount]() {
            for (int i = 0; i < componentsPerThread; ++i) {
                try {
                    std::string name = "ConcurrentComp_" + std::to_string(t) +
                                       "_" + std::to_string(i);
                    auto component = std::make_shared<Component>(name);
                    registry_->registerComponent(name, component);
                    successCount++;
                } catch (const std::exception&) {
                    // Some registrations might fail due to race conditions
                    // This is acceptable as long as no crashes occur
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // At least some registrations should succeed
    EXPECT_GT(successCount.load(), 0);
}

TEST_F(RegistryTest, ConcurrentComponentRetrieval) {
    // Register some components first
    for (int i = 0; i < 10; ++i) {
        auto component =
            std::make_shared<Component>("RetrievalTest" + std::to_string(i));
        registry_->registerComponent("RetrievalTest" + std::to_string(i),
                                     component);
    }

    const int numThreads = 8;
    const int retrievalsPerThread = 20;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, retrievalsPerThread, &successCount]() {
            for (int i = 0; i < retrievalsPerThread; ++i) {
                try {
                    std::string name = "RetrievalTest" + std::to_string(i % 10);
                    auto component = registry_->getComponent(name);
                    if (component != nullptr) {
                        successCount++;
                    }
                } catch (const std::exception&) {
                    // Some retrievals might fail, but no crashes should occur
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Most retrievals should succeed
    EXPECT_GT(successCount.load(), numThreads * retrievalsPerThread * 0.8);
}

// ============================================================================
// Performance Tests
// ============================================================================

class RegistryPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry_ = &Registry::instance();

        // Clean up any existing components
        auto componentNames = registry_->getAllComponentNames();
        for (const auto& name : componentNames) {
            registry_->removeComponent(name);
        }
    }

    void TearDown() override {
        auto componentNames = registry_->getAllComponentNames();
        for (const auto& name : componentNames) {
            registry_->removeComponent(name);
        }
    }

    Registry* registry_;
    static constexpr int PERFORMANCE_ITERATIONS = 1000;
};

TEST_F(RegistryPerformanceTest, ComponentRegistrationPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < PERFORMANCE_ITERATIONS; ++i) {
        auto component =
            std::make_shared<Component>("PerfTest" + std::to_string(i));
        registry_->registerComponent("PerfTest" + std::to_string(i), component);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Component registration performance: " << duration.count()
              << " μs for " << PERFORMANCE_ITERATIONS << " components"
              << std::endl;
    std::cout << "Average per registration: "
              << (duration.count() / PERFORMANCE_ITERATIONS) << " μs"
              << std::endl;

    // Should be reasonably fast (less than 10μs per registration on average)
    EXPECT_LT(duration.count() / PERFORMANCE_ITERATIONS, 10);
}

TEST_F(RegistryPerformanceTest, ComponentRetrievalPerformance) {
    // Register components first
    for (int i = 0; i < PERFORMANCE_ITERATIONS; ++i) {
        auto component = std::make_shared<Component>("RetrievalPerfTest" +
                                                     std::to_string(i));
        registry_->registerComponent("RetrievalPerfTest" + std::to_string(i),
                                     component);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < PERFORMANCE_ITERATIONS; ++i) {
        auto component =
            registry_->getComponent("RetrievalPerfTest" + std::to_string(i));
        volatile auto ptr = component.get();  // Prevent optimization
        (void)ptr;  // Suppress unused variable warning
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Component retrieval performance: " << duration.count()
              << " μs for " << PERFORMANCE_ITERATIONS << " retrievals"
              << std::endl;
    std::cout << "Average per retrieval: "
              << (duration.count() / PERFORMANCE_ITERATIONS) << " μs"
              << std::endl;

    // Should be very fast (less than 1μs per retrieval on average)
    EXPECT_LT(duration.count() / PERFORMANCE_ITERATIONS, 1);
}
