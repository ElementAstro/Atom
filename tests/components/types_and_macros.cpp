#include "atom/components/module_macro.hpp"
#include "atom/components/types.hpp"

#include <gtest/gtest.h>
#include <array>
#include <string>

// ============================================================================
// ComponentType Tests
// ============================================================================

TEST(ComponentTypeTest, EnumValues) {
    // Test that all enum values are properly defined
    EXPECT_EQ(static_cast<int>(ComponentType::NONE), 0);
    EXPECT_EQ(static_cast<int>(ComponentType::SHARED), 1);
    EXPECT_EQ(static_cast<int>(ComponentType::SHARED_INJECTED), 2);
    EXPECT_EQ(static_cast<int>(ComponentType::SCRIPT), 3);
    EXPECT_EQ(static_cast<int>(ComponentType::EXECUTABLE), 4);
    EXPECT_EQ(static_cast<int>(ComponentType::TASK), 5);
    EXPECT_EQ(static_cast<int>(ComponentType::LAST_ENUM_VALUE), 6);
}

TEST(ComponentTypeTest, EnumTraitsValues) {
    using Traits = atom::meta::EnumTraits<ComponentType>;

    // Test that VALUES array contains all enum values
    EXPECT_EQ(Traits::VALUES.size(), 7);
    EXPECT_EQ(Traits::VALUES[0], ComponentType::NONE);
    EXPECT_EQ(Traits::VALUES[1], ComponentType::SHARED);
    EXPECT_EQ(Traits::VALUES[2], ComponentType::SHARED_INJECTED);
    EXPECT_EQ(Traits::VALUES[3], ComponentType::SCRIPT);
    EXPECT_EQ(Traits::VALUES[4], ComponentType::EXECUTABLE);
    EXPECT_EQ(Traits::VALUES[5], ComponentType::TASK);
    EXPECT_EQ(Traits::VALUES[6], ComponentType::LAST_ENUM_VALUE);
}

TEST(ComponentTypeTest, EnumTraitsNames) {
    using Traits = atom::meta::EnumTraits<ComponentType>;

    // Test that NAMES array contains all enum names
    EXPECT_EQ(Traits::NAMES.size(), 7);
    EXPECT_EQ(Traits::NAMES[0], "NONE");
    EXPECT_EQ(Traits::NAMES[1], "SHARED");
    EXPECT_EQ(Traits::NAMES[2], "SHARED_INJECTED");
    EXPECT_EQ(Traits::NAMES[3], "SCRIPT");
    EXPECT_EQ(Traits::NAMES[4], "EXECUTABLE");
    EXPECT_EQ(Traits::NAMES[5], "TASK");
    EXPECT_EQ(Traits::NAMES[6], "LAST_ENUM_VALUE");
}

TEST(ComponentTypeTest, EnumTraitsConsistency) {
    using Traits = atom::meta::EnumTraits<ComponentType>;

    // Test that VALUES and NAMES arrays have the same size
    EXPECT_EQ(Traits::VALUES.size(), Traits::NAMES.size());

    // Test that each value corresponds to the correct name
    for (size_t i = 0; i < Traits::VALUES.size(); ++i) {
        ComponentType value = Traits::VALUES[i];
        std::string_view name = Traits::NAMES[i];

        // Verify the mapping is correct
        switch (value) {
            case ComponentType::NONE:
                EXPECT_EQ(name, "NONE");
                break;
            case ComponentType::SHARED:
                EXPECT_EQ(name, "SHARED");
                break;
            case ComponentType::SHARED_INJECTED:
                EXPECT_EQ(name, "SHARED_INJECTED");
                break;
            case ComponentType::SCRIPT:
                EXPECT_EQ(name, "SCRIPT");
                break;
            case ComponentType::EXECUTABLE:
                EXPECT_EQ(name, "EXECUTABLE");
                break;
            case ComponentType::TASK:
                EXPECT_EQ(name, "TASK");
                break;
            case ComponentType::LAST_ENUM_VALUE:
                EXPECT_EQ(name, "LAST_ENUM_VALUE");
                break;
        }
    }
}

// ============================================================================
// Module Macro Tests
// ============================================================================

// Test component class using module macros
class TestModuleComponent {
public:
    TestModuleComponent(const std::string& name) : name_(name) {}

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }

    bool isActive() const { return active_; }
    void setActive(bool active) { active_ = active; }

private:
    std::string name_;
    int value_ = 0;
    bool active_ = false;
};

// Apply module macros to the test component
ATOM_COMPONENT_MODULE_BEGIN(TestModuleComponent)
ATOM_COMPONENT_PROPERTY(name, getName, setName)
ATOM_COMPONENT_PROPERTY(value, getValue, setValue)
ATOM_COMPONENT_PROPERTY(active, isActive, setActive)
ATOM_COMPONENT_METHOD(getName)
ATOM_COMPONENT_METHOD(setName)
ATOM_COMPONENT_METHOD(getValue)
ATOM_COMPONENT_METHOD(setValue)
ATOM_COMPONENT_METHOD(isActive)
ATOM_COMPONENT_METHOD(setActive)
ATOM_COMPONENT_MODULE_END()

TEST(ModuleMacroTest, ComponentModuleDefinition) {
    TestModuleComponent component("TestComponent");

    // Test that the component works normally
    EXPECT_EQ(component.getName(), "TestComponent");
    EXPECT_EQ(component.getValue(), 0);
    EXPECT_FALSE(component.isActive());

    // Test property setters
    component.setName("ModifiedComponent");
    component.setValue(42);
    component.setActive(true);

    EXPECT_EQ(component.getName(), "ModifiedComponent");
    EXPECT_EQ(component.getValue(), 42);
    EXPECT_TRUE(component.isActive());
}

TEST(ModuleMacroTest, PropertyMacroExpansion) {
    // Test that property macros expand correctly
    // This is mainly a compilation test to ensure macros are syntactically
    // correct
    TestModuleComponent component("PropertyTest");

    // Test all properties
    component.setName("TestName");
    EXPECT_EQ(component.getName(), "TestName");

    component.setValue(123);
    EXPECT_EQ(component.getValue(), 123);

    component.setActive(true);
    EXPECT_TRUE(component.isActive());
}

TEST(ModuleMacroTest, MethodMacroExpansion) {
    // Test that method macros expand correctly
    TestModuleComponent component("MethodTest");

    // Test that all methods are accessible
    EXPECT_NO_THROW(component.getName());
    EXPECT_NO_THROW(component.getValue());
    EXPECT_NO_THROW(component.isActive());

    EXPECT_NO_THROW(component.setName("NewName"));
    EXPECT_NO_THROW(component.setValue(456));
    EXPECT_NO_THROW(component.setActive(false));
}

// ============================================================================
// Module Registration Tests
// ============================================================================

TEST(ModuleMacroTest, ModuleRegistration) {
    // Test module registration functionality
    // This tests that the ATOM_COMPONENT_MODULE_* macros create proper
    // registration

    // Create component instance
    TestModuleComponent component("RegistrationTest");

    // Test that component can be used in various contexts
    std::vector<TestModuleComponent> components;
    components.emplace_back("Component1");
    components.emplace_back("Component2");

    EXPECT_EQ(components.size(), 2);
    EXPECT_EQ(components[0].getName(), "Component1");
    EXPECT_EQ(components[1].getName(), "Component2");
}

// ============================================================================
// Macro Safety Tests
// ============================================================================

TEST(ModuleMacroTest, MacroSafety) {
    // Test that macros don't interfere with normal C++ functionality

    // Test that we can still use normal inheritance
    class DerivedComponent : public TestModuleComponent {
    public:
        DerivedComponent(const std::string& name, int extraValue)
            : TestModuleComponent(name), extraValue_(extraValue) {}

        int getExtraValue() const { return extraValue_; }
        void setExtraValue(int value) { extraValue_ = value; }

    private:
        int extraValue_;
    };

    DerivedComponent derived("DerivedTest", 999);

    // Test base class functionality
    EXPECT_EQ(derived.getName(), "DerivedTest");
    EXPECT_EQ(derived.getValue(), 0);

    // Test derived class functionality
    EXPECT_EQ(derived.getExtraValue(), 999);

    derived.setExtraValue(777);
    EXPECT_EQ(derived.getExtraValue(), 777);
}

TEST(ModuleMacroTest, MultipleComponents) {
    // Test that macros work with multiple component types

    class AnotherTestComponent {
    public:
        AnotherTestComponent(double value) : value_(value) {}

        double getValue() const { return value_; }
        void setValue(double value) { value_ = value; }

    private:
        double value_;
    };

    // Apply macros to another component
    ATOM_COMPONENT_MODULE_BEGIN(AnotherTestComponent)
    ATOM_COMPONENT_PROPERTY(value, getValue, setValue)
    ATOM_COMPONENT_METHOD(getValue)
    ATOM_COMPONENT_METHOD(setValue)
    ATOM_COMPONENT_MODULE_END()

    // Test both components work independently
    TestModuleComponent comp1("Test1");
    AnotherTestComponent comp2(3.14);

    comp1.setName("ModifiedTest1");
    comp2.setValue(2.71);

    EXPECT_EQ(comp1.getName(), "ModifiedTest1");
    EXPECT_DOUBLE_EQ(comp2.getValue(), 2.71);
}

// ============================================================================
// Compilation Tests
// ============================================================================

TEST(ModuleMacroTest, MacroCompilation) {
    // This test primarily ensures that all macros compile correctly
    // and don't produce syntax errors

    // Test that we can create multiple instances
    std::array<TestModuleComponent, 3> components = {
        TestModuleComponent("Comp1"), TestModuleComponent("Comp2"),
        TestModuleComponent("Comp3")};

    // Test that all instances work correctly
    for (size_t i = 0; i < components.size(); ++i) {
        std::string expectedName = "Comp" + std::to_string(i + 1);
        EXPECT_EQ(components[i].getName(), expectedName);

        // Test property modification
        components[i].setValue(static_cast<int>(i * 10));
        EXPECT_EQ(components[i].getValue(), static_cast<int>(i * 10));
    }
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(ComponentTypeTest, EdgeCases) {
    // Test edge cases for ComponentType enum

    // Test that LAST_ENUM_VALUE is indeed the last value
    ComponentType lastValue = ComponentType::LAST_ENUM_VALUE;
    int lastValueInt = static_cast<int>(lastValue);

    // Should be the highest value
    EXPECT_GT(lastValueInt, static_cast<int>(ComponentType::TASK));
    EXPECT_GT(lastValueInt, static_cast<int>(ComponentType::EXECUTABLE));
    EXPECT_GT(lastValueInt, static_cast<int>(ComponentType::SCRIPT));
}

TEST(ModuleMacroTest, EdgeCases) {
    // Test edge cases for module macros

    // Test with empty component
    class EmptyComponent {
    public:
        EmptyComponent() = default;
    };

    ATOM_COMPONENT_MODULE_BEGIN(EmptyComponent)
    // No properties or methods
    ATOM_COMPONENT_MODULE_END()

    EmptyComponent empty;
    // Should compile and work without issues
    EXPECT_NO_THROW(EmptyComponent());
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(TypesAndMacrosIntegrationTest, ComponentTypeWithMacros) {
    // Test integration between ComponentType enum and module macros

    class TypedComponent {
    public:
        TypedComponent(ComponentType type) : type_(type) {}

        ComponentType getType() const { return type_; }
        void setType(ComponentType type) { type_ = type; }

    private:
        ComponentType type_;
    };

    ATOM_COMPONENT_MODULE_BEGIN(TypedComponent)
    ATOM_COMPONENT_PROPERTY(type, getType, setType)
    ATOM_COMPONENT_METHOD(getType)
    ATOM_COMPONENT_METHOD(setType)
    ATOM_COMPONENT_MODULE_END()

    TypedComponent component(ComponentType::SHARED);

    EXPECT_EQ(component.getType(), ComponentType::SHARED);

    component.setType(ComponentType::SCRIPT);
    EXPECT_EQ(component.getType(), ComponentType::SCRIPT);
}

TEST(TypesAndMacrosIntegrationTest, EnumTraitsWithMacros) {
    // Test that enum traits work correctly with macro-enhanced components

    using Traits = atom::meta::EnumTraits<ComponentType>;

    class EnumTraitsComponent {
    public:
        EnumTraitsComponent() = default;

        std::string getTypeName(ComponentType type) const {
            for (size_t i = 0; i < Traits::VALUES.size(); ++i) {
                if (Traits::VALUES[i] == type) {
                    return std::string(Traits::NAMES[i]);
                }
            }
            return "UNKNOWN";
        }

        ComponentType getTypeByName(const std::string& name) const {
            for (size_t i = 0; i < Traits::NAMES.size(); ++i) {
                if (Traits::NAMES[i] == name) {
                    return Traits::VALUES[i];
                }
            }
            return ComponentType::NONE;
        }
    };

    ATOM_COMPONENT_MODULE_BEGIN(EnumTraitsComponent)
    ATOM_COMPONENT_METHOD(getTypeName)
    ATOM_COMPONENT_METHOD(getTypeByName)
    ATOM_COMPONENT_MODULE_END()

    EnumTraitsComponent component;

    // Test type name lookup
    EXPECT_EQ(component.getTypeName(ComponentType::SHARED), "SHARED");
    EXPECT_EQ(component.getTypeName(ComponentType::SCRIPT), "SCRIPT");

    // Test type lookup by name
    EXPECT_EQ(component.getTypeByName("EXECUTABLE"), ComponentType::EXECUTABLE);
    EXPECT_EQ(component.getTypeByName("TASK"), ComponentType::TASK);
    EXPECT_EQ(component.getTypeByName("UNKNOWN"), ComponentType::NONE);
}
