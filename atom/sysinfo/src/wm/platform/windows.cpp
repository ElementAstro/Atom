#include "windows.hpp"

#ifdef _WIN32

#include <array>
#include <string_view>
#include <format>
#include <vector>
#include <unordered_map>

// clang-format off
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <psapi.h>
// clang-format on

#if _MSC_VER
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "psapi.lib")
#endif

#include <spdlog/spdlog.h>

namespace atom::system::wm::windows {

namespace {

/**
 * @brief Gets Windows theme information from registry.
 */
auto getWindowsTheme() -> std::string {
    DWORD appsUseLightTheme = 1;
    DWORD systemUsesLightTheme = 1;

    HKEY hKey;
    constexpr auto regPath =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

    if (RegOpenKeyExW(HKEY_CURRENT_USER, regPath, 0, KEY_READ, &hKey) ==
        ERROR_SUCCESS) {
        DWORD dataSize = sizeof(DWORD);
        RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&appsUseLightTheme),
                         &dataSize);
        RegQueryValueExW(hKey, L"SystemUsesLightTheme", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&systemUsesLightTheme),
                         &dataSize);
        RegCloseKey(hKey);
    }

    return std::format("Windows Theme (System: {}, Apps: {})",
                       systemUsesLightTheme ? "Light" : "Dark",
                       appsUseLightTheme ? "Light" : "Dark");
}

/**
 * @brief Gets Windows system font information.
 */
auto getWindowsFont() -> std::string {
    NONCLIENTMETRICS metrics{};
    metrics.cbSize = sizeof(NONCLIENTMETRICS);

    if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics,
                             0)) {
        return std::format(
            "{} ({}pt)",
            reinterpret_cast<char*>(metrics.lfMessageFont.lfFaceName),
            std::abs(metrics.lfMessageFont.lfHeight));
    }

    return "Unknown Font";
}

/**
 * @brief Checks if Desktop Window Manager is enabled.
 */
auto getWindowsWM() -> std::string {
    BOOL isDWMEnabled = FALSE;
    HRESULT result = DwmIsCompositionEnabled(&isDWMEnabled);

    if (SUCCEEDED(result) && isDWMEnabled) {
        return "Desktop Window Manager (DWM)";
    }

    return "Classic Windows";
}

/**
 * @brief Convert Windows window state to our enum
 */
auto convertWindowState(HWND hwnd) -> WindowState {
    if (!IsWindowVisible(hwnd)) {
        return WindowState::HIDDEN;
    }

    if (IsIconic(hwnd)) {
        return WindowState::MINIMIZED;
    }

    if (IsZoomed(hwnd)) {
        return WindowState::MAXIMIZED;
    }

    // Check for fullscreen (window covers entire screen)
    RECT windowRect, screenRect;
    GetWindowRect(hwnd, &windowRect);
    GetWindowRect(GetDesktopWindow(), &screenRect);

    if (windowRect.left <= screenRect.left &&
        windowRect.top <= screenRect.top &&
        windowRect.right >= screenRect.right &&
        windowRect.bottom >= screenRect.bottom) {
        return WindowState::FULLSCREEN;
    }

    return WindowState::NORMAL;
}

/**
 * @brief Get process name from window handle
 */
auto getProcessName(HWND hwnd) -> std::string {
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!hProcess) {
        return "Unknown";
    }

    char processName[MAX_PATH];
    DWORD size = sizeof(processName);
    if (QueryFullProcessImageNameA(hProcess, 0, processName, &size)) {
        CloseHandle(hProcess);
        std::string fullPath(processName);
        size_t lastSlash = fullPath.find_last_of("\\/");
        return (lastSlash != std::string::npos) ? fullPath.substr(lastSlash + 1) : fullPath;
    }

    CloseHandle(hProcess);
    return "Unknown";
}

/**
 * @brief Callback for EnumWindows
 */
struct EnumWindowsData {
    std::vector<WindowInfo>* windows;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* data = reinterpret_cast<EnumWindowsData*>(lParam);

    // Skip invisible windows and windows without titles
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    char title[256];
    if (GetWindowTextA(hwnd, title, sizeof(title)) == 0) {
        return TRUE; // Skip windows without titles
    }

    char className[256];
    GetClassNameA(hwnd, className, sizeof(className));

    RECT rect;
    GetWindowRect(hwnd, &rect);

    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    WindowInfo info;
    info.id = reinterpret_cast<uint64_t>(hwnd);
    info.title = title;
    info.className = className;
    info.processName = getProcessName(hwnd);
    info.processId = processId;
    info.state = convertWindowState(hwnd);
    info.x = rect.left;
    info.y = rect.top;
    info.width = rect.right - rect.left;
    info.height = rect.bottom - rect.top;
    info.isVisible = IsWindowVisible(hwnd);
    info.isActive = (GetForegroundWindow() == hwnd);

    data->windows->push_back(info);
    return TRUE;
}

}  // anonymous namespace

auto getSystemInfo() -> WMResult<SystemInfo> {
    spdlog::debug("Retrieving Windows system information");

    SystemInfo info;
    info.desktopEnvironment = "Windows Fluent Design";
    info.windowManager = getWindowsWM();
    info.wmTheme = getWindowsTheme();
    info.icons = "Windows Shell Icons";
    info.font = getWindowsFont();
    info.cursor = "Windows Default";
    info.supportsVirtualDesktops = true; // Windows 10+
    info.supportsWorkspaces = true;
    info.wmVersion = "Windows 11"; // Could be detected more precisely
    info.wmFeatures = {"Virtual Desktops", "Snap Layouts", "Window Management"};

    // Get theme info
    auto themeResult = getThemeInfo();
    if (!isError(themeResult)) {
        info.themeInfo = getValue(themeResult);
    }

    // Get monitors
    auto monitorsResult = getMonitors();
    if (!isError(monitorsResult)) {
        info.monitors = getValue(monitorsResult);
    }

    // Get virtual desktops
    auto desktopsResult = getVirtualDesktops();
    if (!isError(desktopsResult)) {
        info.workspaces = getValue(desktopsResult);
    }

    spdlog::debug("Windows system info - DE: {}, WM: {}",
                  info.desktopEnvironment, info.windowManager);

    return info;
}

auto getThemeInfo() -> WMResult<ThemeInfo> {
    ThemeInfo theme;

    DWORD appsUseLightTheme = 1;
    DWORD systemUsesLightTheme = 1;

    HKEY hKey;
    constexpr auto regPath =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

    if (RegOpenKeyExW(HKEY_CURRENT_USER, regPath, 0, KEY_READ, &hKey) ==
        ERROR_SUCCESS) {
        DWORD dataSize = sizeof(DWORD);
        RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&appsUseLightTheme),
                         &dataSize);
        RegQueryValueExW(hKey, L"SystemUsesLightTheme", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&systemUsesLightTheme),
                         &dataSize);
        RegCloseKey(hKey);
    }

    theme.type = (systemUsesLightTheme && appsUseLightTheme) ? ThemeType::LIGHT : ThemeType::DARK;
    theme.name = "Windows Default";
    theme.variant = theme.type == ThemeType::LIGHT ? "light" : "dark";
    theme.followsSystemTheme = true;

    // Set default colors based on theme
    if (theme.type == ThemeType::LIGHT) {
        theme.backgroundColor = "#FFFFFF";
        theme.foregroundColor = "#000000";
        theme.accentColor = "#0078D4"; // Windows blue
    } else {
        theme.backgroundColor = "#1E1E1E";
        theme.foregroundColor = "#FFFFFF";
        theme.accentColor = "#0078D4";
    }

    return theme;
}

auto getFontInfo() -> std::string {
    return getWindowsFont();
}

auto getWindowManager() -> std::string {
    return getWindowsWM();
}

auto enumerateWindows() -> WMResult<std::vector<WindowInfo>> {
    std::vector<WindowInfo> windows;
    EnumWindowsData data{&windows};

    if (!EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data))) {
        return WMError::OPERATION_FAILED;
    }

    return windows;
}

auto getWindowInfo(uint64_t windowId) -> WMResult<WindowInfo> {
    HWND hwnd = reinterpret_cast<HWND>(windowId);

    if (!IsWindow(hwnd)) {
        return WMError::WINDOW_NOT_FOUND;
    }

    char title[256];
    GetWindowTextA(hwnd, title, sizeof(title));

    char className[256];
    GetClassNameA(hwnd, className, sizeof(className));

    RECT rect;
    GetWindowRect(hwnd, &rect);

    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    WindowInfo info;
    info.id = windowId;
    info.title = title;
    info.className = className;
    info.processName = getProcessName(hwnd);
    info.processId = processId;
    info.state = convertWindowState(hwnd);
    info.x = rect.left;
    info.y = rect.top;
    info.width = rect.right - rect.left;
    info.height = rect.bottom - rect.top;
    info.isVisible = IsWindowVisible(hwnd);
    info.isActive = (GetForegroundWindow() == hwnd);

    return info;
}

auto getMonitors() -> WMResult<std::vector<MonitorInfo>> {
    // Implementation would use EnumDisplayMonitors
    // For now, return a basic implementation
    std::vector<MonitorInfo> monitors;

    MonitorInfo primary;
    primary.id = 0;
    primary.name = "Primary Monitor";
    primary.isPrimary = true;
    primary.width = GetSystemMetrics(SM_CXSCREEN);
    primary.height = GetSystemMetrics(SM_CYSCREEN);
    primary.refreshRate = 60; // Default, could be detected
    primary.scaleFactor = 1.0f; // Could be detected from DPI

    monitors.push_back(primary);
    return monitors;
}

auto getVirtualDesktops() -> WMResult<std::vector<WorkspaceInfo>> {
    // Windows Virtual Desktop API is complex and requires COM
    // For now, return a basic implementation
    std::vector<WorkspaceInfo> workspaces;

    WorkspaceInfo desktop;
    desktop.id = 0;
    desktop.name = "Desktop 1";
    desktop.isActive = true;
    desktop.width = GetSystemMetrics(SM_CXSCREEN);
    desktop.height = GetSystemMetrics(SM_CYSCREEN);

    workspaces.push_back(desktop);
    return workspaces;
}

auto setWindowState(uint64_t windowId, WindowState state) -> WMResult<bool> {
    HWND hwnd = reinterpret_cast<HWND>(windowId);

    if (!IsWindow(hwnd)) {
        return WMError::WINDOW_NOT_FOUND;
    }

    int cmdShow;
    switch (state) {
        case WindowState::NORMAL:
            cmdShow = SW_RESTORE;
            break;
        case WindowState::MINIMIZED:
            cmdShow = SW_MINIMIZE;
            break;
        case WindowState::MAXIMIZED:
            cmdShow = SW_MAXIMIZE;
            break;
        case WindowState::HIDDEN:
            cmdShow = SW_HIDE;
            break;
        default:
            return WMError::INVALID_PARAMETER;
    }

    return ShowWindow(hwnd, cmdShow) != 0;
}

auto moveWindow(uint64_t windowId, int x, int y) -> WMResult<bool> {
    HWND hwnd = reinterpret_cast<HWND>(windowId);

    if (!IsWindow(hwnd)) {
        return WMError::WINDOW_NOT_FOUND;
    }

    RECT rect;
    GetWindowRect(hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    return SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER) != 0;
}

auto resizeWindow(uint64_t windowId, int width, int height) -> WMResult<bool> {
    HWND hwnd = reinterpret_cast<HWND>(windowId);

    if (!IsWindow(hwnd)) {
        return WMError::WINDOW_NOT_FOUND;
    }

    RECT rect;
    GetWindowRect(hwnd, &rect);

    return SetWindowPos(hwnd, nullptr, rect.left, rect.top, width, height, SWP_NOZORDER) != 0;
}

auto focusWindow(uint64_t windowId) -> WMResult<bool> {
    HWND hwnd = reinterpret_cast<HWND>(windowId);

    if (!IsWindow(hwnd)) {
        return WMError::WINDOW_NOT_FOUND;
    }

    return SetForegroundWindow(hwnd) != 0;
}

auto closeWindow(uint64_t windowId) -> WMResult<bool> {
    HWND hwnd = reinterpret_cast<HWND>(windowId);

    if (!IsWindow(hwnd)) {
        return WMError::WINDOW_NOT_FOUND;
    }

    return PostMessage(hwnd, WM_CLOSE, 0, 0) != 0;
}

}  // namespace atom::system::wm::windows

#endif  // _WIN32
