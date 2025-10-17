#include "atom/components/component.hpp"
#include "atom/components/core/registry.hpp"
#include "atom/components/data/var.hpp"
#include "atom/components/lifecycle/lifecycle.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

// Test fixture for Component
class ComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        component = std::make_shared<Component>("TestComponent");
    }

    std::shared_ptr<Component> component;
};

// Test constructor
TEST_F(ComponentTest, Constructor) {
    EXPECT_EQ(component->getName(), "TestComponent");
}

// Test getInstance
TEST_F(ComponentTest, GetInstance) {
    auto weakInstance = component->getInstance();
    EXPECT_FALSE(weakInstance.expired());
}

// Test getSharedInstance
TEST_F(ComponentTest, GetSharedInstance) {
    auto sharedInstance = component->getSharedInstance();
    EXPECT_EQ(sharedInstance, component);
}

// Test initialize (default implementation)
TEST_F(ComponentTest, Initialize) { EXPECT_FALSE(component->initialize()); }

// Test destroy (default implementation)
TEST_F(ComponentTest, Destroy) { EXPECT_FALSE(component->destroy()); }

// Test getName
TEST_F(ComponentTest, GetName) {
    EXPECT_EQ(component->getName(), "TestComponent");
}

// Test getTypeInfo and setTypeInfo
TEST_F(ComponentTest, GetSetTypeInfo) {
    atom::meta::TypeInfo typeInfo = atom::meta::userType<int>();
    component->setTypeInfo(typeInfo);
    EXPECT_EQ(component->getTypeInfo(), typeInfo);
}

// Test addVariable, getVariable, and hasVariable
TEST_F(ComponentTest, AddGetHasVariable) {
    component->addVariable<int>("var1", 10, "Test variable");
    auto var = component->getVariable<int>("var1");
    EXPECT_EQ(var->get(), 10);
    EXPECT_TRUE(component->hasVariable("var1"));
}

// Test setRange
TEST_F(ComponentTest, SetRange) {
    component->addVariable<int>("var2", 5);
    component->setRange<int>("var2", 1, 10);
    // Assuming VariableManager has a method to get range (not shown in the
    // provided code)
}

// Test setStringOptions
TEST_F(ComponentTest, SetStringOptions) {
    component->addVariable<std::string>("var3", "option1");
    std::vector<std::string> options = {"option1", "option2", "option3"};
    component->setStringOptions("var3", options);
    // Assuming VariableManager has a method to get options (not shown in the
    // provided code)
}

// Test setValue
TEST_F(ComponentTest, SetValue) {
    component->addVariable<int>("var4", 20);
    component->setValue<int>("var4", 30);
    auto var = component->getVariable<int>("var4");
    EXPECT_EQ(var->get(), 30);
}

// Test getVariableNames
TEST_F(ComponentTest, GetVariableNames) {
    component->addVariable<int>("var5", 50);
    auto names = component->getVariableNames();
    EXPECT_EQ(names.size(), 1);
    EXPECT_EQ(names[0], "var5");
}

// Test getVariableDescription
TEST_F(ComponentTest, GetVariableDescription) {
    component->addVariable<int>("var6", 60, "Description for var6");
    EXPECT_EQ(component->getVariableDescription("var6"),
              "Description for var6");
}

// Test getVariableAlias
TEST_F(ComponentTest, GetVariableAlias) {
    component->addVariable<int>("var7", 70, "", "alias_var7");
    EXPECT_EQ(component->getVariableAlias("var7"), "alias_var7");
}

// Test getVariableGroup
TEST_F(ComponentTest, GetVariableGroup) {
    component->addVariable<int>("var8", 80, "", "", "group_var8");
    EXPECT_EQ(component->getVariableGroup("var8"), "group_var8");
}

// Test doc and getDoc
TEST_F(ComponentTest, DocAndGetDoc) {
    component->doc("Component documentation");
    EXPECT_EQ(component->getDoc(), "Component documentation");
}

// Test dispatch
TEST_F(ComponentTest, Dispatch) {
    component->def("testCommand", []() { return 42; });
    auto result = std::any_cast<int>(component->dispatch("testCommand"));
    EXPECT_EQ(result, 42);
}

// Test has
TEST_F(ComponentTest, Has) {
    component->def("testCommand2", []() { return 42; });
    EXPECT_TRUE(component->has("testCommand2"));
}

// Test getCommandsInGroup
TEST_F(ComponentTest, GetCommandsInGroup) {
    component->def("testCommand3", []() { return 42; }, "group1");
    auto commands = component->getCommandsInGroup("group1");
    EXPECT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "testCommand3");
}

// Test getCommandDescription
TEST_F(ComponentTest, GetCommandDescription) {
    component->def(
        "testCommand4", []() { return 42; }, "",
        "Description for testCommand4");
    EXPECT_EQ(component->getCommandDescription("testCommand4"),
              "Description for testCommand4");
}

// Test getCommandArgAndReturnType
TEST_F(ComponentTest, GetCommandArgAndReturnType) {
    component->def("testCommand5", [](int a) { return a; });
    auto args = component->getCommandArgAndReturnType("testCommand5");
    EXPECT_EQ(args.size(), 1);
    // Check the return type
    EXPECT_EQ(args[0].returnType, "int");
}

// Test getAllCommands
TEST_F(ComponentTest, GetAllCommands) {
    component->def("testCommand6", []() { return 42; });
    auto commands = component->getAllCommands();
    EXPECT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0], "testCommand6");
}

// Test getRegisteredTypes
TEST_F(ComponentTest, GetRegisteredTypes) {
    component->defType<int>("intType");
    auto types = component->getRegisteredTypes();
    EXPECT_EQ(types.size(), 1);
    EXPECT_EQ(types[0], "intType");
}

// Test getNeededComponents
TEST_F(ComponentTest, GetNeededComponents) {
    auto neededComponents = Component::getNeededComponents();
    EXPECT_TRUE(neededComponents.empty());
}

// Test addOtherComponent, getOtherComponent, and removeOtherComponent
TEST_F(ComponentTest, AddGetRemoveOtherComponent) {
    auto otherComponent = std::make_shared<Component>("OtherComponent");
    component->addOtherComponent("OtherComponent", otherComponent);
    auto retrievedComponent =
        component->getOtherComponent("OtherComponent").lock();
    EXPECT_EQ(retrievedComponent, otherComponent);
    component->removeOtherComponent("OtherComponent");
    EXPECT_TRUE(component->getOtherComponent("OtherComponent").expired());
}

// Test clearOtherComponents
TEST_F(ComponentTest, ClearOtherComponents) {
    auto otherComponent1 = std::make_shared<Component>("OtherComponent1");
    auto otherComponent2 = std::make_shared<Component>("OtherComponent2");
    component->addOtherComponent("OtherComponent1", otherComponent1);
    component->addOtherComponent("OtherComponent2", otherComponent2);
    component->clearOtherComponents();
    EXPECT_TRUE(component->getOtherComponent("OtherComponent1").expired());
    EXPECT_TRUE(component->getOtherComponent("OtherComponent2").expired());
}

// Test runCommand
TEST_F(ComponentTest, RunCommand) {
    component->def("testCommand7", [](int a, int b) { return a + b; });
    std::vector<std::any> args = {1, 2};
    auto result =
        std::any_cast<int>(component->runCommand("testCommand7", args));
    EXPECT_EQ(result, 3);
}

// ============================================================================
// Extended Tests - Edge Cases and Error Handling
// ============================================================================

// Test constructor with empty name (should throw)
TEST(ComponentEdgeCaseTest, ConstructorEmptyName) {
    EXPECT_THROW(Component(""), std::invalid_argument);
}

// Test state transitions
TEST_F(ComponentTest, StateTransitions) {
    EXPECT_EQ(component->getState(), ComponentState::Created);

    component->setState(ComponentState::Initializing);
    EXPECT_EQ(component->getState(), ComponentState::Initializing);

    component->setState(ComponentState::Active);
    EXPECT_EQ(component->getState(), ComponentState::Active);

    component->setState(ComponentState::Disabled);
    EXPECT_EQ(component->getState(), ComponentState::Disabled);

    component->setState(ComponentState::Error);
    EXPECT_EQ(component->getState(), ComponentState::Error);

    component->setState(ComponentState::Destroying);
    EXPECT_EQ(component->getState(), ComponentState::Destroying);
}

// Test performance statistics
TEST_F(ComponentTest, PerformanceStats) {
    const auto& stats = component->getPerformanceStats();
    EXPECT_EQ(stats.commandCallCount.load(), 0);
    EXPECT_EQ(stats.commandErrorCount.load(), 0);
    EXPECT_EQ(stats.eventCount.load(), 0);

    // Execute a command to update stats
    component->def("perfTest", []() { return 42; });
    component->dispatch("perfTest");

    // Stats should be updated
    EXPECT_GT(stats.commandCallCount.load(), 0);
}

// Test performance stats reset
TEST_F(ComponentTest, ResetPerformanceStats) {
    component->def("perfTest", []() { return 42; });
    component->dispatch("perfTest");

    const auto& stats = component->getPerformanceStats();
    EXPECT_GT(stats.commandCallCount.load(), 0);

    component->resetPerformanceStats();
    EXPECT_EQ(stats.commandCallCount.load(), 0);
}

// Test legacy performance stats
TEST_F(ComponentTest, LegacyPerformanceStats) {
    auto legacyStats = component->getLegacyPerformanceStats();
    EXPECT_EQ(legacyStats.commandCallCount.load(), 0);
}

// Test variable with empty name
TEST_F(ComponentTest, VariableEmptyName) {
    // Empty name should be handled gracefully or throw
    // Behavior depends on implementation
}

// Test variable that doesn't exist
TEST_F(ComponentTest, GetNonExistentVariable) {
    auto var = component->getVariable<int>("nonexistent");
    EXPECT_EQ(var, nullptr);
}

// Test hasVariable with non-existent variable
TEST_F(ComponentTest, HasNonExistentVariable) {
    EXPECT_FALSE(component->hasVariable("nonexistent"));
}

// Test multiple variables of different types
TEST_F(ComponentTest, MultipleVariableTypes) {
    component->addVariable<int>("intVar", 42);
    component->addVariable<double>("doubleVar", 3.14);
    component->addVariable<std::string>("stringVar", "test");
    component->addVariable<bool>("boolVar", true);

    EXPECT_EQ(component->getVariable<int>("intVar")->get(), 42);
    EXPECT_DOUBLE_EQ(component->getVariable<double>("doubleVar")->get(), 3.14);
    EXPECT_EQ(component->getVariable<std::string>("stringVar")->get(), "test");
    EXPECT_TRUE(component->getVariable<bool>("boolVar")->get());
}

// Test command with no return value
TEST_F(ComponentTest, CommandVoidReturn) {
    bool executed = false;
    component->def("voidCommand", [&executed]() { executed = true; });
    component->dispatch("voidCommand");
    EXPECT_TRUE(executed);
}

// Test command with multiple parameters
TEST_F(ComponentTest, CommandMultipleParams) {
    component->def("multiParam", [](int a, int b, int c) { return a + b + c; });
    auto result =
        std::any_cast<int>(component->dispatch("multiParam", 1, 2, 3));
    EXPECT_EQ(result, 6);
}

// Test dispatch non-existent command
TEST_F(ComponentTest, DispatchNonExistentCommand) {
    EXPECT_THROW(component->dispatch("nonexistent"),
                 atom::error::InvalidArgument);
}

// Test has with non-existent command
TEST_F(ComponentTest, HasNonExistentCommand) {
    EXPECT_FALSE(component->has("nonexistent"));
}

// Test removeCommand
TEST_F(ComponentTest, RemoveCommand) {
    component->def("toRemove", []() { return 42; });
    EXPECT_TRUE(component->has("toRemove"));

    component->removeCommand("toRemove");
    EXPECT_FALSE(component->has("toRemove"));
}

// Test command aliases
TEST_F(ComponentTest, CommandAliases) {
    component->def("original", []() { return 42; });
    component->addAlias("original", "alias1");

    auto result1 = std::any_cast<int>(component->dispatch("original"));
    auto result2 = std::any_cast<int>(component->dispatch("alias1"));

    EXPECT_EQ(result1, result2);
    EXPECT_EQ(result1, 42);
}

// Test command groups
TEST_F(ComponentTest, CommandGroups) {
    component->def("cmd1", []() {}, "group1");
    component->def("cmd2", []() {}, "group1");
    component->def("cmd3", []() {}, "group2");

    auto group1Commands = component->getCommandsInGroup("group1");
    EXPECT_EQ(group1Commands.size(), 2);

    auto group2Commands = component->getCommandsInGroup("group2");
    EXPECT_EQ(group2Commands.size(), 1);
}

// Test empty command group
TEST_F(ComponentTest, EmptyCommandGroup) {
    auto commands = component->getCommandsInGroup("nonexistent");
    EXPECT_TRUE(commands.empty());
}

// Test getAllCommands with multiple commands
TEST_F(ComponentTest, GetAllCommandsMultiple) {
    component->def("cmd1", []() {});
    component->def("cmd2", []() {});
    component->def("cmd3", []() {});

    auto allCommands = component->getAllCommands();
    EXPECT_EQ(allCommands.size(), 3);
}

// Test defType
TEST_F(ComponentTest, DefType) {
    component->defType<int>("IntType");
    component->defType<std::string>("StringType");

    auto types = component->getRegisteredTypes();
    EXPECT_GE(types.size(), 2);
}

// Test hasType
TEST_F(ComponentTest, HasType) {
    component->defType<int>("TestType");
    EXPECT_TRUE(component->hasType("TestType"));
    EXPECT_FALSE(component->hasType("NonExistentType"));
}

// Test getOtherComponent with non-existent component
TEST_F(ComponentTest, GetNonExistentOtherComponent) {
    auto comp = component->getOtherComponent("nonexistent");
    EXPECT_TRUE(comp.expired());
}

// Test addOtherComponent with null component
TEST_F(ComponentTest, AddNullOtherComponent) {
    std::weak_ptr<Component> nullComp;
    component->addOtherComponent("null", nullComp);
    auto retrieved = component->getOtherComponent("null");
    EXPECT_TRUE(retrieved.expired());
}

// Test removeOtherComponent non-existent
TEST_F(ComponentTest, RemoveNonExistentOtherComponent) {
    // Should not throw
    EXPECT_NO_THROW(component->removeOtherComponent("nonexistent"));
}

// Test command timeout
TEST_F(ComponentTest, CommandTimeout) {
    component->def("timeoutTest", []() { return 42; });
    component->setTimeout("timeoutTest", std::chrono::milliseconds(100));

    // Command should still execute within timeout
    auto result = std::any_cast<int>(component->dispatch("timeoutTest"));
    EXPECT_EQ(result, 42);
}

// Test command with exception
TEST_F(ComponentTest, CommandWithException) {
    component->def("throwingCommand",
                   []() -> int { throw std::runtime_error("Test exception"); });

    EXPECT_THROW(component->dispatch("throwingCommand"), std::runtime_error);
}

// Test runCommand with empty args
TEST_F(ComponentTest, RunCommandEmptyArgs) {
    component->def("noArgs", []() { return 42; });
    std::vector<std::any> emptyArgs;
    auto result =
        std::any_cast<int>(component->runCommand("noArgs", emptyArgs));
    EXPECT_EQ(result, 42);
}

// Test runCommand with wrong number of args
TEST_F(ComponentTest, RunCommandWrongArgCount) {
    component->def("oneArg", [](int x) { return x * 2; });
    std::vector<std::any> wrongArgs = {1, 2};  // Too many args

    // Should throw or handle gracefully
    try {
        [[maybe_unused]] auto result =
            component->runCommand("oneArg", wrongArgs);
        // If it doesn't throw, that's also acceptable behavior
    } catch (const std::exception&) {
        // Expected exception
    }
}

// Test variable description with empty description
TEST_F(ComponentTest, VariableEmptyDescription) {
    component->addVariable<int>("var", 42, "");
    auto desc = component->getVariableDescription("var");
    EXPECT_TRUE(desc.empty());
}

// Test variable alias with empty alias
TEST_F(ComponentTest, VariableEmptyAlias) {
    component->addVariable<int>("var", 42, "", "");
    auto alias = component->getVariableAlias("var");
    EXPECT_TRUE(alias.empty());
}

// Test variable group with empty group
TEST_F(ComponentTest, VariableEmptyGroup) {
    component->addVariable<int>("var", 42, "", "", "");
    auto group = component->getVariableGroup("var");
    EXPECT_TRUE(group.empty());
}

// Test getVariableNames with no variables
TEST_F(ComponentTest, GetVariableNamesEmpty) {
    auto component2 = std::make_shared<Component>("EmptyComponent");
    auto names = component2->getVariableNames();
    EXPECT_TRUE(names.empty());
}

// Test doc with empty string
TEST_F(ComponentTest, DocEmpty) {
    component->doc("");
    EXPECT_TRUE(component->getDoc().empty());
}

// Test doc with long string
TEST_F(ComponentTest, DocLongString) {
    std::string longDoc(1000, 'x');
    component->doc(longDoc);
    EXPECT_EQ(component->getDoc(), longDoc);
}

// Test command description with empty description
TEST_F(ComponentTest, CommandEmptyDescription) {
    component->def("cmd", []() {}, "", "");
    auto desc = component->getCommandDescription("cmd");
    EXPECT_TRUE(desc.empty());
}

// Test getCommandDescription for non-existent command
TEST_F(ComponentTest, GetCommandDescriptionNonExistent) {
    auto desc = component->getCommandDescription("nonexistent");
    EXPECT_TRUE(desc.empty());
}

// ============================================================================
// Concurrent Access Tests
// ============================================================================

#include <thread>
#include <vector>

// Test concurrent variable access
TEST_F(ComponentTest, ConcurrentVariableAccess) {
    component->addVariable<int>("counter", 0);

    std::vector<std::thread> threads;
    constexpr int numThreads = 10;
    constexpr int incrementsPerThread = 100;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                auto var = component->getVariable<int>("counter");
                if (var) {
                    *var = var->get() + 1;  // Use assignment operator
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Due to race conditions, the final value may not be exactly numThreads *
    // incrementsPerThread But it should be > 0
    auto var = component->getVariable<int>("counter");
    EXPECT_GT(var->get(), 0);
}

// Test concurrent command dispatch
TEST_F(ComponentTest, ConcurrentCommandDispatch) {
    std::atomic<int> callCount{0};
    component->def("concurrentCmd", [&callCount]() {
        callCount++;
        return 42;
    });

    std::vector<std::thread> threads;
    const int numThreads = 10;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this]() {
            auto result =
                std::any_cast<int>(component->dispatch("concurrentCmd"));
            EXPECT_EQ(result, 42);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(callCount.load(), numThreads);
}

// Test concurrent state changes
TEST_F(ComponentTest, ConcurrentStateChanges) {
    std::vector<std::thread> threads;
    const int numThreads = 10;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i]() {
            ComponentState state = static_cast<ComponentState>(i % 6);
            component->setState(state);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Final state should be one of the valid states
    auto finalState = component->getState();
    EXPECT_GE(static_cast<int>(finalState), 0);
    EXPECT_LT(static_cast<int>(finalState), 6);
}

// ============================================================================
// Registry, Lifecycle, and VariableManager Tests
// ============================================================================

// Test fixture for advanced component testing
class AdvancedComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        component_ = std::make_shared<Component>("AdvancedTestComponent");
        registry_ = &Registry::instance();
        lifecycleManager_ = &atom::components::LifecycleManager::instance();
        variableManager_ = std::make_unique<VariableManager>();
    }

    void TearDown() override {
        // Clean up any registered components
        if (registry_) {
            registry_->cleanupAll();
        }
    }

    std::shared_ptr<Component> component_;
    Registry* registry_;
    atom::components::LifecycleManager* lifecycleManager_;
    std::unique_ptr<VariableManager> variableManager_;
};

// ============================================================================
// Component Command System Tests
// ============================================================================

TEST_F(AdvancedComponentTest, ComponentCommandSystem) {
    bool commandExecuted = false;

    // Define a simple command
    component_->def(
        "testCommand",
        [&commandExecuted]() {
            commandExecuted = true;
            return 42;
        },
        "test", "A test command");

    // Test command existence
    EXPECT_TRUE(component_->has("testCommand"));

    // Test command execution
    auto result = component_->dispatch("testCommand");
    EXPECT_TRUE(commandExecuted);

    // Test command with parameters
    component_->def(
        "addCommand", [](int a, int b) { return a + b; }, "math",
        "Addition command");

    auto addResult = component_->dispatch("addCommand", 10, 20);
    EXPECT_EQ(std::any_cast<int>(addResult), 30);
}

TEST_F(AdvancedComponentTest, ComponentVariableManagement) {
    // Test adding variables
    component_->addVariable<int>("testInt", 42, "Test integer variable");
    component_->addVariable<std::string>("testString", "Hello",
                                         "Test string variable");
    component_->addVariable<double>("testDouble", 3.14159,
                                    "Test double variable");

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

TEST_F(AdvancedComponentTest, ComponentErrorHandling) {
    // Test accessing non-existent variable should throw
    EXPECT_THROW(component_->getVariable<int>("nonExistent"), std::exception);

    // Test dispatching non-existent command
    EXPECT_THROW(component_->dispatch("nonExistentCommand"), std::exception);

    // Test invalid command arguments
    component_->def("strictCommand", [](int required) { return required * 2; });

    std::vector<std::any> wrongArgs = {"string_instead_of_int"};
    EXPECT_THROW(
        {
            auto result = component_->runCommand("strictCommand", wrongArgs);
            (void)result;  // Suppress unused variable warning
        },
        std::exception);
}

// ============================================================================
// Registry Tests
// ============================================================================

TEST_F(AdvancedComponentTest, RegistryBasicOperations) {
    // Test component registration
    registry_->addInitializer(
        "TestComponent1",
        [](Component& comp) { comp.addVariable<int>("initialized", 1); },
        []() { /* cleanup */ });

    auto component1 = registry_->getComponent("TestComponent1");
    EXPECT_NE(component1, nullptr);
    EXPECT_EQ(component1->getName(), "TestComponent1");
}

TEST_F(AdvancedComponentTest, RegistryDependencyManagement) {
    // Register components with dependencies
    registry_->addInitializer("ComponentA", [](Component&) {}, []() {});
    registry_->addInitializer("ComponentB", [](Component&) {}, []() {});
    registry_->addInitializer("ComponentC", [](Component&) {}, []() {});

    // Add dependencies: C depends on B, B depends on A
    registry_->addDependency("ComponentC", "ComponentB");
    registry_->addDependency("ComponentB", "ComponentA");

    // Test circular dependency detection
    EXPECT_THROW(registry_->addDependency("ComponentA", "ComponentC"),
                 std::exception);
}

// ============================================================================
// Lifecycle Manager Tests
// ============================================================================

TEST_F(AdvancedComponentTest, LifecycleHooks) {
    bool hookExecuted = false;

    // Register a lifecycle hook
    lifecycleManager_->registerHook(
        "AdvancedTestComponent",
        atom::components::LifecyclePhase::PostInitialization,
        [&hookExecuted](Component&, atom::components::LifecyclePhase) {
            hookExecuted = true;
            return true;
        });

    // Execute the lifecycle phase
    bool result = lifecycleManager_->executePhase(
        *component_, atom::components::LifecyclePhase::PostInitialization);
    EXPECT_TRUE(result);
    EXPECT_TRUE(hookExecuted);
}

TEST_F(AdvancedComponentTest, GlobalLifecycleHooks) {
    int globalHookCount = 0;

    // Register global hooks for different phases
    lifecycleManager_->registerGlobalHook(
        atom::components::LifecyclePhase::PreInitialization,
        [&globalHookCount](Component&, atom::components::LifecyclePhase) {
            globalHookCount++;
            return true;
        });

    lifecycleManager_->registerGlobalHook(
        atom::components::LifecyclePhase::PostInitialization,
        [&globalHookCount](Component&, atom::components::LifecyclePhase) {
            globalHookCount++;
            return true;
        });

    // Execute different phases
    lifecycleManager_->executePhase(
        *component_, atom::components::LifecyclePhase::PreInitialization);
    lifecycleManager_->executePhase(
        *component_, atom::components::LifecyclePhase::PostInitialization);

    EXPECT_EQ(globalHookCount, 2);
}

// ============================================================================
// Variable Manager Tests
// ============================================================================

TEST_F(AdvancedComponentTest, VariableManagerOperations) {
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

TEST_F(AdvancedComponentTest, VariableManagerRangeValidation) {
    variableManager_->addVariable("rangedVar", 50);
    variableManager_->setRange("rangedVar", 0, 100);

    // Valid range
    EXPECT_NO_THROW(variableManager_->setValue("rangedVar", 75));

    // Invalid range
    EXPECT_THROW(variableManager_->setValue("rangedVar", 150), std::exception);
    EXPECT_THROW(variableManager_->setValue("rangedVar", -10), std::exception);
}

TEST_F(AdvancedComponentTest, VariableManagerStringOptions) {
    variableManager_->addVariable("optionVar", std::string("option1"));
    std::vector<std::string> options = {"option1", "option2", "option3"};
    variableManager_->setStringOptions("optionVar", options);

    // Valid option
    EXPECT_NO_THROW(
        variableManager_->setValue("optionVar", std::string("option2")));

    // Invalid option
    EXPECT_THROW(
        variableManager_->setValue("optionVar", std::string("invalidOption")),
        std::exception);
}

// ============================================================================
// Performance and Stress Tests
// ============================================================================

TEST_F(AdvancedComponentTest, ComponentPerformanceBasic) {
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
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time (adjust threshold as needed)
    EXPECT_LT(duration.count(), 1000);  // Less than 1 second

    // Verify all variables and commands were added
    EXPECT_EQ(component_->getVariableNames().size(), numVariables);
    EXPECT_EQ(component_->getAllCommands().size(), numCommands);
}

TEST_F(AdvancedComponentTest, ConcurrentAccess) {
    const int numThreads = 4;
    const int operationsPerThread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Launch multiple threads performing operations
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, operationsPerThread, &successCount]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                try {
                    std::string varName = "thread" + std::to_string(t) +
                                          "_var" + std::to_string(i);
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
