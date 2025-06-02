
#include "detector_impl.h"
#include "win32_utils.h"

#include <psapi.h>
#include <tlhelp32.h>

namespace shortcut_detector {

// Initialize static map of system-reserved shortcuts
const std::unordered_map<uint32_t, std::vector<uint32_t>>
    ShortcutDetectorImpl::systemReservedShortcuts = {
        // Alt+Tab combinations
        {VK_TAB, {VK_MENU}},
        {VK_TAB, {VK_MENU, VK_SHIFT}},

        // Windows key combinations
        {VK_TAB, {VK_LWIN}},  // Win+Tab (Task View)
        {'D', {VK_LWIN}},     // Win+D (Show desktop)
        {'E', {VK_LWIN}},     // Win+E (File Explorer)
        {'L', {VK_LWIN}},     // Win+L (Lock)
        {'R', {VK_LWIN}},     // Win+R (Run)
        {'I', {VK_LWIN}},     // Win+I (Settings)
        {'X', {VK_LWIN}},     // Win+X (Power User menu)

        // Ctrl+Alt+Del and others
        {VK_DELETE, {VK_CONTROL, VK_MENU}},
        {VK_ESCAPE, {VK_CONTROL, VK_SHIFT}}  // Ctrl+Shift+Esc (Task Manager)
};

ShortcutDetectorImpl::ShortcutDetectorImpl() {}

ShortcutDetectorImpl::~ShortcutDetectorImpl() {}

ShortcutCheckResult ShortcutDetectorImpl::isShortcutCaptured(
    const Shortcut& shortcut) {
    // 1. Check if this is a system-reserved shortcut
    if (isSystemReservedShortcut(shortcut)) {
        return {ShortcutStatus::Reserved, "Windows",
                "This shortcut is reserved by Windows"};
    }

    // 2. Try to register the hotkey to see if it's available
    bool canRegister = attemptHotkeyRegistration(shortcut);
    if (!canRegister) {
        // Determine what might be capturing it
        auto capturingApp = findCapturingApplication(shortcut);
        if (capturingApp.empty()) {
            return {ShortcutStatus::CapturedBySystem,
                    "Unknown System Component",
                    "The shortcut is captured by the system"};
        } else {
            return {ShortcutStatus::CapturedByApp, capturingApp,
                    "The shortcut is registered by another application"};
        }
    }

    // 3. Check for low-level keyboard hooks
    if (hasInterceptingKeyboardHook(shortcut)) {
        auto hookOwner = findKeyboardHookOwner();
        return {ShortcutStatus::CapturedByApp, hookOwner,
                "A keyboard hook may intercept this shortcut"};
    }

    // The shortcut appears to be available
    return {ShortcutStatus::Available, "",
            "The shortcut is available for registration"};
}

bool ShortcutDetectorImpl::isSystemReservedShortcut(const Shortcut& shortcut) {
    // Check if the shortcut is in our system-reserved map
    auto it = systemReservedShortcuts.find(shortcut.vkCode);
    if (it == systemReservedShortcuts.end()) {
        return false;
    }

    // For each potential modifier combination
    for (const auto& modifiers : it->second) {
        bool matchesModifiers = true;

        // Check if modifiers match
        bool hasCtrl = (modifiers & VK_CONTROL) == VK_CONTROL;
        bool hasAlt = (modifiers & VK_MENU) == VK_MENU;
        bool hasShift = (modifiers & VK_SHIFT) == VK_SHIFT;
        bool hasWin = (modifiers & VK_LWIN) == VK_LWIN;

        if (shortcut.ctrl == hasCtrl && shortcut.alt == hasAlt &&
            shortcut.shift == hasShift && shortcut.win == hasWin) {
            return true;
        }
    }

    return false;
}

bool ShortcutDetectorImpl::attemptHotkeyRegistration(const Shortcut& shortcut) {
    UINT modifiers = 0;
    if (shortcut.alt)
        modifiers |= MOD_ALT;
    if (shortcut.ctrl)
        modifiers |= MOD_CONTROL;
    if (shortcut.shift)
        modifiers |= MOD_SHIFT;
    if (shortcut.win)
        modifiers |= MOD_WIN;

    // Add MOD_NOREPEAT to prevent auto-repeat
    modifiers |= MOD_NOREPEAT;

    // Use a unique atom as the hotkey ID
    TCHAR atomName[100];
    wsprintf(atomName, TEXT("ShortcutDetectorTempHotkey_%d"),
             GetCurrentProcessId());
    int uniqueId = GlobalAddAtom(atomName);

    // Try to register and see if it succeeds
    bool result =
        RegisterHotKey(NULL, uniqueId, modifiers, shortcut.vkCode) != 0;

    // Always unregister to clean up
    if (result) {
        UnregisterHotKey(NULL, uniqueId);
    }

    GlobalDeleteAtom(uniqueId);
    return result;
}

bool ShortcutDetectorImpl::hasInterceptingKeyboardHook(
    const Shortcut& shortcut) {
    // This is a simplification - in reality, detecting hooks from another
    // process is difficult without kernel-mode drivers
    return hasKeyboardHookInstalled();
}

bool ShortcutDetectorImpl::hasKeyboardHookInstalled() {
    return !getProcessesWithKeyboardHooks().empty();
}

std::vector<std::string> ShortcutDetectorImpl::getProcessesWithKeyboardHooks() {
    return win32_utils::getProcessesWithKeyboardHooks();
}

std::string ShortcutDetectorImpl::findCapturingApplication(
    const Shortcut& shortcut) {
    // Simplified approach - in a real implementation, we'd need to query system
    // services Win32 API doesn't provide a direct way to find which process
    // registered a hotkey
    return "";
}

std::string ShortcutDetectorImpl::findKeyboardHookOwner() {
    auto processes = getProcessesWithKeyboardHooks();
    if (processes.empty()) {
        return "";
    }
    return processes[0];
}

}  // namespace shortcut_detector