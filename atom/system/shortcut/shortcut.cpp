#include "shortcut.h"
#include <windows.h>
#include <functional>
#include <sstream>
#include <unordered_map>

namespace shortcut_detector {

// Static lookup table for special keys (performance optimization)
static const std::unordered_map<uint32_t, const char*> specialKeys = {
    {VK_TAB, "Tab"}, {VK_SPACE, "Space"}, {VK_RETURN, "Enter"},
    {VK_ESCAPE, "Esc"}, {VK_DELETE, "Delete"}, {VK_BACK, "Backspace"},
    {VK_LEFT, "Left"}, {VK_RIGHT, "Right"}, {VK_UP, "Up"}, {VK_DOWN, "Down"},
    {VK_HOME, "Home"}, {VK_END, "End"}, {VK_PRIOR, "PageUp"}, {VK_NEXT, "PageDown"},
    {VK_INSERT, "Insert"}, {VK_PAUSE, "Pause"}, {VK_PRINT, "PrintScreen"}
};

Shortcut::Shortcut(uint32_t key, bool withCtrl, bool withAlt, bool withShift,
                   bool withWin) noexcept
    : vkCode(key),
      ctrl(withCtrl),
      alt(withAlt),
      shift(withShift),
      win(withWin) {}

Shortcut::Shortcut(const Shortcut& other) noexcept
    : vkCode(other.vkCode),
      ctrl(other.ctrl),
      alt(other.alt),
      shift(other.shift),
      win(other.win) {}

Shortcut::Shortcut(Shortcut&& other) noexcept
    : vkCode(other.vkCode),
      ctrl(other.ctrl),
      alt(other.alt),
      shift(other.shift),
      win(other.win),
      cachedString_(std::move(other.cachedString_)),
      cachedHash_(other.cachedHash_),
      hashCalculated_(other.hashCalculated_) {
    other.clearCache();
}

Shortcut& Shortcut::operator=(const Shortcut& other) noexcept {
    if (this != &other) {
        vkCode = other.vkCode;
        ctrl = other.ctrl;
        alt = other.alt;
        shift = other.shift;
        win = other.win;
        clearCache();
    }
    return *this;
}

Shortcut& Shortcut::operator=(Shortcut&& other) noexcept {
    if (this != &other) {
        vkCode = other.vkCode;
        ctrl = other.ctrl;
        alt = other.alt;
        shift = other.shift;
        win = other.win;
        cachedString_ = std::move(other.cachedString_);
        cachedHash_ = other.cachedHash_;
        hashCalculated_ = other.hashCalculated_;
        other.clearCache();
    }
    return *this;
}

const std::string& Shortcut::toString() const {
    if (!cachedString_.has_value()) {
        cachedString_ = generateString();
    }
    return cachedString_.value();
}

std::string Shortcut::generateString() const {
    std::stringstream ss;

    if (win)
        ss << "Win+";
    if (ctrl)
        ss << "Ctrl+";
    if (alt)
        ss << "Alt+";
    if (shift)
        ss << "Shift+";

    // Handle function keys efficiently
    if (vkCode >= VK_F1 && vkCode <= VK_F24) {
        ss << "F" << (vkCode - VK_F1 + 1);
    } else {
        // Check special keys first (fast lookup)
        auto it = specialKeys.find(vkCode);
        if (it != specialKeys.end()) {
            ss << it->second;
        } else if ((vkCode >= '0' && vkCode <= '9') ||
                   (vkCode >= 'A' && vkCode <= 'Z')) {
            ss << static_cast<char>(vkCode);
        } else {
            // Fallback for other keys
            BYTE keyboardState[256] = {0};
            if (ctrl)
                keyboardState[VK_CONTROL] = 0x80;
            if (alt)
                keyboardState[VK_MENU] = 0x80;
            if (shift)
                keyboardState[VK_SHIFT] = 0x80;

            WORD result = 0;
            if (ToAscii(vkCode, 0, keyboardState, &result, 0) == 1) {
                ss << static_cast<char>(result & 0xFF);
            } else {
                ss << "0x" << std::hex << vkCode;
            }
        }
    }

    return ss.str();
}

size_t Shortcut::hash() const noexcept {
    if (!hashCalculated_) {
        cachedHash_ = calculateHash();
        hashCalculated_ = true;
    }
    return cachedHash_;
}

size_t Shortcut::calculateHash() const noexcept {
    // Optimized hash function using FNV-1a algorithm
    constexpr size_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    constexpr size_t FNV_PRIME = 1099511628211ULL;

    size_t hash = FNV_OFFSET_BASIS;

    // Hash vkCode
    hash ^= vkCode;
    hash *= FNV_PRIME;

    // Hash modifier flags as a single byte for efficiency
    uint8_t modifiers = getModifierMask();
    hash ^= modifiers;
    hash *= FNV_PRIME;

    return hash;
}

bool Shortcut::operator==(const Shortcut& other) const noexcept {
    return vkCode == other.vkCode &&
           getModifierMask() == other.getModifierMask();
}

bool Shortcut::operator!=(const Shortcut& other) const noexcept {
    return !(*this == other);
}

bool Shortcut::operator<(const Shortcut& other) const noexcept {
    if (vkCode != other.vkCode) {
        return vkCode < other.vkCode;
    }
    return getModifierMask() < other.getModifierMask();
}

bool Shortcut::hasModifiers() const noexcept {
    return ctrl || alt || shift || win;
}

uint8_t Shortcut::getModifierMask() const noexcept {
    return (ctrl ? 1 : 0) |
           (alt ? 2 : 0) |
           (shift ? 4 : 0) |
           (win ? 8 : 0);
}

bool Shortcut::isValid() const noexcept {
    return vkCode != 0 && vkCode <= 0xFF;
}

void Shortcut::clearCache() const noexcept {
    cachedString_.reset();
    hashCalculated_ = false;
    cachedHash_ = 0;
}

}  // namespace shortcut_detector
