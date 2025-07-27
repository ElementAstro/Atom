#include "factory.h"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>
#include <sstream>
#include <spdlog/spdlog.h>

namespace shortcut_detector {

// Map of key names to VK codes (expanded)
static const std::map<std::string, uint32_t> keyNameToVK = {
    {"TAB", VK_TAB},       {"ENTER", VK_RETURN},  {"RETURN", VK_RETURN},
    {"ESC", VK_ESCAPE},    {"ESCAPE", VK_ESCAPE}, {"SPACE", VK_SPACE},
    {"PGUP", VK_PRIOR},    {"PAGEUP", VK_PRIOR},  {"PGDN", VK_NEXT},
    {"PAGEDOWN", VK_NEXT}, {"END", VK_END},       {"HOME", VK_HOME},
    {"LEFT", VK_LEFT},     {"UP", VK_UP},         {"RIGHT", VK_RIGHT},
    {"DOWN", VK_DOWN},     {"INS", VK_INSERT},    {"INSERT", VK_INSERT},
    {"DEL", VK_DELETE},    {"DELETE", VK_DELETE}, {"BACKSPACE", VK_BACK},
    {"F1", VK_F1},         {"F2", VK_F2},         {"F3", VK_F3},
    {"F4", VK_F4},         {"F5", VK_F5},         {"F6", VK_F6},
    {"F7", VK_F7},         {"F8", VK_F8},         {"F9", VK_F9},
    {"F10", VK_F10},       {"F11", VK_F11},       {"F12", VK_F12},
    {"F13", VK_F13},       {"F14", VK_F14},       {"F15", VK_F15},
    {"F16", VK_F16},       {"F17", VK_F17},       {"F18", VK_F18},
    {"F19", VK_F19},       {"F20", VK_F20},       {"F21", VK_F21},
    {"F22", VK_F22},       {"F23", VK_F23},       {"F24", VK_F24},
    {"NUMPAD0", VK_NUMPAD0}, {"NUMPAD1", VK_NUMPAD1}, {"NUMPAD2", VK_NUMPAD2},
    {"NUMPAD3", VK_NUMPAD3}, {"NUMPAD4", VK_NUMPAD4}, {"NUMPAD5", VK_NUMPAD5},
    {"NUMPAD6", VK_NUMPAD6}, {"NUMPAD7", VK_NUMPAD7}, {"NUMPAD8", VK_NUMPAD8},
    {"NUMPAD9", VK_NUMPAD9}, {"MULTIPLY", VK_MULTIPLY}, {"ADD", VK_ADD},
    {"SUBTRACT", VK_SUBTRACT}, {"DECIMAL", VK_DECIMAL}, {"DIVIDE", VK_DIVIDE}
};

// Static member definitions
std::unordered_map<std::string, ShortcutTemplate> ShortcutFactory::templates_;
std::unordered_map<std::string, ShortcutPreset> ShortcutFactory::presets_;
bool ShortcutFactory::defaultsInitialized_ = false;

Shortcut ShortcutFactory::create(char letter, bool ctrl, bool alt, bool shift,
                                 bool win) {
    // Convert to uppercase
    uint32_t vkCode = std::toupper(static_cast<unsigned char>(letter));
    return Shortcut(vkCode, ctrl, alt, shift, win);
}

Shortcut ShortcutFactory::createVK(uint32_t vkCode, bool ctrl, bool alt,
                                   bool shift, bool win) {
    return Shortcut(vkCode, ctrl, alt, shift, win);
}

Shortcut ShortcutFactory::fromString(const std::string& description) {
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    bool win = false;
    uint32_t vkCode = 0;

    // Parse the string
    std::string input = description;
    std::transform(input.begin(), input.end(), input.begin(), ::toupper);

    // Check for modifiers
    if (input.find("CTRL+") != std::string::npos ||
        input.find("CONTROL+") != std::string::npos) {
        ctrl = true;
    }
    if (input.find("ALT+") != std::string::npos) {
        alt = true;
    }
    if (input.find("SHIFT+") != std::string::npos) {
        shift = true;
    }
    if (input.find("WIN+") != std::string::npos ||
        input.find("WINDOWS+") != std::string::npos) {
        win = true;
    }

    // Find the key part (after the last '+')
    size_t lastPlusPos = input.find_last_of('+');
    std::string keyPart = (lastPlusPos != std::string::npos)
                              ? input.substr(lastPlusPos + 1)
                              : input;

    // Trim leading/trailing whitespace
    keyPart.erase(0, keyPart.find_first_not_of(" \t\n\r\f\v"));
    keyPart.erase(keyPart.find_last_not_of(" \t\n\r\f\v") + 1);

    // Check if it's a named key
    auto it = keyNameToVK.find(keyPart);
    if (it != keyNameToVK.end()) {
        vkCode = it->second;
    } else if (keyPart.size() == 1) {
        // Single character
        vkCode = std::toupper(static_cast<unsigned char>(keyPart[0]));
    } else {
        throw std::invalid_argument(
            "Invalid key name in shortcut description: " + keyPart);
    }

    return Shortcut(vkCode, ctrl, alt, shift, win);
}

// ShortcutBuilder implementation
ShortcutBuilder::ShortcutBuilder() {
    reset();
}

ShortcutBuilder& ShortcutBuilder::key(char letter) {
    vkCode_ = std::toupper(static_cast<unsigned char>(letter));
    return *this;
}

ShortcutBuilder& ShortcutBuilder::key(uint32_t vkCode) {
    vkCode_ = vkCode;
    return *this;
}

ShortcutBuilder& ShortcutBuilder::functionKey(int number) {
    if (number >= 1 && number <= 24) {
#ifdef _WIN32
        vkCode_ = VK_F1 + (number - 1);
#else
        vkCode_ = 0x70 + (number - 1); // Fallback VK codes
#endif
    }
    return *this;
}

ShortcutBuilder& ShortcutBuilder::numpadKey(int number) {
    if (number >= 0 && number <= 9) {
#ifdef _WIN32
        vkCode_ = VK_NUMPAD0 + number;
#else
        vkCode_ = 0x60 + number; // Fallback VK codes
#endif
    }
    return *this;
}

ShortcutBuilder& ShortcutBuilder::arrowKey(const std::string& direction) {
    std::string dir = direction;
    std::transform(dir.begin(), dir.end(), dir.begin(), ::toupper);

#ifdef _WIN32
    if (dir == "UP") vkCode_ = VK_UP;
    else if (dir == "DOWN") vkCode_ = VK_DOWN;
    else if (dir == "LEFT") vkCode_ = VK_LEFT;
    else if (dir == "RIGHT") vkCode_ = VK_RIGHT;
#else
    if (dir == "UP") vkCode_ = 0x26;
    else if (dir == "DOWN") vkCode_ = 0x28;
    else if (dir == "LEFT") vkCode_ = 0x25;
    else if (dir == "RIGHT") vkCode_ = 0x27;
#endif
    return *this;
}

ShortcutBuilder& ShortcutBuilder::ctrl(bool enabled) {
    ctrl_ = enabled;
    return *this;
}

ShortcutBuilder& ShortcutBuilder::alt(bool enabled) {
    alt_ = enabled;
    return *this;
}

ShortcutBuilder& ShortcutBuilder::shift(bool enabled) {
    shift_ = enabled;
    return *this;
}

ShortcutBuilder& ShortcutBuilder::win(bool enabled) {
    win_ = enabled;
    return *this;
}

ShortcutBuilder& ShortcutBuilder::description(const std::string& desc) {
    description_ = desc;
    return *this;
}

ShortcutBuilder& ShortcutBuilder::category(const std::string& cat) {
    category_ = cat;
    return *this;
}

ShortcutBuilder& ShortcutBuilder::tag(const std::string& t) {
    tags_.push_back(t);
    return *this;
}

ShortcutBuilder& ShortcutBuilder::requireModifier(bool required) {
    requireModifier_ = required;
    return *this;
}

ShortcutBuilder& ShortcutBuilder::allowSystemReserved(bool allowed) {
    allowSystemReserved_ = allowed;
    return *this;
}

ShortcutBuilder& ShortcutBuilder::validateConflicts(bool validate) {
    validateConflicts_ = validate;
    return *this;
}

Shortcut ShortcutBuilder::build() {
    validateConfiguration();
    return Shortcut(vkCode_, ctrl_, alt_, shift_, win_);
}

AdvancedShortcut ShortcutBuilder::buildAdvanced() {
    validateConfiguration();
    AdvancedShortcut result = AdvancedShortcut::createKeyboard(
        Shortcut(vkCode_, ctrl_, alt_, shift_, win_));
    result.description = description_;
    result.category = category_;
    return result;
}

ShortcutBuilder& ShortcutBuilder::reset() {
    vkCode_ = 0;
    ctrl_ = alt_ = shift_ = win_ = false;
    description_.clear();
    category_.clear();
    tags_.clear();
    requireModifier_ = false;
    allowSystemReserved_ = false;
    validateConflicts_ = true;
    return *this;
}

bool ShortcutBuilder::isValid() const {
    try {
        validateConfiguration();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<std::string> ShortcutBuilder::getValidationErrors() const {
    std::vector<std::string> errors;

    if (vkCode_ == 0) {
        errors.push_back("No key specified");
    }

    if (requireModifier_ && !ctrl_ && !alt_ && !shift_ && !win_) {
        errors.push_back("At least one modifier key is required");
    }

    // Add more validation rules as needed

    return errors;
}

void ShortcutBuilder::validateConfiguration() const {
    auto errors = getValidationErrors();
    if (!errors.empty()) {
        std::stringstream ss;
        ss << "Shortcut validation failed: ";
        for (size_t i = 0; i < errors.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << errors[i];
        }
        throw std::invalid_argument(ss.str());
    }
}

// Enhanced ShortcutFactory implementation
ShortcutBuilder ShortcutFactory::builder() {
    return ShortcutBuilder();
}

void ShortcutFactory::registerTemplate(const ShortcutTemplate& tmpl) {
    templates_[tmpl.name] = tmpl;
    spdlog::debug("Registered shortcut template: {}", tmpl.name);
}

Shortcut ShortcutFactory::fromTemplate(const std::string& templateName) {
    auto it = templates_.find(templateName);
    if (it != templates_.end()) {
        return it->second.factory();
    }
    throw std::invalid_argument("Template not found: " + templateName);
}

std::vector<ShortcutTemplate> ShortcutFactory::getTemplates() {
    std::vector<ShortcutTemplate> result;
    result.reserve(templates_.size());

    for (const auto& [name, tmpl] : templates_) {
        result.push_back(tmpl);
    }

    return result;
}

std::vector<ShortcutTemplate> ShortcutFactory::getTemplatesByCategory(const std::string& category) {
    std::vector<ShortcutTemplate> result;

    for (const auto& [name, tmpl] : templates_) {
        if (tmpl.category == category) {
            result.push_back(tmpl);
        }
    }

    return result;
}

void ShortcutFactory::registerPreset(const ShortcutPreset& preset) {
    presets_[preset.name] = preset;
    spdlog::debug("Registered shortcut preset: {}", preset.name);
}

ShortcutPreset ShortcutFactory::getPreset(const std::string& presetName) {
    auto it = presets_.find(presetName);
    if (it != presets_.end()) {
        return it->second;
    }
    throw std::invalid_argument("Preset not found: " + presetName);
}

std::vector<ShortcutPreset> ShortcutFactory::getPresets() {
    std::vector<ShortcutPreset> result;
    result.reserve(presets_.size());

    for (const auto& [name, preset] : presets_) {
        result.push_back(preset);
    }

    return result;
}

void ShortcutFactory::initializeDefaults() {
    if (defaultsInitialized_) return;

    // Register common templates
    registerTemplate(ShortcutTemplate("copy", "Copy to clipboard", "Edit",
        []() { return ctrlC(); }));
    registerTemplate(ShortcutTemplate("paste", "Paste from clipboard", "Edit",
        []() { return ctrlV(); }));
    registerTemplate(ShortcutTemplate("cut", "Cut to clipboard", "Edit",
        []() { return ctrlX(); }));
    registerTemplate(ShortcutTemplate("undo", "Undo last action", "Edit",
        []() { return ctrlZ(); }));
    registerTemplate(ShortcutTemplate("redo", "Redo last action", "Edit",
        []() { return ctrlY(); }));
    registerTemplate(ShortcutTemplate("save", "Save document", "File",
        []() { return ctrlS(); }));
    registerTemplate(ShortcutTemplate("open", "Open document", "File",
        []() { return ctrlO(); }));
    registerTemplate(ShortcutTemplate("new", "New document", "File",
        []() { return ctrlN(); }));

    // Register common presets
    ShortcutPreset editingPreset("editing", "Common editing shortcuts");
    editingPreset.addShortcut("copy", ctrlC());
    editingPreset.addShortcut("paste", ctrlV());
    editingPreset.addShortcut("cut", ctrlX());
    editingPreset.addShortcut("undo", ctrlZ());
    editingPreset.addShortcut("redo", ctrlY());
    editingPreset.addShortcut("select_all", ctrlA());
    registerPreset(editingPreset);

    ShortcutPreset filePreset("file", "File management shortcuts");
    filePreset.addShortcut("new", ctrlN());
    filePreset.addShortcut("open", ctrlO());
    filePreset.addShortcut("save", ctrlS());
    filePreset.addShortcut("print", ctrlP());
    filePreset.addShortcut("close", ctrlW());
    registerPreset(filePreset);

    defaultsInitialized_ = true;
    spdlog::debug("Initialized default shortcut templates and presets");
}

void ShortcutFactory::clear() {
    templates_.clear();
    presets_.clear();
    defaultsInitialized_ = false;
    spdlog::debug("Cleared all shortcut templates and presets");
}

}  // namespace shortcut_detector
