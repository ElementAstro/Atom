#pragma once

#include <string>
#include "shortcut.h"

namespace shortcut_detector {

/**
 * @brief Factory for creating common shortcuts
 */
class ShortcutFactory {
public:
    /**
     * @brief Create a shortcut from a letter or key
     *
     * @param letter The letter or key code
     * @param ctrl Control key flag
     * @param alt Alt key flag
     * @param shift Shift key flag
     * @param win Windows key flag
     * @return Shortcut The created shortcut
     */
    static Shortcut create(char letter, bool ctrl = false, bool alt = false,
                           bool shift = false, bool win = false);

    /**
     * @brief Create a shortcut from a virtual key code
     *
     * @param vkCode Virtual key code
     * @param ctrl Control key flag
     * @param alt Alt key flag
     * @param shift Shift key flag
     * @param win Windows key flag
     * @return Shortcut The created shortcut
     */
    static Shortcut createVK(uint32_t vkCode, bool ctrl = false,
                             bool alt = false, bool shift = false,
                             bool win = false);

    /**
     * @brief Parse a shortcut from string
     *
     * @param description String like "Ctrl+Alt+F1"
     * @return Shortcut The created shortcut
     * @throws std::invalid_argument If string format is incorrect
     */
    static Shortcut fromString(const std::string& description);

    // Common shortcuts
    static Shortcut ctrlC() { return create('C', true); }
    static Shortcut ctrlV() { return create('V', true); }
    static Shortcut ctrlX() { return create('X', true); }
    static Shortcut ctrlZ() { return create('Z', true); }
    static Shortcut ctrlY() { return create('Y', true); }
    static Shortcut ctrlS() { return create('S', true); }
    static Shortcut altTab() {
        return createVK(0x09, false, true);
    }  // VK_TAB = 0x09
    static Shortcut altF4() {
        return createVK(0x73, false, true);
    }  // VK_F4 = 0x73
};

}  // namespace shortcut_detector
