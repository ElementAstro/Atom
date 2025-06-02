#pragma once

#include <string>

namespace shortcut_detector {

/**
 * @brief Possible states for a shortcut
 */
enum class ShortcutStatus {
    Available,         // Shortcut is available for registration
    CapturedByApp,     // Shortcut is captured by an application
    CapturedBySystem,  // Shortcut is captured by system component
    Reserved           // Shortcut is reserved by Windows
};

/**
 * @brief Result of a shortcut check operation
 */
struct ShortcutCheckResult {
    ShortcutStatus status;  // Status of the shortcut
    std::string
        capturingApplication;  // Name of capturing application (if applicable)
    std::string details;       // Additional details
};

}  // namespace shortcut_detector