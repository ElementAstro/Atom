#include "wm.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>

#ifdef _WIN32
#include "platform/windows.hpp"
#elif __linux__
#include "platform/linux.hpp"
#elif __APPLE__
#include "platform/macos.hpp"
#endif

namespace atom::system::wm {

auto getSystemInfo() -> WMResult<SystemInfo> {
    spdlog::debug("Retrieving system information");

#ifdef _WIN32
    return windows::getSystemInfo();
#elif __linux__
    return linux::getSystemInfo();
#elif __APPLE__
    return macos::getSystemInfo();
#else
    spdlog::warn("Unsupported platform for system info detection");

    SystemInfo info;
    constexpr auto unknown = "Unsupported Platform";
    info.desktopEnvironment = unknown;
    info.windowManager = unknown;
    info.wmTheme = unknown;
    info.icons = unknown;
    info.font = unknown;
    info.cursor = unknown;
    info.themeInfo.type = ThemeType::UNKNOWN;
    info.themeInfo.name = unknown;

    return info;
#endif
}

auto getThemeInfo() -> WMResult<ThemeInfo> {
    spdlog::debug("Retrieving theme information");

#ifdef _WIN32
    return windows::getThemeInfo();
#elif __linux__
    return linux::getThemeInfo();
#elif __APPLE__
    return macos::getThemeInfo();
#else
    ThemeInfo theme;
    theme.type = ThemeType::UNKNOWN;
    theme.name = "Unsupported Platform";
    return theme;
#endif
}

auto enumerateWindows() -> WMResult<std::vector<WindowInfo>> {
    spdlog::debug("Enumerating windows");

#ifdef _WIN32
    return windows::enumerateWindows();
#elif __linux__
    return linux::enumerateWindows();
#elif __APPLE__
    return macos::enumerateWindows();
#else
    return WMError::PLATFORM_NOT_SUPPORTED;
#endif
}

auto getWindowInfo(uint64_t windowId) -> WMResult<WindowInfo> {
    spdlog::debug("Getting window info for ID: {}", windowId);

#ifdef _WIN32
    return windows::getWindowInfo(windowId);
#elif __linux__
    return linux::getWindowInfo(windowId);
#elif __APPLE__
    return macos::getWindowInfo(windowId);
#else
    return WMError::PLATFORM_NOT_SUPPORTED;
#endif
}

auto getMonitors() -> WMResult<std::vector<MonitorInfo>> {
    spdlog::debug("Getting monitor information");

#ifdef _WIN32
    return windows::getMonitors();
#elif __linux__
    return linux::getMonitors();
#elif __APPLE__
    return macos::getMonitors();
#else
    return WMError::PLATFORM_NOT_SUPPORTED;
#endif
}

auto getWorkspaces() -> WMResult<std::vector<WorkspaceInfo>> {
    spdlog::debug("Getting workspace information");

#ifdef _WIN32
    return windows::getVirtualDesktops();
#elif __linux__
    return linux::getWorkspaces();
#elif __APPLE__
    return macos::getSpaces();
#else
    return WMError::PLATFORM_NOT_SUPPORTED;
#endif
}

auto setWindowState(uint64_t windowId, WindowState state) -> WMResult<bool> {
    spdlog::debug("Setting window {} state to {}", windowId, static_cast<int>(state));

#ifdef _WIN32
    return windows::setWindowState(windowId, state);
#elif __linux__
    return linux::setWindowState(windowId, state);
#elif __APPLE__
    return macos::setWindowState(windowId, state);
#else
    return WMError::PLATFORM_NOT_SUPPORTED;
#endif
}

auto moveWindow(uint64_t windowId, int x, int y) -> WMResult<bool> {
    spdlog::debug("Moving window {} to ({}, {})", windowId, x, y);

#ifdef _WIN32
    return windows::moveWindow(windowId, x, y);
#elif __linux__
    return linux::moveWindow(windowId, x, y);
#elif __APPLE__
    return macos::moveWindow(windowId, x, y);
#else
    return WMError::PLATFORM_NOT_SUPPORTED;
#endif
}

auto resizeWindow(uint64_t windowId, int width, int height) -> WMResult<bool> {
    spdlog::debug("Resizing window {} to {}x{}", windowId, width, height);

#ifdef _WIN32
    return windows::resizeWindow(windowId, width, height);
#elif __linux__
    return linux::resizeWindow(windowId, width, height);
#elif __APPLE__
    return macos::resizeWindow(windowId, width, height);
#else
    return WMError::PLATFORM_NOT_SUPPORTED;
#endif
}

auto focusWindow(uint64_t windowId) -> WMResult<bool> {
    spdlog::debug("Focusing window {}", windowId);

#ifdef _WIN32
    return windows::focusWindow(windowId);
#elif __linux__
    return linux::focusWindow(windowId);
#elif __APPLE__
    return macos::focusWindow(windowId);
#else
    return WMError::PLATFORM_NOT_SUPPORTED;
#endif
}

auto closeWindow(uint64_t windowId) -> WMResult<bool> {
    spdlog::debug("Closing window {}", windowId);

#ifdef _WIN32
    return windows::closeWindow(windowId);
#elif __linux__
    return linux::closeWindow(windowId);
#elif __APPLE__
    return macos::closeWindow(windowId);
#else
    return WMError::PLATFORM_NOT_SUPPORTED;
#endif
}

auto switchToWorkspace(uint32_t workspaceId) -> WMResult<bool> {
    spdlog::debug("Switching to workspace {}", workspaceId);

#ifdef _WIN32
    // Windows doesn't have traditional workspaces, but could implement virtual desktop switching
    return WMError::PLATFORM_NOT_SUPPORTED;
#elif __linux__
    return linux::switchToWorkspace(workspaceId);
#elif __APPLE__
    return macos::switchToSpace(workspaceId);
#else
    return WMError::PLATFORM_NOT_SUPPORTED;
#endif
}

auto moveWindowToWorkspace(uint64_t windowId, uint32_t workspaceId) -> WMResult<bool> {
    spdlog::debug("Moving window {} to workspace {}", windowId, workspaceId);

#ifdef _WIN32
    // Windows doesn't have traditional workspaces, but could implement virtual desktop moving
    return WMError::PLATFORM_NOT_SUPPORTED;
#elif __linux__
    return linux::moveWindowToWorkspace(windowId, workspaceId);
#elif __APPLE__
    return macos::moveWindowToSpace(windowId, workspaceId);
#else
    return WMError::PLATFORM_NOT_SUPPORTED;
#endif
}

// WindowManager class implementation
WindowManager::WindowManager() {
    spdlog::debug("WindowManager initialized");
}

WindowManager::~WindowManager() {
    stopMonitoring();
}

auto WindowManager::startMonitoring(WindowCallback callback, std::chrono::milliseconds interval) -> bool {
    if (isMonitoring_) {
        spdlog::warn("Window monitoring is already active");
        return false;
    }

    windowCallback_ = std::move(callback);
    isMonitoring_ = true;

    // Start monitoring thread
    monitoringThread_ = std::thread([this, interval]() {
        std::vector<WindowInfo> previousWindows;

        while (isMonitoring_) {
            auto windowsResult = enumerateWindows();
            if (!isError(windowsResult)) {
                const auto& currentWindows = getValue(windowsResult);

                // Compare with previous windows to detect changes
                for (const auto& window : currentWindows) {
                    bool isNew = std::none_of(previousWindows.begin(), previousWindows.end(),
                        [&window](const WindowInfo& prev) { return prev.id == window.id; });

                    if (isNew && windowCallback_) {
                        windowCallback_(window, true); // Window added
                    }
                }

                // Check for removed windows
                for (const auto& prevWindow : previousWindows) {
                    bool isRemoved = std::none_of(currentWindows.begin(), currentWindows.end(),
                        [&prevWindow](const WindowInfo& current) { return current.id == prevWindow.id; });

                    if (isRemoved && windowCallback_) {
                        windowCallback_(prevWindow, false); // Window removed
                    }
                }

                previousWindows = currentWindows;
            }

            std::this_thread::sleep_for(interval);
        }
    });

    return true;
}

void WindowManager::stopMonitoring() {
    if (isMonitoring_) {
        isMonitoring_ = false;
        if (monitoringThread_.joinable()) {
            monitoringThread_.join();
        }
        spdlog::debug("Window monitoring stopped");
    }
}

auto WindowManager::isMonitoring() const -> bool {
    return isMonitoring_;
}

// ThemeManager class implementation
ThemeManager::ThemeManager() {
    spdlog::debug("ThemeManager initialized");
}

auto ThemeManager::getCurrentTheme() -> WMResult<ThemeInfo> {
    return getThemeInfo();
}

auto ThemeManager::startMonitoring(ThemeCallback callback, std::chrono::milliseconds interval) -> bool {
    if (isMonitoring_) {
        spdlog::warn("Theme monitoring is already active");
        return false;
    }

    themeCallback_ = std::move(callback);
    isMonitoring_ = true;

    // Start monitoring thread
    monitoringThread_ = std::thread([this, interval]() {
        auto previousThemeResult = getThemeInfo();
        ThemeInfo previousTheme;
        if (!isError(previousThemeResult)) {
            previousTheme = getValue(previousThemeResult);
        }

        while (isMonitoring_) {
            auto currentThemeResult = getThemeInfo();
            if (!isError(currentThemeResult)) {
                const auto& currentTheme = getValue(currentThemeResult);

                // Check if theme changed
                if (currentTheme.type != previousTheme.type ||
                    currentTheme.name != previousTheme.name ||
                    currentTheme.variant != previousTheme.variant) {

                    if (themeCallback_) {
                        themeCallback_(currentTheme);
                    }

                    previousTheme = currentTheme;
                }
            }

            std::this_thread::sleep_for(interval);
        }
    });

    return true;
}

void ThemeManager::stopMonitoring() {
    if (isMonitoring_) {
        isMonitoring_ = false;
        if (monitoringThread_.joinable()) {
            monitoringThread_.join();
        }
        spdlog::debug("Theme monitoring stopped");
    }
}

auto ThemeManager::isMonitoring() const -> bool {
    return isMonitoring_;
}

// WindowManager additional methods
auto WindowManager::getWindowsByProcess(const std::string& processName) -> WMResult<std::vector<WindowInfo>> {
    return getFilteredWindows([&processName](const WindowInfo& window) {
        return window.processName == processName;
    });
}

auto WindowManager::getWindowsInWorkspace(uint32_t workspaceId) -> WMResult<std::vector<WindowInfo>> {
    return getFilteredWindows([workspaceId](const WindowInfo& window) {
        return window.workspaceId.has_value() && window.workspaceId.value() == workspaceId;
    });
}

// WorkspaceManager implementation
auto WorkspaceManager::getCurrentWorkspace() -> WMResult<WorkspaceInfo> {
    auto workspacesResult = getWorkspaces();
    if (isError(workspacesResult)) {
        return getError(workspacesResult);
    }

    const auto& workspaces = getValue(workspacesResult);
    for (const auto& workspace : workspaces) {
        if (workspace.isActive) {
            return workspace;
        }
    }

    return WMError::WORKSPACE_NOT_FOUND;
}

auto WorkspaceManager::createWorkspace([[maybe_unused]] const std::string& name) -> WMResult<uint32_t> {
    // Platform-specific implementation would be needed
    // For now, return not supported
    return WMError::PLATFORM_NOT_SUPPORTED;
}

auto WorkspaceManager::deleteWorkspace([[maybe_unused]] uint32_t workspaceId) -> WMResult<bool> {
    // Platform-specific implementation would be needed
    // For now, return not supported
    return WMError::PLATFORM_NOT_SUPPORTED;
}

auto WorkspaceManager::renameWorkspace([[maybe_unused]] uint32_t workspaceId, [[maybe_unused]] const std::string& newName) -> WMResult<bool> {
    // Platform-specific implementation would be needed
    // For now, return not supported
    return WMError::PLATFORM_NOT_SUPPORTED;
}

}  // namespace atom::system::wm
