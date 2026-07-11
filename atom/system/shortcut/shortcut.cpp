#include "shortcut.h"
#include <windows.h>
#include <functional>
#include <sstream>
#include <utility>

namespace shortcut_detector {

Shortcut::Shortcut(uint32_t key, bool withCtrl, bool withAlt, bool withShift,
                   bool withWin)
    : vkCode(key),
      ctrl(withCtrl),
      alt(withAlt),
      shift(withShift),
      win(withWin) {}

const std::string& Shortcut::toString() const {
    if (!cachedString_) {
        cachedString_ = generateString();
    }
    return *cachedString_;
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
        if ((vkCode >= '0' && vkCode <= '9') ||
            (vkCode >= 'A' && vkCode <= 'Z')) {
            ss << static_cast<char>(vkCode);
        } else {
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

size_t Shortcut::hash() const {
    std::size_t h = 0;
    h ^= std::hash<uint32_t>{}(vkCode) + 0x9e3779b9 + (h << 6) + (h >> 2);
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

// The cached string/hash are pure memoization, so a copy starts with an empty
// cache (regenerated lazily). This keeps the copy operations noexcept and
// allocation-free.
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
      hashCalculated_(other.hashCalculated_) {}

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
    }
    return *this;
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
    return static_cast<uint8_t>((ctrl ? 0x01 : 0) | (alt ? 0x02 : 0) |
                                (shift ? 0x04 : 0) | (win ? 0x08 : 0));
}

bool Shortcut::isValid() const noexcept { return vkCode != 0; }

void Shortcut::clearCache() const noexcept {
    cachedString_.reset();
    cachedHash_ = 0;
    hashCalculated_ = false;
}

}  // namespace shortcut_detector
