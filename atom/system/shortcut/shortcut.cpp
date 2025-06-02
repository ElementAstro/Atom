#include "shortcut.h"
#include <windows.h>
#include <functional>
#include <sstream>

namespace shortcut_detector {

Shortcut::Shortcut(uint32_t key, bool withCtrl, bool withAlt, bool withShift,
                   bool withWin)
    : vkCode(key),
      ctrl(withCtrl),
      alt(withAlt),
      shift(withShift),
      win(withWin) {}

std::string Shortcut::toString() const {
    std::stringstream ss;

    if (win)
        ss << "Win+";
    if (ctrl)
        ss << "Ctrl+";
    if (alt)
        ss << "Alt+";
    if (shift)
        ss << "Shift+";

    // Handle special keys
    if (vkCode >= VK_F1 && vkCode <= VK_F24) {
        ss << "F" << (vkCode - VK_F1 + 1);
    } else if (vkCode == VK_TAB) {
        ss << "Tab";
    } else if (vkCode == VK_RETURN) {
        ss << "Enter";
    } else if (vkCode == VK_ESCAPE) {
        ss << "Esc";
    } else if (vkCode == VK_SPACE) {
        ss << "Space";
    } else if (vkCode == VK_DELETE) {
        ss << "Delete";
    } else if (vkCode == VK_BACK) {
        ss << "Backspace";
    } else if (vkCode == VK_HOME) {
        ss << "Home";
    } else if (vkCode == VK_END) {
        ss << "End";
    } else if (vkCode == VK_INSERT) {
        ss << "Insert";
    } else if (vkCode == VK_NEXT) {
        ss << "PageDown";
    } else if (vkCode == VK_PRIOR) {
        ss << "PageUp";
    } else if (vkCode == VK_LEFT) {
        ss << "Left";
    } else if (vkCode == VK_RIGHT) {
        ss << "Right";
    } else if (vkCode == VK_UP) {
        ss << "Up";
    } else if (vkCode == VK_DOWN) {
        ss << "Down";
    } else {
        // For letters and numbers
        char keyChar[2] = {0, 0};

        // Get the character representation
        if ((vkCode >= '0' && vkCode <= '9') ||
            (vkCode >= 'A' && vkCode <= 'Z')) {
            keyChar[0] = static_cast<char>(vkCode);
        } else {
            // Try to convert virtual key code to character
            BYTE keyboardState[256] = {0};
            if (ctrl)
                keyboardState[VK_CONTROL] = 0x80;
            if (alt)
                keyboardState[VK_MENU] = 0x80;
            if (shift)
                keyboardState[VK_SHIFT] = 0x80;

            WORD result = 0;
            if (ToAscii(vkCode, 0, keyboardState, &result, 0) == 1) {
                keyChar[0] = static_cast<char>(result & 0xFF);
            } else {
                // Fall back to hexadecimal for unknown keys
                ss << "0x" << std::hex << vkCode;
                return ss.str();
            }
        }

        ss << keyChar;
    }

    return ss.str();
}

size_t Shortcut::hash() const {
    // Combine hash values for all fields
    std::size_t h = 0;

    // Incorporate key code
    h ^= std::hash<uint32_t>{}(vkCode) + 0x9e3779b9 + (h << 6) + (h >> 2);

    // Incorporate modifier keys
    h ^= std::hash<bool>{}(ctrl) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>{}(alt) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>{}(shift) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>{}(win) + 0x9e3779b9 + (h << 6) + (h >> 2);

    return h;
}

bool Shortcut::operator==(const Shortcut& other) const {
    return vkCode == other.vkCode && ctrl == other.ctrl && alt == other.alt &&
           shift == other.shift && win == other.win;
}

}  // namespace shortcut_detector