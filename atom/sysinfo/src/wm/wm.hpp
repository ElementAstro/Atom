#ifndef ATOM_SYSINFO_WM_WM_HPP
#define ATOM_SYSINFO_WM_WM_HPP

#include "common.hpp"

#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include <iterator>

namespace atom::system::wm {

/**
 * @brief Get comprehensive system information including window manager details
 * @return SystemInfo structure containing all system information
 */
[[nodiscard]] auto getSystemInfo() -> WMResult<SystemInfo>;

/**
 * @brief Get current theme information
 * @return ThemeInfo structure containing theme details
 */
[[nodiscard]] auto getThemeInfo() -> WMResult<ThemeInfo>;

/**
 * @brief Enumerate all visible windows in the system
 * @return Vector of WindowInfo structures for all windows
 */
[[nodiscard]] auto enumerateWindows() -> WMResult<std::vector<WindowInfo>>;

/**
 * @brief Get detailed information about a specific window
 * @param windowId The window ID to query
 * @return WindowInfo structure for the specified window
 */
[[nodiscard]] auto getWindowInfo(uint64_t windowId) -> WMResult<WindowInfo>;

/**
 * @brief Get information about all connected monitors/displays
 * @return Vector of MonitorInfo structures
 */
[[nodiscard]] auto getMonitors() -> WMResult<std::vector<MonitorInfo>>;

/**
 * @brief Get information about all available workspaces/virtual desktops
 * @return Vector of WorkspaceInfo structures
 */
[[nodiscard]] auto getWorkspaces() -> WMResult<std::vector<WorkspaceInfo>>;

/**
 * @brief Set the state of a window (minimize, maximize, etc.)
 * @param windowId The window ID to modify
 * @param state The desired window state
 * @return true if successful, error otherwise
 */
[[nodiscard]] auto setWindowState(uint64_t windowId, WindowState state) -> WMResult<bool>;

/**
 * @brief Move a window to a specific position
 * @param windowId The window ID to move
 * @param x The new X coordinate
 * @param y The new Y coordinate
 * @return true if successful, error otherwise
 */
[[nodiscard]] auto moveWindow(uint64_t windowId, int x, int y) -> WMResult<bool>;

/**
 * @brief Resize a window to specific dimensions
 * @param windowId The window ID to resize
 * @param width The new width
 * @param height The new height
 * @return true if successful, error otherwise
 */
[[nodiscard]] auto resizeWindow(uint64_t windowId, int width, int height) -> WMResult<bool>;

/**
 * @brief Focus/activate a window
 * @param windowId The window ID to focus
 * @return true if successful, error otherwise
 */
[[nodiscard]] auto focusWindow(uint64_t windowId) -> WMResult<bool>;

/**
 * @brief Close a window
 * @param windowId The window ID to close
 * @return true if successful, error otherwise
 */
[[nodiscard]] auto closeWindow(uint64_t windowId) -> WMResult<bool>;

/**
 * @brief Switch to a specific workspace/virtual desktop
 * @param workspaceId The workspace ID to switch to
 * @return true if successful, error otherwise
 */
[[nodiscard]] auto switchToWorkspace(uint32_t workspaceId) -> WMResult<bool>;

/**
 * @brief Move a window to a specific workspace/virtual desktop
 * @param windowId The window ID to move
 * @param workspaceId The target workspace ID
 * @return true if successful, error otherwise
 */
[[nodiscard]] auto moveWindowToWorkspace(uint64_t windowId, uint32_t workspaceId) -> WMResult<bool>;

/**
 * @brief Window manager class for advanced window management operations
 */
class WindowManager {
public:
    WindowManager();
    ~WindowManager();

    // Non-copyable, non-movable
    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;
    WindowManager(WindowManager&&) = delete;
    WindowManager& operator=(WindowManager&&) = delete;

    /**
     * @brief Start monitoring window changes
     * @param callback Function to call when windows are added/removed
     * @param interval Monitoring interval (default: 1000ms)
     * @return true if monitoring started successfully
     */
    auto startMonitoring(WindowCallback callback,
                        std::chrono::milliseconds interval = std::chrono::milliseconds(1000)) -> bool;

    /**
     * @brief Stop monitoring window changes
     */
    void stopMonitoring();

    /**
     * @brief Check if currently monitoring
     * @return true if monitoring is active
     */
    [[nodiscard]] auto isMonitoring() const -> bool;

    /**
     * @brief Get all windows matching a filter
     * @param filter Function to filter windows
     * @return Vector of matching windows
     */
    template<typename FilterFunc>
    [[nodiscard]] auto getFilteredWindows(FilterFunc filter) -> WMResult<std::vector<WindowInfo>> {
        auto windowsResult = enumerateWindows();
        if (isError(windowsResult)) {
            return getError(windowsResult);
        }

        std::vector<WindowInfo> filtered;
        const auto& windows = getValue(windowsResult);

        std::copy_if(windows.begin(), windows.end(), std::back_inserter(filtered), filter);
        return filtered;
    }

    /**
     * @brief Get windows by process name
     * @param processName The process name to search for
     * @return Vector of windows belonging to the process
     */
    [[nodiscard]] auto getWindowsByProcess(const std::string& processName) -> WMResult<std::vector<WindowInfo>>;

    /**
     * @brief Get windows in a specific workspace
     * @param workspaceId The workspace ID
     * @return Vector of windows in the workspace
     */
    [[nodiscard]] auto getWindowsInWorkspace(uint32_t workspaceId) -> WMResult<std::vector<WindowInfo>>;

private:
    std::atomic<bool> isMonitoring_{false};
    std::thread monitoringThread_;
    WindowCallback windowCallback_;
};

/**
 * @brief Theme manager class for theme monitoring and management
 */
class ThemeManager {
public:
    ThemeManager();
    ~ThemeManager() { stopMonitoring(); }

    // Non-copyable, non-movable
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
    ThemeManager(ThemeManager&&) = delete;
    ThemeManager& operator=(ThemeManager&&) = delete;

    /**
     * @brief Get current theme information
     * @return Current theme details
     */
    [[nodiscard]] auto getCurrentTheme() -> WMResult<ThemeInfo>;

    /**
     * @brief Start monitoring theme changes
     * @param callback Function to call when theme changes
     * @param interval Monitoring interval (default: 5000ms)
     * @return true if monitoring started successfully
     */
    auto startMonitoring(ThemeCallback callback,
                        std::chrono::milliseconds interval = std::chrono::milliseconds(5000)) -> bool;

    /**
     * @brief Stop monitoring theme changes
     */
    void stopMonitoring();

    /**
     * @brief Check if currently monitoring
     * @return true if monitoring is active
     */
    [[nodiscard]] auto isMonitoring() const -> bool;

private:
    std::atomic<bool> isMonitoring_{false};
    std::thread monitoringThread_;
    ThemeCallback themeCallback_;
};

/**
 * @brief Workspace manager class for workspace operations
 */
class WorkspaceManager {
public:
    /**
     * @brief Get current active workspace
     * @return Active workspace information
     */
    [[nodiscard]] static auto getCurrentWorkspace() -> WMResult<WorkspaceInfo>;

    /**
     * @brief Create a new workspace (if supported)
     * @param name The name for the new workspace
     * @return The new workspace ID if successful
     */
    [[nodiscard]] static auto createWorkspace(const std::string& name) -> WMResult<uint32_t>;

    /**
     * @brief Delete a workspace (if supported)
     * @param workspaceId The workspace ID to delete
     * @return true if successful
     */
    [[nodiscard]] static auto deleteWorkspace(uint32_t workspaceId) -> WMResult<bool>;

    /**
     * @brief Rename a workspace (if supported)
     * @param workspaceId The workspace ID to rename
     * @param newName The new name
     * @return true if successful
     */
    [[nodiscard]] static auto renameWorkspace(uint32_t workspaceId, const std::string& newName) -> WMResult<bool>;
};

}  // namespace atom::system::wm

#endif  // ATOM_SYSINFO_WM_WM_HPP
