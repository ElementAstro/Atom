#ifndef ATOM_SYSINFO_WM_MACOS_HPP
#define ATOM_SYSINFO_WM_MACOS_HPP

#include "common.hpp"

#ifdef __APPLE__

namespace atom::system::wm::macos {

/**
 * @brief Get macOS-specific system information
 */
[[nodiscard]] auto getSystemInfo() -> WMResult<SystemInfo>;

/**
 * @brief Get macOS theme information
 */
[[nodiscard]] auto getThemeInfo() -> WMResult<ThemeInfo>;

/**
 * @brief Enumerate all windows using macOS APIs
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
 * @brief Get Spaces (macOS workspaces)
 */
[[nodiscard]] auto getSpaces() -> WMResult<std::vector<WorkspaceInfo>>;

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

/**
 * @brief Switch to Space (macOS workspace)
 */
[[nodiscard]] auto switchToSpace(uint32_t spaceId) -> WMResult<bool>;

/**
 * @brief Move window to Space
 */
[[nodiscard]] auto moveWindowToSpace(uint64_t windowId, uint32_t spaceId) -> WMResult<bool>;

}  // namespace atom::system::wm::macos

#endif  // __APPLE__

#endif  // ATOM_SYSINFO_WM_MACOS_HPP
