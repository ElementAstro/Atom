#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include <unordered_map>
#include "shortcut.h"

namespace shortcut_detector {

/**
 * @brief Extended shortcut types
 */
enum class ShortcutType {
    Keyboard,           // Standard keyboard shortcut
    Mouse,              // Mouse button combination
    Multimedia,         // Multimedia keys (volume, play, etc.)
    Sequential,         // Sequential key combination (like Vim commands)
    Gesture,            // Mouse gesture
    Combination         // Complex combination of above
};

/**
 * @brief Mouse button identifiers
 */
enum class MouseButton {
    Left = 1,
    Right = 2,
    Middle = 4,
    X1 = 8,
    X2 = 16,
    WheelUp = 32,
    WheelDown = 64
};

/**
 * @brief Multimedia key identifiers
 */
enum class MultimediaKey {
    VolumeUp,
    VolumeDown,
    VolumeMute,
    MediaPlayPause,
    MediaStop,
    MediaNext,
    MediaPrevious,
    BrowserBack,
    BrowserForward,
    BrowserRefresh,
    BrowserHome,
    LaunchMail,
    LaunchCalculator,
    LaunchMediaPlayer
};

/**
 * @brief Shortcut binding representation
 */
class ShortcutBinding {
public:
    ShortcutType type;
    std::vector<Shortcut> keySequence;          // For sequential shortcuts
    std::vector<MouseButton> mouseButtons;      // For mouse shortcuts
    std::vector<MultimediaKey> multimediaKeys;  // For multimedia shortcuts
    std::chrono::milliseconds maxSequenceTime{2000}; // Max time for sequence
    std::string description;
    std::string category;

    ShortcutBinding(ShortcutType t = ShortcutType::Keyboard);

    /**
     * @brief Create keyboard shortcut
     */
    static ShortcutBinding createKeyboard(const Shortcut& shortcut);

    /**
     * @brief Create mouse shortcut
     */
    static ShortcutBinding createMouse(const std::vector<MouseButton>& buttons,
                                       const Shortcut& modifiers = Shortcut(0));

    /**
     * @brief Create multimedia shortcut
     */
    static ShortcutBinding createMultimedia(MultimediaKey key);

    /**
     * @brief Create sequential shortcut
     */
    static ShortcutBinding createSequential(const std::vector<Shortcut>& sequence,
                                            std::chrono::milliseconds maxTime = std::chrono::milliseconds(2000));

    /**
     * @brief Convert to string representation
     */
    std::string toString() const;

    /**
     * @brief Check if shortcut is valid
     */
    bool isValid() const;

    /**
     * @brief Get hash for container usage
     */
    size_t hash() const;

    /**
     * @brief Equality operator
     */
    bool operator==(const ShortcutBinding& other) const;
};

}  // namespace shortcut_detector

// Hash specialization for ShortcutBinding. Must precede any
// std::unordered_map<ShortcutBinding, ...> instantiation (e.g. in
// ShortcutBindingManager below), otherwise the disabled primary std::hash is
// selected ("hash function must be copy constructible").
namespace std {
template <>
struct hash<shortcut_detector::ShortcutBinding> {
    size_t operator()(
        const shortcut_detector::ShortcutBinding& shortcut) const {
        return shortcut.hash();
    }
};
}  // namespace std

namespace shortcut_detector {

/**
 * @brief Shortcut conflict information
 */
struct ShortcutConflict {
    ShortcutBinding shortcut1;
    ShortcutBinding shortcut2;
    std::string conflictReason;
    enum class Severity { Low, Medium, High, Critical } severity;

    ShortcutConflict(const ShortcutBinding& s1, const ShortcutBinding& s2,
                    const std::string& reason, Severity sev = Severity::Medium)
        : shortcut1(s1), shortcut2(s2), conflictReason(reason), severity(sev) {}
};

/**
 * @brief Custom key mapping for remapping shortcuts
 */
struct KeyMapping {
    ShortcutBinding from;
    ShortcutBinding to;
    std::string application;  // Empty for global mapping
    bool enabled{true};

    KeyMapping(const ShortcutBinding& fromShortcut, const ShortcutBinding& toShortcut,
              const std::string& app = "")
        : from(fromShortcut), to(toShortcut), application(app) {}
};

/**
 * @brief Shortcut binding manager with conflict detection and resolution
 */
class ShortcutBindingManager {
public:
    ShortcutBindingManager();
    ~ShortcutBindingManager();

    /**
     * @brief Register a shortcut
     */
    bool registerShortcut(const ShortcutBinding& shortcut, const std::string& owner = "");

    /**
     * @brief Unregister a shortcut
     */
    bool unregisterShortcut(const ShortcutBinding& shortcut);

    /**
     * @brief Check for conflicts with existing shortcuts
     */
    std::vector<ShortcutConflict> checkConflicts(const ShortcutBinding& shortcut) const;

    /**
     * @brief Get all registered shortcuts
     */
    std::vector<ShortcutBinding> getAllShortcuts() const;

    /**
     * @brief Get shortcuts by category
     */
    std::vector<ShortcutBinding> getShortcutsByCategory(const std::string& category) const;

    /**
     * @brief Add custom key mapping
     */
    void addKeyMapping(const KeyMapping& mapping);

    /**
     * @brief Remove key mapping
     */
    void removeKeyMapping(const ShortcutBinding& from);

    /**
     * @brief Get all key mappings
     */
    std::vector<KeyMapping> getKeyMappings() const;

    /**
     * @brief Resolve shortcut through mappings
     */
    ShortcutBinding resolveShortcut(const ShortcutBinding& shortcut,
                                   const std::string& application = "") const;

    /**
     * @brief Auto-resolve conflicts by suggesting alternatives
     */
    std::vector<ShortcutBinding> suggestAlternatives(const ShortcutBinding& shortcut) const;

    /**
     * @brief Export shortcuts to JSON
     */
    std::string exportToJson() const;

    /**
     * @brief Import shortcuts from JSON
     */
    bool importFromJson(const std::string& json);

    /**
     * @brief Clear all shortcuts and mappings
     */
    void clear();

private:
    std::unordered_map<ShortcutBinding, std::string> registeredShortcuts_;
    std::vector<KeyMapping> keyMappings_;

    bool hasConflict(const ShortcutBinding& s1, const ShortcutBinding& s2) const;
    std::string getConflictReason(const ShortcutBinding& s1, const ShortcutBinding& s2) const;
    ShortcutConflict::Severity assessConflictSeverity(const ShortcutBinding& s1,
                                                      const ShortcutBinding& s2) const;
};

/**
 * @brief Utility functions for multimedia keys
 */
namespace multimedia_utils {
    /**
     * @brief Get virtual key code for multimedia key
     */
    uint32_t getVirtualKeyCode(MultimediaKey key);

    /**
     * @brief Get multimedia key from virtual key code
     */
    MultimediaKey getMultimediaKey(uint32_t vkCode);

    /**
     * @brief Check if virtual key is a multimedia key
     */
    bool isMultimediaKey(uint32_t vkCode);

    /**
     * @brief Get human-readable name for multimedia key
     */
    std::string getKeyName(MultimediaKey key);
}

/**
 * @brief Utility functions for mouse shortcuts
 */
namespace mouse_utils {
    /**
     * @brief Convert mouse button to string
     */
    std::string mouseButtonToString(MouseButton button);

    /**
     * @brief Parse mouse button from string
     */
    MouseButton stringToMouseButton(const std::string& str);

    /**
     * @brief Check if mouse button combination is valid
     */
    bool isValidMouseCombination(const std::vector<MouseButton>& buttons);
}

}  // namespace shortcut_detector
