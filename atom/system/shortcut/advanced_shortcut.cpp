#include "advanced_shortcut.h"
#include <sstream>
#include <algorithm>
#include <unordered_set>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace shortcut_detector {

// AdvancedShortcut implementation
AdvancedShortcut::AdvancedShortcut(ShortcutType t) : type(t) {}

AdvancedShortcut AdvancedShortcut::createKeyboard(const Shortcut& shortcut) {
    AdvancedShortcut result(ShortcutType::Keyboard);
    result.keySequence.push_back(shortcut);
    return result;
}

AdvancedShortcut AdvancedShortcut::createMouse(const std::vector<MouseButton>& buttons,
                                              const Shortcut& modifiers) {
    AdvancedShortcut result(ShortcutType::Mouse);
    result.mouseButtons = buttons;
    if (modifiers.vkCode != 0 || modifiers.hasModifiers()) {
        result.keySequence.push_back(modifiers);
    }
    return result;
}

AdvancedShortcut AdvancedShortcut::createMultimedia(MultimediaKey key) {
    AdvancedShortcut result(ShortcutType::Multimedia);
    result.multimediaKeys.push_back(key);
    return result;
}

AdvancedShortcut AdvancedShortcut::createSequential(const std::vector<Shortcut>& sequence,
                                                   std::chrono::milliseconds maxTime) {
    AdvancedShortcut result(ShortcutType::Sequential);
    result.keySequence = sequence;
    result.maxSequenceTime = maxTime;
    return result;
}

std::string AdvancedShortcut::toString() const {
    std::stringstream ss;
    
    switch (type) {
        case ShortcutType::Keyboard:
            if (!keySequence.empty()) {
                ss << keySequence[0].toString();
            }
            break;
            
        case ShortcutType::Mouse:
            if (!keySequence.empty()) {
                ss << keySequence[0].toString() << "+";
            }
            for (size_t i = 0; i < mouseButtons.size(); ++i) {
                if (i > 0) ss << "+";
                ss << mouse_utils::mouseButtonToString(mouseButtons[i]);
            }
            break;
            
        case ShortcutType::Multimedia:
            if (!multimediaKeys.empty()) {
                ss << multimedia_utils::getKeyName(multimediaKeys[0]);
            }
            break;
            
        case ShortcutType::Sequential:
            for (size_t i = 0; i < keySequence.size(); ++i) {
                if (i > 0) ss << " → ";
                ss << keySequence[i].toString();
            }
            ss << " (max " << maxSequenceTime.count() << "ms)";
            break;
            
        case ShortcutType::Gesture:
            ss << "Gesture";
            break;
            
        case ShortcutType::Combination:
            ss << "Complex Combination";
            break;
    }
    
    if (!description.empty()) {
        ss << " (" << description << ")";
    }
    
    return ss.str();
}

bool AdvancedShortcut::isValid() const {
    switch (type) {
        case ShortcutType::Keyboard:
            return !keySequence.empty() && keySequence[0].isValid();
            
        case ShortcutType::Mouse:
            return !mouseButtons.empty() && 
                   mouse_utils::isValidMouseCombination(mouseButtons);
            
        case ShortcutType::Multimedia:
            return !multimediaKeys.empty();
            
        case ShortcutType::Sequential:
            return keySequence.size() >= 2 && 
                   std::all_of(keySequence.begin(), keySequence.end(),
                              [](const Shortcut& s) { return s.isValid(); });
            
        default:
            return false;
    }
}

size_t AdvancedShortcut::hash() const {
    size_t h = std::hash<int>{}(static_cast<int>(type));
    
    for (const auto& key : keySequence) {
        h ^= key.hash() + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    
    for (const auto& button : mouseButtons) {
        h ^= std::hash<int>{}(static_cast<int>(button)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    
    for (const auto& mmKey : multimediaKeys) {
        h ^= std::hash<int>{}(static_cast<int>(mmKey)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    
    return h;
}

bool AdvancedShortcut::operator==(const AdvancedShortcut& other) const {
    return type == other.type &&
           keySequence == other.keySequence &&
           mouseButtons == other.mouseButtons &&
           multimediaKeys == other.multimediaKeys &&
           maxSequenceTime == other.maxSequenceTime;
}

// AdvancedShortcutManager implementation
AdvancedShortcutManager::AdvancedShortcutManager() {
    spdlog::debug("AdvancedShortcutManager initialized");
}

AdvancedShortcutManager::~AdvancedShortcutManager() = default;

bool AdvancedShortcutManager::registerShortcut(const AdvancedShortcut& shortcut, const std::string& owner) {
    if (!shortcut.isValid()) {
        spdlog::warn("Attempted to register invalid shortcut: {}", shortcut.toString());
        return false;
    }
    
    auto conflicts = checkConflicts(shortcut);
    if (!conflicts.empty()) {
        spdlog::warn("Shortcut {} has {} conflicts", shortcut.toString(), conflicts.size());
        for (const auto& conflict : conflicts) {
            if (conflict.severity == ShortcutConflict::Severity::Critical) {
                spdlog::error("Critical conflict detected, registration failed");
                return false;
            }
        }
    }
    
    registeredShortcuts_[shortcut] = owner;
    spdlog::debug("Registered shortcut: {} (owner: {})", shortcut.toString(), owner);
    return true;
}

bool AdvancedShortcutManager::unregisterShortcut(const AdvancedShortcut& shortcut) {
    auto it = registeredShortcuts_.find(shortcut);
    if (it != registeredShortcuts_.end()) {
        registeredShortcuts_.erase(it);
        spdlog::debug("Unregistered shortcut: {}", shortcut.toString());
        return true;
    }
    return false;
}

std::vector<ShortcutConflict> AdvancedShortcutManager::checkConflicts(const AdvancedShortcut& shortcut) const {
    std::vector<ShortcutConflict> conflicts;
    
    for (const auto& [existing, owner] : registeredShortcuts_) {
        if (hasConflict(shortcut, existing)) {
            std::string reason = getConflictReason(shortcut, existing);
            auto severity = assessConflictSeverity(shortcut, existing);
            conflicts.emplace_back(shortcut, existing, reason, severity);
        }
    }
    
    return conflicts;
}

std::vector<AdvancedShortcut> AdvancedShortcutManager::getAllShortcuts() const {
    std::vector<AdvancedShortcut> result;
    result.reserve(registeredShortcuts_.size());
    
    for (const auto& [shortcut, owner] : registeredShortcuts_) {
        result.push_back(shortcut);
    }
    
    return result;
}

std::vector<AdvancedShortcut> AdvancedShortcutManager::getShortcutsByCategory(const std::string& category) const {
    std::vector<AdvancedShortcut> result;
    
    for (const auto& [shortcut, owner] : registeredShortcuts_) {
        if (shortcut.category == category) {
            result.push_back(shortcut);
        }
    }
    
    return result;
}

void AdvancedShortcutManager::addKeyMapping(const KeyMapping& mapping) {
    // Remove existing mapping for the same 'from' shortcut
    removeKeyMapping(mapping.from);
    keyMappings_.push_back(mapping);
    spdlog::debug("Added key mapping: {} → {}", mapping.from.toString(), mapping.to.toString());
}

void AdvancedShortcutManager::removeKeyMapping(const AdvancedShortcut& from) {
    auto it = std::remove_if(keyMappings_.begin(), keyMappings_.end(),
                            [&from](const KeyMapping& mapping) {
                                return mapping.from == from;
                            });
    if (it != keyMappings_.end()) {
        keyMappings_.erase(it, keyMappings_.end());
        spdlog::debug("Removed key mapping for: {}", from.toString());
    }
}

std::vector<KeyMapping> AdvancedShortcutManager::getKeyMappings() const {
    return keyMappings_;
}

AdvancedShortcut AdvancedShortcutManager::resolveShortcut(const AdvancedShortcut& shortcut,
                                                         const std::string& application) const {
    // Check for application-specific mapping first
    for (const auto& mapping : keyMappings_) {
        if (mapping.enabled && mapping.from == shortcut) {
            if (!mapping.application.empty() && mapping.application == application) {
                return mapping.to;
            } else if (mapping.application.empty()) {
                return mapping.to;
            }
        }
    }
    
    return shortcut; // No mapping found, return original
}

std::vector<AdvancedShortcut> AdvancedShortcutManager::suggestAlternatives(const AdvancedShortcut& shortcut) const {
    std::vector<AdvancedShortcut> alternatives;
    
    if (shortcut.type == ShortcutType::Keyboard && !shortcut.keySequence.empty()) {
        const Shortcut& original = shortcut.keySequence[0];
        
        // Suggest alternatives with different modifier combinations
        std::vector<std::tuple<bool, bool, bool, bool>> modifierCombos = {
            {true, true, false, false},   // Ctrl+Alt
            {true, false, true, false},   // Ctrl+Shift
            {false, true, true, false},   // Alt+Shift
            {true, true, true, false},    // Ctrl+Alt+Shift
            {false, false, false, true},  // Win only
            {true, false, false, true},   // Ctrl+Win
        };
        
        for (const auto& [ctrl, alt, shift, win] : modifierCombos) {
            Shortcut alternative(original.vkCode, ctrl, alt, shift, win);
            AdvancedShortcut altShortcut = AdvancedShortcut::createKeyboard(alternative);
            
            if (checkConflicts(altShortcut).empty()) {
                alternatives.push_back(altShortcut);
            }
        }
    }
    
    return alternatives;
}

void AdvancedShortcutManager::clear() {
    registeredShortcuts_.clear();
    keyMappings_.clear();
    spdlog::debug("Cleared all shortcuts and mappings");
}

bool AdvancedShortcutManager::hasConflict(const AdvancedShortcut& s1, const AdvancedShortcut& s2) const {
    // Same type shortcuts can conflict
    if (s1.type == s2.type) {
        switch (s1.type) {
            case ShortcutType::Keyboard:
                return !s1.keySequence.empty() && !s2.keySequence.empty() &&
                       s1.keySequence[0] == s2.keySequence[0];
                       
            case ShortcutType::Mouse:
                return s1.mouseButtons == s2.mouseButtons &&
                       (s1.keySequence.empty() || s2.keySequence.empty() ||
                        s1.keySequence[0] == s2.keySequence[0]);
                        
            case ShortcutType::Multimedia:
                return s1.multimediaKeys == s2.multimediaKeys;
                
            default:
                return false;
        }
    }
    
    return false;
}

std::string AdvancedShortcutManager::getConflictReason(const AdvancedShortcut& s1, const AdvancedShortcut& s2) const {
    if (s1.type == s2.type) {
        return "Identical shortcut combination";
    }
    return "Unknown conflict";
}

ShortcutConflict::Severity AdvancedShortcutManager::assessConflictSeverity(const AdvancedShortcut& s1,
                                                                          const AdvancedShortcut& s2) const {
    // System shortcuts are critical conflicts
    if (s1.category == "System" || s2.category == "System") {
        return ShortcutConflict::Severity::Critical;
    }
    
    // Same application shortcuts are high severity
    if (s1.category == s2.category && !s1.category.empty()) {
        return ShortcutConflict::Severity::High;
    }
    
    return ShortcutConflict::Severity::Medium;
}

// Multimedia utilities implementation
namespace multimedia_utils {

uint32_t getVirtualKeyCode(MultimediaKey key) {
#ifdef _WIN32
    switch (key) {
        case MultimediaKey::VolumeUp: return VK_VOLUME_UP;
        case MultimediaKey::VolumeDown: return VK_VOLUME_DOWN;
        case MultimediaKey::VolumeMute: return VK_VOLUME_MUTE;
        case MultimediaKey::MediaPlayPause: return VK_MEDIA_PLAY_PAUSE;
        case MultimediaKey::MediaStop: return VK_MEDIA_STOP;
        case MultimediaKey::MediaNext: return VK_MEDIA_NEXT_TRACK;
        case MultimediaKey::MediaPrevious: return VK_MEDIA_PREV_TRACK;
        case MultimediaKey::BrowserBack: return VK_BROWSER_BACK;
        case MultimediaKey::BrowserForward: return VK_BROWSER_FORWARD;
        case MultimediaKey::BrowserRefresh: return VK_BROWSER_REFRESH;
        case MultimediaKey::BrowserHome: return VK_BROWSER_HOME;
        case MultimediaKey::LaunchMail: return VK_LAUNCH_MAIL;
        case MultimediaKey::LaunchCalculator: return VK_LAUNCH_APP2;
        case MultimediaKey::LaunchMediaPlayer: return VK_LAUNCH_MEDIA_SELECT;
        default: return 0;
    }
#else
    // Platform-specific implementation for other systems
    return 0;
#endif
}

MultimediaKey getMultimediaKey(uint32_t vkCode) {
#ifdef _WIN32
    switch (vkCode) {
        case VK_VOLUME_UP: return MultimediaKey::VolumeUp;
        case VK_VOLUME_DOWN: return MultimediaKey::VolumeDown;
        case VK_VOLUME_MUTE: return MultimediaKey::VolumeMute;
        case VK_MEDIA_PLAY_PAUSE: return MultimediaKey::MediaPlayPause;
        case VK_MEDIA_STOP: return MultimediaKey::MediaStop;
        case VK_MEDIA_NEXT_TRACK: return MultimediaKey::MediaNext;
        case VK_MEDIA_PREV_TRACK: return MultimediaKey::MediaPrevious;
        case VK_BROWSER_BACK: return MultimediaKey::BrowserBack;
        case VK_BROWSER_FORWARD: return MultimediaKey::BrowserForward;
        case VK_BROWSER_REFRESH: return MultimediaKey::BrowserRefresh;
        case VK_BROWSER_HOME: return MultimediaKey::BrowserHome;
        case VK_LAUNCH_MAIL: return MultimediaKey::LaunchMail;
        case VK_LAUNCH_APP2: return MultimediaKey::LaunchCalculator;
        case VK_LAUNCH_MEDIA_SELECT: return MultimediaKey::LaunchMediaPlayer;
        default: return MultimediaKey::VolumeUp; // Default fallback
    }
#else
    return MultimediaKey::VolumeUp; // Default fallback
#endif
}

bool isMultimediaKey(uint32_t vkCode) {
#ifdef _WIN32
    static const std::unordered_set<uint32_t> multimediaKeys = {
        VK_VOLUME_UP, VK_VOLUME_DOWN, VK_VOLUME_MUTE,
        VK_MEDIA_PLAY_PAUSE, VK_MEDIA_STOP, VK_MEDIA_NEXT_TRACK, VK_MEDIA_PREV_TRACK,
        VK_BROWSER_BACK, VK_BROWSER_FORWARD, VK_BROWSER_REFRESH, VK_BROWSER_HOME,
        VK_LAUNCH_MAIL, VK_LAUNCH_APP1, VK_LAUNCH_APP2, VK_LAUNCH_MEDIA_SELECT
    };
    return multimediaKeys.count(vkCode) > 0;
#else
    return false;
#endif
}

std::string getKeyName(MultimediaKey key) {
    switch (key) {
        case MultimediaKey::VolumeUp: return "Volume Up";
        case MultimediaKey::VolumeDown: return "Volume Down";
        case MultimediaKey::VolumeMute: return "Volume Mute";
        case MultimediaKey::MediaPlayPause: return "Play/Pause";
        case MultimediaKey::MediaStop: return "Stop";
        case MultimediaKey::MediaNext: return "Next Track";
        case MultimediaKey::MediaPrevious: return "Previous Track";
        case MultimediaKey::BrowserBack: return "Browser Back";
        case MultimediaKey::BrowserForward: return "Browser Forward";
        case MultimediaKey::BrowserRefresh: return "Browser Refresh";
        case MultimediaKey::BrowserHome: return "Browser Home";
        case MultimediaKey::LaunchMail: return "Launch Mail";
        case MultimediaKey::LaunchCalculator: return "Launch Calculator";
        case MultimediaKey::LaunchMediaPlayer: return "Launch Media Player";
        default: return "Unknown";
    }
}

}  // namespace multimedia_utils

// Mouse utilities implementation
namespace mouse_utils {

std::string mouseButtonToString(MouseButton button) {
    switch (button) {
        case MouseButton::Left: return "Left Click";
        case MouseButton::Right: return "Right Click";
        case MouseButton::Middle: return "Middle Click";
        case MouseButton::X1: return "X1 Button";
        case MouseButton::X2: return "X2 Button";
        case MouseButton::WheelUp: return "Wheel Up";
        case MouseButton::WheelDown: return "Wheel Down";
        default: return "Unknown Button";
    }
}

MouseButton stringToMouseButton(const std::string& str) {
    if (str == "Left Click") return MouseButton::Left;
    if (str == "Right Click") return MouseButton::Right;
    if (str == "Middle Click") return MouseButton::Middle;
    if (str == "X1 Button") return MouseButton::X1;
    if (str == "X2 Button") return MouseButton::X2;
    if (str == "Wheel Up") return MouseButton::WheelUp;
    if (str == "Wheel Down") return MouseButton::WheelDown;
    return MouseButton::Left; // Default fallback
}

bool isValidMouseCombination(const std::vector<MouseButton>& buttons) {
    if (buttons.empty()) return false;

    // Check for conflicting wheel directions
    bool hasWheelUp = std::find(buttons.begin(), buttons.end(), MouseButton::WheelUp) != buttons.end();
    bool hasWheelDown = std::find(buttons.begin(), buttons.end(), MouseButton::WheelDown) != buttons.end();

    if (hasWheelUp && hasWheelDown) {
        return false; // Can't have both wheel directions
    }

    // Check for duplicate buttons
    std::unordered_set<MouseButton> uniqueButtons(buttons.begin(), buttons.end());
    return uniqueButtons.size() == buttons.size();
}

}  // namespace mouse_utils

}  // namespace shortcut_detector
