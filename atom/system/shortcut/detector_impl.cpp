
#include "detector_impl.h"
#include "win32_utils.h"

#include <psapi.h>
#include <tlhelp32.h>
#include <spdlog/spdlog.h>

namespace shortcut_detector {

/**
 * @brief Static map of system-reserved keyboard shortcuts for Windows
 */
const std::unordered_map<uint32_t, std::vector<uint32_t>>
    ShortcutDetectorImpl::systemReservedShortcuts = {
        {VK_TAB, {VK_MENU}},                         // Alt+Tab
        {VK_TAB, {VK_MENU, VK_SHIFT}},               // Alt+Shift+Tab
        {VK_TAB, {VK_LWIN}},                         // Win+Tab (Task View)
        {'D', {VK_LWIN}},                            // Win+D (Show desktop)
        {'E', {VK_LWIN}},                            // Win+E (File Explorer)
        {'L', {VK_LWIN}},                            // Win+L (Lock)
        {'R', {VK_LWIN}},                            // Win+R (Run)
        {'I', {VK_LWIN}},                            // Win+I (Settings)
        {'X', {VK_LWIN}},                            // Win+X (Power User menu)
        {VK_DELETE, {VK_CONTROL, VK_MENU}},          // Ctrl+Alt+Del
        {VK_ESCAPE, {VK_CONTROL, VK_SHIFT}}          // Ctrl+Shift+Esc (Task Manager)
};

ShortcutDetectorImpl::ShortcutDetectorImpl() {}

ShortcutDetectorImpl::~ShortcutDetectorImpl() {}

ShortcutCheckResult ShortcutDetectorImpl::isShortcutCaptured(
    const Shortcut& shortcut) {
    spdlog::debug("Checking if shortcut {} is captured", shortcut.toString());

    if (isSystemReservedShortcut(shortcut)) {
        spdlog::debug("Shortcut {} is reserved by Windows", shortcut.toString());
        return {ShortcutStatus::Reserved, "Windows",
                "This shortcut is reserved by Windows"};
    }

    const bool canRegister = attemptHotkeyRegistration(shortcut);
    if (!canRegister) {
        const auto capturingApp = findCapturingApplication(shortcut);
        if (capturingApp.empty()) {
            spdlog::debug("Shortcut {} is captured by unknown system component", 
                         shortcut.toString());
            return {ShortcutStatus::CapturedBySystem,
                    "Unknown System Component",
                    "The shortcut is captured by the system"};
        } else {
            spdlog::debug("Shortcut {} is captured by application: {}", 
                         shortcut.toString(), capturingApp);
            return {ShortcutStatus::CapturedByApp, capturingApp,
                    "The shortcut is registered by another application"};
        }
    }

    if (hasInterceptingKeyboardHook(shortcut)) {
        const auto hookOwner = findKeyboardHookOwner();
        spdlog::debug("Shortcut {} may be intercepted by keyboard hook owned by: {}", 
                     shortcut.toString(), hookOwner);
        return {ShortcutStatus::CapturedByApp, hookOwner,
                "A keyboard hook may intercept this shortcut"};
    }

    spdlog::debug("Shortcut {} is available for registration", shortcut.toString());
    return {ShortcutStatus::Available, "",
            "The shortcut is available for registration"};
}

bool ShortcutDetectorImpl::isSystemReservedShortcut(const Shortcut& shortcut) {
    const auto it = systemReservedShortcuts.find(shortcut.vkCode);
    if (it == systemReservedShortcuts.end()) {
        return false;
    }

    for (const auto& modifiers : it->second) {
        const bool hasCtrl = (modifiers & VK_CONTROL) == VK_CONTROL;
        const bool hasAlt = (modifiers & VK_MENU) == VK_MENU;
        const bool hasShift = (modifiers & VK_SHIFT) == VK_SHIFT;
        const bool hasWin = (modifiers & VK_LWIN) == VK_LWIN;

        if (shortcut.ctrl == hasCtrl && shortcut.alt == hasAlt &&
            shortcut.shift == hasShift && shortcut.win == hasWin) {
            return true;
        }
    }

    return false;
}

bool ShortcutDetectorImpl::attemptHotkeyRegistration(const Shortcut& shortcut) {
    UINT modifiers = 0;
    if (shortcut.alt) modifiers |= MOD_ALT;
    if (shortcut.ctrl) modifiers |= MOD_CONTROL;
    if (shortcut.shift) modifiers |= MOD_SHIFT;
    if (shortcut.win) modifiers |= MOD_WIN;

    modifiers |= MOD_NOREPEAT;

    TCHAR atomName[100];
    wsprintf(atomName, TEXT("ShortcutDetectorTempHotkey_%d"), GetCurrentProcessId());
    const int uniqueId = GlobalAddAtom(atomName);

    const bool result = RegisterHotKey(NULL, uniqueId, modifiers, shortcut.vkCode) != 0;

    if (result) {
        UnregisterHotKey(NULL, uniqueId);
    }

    GlobalDeleteAtom(uniqueId);
    return result;
}

bool ShortcutDetectorImpl::hasInterceptingKeyboardHook(const Shortcut& shortcut) {
    return hasKeyboardHookInstalled();
}

bool ShortcutDetectorImpl::hasKeyboardHookInstalled() {
    return !getProcessesWithKeyboardHooks().empty();
}

std::vector<std::string> ShortcutDetectorImpl::getProcessesWithKeyboardHooks() {
    return win32_utils::getProcessesWithKeyboardHooks();
}

std::string ShortcutDetectorImpl::findCapturingApplication(const Shortcut& shortcut) {
    return "";
}

std::string ShortcutDetectorImpl::findKeyboardHookOwner() {
    const auto processes = getProcessesWithKeyboardHooks();
    if (processes.empty()) {
        return "";
    }
    return processes[0];
}

}  // namespace shortcut_detector