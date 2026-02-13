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
 * @brief Advanced shortcut representation
 */
class AdvancedShortcut {
public:
    ShortcutType type;
    std::vector<Shortcut> keySequence;          // For sequential shortcuts
    std::vector<MouseButton> mouseButtons;      // For mouse shortcuts
    std::vector<MultimediaKey> multimediaKeys;  // For multimedia shortcuts
    std::chrono::milliseconds maxSequenceTime{2000}; // Max time for sequence
    std::string description;
    std::string category;

    AdvancedShortcut(ShortcutType t = ShortcutType::Keyboard);

    /**
     * @brief Create keyboard shortcut
     */
    static AdvancedShortcut createKeyboard(const Shortcut& shortcut);

    /**
     * @brief Create mouse shortcut
     */
    static AdvancedShortcut createMouse(const std::vector<MouseButton>& buttons,
                                       const Shortcut& modifiers = Shortcut(0));

    /**
     * @brief Create multimedia shortcut
     */
    static AdvancedShortcut createMultimedia(MultimediaKey key);

    /**
     * @brief Create sequential shortcut
     */
    static AdvancedShortcut createSequential(const std::vector<Shortcut>& sequence,
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
    bool operator==(const AdvancedShortcut& other) const;
};

/**
 * @brief Shortcut conflict information
 */
struct ShortcutConflict {
    AdvancedShortcut shortcut1;
    AdvancedShortcut shortcut2;
    std::string conflictReason;
    enum class Severity { Low, Medium, High, Critical } severity;

    ShortcutConflict(const AdvancedShortcut& s1, const AdvancedShortcut& s2,
                    const std::string& reason, Severity sev = Severity::Medium)
        : shortcut1(s1), shortcut2(s2), conflictReason(reason), severity(sev) {}
};

/**
 * @brief Custom key mapping for remapping shortcuts
 */
struct KeyMapping {
    AdvancedShortcut from;
    AdvancedShortcut to;
    std::string application;  // Empty for global mapping
    bool enabled{true};

    KeyMapping(const AdvancedShortcut& fromShortcut, const AdvancedShortcut& toShortcut,
              const std::string& app = "")
        : from(fromShortcut), to(toShortcut), application(app) {}
};

/**
 * @brief Advanced shortcut manager with conflict detection and resolution
 */
class AdvancedShortcutManager {
public:
    AdvancedShortcutManager();
    ~AdvancedShortcutManager();

    /**
     * @brief Register a shortcut
     */
    bool registerShortcut(const AdvancedShortcut& shortcut, const std::string& owner = "");

    /**
     * @brief Unregister a shortcut
     */
    bool unregisterShortcut(const AdvancedShortcut& shortcut);

    /**
     * @brief Check for conflicts with existing shortcuts
     */
    std::vector<ShortcutConflict> checkConflicts(const AdvancedShortcut& shortcut) const;

    /**
     * @brief Get all registered shortcuts
     */
    std::vector<AdvancedShortcut> getAllShortcuts() const;

    /**
     * @brief Get shortcuts by category
     */
    std::vector<AdvancedShortcut> getShortcutsByCategory(const std::string& category) const;

    /**
     * @brief Add custom key mapping
     */
    void addKeyMapping(const KeyMapping& mapping);

    /**
     * @brief Remove key mapping
     */
    void removeKeyMapping(const AdvancedShortcut& from);

    /**
     * @brief Get all key mappings
     */
    std::vector<KeyMapping> getKeyMappings() const;

    /**
     * @brief Resolve shortcut through mappings
     */
    AdvancedShortcut resolveShortcut(const AdvancedShortcut& shortcut,
                                   const std::string& application = "") const;

    /**
     * @brief Auto-resolve conflicts by suggesting alternatives
     */
    std::vector<AdvancedShortcut> suggestAlternatives(const AdvancedShortcut& shortcut) const;

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
    std::unordered_map<AdvancedShortcut, std::string> registeredShortcuts_;
    std::vector<KeyMapping> keyMappings_;

    bool hasConflict(const AdvancedShortcut& s1, const AdvancedShortcut& s2) const;
    std::string getConflictReason(const AdvancedShortcut& s1, const AdvancedShortcut& s2) const;
    ShortcutConflict::Severity assessConflictSeverity(const AdvancedShortcut& s1,
                                                      const AdvancedShortcut& s2) const;
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

// Hash specialization for AdvancedShortcut
namespace std {
template <>
struct hash<shortcut_detector::AdvancedShortcut> {
    size_t operator()(const shortcut_detector::AdvancedShortcut& shortcut) const {
        return shortcut.hash();
    }
};
}  // namespace std
