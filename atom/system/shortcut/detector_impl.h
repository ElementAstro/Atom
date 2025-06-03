#pragma once

#include <windows.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "shortcut.h"
#include "status.h"

namespace shortcut_detector {

/**
 * @brief Implementation class for ShortcutDetector using PIMPL idiom
 * 
 * This class provides the actual implementation for keyboard shortcut detection
 * on Windows systems. It checks for system-reserved shortcuts, attempts hotkey
 * registration, and detects keyboard hooks.
 */
class ShortcutDetectorImpl {
public:
    ShortcutDetectorImpl();
    ~ShortcutDetectorImpl();

    /**
     * @brief Check if a keyboard shortcut is captured by system or applications
     * 
     * @param shortcut The shortcut to check
     * @return ShortcutCheckResult Result containing status and details
     */
    ShortcutCheckResult isShortcutCaptured(const Shortcut& shortcut);
    
    /**
     * @brief Check if any keyboard hook is currently installed
     * 
     * @return true If keyboard hooks are detected
     * @return false If no keyboard hooks are detected
     */
    bool hasKeyboardHookInstalled();
    
    /**
     * @brief Get list of processes that have keyboard hooks installed
     * 
     * @return std::vector<std::string> Process names with keyboard hooks
     */
    std::vector<std::string> getProcessesWithKeyboardHooks();

private:
    bool isSystemReservedShortcut(const Shortcut& shortcut);
    bool attemptHotkeyRegistration(const Shortcut& shortcut);
    bool hasInterceptingKeyboardHook(const Shortcut& shortcut);
    std::string findCapturingApplication(const Shortcut& shortcut);
    std::string findKeyboardHookOwner();

    /**
     * @brief Static map of Windows system-reserved keyboard shortcuts
     */
    static const std::unordered_map<uint32_t, std::vector<uint32_t>>
        systemReservedShortcuts;
};

}  // namespace shortcut_detector