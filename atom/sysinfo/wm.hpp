#ifndef ATOM_SYSINFO_WM_HPP
#define ATOM_SYSINFO_WM_HPP

// Include the new modular window manager implementation
#include "src/wm/wm.hpp"

#include <string>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Contains system desktop environment and window manager information.
 * @deprecated Use atom::system::wm::SystemInfo for enhanced functionality
 */
struct SystemInfo {
    std::string
        desktopEnvironment;  //!< Desktop environment (e.g., Fluent, GNOME, KDE)
    std::string windowManager;  //!< Window manager (e.g., Desktop Window
                                //!< Manager, i3, bspwm)
    std::string wmTheme;        //!< Window manager theme information
    std::string icons;          //!< Icon theme or icon information
    std::string font;           //!< System font information
    std::string cursor;         //!< Cursor theme information
} ATOM_ALIGNAS(128);

/**
 * @brief Retrieves system desktop environment and window manager information.
 * @deprecated Use atom::system::wm::getSystemInfo() for enhanced functionality
 * @return SystemInfo structure containing desktop environment details
 */
[[nodiscard]] auto getSystemInfo() -> SystemInfo;

// Backward compatibility aliases - expose new namespace types in old namespace
using WindowInfo = wm::WindowInfo;
using WindowState = wm::WindowState;
using WindowType = wm::WindowType;
using ThemeInfo = wm::ThemeInfo;
using ThemeType = wm::ThemeType;
using MonitorInfo = wm::MonitorInfo;
using WorkspaceInfo = wm::WorkspaceInfo;
using WMError = wm::WMError;

template<typename T>
using WMResult = wm::WMResult<T>;

// Backward compatibility function wrappers
[[nodiscard]] inline auto getEnhancedSystemInfo() -> WMResult<wm::SystemInfo> {
    return wm::getSystemInfo();
}

[[nodiscard]] inline auto getThemeInfo() -> WMResult<ThemeInfo> {
    return wm::getThemeInfo();
}

[[nodiscard]] inline auto enumerateWindows() -> WMResult<std::vector<WindowInfo>> {
    return wm::enumerateWindows();
}

[[nodiscard]] inline auto getWindowInfo(uint64_t windowId) -> WMResult<WindowInfo> {
    return wm::getWindowInfo(windowId);
}

[[nodiscard]] inline auto getMonitors() -> WMResult<std::vector<MonitorInfo>> {
    return wm::getMonitors();
}

[[nodiscard]] inline auto getWorkspaces() -> WMResult<std::vector<WorkspaceInfo>> {
    return wm::getWorkspaces();
}

[[nodiscard]] inline auto setWindowState(uint64_t windowId, WindowState state) -> WMResult<bool> {
    return wm::setWindowState(windowId, state);
}

[[nodiscard]] inline auto moveWindow(uint64_t windowId, int x, int y) -> WMResult<bool> {
    return wm::moveWindow(windowId, x, y);
}

[[nodiscard]] inline auto resizeWindow(uint64_t windowId, int width, int height) -> WMResult<bool> {
    return wm::resizeWindow(windowId, width, height);
}

[[nodiscard]] inline auto focusWindow(uint64_t windowId) -> WMResult<bool> {
    return wm::focusWindow(windowId);
}

[[nodiscard]] inline auto closeWindow(uint64_t windowId) -> WMResult<bool> {
    return wm::closeWindow(windowId);
}

[[nodiscard]] inline auto switchToWorkspace(uint32_t workspaceId) -> WMResult<bool> {
    return wm::switchToWorkspace(workspaceId);
}

[[nodiscard]] inline auto moveWindowToWorkspace(uint64_t windowId, uint32_t workspaceId) -> WMResult<bool> {
    return wm::moveWindowToWorkspace(windowId, workspaceId);
}

// Backward compatibility class aliases
using WindowManager = wm::WindowManager;
using ThemeManager = wm::ThemeManager;
using WorkspaceManager = wm::WorkspaceManager;

// Utility functions
using wm::errorToString;
using wm::windowStateToString;
using wm::windowTypeToString;
using wm::themeTypeToString;
using wm::isError;
using wm::getError;
using wm::getValue;

}  // namespace atom::system

#endif
