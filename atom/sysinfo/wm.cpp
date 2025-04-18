#include "wm.hpp"

#include <format>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <dwmapi.h>  // For Desktop Window Manager (DWM)
// clang-format on
#if _MSC_VER
#pragma comment(lib, "dwmapi.lib")
#endif
#elif __linux__
#include <cstdlib>  // For Linux system commands
#endif

#include "atom/log/loguru.hpp"

namespace atom::system {
// Function to get system information cross-platform
auto getSystemInfo() -> SystemInfo {
    LOG_F(INFO, "Starting getSystemInfo function");
    SystemInfo info;

#ifdef _WIN32
    LOG_F(INFO, "Running on Windows");

    // Windows: Get desktop environment, window manager, theme, icons, font,
    // cursor, etc.
    info.desktopEnvironment = "Fluent";  // Windows Fluent Design
    LOG_F(INFO, "Desktop environment: Fluent");

    // Get the status of the window manager (DWM)
    BOOL isDWMEnabled = FALSE;
    HRESULT resultDWM = DwmIsCompositionEnabled(&isDWMEnabled);
    if (SUCCEEDED(resultDWM) && (isDWMEnabled != 0)) {
        info.windowManager = "Desktop Window Manager (DWM)";
    } else {
        info.windowManager = "Unknown WM";
    }
    LOG_F(INFO, "Window manager: {}", info.windowManager);

    // Get theme information (Light/Dark Mode)
    DWORD appsUseLightTheme = 1;
    DWORD systemUsesLightTheme = 1;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\P"
                      L"ersonalize",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD dataSize = sizeof(DWORD);
        RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&appsUseLightTheme),
                         &dataSize);
        RegQueryValueExW(hKey, L"SystemUsesLightTheme", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&systemUsesLightTheme),
                         &dataSize);
        RegCloseKey(hKey);
    }
    info.wmTheme =
        "Oem - Blue (System: " +
        std::string(systemUsesLightTheme ? "Light" : "Dark") +
        ", Apps: " + std::string(appsUseLightTheme ? "Light" : "Dark") + ")";
    LOG_F(INFO, "Window manager theme: {}", info.wmTheme);

    // Icon information (Recycle Bin)
    info.icons = "Recycle Bin";
    LOG_F(INFO, "Icons: Recycle Bin");

    // Get font information
    NONCLIENTMETRICS metrics = {};
    metrics.cbSize = sizeof(NONCLIENTMETRICS);
    metrics.iBorderWidth = 0;
    metrics.iScrollWidth = 0;
    metrics.iScrollHeight = 0;
    metrics.iCaptionWidth = 0;
    metrics.iCaptionHeight = 0;
    metrics.lfCaptionFont = {};
    metrics.iSmCaptionWidth = 0;
    metrics.iSmCaptionHeight = 0;
    metrics.lfSmCaptionFont = {};
    metrics.iMenuWidth = 0;
    metrics.iMenuHeight = 0;
    metrics.lfMenuFont = {};
    metrics.lfStatusFont = {};
    metrics.lfMessageFont = {};
    metrics.iPaddedBorderWidth = 0;
    metrics.cbSize = sizeof(NONCLIENTMETRICS);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    info.font = std::format("{} ({}pt)", metrics.lfMessageFont.lfFaceName,
                            metrics.lfMessageFont.lfHeight);
    LOG_F(INFO, "Font: {}", info.font);

    // Get cursor information
    info.cursor = "Windows Default (32px)";
    LOG_F(INFO, "Cursor: Windows Default (32px)");

#elif __linux__
    LOG_F(INFO, "Running on Linux");

    // Linux: Get desktop environment, window manager, theme, icons, font,
    // cursor, etc.

    // Get desktop environment (DE)
    const char* de = std::getenv("XDG_CURRENT_DESKTOP");
    if (de == nullptr) {
        info.desktopEnvironment = "Unknown";
    } else {
        info.desktopEnvironment = de;
    }
    LOG_F(INFO, "Desktop environment: {}", info.desktopEnvironment);

    // Get window manager (WM)
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen("wmctrl -m | grep 'Name' | awk '{print $2}'", "r");
    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
        info.windowManager = result;
    } else {
        info.windowManager = "Unknown WM";
    }
    LOG_F(INFO, "Window manager: {}", info.windowManager);

    // Get window manager theme
    pipe = popen("gsettings get org.gnome.desktop.interface gtk-theme", "r");
    result = "";
    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
        info.wmTheme = result;
    } else {
        info.wmTheme = "Unknown Theme";
    }
    LOG_F(INFO, "Window manager theme: {}", info.wmTheme);

    // Icons (Recycle Bin)
    info.icons = "Recycle Bin";  // Getting icons on Linux is complex, hardcoded
                                 // as an example
    LOG_F(INFO, "Icons: Recycle Bin");

    // Get font information
    pipe = popen("gsettings get org.gnome.desktop.interface font-name", "r");
    result = "";
    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
        info.font = result;
    } else {
        info.font = "Unknown Font";
    }
    LOG_F(INFO, "Font: {}", info.font);

    // Get cursor information
    pipe = popen("gsettings get org.gnome.desktop.interface cursor-theme", "r");
    result = "";
    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
        info.cursor = result;
    } else {
        info.cursor = "Unknown Cursor";
    }
    LOG_F(INFO, "Cursor: {}", info.cursor);

#else
    LOG_F(WARNING, "Unsupported platform");

    // Unsupported platform
    info.desktopEnvironment = "Unsupported Platform";
    info.windowManager = "Unsupported Platform";
    info.wmTheme = "Unsupported Platform";
    info.icons = "Unsupported Platform";
    info.font = "Unsupported Platform";
    info.cursor = "Unsupported Platform";
#endif

    LOG_F(INFO, "Finished getSystemInfo function");
    return info;
}
}  // namespace atom::system
