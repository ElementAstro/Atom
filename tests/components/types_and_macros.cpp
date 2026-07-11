#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <memory>
#include <string>

#include "atom/components/core/component.hpp"
#include "atom/components/core/module_macro.hpp"
#include "atom/components/core/registry.hpp"
#include "atom/components/core/types.hpp"

// ============================================================================
// ComponentType Tests
// ============================================================================

TEST(ComponentTypeTest, EnumValues) {
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

    ASSERT_EQ(Traits::VALUES.size(), Traits::NAMES.size());
    for (size_t i = 0; i < Traits::VALUES.size(); ++i) {
        EXPECT_EQ(static_cast<size_t>(Traits::VALUES[i]), i)
            << "VALUES must be listed in declaration order";
    }
}

TEST(ComponentTypeTest, EdgeCases) {
    const int lastValueInt = static_cast<int>(ComponentType::LAST_ENUM_VALUE);
    EXPECT_GT(lastValueInt, static_cast<int>(ComponentType::TASK));
    EXPECT_GT(lastValueInt, static_cast<int>(ComponentType::EXECUTABLE));
    EXPECT_GT(lastValueInt, static_cast<int>(ComponentType::SCRIPT));
}

// ============================================================================
// Module Macro Tests — real macros from core/module_macro.hpp
// ============================================================================

namespace {

std::shared_ptr<Component> makeMacroAlpha() {
    return std::make_shared<Component>("macro_alpha");
}

std::shared_ptr<Component> makeMacroBeta() {
    return std::make_shared<Component>("macro_beta");
}

std::atomic<bool> gammaInitialized{false};

}  // namespace

// Dynamic-library style module: registration happens when the generated
// extern "C" entry point is called.
ATOM_MODULE(macro_alpha, makeMacroAlpha)

// Embedded module: registration happens during static initialization.
ATOM_EMBED_MODULE(macro_beta, makeMacroBeta)

REGISTER_INITIALIZER(
    macro_gamma, [](Component&) { gammaInitialized = true; }, nullptr)

TEST(ModuleMacroTest, AtomModuleRegistersInstance) {
    macro_alpha_initialize_registry();

    auto comp = Registry::instance().getComponent("macro_alpha");
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->getName(), "macro_alpha");

    auto instance = macro_alpha_getInstance();
    EXPECT_EQ(instance.get(), comp.get());
}

TEST(ModuleMacroTest, AtomModuleReportsVersion) {
    EXPECT_STREQ(macro_alpha_getVersion(), ATOM_VERSION);
}

TEST(ModuleMacroTest, EmbeddedModuleRegistersAtStaticInit) {
    auto comp = Registry::instance().getComponent("macro_beta");
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->getName(), "macro_beta");

    auto instance = macro_beta_getInstance();
    EXPECT_EQ(instance.get(), comp.get());
}

TEST(ModuleMacroTest, RegisterInitializerDefersInit) {
    auto comp = Registry::instance().getComponent("macro_gamma");
    ASSERT_NE(comp, nullptr);

    Registry::instance().initializeAll();
    EXPECT_TRUE(gammaInitialized.load());
}

TEST(ModuleMacroTest, ModuleCleanupIsIdempotent) {
    macro_alpha_initialize_registry();
    EXPECT_NO_THROW(macro_alpha_cleanup_registry());
    // cleanup() is guarded by std::call_once, so a second call must be safe.
    EXPECT_NO_THROW(macro_alpha_cleanup_registry());
}

// ============================================================================
// ATOM_COMPONENT class-generating macro
// ============================================================================

ATOM_COMPONENT(MacroGeneratedComponent, Component)
int extraValue() const { return 41 + 1; }
ATOM_COMPONENT_END

TEST(ModuleMacroTest, AtomComponentGeneratesUsableClass) {
    auto comp = MacroGeneratedComponent::create();
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->getName(), "MacroGeneratedComponent");
    EXPECT_EQ(comp->extraValue(), 42);

    MacroGeneratedComponent named("custom_name");
    EXPECT_EQ(named.getName(), "custom_name");
}

// ============================================================================
// EnumTraits integration
// ============================================================================

TEST(TypesAndMacrosIntegrationTest, EnumTraitsLookups) {
    using Traits = atom::meta::EnumTraits<ComponentType>;

    auto typeName = [](ComponentType type) -> std::string {
        for (size_t i = 0; i < Traits::VALUES.size(); ++i) {
            if (Traits::VALUES[i] == type) {
                return std::string(Traits::NAMES[i]);
            }
        }
        return "UNKNOWN";
    };
    auto typeByName = [](const std::string& name) -> ComponentType {
        for (size_t i = 0; i < Traits::NAMES.size(); ++i) {
            if (Traits::NAMES[i] == name) {
                return Traits::VALUES[i];
            }
        }
        return ComponentType::NONE;
    };

    EXPECT_EQ(typeName(ComponentType::SHARED), "SHARED");
    EXPECT_EQ(typeName(ComponentType::SCRIPT), "SCRIPT");
    EXPECT_EQ(typeByName("EXECUTABLE"), ComponentType::EXECUTABLE);
    EXPECT_EQ(typeByName("TASK"), ComponentType::TASK);
    EXPECT_EQ(typeByName("nonexistent"), ComponentType::NONE);
}
