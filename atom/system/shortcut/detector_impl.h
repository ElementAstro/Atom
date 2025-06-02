#pragma once

#include <windows.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "shortcut.h"
#include "status.h"


namespace shortcut_detector {

/**
 * @brief Implementation class for ShortcutDetector (PIMPL idiom)
 */
class ShortcutDetectorImpl {
public:
    ShortcutDetectorImpl();
    ~ShortcutDetectorImpl();

    ShortcutCheckResult isShortcutCaptured(const Shortcut& shortcut);
    bool hasKeyboardHookInstalled();
    std::vector<std::string> getProcessesWithKeyboardHooks();

private:
    bool isSystemReservedShortcut(const Shortcut& shortcut);
    bool attemptHotkeyRegistration(const Shortcut& shortcut);
    bool hasInterceptingKeyboardHook(const Shortcut& shortcut);
    std::string findCapturingApplication(const Shortcut& shortcut);
    std::string findKeyboardHookOwner();

    // Windows system-reserved shortcuts
    static const std::unordered_map<uint32_t, std::vector<uint32_t>>
        systemReservedShortcuts;
};

}  // namespace shortcut_detector