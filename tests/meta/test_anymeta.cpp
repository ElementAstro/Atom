#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "atom/meta/anymeta.hpp"

#include <string>
#include <vector>
#include <memory>

using namespace atom::meta;

// Test fixture for TypeMetadata tests
class TypeMetadataTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test class for metadata testing
        metadata = std::make_shared<TypeMetadata>();
    }

    std::shared_ptr<TypeMetadata> metadata;

    // Helper test class
    class TestClass {
    public:
        int value;
        std::string name;
        
        TestClass(int v = 0, std::string n = "") : value(v), name(std::move(n)) {}
        
        int getValue() const { return value; }
        void setValue(int v) { value = v; }
        
        std::string getName() const { return name; }
        void setName(const std::string& n) { name = n; }
        
        int add(int a, int b) { return a + b; }
        std::string concatenate(const std::string& a, const std::string& b) {
            return a + b;
        }
    };
};

// Test basic metadata creation and properties
TEST_F(TypeMetadataTest, BasicMetadataCreation) {
    // TypeMetadata should be created successfully
    EXPECT_NE(metadata, nullptr);

    // Initially should have no methods for a test method name
    EXPECT_EQ(metadata->getMethods("testMethod"), nullptr);
}

// Test method registration and invocation
TEST_F(TypeMetadataTest, MethodRegistration) {
    // Register a simple method
    metadata->addMethod("add", [](std::vector<BoxedValue> args) -> BoxedValue {
        if (args.size() != 2) {
            throw std::invalid_argument("add requires 2 arguments");
        }
        auto a_opt = args[0].tryCast<int>();
        auto b_opt = args[1].tryCast<int>();
        if (!a_opt || !b_opt) {
            throw std::invalid_argument("Arguments must be integers");
        }
        return BoxedValue(*a_opt + *b_opt);
    });

    // Test method existence
    auto methods = metadata->getMethods("add");
    EXPECT_NE(methods, nullptr);
    EXPECT_FALSE(methods->empty());

    // Test that non-existent method returns nullptr
    EXPECT_EQ(metadata->getMethods("nonexistent"), nullptr);

    // Test method invocation through the method vector
    std::vector<BoxedValue> args = {BoxedValue(5), BoxedValue(3)};
    BoxedValue result = (*methods)[0](args);
    auto result_opt = result.tryCast<int>();
    EXPECT_TRUE(result_opt.has_value());
    EXPECT_EQ(*result_opt, 8);

    // Test method with wrong arguments
    std::vector<BoxedValue> wrongArgs = {BoxedValue(5)};
    EXPECT_THROW((*methods)[0](wrongArgs), std::invalid_argument);
}

// Test property registration and access
TEST_F(TypeMetadataTest, PropertyRegistration) {
    // Register a simple property with getter and setter
    metadata->addProperty("testProp",
        [](const BoxedValue& obj) -> BoxedValue {
            // Simple getter that returns a fixed value
            return BoxedValue(42);
        },
        [](BoxedValue& obj, const BoxedValue& value) {
            // Simple setter that does nothing for this test
        }
    );

    // Test that we can add properties without errors
    EXPECT_NO_THROW(metadata->addProperty("anotherProp",
        [](const BoxedValue& obj) -> BoxedValue { return BoxedValue(100); },
        [](BoxedValue& obj, const BoxedValue& value) {}
    ));
}

// Test constructor registration
TEST_F(TypeMetadataTest, ConstructorRegistration) {
    // Register default constructor
    metadata->addConstructor("TestClass", [](std::vector<BoxedValue> args) -> BoxedValue {
        if (args.empty()) {
            return BoxedValue(42);  // Return a simple int for testing
        }
        throw std::invalid_argument("Default constructor takes no arguments");
    });

    // Test that constructor was added without errors
    EXPECT_NO_THROW(metadata->addConstructor("TestClass", [](std::vector<BoxedValue> args) -> BoxedValue {
        return BoxedValue(100);
    }));
}

// Test event system
TEST_F(TypeMetadataTest, EventSystem) {
    bool eventTriggered = false;

    // Register event handler
    metadata->addEventListener("test_event",
        [&eventTriggered](BoxedValue& obj, const std::vector<BoxedValue>& args) {
            eventTriggered = true;
        }
    );

    // Test that event was added without errors
    EXPECT_NO_THROW(metadata->addEvent("another_event", "Test event"));
}

// Test method overloading
TEST_F(TypeMetadataTest, MethodOverloading) {
    // Register overloaded methods with different signatures
    metadata->addMethod("process", [](std::vector<BoxedValue> args) -> BoxedValue {
        if (args.size() == 1 && args[0].isType<int>()) {
            auto val_opt = args[0].tryCast<int>();
            if (val_opt) {
                return BoxedValue(*val_opt * 2);
            }
        }
        throw std::invalid_argument("process(int) signature not matched");
    });

    metadata->addMethod("process", [](std::vector<BoxedValue> args) -> BoxedValue {
        if (args.size() == 1 && args[0].isType<std::string>()) {
            auto val_opt = args[0].tryCast<std::string>();
            if (val_opt) {
                return BoxedValue(*val_opt + "_processed");
            }
        }
        throw std::invalid_argument("process(string) signature not matched");
    });

    // Test that methods were registered
    auto methods = metadata->getMethods("process");
    EXPECT_NE(methods, nullptr);
    EXPECT_EQ(methods->size(), 2);  // Two overloads
}

// Test basic functionality
TEST_F(TypeMetadataTest, BasicFunctionality) {
    // Test that we can create and use TypeMetadata
    EXPECT_NE(metadata, nullptr);

    // Test adding multiple methods
    metadata->addMethod("method1", [](std::vector<BoxedValue> args) -> BoxedValue {
        return BoxedValue(1);
    });

    metadata->addMethod("method2", [](std::vector<BoxedValue> args) -> BoxedValue {
        return BoxedValue(2);
    });

    // Test that methods were added
    EXPECT_NE(metadata->getMethods("method1"), nullptr);
    EXPECT_NE(metadata->getMethods("method2"), nullptr);
    EXPECT_EQ(metadata->getMethods("nonexistent"), nullptr);

    // Test adding properties
    EXPECT_NO_THROW(metadata->addProperty("prop1",
        [](const BoxedValue& obj) -> BoxedValue { return BoxedValue(100); },
        [](BoxedValue& obj, const BoxedValue& value) {}
    ));

    // Test adding constructors
    EXPECT_NO_THROW(metadata->addConstructor("TestType",
        [](std::vector<BoxedValue> args) -> BoxedValue { return BoxedValue(42); }
    ));

    // Test adding events
    EXPECT_NO_THROW(metadata->addEvent("testEvent", "Test event description"));
}
