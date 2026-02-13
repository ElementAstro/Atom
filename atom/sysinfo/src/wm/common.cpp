#include "common.hpp"

namespace atom::system::wm {

auto errorToString(WMError error) -> std::string {
    switch (error) {
        case WMError::NONE:
            return "No error";
        case WMError::PLATFORM_NOT_SUPPORTED:
            return "Platform not supported";
        case WMError::PERMISSION_DENIED:
            return "Permission denied";
        case WMError::WINDOW_NOT_FOUND:
            return "Window not found";
        case WMError::WORKSPACE_NOT_FOUND:
            return "Workspace not found";
        case WMError::OPERATION_FAILED:
            return "Operation failed";
        case WMError::INVALID_PARAMETER:
            return "Invalid parameter";
        case WMError::TIMEOUT:
            return "Operation timed out";
        case WMError::UNKNOWN_ERROR:
        default:
            return "Unknown error";
    }
}

auto windowStateToString(WindowState state) -> std::string {
    switch (state) {
        case WindowState::NORMAL:
            return "Normal";
        case WindowState::MINIMIZED:
            return "Minimized";
        case WindowState::MAXIMIZED:
            return "Maximized";
        case WindowState::FULLSCREEN:
            return "Fullscreen";
        case WindowState::HIDDEN:
            return "Hidden";
        case WindowState::UNKNOWN:
        default:
            return "Unknown";
    }
}

auto windowTypeToString(WindowType type) -> std::string {
    switch (type) {
        case WindowType::NORMAL:
            return "Normal";
        case WindowType::DIALOG:
            return "Dialog";
        case WindowType::UTILITY:
            return "Utility";
        case WindowType::TOOLBAR:
            return "Toolbar";
        case WindowType::SPLASH:
            return "Splash";
        case WindowType::MENU:
            return "Menu";
        case WindowType::DROPDOWN_MENU:
            return "Dropdown Menu";
        case WindowType::POPUP_MENU:
            return "Popup Menu";
        case WindowType::TOOLTIP:
            return "Tooltip";
        case WindowType::NOTIFICATION:
            return "Notification";
        case WindowType::COMBO:
            return "Combo";
        case WindowType::DND:
            return "Drag and Drop";
        case WindowType::DESKTOP:
            return "Desktop";
        case WindowType::DOCK:
            return "Dock";
        case WindowType::UNKNOWN:
        default:
            return "Unknown";
    }
}

auto themeTypeToString(ThemeType type) -> std::string {
    switch (type) {
        case ThemeType::LIGHT:
            return "Light";
        case ThemeType::DARK:
            return "Dark";
        case ThemeType::AUTO:
            return "Auto";
        case ThemeType::HIGH_CONTRAST:
            return "High Contrast";
        case ThemeType::CUSTOM:
            return "Custom";
        case ThemeType::UNKNOWN:
        default:
            return "Unknown";
    }
}

}  // namespace atom::system::wm
