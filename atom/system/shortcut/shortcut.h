#pragma once

#include <cstdint>
#include <string>

namespace shortcut_detector {

/**
 * @brief Represents a keyboard shortcut
 */
struct Shortcut {
    uint32_t vkCode;  // Virtual key code
    bool ctrl;        // Control key required
    bool alt;         // Alt key required
    bool shift;       // Shift key required
    bool win;         // Windows key required

    /**
     * @brief Construct a new Shortcut
     *
     * @param key Virtual key code
     * @param withCtrl Control key flag
     * @param withAlt Alt key flag
     * @param withShift Shift key flag
     * @param withWin Windows key flag
     */
    Shortcut(uint32_t key, bool withCtrl = false, bool withAlt = false,
             bool withShift = false, bool withWin = false);

    /**
     * @brief Convert to human-readable string
     *
     * @return std::string String representation (e.g. "Ctrl+Alt+F1")
     */
    std::string toString() const;

    /**
     * @brief Get hash value for the shortcut
     *
     * @return size_t Hash value
     */
    size_t hash() const;

    /**
     * @brief Equality operator
     *
     * @param other Other shortcut to compare with
     * @return bool True if shortcuts are equal
     */
    bool operator==(const Shortcut& other) const;
};

}  // namespace shortcut_detector

// Add hash support for Shortcut to use in unordered containers
namespace std {
template <>
struct hash<shortcut_detector::Shortcut> {
    size_t operator()(const shortcut_detector::Shortcut& s) const {
        return s.hash();
    }
};
}  // namespace std