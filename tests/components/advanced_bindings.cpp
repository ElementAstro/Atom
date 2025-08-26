#include "atom/components/advanced_bindings.hpp"
#include "atom/components/scripting_api.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <stdexcept>

using namespace atom::components::scripting;

// Test class for binding tests
class TestBindingClass {
public:
    TestBindingClass(int value = 0) : value_(value) {}
    
    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }
    
    int add(int a, int b) const { return a + b; }
    std::string getName() const { return "TestBindingClass"; }
    
    // Static method
    static int staticMethod(int x) { return x * 2; }
    
    // Method that throws exception
    void throwException() const {
        throw std::runtime_error("Test exception");
    }
    
    // Operator overloading
    TestBindingClass operator+(const TestBindingClass& other) const {
        return TestBindingClass(value_ + other.value_);
    }
    
    bool operator==(const TestBindingClass& other) const {
        return value_ == other.value_;
    }

private:
    int value_;
};

// Test fixture for ExceptionTranslator tests
class ExceptionTranslatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        translator_ = std::make_unique<ExceptionTranslator>();
    }

    std::unique_ptr<ExceptionTranslator> translator_;
};

// Test fixture for ClassBinder tests
class ClassBinderTest : public ::testing::Test {
protected:
    void SetUp() override {
        binder_ = std::make_unique<ClassBinder<TestBindingClass>>("TestBindingClass");
    }

    std::unique_ptr<ClassBinder<TestBindingClass>> binder_;
};

// Test fixture for PropertyBinder tests
class PropertyBinderTest : public ::testing::Test {
protected:
    void SetUp() override {
        testObject_ = std::make_shared<TestBindingClass>(42);
        propertyBinder_ = std::make_unique<PropertyBinder<TestBindingClass>>();
    }

    std::shared_ptr<TestBindingClass> testObject_;
    std::unique_ptr<PropertyBinder<TestBindingClass>> propertyBinder_;
};

// Test fixture for CallbackManager tests
class CallbackManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        callbackManager_ = std::make_unique<CallbackManager>();
    }

    std::unique_ptr<CallbackManager> callbackManager_;
};

// ============================================================================
// ExceptionTranslator Tests
// ============================================================================

TEST_F(ExceptionTranslatorTest, RegisterTranslator) {
    // Register a translator for std::runtime_error
    translator_->registerTranslator<std::runtime_error>(
        [](const std::runtime_error& e) {
            return std::string("Runtime Error: ") + e.what();
        });
    
    // Test translation
    std::runtime_error testError("Test message");
    std::string translated = translator_->translate(testError);
    
    EXPECT_EQ(translated, "Runtime Error: Test message");
}

TEST_F(ExceptionTranslatorTest, MultipleTranslators) {
    // Register multiple translators
    translator_->registerTranslator<std::runtime_error>(
        [](const std::runtime_error& e) {
            return std::string("Runtime: ") + e.what();
        });
    
    translator_->registerTranslator<std::logic_error>(
        [](const std::logic_error& e) {
            return std::string("Logic: ") + e.what();
        });
    
    std::runtime_error runtimeError("runtime test");
    std::logic_error logicError("logic test");
    
    EXPECT_EQ(translator_->translate(runtimeError), "Runtime: runtime test");
    EXPECT_EQ(translator_->translate(logicError), "Logic: logic test");
}

TEST_F(ExceptionTranslatorTest, UnregisteredExceptionType) {
    // Try to translate an unregistered exception type
    std::invalid_argument testError("unregistered");
    
    std::string translated = translator_->translate(testError);
    
    // Should return a default message or the original what()
    EXPECT_FALSE(translated.empty());
}

// ============================================================================
// ClassBinder Tests
// ============================================================================

TEST_F(ClassBinderTest, BindConstructor) {
    // Bind default constructor
    binder_->bindConstructor<>();
    
    // Bind constructor with parameters
    binder_->bindConstructor<int>();
    
    // Test that binding doesn't throw
    EXPECT_NO_THROW(binder_->finalize());
}

TEST_F(ClassBinderTest, BindMethod) {
    binder_->bindMethod("getValue", &TestBindingClass::getValue);
    binder_->bindMethod("setValue", &TestBindingClass::setValue);
    binder_->bindMethod("add", &TestBindingClass::add);
    binder_->bindMethod("getName", &TestBindingClass::getName);
    
    EXPECT_NO_THROW(binder_->finalize());
}

TEST_F(ClassBinderTest, BindStaticMethod) {
    binder_->bindStaticMethod("staticMethod", &TestBindingClass::staticMethod);
    
    EXPECT_NO_THROW(binder_->finalize());
}

TEST_F(ClassBinderTest, BindProperty) {
    binder_->bindProperty("value", 
                         &TestBindingClass::getValue, 
                         &TestBindingClass::setValue);
    
    EXPECT_NO_THROW(binder_->finalize());
}

TEST_F(ClassBinderTest, BindOperator) {
    binder_->bindOperator("+", &TestBindingClass::operator+);
    binder_->bindOperator("==", &TestBindingClass::operator==);
    
    EXPECT_NO_THROW(binder_->finalize());
}

TEST_F(ClassBinderTest, SetMetadata) {
    binder_->setDescription("Test class for binding");
    binder_->setVersion("1.0.0");
    binder_->addTag("test");
    binder_->addTag("binding");
    
    auto metadata = binder_->getMetadata();
    EXPECT_EQ(metadata.description, "Test class for binding");
    EXPECT_EQ(metadata.version, "1.0.0");
    EXPECT_EQ(metadata.tags.size(), 2);
}

// ============================================================================
// PropertyBinder Tests
// ============================================================================

TEST_F(PropertyBinderTest, BindReadOnlyProperty) {
    propertyBinder_->bindReadOnly("name", &TestBindingClass::getName);
    
    auto value = propertyBinder_->getValue(*testObject_, "name");
    EXPECT_TRUE(value.has_value());
    if (value.has_value()) {
        EXPECT_EQ(value->get<std::string>(), "TestBindingClass");
    }
}

TEST_F(PropertyBinderTest, BindReadWriteProperty) {
    propertyBinder_->bindReadWrite("value", 
                                  &TestBindingClass::getValue,
                                  &TestBindingClass::setValue);
    
    // Test getter
    auto getValue = propertyBinder_->getValue(*testObject_, "value");
    EXPECT_TRUE(getValue.has_value());
    if (getValue.has_value()) {
        EXPECT_EQ(getValue->get<int64_t>(), 42);
    }
    
    // Test setter
    ScriptValue newValue(100);
    bool setResult = propertyBinder_->setValue(*testObject_, "value", newValue);
    EXPECT_TRUE(setResult);
    
    // Verify the value was set
    EXPECT_EQ(testObject_->getValue(), 100);
}

TEST_F(PropertyBinderTest, BindWriteOnlyProperty) {
    propertyBinder_->bindWriteOnly("writeValue", &TestBindingClass::setValue);
    
    ScriptValue newValue(200);
    bool setResult = propertyBinder_->setValue(*testObject_, "writeValue", newValue);
    EXPECT_TRUE(setResult);
    
    // Verify the value was set
    EXPECT_EQ(testObject_->getValue(), 200);
}

TEST_F(PropertyBinderTest, NonexistentProperty) {
    auto value = propertyBinder_->getValue(*testObject_, "nonexistent");
    EXPECT_FALSE(value.has_value());
    
    ScriptValue testValue(123);
    bool setResult = propertyBinder_->setValue(*testObject_, "nonexistent", testValue);
    EXPECT_FALSE(setResult);
}

// ============================================================================
// CallbackManager Tests
// ============================================================================

TEST_F(CallbackManagerTest, RegisterCallback) {
    bool callbackExecuted = false;
    
    auto callbackId = callbackManager_->registerCallback("test_event", 
        [&callbackExecuted](const std::vector<ScriptValue>& args) {
            callbackExecuted = true;
            return ScriptValue(true);
        });
    
    EXPECT_GT(callbackId, 0);
    
    // Trigger the callback
    std::vector<ScriptValue> args;
    callbackManager_->triggerCallback("test_event", args);
    
    EXPECT_TRUE(callbackExecuted);
}

TEST_F(CallbackManagerTest, MultipleCallbacks) {
    int callbackCount = 0;
    
    // Register multiple callbacks for the same event
    callbackManager_->registerCallback("multi_event", 
        [&callbackCount](const std::vector<ScriptValue>&) {
            callbackCount++;
            return ScriptValue();
        });
    
    callbackManager_->registerCallback("multi_event", 
        [&callbackCount](const std::vector<ScriptValue>&) {
            callbackCount++;
            return ScriptValue();
        });
    
    // Trigger the event
    std::vector<ScriptValue> args;
    callbackManager_->triggerCallback("multi_event", args);
    
    EXPECT_EQ(callbackCount, 2);
}

TEST_F(CallbackManagerTest, UnregisterCallback) {
    bool callbackExecuted = false;
    
    auto callbackId = callbackManager_->registerCallback("unregister_test", 
        [&callbackExecuted](const std::vector<ScriptValue>&) {
            callbackExecuted = true;
            return ScriptValue();
        });
    
    // Unregister the callback
    bool unregisterResult = callbackManager_->unregisterCallback(callbackId);
    EXPECT_TRUE(unregisterResult);
    
    // Trigger the event - callback should not execute
    std::vector<ScriptValue> args;
    callbackManager_->triggerCallback("unregister_test", args);
    
    EXPECT_FALSE(callbackExecuted);
}

TEST_F(CallbackManagerTest, CallbackWithArguments) {
    ScriptValue receivedArg;
    
    callbackManager_->registerCallback("arg_test", 
        [&receivedArg](const std::vector<ScriptValue>& args) {
            if (!args.empty()) {
                receivedArg = args[0];
            }
            return ScriptValue();
        });
    
    // Trigger with arguments
    std::vector<ScriptValue> args = {ScriptValue(42)};
    callbackManager_->triggerCallback("arg_test", args);
    
    EXPECT_EQ(receivedArg.get<int64_t>(), 42);
}

TEST_F(CallbackManagerTest, NonexistentEvent) {
    std::vector<ScriptValue> args;
    
    // Should handle nonexistent event gracefully
    EXPECT_NO_THROW(callbackManager_->triggerCallback("nonexistent_event", args));
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(AdvancedBindingsIntegrationTest, CompleteClassBinding) {
    // Create a complete class binding
    ClassBinder<TestBindingClass> binder("TestBindingClass");
    
    // Bind everything
    binder.bindConstructor<>();
    binder.bindConstructor<int>();
    binder.bindMethod("getValue", &TestBindingClass::getValue);
    binder.bindMethod("setValue", &TestBindingClass::setValue);
    binder.bindMethod("add", &TestBindingClass::add);
    binder.bindStaticMethod("staticMethod", &TestBindingClass::staticMethod);
    binder.bindProperty("value", &TestBindingClass::getValue, &TestBindingClass::setValue);
    binder.bindOperator("+", &TestBindingClass::operator+);
    
    EXPECT_NO_THROW(binder.finalize());
}

TEST(AdvancedBindingsIntegrationTest, ExceptionHandling) {
    ExceptionTranslator translator;
    
    translator.registerTranslator<std::runtime_error>(
        [](const std::runtime_error& e) {
            return std::string("Caught: ") + e.what();
        });
    
    TestBindingClass testObj;
    
    try {
        testObj.throwException();
        FAIL() << "Expected exception was not thrown";
    } catch (const std::runtime_error& e) {
        std::string translated = translator.translate(e);
        EXPECT_EQ(translated, "Caught: Test exception");
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(AdvancedBindingsErrorTest, InvalidMethodBinding) {
    ClassBinder<TestBindingClass> binder("TestClass");
    
    // Try to bind a method that doesn't exist (this would be a compile-time error)
    // So we test that valid bindings don't throw
    EXPECT_NO_THROW(binder.bindMethod("getValue", &TestBindingClass::getValue));
}

TEST(AdvancedBindingsErrorTest, InvalidPropertyAccess) {
    PropertyBinder<TestBindingClass> propertyBinder;
    TestBindingClass testObj;
    
    // Try to access a property that wasn't bound
    auto value = propertyBinder.getValue(testObj, "unboundProperty");
    EXPECT_FALSE(value.has_value());
}

TEST(AdvancedBindingsErrorTest, CallbackException) {
    CallbackManager manager;
    
    // Register a callback that throws
    manager.registerCallback("exception_test", 
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            throw std::runtime_error("Callback exception");
        });
    
    // Triggering should handle the exception gracefully
    std::vector<ScriptValue> args;
    EXPECT_NO_THROW(manager.triggerCallback("exception_test", args));
}
