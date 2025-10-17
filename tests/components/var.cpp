#include "atom/components/data/var.hpp"

#include <gtest/gtest.h>

#include "atom/error/exception.hpp"

// VariableManager is not in a namespace

struct TestClass {
    int intValue;
    std::string stringValue;
};

TEST(VariableManagerTest, AddAndGetVariable) {
    VariableManager vm;

    vm.addVariable("intVar", 42, "An integer variable", "intVarAlias",
                   "group1");
    vm.addVariable("stringVar", std::string("Hello"), "A string variable");

    auto intVar = vm.getVariable<int>("intVar");
    ASSERT_NE(intVar, nullptr);
    EXPECT_EQ(intVar->get(), 42);

    auto stringVar = vm.getVariable<std::string>("stringVar");
    ASSERT_NE(stringVar, nullptr);
    EXPECT_EQ(stringVar->get(), "Hello");

    EXPECT_TRUE(vm.has("intVar"));
    EXPECT_FALSE(vm.has("nonExistentVar"));
}

TEST(VariableManagerTest, SetAndGetValue) {
    VariableManager vm;

    vm.addVariable("intVar", 42);
    vm.setValue("intVar", 84);
    auto intVar = vm.getVariable<int>("intVar");
    ASSERT_NE(intVar, nullptr);
    EXPECT_EQ(intVar->get(), 84);

    vm.addVariable("stringVar", std::string("Hello"));
    vm.setValue("stringVar", std::string("World"));
    auto stringVar = vm.getVariable<std::string>("stringVar");
    ASSERT_NE(stringVar, nullptr);
    EXPECT_EQ(stringVar->get(), "World");
}

TEST(VariableManagerTest, SetRangeAndValueOutOfRange) {
    VariableManager vm;

    vm.addVariable("intVar", 42);
    vm.setRange("intVar", 0, 100);

    vm.setValue("intVar", 50);
    auto intVar = vm.getVariable<int>("intVar");
    EXPECT_EQ(intVar->get(), 50);

    EXPECT_THROW(vm.setValue("intVar", 150), atom::error::OutOfRange);
    EXPECT_EQ(intVar->get(), 50);
}

TEST(VariableManagerTest, SetStringOptions) {
    VariableManager vm;

    vm.addVariable("stringVar", std::string("Option1"));
    std::vector<std::string> options = {"Option1", "Option2", "Option3"};
    vm.setStringOptions("stringVar", options);

    vm.setValue("stringVar", std::string("Option2"));
    auto stringVar = vm.getVariable<std::string>("stringVar");
    ASSERT_NE(stringVar, nullptr);
    EXPECT_EQ(stringVar->get(), "Option2");

    EXPECT_THROW(vm.setValue("stringVar", std::string("InvalidOption")),
                 atom::error::InvalidArgument);
    EXPECT_EQ(stringVar->get(), "Option2");
}

TEST(VariableManagerTest, ClassMemberVariable) {
    struct TestClass {
        int intValue;
        std::string stringValue;
    };

    TestClass obj{42, "Hello"};
    VariableManager vm;

    vm.addVariable("intMember", &TestClass::intValue, obj,
                   "Integer member variable");
    vm.addVariable("stringMember", &TestClass::stringValue, obj,
                   "String member variable");

    auto intMember = vm.getVariable<int>("intMember");
    ASSERT_NE(intMember, nullptr);
    EXPECT_EQ(intMember->get(), 42);

    auto stringMember = vm.getVariable<std::string>("stringMember");
    ASSERT_NE(stringMember, nullptr);
    EXPECT_EQ(stringMember->get(), "Hello");

    vm.setValue("intMember", 84);
    vm.setValue("stringMember", std::string("World"));

    EXPECT_EQ(obj.intValue, 84);
    EXPECT_EQ(obj.stringValue, "World");
}

TEST(VariableManagerTest, GetDescriptionAliasGroup) {
    VariableManager vm;

    vm.addVariable("var1", 42, "Description for var1", "alias1", "group1");
    vm.addVariable("var2", std::string("Hello"), "Description for var2",
                   "alias2", "group2");

    EXPECT_EQ(vm.getDescription("var1"), "Description for var1");
    EXPECT_EQ(vm.getDescription("alias1"), "Description for var1");

    EXPECT_EQ(vm.getAlias("var1"), "alias1");
    EXPECT_EQ(vm.getAlias("alias1"), "var1");

    EXPECT_EQ(vm.getGroup("var1"), "group1");
    EXPECT_EQ(vm.getGroup("alias1"), "group1");

    EXPECT_EQ(vm.getDescription("var2"), "Description for var2");
    EXPECT_EQ(vm.getDescription("alias2"), "Description for var2");

    EXPECT_EQ(vm.getAlias("var2"), "alias2");
    EXPECT_EQ(vm.getAlias("alias2"), "var2");

    EXPECT_EQ(vm.getGroup("var2"), "group2");
    EXPECT_EQ(vm.getGroup("alias2"), "group2");
}

// ============================================================================
// Extended VariableManager Tests - Advanced Operations
// ============================================================================

TEST(VariableManagerTest, RemoveVariable) {
    VariableManager vm;

    vm.addVariable("toRemove", 42);
    EXPECT_TRUE(vm.has("toRemove"));

    vm.removeVariable("toRemove");
    EXPECT_FALSE(vm.has("toRemove"));
}

TEST(VariableManagerTest, RemoveVariableByAlias) {
    VariableManager vm;

    vm.addVariable("primary", 42, "", "alias");
    EXPECT_TRUE(vm.has("primary"));
    EXPECT_TRUE(vm.has("alias"));

    // Remove by alias should remove both
    vm.removeVariable("alias");
    EXPECT_FALSE(vm.has("primary"));
    EXPECT_FALSE(vm.has("alias"));
}

TEST(VariableManagerTest, GetAllVariables) {
    VariableManager vm;

    vm.addVariable("var1", 1);
    vm.addVariable("var2", 2);
    vm.addVariable("var3", 3);

    auto allVars = vm.getAllVariables();
    EXPECT_EQ(allVars.size(), 3);

    // Check that all variables are present
    EXPECT_TRUE(std::find(allVars.begin(), allVars.end(), "var1") != allVars.end());
    EXPECT_TRUE(std::find(allVars.begin(), allVars.end(), "var2") != allVars.end());
    EXPECT_TRUE(std::find(allVars.begin(), allVars.end(), "var3") != allVars.end());
}

TEST(VariableManagerTest, GetVariablesByGroup) {
    VariableManager vm;

    vm.addVariable("var1", 1, "", "", "group1");
    vm.addVariable("var2", 2, "", "", "group1");
    vm.addVariable("var3", 3, "", "", "group2");

    auto group1Vars = vm.getVariablesByGroup("group1");
    EXPECT_EQ(group1Vars.size(), 2);

    auto group2Vars = vm.getVariablesByGroup("group2");
    EXPECT_EQ(group2Vars.size(), 1);

    auto emptyGroup = vm.getVariablesByGroup("nonexistent");
    EXPECT_TRUE(emptyGroup.empty());
}

TEST(VariableManagerTest, ForEachVariable) {
    VariableManager vm;

    vm.addVariable("var1", 1);
    vm.addVariable("var2", 2);
    vm.addVariable("var3", 3);

    int count = 0;
    vm.forEachVariable([&count](const std::string&, const auto&) {
        count++;
    });

    EXPECT_EQ(count, 3);
}

// ============================================================================
// Extended VariableManager Tests - Edge Cases
// ============================================================================

TEST(VariableManagerTest, DuplicateVariableName) {
    VariableManager vm;

    vm.addVariable("duplicate", 42);

    // Adding duplicate should throw
    EXPECT_THROW(vm.addVariable("duplicate", 100), atom::error::ObjectAlreadyExist);
}

TEST(VariableManagerTest, RangeValidationDifferentTypes) {
    VariableManager vm;

    // Test with double
    vm.addVariable("doubleVar", 5.0);
    vm.setRange("doubleVar", 0.0, 10.0);

    vm.setValue("doubleVar", 7.5);
    EXPECT_DOUBLE_EQ(vm.getVariable<double>("doubleVar")->get(), 7.5);

    EXPECT_THROW(vm.setValue("doubleVar", 15.0), atom::error::OutOfRange);

    // Test with float
    vm.addVariable("floatVar", 5.0f);
    vm.setRange("floatVar", 0.0f, 10.0f);

    vm.setValue("floatVar", 7.5f);
    EXPECT_FLOAT_EQ(vm.getVariable<float>("floatVar")->get(), 7.5f);
}

TEST(VariableManagerTest, StringOptionsEmptyList) {
    VariableManager vm;

    vm.addVariable("stringVar", std::string("value"));

    // Empty options list
    std::vector<std::string> emptyOptions;
    vm.setStringOptions("stringVar", emptyOptions);

    // Any value should be invalid now
    EXPECT_THROW(vm.setValue("stringVar", std::string("anything")),
                 atom::error::InvalidArgument);
}

TEST(VariableManagerTest, StringOptionsCaseSensitivity) {
    VariableManager vm;

    vm.addVariable("stringVar", std::string("Option1"));
    std::vector<std::string> options = {"Option1", "Option2"};
    vm.setStringOptions("stringVar", options);

    // Exact match should work
    vm.setValue("stringVar", std::string("Option1"));
    EXPECT_EQ(vm.getVariable<std::string>("stringVar")->get(), "Option1");

    // Case mismatch should fail
    EXPECT_THROW(vm.setValue("stringVar", std::string("option1")),
                 atom::error::InvalidArgument);
}

TEST(VariableManagerTest, TypeMismatchInSetValue) {
    VariableManager vm;

    vm.addVariable("intVar", 42);

    // Trying to set with wrong type should throw
    EXPECT_THROW(vm.getVariable<std::string>("intVar"), VariableTypeError);
}

TEST(VariableManagerTest, GetNonExistentVariable) {
    VariableManager vm;

    EXPECT_THROW(vm.getVariable<int>("nonexistent"), atom::error::ObjectNotExist);
}

TEST(VariableManagerTest, SetValueForNonExistentVariable) {
    VariableManager vm;

    EXPECT_THROW(vm.setValue("nonexistent", 42), atom::error::ObjectNotExist);
}

TEST(VariableManagerTest, CStringSetValue) {
    VariableManager vm;

    vm.addVariable("stringVar", std::string("initial"));

    // Test C-string overload
    vm.setValue("stringVar", "updated");

    auto var = vm.getVariable<std::string>("stringVar");
    EXPECT_EQ(var->get(), "updated");
}
