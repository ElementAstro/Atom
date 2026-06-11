#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include "shortcut.h"
#include "shortcut_binding.h"

namespace shortcut_detector {

/**
 * @brief Shortcut template for creating predefined shortcut patterns
 */
struct ShortcutTemplate {
    std::string name;
    std::string description;
    std::string category;
    std::function<Shortcut()> factory;
    std::vector<std::string> tags;

    ShortcutTemplate(const std::string& n, const std::string& desc,
                    const std::string& cat, std::function<Shortcut()> f)
        : name(n), description(desc), category(cat), factory(f) {}
};

/**
 * @brief Shortcut preset containing multiple related shortcuts
 */
struct ShortcutPreset {
    std::string name;
    std::string description;
    std::vector<std::pair<std::string, Shortcut>> shortcuts;

    ShortcutPreset(const std::string& n, const std::string& desc)
        : name(n), description(desc) {}

    void addShortcut(const std::string& action, const Shortcut& shortcut) {
        shortcuts.emplace_back(action, shortcut);
    }
};

/**
 * @brief Builder pattern for creating complex shortcuts with validation
 */
class ShortcutBuilder {
public:
    ShortcutBuilder();

    /**
     * @brief Set the main key
     */
    ShortcutBuilder& key(char letter);
    ShortcutBuilder& key(uint32_t vkCode);
    ShortcutBuilder& functionKey(int number); // F1-F24
    ShortcutBuilder& numpadKey(int number);   // Numpad 0-9
    ShortcutBuilder& arrowKey(const std::string& direction); // Up, Down, Left, Right

    /**
     * @brief Set modifier keys
     */
    ShortcutBuilder& ctrl(bool enabled = true);
    ShortcutBuilder& alt(bool enabled = true);
    ShortcutBuilder& shift(bool enabled = true);
    ShortcutBuilder& win(bool enabled = true);

    /**
     * @brief Set metadata
     */
    ShortcutBuilder& description(const std::string& desc);
    ShortcutBuilder& category(const std::string& cat);
    ShortcutBuilder& tag(const std::string& t);

    /**
     * @brief Validation options
     */
    ShortcutBuilder& requireModifier(bool required = true);
    ShortcutBuilder& allowSystemReserved(bool allowed = false);
    ShortcutBuilder& validateConflicts(bool validate = true);

    /**
     * @brief Build the shortcut
     */
    Shortcut build();

    /**
     * @brief Build shortcut binding with metadata
     */
    ShortcutBinding buildBinding();

    /**
     * @brief Reset builder to initial state
     */
    ShortcutBuilder& reset();

    /**
     * @brief Validate current configuration
     */
    bool isValid() const;

    /**
     * @brief Get validation errors
     */
    std::vector<std::string> getValidationErrors() const;

private:
    uint32_t vkCode_;
    bool ctrl_, alt_, shift_, win_;
    std::string description_, category_;
    std::vector<std::string> tags_;
    bool requireModifier_, allowSystemReserved_, validateConflicts_;

    void validateConfiguration() const;
};

/**
 * @brief Enhanced factory for creating shortcuts with builder pattern and templates
 */
class ShortcutFactory {
public:
    /**
     * @brief Create a shortcut from a letter or key (legacy)
     */
    static Shortcut create(char letter, bool ctrl = false, bool alt = false,
                           bool shift = false, bool win = false);

    /**
     * @brief Create a shortcut from a virtual key code (legacy)
     */
    static Shortcut createVK(uint32_t vkCode, bool ctrl = false,
                             bool alt = false, bool shift = false,
                             bool win = false);

    /**
     * @brief Parse a shortcut from string (enhanced)
     */
    static Shortcut fromString(const std::string& description);

    /**
     * @brief Create builder instance
     */
    static ShortcutBuilder builder();

    /**
     * @brief Register a shortcut template
     */
    static void registerTemplate(const ShortcutTemplate& tmpl);

    /**
     * @brief Get shortcut from template
     */
    static Shortcut fromTemplate(const std::string& templateName);

    /**
     * @brief Get all available templates
     */
    static std::vector<ShortcutTemplate> getTemplates();

    /**
     * @brief Get templates by category
     */
    static std::vector<ShortcutTemplate> getTemplatesByCategory(const std::string& category);

    /**
     * @brief Register a shortcut preset
     */
    static void registerPreset(const ShortcutPreset& preset);

    /**
     * @brief Get shortcut preset
     */
    static ShortcutPreset getPreset(const std::string& presetName);

    /**
     * @brief Get all available presets
     */
    static std::vector<ShortcutPreset> getPresets();

    /**
     * @brief Initialize default templates and presets
     */
    static void initializeDefaults();

    /**
     * @brief Clear all templates and presets
     */
    static void clear();

    // Common shortcuts (legacy)
    static Shortcut ctrlC() { return create('C', true); }
    static Shortcut ctrlV() { return create('V', true); }
    static Shortcut ctrlX() { return create('X', true); }
    static Shortcut ctrlZ() { return create('Z', true); }
    static Shortcut ctrlY() { return create('Y', true); }
    static Shortcut ctrlS() { return create('S', true); }
    static Shortcut altTab() { return createVK(0x09, false, true); }
    static Shortcut altF4() { return createVK(0x73, false, true); }

    // Enhanced common shortcuts
    static Shortcut ctrlA() { return create('A', true); }
    static Shortcut ctrlF() { return create('F', true); }
    static Shortcut ctrlN() { return create('N', true); }
    static Shortcut ctrlO() { return create('O', true); }
    static Shortcut ctrlP() { return create('P', true); }
    static Shortcut ctrlR() { return create('R', true); }
    static Shortcut ctrlW() { return create('W', true); }
    static Shortcut escape() { return createVK(0x1B); } // VK_ESCAPE
    static Shortcut enter() { return createVK(0x0D); }  // VK_RETURN
    static Shortcut space() { return createVK(0x20); }  // VK_SPACE
    static Shortcut tab() { return createVK(0x09); }    // VK_TAB
    static Shortcut backspace() { return createVK(0x08); } // VK_BACK
    static Shortcut del() { return createVK(0x2E); }    // VK_DELETE

private:
    static std::unordered_map<std::string, ShortcutTemplate> templates_;
    static std::unordered_map<std::string, ShortcutPreset> presets_;
    static bool defaultsInitialized_;
};

}  // namespace shortcut_detector
