#ifndef ATOM_SYSINFO_WM_LINUX_HPP
#define ATOM_SYSINFO_WM_LINUX_HPP

#include "common.hpp"

#ifdef __linux__

namespace atom::system::wm::linux {

/**
 * @brief Get Linux-specific system information
 */
[[nodiscard]] auto getSystemInfo() -> WMResult<SystemInfo>;

/**
 * @brief Get desktop environment from environment variables
 */
[[nodiscard]] auto getDesktopEnvironment() -> std::string;

/**
 * @brief Execute a shell command and return the output
 */
[[nodiscard]] auto executeCommand(std::string_view command) -> std::string;

/**
 * @brief Get theme information for Linux desktop environments
 */
[[nodiscard]] auto getThemeInfo() -> WMResult<ThemeInfo>;

/**
 * @brief Enumerate all windows using X11/Wayland
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
 * @brief Get workspaces/desktops
 */
[[nodiscard]] auto getWorkspaces() -> WMResult<std::vector<WorkspaceInfo>>;

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
 * @brief Switch to workspace
 */
[[nodiscard]] auto switchToWorkspace(uint32_t workspaceId) -> WMResult<bool>;

/**
 * @brief Move window to workspace
 */
[[nodiscard]] auto moveWindowToWorkspace(uint64_t windowId, uint32_t workspaceId) -> WMResult<bool>;

/**
 * @brief Check if running under Wayland
 */
[[nodiscard]] auto isWayland() -> bool;

/**
 * @brief Check if running under X11
 */
[[nodiscard]] auto isX11() -> bool;

}  // namespace atom::system::wm::linux

#endif  // __linux__

#endif  // ATOM_SYSINFO_WM_LINUX_HPP
