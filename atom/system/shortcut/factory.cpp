#include "factory.h"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>

namespace shortcut_detector {

// Map of key names to VK codes
static const std::map<std::string, uint32_t> keyNameToVK = {
    {"TAB", VK_TAB},       {"ENTER", VK_RETURN},  {"RETURN", VK_RETURN},
    {"ESC", VK_ESCAPE},    {"ESCAPE", VK_ESCAPE}, {"SPACE", VK_SPACE},
    {"PGUP", VK_PRIOR},    {"PAGEUP", VK_PRIOR},  {"PGDN", VK_NEXT},
    {"PAGEDOWN", VK_NEXT}, {"END", VK_END},       {"HOME", VK_HOME},
    {"LEFT", VK_LEFT},     {"UP", VK_UP},         {"RIGHT", VK_RIGHT},
    {"DOWN", VK_DOWN},     {"INS", VK_INSERT},    {"INSERT", VK_INSERT},
    {"DEL", VK_DELETE},    {"DELETE", VK_DELETE}, {"F1", VK_F1},
    {"F2", VK_F2},         {"F3", VK_F3},         {"F4", VK_F4},
    {"F5", VK_F5},         {"F6", VK_F6},         {"F7", VK_F7},
    {"F8", VK_F8},         {"F9", VK_F9},         {"F10", VK_F10},
    {"F11", VK_F11},       {"F12", VK_F12},
};

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

}  // namespace shortcut_detector