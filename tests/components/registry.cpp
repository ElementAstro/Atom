#include "atom/components/core/registry.hpp"

#include <gtest/gtest.h>

#include "atom/components/core/component.hpp"
#include "atom/error/exception.hpp"

using namespace atom::components;

class TestComponent : public Component {
public:
    TestComponent(const std::string& name) : Component(name) {}
    bool initialized = false;
    bool cleaned_up = false;
};

TEST(RegistryTest, AddAndGetComponent) {
    auto& registry = Registry::instance();
    registry.addInitializer("Component1", [](Component&) {}, []() {});
    auto component = registry.getComponent("Component1");
    EXPECT_EQ(component->getName(), "Component1");
}

/*
TEST(RegistryTest, InitializeAndCleanupComponent) {
    auto& registry = Registry::instance();
    registry.cleanupAll();
    auto testComponent = std::make_shared<TestComponent>("Component1");

    registry.addInitializer(
        "Component1", [testComponent]() { testComponent->initialized = true; },
        [testComponent]() { testComponent->cleaned_up = true; });

    registry.initializeAll();
    EXPECT_TRUE(testComponent->initialized);

    registry.cleanupAll();
    EXPECT_TRUE(testComponent->cleaned_up);
}
*/

/*
TEST(RegistryTest, ReinitializeComponent) {
    auto& registry = Registry::instance();
    auto testComponent = std::make_shared<TestComponent>("Component1");

    registry.addInitializer(
        "Component1", [testComponent]() { testComponent->initialized = true; },
        [testComponent]() { testComponent->cleaned_up = true; });

    registry.initializeAll();
    EXPECT_TRUE(testComponent->initialized);

    testComponent->initialized = false;
    registry.reinitializeComponent("Component1");
    EXPECT_TRUE(testComponent->initialized);
}

*/

TEST(RegistryTest, CircularDependencyDetection) {
    auto& registry = Registry::instance();
    registry.addInitializer("Component1", [](Component&) {}, []() {});
    registry.addInitializer("Component2", [](Component&) {}, []() {});

    registry.addDependency("Component1", "Component2");
    EXPECT_THROW(registry.addDependency("Component2", "Component1"),
                 atom::error::RuntimeError);
}

TEST(RegistryTest, DependencyInitializationOrder) {
    auto& registry = Registry::instance();
    registry.cleanupAll();
    std::vector<std::string> initialization_order;

    registry.addInitializer(
        "ComponentA",
        [&](Component&) { initialization_order.emplace_back("ComponentA"); },
        []() {});
    registry.addInitializer(
        "ComponentB",
        [&](Component&) { initialization_order.emplace_back("ComponentB"); },
        []() {});
    registry.addInitializer(
        "ComponentC",
        [&](Component&) { initialization_order.emplace_back("ComponentC"); },
        []() {});

    registry.addDependency("ComponentA", "ComponentB");
    registry.addDependency("ComponentB", "ComponentC");

    registry.initializeAll();

    ASSERT_EQ(initialization_order.size(), 3);
    EXPECT_EQ(initialization_order[0], "ComponentC");
    EXPECT_EQ(initialization_order[1], "ComponentB");
    EXPECT_EQ(initialization_order[2], "ComponentA");
}

// ============================================================================
// Extended Registry Tests - Core Functionality
// ============================================================================

TEST(RegistryTest, IsInitialized) {
    auto& registry = Registry::instance();
    registry.addInitializer("TestInit", [](Component&) {}, []() {});

    // Before initialization
    EXPECT_FALSE(registry.isInitialized("TestInit"));

    registry.initializeAll();

    // After initialization
    EXPECT_TRUE(registry.isInitialized("TestInit"));
}

TEST(RegistryTest, IsEnabled) {
    auto& registry = Registry::instance();
    registry.addInitializer("TestEnabled", [](Component&) {}, []() {});

    // Components should be enabled by default
    EXPECT_TRUE(registry.isEnabled("TestEnabled"));
}

TEST(RegistryTest, EnableDisableComponent) {
    auto& registry = Registry::instance();
    registry.addInitializer("TestEnableDisable", [](Component&) {}, []() {});

    // Disable component
    EXPECT_TRUE(registry.enableComponent("TestEnableDisable", false));
    EXPECT_FALSE(registry.isEnabled("TestEnableDisable"));

    // Re-enable component
    EXPECT_TRUE(registry.enableComponent("TestEnableDisable", true));
    EXPECT_TRUE(registry.isEnabled("TestEnableDisable"));
}

TEST(RegistryTest, GetAllComponentNames) {
    auto& registry = Registry::instance();
    registry.cleanupAll();

    registry.addInitializer("Comp1", [](Component&) {}, []() {});
    registry.addInitializer("Comp2", [](Component&) {}, []() {});
    registry.addInitializer("Comp3", [](Component&) {}, []() {});

    auto names = registry.getAllComponentNames();
    EXPECT_GE(names.size(), 3);

    // Check that our components are in the list
    bool hasComp1 =
        std::find(names.begin(), names.end(), "Comp1") != names.end();
    bool hasComp2 =
        std::find(names.begin(), names.end(), "Comp2") != names.end();
    bool hasComp3 =
        std::find(names.begin(), names.end(), "Comp3") != names.end();

    EXPECT_TRUE(hasComp1);
    EXPECT_TRUE(hasComp2);
    EXPECT_TRUE(hasComp3);
}

TEST(RegistryTest, GetAllComponents) {
    auto& registry = Registry::instance();
    registry.cleanupAll();

    registry.addInitializer("CompA", [](Component&) {}, []() {});
    registry.addInitializer("CompB", [](Component&) {}, []() {});

    auto components = registry.getAllComponents();
    EXPECT_GE(components.size(), 2);
}

TEST(RegistryTest, RemoveComponent) {
    auto& registry = Registry::instance();
    registry.addInitializer("ToRemove", [](Component&) {}, []() {});

    // Component should exist
    auto comp = registry.getComponent("ToRemove");
    EXPECT_NE(comp, nullptr);

    // Remove it
    EXPECT_TRUE(registry.removeComponent("ToRemove"));

    // Should no longer exist
    EXPECT_THROW(registry.getComponent("ToRemove"),
                 Registry::RegistryException);
}

// ============================================================================
// Extended Registry Tests - Dependency Management
// ============================================================================

TEST(RegistryTest, OptionalDependency) {
    auto& registry = Registry::instance();
    registry.cleanupAll();

    registry.addInitializer("MainComp", [](Component&) {}, []() {});
    registry.addInitializer("OptionalComp", [](Component&) {}, []() {});

    // Add optional dependency
    registry.addDependency("MainComp", "OptionalComp", true);

    // Should initialize successfully even if optional dependency fails
    EXPECT_NO_THROW(registry.initializeAll());
}

TEST(RegistryTest, MultipleDependencies) {
    auto& registry = Registry::instance();
    registry.cleanupAll();
    std::vector<std::string> init_order;

    registry.addInitializer(
        "Root", [&](Component&) { init_order.push_back("Root"); }, []() {});
    registry.addInitializer(
        "Dep1", [&](Component&) { init_order.push_back("Dep1"); }, []() {});
    registry.addInitializer(
        "Dep2", [&](Component&) { init_order.push_back("Dep2"); }, []() {});
    registry.addInitializer(
        "Dep3", [&](Component&) { init_order.push_back("Dep3"); }, []() {});

    // Root depends on all three
    registry.addDependency("Root", "Dep1");
    registry.addDependency("Root", "Dep2");
    registry.addDependency("Root", "Dep3");

    registry.initializeAll();

    // Root should be initialized last
    EXPECT_EQ(init_order.back(), "Root");
}

TEST(RegistryTest, DiamondDependency) {
    auto& registry = Registry::instance();
    registry.cleanupAll();
    std::vector<std::string> init_order;

    // Diamond: A depends on B and C, both B and C depend on D
    registry.addInitializer(
        "A", [&](Component&) { init_order.push_back("A"); }, []() {});
    registry.addInitializer(
        "B", [&](Component&) { init_order.push_back("B"); }, []() {});
    registry.addInitializer(
        "C", [&](Component&) { init_order.push_back("C"); }, []() {});
    registry.addInitializer(
        "D", [&](Component&) { init_order.push_back("D"); }, []() {});

    registry.addDependency("A", "B");
    registry.addDependency("A", "C");
    registry.addDependency("B", "D");
    registry.addDependency("C", "D");

    registry.initializeAll();

    // D should be first, A should be last
    EXPECT_EQ(init_order.front(), "D");
    EXPECT_EQ(init_order.back(), "A");
}

// ============================================================================
// Extended Registry Tests - Error Handling
// ============================================================================

TEST(RegistryTest, GetNonExistentComponent) {
    auto& registry = Registry::instance();
    EXPECT_THROW(registry.getComponent("NonExistent"),
                 Registry::RegistryException);
}

TEST(RegistryTest, AddDependencyToNonExistentComponent) {
    auto& registry = Registry::instance();

    // Adding dependency to non-existent component should handle gracefully
    // Behavior depends on implementation - may throw or ignore
    try {
        registry.addDependency("NonExistent1", "NonExistent2");
    } catch (const std::exception&) {
        // Expected behavior
    }
}

TEST(RegistryTest, RemoveNonExistentComponent) {
    auto& registry = Registry::instance();

    // Removing non-existent component should return false or handle gracefully
    bool result = registry.removeComponent("DoesNotExist");
    EXPECT_FALSE(result);
}

TEST(RegistryTest, InitializeAllWithForceReload) {
    auto& registry = Registry::instance();
    registry.cleanupAll();

    std::atomic<int> initCount{0};
    registry.addInitializer(
        "ReloadTest", [&](Component&) { initCount++; }, []() {});

    registry.initializeAll(false);
    EXPECT_EQ(initCount.load(), 1);

    // Initialize again without force - should not reinitialize
    registry.initializeAll(false);
    EXPECT_EQ(initCount.load(), 1);

    // Initialize with force - should reinitialize
    registry.initializeAll(true);
    EXPECT_GT(initCount.load(), 1);
}

TEST(RegistryTest, CleanupAllWithForce) {
    auto& registry = Registry::instance();
    registry.cleanupAll();

    std::atomic<int> cleanupCount{0};
    registry.addInitializer(
        "CleanupTest", [](Component&) {}, [&]() { cleanupCount++; });

    registry.initializeAll();
    registry.cleanupAll(false);
    EXPECT_EQ(cleanupCount.load(), 1);

    // Cleanup again with force
    registry.cleanupAll(true);
    EXPECT_GT(cleanupCount.load(), 1);
}

// ============================================================================
// Extended Registry Tests - Component Metadata
// ============================================================================

TEST(RegistryTest, GetComponentInfo) {
    auto& registry = Registry::instance();

    Registry::ComponentInfo info;
    info.name = "MetadataTest";
    info.version = "1.0.0";
    info.description = "Test component with metadata";
    info.author = "Test Author";
    info.license = "MIT";

    registry.addInitializer("MetadataTest", [](Component&) {}, []() {}, info);

    const auto& retrievedInfo = registry.getComponentInfo("MetadataTest");
    EXPECT_EQ(retrievedInfo.name, "MetadataTest");
    EXPECT_EQ(retrievedInfo.version, "1.0.0");
    EXPECT_EQ(retrievedInfo.description, "Test component with metadata");
    EXPECT_EQ(retrievedInfo.author, "Test Author");
    EXPECT_EQ(retrievedInfo.license, "MIT");
}

TEST(RegistryTest, UpdateComponentInfo) {
    auto& registry = Registry::instance();

    registry.addInitializer("UpdateInfoTest", [](Component&) {}, []() {});

    Registry::ComponentInfo newInfo;
    newInfo.name = "UpdateInfoTest";
    newInfo.version = "2.0.0";
    newInfo.description = "Updated description";

    EXPECT_TRUE(registry.updateComponentInfo("UpdateInfoTest", newInfo));

    const auto& info = registry.getComponentInfo("UpdateInfoTest");
    EXPECT_EQ(info.version, "2.0.0");
    EXPECT_EQ(info.description, "Updated description");
}

TEST(RegistryTest, ComponentInfoWithDependencies) {
    auto& registry = Registry::instance();

    Registry::ComponentInfo info;
    info.name = "DependentComp";
    info.dependencies = {"Dep1", "Dep2"};
    info.optionalDeps = {"OptDep1"};
    info.conflicts = {"ConflictComp"};
    info.provides = {"Service1", "Service2"};

    registry.addInitializer("DependentComp", [](Component&) {}, []() {}, info);

    const auto& retrievedInfo = registry.getComponentInfo("DependentComp");
    EXPECT_EQ(retrievedInfo.dependencies.size(), 2);
    EXPECT_EQ(retrievedInfo.optionalDeps.size(), 1);
    EXPECT_EQ(retrievedInfo.conflicts.size(), 1);
    EXPECT_EQ(retrievedInfo.provides.size(), 2);
}

TEST(RegistryTest, ComponentInfoAutoLoad) {
    auto& registry = Registry::instance();

    Registry::ComponentInfo info;
    info.name = "AutoLoadComp";
    info.isAutoLoad = true;
    info.isLazyLoad = false;

    registry.addInitializer("AutoLoadComp", [](Component&) {}, []() {}, info);

    const auto& retrievedInfo = registry.getComponentInfo("AutoLoadComp");
    EXPECT_TRUE(retrievedInfo.isAutoLoad);
    EXPECT_FALSE(retrievedInfo.isLazyLoad);
}

// ============================================================================
// Extended Registry Tests - Memory Pool Integration
// ============================================================================

TEST(RegistryTest, CreateComponent) {
    auto& registry = Registry::instance();

    auto component = registry.createComponent<Component>("CreatedComp");
    ASSERT_NE(component, nullptr);
    EXPECT_EQ(component->getName(), "CreatedComp");
}

TEST(RegistryTest, CreateComponentWithArgs) {
    auto& registry = Registry::instance();

    // Component constructor takes name as argument
    auto component = registry.createComponent<Component>("CreatedWithArgs");
    ASSERT_NE(component, nullptr);
    EXPECT_EQ(component->getName(), "CreatedWithArgs");
}

/* TODO: Uncomment when methods are implemented
TEST(RegistryTest, GetTotalPoolMemoryUsage) {
    auto& registry = Registry::instance();

    // Create some components to allocate memory
    auto comp1 = registry.createComponent<Component>("PoolTest1");
    auto comp2 = registry.createComponent<Component>("PoolTest2");

    size_t memoryUsage = registry.getTotalPoolMemoryUsage();
    EXPECT_GT(memoryUsage, 0);
}

TEST(RegistryTest, CleanupComponentPools) {
    auto& registry = Registry::instance();

    // Create and destroy some components
    {
        auto comp = registry.createComponent<Component>("PoolCleanupTest");
    }

    // Cleanup should not throw
    EXPECT_NO_THROW(registry.cleanupComponentPools());
}
*/

// ============================================================================
// Extended Registry Tests - Lifecycle Integration
// ============================================================================

/* TODO: Uncomment when methods are implemented
TEST(RegistryTest, GetDependencyOrder) {
    auto& registry = Registry::instance();
    registry.cleanupAll();

    registry.addInitializer("OrderA", [](Component&) {}, []() {});
    registry.addInitializer("OrderB", [](Component&) {}, []() {});
    registry.addInitializer("OrderC", [](Component&) {}, []() {});

    registry.addDependency("OrderA", "OrderB");
    registry.addDependency("OrderB", "OrderC");

    auto order = registry.getDependencyOrder("OrderA");

    // Order should include all dependencies
    EXPECT_GE(order.size(), 3);

    // OrderC should come before OrderB, OrderB before OrderA
    auto posC = std::find(order.begin(), order.end(), "OrderC");
    auto posB = std::find(order.begin(), order.end(), "OrderB");
    auto posA = std::find(order.begin(), order.end(), "OrderA");

    EXPECT_TRUE(posC < posB);
    EXPECT_TRUE(posB < posA);
}

TEST(RegistryTest, HasCircularDependencies) {
    auto& registry = Registry::instance();
    registry.cleanupAll();

    registry.addInitializer("CircA", [](Component&) {}, []() {});
    registry.addInitializer("CircB", [](Component&) {}, []() {});

    // No circular dependency initially
    EXPECT_FALSE(registry.hasCircularDependencies("CircA"));

    registry.addDependency("CircA", "CircB");

    // Still no circular dependency
    EXPECT_FALSE(registry.hasCircularDependencies("CircA"));

    // Adding reverse dependency creates circular dependency
    // This should be caught by addDependency
    EXPECT_THROW(registry.addDependency("CircB", "CircA"),
atom::error::RuntimeError);
}

TEST(RegistryTest, GetLifecycleEvents) {
    auto& registry = Registry::instance();
    registry.cleanupAll();

    registry.addInitializer("EventTest", [](Component&) {}, []() {});
    registry.initializeAll();

    // Get lifecycle events for specific component
    auto events = registry.getLifecycleEvents("EventTest");

    // Should have at least initialization event
    EXPECT_GE(events.size(), 0);
}

TEST(RegistryTest, GetAllLifecycleEvents) {
    auto& registry = Registry::instance();

    // Get all lifecycle events (empty string = all components)
    auto allEvents = registry.getLifecycleEvents("");

    // Should have events from all components
    EXPECT_GE(allEvents.size(), 0);
}
*/
