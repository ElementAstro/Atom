#ifndef ATOM_SYSINFO_WM_WINDOWS_HPP
#define ATOM_SYSINFO_WM_WINDOWS_HPP

#include "common.hpp"

#ifdef _WIN32

namespace atom::system::wm::windows {

/**
 * @brief Get Windows-specific system information
 */
[[nodiscard]] auto getSystemInfo() -> WMResult<SystemInfo>;

/**
 * @brief Get Windows theme information
 */
[[nodiscard]] auto getThemeInfo() -> WMResult<ThemeInfo>;

/**
 * @brief Get Windows font information
 */
[[nodiscard]] auto getFontInfo() -> std::string;

/**
 * @brief Check if Desktop Window Manager is enabled
 */
[[nodiscard]] auto getWindowManager() -> std::string;

/**
 * @brief Enumerate all windows
 */
[[nodiscard]] auto enumerateWindows() -> WMResult<std::vector<WindowInfo>>;

/**
 * @brief Get information about a specific window
 */
[[nodiscard]] auto getWindowInfo(uint64_t windowId) -> WMResult<WindowInfo>;

/**
 * @brief Get all monitors/displays
 */
[[nodiscard]] auto getMonitors() -> WMResult<std::vector<MonitorInfo>>;

/**
 * @brief Get virtual desktops (Windows 10+)
 */
[[nodiscard]] auto getVirtualDesktops() -> WMResult<std::vector<WorkspaceInfo>>;

/**
 * @brief Set window state
 */
[[nodiscard]] auto setWindowState(uint64_t windowId, WindowState state) -> WMResult<bool>;

/**
 * @brief Move window to position
 */
[[nodiscard]] auto moveWindow(uint64_t windowId, int x, int y) -> WMResult<bool>;

/**
 * @brief Resize window
 */
[[nodiscard]] auto resizeWindow(uint64_t windowId, int width, int height) -> WMResult<bool>;

/**
 * @brief Focus window
 */
[[nodiscard]] auto focusWindow(uint64_t windowId) -> WMResult<bool>;

/**
 * @brief Close window
 */
[[nodiscard]] auto closeWindow(uint64_t windowId) -> WMResult<bool>;

}  // namespace atom::system::wm::windows

#endif  // _WIN32

#endif  // ATOM_SYSINFO_WM_WINDOWS_HPP
