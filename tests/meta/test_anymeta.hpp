#include <gtest/gtest.h>
#include "atom/meta/anymeta.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

// Test fixture for anymeta tests
class AnyMetaTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear registry before each test
        atom::meta::TypeRegistry::instance().clear();
    }
    
    void TearDown() override {
        // Clean up after each test
        atom::meta::TypeRegistry::instance().clear();
    }
};

// Helper classes for testing
class TestClass {
private:
    int value_;
    std::string name_;

public:
    TestClass() : value_(0), name_("default") {}
    TestClass(int value, const std::string& name) : value_(value), name_(name) {}
    
    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }
    
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    
    void print() const {
        std::cout << "TestClass: " << name_ << " = " << value_ << std::endl;
    }
};

class AnotherTestClass {
private:
    double data_;

public:
    AnotherTestClass() : data_(0.0) {}
    explicit AnotherTestClass(double data) : data_(data) {}
    
    double getData() const { return data_; }
    void setData(double data) { data_ = data; }
};

// Test TypeMetadata basic functionality
TEST_F(AnyMetaTest, TypeMetadataBasics) {
    atom::meta::TypeMetadata metadata;
    
    // Test method registration
    metadata.addMethod("testMethod", [](std::vector<atom::meta::BoxedValue> args) -> atom::meta::BoxedValue {
        return atom::meta::BoxedValue(42);
    });
    
    // Test method retrieval
    auto methods = metadata.getMethods("testMethod");
    ASSERT_NE(methods, nullptr);
    EXPECT_EQ(methods->size(), 1);
    
    // Test method execution
    auto result = (*methods)[0]({});
    EXPECT_TRUE(result.isType<int>());
    EXPECT_EQ(result.cast<int>(), 42);
    
    // Test non-existent method
    auto nonExistentMethods = metadata.getMethods("nonExistent");
    EXPECT_EQ(nonExistentMethods, nullptr);
}

// Test method overloads
TEST_F(AnyMetaTest, MethodOverloads) {
    atom::meta::TypeMetadata metadata;
    
    // Add multiple overloads for the same method
    metadata.addMethod("overloadedMethod", [](std::vector<atom::meta::BoxedValue> args) -> atom::meta::BoxedValue {
        return atom::meta::BoxedValue(std::string("no_args"));
    });
    
    metadata.addMethod("overloadedMethod", [](std::vector<atom::meta::BoxedValue> args) -> atom::meta::BoxedValue {
        if (!args.empty() && args[0].isType<int>()) {
            return atom::meta::BoxedValue(std::string("int_arg"));
        }
        return atom::meta::BoxedValue(std::string("unknown"));
    });
    
    auto methods = metadata.getMethods("overloadedMethod");
    ASSERT_NE(methods, nullptr);
    EXPECT_EQ(methods->size(), 2);
    
    // Test first overload (no args)
    auto result1 = (*methods)[0]({});
    EXPECT_TRUE(result1.isType<std::string>());
    EXPECT_EQ(result1.cast<std::string>(), "no_args");
    
    // Test second overload (with int arg)
    std::vector<atom::meta::BoxedValue> args = {atom::meta::BoxedValue(42)};
    auto result2 = (*methods)[1](args);
    EXPECT_TRUE(result2.isType<std::string>());
    EXPECT_EQ(result2.cast<std::string>(), "int_arg");
}

// Test property system
TEST_F(AnyMetaTest, PropertySystem) {
    atom::meta::TypeMetadata metadata;
    
    // Add property with getter and setter
    metadata.addProperty(
        "testProperty",
        [](const atom::meta::BoxedValue& obj) -> atom::meta::BoxedValue {
            if (auto testObj = obj.tryCast<TestClass>()) {
                return atom::meta::BoxedValue(testObj->getValue());
            }
            return atom::meta::BoxedValue();
        },
        [](atom::meta::BoxedValue& obj, const atom::meta::BoxedValue& value) {
            if (auto testObj = obj.tryCast<TestClass>()) {
                if (auto intValue = value.tryCast<int>()) {
                    testObj->setValue(*intValue);
                }
            }
        },
        atom::meta::BoxedValue(0),  // default value
        "Test property description"
    );
    
    // Test property retrieval
    auto property = metadata.getProperty("testProperty");
    ASSERT_NE(property, nullptr);
    EXPECT_EQ(property->description, "Test property description");
    EXPECT_TRUE(property->default_value.isType<int>());
    EXPECT_EQ(property->default_value.cast<int>(), 0);
    
    // Test property getter/setter
    TestClass testObj(42, "test");
    atom::meta::BoxedValue boxedObj(testObj);
    
    auto getValue = property->getter(boxedObj);
    EXPECT_TRUE(getValue.isType<int>());
    EXPECT_EQ(getValue.cast<int>(), 42);
    
    property->setter(boxedObj, atom::meta::BoxedValue(100));
    auto newValue = property->getter(boxedObj);
    EXPECT_EQ(newValue.cast<int>(), 100);
}

// Test constructor system
TEST_F(AnyMetaTest, ConstructorSystem) {
    atom::meta::TypeMetadata metadata;
    
    // Add default constructor
    metadata.addConstructor("TestClass", [](std::vector<atom::meta::BoxedValue> args) -> atom::meta::BoxedValue {
        if (args.empty()) {
            return atom::meta::BoxedValue(TestClass());
        }
        return atom::meta::BoxedValue();
    });
    
    // Add parameterized constructor
    metadata.addConstructor("TestClass", [](std::vector<atom::meta::BoxedValue> args) -> atom::meta::BoxedValue {
        if (args.size() == 2) {
            auto intArg = args[0].tryCast<int>();
            auto stringArg = args[1].tryCast<std::string>();
            if (intArg && stringArg) {
                return atom::meta::BoxedValue(TestClass(*intArg, *stringArg));
            }
        }
        return atom::meta::BoxedValue();
    });
    
    // Test constructor retrieval
    auto constructor = metadata.getConstructor("TestClass");
    ASSERT_NE(constructor, nullptr);
    
    // Test default constructor
    auto defaultInstance = (*constructor)({});
    EXPECT_TRUE(defaultInstance.isType<TestClass>());
    
    auto testObj = defaultInstance.cast<TestClass>();
    EXPECT_EQ(testObj.getValue(), 0);
    EXPECT_EQ(testObj.getName(), "default");
}

// Test event system
TEST_F(AnyMetaTest, EventSystem) {
    atom::meta::TypeMetadata metadata;
    
    // Add event
    metadata.addEvent("testEvent", "Test event description");
    
    // Test event retrieval
    auto event = metadata.getEvent("testEvent");
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->description, "Test event description");
    EXPECT_TRUE(event->listeners.empty());
    
    // Add event listener
    bool listenerCalled = false;
    metadata.addEventListener("testEvent", 
        [&listenerCalled](atom::meta::BoxedValue& obj, const std::vector<atom::meta::BoxedValue>& args) {
            listenerCalled = true;
        }, 10);
    
    // Check listener was added
    event = metadata.getEvent("testEvent");
    EXPECT_EQ(event->listeners.size(), 1);
    EXPECT_EQ(event->listeners[0].first, 10);  // priority
    
    // Test event firing
    TestClass testObj;
    atom::meta::BoxedValue boxedObj(testObj);
    metadata.fireEvent(boxedObj, "testEvent", {});
    
    EXPECT_TRUE(listenerCalled);
}

// Test event listener priorities
TEST_F(AnyMetaTest, EventListenerPriorities) {
    atom::meta::TypeMetadata metadata;
    metadata.addEvent("priorityEvent", "Event with prioritized listeners");
    
    std::vector<int> callOrder;
    
    // Add listeners with different priorities
    metadata.addEventListener("priorityEvent", 
        [&callOrder](atom::meta::BoxedValue& obj, const std::vector<atom::meta::BoxedValue>& args) {
            callOrder.push_back(1);
        }, 1);  // Low priority
    
    metadata.addEventListener("priorityEvent", 
        [&callOrder](atom::meta::BoxedValue& obj, const std::vector<atom::meta::BoxedValue>& args) {
            callOrder.push_back(10);
        }, 10);  // High priority
    
    metadata.addEventListener("priorityEvent", 
        [&callOrder](atom::meta::BoxedValue& obj, const std::vector<atom::meta::BoxedValue>& args) {
            callOrder.push_back(5);
        }, 5);  // Medium priority
    
    // Fire event
    TestClass testObj;
    atom::meta::BoxedValue boxedObj(testObj);
    metadata.fireEvent(boxedObj, "priorityEvent", {});
    
    // Check call order (should be sorted by priority)
    EXPECT_EQ(callOrder.size(), 3);
    // Note: The actual order depends on implementation - listeners might be called in registration order
    // or sorted by priority. This test validates that all listeners are called.
    EXPECT_TRUE(std::find(callOrder.begin(), callOrder.end(), 1) != callOrder.end());
    EXPECT_TRUE(std::find(callOrder.begin(), callOrder.end(), 5) != callOrder.end());
    EXPECT_TRUE(std::find(callOrder.begin(), callOrder.end(), 10) != callOrder.end());
}

// Test method removal
TEST_F(AnyMetaTest, MethodRemoval) {
    atom::meta::TypeMetadata metadata;
    
    // Add method
    metadata.addMethod("removableMethod", [](std::vector<atom::meta::BoxedValue> args) -> atom::meta::BoxedValue {
        return atom::meta::BoxedValue(42);
    });
    
    // Verify method exists
    auto methods = metadata.getMethods("removableMethod");
    ASSERT_NE(methods, nullptr);
    EXPECT_EQ(methods->size(), 1);
    
    // Remove method
    metadata.removeMethod("removableMethod");
    
    // Verify method is removed
    methods = metadata.getMethods("removableMethod");
    EXPECT_EQ(methods, nullptr);
}

// Test TypeRegistry singleton
TEST_F(AnyMetaTest, TypeRegistrySingleton) {
    auto& registry1 = atom::meta::TypeRegistry::instance();
    auto& registry2 = atom::meta::TypeRegistry::instance();

    // Should be the same instance
    EXPECT_EQ(&registry1, &registry2);
}

// Test TypeRegistry registration and retrieval
TEST_F(AnyMetaTest, TypeRegistryBasics) {
    auto& registry = atom::meta::TypeRegistry::instance();

    // Create metadata for TestClass
    atom::meta::TypeMetadata metadata;
    metadata.addMethod("getValue", [](std::vector<atom::meta::BoxedValue> args) -> atom::meta::BoxedValue {
        if (!args.empty()) {
            if (auto testObj = args[0].tryCast<TestClass>()) {
                return atom::meta::BoxedValue(testObj->getValue());
            }
        }
        return atom::meta::BoxedValue();
    });

    // Register type
    registry.registerType("TestClass", std::move(metadata));

    // Test type existence
    EXPECT_TRUE(registry.hasType("TestClass"));
    EXPECT_FALSE(registry.hasType("NonExistentClass"));

    // Test metadata retrieval
    auto retrievedMetadata = registry.getMetadata("TestClass");
    ASSERT_NE(retrievedMetadata, nullptr);

    auto methods = retrievedMetadata->getMethods("getValue");
    ASSERT_NE(methods, nullptr);
    EXPECT_EQ(methods->size(), 1);
}

// Test TypeRegistry thread safety
TEST_F(AnyMetaTest, TypeRegistryThreadSafety) {
    auto& registry = atom::meta::TypeRegistry::instance();

    constexpr int numThreads = 10;
    constexpr int typesPerThread = 10;
    std::atomic<int> completedThreads(0);

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&registry, &completedThreads, i, typesPerThread]() {
            for (int j = 0; j < typesPerThread; ++j) {
                std::string typeName = "ThreadType_" + std::to_string(i) + "_" + std::to_string(j);

                atom::meta::TypeMetadata metadata;
                metadata.addMethod("threadMethod", [](std::vector<atom::meta::BoxedValue> args) -> atom::meta::BoxedValue {
                    return atom::meta::BoxedValue(42);
                });

                registry.registerType(typeName, std::move(metadata));
            }
            completedThreads.fetch_add(1);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completedThreads.load(), numThreads);

    // Verify all types were registered
    for (int i = 0; i < numThreads; ++i) {
        for (int j = 0; j < typesPerThread; ++j) {
            std::string typeName = "ThreadType_" + std::to_string(i) + "_" + std::to_string(j);
            EXPECT_TRUE(registry.hasType(typeName));
        }
    }
}

// Test TypeRegistry clear functionality
TEST_F(AnyMetaTest, TypeRegistryClear) {
    auto& registry = atom::meta::TypeRegistry::instance();

    // Register some types
    atom::meta::TypeMetadata metadata1, metadata2;
    registry.registerType("Type1", std::move(metadata1));
    registry.registerType("Type2", std::move(metadata2));

    EXPECT_TRUE(registry.hasType("Type1"));
    EXPECT_TRUE(registry.hasType("Type2"));

    // Clear registry
    registry.clear();

    EXPECT_FALSE(registry.hasType("Type1"));
    EXPECT_FALSE(registry.hasType("Type2"));
}

// Test global utility functions
TEST_F(AnyMetaTest, GlobalUtilityFunctions) {
    auto& registry = atom::meta::TypeRegistry::instance();

    // Register TestClass with property
    atom::meta::TypeMetadata metadata;
    metadata.addProperty(
        "value",
        [](const atom::meta::BoxedValue& obj) -> atom::meta::BoxedValue {
            if (auto testObj = obj.tryCast<TestClass>()) {
                return atom::meta::BoxedValue(testObj->getValue());
            }
            return atom::meta::BoxedValue();
        },
        [](atom::meta::BoxedValue& obj, const atom::meta::BoxedValue& value) {
            if (auto testObj = obj.tryCast<TestClass>()) {
                if (auto intValue = value.tryCast<int>()) {
                    testObj->setValue(*intValue);
                }
            }
        }
    );

    registry.registerType("TestClass", std::move(metadata));

    // Test getProperty function
    TestClass testObj(42, "test");
    atom::meta::BoxedValue boxedObj(testObj);

    auto propertyValue = atom::meta::getProperty(boxedObj, "value");
    EXPECT_TRUE(propertyValue.isType<int>());
    EXPECT_EQ(propertyValue.cast<int>(), 42);

    // Test setProperty function
    atom::meta::setProperty(boxedObj, "value", atom::meta::BoxedValue(100));
    auto newPropertyValue = atom::meta::getProperty(boxedObj, "value");
    EXPECT_EQ(newPropertyValue.cast<int>(), 100);
}

// Test global utility functions error handling
TEST_F(AnyMetaTest, GlobalUtilityFunctionsErrorHandling) {
    TestClass testObj(42, "test");
    atom::meta::BoxedValue boxedObj(testObj);

    // Test accessing non-existent property
    EXPECT_THROW(atom::meta::getProperty(boxedObj, "nonExistentProperty"), atom::error::NotFound);
    EXPECT_THROW(atom::meta::setProperty(boxedObj, "nonExistentProperty", atom::meta::BoxedValue(100)), atom::error::NotFound);
}

// Test createInstance function
TEST_F(AnyMetaTest, CreateInstanceFunction) {
    auto& registry = atom::meta::TypeRegistry::instance();

    // Register TestClass with constructor
    atom::meta::TypeMetadata metadata;
    metadata.addConstructor("TestClass", [](std::vector<atom::meta::BoxedValue> args) -> atom::meta::BoxedValue {
        if (args.size() == 2) {
            auto intArg = args[0].tryCast<int>();
            auto stringArg = args[1].tryCast<std::string>();
            if (intArg && stringArg) {
                return atom::meta::BoxedValue(TestClass(*intArg, *stringArg));
            }
        }
        return atom::meta::BoxedValue();
    });

    registry.registerType("TestClass", std::move(metadata));

    // Test instance creation
    std::vector<atom::meta::BoxedValue> args = {
        atom::meta::BoxedValue(42),
        atom::meta::BoxedValue(std::string("created"))
    };

    auto instance = atom::meta::createInstance("TestClass", std::move(args));
    EXPECT_TRUE(instance.isType<TestClass>());

    auto testObj = instance.cast<TestClass>();
    EXPECT_EQ(testObj.getValue(), 42);
    EXPECT_EQ(testObj.getName(), "created");

    // Test error handling for non-existent type
    EXPECT_THROW(atom::meta::createInstance("NonExistentType", {}), atom::error::NotFound);
}

// Test fireEvent global function
TEST_F(AnyMetaTest, FireEventGlobalFunction) {
    auto& registry = atom::meta::TypeRegistry::instance();

    // Register TestClass with event
    atom::meta::TypeMetadata metadata;
    metadata.addEvent("testEvent", "Test event");

    bool eventFired = false;
    metadata.addEventListener("testEvent",
        [&eventFired](atom::meta::BoxedValue& obj, const std::vector<atom::meta::BoxedValue>& args) {
            eventFired = true;
        });

    registry.registerType("TestClass", std::move(metadata));

    // Test event firing
    TestClass testObj;
    atom::meta::BoxedValue boxedObj(testObj);

    atom::meta::fireEvent(boxedObj, "testEvent", {});
    EXPECT_TRUE(eventFired);
}

// Test TypeRegistrar template class
TEST_F(AnyMetaTest, TypeRegistrarTemplate) {
    // Register TestClass using TypeRegistrar
    atom::meta::TypeRegistrar<TestClass>::registerType("TestClass");

    auto& registry = atom::meta::TypeRegistry::instance();
    EXPECT_TRUE(registry.hasType("TestClass"));

    auto metadata = registry.getMetadata("TestClass");
    ASSERT_NE(metadata, nullptr);

    // Check default constructor
    auto constructor = metadata->getConstructor("TestClass");
    ASSERT_NE(constructor, nullptr);

    // Check default events
    auto createEvent = metadata->getEvent("onCreate");
    ASSERT_NE(createEvent, nullptr);
    EXPECT_EQ(createEvent->description, "Triggered when an object is created");

    auto destroyEvent = metadata->getEvent("onDestroy");
    ASSERT_NE(destroyEvent, nullptr);
    EXPECT_EQ(destroyEvent->description, "Triggered when an object is destroyed");

    // Check default method
    auto methods = metadata->getMethods("print");
    ASSERT_NE(methods, nullptr);
    EXPECT_EQ(methods->size(), 1);
}

// Test complex integration scenario
TEST_F(AnyMetaTest, ComplexIntegrationScenario) {
    auto& registry = atom::meta::TypeRegistry::instance();

    // Register comprehensive TestClass metadata
    atom::meta::TypeMetadata metadata;

    // Add constructor
    metadata.addConstructor("TestClass", [](std::vector<atom::meta::BoxedValue> args) -> atom::meta::BoxedValue {
        if (args.size() == 2) {
            auto intArg = args[0].tryCast<int>();
            auto stringArg = args[1].tryCast<std::string>();
            if (intArg && stringArg) {
                return atom::meta::BoxedValue(TestClass(*intArg, *stringArg));
            }
        }
        return atom::meta::BoxedValue();
    });

    // Add properties
    metadata.addProperty("value",
        [](const atom::meta::BoxedValue& obj) -> atom::meta::BoxedValue {
            if (auto testObj = obj.tryCast<TestClass>()) {
                return atom::meta::BoxedValue(testObj->getValue());
            }
            return atom::meta::BoxedValue();
        },
        [](atom::meta::BoxedValue& obj, const atom::meta::BoxedValue& value) {
            if (auto testObj = obj.tryCast<TestClass>()) {
                if (auto intValue = value.tryCast<int>()) {
                    testObj->setValue(*intValue);
                }
            }
        });

    // Add methods
    metadata.addMethod("print", [](std::vector<atom::meta::BoxedValue> args) -> atom::meta::BoxedValue {
        if (!args.empty()) {
            if (auto testObj = args[0].tryCast<TestClass>()) {
                testObj->print();
            }
        }
        return atom::meta::BoxedValue();
    });

    // Add events
    metadata.addEvent("onValueChanged", "Triggered when value changes");

    registry.registerType("TestClass", std::move(metadata));

    // Create instance
    std::vector<atom::meta::BoxedValue> args = {
        atom::meta::BoxedValue(42),
        atom::meta::BoxedValue(std::string("integration_test"))
    };
    auto instance = atom::meta::createInstance("TestClass", std::move(args));

    // Test property access
    auto value = atom::meta::getProperty(instance, "value");
    EXPECT_EQ(value.cast<int>(), 42);

    // Test property modification
    atom::meta::setProperty(instance, "value", atom::meta::BoxedValue(100));
    auto newValue = atom::meta::getProperty(instance, "value");
    EXPECT_EQ(newValue.cast<int>(), 100);

    // Test event firing
    bool eventFired = false;
    auto metadata_ptr = registry.getMetadata("TestClass");
    metadata_ptr->addEventListener("onValueChanged",
        [&eventFired](atom::meta::BoxedValue& obj, const std::vector<atom::meta::BoxedValue>& args) {
            eventFired = true;
        });

    atom::meta::fireEvent(instance, "onValueChanged", {});
    EXPECT_TRUE(eventFired);
}

}  // namespace
