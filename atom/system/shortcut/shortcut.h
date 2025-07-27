#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <optional>

namespace shortcut_detector {

/**
 * @brief Represents a keyboard shortcut with optimized performance
 */
class Shortcut {
public:
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
             bool withShift = false, bool withWin = false) noexcept;

    /**
     * @brief Copy constructor
     */
    Shortcut(const Shortcut& other) noexcept;

    /**
     * @brief Move constructor
     */
    Shortcut(Shortcut&& other) noexcept;

    /**
     * @brief Copy assignment operator
     */
    Shortcut& operator=(const Shortcut& other) noexcept;

    /**
     * @brief Move assignment operator
     */
    Shortcut& operator=(Shortcut&& other) noexcept;

    /**
     * @brief Convert to human-readable string with caching
     *
     * @return const std::string& Cached string representation (e.g. "Ctrl+Alt+F1")
     */
    const std::string& toString() const;

    /**
     * @brief Calculate hash for use in containers (optimized)
     *
     * @return size_t Hash value
     */
    size_t hash() const noexcept;

    /**
     * @brief Equality comparison operator
     *
     * @param other Other shortcut to compare
     * @return bool True if shortcuts are equal
     */
    bool operator==(const Shortcut& other) const noexcept;

    /**
     * @brief Inequality comparison operator
     *
     * @param other Other shortcut to compare
     * @return bool True if shortcuts are not equal
     */
    bool operator!=(const Shortcut& other) const noexcept;

    /**
     * @brief Less than comparison operator for ordered containers
     *
     * @param other Other shortcut to compare
     * @return bool True if this shortcut is less than other
     */
    bool operator<(const Shortcut& other) const noexcept;

    /**
     * @brief Check if shortcut has any modifier keys
     *
     * @return bool True if any modifier key is pressed
     */
    bool hasModifiers() const noexcept;

    /**
     * @brief Get modifier mask as a single value for fast comparison
     *
     * @return uint8_t Packed modifier flags
     */
    uint8_t getModifierMask() const noexcept;

    /**
     * @brief Check if shortcut is valid (has a valid virtual key code)
     *
     * @return bool True if shortcut is valid
     */
    bool isValid() const noexcept;

    /**
     * @brief Clear cached string representation (for internal use)
     */
    void clearCache() const noexcept;

private:
    mutable std::optional<std::string> cachedString_;  // Cached string representation
    mutable size_t cachedHash_ = 0;                    // Cached hash value
    mutable bool hashCalculated_ = false;              // Hash calculation flag

    /**
     * @brief Generate string representation
     *
     * @return std::string Generated string
     */
    std::string generateString() const;

    /**
     * @brief Calculate hash value
     *
     * @return size_t Calculated hash
     */
    size_t calculateHash() const noexcept;
};

}  // namespace shortcut_detector

// Hash specialization for std::unordered_map
namespace std {
template <>
struct hash<shortcut_detector::Shortcut> {
    size_t operator()(const shortcut_detector::Shortcut& shortcut) const noexcept {
        return shortcut.hash();
    }
};
}  // namespace std
