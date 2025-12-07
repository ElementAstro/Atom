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
    EXPECT_TRUE(std::find(allVars.begin(), allVars.end(), "var1") !=
                allVars.end());
    EXPECT_TRUE(std::find(allVars.begin(), allVars.end(), "var2") !=
                allVars.end());
    EXPECT_TRUE(std::find(allVars.begin(), allVars.end(), "var3") !=
                allVars.end());
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
    vm.forEachVariable([&count](const std::string&, const auto&) { count++; });

    EXPECT_EQ(count, 3);
}

// ============================================================================
// Extended VariableManager Tests - Edge Cases
// ============================================================================

TEST(VariableManagerTest, DuplicateVariableName) {
    VariableManager vm;

    vm.addVariable("duplicate", 42);

    // Adding duplicate should throw
    EXPECT_THROW(vm.addVariable("duplicate", 100),
                 atom::error::ObjectAlreadyExist);
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

    EXPECT_THROW(vm.getVariable<int>("nonexistent"),
                 atom::error::ObjectNotExist);
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

// =============================================================================
// Additional VariableManager Tests
// =============================================================================

TEST(VariableManagerTest, BoolVariable) {
    VariableManager vm;

    vm.addVariable("boolVar", true, "A boolean variable");

    auto boolVar = vm.getVariable<bool>("boolVar");
    ASSERT_NE(boolVar, nullptr);
    EXPECT_TRUE(boolVar->get());

    vm.setValue("boolVar", false);
    EXPECT_FALSE(boolVar->get());
}

TEST(VariableManagerTest, CharVariable) {
    VariableManager vm;

    vm.addVariable("charVar", 'A', "A character variable");

    auto charVar = vm.getVariable<char>("charVar");
    ASSERT_NE(charVar, nullptr);
    EXPECT_EQ(charVar->get(), 'A');

    vm.setValue("charVar", 'Z');
    EXPECT_EQ(charVar->get(), 'Z');
}

TEST(VariableManagerTest, LongVariable) {
    VariableManager vm;

    vm.addVariable("longVar", 1234567890L, "A long variable");

    auto longVar = vm.getVariable<long>("longVar");
    ASSERT_NE(longVar, nullptr);
    EXPECT_EQ(longVar->get(), 1234567890L);
}

TEST(VariableManagerTest, NegativeRange) {
    VariableManager vm;

    vm.addVariable("negVar", 0);
    vm.setRange("negVar", -100, 100);

    vm.setValue("negVar", -50);
    EXPECT_EQ(vm.getVariable<int>("negVar")->get(), -50);

    vm.setValue("negVar", 50);
    EXPECT_EQ(vm.getVariable<int>("negVar")->get(), 50);

    EXPECT_THROW(vm.setValue("negVar", -150), atom::error::OutOfRange);
    EXPECT_THROW(vm.setValue("negVar", 150), atom::error::OutOfRange);
}

TEST(VariableManagerTest, RangeBoundaryValues) {
    VariableManager vm;

    vm.addVariable("boundaryVar", 50);
    vm.setRange("boundaryVar", 0, 100);

    // Test exact boundary values
    vm.setValue("boundaryVar", 0);
    EXPECT_EQ(vm.getVariable<int>("boundaryVar")->get(), 0);

    vm.setValue("boundaryVar", 100);
    EXPECT_EQ(vm.getVariable<int>("boundaryVar")->get(), 100);
}

TEST(VariableManagerTest, MultipleAliases) {
    VariableManager vm;

    vm.addVariable("primary", 42, "Primary variable", "alias1");

    // Access via alias
    auto varByAlias = vm.getVariable<int>("alias1");
    ASSERT_NE(varByAlias, nullptr);
    EXPECT_EQ(varByAlias->get(), 42);

    // Set via alias
    vm.setValue("alias1", 100);
    EXPECT_EQ(vm.getVariable<int>("primary")->get(), 100);
}

TEST(VariableManagerTest, EmptyDescription) {
    VariableManager vm;

    vm.addVariable("noDesc", 42);

    std::string desc = vm.getDescription("noDesc");
    EXPECT_TRUE(desc.empty());
}

TEST(VariableManagerTest, EmptyAlias) {
    VariableManager vm;

    vm.addVariable("noAlias", 42, "Description", "");

    std::string alias = vm.getAlias("noAlias");
    EXPECT_TRUE(alias.empty());
}

TEST(VariableManagerTest, EmptyGroup) {
    VariableManager vm;

    vm.addVariable("noGroup", 42, "Description", "alias", "");

    std::string group = vm.getGroup("noGroup");
    EXPECT_TRUE(group.empty());
}

TEST(VariableManagerTest, VectorVariable) {
    VariableManager vm;

    std::vector<int> vec = {1, 2, 3, 4, 5};
    vm.addVariable("vectorVar", vec, "A vector variable");

    auto vectorVar = vm.getVariable<std::vector<int>>("vectorVar");
    ASSERT_NE(vectorVar, nullptr);
    EXPECT_EQ(vectorVar->get().size(), 5);
    EXPECT_EQ(vectorVar->get()[0], 1);
    EXPECT_EQ(vectorVar->get()[4], 5);
}

TEST(VariableManagerTest, MapVariable) {
    VariableManager vm;

    std::map<std::string, int> map = {{"one", 1}, {"two", 2}, {"three", 3}};
    vm.addVariable("mapVar", map, "A map variable");

    auto mapVar = vm.getVariable<std::map<std::string, int>>("mapVar");
    ASSERT_NE(mapVar, nullptr);
    EXPECT_EQ(mapVar->get().size(), 3);
    EXPECT_EQ(mapVar->get().at("one"), 1);
}

TEST(VariableManagerTest, ClassMemberWithRange) {
    struct TestClass {
        int value;
    };

    TestClass obj{50};
    VariableManager vm;

    vm.addVariable("member", &TestClass::value, obj, "Member with range");
    vm.setRange("member", 0, 100);

    vm.setValue("member", 75);
    EXPECT_EQ(obj.value, 75);

    EXPECT_THROW(vm.setValue("member", 150), atom::error::OutOfRange);
}

TEST(VariableManagerTest, ClearAllVariables) {
    VariableManager vm;

    vm.addVariable("var1", 1);
    vm.addVariable("var2", 2);
    vm.addVariable("var3", 3);

    EXPECT_EQ(vm.getAllVariables().size(), 3);

    vm.clear();

    EXPECT_EQ(vm.getAllVariables().size(), 0);
    EXPECT_FALSE(vm.has("var1"));
    EXPECT_FALSE(vm.has("var2"));
    EXPECT_FALSE(vm.has("var3"));
}

TEST(VariableManagerTest, VariableCount) {
    VariableManager vm;

    EXPECT_EQ(vm.size(), 0);

    vm.addVariable("var1", 1);
    EXPECT_EQ(vm.size(), 1);

    vm.addVariable("var2", 2);
    EXPECT_EQ(vm.size(), 2);

    vm.removeVariable("var1");
    EXPECT_EQ(vm.size(), 1);
}

TEST(VariableManagerTest, StringOptionsWithSpecialCharacters) {
    VariableManager vm;

    vm.addVariable("specialVar", std::string("option-1"));
    std::vector<std::string> options = {"option-1", "option_2", "option.3",
                                        "option 4"};
    vm.setStringOptions("specialVar", options);

    vm.setValue("specialVar", std::string("option_2"));
    EXPECT_EQ(vm.getVariable<std::string>("specialVar")->get(), "option_2");

    vm.setValue("specialVar", std::string("option.3"));
    EXPECT_EQ(vm.getVariable<std::string>("specialVar")->get(), "option.3");

    vm.setValue("specialVar", std::string("option 4"));
    EXPECT_EQ(vm.getVariable<std::string>("specialVar")->get(), "option 4");
}

TEST(VariableManagerTest, ZeroRange) {
    VariableManager vm;

    vm.addVariable("zeroRange", 0);
    vm.setRange("zeroRange", 0, 0);

    vm.setValue("zeroRange", 0);
    EXPECT_EQ(vm.getVariable<int>("zeroRange")->get(), 0);

    EXPECT_THROW(vm.setValue("zeroRange", 1), atom::error::OutOfRange);
    EXPECT_THROW(vm.setValue("zeroRange", -1), atom::error::OutOfRange);
}

TEST(VariableManagerTest, UnsignedIntVariable) {
    VariableManager vm;

    vm.addVariable("unsignedVar", 42u, "An unsigned int variable");

    auto unsignedVar = vm.getVariable<unsigned int>("unsignedVar");
    ASSERT_NE(unsignedVar, nullptr);
    EXPECT_EQ(unsignedVar->get(), 42u);

    vm.setValue("unsignedVar", 100u);
    EXPECT_EQ(unsignedVar->get(), 100u);
}
