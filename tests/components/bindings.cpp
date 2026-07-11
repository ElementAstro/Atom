#include "atom/components/scripting/bindings.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace atom::components::scripting;

namespace {

// Minimal engine satisfying what ClassBinder/ScriptModule use: it only needs
// registerFunction and setGlobal members (the binders are templated on the
// engine type, no inheritance required).
class RecordingEngine {
public:
    void registerFunction(const std::string& name, ScriptFunction function) {
        functions[name] = std::move(function);
    }

    void setGlobal(const std::string& name, const ScriptValue& value) {
        globals[name] = value;
    }

    std::unordered_map<std::string, ScriptFunction> functions;
    std::unordered_map<std::string, ScriptValue> globals;
};

struct TestClass {
    int value = 0;
    int getValue() const { return value; }
};

struct BaseClass {
    virtual ~BaseClass() = default;
};
struct DerivedClass : BaseClass {};
struct UnrelatedClass : BaseClass {};

}  // namespace

// ============================================================================
// ExceptionTranslator
// ============================================================================

TEST(ExceptionTranslatorTest, DefaultTranslation) {
    ExceptionTranslator translator;
    std::runtime_error error("something broke");

    std::string message = translator.translateException(error);
    EXPECT_NE(message.find("C++ Exception"), std::string::npos);
    EXPECT_NE(message.find("something broke"), std::string::npos);
}

TEST(ExceptionTranslatorTest, RegisteredTranslatorIsUsed) {
    ExceptionTranslator translator;
    translator.registerTranslator<std::invalid_argument>(
        [](const std::invalid_argument& e) {
            return std::string("invalid-argument: ") + e.what();
        });

    std::invalid_argument error("bad input");
    EXPECT_EQ(translator.translateException(error),
              "invalid-argument: bad input");

    // Unregistered types still fall back to the default translation.
    std::runtime_error other("other");
    EXPECT_NE(translator.translateException(other).find("C++ Exception"),
              std::string::npos);
}

TEST(ExceptionTranslatorTest, ExecuteWithTranslationSuccess) {
    ExceptionTranslator translator;

    auto result = translator.executeWithTranslation([]() {
        ScriptResult r;
        r.success = true;
        r.returnValue = ScriptValue(int64_t{7});
        return r;
    });

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.returnValue.get<int64_t>(), 7);
}

TEST(ExceptionTranslatorTest, ExecuteWithTranslationCatchesExceptions) {
    ExceptionTranslator translator;

    auto result = translator.executeWithTranslation([]() -> ScriptResult {
        throw std::runtime_error("boom");
    });

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.errorMessage.find("boom"), std::string::npos);
}

TEST(ExceptionTranslatorTest, ExecuteWithTranslationCatchesUnknown) {
    ExceptionTranslator translator;

    auto result =
        translator.executeWithTranslation([]() -> ScriptResult { throw 42; });

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Unknown C++ exception occurred");
}

// ============================================================================
// CallbackManager
// ============================================================================

TEST(CallbackManagerTest, InvokeUnknownCallbackReturnsNil) {
    CallbackManager manager;
    ScriptValue result = manager.invokeCallback("missing", {});
    EXPECT_TRUE(result.holds<std::monostate>());
}

TEST(CallbackManagerTest, RegisterAndInvokeCallback) {
    CallbackManager manager;
    bool called = false;
    manager.registerCallback("ping", [&called]() { called = true; });

    manager.invokeCallback("ping", {});
    EXPECT_TRUE(called);
}

TEST(CallbackManagerTest, CallbackReturnValueIsConverted) {
    CallbackManager manager;
    manager.registerCallback("answer", []() { return int64_t{42}; });

    ScriptValue result = manager.invokeCallback("answer", {});
    ASSERT_TRUE(result.holds<int64_t>());
    EXPECT_EQ(result.get<int64_t>(), 42);
}

TEST(CallbackManagerTest, CreateCppCallbackRoundTrip) {
    CallbackManager manager;

    ScriptFunction scriptFunc =
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
        int64_t sum = 0;
        for (const auto& arg : args) {
            sum += arg.get<int64_t>();
        }
        return ScriptValue(sum);
    };

    auto cppFunc =
        manager.createCppCallback<int64_t(int64_t, int64_t)>(scriptFunc);
    EXPECT_EQ(cppFunc(20, 22), 42);
}

// ============================================================================
// ClassBinder / ScriptModule against a recording engine
// ============================================================================

TEST(ClassBinderTest, DefMethodRegistersQualifiedName) {
    RecordingEngine engine;
    ClassBinder<TestClass, RecordingEngine> binder(engine, "TestClass");

    binder.def_method("getValue", &TestClass::getValue)
        .def_static_method("create", []() { return TestClass{}; });

    EXPECT_TRUE(engine.functions.contains("TestClass.getValue"));
    EXPECT_TRUE(engine.functions.contains("TestClass.create"));
}

TEST(ClassBinderTest, DefConstructorRegistersInit) {
    RecordingEngine engine;
    ClassBinder<TestClass, RecordingEngine> binder(engine, "TestClass");

    binder.def_constructor<>();
    EXPECT_TRUE(engine.functions.contains("TestClass.__init__"));
}

TEST(ClassBinderTest, DefEnumRegistersGlobals) {
    enum class Color { Red = 1, Green = 2 };

    RecordingEngine engine;
    ClassBinder<TestClass, RecordingEngine> binder(engine, "TestClass");

    binder.def_enum<Color>("Color",
                           {{Color::Red, "Red"}, {Color::Green, "Green"}});

    ASSERT_TRUE(engine.globals.contains("TestClass.Color.Red"));
    ASSERT_TRUE(engine.globals.contains("TestClass.Color.Green"));
    EXPECT_EQ(engine.globals["TestClass.Color.Red"].get<int64_t>(), 1);
    EXPECT_EQ(engine.globals["TestClass.Color.Green"].get<int64_t>(), 2);
}

TEST(ScriptModuleTest, DefAndAttrUseModulePrefix) {
    RecordingEngine engine;
    ScriptModule<RecordingEngine> module(engine, "math");

    module.def("zero", []() { return int64_t{0}; }).attr("pi", 3.14159);

    EXPECT_TRUE(engine.functions.contains("math.zero"));
    ASSERT_TRUE(engine.globals.contains("math.pi"));
    EXPECT_DOUBLE_EQ(engine.globals["math.pi"].get<double>(), 3.14159);
}

// ============================================================================
// InheritanceBinder
// ============================================================================

TEST(InheritanceBinderTest, RegisterAndQuery) {
    using Binder = InheritanceBinder<DerivedClass, BaseClass>;

    Binder::registerInheritance("DerivedClass", "BaseClass");
    EXPECT_TRUE(Binder::isDerivedFrom("DerivedClass", "BaseClass"));
    EXPECT_FALSE(Binder::isDerivedFrom("BaseClass", "DerivedClass"));
    EXPECT_FALSE(Binder::isDerivedFrom("Unknown", "BaseClass"));
}

TEST(InheritanceBinderTest, SafeCast) {
    using Binder = InheritanceBinder<DerivedClass, BaseClass>;

    std::shared_ptr<BaseClass> derived = std::make_shared<DerivedClass>();
    std::shared_ptr<BaseClass> unrelated = std::make_shared<UnrelatedClass>();

    EXPECT_NE(Binder::safeCast(derived), nullptr);
    EXPECT_EQ(Binder::safeCast(unrelated), nullptr);
}
