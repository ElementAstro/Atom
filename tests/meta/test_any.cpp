#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "atom/meta/any.hpp"

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>

using namespace atom::meta;
using ::testing::HasSubstr;

// Test fixture for BoxedValue tests
class BoxedValueTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test values
        intValue = 42;
        doubleValue = 3.14159;
        stringValue = "Hello, BoxedValue!";
        boolValue = true;
    }

    // Test values
    int intValue;
    double doubleValue;
    std::string stringValue;
    bool boolValue;

    // Helper struct for testing
    struct TestStruct {
        int id;
        std::string name;
        
        TestStruct(int i, std::string n) : id(i), name(std::move(n)) {}
        
        bool operator==(const TestStruct& other) const {
            return id == other.id && name == other.name;
        }
    };
};

// Test basic construction and type checking
TEST_F(BoxedValueTest, BasicConstruction) {
    // Test default constructor (creates undefined value)
    BoxedValue voidVal;
    EXPECT_TRUE(voidVal.isUndef());
    EXPECT_TRUE(voidVal.isNull());

    // Test construction with various types
    BoxedValue intVal(intValue);
    EXPECT_TRUE(intVal.isType<int>());
    EXPECT_FALSE(intVal.isVoid());
    EXPECT_FALSE(intVal.isUndef());

    BoxedValue doubleVal(doubleValue);
    EXPECT_TRUE(doubleVal.isType<double>());

    BoxedValue stringVal(stringValue);
    EXPECT_TRUE(stringVal.isType<std::string>());

    BoxedValue boolVal(boolValue);
    EXPECT_TRUE(boolVal.isType<bool>());

    // Test with custom struct
    TestStruct testStruct(1, "test");
    BoxedValue structVal(testStruct);
    EXPECT_TRUE(structVal.isType<TestStruct>());
}

// Test value retrieval and casting
TEST_F(BoxedValueTest, ValueRetrievalAndCasting) {
    BoxedValue intVal(intValue);
    BoxedValue stringVal(stringValue);
    BoxedValue structVal(TestStruct(2, "struct_test"));

    // Test successful casting
    auto int_opt = intVal.tryCast<int>();
    EXPECT_TRUE(int_opt.has_value());
    EXPECT_EQ(*int_opt, intValue);

    auto string_opt = stringVal.tryCast<std::string>();
    EXPECT_TRUE(string_opt.has_value());
    EXPECT_EQ(*string_opt, stringValue);

    auto struct_opt = structVal.tryCast<TestStruct>();
    EXPECT_TRUE(struct_opt.has_value());
    EXPECT_EQ(struct_opt->id, 2);
    EXPECT_EQ(struct_opt->name, "struct_test");

    // Test failed casting (should return nullopt)
    auto failed_cast1 = intVal.tryCast<std::string>();
    EXPECT_FALSE(failed_cast1.has_value());

    auto failed_cast2 = stringVal.tryCast<int>();
    EXPECT_FALSE(failed_cast2.has_value());
}

// Test copy and move semantics
TEST_F(BoxedValueTest, CopyAndMoveSemantics) {
    BoxedValue original(stringValue);

    // Test copy constructor
    BoxedValue copied(original);
    EXPECT_TRUE(copied.isType<std::string>());
    auto copied_opt = copied.tryCast<std::string>();
    EXPECT_TRUE(copied_opt.has_value());
    EXPECT_EQ(*copied_opt, stringValue);

    auto original_opt = original.tryCast<std::string>();
    EXPECT_TRUE(original_opt.has_value());
    EXPECT_EQ(*original_opt, stringValue); // Original unchanged

    // Test copy assignment
    BoxedValue assigned;
    assigned = original;
    EXPECT_TRUE(assigned.isType<std::string>());
    auto assigned_opt = assigned.tryCast<std::string>();
    EXPECT_TRUE(assigned_opt.has_value());
    EXPECT_EQ(*assigned_opt, stringValue);

    // Test move constructor
    BoxedValue moved(std::move(copied));
    EXPECT_TRUE(moved.isType<std::string>());
    auto moved_opt = moved.tryCast<std::string>();
    EXPECT_TRUE(moved_opt.has_value());
    EXPECT_EQ(*moved_opt, stringValue);

    // Test move assignment
    BoxedValue moveAssigned;
    moveAssigned = std::move(assigned);
    EXPECT_TRUE(moveAssigned.isType<std::string>());
    auto move_assigned_opt = moveAssigned.tryCast<std::string>();
    EXPECT_TRUE(move_assigned_opt.has_value());
    EXPECT_EQ(*move_assigned_opt, stringValue);
}

// Test basic functionality
TEST_F(BoxedValueTest, BasicFunctionality) {
    // Test that BoxedValue can hold different types
    BoxedValue intVal(42);
    BoxedValue stringVal(std::string("test"));
    BoxedValue doubleVal(3.14);

    // Test type checking
    EXPECT_TRUE(intVal.isType<int>());
    EXPECT_FALSE(intVal.isType<std::string>());

    EXPECT_TRUE(stringVal.isType<std::string>());
    EXPECT_FALSE(stringVal.isType<int>());

    EXPECT_TRUE(doubleVal.isType<double>());
    EXPECT_FALSE(doubleVal.isType<int>());
}

// Test attribute system
TEST_F(BoxedValueTest, AttributeSystem) {
    BoxedValue val(intValue);

    // Test setting and getting attributes
    val.setAttr("description", BoxedValue(std::string("Test integer")));
    val.setAttr("category", BoxedValue(std::string("number")));
    val.setAttr("readonly", BoxedValue(false));

    EXPECT_TRUE(val.hasAttr("description"));
    EXPECT_TRUE(val.hasAttr("category"));
    EXPECT_TRUE(val.hasAttr("readonly"));
    EXPECT_FALSE(val.hasAttr("nonexistent"));

    // Test attribute retrieval
    auto desc = val.getAttr("description");
    EXPECT_TRUE(desc.isType<std::string>());
    auto desc_opt = desc.tryCast<std::string>();
    EXPECT_TRUE(desc_opt.has_value());
    EXPECT_EQ(*desc_opt, "Test integer");

    auto readonly = val.getAttr("readonly");
    EXPECT_TRUE(readonly.isType<bool>());
    auto readonly_opt = readonly.tryCast<bool>();
    EXPECT_TRUE(readonly_opt.has_value());
    EXPECT_FALSE(*readonly_opt);

    // Test attribute removal
    val.removeAttr("category");
    EXPECT_FALSE(val.hasAttr("category"));
    EXPECT_TRUE(val.hasAttr("description")); // Others should remain

    // Test getting nonexistent attribute
    auto nonexistent = val.getAttr("nonexistent");
    EXPECT_TRUE(nonexistent.isUndef());
}

// Test type information
TEST_F(BoxedValueTest, TypeInformation) {
    BoxedValue intVal(intValue);
    BoxedValue stringVal(stringValue);
    BoxedValue structVal(TestStruct(3, "type_test"));

    // Test type info access
    const TypeInfo& intTypeInfo = intVal.getTypeInfo();
    EXPECT_EQ(intTypeInfo.name(), "int");
    EXPECT_FALSE(intTypeInfo.isPointer());
    EXPECT_FALSE(intTypeInfo.isReference());

    const TypeInfo& stringTypeInfo = stringVal.getTypeInfo();
    EXPECT_THAT(stringTypeInfo.name(), HasSubstr("string"));

    const TypeInfo& structTypeInfo = structVal.getTypeInfo();
    EXPECT_THAT(structTypeInfo.name(), HasSubstr("TestStruct"));
}

// Test core functionality
TEST_F(BoxedValueTest, CoreFunctionality) {
    BoxedValue intVal(intValue);
    BoxedValue stringVal(stringValue);
    BoxedValue boolVal(boolValue);

    // Test that values can be stored and retrieved
    EXPECT_TRUE(intVal.isType<int>());
    EXPECT_TRUE(stringVal.isType<std::string>());
    EXPECT_TRUE(boolVal.isType<bool>());

    // Test tryCast functionality
    auto int_opt = intVal.tryCast<int>();
    EXPECT_TRUE(int_opt.has_value());
    EXPECT_EQ(*int_opt, intValue);

    auto string_opt = stringVal.tryCast<std::string>();
    EXPECT_TRUE(string_opt.has_value());
    EXPECT_EQ(*string_opt, stringValue);

    auto bool_opt = boolVal.tryCast<bool>();
    EXPECT_TRUE(bool_opt.has_value());
    EXPECT_EQ(*bool_opt, boolValue);
}

// Test readonly and const behavior
TEST_F(BoxedValueTest, ReadonlyAndConstBehavior) {
    BoxedValue val(intValue);

    // Test readonly status (read-only, no setters available)
    EXPECT_FALSE(val.isReadonly());  // Should be false by default
}

// Test return value flag
TEST_F(BoxedValueTest, ReturnValueFlag) {
    BoxedValue val(intValue);

    // Test return value flag (read-only, no setters available)
    EXPECT_FALSE(val.isReturnValue());  // Should be false by default
}
