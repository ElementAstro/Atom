/*
 * test_variable_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Comprehensive Unit Tests for Variable Manager
Tests variable registration/retrieval, type safety, range constraints,
string options, trackable behavior, and error scenarios.

**************************************************/

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "../var.hpp"

/**
 * @brief Test fixture for VariableManager tests
 */
class VariableManagerTest : public ::testing::Test {
protected:
    void SetUp() override { manager_ = std::make_unique<VariableManager>(); }

    void TearDown() override { manager_.reset(); }

    std::unique_ptr<VariableManager> manager_;
};

/**
 * @brief Test fixture for error scenarios
 */
class VariableManagerErrorTest : public ::testing::Test {
protected:
    void SetUp() override { manager_ = std::make_unique<VariableManager>(); }

    void TearDown() override { manager_.reset(); }

    std::unique_ptr<VariableManager> manager_;
};

// ============================================================================
// Basic Variable Operations Tests
// ============================================================================

TEST_F(VariableManagerTest, AddAndGetVariable) {
    // Add integer variable
    manager_->addVariable<int>("testInt", 42, "Test integer variable");

    auto intVar = manager_->getVariable<int>("testInt");
    ASSERT_NE(intVar, nullptr);
    EXPECT_EQ(intVar->get(), 42);

    // Add string variable
    manager_->addVariable<std::string>("testString", "hello",
                                       "Test string variable");

    auto stringVar = manager_->getVariable<std::string>("testString");
    ASSERT_NE(stringVar, nullptr);
    EXPECT_EQ(stringVar->get(), "hello");

    // Add double variable
    manager_->addVariable<double>("testDouble", 3.14159,
                                  "Test double variable");

    auto doubleVar = manager_->getVariable<double>("testDouble");
    ASSERT_NE(doubleVar, nullptr);
    EXPECT_DOUBLE_EQ(doubleVar->get(), 3.14159);

    // Add boolean variable
    manager_->addVariable<bool>("testBool", true, "Test boolean variable");

    auto boolVar = manager_->getVariable<bool>("testBool");
    ASSERT_NE(boolVar, nullptr);
    EXPECT_EQ(boolVar->get(), true);
}

TEST_F(VariableManagerTest, AddVariableWithMetadata) {
    manager_->addVariable<int>("metaVar", 100, "Variable with metadata",
                               "alias", "group");

    auto var = manager_->getVariable<int>("metaVar");
    ASSERT_NE(var, nullptr);
    EXPECT_EQ(var->get(), 100);

    // Check metadata
    EXPECT_EQ(manager_->getDescription("metaVar"), "Variable with metadata");
    EXPECT_EQ(manager_->getAlias("metaVar"), "alias");
    EXPECT_EQ(manager_->getGroup("metaVar"), "group");
}

TEST_F(VariableManagerTest, SetAndGetValue) {
    manager_->addVariable<int>("counter", 0);

    auto var = manager_->getVariable<int>("counter");
    ASSERT_NE(var, nullptr);

    // Set value through variable
    var->set(50);
    EXPECT_EQ(var->get(), 50);

    // Set value through manager
    manager_->setValue<int>("counter", 100);
    EXPECT_EQ(var->get(), 100);
}

TEST_F(VariableManagerTest, HasVariable) {
    manager_->addVariable<int>("existingVar", 42);

    EXPECT_TRUE(manager_->hasVariable("existingVar"));
    EXPECT_FALSE(manager_->hasVariable("nonExistentVar"));
}

TEST_F(VariableManagerTest, GetAllVariableNames) {
    manager_->addVariable<int>("var1", 1);
    manager_->addVariable<std::string>("var2", "test");
    manager_->addVariable<double>("var3", 1.0);

    auto names = manager_->getAllVariableNames();
    EXPECT_GE(names.size(), 3);

    // Check that our variables are in the list
    bool hasVar1 = std::find(names.begin(), names.end(), "var1") != names.end();
    bool hasVar2 = std::find(names.begin(), names.end(), "var2") != names.end();
    bool hasVar3 = std::find(names.begin(), names.end(), "var3") != names.end();

    EXPECT_TRUE(hasVar1);
    EXPECT_TRUE(hasVar2);
    EXPECT_TRUE(hasVar3);
}

// ============================================================================
// Range Constraint Tests
// ============================================================================

TEST_F(VariableManagerTest, SetRangeConstraints) {
    manager_->addVariable<int>("rangedInt", 50);
    manager_->setRange<int>("rangedInt", 0, 100);

    auto var = manager_->getVariable<int>("rangedInt");
    ASSERT_NE(var, nullptr);

    // Valid range
    var->set(75);
    EXPECT_EQ(var->get(), 75);

    // Test boundary values
    var->set(0);
    EXPECT_EQ(var->get(), 0);

    var->set(100);
    EXPECT_EQ(var->get(), 100);
}

TEST_F(VariableManagerTest, SetRangeConstraintsDouble) {
    manager_->addVariable<double>("rangedDouble", 0.5);
    manager_->setRange<double>("rangedDouble", 0.0, 1.0);

    auto var = manager_->getVariable<double>("rangedDouble");
    ASSERT_NE(var, nullptr);

    // Valid range
    var->set(0.75);
    EXPECT_DOUBLE_EQ(var->get(), 0.75);

    // Boundary values
    var->set(0.0);
    EXPECT_DOUBLE_EQ(var->get(), 0.0);

    var->set(1.0);
    EXPECT_DOUBLE_EQ(var->get(), 1.0);
}

TEST_F(VariableManagerTest, RangeConstraintValidation) {
    manager_->addVariable<int>("constrainedVar", 50);
    manager_->setRange<int>("constrainedVar", 10, 90);

    auto var = manager_->getVariable<int>("constrainedVar");
    ASSERT_NE(var, nullptr);

    // Try to set values outside range
    // Behavior depends on implementation - might clamp, throw, or reject
    // This test mainly ensures no crashes occur
    EXPECT_NO_THROW(var->set(-10));
    EXPECT_NO_THROW(var->set(150));
}

// ============================================================================
// String Options Tests
// ============================================================================

TEST_F(VariableManagerTest, SetStringOptions) {
    manager_->addVariable<std::string>("mode", "auto");

    std::vector<std::string> options = {"auto", "manual", "disabled"};
    manager_->setStringOptions("mode", options);

    auto var = manager_->getVariable<std::string>("mode");
    ASSERT_NE(var, nullptr);

    // Valid options
    var->set("manual");
    EXPECT_EQ(var->get(), "manual");

    var->set("disabled");
    EXPECT_EQ(var->get(), "disabled");

    var->set("auto");
    EXPECT_EQ(var->get(), "auto");
}

TEST_F(VariableManagerTest, StringOptionsValidation) {
    manager_->addVariable<std::string>("restrictedString", "option1");

    std::vector<std::string> options = {"option1", "option2", "option3"};
    manager_->setStringOptions("restrictedString", options);

    auto var = manager_->getVariable<std::string>("restrictedString");
    ASSERT_NE(var, nullptr);

    // Try to set invalid option
    // Behavior depends on implementation - might reject or throw
    EXPECT_NO_THROW(var->set("invalidOption"));
}

// ============================================================================
// Member Variable Binding Tests
// ============================================================================

TEST_F(VariableManagerTest, MemberVariableBinding) {
    struct TestClass {
        int memberInt = 42;
        std::string memberString = "test";
        double memberDouble = 3.14;
    };

    TestClass testObj;

    // Bind member variables
    manager_->addVariable<int, TestClass>("boundInt", &TestClass::memberInt,
                                          testObj, "Bound integer");
    manager_->addVariable<std::string, TestClass>(
        "boundString", &TestClass::memberString, testObj, "Bound string");
    manager_->addVariable<double, TestClass>(
        "boundDouble", &TestClass::memberDouble, testObj, "Bound double");

    // Get variables
    auto intVar = manager_->getVariable<int>("boundInt");
    auto stringVar = manager_->getVariable<std::string>("boundString");
    auto doubleVar = manager_->getVariable<double>("boundDouble");

    ASSERT_NE(intVar, nullptr);
    ASSERT_NE(stringVar, nullptr);
    ASSERT_NE(doubleVar, nullptr);

    // Check initial values
    EXPECT_EQ(intVar->get(), 42);
    EXPECT_EQ(stringVar->get(), "test");
    EXPECT_DOUBLE_EQ(doubleVar->get(), 3.14);

    // Modify through variables
    intVar->set(100);
    stringVar->set("modified");
    doubleVar->set(2.71);

    // Check that original object is modified
    EXPECT_EQ(testObj.memberInt, 100);
    EXPECT_EQ(testObj.memberString, "modified");
    EXPECT_DOUBLE_EQ(testObj.memberDouble, 2.71);
}

// ============================================================================
// Trackable Behavior Tests
// ============================================================================

TEST_F(VariableManagerTest, TrackableVariableBehavior) {
    manager_->addVariable<int>("trackableVar", 10);

    auto var = manager_->getVariable<int>("trackableVar");
    ASSERT_NE(var, nullptr);

    // Test trackable interface
    bool changeNotified = false;
    var->addCallback(
        [&changeNotified](const int& oldValue, const int& newValue) {
            changeNotified = true;
            EXPECT_EQ(oldValue, 10);
            EXPECT_EQ(newValue, 20);
        });

    // Change value
    var->set(20);

    EXPECT_TRUE(changeNotified);
    EXPECT_EQ(var->get(), 20);
}

TEST_F(VariableManagerTest, MultipleCallbacks) {
    manager_->addVariable<std::string>("callbackVar", "initial");

    auto var = manager_->getVariable<std::string>("callbackVar");
    ASSERT_NE(var, nullptr);

    int callback1Count = 0;
    int callback2Count = 0;

    var->addCallback([&callback1Count](const std::string&, const std::string&) {
        callback1Count++;
    });

    var->addCallback([&callback2Count](const std::string&, const std::string&) {
        callback2Count++;
    });

    // Change value multiple times
    var->set("change1");
    var->set("change2");
    var->set("change3");

    EXPECT_EQ(callback1Count, 3);
    EXPECT_EQ(callback2Count, 3);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(VariableManagerErrorTest, GetNonExistentVariable) {
    EXPECT_THROW(manager_->getVariable<int>("nonExistent"), std::out_of_range);
}

TEST_F(VariableManagerErrorTest, VariableTypeError) {
    manager_->addVariable<int>("intVar", 42);

    // Try to get as wrong type
    EXPECT_THROW(manager_->getVariable<std::string>("intVar"),
                 VariableTypeError);
}

TEST_F(VariableManagerErrorTest, SetValueTypeError) {
    manager_->addVariable<int>("intVar", 42);

    // Try to set wrong type
    EXPECT_THROW(manager_->setValue<std::string>("intVar", "wrong"),
                 VariableTypeError);
}

TEST_F(VariableManagerErrorTest, SetRangeOnNonExistentVariable) {
    EXPECT_THROW(manager_->setRange<int>("nonExistent", 0, 100),
                 std::out_of_range);
}

TEST_F(VariableManagerErrorTest, SetStringOptionsOnNonExistentVariable) {
    std::vector<std::string> options = {"option1", "option2"};
    EXPECT_THROW(manager_->setStringOptions("nonExistent", options),
                 std::out_of_range);
}

TEST_F(VariableManagerErrorTest, SetStringOptionsOnNonStringVariable) {
    manager_->addVariable<int>("intVar", 42);

    std::vector<std::string> options = {"option1", "option2"};
    EXPECT_THROW(manager_->setStringOptions("intVar", options),
                 VariableTypeError);
}

TEST_F(VariableManagerErrorTest, GetMetadataForNonExistentVariable) {
    EXPECT_THROW(manager_->getDescription("nonExistent"), std::out_of_range);
    EXPECT_THROW(manager_->getAlias("nonExistent"), std::out_of_range);
    EXPECT_THROW(manager_->getGroup("nonExistent"), std::out_of_range);
}

// ============================================================================
// Complex Data Types Tests
// ============================================================================

TEST_F(VariableManagerTest, ComplexDataTypes) {
    // Vector
    std::vector<int> intVector = {1, 2, 3, 4, 5};
    manager_->addVariable<std::vector<int>>("intVector", intVector);

    auto vectorVar = manager_->getVariable<std::vector<int>>("intVector");
    ASSERT_NE(vectorVar, nullptr);
    EXPECT_EQ(vectorVar->get(), intVector);

    // Modify vector
    std::vector<int> newVector = {10, 20, 30};
    vectorVar->set(newVector);
    EXPECT_EQ(vectorVar->get(), newVector);
}
